#pragma once

#include "sim/agent.h"
#include "sim/order_book.h"
#include "sim/params.h"
#include "sim/world.h"

#include <array>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace aeq {

struct PriceSample {
    std::int64_t price = 0;
    std::int64_t volume = 0;
};

class Market {
public:
    void init_hub(int hub_id);

    OrderBook& book(int hub_id, Resource r);
    const OrderBook& book(int hub_id, Resource r) const;

    MarketQuotes quotes(int hub_id) const;

    void record_tick_prices(int hub_id);
    const std::deque<PriceSample>& price_history(int hub_id, Resource r) const;

    std::int64_t total_escrowed_cash() const;
    std::int64_t total_escrowed_qty(Resource r) const;

    // All hubs (foundation: one).
    const std::vector<int>& hub_ids() const { return hub_ids_; }

private:
    struct HubBooks {
        std::array<OrderBook, 3> books{};
        std::array<std::deque<PriceSample>, 3> history{};
    };
    std::unordered_map<int, HubBooks> hubs_;
    std::vector<int> hub_ids_;
};

} // namespace aeq
