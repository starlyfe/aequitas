#pragma once

#include <cstdint>

namespace aeq::params {

inline constexpr int MAP_RADIUS = 16;
inline constexpr float HEX_SIZE = 1.0f;

inline constexpr int AGENT_COUNT = 128;
inline constexpr std::int64_t INITIAL_CASH = 10'000;
inline constexpr int INITIAL_FOOD = 12;
inline constexpr int HUNGER_PER_TICK = 1;
inline constexpr int HUNGER_MAX = 80;
inline constexpr int HUNGER_EAT_AT = 15; // eat less often → lower ecological load
inline constexpr int FOOD_KEEP = 4;
inline constexpr int WOOD_KEEP = 0;
inline constexpr int STONE_KEEP = 0;
inline constexpr int HARVEST_AMOUNT = 2;
inline constexpr int MOVE_RANGE = 1;

inline constexpr int K_PLAINS = 14;
inline constexpr int K_FOREST = 10;
inline constexpr int K_MOUNTAIN = 6;
inline constexpr double REGEN_R = 0.30;

inline constexpr int HUB_ID = 0;
inline constexpr int ORDER_EXPIRY_TICKS = 12;
inline constexpr int DEFAULT_PRICE = 100;
inline constexpr double SELL_SPREAD = 0.03;
inline constexpr double PRICE_NOISE = 0.02;
inline constexpr int BUY_FOOD_THRESHOLD = 4;
inline constexpr std::int64_t MAX_ORDER_QTY = 5;

inline constexpr int RING_CAPACITY = 2048;

inline constexpr int DROUGHT_RADIUS = 2;
inline constexpr double DROUGHT_FACTOR = 0.1;
inline constexpr std::int64_t SOVEREIGN_CASH_DELTA = 1'000;

} // namespace aeq::params
