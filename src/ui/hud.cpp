#include "ui/hud.h"

#include "sim/params.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <implot.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace aeq {
namespace {

const char* resource_short_name(Resource r) {
    switch (r) {
    case Resource::Food:
        return "Food";
    case Resource::Wood:
        return "Wood";
    case Resource::Stone:
        return "Stone";
    default:
        return "?";
    }
}

} // namespace

void Hud::apply_dark_theme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.f;
    style.FrameRounding = 3.f;
    style.GrabRounding = 3.f;
    style.ScrollbarRounding = 3.f;
    style.WindowBorderSize = 1.f;
    style.FrameBorderSize = 0.f;
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.12f, 0.94f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.18f, 0.22f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.28f, 0.24f, 0.8f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.38f, 0.30f, 0.9f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.30f, 0.25f, 0.85f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.40f, 0.32f, 0.95f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.48f, 0.36f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.14f, 0.16f, 0.9f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.20f, 0.22f, 0.9f);
    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.18f, 0.18f, 0.9f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.38f, 0.30f, 0.9f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.32f, 0.26f, 1.0f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.70f, 0.66f, 0.40f, 1.0f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.60f, 0.50f, 0.30f, 1.0f);
}

void Hud::init(VkContext& ctx, GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    apply_dark_theme();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.ApiVersion = VK_API_VERSION_1_3;
    init_info.Instance = ctx.instance();
    init_info.PhysicalDevice = ctx.physical_device();
    init_info.Device = ctx.device();
    init_info.QueueFamily = ctx.graphics_queue_family();
    init_info.Queue = ctx.graphics_queue();
    init_info.DescriptorPoolSize = 8;
    init_info.MinImageCount = 2;
    init_info.ImageCount = std::max<std::uint32_t>(2, ctx.swapchain_image_count());
    init_info.UseDynamicRendering = true;

    const VkFormat color_format = ctx.swapchain_format();
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = ctx.depth_format();

    ImGui_ImplVulkan_Init(&init_info);

    push_event("Aequitas HUD ready.");
}

void Hud::shutdown(VkContext& ctx) {
    ctx.wait_idle();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void Hud::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Hud::end_frame(VkContext& ctx, const FrameContext& frame) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr) {
        return;
    }

    VkRenderingAttachmentInfo color_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color_attachment.imageView = frame.swapchain_view;
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingAttachmentInfo depth_attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth_attachment.imageView = ctx.depth_view();
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    VkRenderingInfo rendering_info{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, frame.extent};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    // Ensure the 3D scene pass's attachment writes are visible before the UI pass loads them
    // (two separate dynamic-rendering instances on the same command buffer are not implicitly
    // ordered the way subpasses within one VkRenderPass would be).
    VkImageMemoryBarrier color_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    color_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    color_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    color_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    color_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    color_barrier.image = frame.swapchain_image;
    color_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(frame.cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &color_barrier);

    vkCmdBeginRendering(frame.cmd, &rendering_info);
    ImGui_ImplVulkan_RenderDrawData(draw_data, frame.cmd);
    vkCmdEndRendering(frame.cmd);
}

bool Hud::want_capture_mouse() const { return ImGui::GetIO().WantCaptureMouse; }
bool Hud::want_capture_keyboard() const { return ImGui::GetIO().WantCaptureKeyboard; }

void Hud::push_event(const std::string& text) {
    event_log_.push_back(text);
    while (event_log_.size() > 40) {
        event_log_.pop_front();
    }
}

void Hud::detect_auto_events(const Simulation& sim) {
    const int tick = sim.tick_index();
    if (tick == last_tick_seen_) {
        return;
    }
    last_tick_seen_ = tick;

    const int pop = sim.population();
    if (last_population_ >= 0 && pop < last_population_) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%d agent(s) died (tick %d, pop=%d)", last_population_ - pop, tick, pop);
        push_event(buf);
    }
    last_population_ = pop;
}

void Hud::draw(Simulation& sim, bool& paused, bool& step_once, int& speed_multiplier, SimMode& mode, float fps) {
    detect_auto_events(sim);
    draw_control_window(sim, paused, step_once, speed_multiplier, mode, fps);
    draw_inspector_window(sim);
    draw_market_window(sim);
    draw_macro_window(sim);
}

