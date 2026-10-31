#include <gtest/gtest.h>

#include "orderbook/OrderBook.hpp"

using namespace ob;

static Trade T(OrderId aggr, OrderId rest, Side side, Price px, Quantity qty) {
    return Trade{aggr, rest, side, px, qty};
}

TEST(OrderBook, EmptyBookHasNoBestPrices) {
    OrderBook book;
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(OrderBook, NonCrossingOrdersRest) {
    OrderBook book;
    auto r1 = book.add_limit(1, Side::Buy, 100, 10);
    auto r2 = book.add_limit(2, Side::Sell, 101, 5);

    EXPECT_EQ(r1.status, AddStatus::Accepted);
    EXPECT_TRUE(r1.trades.empty());
    EXPECT_EQ(r1.remaining, 10u);
    EXPECT_TRUE(r2.trades.empty());

    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.best_ask(), 101);
    EXPECT_EQ(book.order_count(), 2u);
}

TEST(OrderBook, BestPricesTrackTheTopOfEachSide) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 99, 1);
    book.add_limit(2, Side::Buy, 100, 1);
    book.add_limit(3, Side::Buy, 98, 1);
    book.add_limit(4, Side::Sell, 103, 1);
    book.add_limit(5, Side::Sell, 102, 1);
    book.add_limit(6, Side::Sell, 104, 1);

    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.best_ask(), 102);
    EXPECT_EQ(book.level_count(Side::Buy), 3u);
    EXPECT_EQ(book.level_count(Side::Sell), 3u);
}

TEST(OrderBook, SamePriceOrdersAggregateIntoOneLevel) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 10);
    book.add_limit(2, Side::Buy, 100, 15);
    EXPECT_EQ(book.level_count(Side::Buy), 1u);
    EXPECT_EQ(book.volume_at(Side::Buy, 100), 25u);
}

TEST(Matching, ExactFullFill) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 10);
    auto r = book.add_limit(2, Side::Buy, 100, 10);

    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0], T(2, 1, Side::Buy, 100, 10));
    EXPECT_EQ(r.remaining, 0u);
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(Matching, IncomingPartiallyFillsThenRests) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 4);
    auto r = book.add_limit(2, Side::Buy, 100, 10);

    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].quantity, 4u);
    EXPECT_EQ(r.remaining, 6u);
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.volume_at(Side::Buy, 100), 6u);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(Matching, RestingOrderPartiallyFilledKeepsItsPlace) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 10);
    book.add_limit(2, Side::Sell, 100, 10);
    book.add_limit(3, Side::Buy, 100, 4);

    EXPECT_EQ(book.volume_at(Side::Sell, 100), 16u);

    auto r = book.add_limit(4, Side::Buy, 100, 8);
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0], T(4, 1, Side::Buy, 100, 6));
    EXPECT_EQ(r.trades[1], T(4, 2, Side::Buy, 100, 2));
}

TEST(Matching, TimePriorityWithinALevel) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 5);
    book.add_limit(2, Side::Buy, 100, 5);
    book.add_limit(3, Side::Buy, 100, 5);

    auto r = book.add_limit(4, Side::Sell, 100, 12);
    ASSERT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.trades[0].resting_id, 1u);
    EXPECT_EQ(r.trades[1].resting_id, 2u);
    EXPECT_EQ(r.trades[2].resting_id, 3u);
    EXPECT_EQ(r.trades[2].quantity, 2u);
}

TEST(Matching, PricePriorityBeatsTimePriority) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 101, 5);
    book.add_limit(2, Side::Sell, 100, 5);

    auto r = book.add_limit(3, Side::Buy, 101, 5);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].resting_id, 2u);
    EXPECT_EQ(r.trades[0].price, 100);
}

TEST(Matching, TradesExecuteAtRestingPrice) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 5);
    auto r = book.add_limit(2, Side::Buy, 105, 5);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.trades[0].price, 100);
}

TEST(Matching, SweepsMultipleLevelsUpToLimit) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 10050, 100);
    book.add_limit(2, Side::Sell, 10050, 50);
    book.add_limit(3, Side::Sell, 10100, 200);
    book.add_limit(4, Side::Buy, 10000, 300);
    book.add_limit(5, Side::Buy, 9950, 100);

    auto r = book.add_limit(6, Side::Buy, 10100, 200);

    ASSERT_EQ(r.trades.size(), 3u);
    EXPECT_EQ(r.trades[0], T(6, 1, Side::Buy, 10050, 100));
    EXPECT_EQ(r.trades[1], T(6, 2, Side::Buy, 10050, 50));
    EXPECT_EQ(r.trades[2], T(6, 3, Side::Buy, 10100, 50));
    EXPECT_EQ(r.remaining, 0u);

    EXPECT_EQ(book.best_ask(), 10100);
    EXPECT_EQ(book.volume_at(Side::Sell, 10100), 150u);
    EXPECT_EQ(book.volume_at(Side::Sell, 10050), 0u);
    EXPECT_EQ(book.best_bid(), 10000);
}

