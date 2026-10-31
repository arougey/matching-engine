#pragma once

#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "orderbook/Types.hpp"

namespace ob {

class OrderBook {
public:
    AddResult add_limit(OrderId id, Side side, Price price, Quantity quantity);
    bool cancel(OrderId id);

    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    Quantity volume_at(Side side, Price price) const;
    std::size_t level_count(Side side) const;
    std::size_t order_count() const { return index_.size(); }
    bool contains(OrderId id) const { return index_.contains(id); }

private:
    struct Order {
        OrderId  id;
        Side     side;
        Price    price;
        Quantity remaining;
    };

    struct Level {
        std::list<Order> orders;
        Quantity         total = 0;
    };

    using BidMap = std::map<Price, Level, std::greater<>>;
    using AskMap = std::map<Price, Level, std::less<>>;

    struct Locator {
        Side                       side;
        Price                      price;
        std::list<Order>::iterator it;
    };

    template <typename BookSide, typename Crosses>
    void match(BookSide& opposite, Order& incoming, Crosses crosses,
               std::vector<Trade>& trades);

    template <typename BookSide>
    void rest(BookSide& own, const Order& order);

    BidMap bids_;
    AskMap asks_;
    std::unordered_map<OrderId, Locator> index_;
};

}  // namespace ob
