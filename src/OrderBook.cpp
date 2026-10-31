#include "orderbook/OrderBook.hpp"

#include <algorithm>

namespace ob {

AddResult OrderBook::add_limit(OrderId id, Side side, Price price, Quantity quantity) {
    if (quantity == 0) return {AddStatus::RejectedInvalidQuantity, {}, 0};
    if (price <= 0)    return {AddStatus::RejectedInvalidPrice, {}, 0};
    if (index_.contains(id)) return {AddStatus::RejectedDuplicateId, {}, 0};

    Order incoming{id, side, price, quantity};
    std::vector<Trade> trades;

    if (side == Side::Buy) {
        match(asks_, incoming, [&](Price ask) { return ask <= price; }, trades);
    } else {
        match(bids_, incoming, [&](Price bid) { return bid >= price; }, trades);
    }

    if (incoming.remaining > 0) {
        if (side == Side::Buy) rest(bids_, incoming);
        else                   rest(asks_, incoming);
    }

    return {AddStatus::Accepted, std::move(trades), incoming.remaining};
}

template <typename BookSide, typename Crosses>
void OrderBook::match(BookSide& opposite, Order& incoming, Crosses crosses,
                      std::vector<Trade>& trades) {
    while (incoming.remaining > 0 && !opposite.empty()) {
        auto levelIt = opposite.begin();
        const Price levelPrice = levelIt->first;
        if (!crosses(levelPrice)) break;

        Level& level = levelIt->second;

        while (incoming.remaining > 0 && !level.orders.empty()) {
            Order& resting = level.orders.front();
            const Quantity fill = std::min(incoming.remaining, resting.remaining);

            trades.push_back({incoming.id, resting.id, incoming.side, levelPrice, fill});

            incoming.remaining -= fill;
            resting.remaining  -= fill;
            level.total        -= fill;

            if (resting.remaining == 0) {
                index_.erase(resting.id);
                level.orders.pop_front();
            }
        }

        if (level.orders.empty()) opposite.erase(levelIt);
    }
}

template <typename BookSide>
void OrderBook::rest(BookSide& own, const Order& order) {
    Level& level = own[order.price];
    level.orders.push_back(order);
    level.total += order.remaining;
    index_[order.id] = Locator{order.side, order.price, std::prev(level.orders.end())};
}

bool OrderBook::cancel(OrderId id) {
    auto found = index_.find(id);
    if (found == index_.end()) return false;

    const Locator loc = found->second;
    auto removeFrom = [&](auto& bookSide) {
        auto levelIt = bookSide.find(loc.price);
        Level& level = levelIt->second;
        level.total -= loc.it->remaining;
        level.orders.erase(loc.it);
        if (level.orders.empty()) bookSide.erase(levelIt);
    };

    if (loc.side == Side::Buy) removeFrom(bids_);
    else                       removeFrom(asks_);

    index_.erase(found);
    return true;
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Quantity OrderBook::volume_at(Side side, Price price) const {
    auto lookup = [&](const auto& bookSide) -> Quantity {
        auto it = bookSide.find(price);
        return it == bookSide.end() ? 0 : it->second.total;
    };
    return side == Side::Buy ? lookup(bids_) : lookup(asks_);
}

std::size_t OrderBook::level_count(Side side) const {
    return side == Side::Buy ? bids_.size() : asks_.size();
}

}  // namespace ob
