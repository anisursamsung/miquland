#include "core/input/input.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/view.hpp"
#include "core/output.hpp"
#include "core/session_lock.hpp"
#include "core/config/config.hpp"
#include <unistd.h>
#include <cstdlib>
#include <cmath>
#include <linux/input-event-codes.h>

namespace miquland {

namespace {

const char* edge_to_cursor_name(uint32_t edges) {
    if ((edges & WLR_EDGE_TOP) && (edges & WLR_EDGE_LEFT)) return "nw-resize";
    if ((edges & WLR_EDGE_TOP) && (edges & WLR_EDGE_RIGHT)) return "ne-resize";
    if ((edges & WLR_EDGE_BOTTOM) && (edges & WLR_EDGE_LEFT)) return "sw-resize";
    if ((edges & WLR_EDGE_BOTTOM) && (edges & WLR_EDGE_RIGHT)) return "se-resize";
    if (edges & WLR_EDGE_TOP) return "n-resize";
    if (edges & WLR_EDGE_BOTTOM) return "s-resize";
    if (edges & WLR_EDGE_LEFT) return "w-resize";
    if (edges & WLR_EDGE_RIGHT) return "e-resize";
    return "se-resize";
}

uint32_t detect_border_edges(const View* view, double sx, double sy, int grab_area) {
    if (!view) return 0;
    uint32_t edges = 0;
    if (sx < grab_area) edges |= WLR_EDGE_LEFT;
    else if (sx >= view->get_width() - grab_area) edges |= WLR_EDGE_RIGHT;
    if (sy < grab_area) edges |= WLR_EDGE_TOP;
    else if (sy >= view->get_height() - grab_area) edges |= WLR_EDGE_BOTTOM;
    return edges;
}

struct wlr_box calculate_interactive_move(const struct wlr_box& initial, double dx, double dy) {
    return {
        .x = initial.x + static_cast<int>(std::round(dx)),
        .y = initial.y + static_cast<int>(std::round(dy)),
        .width = initial.width,
        .height = initial.height,
    };
}

struct wlr_box calculate_interactive_resize(const struct wlr_box& initial, uint32_t edges, double dx, double dy, int min_w = 100, int min_h = 80) {
    struct wlr_box box = initial;

    if (edges & WLR_EDGE_RIGHT) {
        box.width = std::max(min_w, initial.width + static_cast<int>(std::round(dx)));
    } else if (edges & WLR_EDGE_LEFT) {
        int w = initial.width - static_cast<int>(std::round(dx));
        if (w >= min_w) {
            box.width = w;
            box.x = initial.x + static_cast<int>(std::round(dx));
        } else {
            box.width = min_w;
            box.x = initial.x + initial.width - min_w;
        }
    }

    if (edges & WLR_EDGE_BOTTOM) {
        box.height = std::max(min_h, initial.height + static_cast<int>(std::round(dy)));
    } else if (edges & WLR_EDGE_TOP) {
        int h = initial.height - static_cast<int>(std::round(dy));
        if (h >= min_h) {
            box.height = h;
            box.y = initial.y + static_cast<int>(std::round(dy));
        } else {
            box.height = min_h;
            box.y = initial.y + initial.height - min_h;
        }
    }

