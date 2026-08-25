#pragma once

#include "util.hpp"
#include "cairo_buffer.hpp"
#include <memory>
#include <vector>
#include <utility>

namespace biway {

class Server;

class Bar {
public:
    explicit Bar(Server* server);
    ~Bar();

    void render(int width);
    void toggle_visibility();
    void set_visible(bool visible);
    bool is_visible() const { return m_visible; }

    int get_height() const { return m_visible ? m_height : 0; }

    bool handle_click(double lx, double ly);
    void schedule_redraw();

private:
    static int handle_timer(void* data);

    Server* m_server = nullptr;
    struct wlr_scene_buffer* m_scene_buffer = nullptr;
    struct wl_event_source* m_timer = nullptr;
    std::unique_ptr<CairoBuffer> m_buffer;

    int m_width = 0;
    int m_height = 30;
    bool m_visible = true;

    struct ButtonRect {
        int x, y, width, height;
    };
    ButtonRect m_menu_btn = { 0, 0, 0, 0 };

    struct WorkspaceButton {
        int x, y, width, height;
        size_t ws_id;
    };
    std::vector<WorkspaceButton> m_buttons;
};

} // namespace biway
