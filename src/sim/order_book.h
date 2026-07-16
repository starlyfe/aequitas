#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace aeq {

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : std::uint8_t { Limit = 0, Market = 1 };

struct Order {
    std::uint64_t id = 0;
    int agent_id = -1;
    Side side = Side::Buy;
    OrderType type = OrderType::Limit;
    std::int64_t price = 0; // cents; ignored for market orders on matching side
    std::int64_t qty = 0;   // remaining
    std::int64_t escrow_cash = 0; // buyer: price*qty locked; seller: 0
    std::int64_t escrow_qty = 0;  // seller: qty locked; buyer: 0
    int placed_tick = 0;
    int expire_tick = 0;
};

struct Trade {
    std::uint64_t buy_order_id = 0;
    std::uint64_t sell_order_id = 0;
    int buyer_id = -1;
    int seller_id = -1;
    std::int64_t price = 0;
    std::int64_t qty = 0;
    int tick = 0;
};

struct DepthLevel {
    std::int64_t price = 0;
    std::int64_t qty = 0;
};

struct EscrowRefund {
    int agent_id = -1;
    std::int64_t cash = 0;
    std::int64_t qty = 0; // goods returned to seller
};

// Pure price-time-priority LOB. Escrow accounting is the caller's responsibility
// to apply to agent wallets; this class tracks escrow amounts on each Order.
class OrderBook {
public:
    std::uint64_t place(Order order);
    // Cancel by id; returns refund info if found.
    std::optional<EscrowRefund> cancel(std::uint64_t order_id);
    struct MatchResult {
        std::vector<Trade> trades;
        std::vector<EscrowRefund> leftover_refunds; // unused buyer escrow after fills
    };
    // Match crossing orders. Updates escrow on residual legs.
    MatchResult match(int tick);
    // Expire orders with expire_tick <= tick; returns refunds.
    std::vector<EscrowRefund> expire(int tick);

    std::optional<std::int64_t> best_bid() const;
    std::optional<std::int64_t> best_ask() const;
    std::int64_t last_price() const { return last_price_; }
    std::int64_t last_volume() const { return last_volume_; }
    void set_last_price(std::int64_t p) { last_price_ = p; }

    std::vector<DepthLevel> bid_depth(int levels = 10) const;
    std::vector<DepthLevel> ask_depth(int levels = 10) const;

    // Sum of escrowed cash / goods currently in open orders.
    std::int64_t escrowed_cash() const;
    std::int64_t escrowed_qty() const;

    // Cancel all orders for an agent (e.g. on death).
    std::vector<EscrowRefund> cancel_agent(int agent_id);

    const std::vector<Trade>& trade_log() const { return trade_log_; }
    void clear_trade_log() { trade_log_.clear(); }

private:
    // Bids: highest price first → map with greater<>; Asks: lowest first → map default.
    std::map<std::int64_t, std::deque<Order>, std::greater<std::int64_t>> bids_;
    std::map<std::int64_t, std::deque<Order>> asks_;
    std::uint64_t next_id_ = 1;
    std::int64_t last_price_ = 100;
    std::int64_t last_volume_ = 0;
    std::vector<Trade> trade_log_;

    Order* find_mutable(std::uint64_t id);
};

} // namespace aeq