    return box;
}

} // namespace

InputManager::InputManager(Server* server)
    : m_server(server)
{
    m_seat = wlr_seat_create(server->get_display(), "seat0");

    m_cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(m_cursor, server->get_output_manager()->get_layout());

    m_cursor_mgr = wlr_xcursor_manager_create(nullptr, 24);

    m_new_input_listener.notify = handle_new_input;
    wl_signal_add(&server->get_backend()->events.new_input, &m_new_input_listener);

    m_cursor_motion_listener.notify = handle_cursor_motion;
    wl_signal_add(&m_cursor->events.motion, &m_cursor_motion_listener);

    m_cursor_motion_absolute_listener.notify = handle_cursor_motion_absolute;
    wl_signal_add(&m_cursor->events.motion_absolute, &m_cursor_motion_absolute_listener);

    m_cursor_button_listener.notify = handle_cursor_button;
    wl_signal_add(&m_cursor->events.button, &m_cursor_button_listener);

    m_cursor_axis_listener.notify = handle_cursor_axis;
    wl_signal_add(&m_cursor->events.axis, &m_cursor_axis_listener);

    m_cursor_frame_listener.notify = handle_cursor_frame;
    wl_signal_add(&m_cursor->events.frame, &m_cursor_frame_listener);

    m_request_set_cursor_listener.notify = handle_request_set_cursor;
    wl_signal_add(&m_seat->events.request_set_cursor, &m_request_set_cursor_listener);
    
    // Clipboard Selection wiring
    m_request_set_selection_listener.notify = handle_request_set_selection;
    wl_signal_add(&m_seat->events.request_set_selection, &m_request_set_selection_listener);

    

    m_selection_destroy_listener.notify = handle_selection_destroy;
    wl_list_init(&m_selection_destroy_listener.link);

    // Touchpad swipe & pinch gesture listeners
    m_cursor_swipe_begin_listener.notify = handle_cursor_swipe_begin;
    wl_signal_add(&m_cursor->events.swipe_begin, &m_cursor_swipe_begin_listener);

    m_cursor_swipe_update_listener.notify = handle_cursor_swipe_update;
    wl_signal_add(&m_cursor->events.swipe_update, &m_cursor_swipe_update_listener);

    m_cursor_swipe_end_listener.notify = handle_cursor_swipe_end;
    wl_signal_add(&m_cursor->events.swipe_end, &m_cursor_swipe_end_listener);

    m_cursor_pinch_begin_listener.notify = handle_cursor_pinch_begin;
    wl_signal_add(&m_cursor->events.pinch_begin, &m_cursor_pinch_begin_listener);

    m_cursor_pinch_update_listener.notify = handle_cursor_pinch_update;
    wl_signal_add(&m_cursor->events.pinch_update, &m_cursor_pinch_update_listener);

    m_cursor_pinch_end_listener.notify = handle_cursor_pinch_end;
    wl_signal_add(&m_cursor->events.pinch_end, &m_cursor_pinch_end_listener);

    m_cursor_hold_begin_listener.notify = handle_cursor_hold_begin;
    wl_signal_add(&m_cursor->events.hold_begin, &m_cursor_hold_begin_listener);

    m_cursor_hold_end_listener.notify = handle_cursor_hold_end;
    wl_signal_add(&m_cursor->events.hold_end, &m_cursor_hold_end_listener);

    // Touchscreen listeners
    m_cursor_touch_down_listener.notify = handle_cursor_touch_down;
    wl_signal_add(&m_cursor->events.touch_down, &m_cursor_touch_down_listener);

    m_cursor_touch_up_listener.notify = handle_cursor_touch_up;
    wl_signal_add(&m_cursor->events.touch_up, &m_cursor_touch_up_listener);

    m_cursor_touch_motion_listener.notify = handle_cursor_touch_motion;
    wl_signal_add(&m_cursor->events.touch_motion, &m_cursor_touch_motion_listener);

    m_cursor_touch_cancel_listener.notify = handle_cursor_touch_cancel;
    wl_signal_add(&m_cursor->events.touch_cancel, &m_cursor_touch_cancel_listener);

    m_cursor_touch_frame_listener.notify = handle_cursor_touch_frame;
    wl_signal_add(&m_cursor->events.touch_frame, &m_cursor_touch_frame_listener);
}

InputManager::~InputManager() {
    wl_list_remove(&m_new_input_listener.link);
    wl_list_remove(&m_cursor_motion_listener.link);
    wl_list_remove(&m_cursor_motion_absolute_listener.link);
    wl_list_remove(&m_cursor_button_listener.link);
    wl_list_remove(&m_cursor_axis_listener.link);
    wl_list_remove(&m_cursor_frame_listener.link);
    wl_list_remove(&m_request_set_cursor_listener.link);
    
    // Cleanup Clipboard listeners
    wl_list_remove(&m_request_set_selection_listener.link);
    wl_list_remove(&m_selection_destroy_listener.link);

    wl_list_remove(&m_cursor_swipe_begin_listener.link);
    wl_list_remove(&m_cursor_swipe_update_listener.link);
    wl_list_remove(&m_cursor_swipe_end_listener.link);
    wl_list_remove(&m_cursor_pinch_begin_listener.link);
    wl_list_remove(&m_cursor_pinch_update_listener.link);
    wl_list_remove(&m_cursor_pinch_end_listener.link);
    wl_list_remove(&m_cursor_hold_begin_listener.link);
    wl_list_remove(&m_cursor_hold_end_listener.link);

    wl_list_remove(&m_cursor_touch_down_listener.link);
    wl_list_remove(&m_cursor_touch_up_listener.link);
    wl_list_remove(&m_cursor_touch_motion_listener.link);
    wl_list_remove(&m_cursor_touch_cancel_listener.link);
    wl_list_remove(&m_cursor_touch_frame_listener.link);

    if (m_cursor_mgr) wlr_xcursor_manager_destroy(m_cursor_mgr);
    if (m_cursor) wlr_cursor_destroy(m_cursor);
}

void InputManager::spawn_command(const char* cmd) {
    if (!cmd || !*cmd) return;
    log_info("Executing command: " + std::string(cmd));

    if (fork() == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(1);
    }
}

void InputManager::remove_keyboard(Keyboard* kb) {
    for (auto it = m_keyboards.begin(); it != m_keyboards.end(); ++it) {
        if (it->get() == kb) {
            m_keyboards.erase(it);
            break;
        }
    }
}

void InputManager::reapply_device_config() {
    bool natural_scroll = Config::get().is_natural_scroll_enabled();
    for (auto* device : m_pointers) {
        if (wlr_input_device_is_libinput(device)) {
            struct libinput_device* libinput_dev = wlr_libinput_get_device_handle(device);
            if (libinput_dev) {
                if (libinput_device_config_scroll_has_natural_scroll(libinput_dev)) {
                    libinput_device_config_scroll_set_natural_scroll_enabled(libinput_dev, natural_scroll ? 1 : 0);
                    log_info(std::string("Live updated natural scroll: ") + (natural_scroll ? "true" : "false"));
                }
            }
        }
    }
}

void InputManager::set_cursor_icon(const char* name) {
    if (m_cursor && m_cursor_mgr) {
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, name ? name : "default");
    }
}

