#pragma once

#include "core/common/util.hpp"
#include "toolkit/render/cairo_buffer.hpp"
#include "toolkit/layout/row_view.hpp"
#include "toolkit/widgets/button_view.hpp"
#include "toolkit/widgets/text_view.hpp"
#include <memory>
#include <vector>

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

    std::shared_ptr<RowView> m_bar_layout;
    std::shared_ptr<ButtonView> m_menu_btn;
    std::vector<std::shared_ptr<ButtonView>> m_ws_buttons;
    std::shared_ptr<TextView> m_title_view;
    std::shared_ptr<TextView> m_clock_view;
};

} // namespace biway
