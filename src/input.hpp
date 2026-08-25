#pragma once

#include "util.hpp"
#include <vector>

namespace biway {

class Server;
class Keyboard;

class InputManager {
public:
    explicit InputManager(Server* server);
    ~InputManager();

    struct wlr_seat* get_seat() const { return m_seat; }
    struct wlr_cursor* get_cursor() const { return m_cursor; }

    bool handle_keybinding(uint32_t modifiers, xkb_keysym_t keysym);
    void spawn_command(const char* cmd);

    void remove_keyboard(Keyboard* kb);

private:
    static void handle_new_input(struct wl_listener* listener, void* data);
    static void handle_cursor_motion(struct wl_listener* listener, void* data);
    static void handle_cursor_motion_absolute(struct wl_listener* listener, void* data);
    static void handle_cursor_button(struct wl_listener* listener, void* data);
    static void handle_cursor_axis(struct wl_listener* listener, void* data);
    static void handle_cursor_frame(struct wl_listener* listener, void* data);
    static void handle_request_set_cursor(struct wl_listener* listener, void* data);

    // Touchpad swipe gesture handlers
    static void handle_cursor_swipe_begin(struct wl_listener* listener, void* data);
    static void handle_cursor_swipe_update(struct wl_listener* listener, void* data);
    static void handle_cursor_swipe_end(struct wl_listener* listener, void* data);

    void process_cursor_motion(uint32_t time);

    Server* m_server = nullptr;
    struct wlr_seat* m_seat = nullptr;
    struct wlr_cursor* m_cursor = nullptr;
    struct wlr_xcursor_manager* m_cursor_mgr = nullptr;

    std::vector<std::unique_ptr<Keyboard>> m_keyboards;

    // Gesture tracking state
    double m_swipe_dx = 0.0;
    bool m_swipe_triggered = false;

    struct wl_listener m_new_input_listener;
    struct wl_listener m_cursor_motion_listener;
    struct wl_listener m_cursor_motion_absolute_listener;
    struct wl_listener m_cursor_button_listener;
    struct wl_listener m_cursor_axis_listener;
    struct wl_listener m_cursor_frame_listener;
    struct wl_listener m_request_set_cursor_listener;

    struct wl_listener m_cursor_swipe_begin_listener;
    struct wl_listener m_cursor_swipe_update_listener;
    struct wl_listener m_cursor_swipe_end_listener;
};

class Keyboard {
public:
    Keyboard(Server* server, struct wlr_input_device* device);
    ~Keyboard();

    struct wlr_keyboard* get_wlr_keyboard() const { return m_keyboard; }
    struct wlr_input_device* get_device() const { return m_device; }

private:
    static void handle_modifiers(struct wl_listener* listener, void* data);
    static void handle_key(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);

    Server* m_server = nullptr;
    struct wlr_input_device* m_device = nullptr;
    struct wlr_keyboard* m_keyboard = nullptr;

    struct wl_listener m_modifiers_listener;
    struct wl_listener m_key_listener;
    struct wl_listener m_destroy_listener;
};

} // namespace biway
