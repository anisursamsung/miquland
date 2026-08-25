#include "input.hpp"
#include "server.hpp"
#include "workspace.hpp"
#include "view.hpp"
#include "output.hpp"
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
}

InputManager::~InputManager() {
    wl_list_remove(&m_new_input_listener.link);
    wl_list_remove(&m_cursor_motion_listener.link);
    wl_list_remove(&m_cursor_motion_absolute_listener.link);
    wl_list_remove(&m_cursor_button_listener.link);
    wl_list_remove(&m_cursor_axis_listener.link);
    wl_list_remove(&m_cursor_frame_listener.link);
    wl_list_remove(&m_request_set_cursor_listener.link);

    if (m_cursor_mgr) wlr_xcursor_manager_destroy(m_cursor_mgr);
    if (m_cursor) wlr_cursor_destroy(m_cursor);
}

void InputManager::spawn_command(const char* cmd) {
    if (!cmd || !*cmd) return;
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

bool InputManager::handle_keybinding(uint32_t modifiers, xkb_keysym_t keysym) {
    bool mod = (modifiers & WLR_MODIFIER_LOGO) != 0;
    bool shift = (modifiers & WLR_MODIFIER_SHIFT) != 0;

    if (!mod) {
        return false;
    }

    // Terminal: Super + Return
    if (keysym == XKB_KEY_Return) {
        const char* term = getenv("TERMINAL");
        if (!term) term = "foot || alacritty || kitty || weston-terminal || xterm";
        spawn_command(term);
        return true;
    }

    // App launcher: Super + D or Super + Space
    if (keysym == XKB_KEY_d || keysym == XKB_KEY_space) {
        spawn_command("fuzzel || wofi --show drun || bemenu-run || dmenu_run");
        return true;
    }

    // Close window: Super + Q or Super + Shift + C
    if ((keysym == XKB_KEY_q && !shift) || (keysym == XKB_KEY_c && shift) || (keysym == XKB_KEY_C)) {
        View* focused = m_server->get_focused_view();
        if (focused) {
            focused->close();
        }
        return true;
    }

    // Focus switching: Super + H / Left (Prev), Super + L / Right / Tab (Next)
    if (keysym == XKB_KEY_h || keysym == XKB_KEY_Left) {
        m_server->get_workspace_manager()->focus_prev_view();
        return true;
    }
    if (keysym == XKB_KEY_l || keysym == XKB_KEY_Right || keysym == XKB_KEY_Tab) {
        m_server->get_workspace_manager()->focus_next_view();
        return true;
    }

    // Workspace switching & moving: Super + [1..9] and Super + Shift + [1..9]
    if (keysym >= XKB_KEY_1 && keysym <= XKB_KEY_9) {
        size_t ws_id = (keysym - XKB_KEY_1) + 1;
        if (shift) {
            View* focused = m_server->get_focused_view();
            if (focused) {
                m_server->get_workspace_manager()->move_view_to_workspace(focused, ws_id);
            }
        } else {
            m_server->get_workspace_manager()->switch_to_workspace(ws_id);
        }
        return true;
    }

    // Exit compositor: Super + Shift + E or Super + Shift + Q
    if (shift && (keysym == XKB_KEY_e || keysym == XKB_KEY_E || keysym == XKB_KEY_q || keysym == XKB_KEY_Q)) {
        m_server->terminate();
        return true;
    }

    return false;
}

void InputManager::process_cursor_motion(uint32_t time) {
    double sx, sy;
    struct wlr_surface* surface = nullptr;
    m_server->view_at(m_cursor->x, m_cursor->y, &surface, &sx, &sy);

    if (!surface) {
        wlr_cursor_set_xcursor(m_cursor, m_cursor_mgr, "default");
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(m_seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(m_seat, time, sx, sy);
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
        wlr_cursor_attach_input_device(manager->m_cursor, device);
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
    manager->process_cursor_motion(event->time_msec);
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
