#include "sim/simulation.h"

#include "sim/params.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>

namespace aeq {
namespace {

#ifdef NDEBUG
#define AEQ_ASSERT(x) ((void)0)
#else
#define AEQ_ASSERT(x) assert(x)
#endif

} // namespace

void Simulation::init(const SimConfig& cfg) {
    cfg_ = cfg;
    rng_ = Rng(cfg.seed);
    tick_ = 0;
    central_ledger_ = 0;
    regen_[0] = regen_[1] = regen_[2] = 0;
    consumed_[0] = consumed_[1] = consumed_[2] = 0;

    world_.generate(params::MAP_RADIUS, rng_);
    market_ = Market{};
    market_.init_hub(params::HUB_ID);

    agents_.clear();
    agents_.reserve(static_cast<std::size_t>(params::AGENT_COUNT));
    // Place agents on non-water tiles.
    std::vector<Hex> land;
    for (const auto& t : world_.tiles()) {
        if (t.biome != Biome::Water) {
            land.push_back(t.hex);
        }
    }
    for (int i = 0; i < params::AGENT_COUNT; ++i) {
        Agent a;
        a.id = i;
        a.pos = land[static_cast<std::size_t>(rng_.uniform_int(0, static_cast<int>(land.size()) - 1))];
        a.prev_pos = a.pos;
        a.cash = params::INITIAL_CASH;
        a.inventory = {params::INITIAL_FOOD, 0, 0};
        a.hunger = 0;
        a.alive = true;
        agents_.push_back(a);
    }

    initial_money_ = static_cast<std::int64_t>(params::AGENT_COUNT) * params::INITIAL_CASH;
    policy_ = std::make_unique<HeuristicPolicy>();
    pending_.assign(static_cast<std::size_t>(params::AGENT_COUNT), Action{});

    telemetry_ = Telemetry{};
    if (!cfg.out_dir.empty()) {
        telemetry_.open(cfg.out_dir);
    }
    std::int64_t base[3] = {params::DEFAULT_PRICE, params::DEFAULT_PRICE, params::DEFAULT_PRICE};
    telemetry_.set_baseline(base);

    AEQ_ASSERT(money_conserved());
}

SimulationView Simulation::view() const {
    SimulationView v;
    v.tick = tick_;
    v.world = &world_;
    v.agents = &agents_;
    v.market = &market_;
    v.telemetry = &telemetry_;
    v.central_ledger = central_ledger_;
    v.initial_money = initial_money_;
    return v;
}

void Simulation::tick() {
    ++tick_;
    step_regen();
    step_metabolism();
    step_decisions();
    step_movement_harvest();
    step_orders();
    step_match();
    step_expire();
    step_telemetry();
    AEQ_ASSERT(money_conserved());
}

void Simulation::step_regen() {
    std::int64_t added[3]{};
    world_.regenerate(added);
    for (int i = 0; i < 3; ++i) {
        regen_[i] += added[i];
    }
}

void Simulation::kill_agent(Agent& a) {
    if (!a.alive) {
        return;
    }
    // Cancel open orders first so escrow returns to this still-alive agent, then transfer.
    for (int ri = 0; ri < 3; ++ri) {
        auto refunds =
            market_.book(params::HUB_ID, static_cast<Resource>(ri)).cancel_agent(a.id);
        for (const auto& r : refunds) {
            apply_refund(r, static_cast<Resource>(ri));
        }
    }
    // Return inventory to matching biome tile stocks (matter conservation).
    for (int ri = 0; ri < 3; ++ri) {
        const int qty = a.inventory[static_cast<std::size_t>(ri)];
        a.inventory[static_cast<std::size_t>(ri)] = 0;
        if (qty <= 0) {
            continue;
        }
        bool placed = false;
        if (Tile* here = world_.try_get(a.pos);
            here && here->biome != Biome::Water &&
            biome_resource(here->biome) == static_cast<Resource>(ri)) {
            here->stock += qty;
            placed = true;
        }
        if (!placed) {
            for (auto& tile : world_.tiles()) {
                if (tile.biome != Biome::Water &&
                    biome_resource(tile.biome) == static_cast<Resource>(ri)) {
                    tile.stock += qty;
                    placed = true;
                    break;
                }
            }
        }
        if (!placed) {
            consumed_[ri] += qty;
        }
    }
    central_ledger_ += a.cash;
    a.cash = 0;
    a.alive = false;
    a.last_action_label = "dead";
}

void Simulation::step_metabolism() {
    for (auto& a : agents_) {
        if (!a.alive) {
            continue;
        }
        a.hunger += params::HUNGER_PER_TICK;
        if (a.hunger >= params::HUNGER_MAX) {
            kill_agent(a);
        }
    }
}

const char* Simulation::action_label(ActionKind k) {
    switch (k) {
    case ActionKind::Eat:
        return "eat";
    case ActionKind::Harvest:
        return "harvest";
    case ActionKind::MoveTo:
        return "move";
    case ActionKind::BuyLimit:
        return "buy";
    case ActionKind::SellLimit:
        return "sell";
    default:
        return "idle";
    }
}

void Simulation::step_decisions() {
    const MarketView mv{market_.quotes(params::HUB_ID)};
    for (auto& a : agents_) {
        if (!a.alive) {
            pending_[static_cast<std::size_t>(a.id)] = Action{};
            continue;
        }
        AgentView av{a, world_.hub()};
        Action act = policy_->decide(av, mv, rng_);
        a.last_action = act;
        a.last_action_label = action_label(act.kind);
        pending_[static_cast<std::size_t>(a.id)] = act;
    }
}

Hex Simulation::step_toward(Hex from, Hex to) const {
    if (from == to) {
        return from;
    }
    Hex best = from;
    int best_d = hex_distance(from, to);
    for (int d = 0; d < 6; ++d) {
        Hex n = hex_neighbor(from, d);
        if (const Tile* t = world_.try_get(n); t && t->biome != Biome::Water) {
            const int dist = hex_distance(n, to);
            if (dist < best_d) {
                best_d = dist;
                best = n;
            }
        }
    }
    return best;
}

Hex Simulation::find_harvest_tile(const Agent& a, Resource r) const {
    Hex best = a.pos;
    int best_score = -1;
    for (const auto& t : world_.tiles()) {
        if (t.biome == Biome::Water) {
            continue;
        }
        if (biome_resource(t.biome) != r) {
            continue;
        }
        if (t.stock <= 0) {
            continue;
        }
        const int dist = hex_distance(a.pos, t.hex);
        const int score = t.stock * 10 - dist;
        if (score > best_score) {
            best_score = score;
            best = t.hex;
        }
    }
    return best;
}

void Simulation::step_movement_harvest() {
    for (auto& a : agents_) {
        if (!a.alive) {
            continue;
        }
        a.prev_pos = a.pos;
        Action& act = pending_[static_cast<std::size_t>(a.id)];

        if (act.kind == ActionKind::Eat) {
            if (a.inventory[0] > 0) {
                a.inventory[0] -= 1;
                a.hunger = 0;
                consumed_[0] += 1;
            }
            continue;
        }

        if (act.kind == ActionKind::MoveTo) {
            a.pos = step_toward(a.pos, act.target);
            continue;
        }

        if (act.kind == ActionKind::Harvest) {
            const Hex target = find_harvest_tile(a, act.resource);
            if (a.pos != target) {
                a.pos = step_toward(a.pos, target);
                a.last_action_label = "to_harvest";
            } else if (Tile* t = world_.try_get(a.pos)) {
                if (t->biome != Biome::Water && biome_resource(t->biome) == act.resource &&
                    t->stock >= params::HARVEST_AMOUNT) {
                    t->stock -= params::HARVEST_AMOUNT;
                    a.inventory[static_cast<std::size_t>(act.resource)] += params::HARVEST_AMOUNT;
                }
            }
            continue;
        }

        // Buy/Sell: move to hub if needed (policy usually emits MoveTo first).
        if (act.kind == ActionKind::BuyLimit || act.kind == ActionKind::SellLimit) {
            if (a.pos != world_.hub()) {
                a.pos = step_toward(a.pos, world_.hub());
            }
        }
    }
}

void Simulation::apply_refund(const EscrowRefund& r, Resource res_for_qty) {
    if (r.agent_id < 0 || r.agent_id >= static_cast<int>(agents_.size())) {
        return;
    }
    Agent& a = agents_[static_cast<std::size_t>(r.agent_id)];
    if (!a.alive) {
        central_ledger_ += r.cash;
        // Goods from dead agent already handled; dump qty to world.
        if (r.qty > 0) {
            for (auto& tile : world_.tiles()) {
                if (tile.biome != Biome::Water && biome_resource(tile.biome) == res_for_qty) {
                    tile.stock += static_cast<int>(r.qty);
                    break;
                }
            }
        }
        return;
    }
    a.cash += r.cash;
    a.inventory[static_cast<std::size_t>(res_for_qty)] += static_cast<int>(r.qty);
}

void Simulation::step_orders() {
    for (auto& a : agents_) {
        if (!a.alive) {
            continue;
        }
        const Action& act = pending_[static_cast<std::size_t>(a.id)];
        if (a.pos != world_.hub()) {
            continue;
        }
        if (act.kind == ActionKind::BuyLimit) {
            const std::int64_t cost = act.price * act.qty;
            if (act.qty <= 0 || act.price <= 0 || a.cash < cost) {
                continue;
            }
            a.cash -= cost;
            Order o;
            o.agent_id = a.id;
            o.side = Side::Buy;
            o.type = OrderType::Limit;
            o.price = act.price;
            o.qty = act.qty;
            o.escrow_cash = cost;
            o.placed_tick = tick_;
            o.expire_tick = tick_ + params::ORDER_EXPIRY_TICKS;
            market_.book(params::HUB_ID, act.resource).place(o);
        } else if (act.kind == ActionKind::SellLimit) {
            const int inv = a.inventory[static_cast<std::size_t>(act.resource)];
            if (act.qty <= 0 || act.price <= 0 || inv < act.qty) {
                continue;
            }
            a.inventory[static_cast<std::size_t>(act.resource)] -= static_cast<int>(act.qty);
            Order o;
            o.agent_id = a.id;
            o.side = Side::Sell;
            o.type = OrderType::Limit;
            o.price = act.price;
            o.qty = act.qty;
            o.escrow_qty = act.qty;
            o.placed_tick = tick_;
            o.expire_tick = tick_ + params::ORDER_EXPIRY_TICKS;
            market_.book(params::HUB_ID, act.resource).place(o);
        }
    }
}

void Simulation::step_match() {
    for (int ri = 0; ri < 3; ++ri) {
        auto& book = market_.book(params::HUB_ID, static_cast<Resource>(ri));
        auto result = book.match(tick_);
        for (const auto& t : result.trades) {
            Agent& buyer = agents_[static_cast<std::size_t>(t.buyer_id)];
            Agent& seller = agents_[static_cast<std::size_t>(t.seller_id)];
            // Cash: already escrowed from buyer; pay seller.
            // Buyer escrow reduced inside book; seller gets cash now.
            if (seller.alive) {
                seller.cash += t.price * t.qty;
            } else {
                central_ledger_ += t.price * t.qty;
            }
            // Goods: already escrowed from seller; deliver to buyer.
            if (buyer.alive) {
                buyer.inventory[static_cast<std::size_t>(ri)] += static_cast<int>(t.qty);
            } else {
                // Return to world.
                for (auto& tile : world_.tiles()) {
                    if (tile.biome != Biome::Water &&
                        biome_resource(tile.biome) == static_cast<Resource>(ri)) {
                        tile.stock += static_cast<int>(t.qty);
                        break;
                    }
                }
            }
            telemetry_.log_trade({tick_, ri, t.price, t.qty, t.buyer_id, t.seller_id});
        }
        for (const auto& r : result.leftover_refunds) {
            apply_refund(r, static_cast<Resource>(ri));
        }
        book.clear_trade_log();
    }
}

void Simulation::step_expire() {
    for (int ri = 0; ri < 3; ++ri) {
        auto refunds = market_.book(params::HUB_ID, static_cast<Resource>(ri)).expire(tick_);
        for (const auto& r : refunds) {
            apply_refund(r, static_cast<Resource>(ri));
        }
    }
}

void Simulation::step_telemetry() {
    market_.record_tick_prices(params::HUB_ID);
    MacroSample s;
    s.tick = tick_;
    s.population = population();
    s.money_supply = initial_money_; // conserved total
    const auto q = market_.quotes(params::HUB_ID);
    double idx = 0.0;
    for (int i = 0; i < 3; ++i) {
        s.last_price[i] = q.last_price[i];
        s.volume[i] = market_.book(params::HUB_ID, static_cast<Resource>(i)).last_volume();
        s.best_bid[i] = q.best_bid[i].value_or(0);
        s.best_ask[i] = q.best_ask[i].value_or(0);
        idx += static_cast<double>(q.last_price[i]) / telemetry_.baseline_price(i);
    }
    s.price_index = idx / 3.0;
    world_.total_stock(s.tile_stock);

    std::vector<double> wealth;
    wealth.reserve(agents_.size());
    for (const auto& a : agents_) {
        if (!a.alive) {
            continue;
        }
        double w = static_cast<double>(a.cash);
        for (int i = 0; i < 3; ++i) {
            w += static_cast<double>(a.inventory[static_cast<std::size_t>(i)]) *
                 static_cast<double>(q.last_price[i]);
        }
        wealth.push_back(w);
    }
    s.gini = Telemetry::gini_coefficient(std::move(wealth));
    telemetry_.sample(s);
}

bool Simulation::money_conserved() const {
    const std::int64_t total =
        total_agent_cash() + market_.total_escrowed_cash() + central_ledger_;
    return total == initial_money_;
}

std::int64_t Simulation::total_agent_cash() const {
    std::int64_t s = 0;
    for (const auto& a : agents_) {
        if (a.alive) {
            s += a.cash;
        }
    }
    return s;
}

int Simulation::population() const {
    int n = 0;
    for (const auto& a : agents_) {
        if (a.alive) {
            ++n;
        }
    }
    return n;
}

double Simulation::current_gini() const {
    if (telemetry_.ring().empty()) {
        return 0.0;
    }
    return telemetry_.ring().back().gini;
}

void Simulation::inject_cash(int agent_id, std::int64_t amount) {
    if (agent_id < 0 || agent_id >= static_cast<int>(agents_.size())) {
        return;
    }
    Agent& a = agents_[static_cast<std::size_t>(agent_id)];
    if (!a.alive) {
        return;
    }
    if (amount > 0) {
        // Inject from ledger (may go negative = money creation tracked).
        central_ledger_ -= amount;
        a.cash += amount;
        initial_money_ += amount; // expand supply so invariant holds
    } else if (amount < 0) {
        const std::int64_t drain = std::min(a.cash, -amount);
        a.cash -= drain;
        central_ledger_ += drain;
    }
    std::ostringstream oss;
    oss << "agent=" << agent_id << " delta=" << amount;
    telemetry_.log_event({tick_, "cash", oss.str()});
}

void Simulation::inject_cash_all(std::int64_t amount_each) {
    for (auto& a : agents_) {
        if (a.alive) {
            inject_cash(a.id, amount_each);
        }
    }
}

void Simulation::drought_at(Hex center, int radius) {
    world_.drought(center, radius, params::DROUGHT_FACTOR);
    std::ostringstream oss;
    oss << "q=" << center.q << " r=" << center.r << " radius=" << radius;
    telemetry_.log_event({tick_, "drought", oss.str()});
}

std::uint64_t Simulation::state_hash() const {
    std::uint64_t h = 14695981039346656037ull;
    auto mix = [&](std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<std::uint64_t>(tick_));
    mix(static_cast<std::uint64_t>(central_ledger_));
    for (const auto& a : agents_) {
        mix(static_cast<std::uint64_t>(a.id));
        mix(a.alive ? 1ull : 0ull);
        mix(static_cast<std::uint64_t>(a.pos.q) + 1000ull);
        mix(static_cast<std::uint64_t>(a.pos.r) + 1000ull);
        mix(static_cast<std::uint64_t>(a.cash));
        mix(static_cast<std::uint64_t>(a.hunger));
        for (int i = 0; i < 3; ++i) {
            mix(static_cast<std::uint64_t>(a.inventory[static_cast<std::size_t>(i)]));
        }
    }
    for (const auto& t : world_.tiles()) {
        mix(static_cast<std::uint64_t>(t.stock));
        mix(static_cast<std::uint64_t>(t.biome));
    }
    for (int i = 0; i < 3; ++i) {
        mix(static_cast<std::uint64_t>(
            market_.book(params::HUB_ID, static_cast<Resource>(i)).last_price()));
        mix(static_cast<std::uint64_t>(
            market_.book(params::HUB_ID, static_cast<Resource>(i)).escrowed_cash()));
    }
    return h;
}

} // namespace aeq