void InputManager::begin_interactive(View* view, CursorMode mode, uint32_t edges) {
    if (!view || view->is_fullscreen() || view->is_override_redirect()) return;

    m_grabbed_view = view;
    m_grabbed_view_was_tiled = !view->is_floating();
    m_cursor_mode = mode;
    m_grab_x = m_cursor->x;
    m_grab_y = m_cursor->y;
    m_grab_initial_view_box = {
        .x = view->get_x(),
        .y = view->get_y(),
        .width = view->get_width(),
        .height = view->get_height()
    };
    m_resize_edges = edges;

    if (m_grabbed_view_was_tiled && view->get_workspace()) {
        Workspace* ws = view->get_workspace();
        m_grab_initial_split_ratio = ws->get_split_ratio();
        m_grab_initial_secondary_ratio = ws->get_secondary_split_ratio();

        bool primary_is_horizontal = (ws->get_split_mode() == SplitMode::Horizontal);
        if (Config::get().get_layout_mode() == Config::LayoutMode::Stack) {
            primary_is_horizontal = true;
        }

        if (primary_is_horizontal) {
            m_grab_resize_is_secondary = ((edges & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)) != 0);
        } else {
            m_grab_resize_is_secondary = ((edges & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) != 0);
        }
    }

    view->focus();
    if (view->get_scene_tree()) {
        wlr_scene_node_raise_to_top(&view->get_scene_tree()->node);
    }

    if (mode == CursorMode::Move) {
        set_cursor_icon("grab");
    } else if (mode == CursorMode::Resize) {
        set_cursor_icon(edge_to_cursor_name(edges));
    }
}

void InputManager::end_interactive() {
    if (m_cursor_mode == CursorMode::Move && m_grabbed_view && m_grabbed_view_was_tiled) {
        View* target = nullptr;
        if (m_grabbed_view->get_scene_tree()) {
            wlr_scene_node_set_enabled(&m_grabbed_view->get_scene_tree()->node, false);
            double sx = 0, sy = 0;
            struct wlr_surface* surf = nullptr;
            target = m_server->view_at(m_cursor->x, m_cursor->y, &surf, &sx, &sy);
            wlr_scene_node_set_enabled(&m_grabbed_view->get_scene_tree()->node, true);
        }

        Workspace* ws = m_grabbed_view->get_workspace();
        double dist = std::hypot(m_cursor->x - m_grab_x, m_cursor->y - m_grab_y);

        if (target && target != m_grabbed_view && ws && !target->is_floating() && target->get_workspace() == ws) {
            // Drop onto another tiled window: Swap positions in layout!
            ws->swap_views(m_grabbed_view, target);
        } else if (target == m_grabbed_view || dist < 40.0) {
            // Dropped back in place or slight twitch: snap back to tiled position
            if (ws) {
                auto* out_mgr = m_server->get_output_manager();
                if (out_mgr) {
                    ws->recalculate_layout(out_mgr->get_primary_usable_geometry());
                }
            }
        } else {
            // Dragged far into empty space: pop out to floating
            if (ws) {
                ws->toggle_floating(m_grabbed_view);
            }
        }
    }

    m_cursor_mode = CursorMode::Passthrough;
    m_grabbed_view = nullptr;
    m_grabbed_view_was_tiled = false;
    set_cursor_icon("default");
}

void InputManager::notify_view_destroyed(View* view) {
    if (m_grabbed_view == view) {
        m_grabbed_view = nullptr;
        m_grabbed_view_was_tiled = false;
        m_cursor_mode = CursorMode::Passthrough;
        set_cursor_icon("default");
    }
    if (m_border_hover_view == view) {
        m_border_hover_view = nullptr;
        m_border_hover_edges = 0;
    }
}