TEST(Matching, StopsAtLimitPriceAndRestsRemainder) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 5);
    book.add_limit(2, Side::Sell, 102, 5);

    auto r = book.add_limit(3, Side::Buy, 101, 8);
    ASSERT_EQ(r.trades.size(), 1u);
    EXPECT_EQ(r.remaining, 3u);
    EXPECT_EQ(book.best_bid(), 101);
    EXPECT_EQ(book.best_ask(), 102);
}

TEST(Matching, SellSideIsSymmetric) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 5);
    book.add_limit(2, Side::Buy, 99, 5);
    book.add_limit(3, Side::Buy, 98, 5);

    auto r = book.add_limit(4, Side::Sell, 99, 12);
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0], T(4, 1, Side::Sell, 100, 5));
    EXPECT_EQ(r.trades[1], T(4, 2, Side::Sell, 99, 5));
    EXPECT_EQ(r.remaining, 2u);
    EXPECT_EQ(book.best_ask(), 99);
    EXPECT_EQ(book.best_bid(), 98);
}

TEST(Matching, FilledOrdersLeaveTheBook) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 5);
    book.add_limit(2, Side::Buy, 100, 5);
    EXPECT_FALSE(book.contains(1));
    EXPECT_FALSE(book.contains(2));
    EXPECT_FALSE(book.cancel(1));
}

TEST(Cancel, RemovesRestingOrder) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 10);
    EXPECT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.contains(1));
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.level_count(Side::Buy), 0u);
}

TEST(Cancel, UnknownOrDoubleCancelReturnsFalse) {
    OrderBook book;
    EXPECT_FALSE(book.cancel(42));
    book.add_limit(1, Side::Buy, 100, 10);
    EXPECT_TRUE(book.cancel(1));
    EXPECT_FALSE(book.cancel(1));
}

TEST(Cancel, MiddleOfQueuePreservesOthersOrder) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 5);
    book.add_limit(2, Side::Sell, 100, 5);
    book.add_limit(3, Side::Sell, 100, 5);
    EXPECT_TRUE(book.cancel(2));
    EXPECT_EQ(book.volume_at(Side::Sell, 100), 10u);

    auto r = book.add_limit(4, Side::Buy, 100, 10);
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].resting_id, 1u);
    EXPECT_EQ(r.trades[1].resting_id, 3u);
}

TEST(Cancel, BestPriceUpdatesWhenTopLevelEmpties) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 5);
    book.add_limit(2, Side::Buy, 99, 5);
    book.cancel(1);
    EXPECT_EQ(book.best_bid(), 99);
}

TEST(Cancel, AfterPartialFillRemovesOnlyRemainder) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 10);
    book.add_limit(2, Side::Sell, 100, 10);
    book.add_limit(3, Side::Buy, 100, 4);

    EXPECT_EQ(book.volume_at(Side::Sell, 100), 16u);
    EXPECT_TRUE(book.cancel(1));
    EXPECT_EQ(book.volume_at(Side::Sell, 100), 10u);
}

TEST(Validation, RejectsZeroQuantity) {
    OrderBook book;
    auto r = book.add_limit(1, Side::Buy, 100, 0);
    EXPECT_EQ(r.status, AddStatus::RejectedInvalidQuantity);
    EXPECT_EQ(book.order_count(), 0u);
}

TEST(Validation, RejectsNonPositivePrice) {
    OrderBook book;
    EXPECT_EQ(book.add_limit(1, Side::Buy, 0, 5).status, AddStatus::RejectedInvalidPrice);
    EXPECT_EQ(book.add_limit(2, Side::Sell, -5, 5).status, AddStatus::RejectedInvalidPrice);
}

TEST(Validation, RejectsDuplicateLiveId) {
    OrderBook book;
    book.add_limit(1, Side::Buy, 100, 5);
    auto r = book.add_limit(1, Side::Sell, 200, 5);
    EXPECT_EQ(r.status, AddStatus::RejectedDuplicateId);
    EXPECT_EQ(book.volume_at(Side::Buy, 100), 5u);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(Validation, RejectedOrderNeverTrades) {
    OrderBook book;
    book.add_limit(1, Side::Sell, 100, 5);
    auto r = book.add_limit(1, Side::Buy, 100, 5);
    EXPECT_TRUE(r.trades.empty());
    EXPECT_EQ(book.volume_at(Side::Sell, 100), 5u);
}
