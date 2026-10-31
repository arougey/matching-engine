#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "orderbook/OrderBook.hpp"

using namespace ob;

static std::string fmt(Price ticks) {
    std::ostringstream os;
    os << ticks / 100 << '.' << std::setw(2) << std::setfill('0') << ticks % 100;
    return os.str();
}

int main() {
    OrderBook book;

    book.add_limit(1, Side::Sell, 10050, 100);
    book.add_limit(2, Side::Sell, 10050, 50);
    book.add_limit(3, Side::Sell, 10100, 200);
    book.add_limit(4, Side::Buy,  10000, 300);
    book.add_limit(5, Side::Buy,  9950,  100);

    std::cout << "Before: best bid " << fmt(*book.best_bid())
              << " | best ask " << fmt(*book.best_ask()) << "\n\n";

    auto result = book.add_limit(6, Side::Buy, 10100, 200);

    std::cout << "Incoming BUY 200 @ 101.00 produced " << result.trades.size() << " trades:\n";
    for (const Trade& t : result.trades) {
        std::cout << "  order " << t.aggressor_id << " x resting " << t.resting_id
                  << "  " << t.quantity << " @ " << fmt(t.price) << '\n';
    }
    std::cout << "Remaining resting: " << result.remaining << "\n\n";

    std::cout << "After: best bid " << fmt(*book.best_bid())
              << " | best ask " << fmt(*book.best_ask())
              << " | volume at 101.00 ask: " << book.volume_at(Side::Sell, 10100) << '\n';
}