bool InputManager::handle_keybinding(uint32_t modifiers, xkb_keysym_t keysym) {
    // 0. If session is locked, NEVER process any shortcuts (all keys go directly to locker)
    if (m_server->is_locked()) {
        return false;
    }

    xkb_keysym_t norm_sym = (keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z) ? (keysym - XKB_KEY_A + XKB_KEY_a) : keysym;

    bool mod = (modifiers & WLR_MODIFIER_LOGO) != 0;
    bool shift = (modifiers & WLR_MODIFIER_SHIFT) != 0;
    bool ctrl = (modifiers & WLR_MODIFIER_CTRL) != 0;
    bool alt = (modifiers & WLR_MODIFIER_ALT) != 0;

    // 1. If an exclusive Layer Surface is focused (e.g. rofi, miqulauncher, swaylock), forward keys directly to it
    if (m_server->get_focused_layer_surface()) {
        if (mod && shift && norm_sym == XKB_KEY_q) {
            m_server->terminate();
            return true;
        }
        return false;
    }

    uint32_t active_mods = 0;
    if (mod) active_mods |= WLR_MODIFIER_LOGO;
    if (shift) active_mods |= WLR_MODIFIER_SHIFT;
    if (ctrl) active_mods |= WLR_MODIFIER_CTRL;
    if (alt) active_mods |= WLR_MODIFIER_ALT;

    // 2. Reserved System Bindings
    // Safe Exit compositor: Super + Shift + Q
    if (mod && shift && norm_sym == XKB_KEY_q) {
        log_info("System exit keybinding triggered (Super+Shift+Q)");
        m_server->terminate();
        return true;
    }

    // Terminal: Super + T
    if (mod && !shift && !ctrl && !alt && norm_sym == XKB_KEY_t) {
        std::string term = Config::get().get_terminal();
        if (term.empty()) term = "kitty || foot || alacritty || wezterm || weston-terminal || xterm";
        log_info("Terminal keybinding triggered: " + term);
        spawn_command(term.c_str());
        return true;
    }

    // 3. User & Default Configurable Bindings
    for (const auto& kb : Config::get().get_keybindings()) {
        xkb_keysym_t kb_norm = (kb.keysym >= XKB_KEY_A && kb.keysym <= XKB_KEY_Z) ? (kb.keysym - XKB_KEY_A + XKB_KEY_a) : kb.keysym;

        if (kb.modifiers == active_mods && kb_norm == norm_sym) {
            const std::string& action = kb.action;
            log_info("Keybinding matched: " + kb.combo_str + " -> " + action);

            if (action == "menu" || action == "app_launcher") {
                spawn_command("miqulauncher");
            } else if (action == "close" || action == "close_window") {
                View* focused = m_server->get_focused_view();
                if (focused) focused->close();
            } else if (action == "toggle_fullscreen" || action == "fullscreen") {
                View* focused = m_server->get_focused_view();
                if (focused) {
                    focused->set_fullscreen(!focused->is_fullscreen());
                }
            } else if (action == "toggle_floating" || action == "toggle_float" || action == "floating_toggle") {
                m_server->get_workspace_manager()->toggle_floating_active();
            } else if (action == "toggle_layout" || action == "layout_toggle") {
                m_server->get_workspace_manager()->toggle_layout_mode();
            } else if (action == "swap_main" || action == "swap_master" || action == "swap_with_main") {
                m_server->get_workspace_manager()->swap_with_main();
            } else if (action == "toggle_split" || action == "split_toggle") {
                m_server->get_workspace_manager()->toggle_active_split();
            } else if (action == "focus_win_1" || action == "window_1") {
                m_server->get_workspace_manager()->focus_window_index(0);
            } else if (action == "focus_win_2" || action == "window_2") {
                m_server->get_workspace_manager()->focus_window_index(1);
            } else if (action == "toggle_focus" || action == "next_window" || action == "focus_next") {
                m_server->get_workspace_manager()->focus_next_view();
            } else if (action == "prev_window" || action == "focus_prev") {
                m_server->get_workspace_manager()->focus_prev_view();
            } else if (action == "prev_ws" || action == "prev_workspace") {
                m_server->get_workspace_manager()->prev_workspace();
            } else if (action == "next_ws" || action == "next_workspace") {
                m_server->get_workspace_manager()->next_workspace();
            } else if (action.rfind("ws_", 0) == 0) {
                try {
                    size_t ws_id = std::stoul(action.substr(3));
                    m_server->get_workspace_manager()->switch_to_workspace(ws_id);
                } catch (...) {}
            } else if (action.rfind("move_ws_", 0) == 0) {
                try {
                    size_t ws_id = std::stoul(action.substr(8));
                    View* focused = m_server->get_focused_view();
                    if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
                } catch (...) {}
            } else if (action.rfind("move_to_ws_", 0) == 0) {
                try {
                    size_t ws_id = std::stoul(action.substr(11));
                    View* focused = m_server->get_focused_view();
                    if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
                } catch (...) {}
            } else if (action.rfind("movetoworkspace_", 0) == 0) {
                try {
                    size_t ws_id = std::stoul(action.substr(16));
                    View* focused = m_server->get_focused_view();
                    if (focused) m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
                } catch (...) {}
            } else if (action == "exit" || action == "quit") {
                m_server->terminate();
            } else {
                spawn_command(action.c_str());
            }
            return true;
        }
    }
    return false;
}

