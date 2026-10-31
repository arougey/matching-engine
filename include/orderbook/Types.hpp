#pragma once

#include <cstdint>
#include <vector>

namespace ob {

using Price    = std::int64_t;
using Quantity = std::uint64_t;
using OrderId  = std::uint64_t;

enum class Side : std::uint8_t { Buy, Sell };

struct Trade {
    OrderId  aggressor_id;
    OrderId  resting_id;
    Side     aggressor_side;
    Price    price;
    Quantity quantity;

    bool operator==(const Trade&) const = default;
};

enum class AddStatus : std::uint8_t {
    Accepted,
    RejectedDuplicateId,
    RejectedInvalidQuantity,
    RejectedInvalidPrice,
};

struct AddResult {
    AddStatus          status;
    std::vector<Trade> trades;
    Quantity           remaining;
};

}  // namespace ob
