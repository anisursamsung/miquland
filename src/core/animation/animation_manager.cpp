#include "core/animation/animation_manager.hpp"
#include "core/server.hpp"
#include "core/view.hpp"
#include "core/workspace.hpp"
#include "core/output.hpp"
#include "core/input/input.hpp"
#include "core/config/config.hpp"
#include <ctime>
#include <cmath>
#include <algorithm>

namespace miquland {

AnimationManager::AnimationManager(Server* server)
    : m_server(server)
{
}

AnimationManager::~AnimationManager() {
    clear();
}

uint64_t AnimationManager::get_current_time_ms() const {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000 + static_cast<uint64_t>(ts.tv_nsec) / 1000000;
}

void AnimationManager::schedule_workspace_transition(
    Workspace* from_ws,
    Workspace* to_ws,
    bool slide_right,
    int screen_width
) {
    if (!to_ws) return;

    if (!Config::get().is_animations_enabled() || !from_ws || screen_width <= 0) {
        if (from_ws) from_ws->set_visible(false);
        to_ws->set_visible(true);
        return;
    }

    cancel_for_workspace(from_ws);
    cancel_for_workspace(to_ws);

    int duration = Config::get().get_animation_duration_ms();
    auto easing = Easing::from_name(Config::get().get_animation_curve());
    int offset = slide_right ? screen_width : -screen_width;
    uint64_t now = get_current_time_ms();

    // Position target workspace off-screen and make visible
    if (to_ws->get_scene_tree()) {
        wlr_scene_node_set_position(&to_ws->get_scene_tree()->node, offset, 0);
    }
    to_ws->set_visible(true);

    // Animation 1: Move old workspace off-screen
    if (from_ws->get_scene_tree()) {
        Server* srv = m_server;
        size_t from_id = from_ws ? from_ws->get_id() : 0;
        NodeAnimation anim_from;
        anim_from.id = m_next_id++;
        anim_from.node = &from_ws->get_scene_tree()->node;
        anim_from.bound_workspace = from_ws;
        anim_from.start_x = 0;
        anim_from.start_y = 0;
        anim_from.target_x = -offset;
        anim_from.target_y = 0;
        anim_from.start_time_ms = now;
        anim_from.duration_ms = duration;
        anim_from.easing_fn = easing;
        anim_from.on_complete = [srv, from_ws, from_id]() {
            from_ws->set_visible(false);
            if (from_ws->get_scene_tree()) {
                wlr_scene_node_set_position(&from_ws->get_scene_tree()->node, 0, 0);
            }
            if (srv && srv->get_workspace_manager() && from_id > 0) {
                srv->get_workspace_manager()->prune_workspace(from_id);
            }
        };
        m_animations.push_back(std::move(anim_from));
    }

    // Animation 2: Move new workspace on-screen to 0,0
    if (to_ws->get_scene_tree()) {
        Server* srv = m_server;
        NodeAnimation anim_to;
        anim_to.id = m_next_id++;
        anim_to.node = &to_ws->get_scene_tree()->node;
        anim_to.bound_workspace = to_ws;
        anim_to.start_x = offset;
        anim_to.start_y = 0;
        anim_to.target_x = 0;
        anim_to.target_y = 0;
        anim_to.start_time_ms = now;
        anim_to.duration_ms = duration;
        anim_to.easing_fn = easing;
        anim_to.on_complete = [srv, to_ws]() {
            if (to_ws->get_scene_tree()) {
                wlr_scene_node_set_position(&to_ws->get_scene_tree()->node, 0, 0);
            }
            if (srv && srv->get_input_manager()) {
                srv->get_input_manager()->recheck_cursor_focus();
            }
        };
        m_animations.push_back(std::move(anim_to));
    }

    schedule_next_frame();
}

bool AnimationManager::is_workspace_animating() const {
    if (m_swipe_state.active) return true;
    for (const auto& anim : m_animations) {
        if (anim.bound_workspace != nullptr) return true;
    }
    return false;
}

void AnimationManager::begin_workspace_swipe(int fingers) {
    if (!Config::get().is_animations_enabled() || !m_server || !m_server->get_workspace_manager()) {
        return;
    }

    if (m_swipe_state.active) {
        end_workspace_swipe(true);
    }

    Workspace* current = m_server->get_workspace_manager()->get_active_workspace();
    if (!current) return;

    cancel_for_workspace(current);

    int screen_w = 1920;
    if (m_server->get_output_manager()) {
        struct wlr_box geom = m_server->get_output_manager()->get_primary_usable_geometry();
        if (geom.width > 0) screen_w = geom.width;
    }

    m_swipe_state.active = true;
    m_swipe_state.current_ws = current;
    m_swipe_state.target_ws = nullptr;
    m_swipe_state.target_ws_id = 0;
    m_swipe_state.direction = 0;
    m_swipe_state.delta_x = 0.0;
    m_swipe_state.screen_width = screen_w;
}

void AnimationManager::update_workspace_swipe(double dx, double dy) {
    if (!m_swipe_state.active || !m_swipe_state.current_ws) return;

    m_swipe_state.delta_x += dx;

    // Determine target direction:
    // delta_x < 0 means dragged left -> revealing next workspace from right (+screen_width)
    // delta_x > 0 means dragged right -> revealing prev workspace from left (-screen_width)
    int target_dir = 0;
    if (m_swipe_state.delta_x < -2.0) {
        target_dir = -1;
    } else if (m_swipe_state.delta_x > 2.0) {
        target_dir = 1;
    }

    if (target_dir != 0 && (target_dir != m_swipe_state.direction || !m_swipe_state.target_ws)) {
        size_t next_target_id = 0;
        if (target_dir == -1) {
            next_target_id = m_server->get_workspace_manager()->get_next_workspace_id();
        } else {
            next_target_id = m_server->get_workspace_manager()->get_prev_workspace_id();
        }

        if (next_target_id == m_swipe_state.current_ws->get_id()) {
            // Cannot swipe past boundary if cycling is disabled: apply rubber-band limit
            double limit = static_cast<double>(m_swipe_state.screen_width) * 0.15;
            m_swipe_state.delta_x = std::clamp(m_swipe_state.delta_x, -limit, limit);
        } else {
            if (m_swipe_state.target_ws && m_swipe_state.target_ws_id != next_target_id) {
                m_swipe_state.target_ws->set_visible(false);
                if (m_swipe_state.target_ws->get_scene_tree()) {
                    wlr_scene_node_set_position(&m_swipe_state.target_ws->get_scene_tree()->node, 0, 0);
                }
            }

            m_swipe_state.target_ws_id = next_target_id;
            m_swipe_state.target_ws = m_server->get_workspace_manager()->get_or_create_workspace(next_target_id);
            m_swipe_state.direction = target_dir;

            if (m_swipe_state.target_ws) {
                if (m_server->get_output_manager()) {
                    m_swipe_state.target_ws->recalculate_layout(m_server->get_output_manager()->get_primary_usable_geometry());
                }
                m_swipe_state.target_ws->set_visible(true);
            }
        }
    }

    // Clamp delta_x to [-screen_width, +screen_width]
    m_swipe_state.delta_x = std::clamp(m_swipe_state.delta_x,
        -static_cast<double>(m_swipe_state.screen_width),
        static_cast<double>(m_swipe_state.screen_width));

    int cur_x = static_cast<int>(std::round(m_swipe_state.delta_x));

    if (m_swipe_state.current_ws && m_swipe_state.current_ws->get_scene_tree()) {
        wlr_scene_node_set_position(&m_swipe_state.current_ws->get_scene_tree()->node, cur_x, 0);
    }

    if (m_swipe_state.target_ws && m_swipe_state.target_ws->get_scene_tree()) {
        int target_x = 0;
        if (m_swipe_state.direction == -1) {
            target_x = m_swipe_state.screen_width + cur_x;
        } else if (m_swipe_state.direction == 1) {
            target_x = -m_swipe_state.screen_width + cur_x;
        }
        wlr_scene_node_set_position(&m_swipe_state.target_ws->get_scene_tree()->node, target_x, 0);
    }

    schedule_next_frame();
}

void AnimationManager::end_workspace_swipe(bool cancelled) {
    if (!m_swipe_state.active || !m_swipe_state.current_ws) {
        m_swipe_state.active = false;
        return;
    }

    Workspace* current = m_swipe_state.current_ws;
    Workspace* target = m_swipe_state.target_ws;
    size_t target_id = m_swipe_state.target_ws_id;
    int direction = m_swipe_state.direction;
    double delta_x = m_swipe_state.delta_x;
    int screen_w = m_swipe_state.screen_width;

    m_swipe_state.active = false;
    m_swipe_state.current_ws = nullptr;
    m_swipe_state.target_ws = nullptr;
    m_swipe_state.target_ws_id = 0;
    m_swipe_state.direction = 0;
    m_swipe_state.delta_x = 0.0;

    int cur_start_x = static_cast<int>(std::round(delta_x));
    double ratio = (screen_w > 0) ? (std::abs(delta_x) / static_cast<double>(screen_w)) : 0.0;
    bool commit = !cancelled && (target != nullptr) && (ratio >= 0.30);

    auto easing = Easing::from_name(Config::get().get_animation_curve());
    uint64_t now = get_current_time_ms();
    int base_dur = Config::get().get_animation_duration_ms();

    if (commit && target) {
        int cur_target_x = (direction == -1) ? -screen_w : screen_w;
        int target_start_x = (direction == -1) ? (screen_w + cur_start_x) : (-screen_w + cur_start_x);
        int duration = std::max(50, static_cast<int>(base_dur * (1.0 - ratio)));

        // Animate old workspace to offscreen
        if (current->get_scene_tree()) {
            NodeAnimation anim_from;
            anim_from.id = m_next_id++;
            anim_from.node = &current->get_scene_tree()->node;
            anim_from.bound_workspace = current;
            anim_from.start_x = cur_start_x;
            anim_from.start_y = 0;
            anim_from.target_x = cur_target_x;
            anim_from.target_y = 0;
            anim_from.start_time_ms = now;
            anim_from.duration_ms = duration;
            anim_from.easing_fn = easing;
            anim_from.on_complete = [current]() {
                current->set_visible(false);
                if (current->get_scene_tree()) {
                    wlr_scene_node_set_position(&current->get_scene_tree()->node, 0, 0);
                }
            };
            m_animations.push_back(std::move(anim_from));
        }

        // Animate target workspace to 0,0 and commit switch
        Server* srv = m_server;
        if (target->get_scene_tree()) {
            NodeAnimation anim_to;
            anim_to.id = m_next_id++;
            anim_to.node = &target->get_scene_tree()->node;
            anim_to.bound_workspace = target;
            anim_to.start_x = target_start_x;
            anim_to.start_y = 0;
            anim_to.target_x = 0;
            anim_to.target_y = 0;
            anim_to.start_time_ms = now;
            anim_to.duration_ms = duration;
            anim_to.easing_fn = easing;
            anim_to.on_complete = [srv, target, target_id]() {
                if (target->get_scene_tree()) {
                    wlr_scene_node_set_position(&target->get_scene_tree()->node, 0, 0);
                }
                if (srv && srv->get_workspace_manager()) {
                    srv->get_workspace_manager()->commit_workspace_switch(target_id);
                }
            };
            m_animations.push_back(std::move(anim_to));
        } else {
            if (srv && srv->get_workspace_manager()) {
                srv->get_workspace_manager()->commit_workspace_switch(target_id);
            }
        }
    } else {
        // Snap back to current workspace
        int duration = std::max(50, static_cast<int>(base_dur * std::max(0.1, ratio)));
        Server* srv = m_server;

        if (current->get_scene_tree()) {
            NodeAnimation anim_cur;
            anim_cur.id = m_next_id++;
            anim_cur.node = &current->get_scene_tree()->node;
            anim_cur.bound_workspace = current;
            anim_cur.start_x = cur_start_x;
            anim_cur.start_y = 0;
            anim_cur.target_x = 0;
            anim_cur.target_y = 0;
            anim_cur.start_time_ms = now;
            anim_cur.duration_ms = duration;
            anim_cur.easing_fn = easing;
            anim_cur.on_complete = [srv, current]() {
                if (current->get_scene_tree()) {
                    wlr_scene_node_set_position(&current->get_scene_tree()->node, 0, 0);
                }
                if (srv && srv->get_input_manager()) {
                    srv->get_input_manager()->recheck_cursor_focus();
                }
            };
            m_animations.push_back(std::move(anim_cur));
        }

        if (target && target->get_scene_tree()) {
            int target_start_x = (direction == -1) ? (screen_w + cur_start_x) : (-screen_w + cur_start_x);
            int target_end_x = (direction == -1) ? screen_w : -screen_w;

            NodeAnimation anim_target;
            anim_target.id = m_next_id++;
            anim_target.node = &target->get_scene_tree()->node;
            anim_target.bound_workspace = target;
            anim_target.start_x = target_start_x;
            anim_target.start_y = 0;
            anim_target.target_x = target_end_x;
            anim_target.target_y = 0;
            anim_target.start_time_ms = now;
            anim_target.duration_ms = duration;
            anim_target.easing_fn = easing;
            anim_target.on_complete = [target]() {
                target->set_visible(false);
                if (target->get_scene_tree()) {
                    wlr_scene_node_set_position(&target->get_scene_tree()->node, 0, 0);
                }
            };
            m_animations.push_back(std::move(anim_target));
        }
    }

    schedule_next_frame();
}

void AnimationManager::cancel_for_view(View* view) {
    if (!view) return;
    auto it = std::remove_if(m_animations.begin(), m_animations.end(), [view](const NodeAnimation& a) {
        return a.bound_view == view;
    });
    m_animations.erase(it, m_animations.end());
}

void AnimationManager::cancel_for_workspace(Workspace* ws) {
    if (!ws) return;
    if (m_swipe_state.current_ws == ws || m_swipe_state.target_ws == ws) {
        if (m_swipe_state.current_ws && m_swipe_state.current_ws->get_scene_tree()) {
            wlr_scene_node_set_position(&m_swipe_state.current_ws->get_scene_tree()->node, 0, 0);
        }
        if (m_swipe_state.target_ws && m_swipe_state.target_ws != m_swipe_state.current_ws) {
            m_swipe_state.target_ws->set_visible(false);
            if (m_swipe_state.target_ws->get_scene_tree()) {
                wlr_scene_node_set_position(&m_swipe_state.target_ws->get_scene_tree()->node, 0, 0);
            }
        }
        m_swipe_state.active = false;
        m_swipe_state.current_ws = nullptr;
        m_swipe_state.target_ws = nullptr;
        m_swipe_state.target_ws_id = 0;
        m_swipe_state.direction = 0;
        m_swipe_state.delta_x = 0.0;
    }
    auto it = std::remove_if(m_animations.begin(), m_animations.end(), [ws](const NodeAnimation& a) {
        return a.bound_workspace == ws;
    });
    m_animations.erase(it, m_animations.end());
}

void AnimationManager::cancel_for_node(struct wlr_scene_node* node) {
    if (!node) return;
    auto it = std::remove_if(m_animations.begin(), m_animations.end(), [node](const NodeAnimation& a) {
        return a.node == node;
    });
    m_animations.erase(it, m_animations.end());
}

void AnimationManager::clear() {
    if (m_swipe_state.active) {
        if (m_swipe_state.current_ws && m_swipe_state.current_ws->get_scene_tree()) {
            wlr_scene_node_set_position(&m_swipe_state.current_ws->get_scene_tree()->node, 0, 0);
        }
        if (m_swipe_state.target_ws && m_swipe_state.target_ws != m_swipe_state.current_ws) {
            m_swipe_state.target_ws->set_visible(false);
            if (m_swipe_state.target_ws->get_scene_tree()) {
                wlr_scene_node_set_position(&m_swipe_state.target_ws->get_scene_tree()->node, 0, 0);
            }
        }
        m_swipe_state.active = false;
        m_swipe_state.current_ws = nullptr;
        m_swipe_state.target_ws = nullptr;
    }
    m_animations.clear();
}

void AnimationManager::tick(uint64_t now_ms) {
    if (m_animations.empty()) return;

    if (now_ms == 0) {
        now_ms = get_current_time_ms();
    }

    std::vector<std::function<void()>> completions;

    for (auto& anim : m_animations) {
        if (!anim.node) {
            anim.completed = true;
            continue;
        }

        float progress = 1.0f;
        if (anim.duration_ms > 0) {
            progress = static_cast<float>(now_ms - anim.start_time_ms) / static_cast<float>(anim.duration_ms);
        }

        if (progress >= 1.0f) {
            progress = 1.0f;
            anim.completed = true;
        }

        float eased = anim.easing_fn ? anim.easing_fn(progress) : progress;
        int cur_x = anim.start_x + static_cast<int>(std::round(static_cast<float>(anim.target_x - anim.start_x) * eased));
        int cur_y = anim.start_y + static_cast<int>(std::round(static_cast<float>(anim.target_y - anim.start_y) * eased));

        wlr_scene_node_set_position(anim.node, cur_x, cur_y);

        if (anim.completed && anim.on_complete) {
            completions.push_back(anim.on_complete);
        }
    }

    auto it = std::remove_if(m_animations.begin(), m_animations.end(), [](const NodeAnimation& a) {
        return a.completed;
    });
    m_animations.erase(it, m_animations.end());

    for (auto& cb : completions) {
        cb();
    }

    if (!m_animations.empty()) {
        schedule_next_frame();
    }
}

void AnimationManager::schedule_next_frame() {
    if (!m_server || !m_server->get_output_manager()) return;

    const auto& outputs = m_server->get_output_manager()->get_outputs();
    for (const auto& out : outputs) {
        if (out && out->get_wlr_output()) {
            wlr_output_schedule_frame(out->get_wlr_output());
        }
    }
}

} // namespace miquland