void InputManager::process_cursor_motion(uint32_t time) {
    if (m_server->is_locked()) {
        double sx = 0, sy = 0;
        SessionLockSurface* lock_surf = m_server->get_session_lock()->surface_at(m_cursor->x, m_cursor->y, &sx, &sy);
        if (lock_surf && lock_surf->get_wlr_surface()) {
            struct wlr_surface* surface = lock_surf->get_wlr_surface();
            if (m_seat->pointer_state.focused_surface != surface) {
                wlr_seat_pointer_notify_enter(m_seat, surface, sx, sy);
            }
            wlr_seat_pointer_notify_motion(m_seat, time, sx, sy);
            lock_surf->focus();
        } else {
            wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
            wlr_seat_pointer_clear_focus(m_seat);
        }
        return;
    }

    if (m_server->get_idle_notifier()) {
        wlr_idle_notifier_v1_notify_activity(m_server->get_idle_notifier(), m_seat);
    }

    // Handle interactive Move & Resize
    if (m_cursor_mode == CursorMode::Move) {
        if (m_grabbed_view) {
            struct wlr_box box = calculate_interactive_move(m_grab_initial_view_box, m_cursor->x - m_grab_x, m_cursor->y - m_grab_y);
            m_grabbed_view->set_geometry(box.x, box.y, box.width, box.height);
        }
        return;
    } else if (m_cursor_mode == CursorMode::Resize) {
        if (m_grabbed_view) {
            if (m_grabbed_view_was_tiled && m_grabbed_view->get_workspace()) {
                Workspace* ws = m_grabbed_view->get_workspace();
                auto* out_mgr = m_server->get_output_manager();
                if (out_mgr && ws->get_tiled_views().size() >= 2) {
                    struct wlr_box usable = out_mgr->get_primary_usable_geometry();
                    int pad = Config::get().get_screen_edge_padding();
                    int usable_w = std::max(50, usable.width - 2 * pad);
                    int usable_h = std::max(50, usable.height - 2 * pad);

                    bool primary_is_horizontal = (ws->get_split_mode() == SplitMode::Horizontal);
                    if (Config::get().get_layout_mode() == Config::LayoutMode::Stack) {
                        primary_is_horizontal = true;
                    }

                    double dx = m_cursor->x - m_grab_x;
                    double dy = m_cursor->y - m_grab_y;

                    if (m_grab_resize_is_secondary && ws->get_tiled_views().size() >= 3) {
                        if (primary_is_horizontal) {
                            double delta = dy / static_cast<double>(usable_h);
                            bool is_top_of_stack = (ws->get_tiled_views().size() > 1 && ws->get_tiled_views()[1] == m_grabbed_view);
                            double new_ratio = m_grab_initial_secondary_ratio;
                            if (is_top_of_stack) {
                                new_ratio = (m_resize_edges & WLR_EDGE_BOTTOM)
                                    ? (m_grab_initial_secondary_ratio + delta)
                                    : (m_grab_initial_secondary_ratio - delta);
                            } else {
                                new_ratio = (m_resize_edges & WLR_EDGE_TOP)
                                    ? (m_grab_initial_secondary_ratio + delta)
                                    : (m_grab_initial_secondary_ratio - delta);
                            }
                            ws->set_secondary_split_ratio(new_ratio);
                        } else {
                            double delta = dx / static_cast<double>(usable_w);
                            bool is_left_of_stack = (ws->get_tiled_views().size() > 1 && ws->get_tiled_views()[1] == m_grabbed_view);
                            double new_ratio = m_grab_initial_secondary_ratio;
                            if (is_left_of_stack) {
                                new_ratio = (m_resize_edges & WLR_EDGE_RIGHT)
                                    ? (m_grab_initial_secondary_ratio + delta)
                                    : (m_grab_initial_secondary_ratio - delta);
                            } else {
                                new_ratio = (m_resize_edges & WLR_EDGE_LEFT)
                                    ? (m_grab_initial_secondary_ratio + delta)
                                    : (m_grab_initial_secondary_ratio - delta);
                            }
                            ws->set_secondary_split_ratio(new_ratio);
                        }
                    } else {
                        bool is_first_view = (!ws->get_tiled_views().empty() && ws->get_tiled_views().front() == m_grabbed_view);
                        double new_ratio = m_grab_initial_split_ratio;
                        if (primary_is_horizontal) {
                            double delta = dx / static_cast<double>(usable_w);
                            if (is_first_view) {
                                new_ratio = m_grab_initial_split_ratio + delta;
                            } else {
                                if (m_resize_edges & WLR_EDGE_LEFT) {
                                    new_ratio = m_grab_initial_split_ratio + delta;
                                } else {
                                    new_ratio = m_grab_initial_split_ratio - delta;
                                }
                            }
                        } else {
                            double delta = dy / static_cast<double>(usable_h);
                            if (is_first_view) {
                                new_ratio = m_grab_initial_split_ratio + delta;
                            } else {
                                if (m_resize_edges & WLR_EDGE_TOP) {
                                    new_ratio = m_grab_initial_split_ratio + delta;
                                } else {
                                    new_ratio = m_grab_initial_split_ratio - delta;
                                }
                            }
                        }
                        ws->set_split_ratio(new_ratio);
                    }

                    ws->recalculate_layout(usable);
                }
            } else {
                struct wlr_box box = calculate_interactive_resize(m_grab_initial_view_box, m_resize_edges, m_cursor->x - m_grab_x, m_cursor->y - m_grab_y);
                m_grabbed_view->set_geometry(box.x, box.y, box.width, box.height);
            }
        }
        return;
    }

    double sx, sy;
    struct wlr_surface* surface = nullptr;
    View* view = m_server->view_at(m_cursor->x, m_cursor->y, &surface, &sx, &sy);

    m_border_hover_view = nullptr;
    m_border_hover_edges = 0;

    if (!surface) {
        if (view && Config::get().is_resize_on_border_enabled() && !view->is_fullscreen() && !view->is_override_redirect()) {
            int grab = std::max(Config::get().get_window_border_width(), Config::get().get_border_grab_area());
            uint32_t edges = detect_border_edges(view, sx, sy, grab);

            if (edges != 0) {
                m_border_hover_view = view;
                m_border_hover_edges = edges;
                set_cursor_icon(edge_to_cursor_name(edges));
                wlr_seat_pointer_clear_focus(m_seat);
                return;
            }
        }

        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
        wlr_seat_pointer_clear_focus(m_seat);
        return;
    }

    if (m_seat->pointer_state.focused_surface != surface) {
        wlr_seat_pointer_notify_enter(m_seat, surface, sx, sy);
    }
    wlr_seat_pointer_notify_motion(m_seat, time, sx, sy);

    View* target_view = view;
    if (target_view && target_view->has_child_dialogs()) {
        View* top_dialog = target_view->get_top_dialog();
        if (top_dialog) {
            target_view = top_dialog;
        }
    }

    // Hover to focus: if cursor hovers over a view, focus it!
    if (target_view && !target_view->is_override_redirect() && target_view != m_server->get_focused_view()) {
        target_view->focus();
    }
}

