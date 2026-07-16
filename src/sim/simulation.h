#pragma once

#include "sim/agent.h"
#include "sim/market.h"
#include "sim/rng.h"
#include "sim/telemetry.h"
#include "sim/world.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aeq {

struct SimConfig {
    std::uint64_t seed = 42;
    std::string out_dir; // empty = no CSV
};

// Read-only views for renderer / HUD.
struct SimulationView {
    int tick = 0;
    const World* world = nullptr;
    const std::vector<Agent>* agents = nullptr;
    const Market* market = nullptr;
    const Telemetry* telemetry = nullptr;
    std::int64_t central_ledger = 0;
    std::int64_t initial_money = 0;
};

class Simulation {
public:
    void init(const SimConfig& cfg);
    void tick();

    int tick_index() const { return tick_; }
    SimulationView view() const;

    // Sovereign commands (mutate via CentralLedger so money still conserves).
    void inject_cash(int agent_id, std::int64_t amount); // amount>0 inject, <0 drain
    void inject_cash_all(std::int64_t amount_each);
    void drought_at(Hex center, int radius = params::DROUGHT_RADIUS);

    // Conservation check — returns false if broken.
    bool money_conserved() const;
    std::int64_t total_agent_cash() const;
    std::int64_t central_ledger() const { return central_ledger_; }
    std::int64_t initial_money() const { return initial_money_; }

    int population() const;
    double current_gini() const;

    // FNV-1a over core state for determinism / parity tests.
    std::uint64_t state_hash() const;

    Telemetry& telemetry() { return telemetry_; }
    const Telemetry& telemetry() const { return telemetry_; }
    Market& market() { return market_; }
    const Market& market() const { return market_; }
    World& world() { return world_; }
    const World& world() const { return world_; }
    std::vector<Agent>& agents() { return agents_; }
    const std::vector<Agent>& agents() const { return agents_; }

    // Resource flow ledger: regen - consumption accounted.
    std::int64_t resource_flow_consumed(Resource r) const { return consumed_[static_cast<int>(r)]; }
    std::int64_t resource_flow_regen(Resource r) const { return regen_[static_cast<int>(r)]; }

private:
    void step_regen();
    void step_metabolism();
    void step_decisions();
    void step_movement_harvest();
    void step_orders();
    void step_match();
    void step_expire();
    void step_telemetry();
    void apply_refund(const EscrowRefund& r, Resource res_for_qty);
    void kill_agent(Agent& a);
    Hex step_toward(Hex from, Hex to) const;
    Hex find_harvest_tile(const Agent& a, Resource r) const;
    static const char* action_label(ActionKind k);

    SimConfig cfg_{};
    Rng rng_{42};
    World world_;
    Market market_;
    Telemetry telemetry_;
    std::vector<Agent> agents_;
    std::unique_ptr<Policy> policy_;
    std::vector<Action> pending_; // per-agent decision this tick

    int tick_ = 0;
    std::int64_t central_ledger_ = 0;
    std::int64_t initial_money_ = 0;
    std::int64_t regen_[3]{};
    std::int64_t consumed_[3]{};
    // Pending sell resource for escrow qty refunds during match/expire (per book).
};

} // namespace aeq
