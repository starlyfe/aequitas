#include "sim/order_book.h"

#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace aeq;

static int failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__ << '\n';     \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

int main() {
    // Crossing limit orders.
    {
        OrderBook book;
        book.set_last_price(100);
        Order buy;
        buy.agent_id = 1;
        buy.side = Side::Buy;
        buy.type = OrderType::Limit;
        buy.price = 105;
        buy.qty = 2;
        buy.escrow_cash = 210;
        buy.placed_tick = 1;
        buy.expire_tick = 100;
        book.place(buy);

        Order sell;
        sell.agent_id = 2;
        sell.side = Side::Sell;
        sell.type = OrderType::Limit;
        sell.price = 100;
        sell.qty = 2;
        sell.escrow_qty = 2;
        sell.placed_tick = 1;
        sell.expire_tick = 100;
        book.place(sell);

        auto r = book.match(1);
        CHECK(r.trades.size() == 1);
        CHECK(r.trades[0].qty == 2);
        CHECK(r.trades[0].price == 100); // resting ask
        CHECK(!book.best_bid().has_value());
        CHECK(!book.best_ask().has_value());
    }

    // Partial fill.
    {
        OrderBook book;
        Order buy;
        buy.agent_id = 1;
        buy.side = Side::Buy;
        buy.type = OrderType::Limit;
        buy.price = 50;
        buy.qty = 5;
        buy.escrow_cash = 250;
        buy.placed_tick = 1;
        buy.expire_tick = 100;
        book.place(buy);

        Order sell;
        sell.agent_id = 2;
        sell.side = Side::Sell;
        sell.type = OrderType::Limit;
        sell.price = 50;
        sell.qty = 2;
        sell.escrow_qty = 2;
        sell.placed_tick = 1;
        sell.expire_tick = 100;
        book.place(sell);

        auto r = book.match(1);
        CHECK(r.trades.size() == 1);
        CHECK(r.trades[0].qty == 2);
        CHECK(book.best_bid().value_or(0) == 50);
        CHECK(book.escrowed_cash() == 150); // 3 remaining * 50
    }

    // Price-time priority among 3 sells at same price — earliest fills first.
    {
        OrderBook book;
        Order buy;
        buy.agent_id = 0;
        buy.side = Side::Buy;
        buy.type = OrderType::Limit;
        buy.price = 10;
        buy.qty = 1;
        buy.escrow_cash = 10;
        buy.placed_tick = 5;
        buy.expire_tick = 100;
        book.place(buy);

        for (int i = 0; i < 3; ++i) {
            Order sell;
            sell.agent_id = 10 + i;
            sell.side = Side::Sell;
            sell.type = OrderType::Limit;
            sell.price = 10;
            sell.qty = 1;
            sell.escrow_qty = 1;
            sell.placed_tick = i;
            sell.expire_tick = 100;
            book.place(sell);
        }
        auto r = book.match(5);
        CHECK(r.trades.size() == 1);
        CHECK(r.trades[0].seller_id == 10); // first placed
    }

    // Escrow refund on expiry.
    {
        OrderBook book;
        Order buy;
        buy.agent_id = 7;
        buy.side = Side::Buy;
        buy.type = OrderType::Limit;
        buy.price = 40;
        buy.qty = 3;
        buy.escrow_cash = 120;
        buy.placed_tick = 1;
        buy.expire_tick = 5;
        book.place(buy);

        auto refunds = book.expire(5);
        CHECK(refunds.size() == 1);
        CHECK(refunds[0].agent_id == 7);
        CHECK(refunds[0].cash == 120);
        CHECK(book.escrowed_cash() == 0);
    }

    // Cancel refund.
    {
        OrderBook book;
        Order sell;
        sell.agent_id = 3;
        sell.side = Side::Sell;
        sell.type = OrderType::Limit;
        sell.price = 20;
        sell.qty = 4;
        sell.escrow_qty = 4;
        sell.placed_tick = 1;
        sell.expire_tick = 100;
        const auto id = book.place(sell);
        auto r = book.cancel(id);
        CHECK(r.has_value());
        CHECK(r->qty == 4);
        CHECK(book.escrowed_qty() == 0);
    }

    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_order_book: OK\n";
    return 0;
}
