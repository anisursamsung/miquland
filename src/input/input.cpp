#include "input/input.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/view.hpp"
#include "core/output.hpp"
#include "ui/bar/bar.hpp"
#include "config/config.hpp"
#include "ui/menu/menu.hpp"
#include <unistd.h>
#include <cstdlib>

namespace biway {

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

    // Touchpad swipe gesture listeners
    m_cursor_swipe_begin_listener.notify = handle_cursor_swipe_begin;
    wl_signal_add(&m_cursor->events.swipe_begin, &m_cursor_swipe_begin_listener);

    m_cursor_swipe_update_listener.notify = handle_cursor_swipe_update;
    wl_signal_add(&m_cursor->events.swipe_update, &m_cursor_swipe_update_listener);

    m_cursor_swipe_end_listener.notify = handle_cursor_swipe_end;
    wl_signal_add(&m_cursor->events.swipe_end, &m_cursor_swipe_end_listener);
}

InputManager::~InputManager() {
    wl_list_remove(&m_new_input_listener.link);
    wl_list_remove(&m_cursor_motion_listener.link);
    wl_list_remove(&m_cursor_motion_absolute_listener.link);
    wl_list_remove(&m_cursor_button_listener.link);
    wl_list_remove(&m_cursor_axis_listener.link);
    wl_list_remove(&m_cursor_frame_listener.link);
    wl_list_remove(&m_request_set_cursor_listener.link);

    wl_list_remove(&m_cursor_swipe_begin_listener.link);
    wl_list_remove(&m_cursor_swipe_update_listener.link);
    wl_list_remove(&m_cursor_swipe_end_listener.link);

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

bool InputManager::handle_keybinding(uint32_t modifiers, xkb_keysym_t keysym) {
    xkb_keysym_t norm_sym = (keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z) ? (keysym - XKB_KEY_A + XKB_KEY_a) : keysym;

    bool mod = (modifiers & WLR_MODIFIER_LOGO) != 0;
    bool shift = (modifiers & WLR_MODIFIER_SHIFT) != 0;
    bool ctrl = (modifiers & WLR_MODIFIER_CTRL) != 0;
    bool alt = (modifiers & WLR_MODIFIER_ALT) != 0;

    // 1. If Menu is open, check if modal consumes the key
    if (m_server->get_menu() && m_server->get_menu()->is_visible()) {
        // System Exit: Super + Shift + Q
        if (mod && shift && norm_sym == XKB_KEY_q) {
            m_server->terminate();
            return true;
        }
        return m_server->get_menu()->handle_key(modifiers, keysym);
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
                if (m_server->get_menu()) m_server->get_menu()->toggle();
            } else if (action == "close" || action == "close_window") {
                View* focused = m_server->get_focused_view();
                if (focused) focused->close();
            } else if (action == "toggle_bar") {
                if (m_server->get_bar()) m_server->get_bar()->toggle_visibility();
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
    double sx, sy;
    struct wlr_surface* surface = nullptr;
    View* view = m_server->view_at(m_cursor->x, m_cursor->y, &surface, &sx, &sy);

    if (!surface) {
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(m_seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(m_seat, time, sx, sy);

        // Hover to focus: if cursor hovers over a view and menu is not open, focus it!
        if (view && view != m_server->get_focused_view()) {
            if (!m_server->get_menu() || !m_server->get_menu()->is_visible()) {
                view->focus();
            }
        }
    } else {
        wlr_seat_pointer_clear_focus(m_seat);
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
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!manager->m_keyboards.empty()) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    wlr_seat_set_capabilities(manager->m_seat, caps);
}

void InputManager::handle_cursor_motion(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_motion_listener);
    auto* event = static_cast<struct wlr_pointer_motion_event*>(data);

    wlr_cursor_move(manager->m_cursor, &event->pointer->base, event->delta_x, event->delta_y);
    if (manager->m_server->get_menu() && manager->m_server->get_menu()->is_visible()) {
        manager->m_server->get_menu()->handle_mouse_move(manager->m_cursor->x, manager->m_cursor->y);
    }
    manager->process_cursor_motion(event->time_msec);
}

void InputManager::handle_cursor_motion_absolute(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_motion_absolute_listener);
    auto* event = static_cast<struct wlr_pointer_motion_absolute_event*>(data);

    wlr_cursor_warp_absolute(manager->m_cursor, &event->pointer->base, event->x, event->y);
    if (manager->m_server->get_menu() && manager->m_server->get_menu()->is_visible()) {
        manager->m_server->get_menu()->handle_mouse_move(manager->m_cursor->x, manager->m_cursor->y);
    }
    manager->process_cursor_motion(event->time_msec);
}

void InputManager::handle_cursor_button(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_button_listener);
    auto* event = static_cast<struct wlr_pointer_button_event*>(data);

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
        // If Menu is open, route click to Menu first
        if (manager->m_server->get_menu() && manager->m_server->get_menu()->is_visible()) {
            if (manager->m_server->get_menu()->handle_mouse_click(manager->m_cursor->x, manager->m_cursor->y)) {
                return;
            }
        }

        // Check if clicked on built-in bar
        if (manager->m_server->get_bar() && manager->m_server->get_bar()->handle_click(manager->m_cursor->x, manager->m_cursor->y)) {
            return;
        }
    }

    double sx, sy;
    struct wlr_surface* surface = nullptr;
    View* view = manager->m_server->view_at(manager->m_cursor->x, manager->m_cursor->y, &surface, &sx, &sy);

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED && view) {
        view->focus();
    }

    wlr_seat_pointer_notify_button(manager->m_seat, event->time_msec, event->button, event->state);
}

void InputManager::handle_cursor_axis(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_axis_listener);
    auto* event = static_cast<struct wlr_pointer_axis_event*>(data);

    if (manager->m_server->get_menu() && manager->m_server->get_menu()->is_visible()) {
        manager->m_server->get_menu()->handle_scroll(event->delta);
        return;
    }

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
            size_t current = manager->m_server->get_workspace_manager()->get_active_workspace_id();
            if (current > 1) {
                manager->m_server->get_workspace_manager()->switch_to_workspace(current - 1);
            }
            manager->m_swipe_triggered = true;
        } else if (manager->m_swipe_dx < -threshold) {
            // Swiped Left -> Switch to next workspace
            size_t current = manager->m_server->get_workspace_manager()->get_active_workspace_id();
            manager->m_server->get_workspace_manager()->switch_to_workspace(current + 1);
            manager->m_swipe_triggered = true;
        }
    }
}

void InputManager::handle_cursor_swipe_end(struct wl_listener* listener, void* data) {
    InputManager* manager = wl_container_of(listener, manager, m_cursor_swipe_end_listener);
    manager->m_swipe_dx = 0.0;
    manager->m_swipe_triggered = false;
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

} // namespace biway