void InputManager::handle_new_input(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_new_input_listener);
    auto* device = static_cast<struct wlr_input_device*>(data);

    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        manager->m_keyboards.emplace_back(std::make_unique<Keyboard>(manager->m_server, device));
    } else if (device->type == WLR_INPUT_DEVICE_POINTER) {
        if (wlr_input_device_is_libinput(device)) {
            struct libinput_device* libinput_dev = wlr_libinput_get_device_handle(device);
            if (libinput_dev) {
                // Hardcode Tap-To-Click and Tap Drag enabled by default
                if (libinput_device_config_tap_get_finger_count(libinput_dev) > 0) {
                    libinput_device_config_tap_set_enabled(libinput_dev, LIBINPUT_CONFIG_TAP_ENABLED);
                    libinput_device_config_tap_set_drag_enabled(libinput_dev, LIBINPUT_CONFIG_DRAG_ENABLED);
                }

                // Configure Natural Scroll if supported
                if (libinput_device_config_scroll_has_natural_scroll(libinput_dev)) {
                    bool natural_scroll = Config::get().is_natural_scroll_enabled();
                    libinput_device_config_scroll_set_natural_scroll_enabled(libinput_dev, natural_scroll ? 1 : 0);
                }
            }
        }

        wlr_cursor_attach_input_device(manager->m_cursor, device);
        manager->m_pointers.push_back(device);
    } else if (device->type == WLR_INPUT_DEVICE_TOUCH) {
        wlr_cursor_attach_input_device(manager->m_cursor, device);
        manager->m_touch_devices.push_back(device);
        log_info("Touchscreen device attached: " + std::string(device->name ? device->name : "unnamed"));
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!manager->m_keyboards.empty()) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    if (!manager->m_touch_devices.empty()) {
        caps |= WL_SEAT_CAPABILITY_TOUCH;
    }
    wlr_seat_set_capabilities(manager->m_seat, caps);
}

void InputManager::handle_cursor_motion(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_motion_listener);
    auto* event = static_cast<struct wlr_pointer_motion_event*>(data);

    wlr_cursor_move(manager->m_cursor, &event->pointer->base, event->delta_x, event->delta_y);
    manager->process_cursor_motion(event->time_msec);

    if (manager->m_server->get_relative_pointer_manager()) {
        wlr_relative_pointer_manager_v1_send_relative_motion(
            manager->m_server->get_relative_pointer_manager(),
            manager->m_seat,
            (uint64_t)event->time_msec * 1000,
            event->delta_x, event->delta_y,
            event->unaccel_dx, event->unaccel_dy
        );
    }
}

void InputManager::handle_cursor_motion_absolute(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_motion_absolute_listener);
    auto* event = static_cast<struct wlr_pointer_motion_absolute_event*>(data);

    wlr_cursor_warp_absolute(manager->m_cursor, &event->pointer->base, event->x, event->y);
    manager->process_cursor_motion(event->time_msec);
}

void InputManager::handle_cursor_button(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_button_listener);
    auto* event = static_cast<struct wlr_pointer_button_event*>(data);

    if (manager->m_server->get_idle_notifier()) {
        wlr_idle_notifier_v1_notify_activity(manager->m_server->get_idle_notifier(), manager->m_seat);
    }

    if (manager->m_server->is_locked()) {
        double sx = 0, sy = 0;
        SessionLockSurface* lock_surf = manager->m_server->get_session_lock()->surface_at(manager->m_cursor->x, manager->m_cursor->y, &sx, &sy);
        if (lock_surf) {
            lock_surf->focus();
        }
        wlr_seat_pointer_notify_button(manager->m_seat, event->time_msec, event->button, event->state);
        return;
    }

    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        if (manager->m_cursor_mode != CursorMode::Passthrough) {
            manager->end_interactive();
            return;
        }
    }

    double sx, sy;
    struct wlr_surface* surface = nullptr;
    View* view = manager->m_server->view_at(manager->m_cursor->x, manager->m_cursor->y, &surface, &sx, &sy);

    View* target_view = view;
    if (target_view && target_view->has_child_dialogs()) {
        View* top_dialog = target_view->get_top_dialog();
        if (top_dialog) {
            target_view = top_dialog;
        }
    }

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        // Direct border drag-resize without holding Super
        if (event->button == BTN_LEFT && manager->m_border_hover_view && manager->m_border_hover_edges != 0) {
            manager->begin_interactive(manager->m_border_hover_view, CursorMode::Resize, manager->m_border_hover_edges);
            return;
        }

        struct wlr_keyboard* kb = wlr_seat_get_keyboard(manager->m_seat);
        uint32_t modifiers = kb ? wlr_keyboard_get_modifiers(kb) : 0;
        bool mod_pressed = (modifiers & WLR_MODIFIER_LOGO) != 0;

        if (mod_pressed && target_view && !target_view->is_fullscreen() && !target_view->is_override_redirect()) {
            if (event->button == BTN_LEFT) {
                manager->begin_interactive(target_view, CursorMode::Move, 0);
                return;
            } else if (event->button == BTN_RIGHT) {
                uint32_t edges = 0;
                double center_x = target_view->get_x() + target_view->get_width() / 2.0;
                double center_y = target_view->get_y() + target_view->get_height() / 2.0;
                if (manager->m_cursor->x < center_x) edges |= WLR_EDGE_LEFT;
                else edges |= WLR_EDGE_RIGHT;
                if (manager->m_cursor->y < center_y) edges |= WLR_EDGE_TOP;
                else edges |= WLR_EDGE_BOTTOM;

                manager->begin_interactive(target_view, CursorMode::Resize, edges);
                return;
            }
        }

        if (target_view && !target_view->is_override_redirect()) {
            target_view->focus();
        }
    }

    wlr_seat_pointer_notify_button(manager->m_seat, event->time_msec, event->button, event->state);
}

