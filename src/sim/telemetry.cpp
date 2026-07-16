#include "sim/telemetry.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numeric>

namespace aeq {

void Telemetry::open(const std::string& out_dir) {
    namespace fs = std::filesystem;
    if (!out_dir.empty()) {
        fs::create_directories(out_dir);
        macro_csv_.open(fs::path(out_dir) / "macro.csv");
        trades_csv_.open(fs::path(out_dir) / "trades.csv");
        events_csv_.open(fs::path(out_dir) / "events.csv");
        if (macro_csv_) {
            macro_csv_ << "tick,population,money_supply,"
                       << "food_price,wood_price,stone_price,"
                       << "food_vol,wood_vol,stone_vol,"
                       << "food_bid,food_ask,wood_bid,wood_ask,stone_bid,stone_ask,"
                       << "price_index,gini,"
                       << "food_stock,wood_stock,stone_stock\n";
        }
        if (trades_csv_) {
            trades_csv_ << "tick,resource,price,qty,buyer,seller\n";
        }
        if (events_csv_) {
            events_csv_ << "tick,kind,detail\n";
        }
        open_ = macro_csv_.good();
    }
}

void Telemetry::close() {
    if (macro_csv_.is_open()) {
        macro_csv_.close();
    }
    if (trades_csv_.is_open()) {
        trades_csv_.close();
    }
    if (events_csv_.is_open()) {
        events_csv_.close();
    }
    open_ = false;
}

void Telemetry::set_baseline(const std::int64_t prices[3]) {
    for (int i = 0; i < 3; ++i) {
        baseline_[i] = prices[i] > 0 ? static_cast<double>(prices[i]) : 100.0;
    }
}

void Telemetry::sample(const MacroSample& s) {
    ring_.push_back(s);
    while (static_cast<int>(ring_.size()) > params::RING_CAPACITY) {
        ring_.pop_front();
    }
    if (macro_csv_) {
        macro_csv_ << s.tick << ',' << s.population << ',' << s.money_supply << ','
                   << s.last_price[0] << ',' << s.last_price[1] << ',' << s.last_price[2] << ','
                   << s.volume[0] << ',' << s.volume[1] << ',' << s.volume[2] << ','
                   << s.best_bid[0] << ',' << s.best_ask[0] << ',' << s.best_bid[1] << ','
                   << s.best_ask[1] << ',' << s.best_bid[2] << ',' << s.best_ask[2] << ','
                   << s.price_index << ',' << s.gini << ',' << s.tile_stock[0] << ','
                   << s.tile_stock[1] << ',' << s.tile_stock[2] << '\n';
    }
}

void Telemetry::log_trade(const TradeRow& t) {
    if (trades_csv_) {
        trades_csv_ << t.tick << ',' << t.resource << ',' << t.price << ',' << t.qty << ','
                    << t.buyer << ',' << t.seller << '\n';
    }
}

void Telemetry::log_event(const EventRow& e) {
    if (events_csv_) {
        events_csv_ << e.tick << ',' << e.kind << ',' << e.detail << '\n';
    }
}

double Telemetry::gini_coefficient(std::vector<double> wealth) {
    if (wealth.empty()) {
        return 0.0;
    }
    std::sort(wealth.begin(), wealth.end());
    const double n = static_cast<double>(wealth.size());
    double sum = 0.0;
    double weighted = 0.0;
    for (std::size_t i = 0; i < wealth.size(); ++i) {
        sum += wealth[i];
        weighted += wealth[i] * static_cast<double>(i + 1);
    }
    if (sum <= 0.0) {
        return 0.0;
    }
    // G = (2 * sum_i i*x_i) / (n * sum_x) - (n+1)/n
    return (2.0 * weighted) / (n * sum) - (n + 1.0) / n;
}

} // namespace aeq
