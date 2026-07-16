#pragma once

#include "sim/hex.h"
#include "sim/rng.h"
#include "sim/world.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aeq {

enum class ActionKind : std::uint8_t {
    None = 0,
    Eat,
    Harvest,
    MoveTo,
    BuyLimit,
    SellLimit,
};

struct Action {
    ActionKind kind = ActionKind::None;
    Hex target{};
    Resource resource = Resource::Food;
    std::int64_t price = 0;
    std::int64_t qty = 0;
};

struct Agent {
    int id = 0;
    Hex pos{};
    Hex prev_pos{};
    std::int64_t cash = 0;
    std::array<int, 3> inventory{}; // Food, Wood, Stone
    int hunger = 0;
    bool alive = true;
    Action last_action{};
    std::string last_action_label = "idle";
};

// Read-only slices for Policy (no mutation).
struct AgentView {
    const Agent& agent;
    Hex hub;
};

struct MarketQuotes {
    std::int64_t last_price[3]{};
    std::optional<std::int64_t> best_bid[3]{};
    std::optional<std::int64_t> best_ask[3]{};
};

struct MarketView {
    MarketQuotes quotes;
};

// Expansion seam: implement Q-learning / AR / PlayerPolicy behind this.
class Policy {
public:
    virtual ~Policy() = default;
    virtual Action decide(const AgentView& self, const MarketView& market, Rng& rng) = 0;
};

class HeuristicPolicy : public Policy {
public:
    Action decide(const AgentView& self, const MarketView& market, Rng& rng) override;
};

} // namespace aeq
