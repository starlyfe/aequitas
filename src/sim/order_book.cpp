#include "sim/order_book.h"

#include <algorithm>

namespace aeq {

std::uint64_t OrderBook::place(Order order) {
    order.id = next_id_++;
    if (order.type == OrderType::Market) {
        order.price = (order.side == Side::Buy) ? std::int64_t{1'000'000'000} : std::int64_t{1};
    }
    if (order.side == Side::Buy) {
        if (order.escrow_cash == 0 && order.type == OrderType::Limit) {
            order.escrow_cash = order.price * order.qty;
        }
        bids_[order.price].push_back(order);
    } else {
        if (order.escrow_qty == 0) {
            order.escrow_qty = order.qty;
        }
        asks_[order.price].push_back(order);
    }
    return order.id;
}

std::optional<EscrowRefund> OrderBook::cancel(std::uint64_t order_id) {
    auto try_erase = [&](auto& book) -> std::optional<EscrowRefund> {
        for (auto it = book.begin(); it != book.end(); ++it) {
            auto& dq = it->second;
            for (auto oit = dq.begin(); oit != dq.end(); ++oit) {
                if (oit->id == order_id) {
                    EscrowRefund r{oit->agent_id, oit->escrow_cash, oit->escrow_qty};
                    dq.erase(oit);
                    if (dq.empty()) {
                        book.erase(it);
                    }
                    return r;
                }
            }
        }
        return std::nullopt;
    };
    if (auto r = try_erase(bids_)) {
        return r;
    }
    return try_erase(asks_);
}

std::vector<EscrowRefund> OrderBook::cancel_agent(int agent_id) {
    std::vector<EscrowRefund> out;
    auto sweep = [&](auto& book) {
        for (auto it = book.begin(); it != book.end();) {
            auto& dq = it->second;
            for (auto oit = dq.begin(); oit != dq.end();) {
                if (oit->agent_id == agent_id) {
                    out.push_back({oit->agent_id, oit->escrow_cash, oit->escrow_qty});
                    oit = dq.erase(oit);
                } else {
                    ++oit;
                }
            }
            if (dq.empty()) {
                it = book.erase(it);
            } else {
                ++it;
            }
        }
    };
    sweep(bids_);
    sweep(asks_);
    return out;
}

OrderBook::MatchResult OrderBook::match(int tick) {
    MatchResult result;
    while (!bids_.empty() && !asks_.empty()) {
        auto bit = bids_.begin();
        auto ait = asks_.begin();
        if (bit->first < ait->first) {
            break;
        }
        Order& buy = bit->second.front();
        Order& sell = ait->second.front();

        // Self-trade prevention: cancel the sell (aggressor typically).
        if (buy.agent_id == sell.agent_id) {
            result.leftover_refunds.push_back({sell.agent_id, sell.escrow_cash, sell.escrow_qty});
            ait->second.pop_front();
            if (ait->second.empty()) {
                asks_.erase(ait);
            }
            continue;
        }

        // Resting ask sets trade price (continuous double auction convention).
        const std::int64_t trade_price = sell.price;
        const std::int64_t qty = std::min(buy.qty, sell.qty);
        const std::int64_t cash_spent = trade_price * qty;

        Trade t;
        t.buy_order_id = buy.id;
        t.sell_order_id = sell.id;
        t.buyer_id = buy.agent_id;
        t.seller_id = sell.agent_id;
        t.price = trade_price;
        t.qty = qty;
        t.tick = tick;
        result.trades.push_back(t);
        trade_log_.push_back(t);

        last_price_ = trade_price;
        last_volume_ = qty;

        buy.qty -= qty;
        sell.qty -= qty;
        buy.escrow_cash = (buy.escrow_cash >= cash_spent) ? (buy.escrow_cash - cash_spent) : 0;
        sell.escrow_qty = (sell.escrow_qty >= qty) ? (sell.escrow_qty - qty) : 0;

        if (buy.qty == 0) {
            if (buy.escrow_cash > 0) {
                result.leftover_refunds.push_back({buy.agent_id, buy.escrow_cash, 0});
            }
            bit->second.pop_front();
            if (bit->second.empty()) {
                bids_.erase(bit);
            }
        }
        if (sell.qty == 0) {
            ait->second.pop_front();
            if (ait->second.empty()) {
                asks_.erase(ait);
            }
        }
    }
    return result;
}

std::vector<EscrowRefund> OrderBook::expire(int tick) {
    std::vector<EscrowRefund> refunds;
    auto sweep = [&](auto& book) {
        for (auto it = book.begin(); it != book.end();) {
            auto& dq = it->second;
            for (auto oit = dq.begin(); oit != dq.end();) {
                if (oit->expire_tick <= tick) {
                    refunds.push_back({oit->agent_id, oit->escrow_cash, oit->escrow_qty});
                    oit = dq.erase(oit);
                } else {
                    ++oit;
                }
            }
            if (dq.empty()) {
                it = book.erase(it);
            } else {
                ++it;
            }
        }
    };
    sweep(bids_);
    sweep(asks_);
    return refunds;
}

std::optional<std::int64_t> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<std::int64_t> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::vector<DepthLevel> OrderBook::bid_depth(int levels) const {
    std::vector<DepthLevel> out;
    for (const auto& [price, dq] : bids_) {
        std::int64_t q = 0;
        for (const auto& o : dq) {
            q += o.qty;
        }
        out.push_back({price, q});
        if (static_cast<int>(out.size()) >= levels) {
            break;
        }
    }
    return out;
}

std::vector<DepthLevel> OrderBook::ask_depth(int levels) const {
    std::vector<DepthLevel> out;
    for (const auto& [price, dq] : asks_) {
        std::int64_t q = 0;
        for (const auto& o : dq) {
            q += o.qty;
        }
        out.push_back({price, q});
        if (static_cast<int>(out.size()) >= levels) {
            break;
        }
    }
    return out;
}

std::int64_t OrderBook::escrowed_cash() const {
    std::int64_t s = 0;
    for (const auto& [_, dq] : bids_) {
        for (const auto& o : dq) {
            s += o.escrow_cash;
        }
    }
    return s;
}

std::int64_t OrderBook::escrowed_qty() const {
    std::int64_t s = 0;
    for (const auto& [_, dq] : asks_) {
        for (const auto& o : dq) {
            s += o.escrow_qty;
        }
    }
    return s;
}

} // namespace aeq