void InputManager::handle_cursor_axis(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_axis_listener);
    auto* event = static_cast<struct wlr_pointer_axis_event*>(data);

    wlr_seat_pointer_notify_axis(manager->m_seat, event->time_msec, event->orientation,
        event->delta, event->delta_discrete, event->source, event->relative_direction);
}

void InputManager::handle_cursor_frame(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_frame_listener);
    wlr_seat_pointer_notify_frame(manager->m_seat);
}

void InputManager::handle_request_set_cursor(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_request_set_cursor_listener);
    auto* event = static_cast<struct wlr_seat_pointer_request_set_cursor_event*>(data);

    struct wlr_seat_client* focused_client = manager->m_seat->pointer_state.focused_client;
    if (focused_client == event->seat_client) {
        wlr_cursor_set_surface(manager->m_cursor, event->surface, event->hotspot_x, event->hotspot_y);
    }
}

// === Clipboard Event Handlers ===

void InputManager::handle_request_set_selection(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_request_set_selection_listener);
    auto* event = static_cast<struct wlr_seat_request_set_selection_event*>(data);
    wlr_seat_set_selection(manager->m_seat, event->source, event->serial);
}



void InputManager::handle_selection_destroy(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_selection_destroy_listener);
    wl_list_remove(&manager->m_selection_destroy_listener.link);
    wl_list_init(&manager->m_selection_destroy_listener.link);
}

// ================================

void InputManager::handle_cursor_swipe_begin(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_swipe_begin_listener);
    manager->m_swipe_dx = 0.0;
    manager->m_swipe_triggered = false;
}

void InputManager::handle_cursor_swipe_update(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_swipe_update_listener);
    auto* event = static_cast<struct wlr_pointer_swipe_update_event*>(data);

    if (event->fingers == 3 && !manager->m_swipe_triggered) {
        manager->m_swipe_dx += event->dx;
        const double threshold = 50.0;

        if (manager->m_swipe_dx > threshold) {
            // Swiped Right -> Switch to previous workspace
            manager->m_server->get_workspace_manager()->prev_workspace();
            manager->m_swipe_triggered = true;
        } else if (manager->m_swipe_dx < -threshold) {
            // Swiped Left -> Switch to next workspace
            manager->m_server->get_workspace_manager()->next_workspace();
            manager->m_swipe_triggered = true;
        }

    }
}

void InputManager::handle_cursor_swipe_end(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_swipe_end_listener);
    manager->m_swipe_dx = 0.0;
    manager->m_swipe_triggered = false;
}

void InputManager::handle_cursor_pinch_begin(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_pinch_begin_listener);
    auto* event = static_cast<struct wlr_pointer_pinch_begin_event*>(data);
    if (manager->m_server->get_pointer_gestures()) {
        wlr_pointer_gestures_v1_send_pinch_begin(manager->m_server->get_pointer_gestures(),
            manager->m_seat, event->time_msec, event->fingers);
    }
}

void InputManager::handle_cursor_pinch_update(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_pinch_update_listener);
    auto* event = static_cast<struct wlr_pointer_pinch_update_event*>(data);
    if (manager->m_server->get_pointer_gestures()) {
        wlr_pointer_gestures_v1_send_pinch_update(manager->m_server->get_pointer_gestures(),
            manager->m_seat, event->time_msec, event->dx, event->dy, event->scale, event->rotation);
    }
}

void InputManager::handle_cursor_pinch_end(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_pinch_end_listener);
    auto* event = static_cast<struct wlr_pointer_pinch_end_event*>(data);
    if (manager->m_server->get_pointer_gestures()) {
        wlr_pointer_gestures_v1_send_pinch_end(manager->m_server->get_pointer_gestures(),
            manager->m_seat, event->time_msec, event->cancelled);
    }
}

void InputManager::handle_cursor_hold_begin(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_hold_begin_listener);
    auto* event = static_cast<struct wlr_pointer_hold_begin_event*>(data);
    if (manager->m_server->get_pointer_gestures()) {
        wlr_pointer_gestures_v1_send_hold_begin(manager->m_server->get_pointer_gestures(),
            manager->m_seat, event->time_msec, event->fingers);
    }
}

void InputManager::handle_cursor_hold_end(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_hold_end_listener);
    auto* event = static_cast<struct wlr_pointer_hold_end_event*>(data);
    if (manager->m_server->get_pointer_gestures()) {
        wlr_pointer_gestures_v1_send_hold_end(manager->m_server->get_pointer_gestures(),
            manager->m_seat, event->time_msec, event->cancelled);
    }
}

