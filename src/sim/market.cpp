#include "sim/market.h"

#include <stdexcept>

namespace aeq {

void Market::init_hub(int hub_id) {
    HubBooks hb;
    for (int i = 0; i < 3; ++i) {
        hb.books[static_cast<std::size_t>(i)].set_last_price(params::DEFAULT_PRICE);
    }
    hubs_[hub_id] = std::move(hb);
    hub_ids_.push_back(hub_id);
}

OrderBook& Market::book(int hub_id, Resource r) {
    auto it = hubs_.find(hub_id);
    if (it == hubs_.end()) {
        throw std::runtime_error("unknown hub");
    }
    return it->second.books[static_cast<std::size_t>(r)];
}

const OrderBook& Market::book(int hub_id, Resource r) const {
    auto it = hubs_.find(hub_id);
    if (it == hubs_.end()) {
        throw std::runtime_error("unknown hub");
    }
    return it->second.books[static_cast<std::size_t>(r)];
}

MarketQuotes Market::quotes(int hub_id) const {
    MarketQuotes q;
    for (int i = 0; i < 3; ++i) {
        const auto& b = book(hub_id, static_cast<Resource>(i));
        q.last_price[i] = b.last_price();
        q.best_bid[i] = b.best_bid();
        q.best_ask[i] = b.best_ask();
    }
    return q;
}

void Market::record_tick_prices(int hub_id) {
    auto it = hubs_.find(hub_id);
    if (it == hubs_.end()) {
        return;
    }
    for (int i = 0; i < 3; ++i) {
        auto& hist = it->second.history[static_cast<std::size_t>(i)];
        const auto& b = it->second.books[static_cast<std::size_t>(i)];
        hist.push_back({b.last_price(), b.last_volume()});
        while (static_cast<int>(hist.size()) > params::RING_CAPACITY) {
            hist.pop_front();
        }
    }
}

const std::deque<PriceSample>& Market::price_history(int hub_id, Resource r) const {
    return hubs_.at(hub_id).history[static_cast<std::size_t>(r)];
}

std::int64_t Market::total_escrowed_cash() const {
    std::int64_t s = 0;
    for (const auto& [_, hb] : hubs_) {
        for (const auto& b : hb.books) {
            s += b.escrowed_cash();
        }
    }
    return s;
}

std::int64_t Market::total_escrowed_qty(Resource r) const {
    std::int64_t s = 0;
    for (const auto& [_, hb] : hubs_) {
        s += hb.books[static_cast<std::size_t>(r)].escrowed_qty();
    }
    return s;
}

} // namespace aeq
