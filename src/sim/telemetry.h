#pragma once

#include "sim/params.h"

#include <cstdint>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

namespace aeq {

struct MacroSample {
    int tick = 0;
    int population = 0;
    std::int64_t money_supply = 0;
    std::int64_t last_price[3]{};
    std::int64_t volume[3]{};
    std::int64_t best_bid[3]{};
    std::int64_t best_ask[3]{};
    double price_index = 1.0;
    double gini = 0.0;
    std::int64_t tile_stock[3]{};
};

struct TradeRow {
    int tick = 0;
    int resource = 0;
    std::int64_t price = 0;
    std::int64_t qty = 0;
    int buyer = -1;
    int seller = -1;
};

struct EventRow {
    int tick = 0;
    std::string kind;
    std::string detail;
};

class Telemetry {
public:
    void open(const std::string& out_dir);
    void close();

    void sample(const MacroSample& s);
    void log_trade(const TradeRow& t);
    void log_event(const EventRow& e);

    const std::deque<MacroSample>& ring() const { return ring_; }
    double baseline_price(int resource) const { return baseline_[resource]; }
    void set_baseline(const std::int64_t prices[3]);

    static double gini_coefficient(std::vector<double> wealth);

private:
    std::deque<MacroSample> ring_;
    double baseline_[3] = {100.0, 100.0, 100.0};
    std::ofstream macro_csv_;
    std::ofstream trades_csv_;
    std::ofstream events_csv_;
    bool open_ = false;
};

} // namespace aeq