void Hud::draw_control_window(Simulation& sim, bool& paused, bool& step_once, int& speed_multiplier, SimMode& mode,
                               float fps) {
    ImGui::Begin("Control");
    ImGui::Text("Tick: %d", sim.tick_index());
    ImGui::Text("FPS: %.1f", static_cast<double>(fps));
    ImGui::Separator();

    if (ImGui::Button(paused ? "Resume" : "Pause")) {
        paused = !paused;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    if (ImGui::Button("Step")) {
        step_once = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    static const char* kSpeedLabels[] = {"1x", "2x", "4x", "8x", "Max"};
    static const int kSpeedValues[] = {1, 2, 4, 8, 0};
    int current = 0;
    for (int i = 0; i < 5; ++i) {
        if (kSpeedValues[i] == speed_multiplier) {
            current = i;
        }
    }
    if (ImGui::Combo("Speed", &current, kSpeedLabels, 5)) {
        speed_multiplier = kSpeedValues[current];
    }

    ImGui::Separator();
    int mode_i = static_cast<int>(mode);
    const char* kModeLabels[] = {"Observer", "Sovereign"};
    if (ImGui::Combo("Mode", &mode_i, kModeLabels, 2)) {
        mode = static_cast<SimMode>(mode_i);
    }

    if (mode == SimMode::Sovereign) {
        ImGui::Separator();
        ImGui::TextUnformatted("Sovereign Tools");
        ImGui::InputInt("Cash amount", &cash_amount_);

        ImGui::BeginDisabled(!selected_agent.has_value());
        if (ImGui::Button("Inject -> selected")) {
            sim.inject_cash(*selected_agent, cash_amount_);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Injected %d to agent #%d", cash_amount_, *selected_agent);
            push_event(buf);
        }
        ImGui::SameLine();
        if (ImGui::Button("Drain <- selected")) {
            sim.inject_cash(*selected_agent, -cash_amount_);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Drained %d from agent #%d", cash_amount_, *selected_agent);
            push_event(buf);
        }
        ImGui::EndDisabled();

        if (ImGui::Button("Inject -> all")) {
            sim.inject_cash_all(cash_amount_);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Injected %d to all agents", cash_amount_);
            push_event(buf);
        }
        ImGui::SameLine();
        if (ImGui::Button("Drain <- all")) {
            sim.inject_cash_all(-cash_amount_);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Drained %d from all agents", cash_amount_);
            push_event(buf);
        }

        ImGui::Separator();
        ImGui::BeginDisabled(!selected_tile.has_value());
        if (ImGui::Button("Drought at selected tile (r=2)")) {
            sim.drought_at(*selected_tile, params::DROUGHT_RADIUS);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Drought at (%d, %d)", selected_tile->q, selected_tile->r);
            push_event(buf);
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Events");
    ImGui::BeginChild("EventTicker", ImVec2(0.f, 120.f), true);
    for (auto it = event_log_.rbegin(); it != event_log_.rend(); ++it) {
        ImGui::TextUnformatted(it->c_str());
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextUnformatted("Controls");
    ImGui::BulletText("LMB — select tile / agent");
    ImGui::BulletText("RMB drag — pan (hand cursor)");
    ImGui::BulletText("MMB drag — orbit");
    ImGui::BulletText("Scroll — zoom");
    ImGui::BulletText("WASD — move look target");

    ImGui::End();
}

void Hud::draw_inspector_window(const Simulation& sim) {
    ImGui::Begin("Inspector");

    ImGui::SeparatorText("Hex coordinates");
    if (hover_tile.has_value()) {
        ImGui::Text("Hover:  q=%d  r=%d", hover_tile->q, hover_tile->r);
        const Vec2 w = hex_to_world(*hover_tile);
        ImGui::Text("         world xz=(%.2f, %.2f)", w.x, w.y);
    } else {
        ImGui::TextUnformatted("Hover:  (none)");
    }
    if (selected_tile.has_value()) {
        ImGui::Text("Select: q=%d  r=%d", selected_tile->q, selected_tile->r);
    } else if (selected_agent.has_value()) {
        ImGui::Text("Select: agent #%d", *selected_agent);
    } else {
        ImGui::TextUnformatted("Select: (none)");
    }
    ImGui::Separator();

    if (selected_agent.has_value()) {
        const auto& agents = sim.agents();
        const Agent* found = nullptr;
        for (const auto& a : agents) {
            if (a.id == *selected_agent) {
                found = &a;
                break;
            }
        }
        if (found != nullptr) {
            ImGui::Text("Agent #%d %s", found->id, found->alive ? "" : "(dead)");
            ImGui::Text("Pos: q=%d  r=%d", found->pos.q, found->pos.r);
            ImGui::Text("Cash: %lld", static_cast<long long>(found->cash));
            ImGui::Text("Food: %d  Wood: %d  Stone: %d", found->inventory[0], found->inventory[1],
                        found->inventory[2]);
            ImGui::Text("Hunger: %d / %d", found->hunger, params::HUNGER_MAX);
            ImGui::Text("Action: %s", found->last_action_label.c_str());
        } else {
            ImGui::TextUnformatted("(agent no longer exists)");
        }
    } else if (selected_tile.has_value()) {
        const Tile* t = sim.world().try_get(*selected_tile);
        if (t != nullptr) {
            ImGui::Text("Tile q=%d  r=%d", t->hex.q, t->hex.r);
            ImGui::Text("Biome: %s", biome_name(t->biome));
            ImGui::Text("Stock: %d / %d", t->stock, t->capacity);
            ImGui::Text("Resource: %s", resource_name(biome_resource(t->biome)));
        } else {
            ImGui::TextUnformatted("(no tile at selection — outside map?)");
            ImGui::Text("Clicked q=%d  r=%d", selected_tile->q, selected_tile->r);
        }
    } else {
        ImGui::TextUnformatted("LMB: select tile/agent. RMB: pan. MMB: orbit.");
    }

    ImGui::End();
}

void Hud::draw_market_window(const Simulation& sim) {
    ImGui::Begin("Market");

    if (ImGui::BeginTabBar("ResourceTabs")) {
        for (int i = 0; i < 3; ++i) {
            const Resource res = static_cast<Resource>(i);
            if (!ImGui::BeginTabItem(resource_short_name(res))) {
                continue;
            }

            const OrderBook& book = sim.market().book(params::HUB_ID, res);
            const auto& history = sim.market().price_history(params::HUB_ID, res);

            ImGui::Text("Last trade: %lld", static_cast<long long>(book.last_price()));
            ImGui::SameLine();
            if (book.best_bid().has_value()) {
                ImGui::Text(" | Bid: %lld", static_cast<long long>(*book.best_bid()));
            } else {
                ImGui::TextUnformatted(" | Bid: -");
            }
            ImGui::SameLine();
            if (book.best_ask().has_value()) {
                ImGui::Text(" | Ask: %lld", static_cast<long long>(*book.best_ask()));
            } else {
                ImGui::TextUnformatted(" | Ask: -");
            }

            if (ImPlot::BeginPlot("Price History", ImVec2(-1.f, 160.f))) {
                std::vector<double> xs;
                std::vector<double> ys;
                xs.reserve(history.size());
                ys.reserve(history.size());
                double idx = 0.0;
                for (const auto& sample : history) {
                    xs.push_back(idx);
                    ys.push_back(static_cast<double>(sample.price));
                    idx += 1.0;
                }
                if (!xs.empty()) {
                    ImPlot::PlotLine("Price", xs.data(), ys.data(), static_cast<int>(xs.size()));
                }
                ImPlot::EndPlot();
            }

            const std::vector<DepthLevel> bids = book.bid_depth(10);
            const std::vector<DepthLevel> asks = book.ask_depth(10);

            if (ImPlot::BeginPlot("Order Book Depth", ImVec2(-1.f, 160.f))) {
                std::vector<double> bid_px;
                std::vector<double> bid_cum;
                std::vector<double> ask_px;
                std::vector<double> ask_cum;
                double cum = 0.0;
                for (const auto& lvl : bids) {
                    cum += static_cast<double>(lvl.qty);
                    bid_px.push_back(static_cast<double>(lvl.price));
                    bid_cum.push_back(cum);
                }
                cum = 0.0;
                for (const auto& lvl : asks) {
                    cum += static_cast<double>(lvl.qty);
                    ask_px.push_back(static_cast<double>(lvl.price));
                    ask_cum.push_back(cum);
                }
                if (!bid_px.empty()) {
                    ImPlot::PlotStairs("Bids", bid_px.data(), bid_cum.data(), static_cast<int>(bid_px.size()));
                }
                if (!ask_px.empty()) {
                    ImPlot::PlotStairs("Asks", ask_px.data(), ask_cum.data(), static_cast<int>(ask_px.size()));
                }
                ImPlot::EndPlot();
            }

            if (ImGui::BeginTable("BookTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Bid Qty");
                ImGui::TableSetupColumn("Bid Px");
                ImGui::TableSetupColumn("Ask Px");
                ImGui::TableSetupColumn("Ask Qty");
                ImGui::TableHeadersRow();
                for (int row = 0; row < 10; ++row) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (row < static_cast<int>(bids.size())) {
                        ImGui::Text("%lld", static_cast<long long>(bids[static_cast<std::size_t>(row)].qty));
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(1);
                    if (row < static_cast<int>(bids.size())) {
                        ImGui::Text("%lld", static_cast<long long>(bids[static_cast<std::size_t>(row)].price));
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(2);
                    if (row < static_cast<int>(asks.size())) {
                        ImGui::Text("%lld", static_cast<long long>(asks[static_cast<std::size_t>(row)].price));
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                    ImGui::TableSetColumnIndex(3);
                    if (row < static_cast<int>(asks.size())) {
                        ImGui::Text("%lld", static_cast<long long>(asks[static_cast<std::size_t>(row)].qty));
                    } else {
                        ImGui::TextUnformatted("-");
                    }
                }
                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Hud::draw_macro_window(const Simulation& sim) {
    ImGui::Begin("Macro");

    ImGui::Text("Population: %d / %d", sim.population(), params::AGENT_COUNT);
    ImGui::Text("Gini: %.3f", sim.current_gini());
    ImGui::Text("Total agent cash: %lld", static_cast<long long>(sim.total_agent_cash()));
    ImGui::Text("Central ledger: %lld", static_cast<long long>(sim.central_ledger()));
    ImGui::Text("Money conserved: %s", sim.money_conserved() ? "yes" : "NO");

    if (!sim.telemetry().ring().empty()) {
        const MacroSample& s = sim.telemetry().ring().back();
        ImGui::Text("Price index: %.3f", s.price_index);
        ImGui::Text("Stocks — Food: %lld  Wood: %lld  Stone: %lld", static_cast<long long>(s.tile_stock[0]),
                    static_cast<long long>(s.tile_stock[1]), static_cast<long long>(s.tile_stock[2]));

        if (ImPlot::BeginPlot("Gini over time", ImVec2(-1.f, 150.f))) {
            std::vector<double> xs;
            std::vector<double> ys;
            xs.reserve(sim.telemetry().ring().size());
            ys.reserve(sim.telemetry().ring().size());
            for (const auto& sample : sim.telemetry().ring()) {
                xs.push_back(static_cast<double>(sample.tick));
                ys.push_back(sample.gini);
            }
            if (!xs.empty()) {
                ImPlot::PlotLine("Gini", xs.data(), ys.data(), static_cast<int>(xs.size()));
            }
            ImPlot::EndPlot();
        }

        if (ImPlot::BeginPlot("Price index over time", ImVec2(-1.f, 150.f))) {
            std::vector<double> xs;
            std::vector<double> ys;
            xs.reserve(sim.telemetry().ring().size());
            ys.reserve(sim.telemetry().ring().size());
            for (const auto& sample : sim.telemetry().ring()) {
                xs.push_back(static_cast<double>(sample.tick));
                ys.push_back(sample.price_index);
            }
            if (!xs.empty()) {
                ImPlot::PlotLine("Price Index", xs.data(), ys.data(), static_cast<int>(xs.size()));
            }
            ImPlot::EndPlot();
        }
    }

    ImGui::End();
}

} // namespace aeq
