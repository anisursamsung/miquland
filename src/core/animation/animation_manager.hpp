#pragma once

#include "core/common/util.hpp"
#include "core/animation/easing.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

struct wlr_scene_node;
struct wlr_scene_tree;

namespace miquland {

class Server;
class View;
class Workspace;

struct NodeAnimation {
    uint64_t id = 0;
    struct wlr_scene_node* node = nullptr;
    View* bound_view = nullptr;
    Workspace* bound_workspace = nullptr;
    int start_x = 0;
    int start_y = 0;
    int target_x = 0;
    int target_y = 0;
    uint64_t start_time_ms = 0;
    uint64_t duration_ms = 0;
    std::function<float(float)> easing_fn;
    std::function<void()> on_complete;
    bool completed = false;
};

class AnimationManager {
public:
    explicit AnimationManager(Server* server);
    ~AnimationManager();

    // High-level animations
    void schedule_workspace_transition(Workspace* from_ws, Workspace* to_ws, bool slide_right, int screen_width);

    // Interactive 1:1 Workspace Gestures
    void begin_workspace_swipe(int fingers);
    void update_workspace_swipe(double dx, double dy);
    void end_workspace_swipe(bool cancelled = false);
    bool is_workspace_swipe_active() const { return m_swipe_state.active; }
    bool is_workspace_animating() const;

    // Lifecycle cancellation
    void cancel_for_view(View* view);
    void cancel_for_workspace(Workspace* ws);
    void cancel_for_node(struct wlr_scene_node* node);
    void clear();

    // VSync Frame Driver
    void tick(uint64_t now_ms);
    bool has_active_animations() const { return !m_animations.empty() || m_swipe_state.active; }
    void schedule_next_frame();

private:
    struct WorkspaceSwipeState {
        bool active = false;
        Workspace* current_ws = nullptr;
        Workspace* target_ws = nullptr;
        size_t target_ws_id = 0;
        int direction = 0; // -1: left (next), +1: right (prev)
        double delta_x = 0.0;
        int screen_width = 1920;
    };

    uint64_t get_current_time_ms() const;

    Server* m_server = nullptr;
    std::vector<NodeAnimation> m_animations;
    WorkspaceSwipeState m_swipe_state;
    uint64_t m_next_id = 1;
};

} // namespace miquland
