#include "bar.hpp"
#include "server.hpp"
#include "workspace.hpp"
#include "view.hpp"
#include "config.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace biway {

Bar::Bar(Server* server)
    : m_server(server)
{
    m_height = Config::get().get_bar_height();
    m_visible = Config::get().is_bar_visible();

    m_scene_buffer = wlr_scene_buffer_create(server->get_bar_tree(), nullptr);
    wlr_scene_node_set_enabled(&m_scene_buffer->node, m_visible);

    // Setup 1-second refresh timer for clock and status
    m_timer = wl_event_loop_add_timer(server->get_event_loop(), handle_timer, this);
    wl_event_source_timer_update(m_timer, 1000);
}

Bar::~Bar() {
    if (m_timer) {
        wl_event_source_remove(m_timer);
    }
}

int Bar::handle_timer(void* data) {
    auto* bar = static_cast<Bar*>(data);
    if (bar->m_visible && bar->m_width > 0) {
        bar->render(bar->m_width);
    }
    wl_event_source_timer_update(bar->m_timer, 1000);
    return 0;
}

void Bar::schedule_redraw() {
    if (m_visible && m_width > 0) {
        render(m_width);
    }
}

void Bar::toggle_visibility() {
    set_visible(!m_visible);
}

void Bar::set_visible(bool visible) {
    if (m_visible == visible) return;

    m_visible = visible;
    Config::get().set_bar_visible(visible);
    wlr_scene_node_set_enabled(&m_scene_buffer->node, m_visible);

    if (m_visible && m_width > 0) {
        render(m_width);
    }

    m_server->get_workspace_manager()->recalculate_layout();
}

bool Bar::handle_click(double lx, double ly) {
    if (!m_visible || ly < 0 || ly > m_height) {
        return false;
    }

    for (const auto& btn : m_buttons) {
        if (lx >= btn.x && lx <= btn.x + btn.width &&
            ly >= btn.y && ly <= btn.y + btn.height) {
            m_server->get_workspace_manager()->switch_to_workspace(btn.ws_id);
            return true;
        }
    }

    return true; // Clicked on bar, consume event
}

void Bar::render(int width) {
    if (width <= 0 || !m_visible) return;

    m_width = width;
    m_height = Config::get().get_bar_height();

    if (!m_buffer) {
        m_buffer = std::make_unique<CairoBuffer>(m_width, m_height);
    } else {
        m_buffer->resize(m_width, m_height);
    }

    cairo_t* cr = m_buffer->get_cairo();
    m_buttons.clear();

    // 1. Draw Bar Background
    cairo_set_source_rgb(cr, 0.08, 0.08, 0.12); // #14141e
    cairo_rectangle(cr, 0, 0, m_width, m_height);
    cairo_fill(cr);

    // Subtle bottom border
    cairo_set_source_rgb(cr, 0.22, 0.22, 0.30);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, m_height - 0.5);
    cairo_line_to(cr, m_width, m_height - 0.5);
    cairo_stroke(cr);

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    // 2. Left: Workspace Badges
    size_t active_ws = m_server->get_workspace_manager()->get_active_workspace_id();
    int current_x = 8;
    int btn_height = m_height - 8;
    int btn_y = 4;

    for (size_t id = 1; id <= 6; ++id) {
        std::string ws_label = std::to_string(id);
        pango_layout_set_text(layout, ws_label.c_str(), -1);

        int text_w, text_h;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        int btn_w = std::max(26, text_w + 14);

        if (id == active_ws) {
            // Active badge (highlighted)
            cairo_set_source_rgb(cr, 0.54, 0.71, 0.98); // #89b4fa
            cairo_rectangle(cr, current_x, btn_y, btn_w, btn_height);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.07, 0.07, 0.11); // Dark text
        } else {
            // Inactive badge
            cairo_set_source_rgb(cr, 0.16, 0.16, 0.23); // #29293a
            cairo_rectangle(cr, current_x, btn_y, btn_w, btn_height);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.75, 0.78, 0.90); // Light text
        }

        cairo_move_to(cr, current_x + (btn_w - text_w) / 2, btn_y + (btn_height - text_h) / 2);
        pango_cairo_show_layout(cr, layout);

        m_buttons.push_back({ current_x, btn_y, btn_w, btn_height, id });
        current_x += btn_w + 6;
    }

    // 3. Right: Clock & Date
    time_t rawtime;
    time(&rawtime);
    struct tm timeinfo = {};
    localtime_r(&rawtime, &timeinfo);

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%a %b %d  %H:%M:%S", &timeinfo);

    pango_layout_set_text(layout, time_str, -1);
    int clock_w, clock_h;
    pango_layout_get_pixel_size(layout, &clock_w, &clock_h);

    int clock_x = m_width - clock_w - 14;
    cairo_set_source_rgb(cr, 0.90, 0.93, 0.98);
    cairo_move_to(cr, clock_x, (m_height - clock_h) / 2);
    pango_cairo_show_layout(cr, layout);

    // 4. Center: Active Window Title
    View* focused = m_server->get_focused_view();
    std::string title = "biway";
    if (focused && focused->get_xdg_toplevel() && focused->get_xdg_toplevel()->title) {
        title = focused->get_xdg_toplevel()->title;
    }

    // Truncate long titles
    if (title.length() > 60) {
        title = title.substr(0, 57) + "...";
    }

    pango_layout_set_text(layout, title.c_str(), -1);
    int title_w, title_h;
    pango_layout_get_pixel_size(layout, &title_w, &title_h);

    int title_x = std::max(current_x + 10, (m_width - title_w) / 2);
    if (title_x + title_w < clock_x - 10) {
        cairo_set_source_rgb(cr, 0.80, 0.84, 0.96);
        cairo_move_to(cr, title_x, (m_height - title_h) / 2);
        pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);

    wlr_scene_buffer_set_buffer(m_scene_buffer, m_buffer->get_wlr_buffer());
}

} // namespace biway
