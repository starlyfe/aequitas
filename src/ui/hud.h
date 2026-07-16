#pragma once

#include "render/vk_context.h"

#include "sim/hex.h"
#include "sim/simulation.h"

#include <deque>
#include <optional>
#include <string>

struct GLFWwindow;

namespace aeq {

enum class SimMode { Observer, Sovereign };

// Dear ImGui + ImPlot overlay: Control / Inspector / Market / Macro windows, sovereign tools,
// and a rolling event ticker. Uses imgui_impl_vulkan with dynamic rendering (no VkRenderPass).
class Hud {
public:
    void init(VkContext& ctx, GLFWwindow* window);
    void shutdown(VkContext& ctx);

    void begin_frame();
    void draw(Simulation& sim, bool& paused, bool& step_once, int& speed_multiplier, SimMode& mode, float fps);
    // Opens its own dynamic-rendering instance (color LOAD, no clear) layered on top of
    // whatever the 3D renderer already drew, so ImGui's draw calls have an active render
    // pass instance to record into.
    void end_frame(VkContext& ctx, const FrameContext& frame);

    bool want_capture_mouse() const;
    bool want_capture_keyboard() const;

    void push_event(const std::string& text);

    std::optional<int> selected_agent;
    std::optional<Hex> selected_tile;
    std::optional<Hex> hover_tile; // under-cursor hex (updated every frame)

private:
    void apply_dark_theme();
    void draw_control_window(Simulation& sim, bool& paused, bool& step_once, int& speed_multiplier, SimMode& mode,
                              float fps, int pos_cond);
    void draw_inspector_window(const Simulation& sim, int pos_cond);
    void draw_market_window(const Simulation& sim, int pos_cond);
    void draw_macro_window(const Simulation& sim, int pos_cond);
    void detect_auto_events(const Simulation& sim);

    std::deque<std::string> event_log_;
    int last_population_ = -1;
    int last_tick_seen_ = -1;
    int cash_amount_ = 1000;
};

} // namespace aeq