void InputManager::handle_cursor_touch_down(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_touch_down_listener);
    auto* event = static_cast<struct wlr_touch_down_event*>(data);

    if (manager->m_server->get_idle_notifier()) {
        wlr_idle_notifier_v1_notify_activity(manager->m_server->get_idle_notifier(), manager->m_seat);
    }

    double lx = 0.0, ly = 0.0;
    wlr_cursor_absolute_to_layout_coords(manager->m_cursor, &event->touch->base, event->x, event->y, &lx, &ly);

    if (manager->m_server->is_locked()) {
        double sx = 0, sy = 0;
        SessionLockSurface* lock_surf = manager->m_server->get_session_lock()->surface_at(lx, ly, &sx, &sy);
        if (lock_surf && lock_surf->get_wlr_surface()) {
            lock_surf->focus();
            wlr_seat_touch_notify_down(manager->m_seat, lock_surf->get_wlr_surface(),
                event->time_msec, event->touch_id, sx, sy);
        }
        return;
    }

    double sx = 0.0, sy = 0.0;
    struct wlr_surface* surface = nullptr;
    View* view = manager->m_server->view_at(lx, ly, &surface, &sx, &sy);

    View* target_view = view;
    if (target_view && target_view->has_child_dialogs()) {
        View* top_dialog = target_view->get_top_dialog();
        if (top_dialog) target_view = top_dialog;
    }

    if (target_view && !target_view->is_override_redirect() && target_view != manager->m_server->get_focused_view()) {
        target_view->focus();
    }

    if (surface) {
        wlr_seat_touch_notify_down(manager->m_seat, surface, event->time_msec, event->touch_id, sx, sy);
    }
}

void InputManager::handle_cursor_touch_up(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_touch_up_listener);
    auto* event = static_cast<struct wlr_touch_up_event*>(data);
    wlr_seat_touch_notify_up(manager->m_seat, event->time_msec, event->touch_id);
}

void InputManager::handle_cursor_touch_motion(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_touch_motion_listener);
    auto* event = static_cast<struct wlr_touch_motion_event*>(data);

    double lx = 0.0, ly = 0.0;
    wlr_cursor_absolute_to_layout_coords(manager->m_cursor, &event->touch->base, event->x, event->y, &lx, &ly);

    double sx = 0.0, sy = 0.0;
    struct wlr_surface* surface = nullptr;
    manager->m_server->view_at(lx, ly, &surface, &sx, &sy);

    if (surface) {
        wlr_seat_touch_notify_motion(manager->m_seat, event->time_msec, event->touch_id, sx, sy);
    }
}

void InputManager::handle_cursor_touch_cancel(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_touch_cancel_listener);
    auto* event = static_cast<struct wlr_touch_cancel_event*>(data);
    wlr_seat_touch_notify_clear_focus(manager->m_seat, event->time_msec, event->touch_id);
}

void InputManager::handle_cursor_touch_frame(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_touch_frame_listener);
    wlr_seat_touch_notify_frame(manager->m_seat);
}

Keyboard::Keyboard(Server* server, struct wlr_input_device* device)
    : m_server(server), m_device(device)
{
    m_keyboard = wlr_keyboard_from_input_device(device);

    struct xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_rule_names rules = {};
    struct xkb_keymap* keymap = xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    wlr_keyboard_set_keymap(m_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(m_keyboard, 25, 600);

    m_modifiers_listener.notify = handle_modifiers;
    wl_signal_add(&m_keyboard->events.modifiers, &m_modifiers_listener);

    m_key_listener.notify = handle_key;
    wl_signal_add(&m_keyboard->events.key, &m_key_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&device->events.destroy, &m_destroy_listener);

    wlr_seat_set_keyboard(server->get_input_manager()->get_seat(), m_keyboard);
}

Keyboard::~Keyboard() {
    wl_list_remove(&m_modifiers_listener.link);
    wl_list_remove(&m_key_listener.link);
    wl_list_remove(&m_destroy_listener.link);
}

void Keyboard::handle_modifiers(struct wl_listener* listener, void* data) {
    Keyboard* kb = wl_container_of(listener, kb, m_modifiers_listener);
    struct wlr_seat* seat = kb->m_server->get_input_manager()->get_seat();
    
    wlr_seat_set_keyboard(seat, kb->m_keyboard);
    wlr_seat_keyboard_notify_modifiers(seat, &kb->m_keyboard->modifiers);
}

void Keyboard::handle_key(struct wl_listener* listener, void* data) {
    Keyboard* kb = wl_container_of(listener, kb, m_key_listener);
    auto* event = static_cast<struct wlr_keyboard_key_event*>(data);
    struct wlr_seat* seat = kb->m_server->get_input_manager()->get_seat();

    if (kb->m_server->get_idle_notifier()) {
        wlr_idle_notifier_v1_notify_activity(kb->m_server->get_idle_notifier(), seat);
    }

    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t* syms;
    int nsyms = xkb_state_key_get_syms(kb->m_keyboard->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = wlr_keyboard_get_modifiers(kb->m_keyboard);

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < nsyms; ++i) {
            handled = kb->m_server->get_input_manager()->handle_keybinding(modifiers, syms[i]);
            if (handled) break;
        }

        // If not handled (e.g. Shift was held down turning '1' into '!'), check unshifted base sym
        if (!handled && kb->m_keyboard->keymap) {
            xkb_layout_index_t layout = xkb_state_key_get_layout(kb->m_keyboard->xkb_state, keycode);
            const xkb_keysym_t* raw_syms;
            int n_raw_syms = xkb_keymap_key_get_syms_by_level(kb->m_keyboard->keymap, keycode, layout, 0, &raw_syms);
            for (int i = 0; i < n_raw_syms; ++i) {
                handled = kb->m_server->get_input_manager()->handle_keybinding(modifiers, raw_syms[i]);
                if (handled) break;
            }
        }
    }

    if (!handled) {
        wlr_seat_set_keyboard(seat, kb->m_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
    }
}

void Keyboard::handle_destroy(struct wl_listener* listener, void* data) {
    Keyboard* kb = wl_container_of(listener, kb, m_destroy_listener);
    kb->m_server->get_input_manager()->remove_keyboard(kb);
}

} // namespace miquland
