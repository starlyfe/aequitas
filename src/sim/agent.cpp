#include "sim/agent.h"

#include "sim/params.h"

#include <algorithm>
#include <cmath>

namespace aeq {

Action HeuristicPolicy::decide(const AgentView& self, const MarketView& market, Rng& rng) {
    const Agent& a = self.agent;
    Action act;

    // Survival first: eat, then forage. Never travel to the hub while food-stressed.
    if (a.hunger >= params::HUNGER_EAT_AT && a.inventory[0] > 0) {
        act.kind = ActionKind::Eat;
        act.resource = Resource::Food;
        return act;
    }

    if (a.inventory[0] < params::FOOD_KEEP) {
        // Buy only if already at the hub with a resting ask; otherwise forage.
        if (a.pos == self.hub && market.quotes.best_ask[0].has_value()) {
            const std::int64_t ask = *market.quotes.best_ask[0];
            const std::int64_t bid_price = std::max<std::int64_t>(
                1, static_cast<std::int64_t>(std::lround(
                       static_cast<double>(ask) * rng.noisy_mult(params::PRICE_NOISE))));
            if (a.cash >= bid_price) {
                act.kind = ActionKind::BuyLimit;
                act.resource = Resource::Food;
                act.price = bid_price;
                act.qty = 1;
                return act;
            }
        }
        act.kind = ActionKind::Harvest;
        act.resource = Resource::Food;
        return act;
    }

    // Opportunistic sell when already at hub with surplus.
    if (a.pos == self.hub) {
        for (int ri = 0; ri < 3; ++ri) {
            const int keep = (ri == 0)   ? params::FOOD_KEEP
                             : (ri == 1) ? params::WOOD_KEEP
                                         : params::STONE_KEEP;
            const int inv = a.inventory[static_cast<std::size_t>(ri)];
            if (inv <= keep) {
                continue;
            }
            const std::int64_t last = market.quotes.last_price[ri];
            const std::int64_t price = std::max<std::int64_t>(
                1, static_cast<std::int64_t>(std::lround(
                       static_cast<double>(last) * (1.0 + params::SELL_SPREAD) *
                       rng.noisy_mult(params::PRICE_NOISE))));
            const std::int64_t qty =
                std::min<std::int64_t>(params::MAX_ORDER_QTY, inv - keep);
            if (qty <= 0) {
                continue;
            }
            act.kind = ActionKind::SellLimit;
            act.resource = static_cast<Resource>(ri);
            act.price = price;
            act.qty = qty;
            return act;
        }
    }

    // Sometimes travel to hub to sell wood/stone surplus (only with healthy food buffer).
    if (a.inventory[0] >= params::FOOD_KEEP + 2 && a.hunger < 10) {
        for (int ri = 1; ri < 3; ++ri) {
            const int keep = (ri == 1) ? params::WOOD_KEEP : params::STONE_KEEP;
            if (a.inventory[static_cast<std::size_t>(ri)] > keep + 2) {
                act.kind = ActionKind::MoveTo;
                act.target = self.hub;
                return act;
            }
        }
    }

    // Default: harvest highest scarcity-weighted resource (food-biased).
    int best_r = 0;
    double best_score = -1.0;
    for (int ri = 0; ri < 3; ++ri) {
        const double inv = static_cast<double>(a.inventory[static_cast<std::size_t>(ri)] + 1);
        const double price = static_cast<double>(market.quotes.last_price[ri]);
        double score = price / inv + rng.uniform(0.0, 0.1);
        if (ri == 0) {
            score *= 1.4;
        }
        if (score > best_score) {
            best_score = score;
            best_r = ri;
        }
    }
    act.kind = ActionKind::Harvest;
    act.resource = static_cast<Resource>(best_r);
    return act;
}

} // namespace aeq
