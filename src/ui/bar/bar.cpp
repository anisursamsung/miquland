#include "ui/bar/bar.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/view.hpp"
#include "config/config.hpp"
#include "ui/menu/menu.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace biway {

static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

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

    // 1. Check Menu Button Click
    if (lx >= m_menu_btn.x && lx <= m_menu_btn.x + m_menu_btn.width &&
        ly >= m_menu_btn.y && ly <= m_menu_btn.y + m_menu_btn.height) {
        if (m_server->get_menu()) {
            m_server->get_menu()->toggle();
        }
        return true;
    }

    // 2. Check Workspace Buttons
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

    auto set_cairo_hex = [&](const std::string& hex, float default_r, float default_g, float default_b, float default_a = 1.0f) {
        float r = default_r, g = default_g, b = default_b, a = default_a;
        if (!Config::parse_hex_color(hex, r, g, b, a)) {
            r = default_r; g = default_g; b = default_b; a = default_a;
        }
        cairo_set_source_rgba(cr, r, g, b, a);
    };

    // 1. Draw Bar Background
    set_cairo_hex(Config::get().get_color_background(), 0.08f, 0.08f, 0.12f);
    cairo_rectangle(cr, 0, 0, m_width, m_height);
    cairo_fill(cr);

    // Subtle bottom border
    set_cairo_hex(Config::get().get_color_outline_variant(), 0.16f, 0.16f, 0.22f);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, m_height - 0.5);
    cairo_line_to(cr, m_width, m_height - 0.5);
    cairo_stroke(cr);

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    int btn_height = m_height - 8;
    int btn_y = 4;

    // 2. Left: [ ☰ Menu ] Button
    pango_layout_set_text(layout, "☰ Menu", -1);
    int menu_text_w, menu_text_h;
    pango_layout_get_pixel_size(layout, &menu_text_w, &menu_text_h);

    int menu_btn_w = menu_text_w + 18;
    int menu_btn_x = 8;

    set_cairo_hex(Config::get().get_color_secondary(), 0.18f, 0.19f, 0.28f);
    draw_rounded_rect(cr, menu_btn_x, btn_y, menu_btn_w, btn_height, 4.0);
    cairo_fill(cr);

    set_cairo_hex(Config::get().get_color_outline(), 0.25f, 0.27f, 0.38f);
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, menu_btn_x, btn_y, menu_btn_w, btn_height, 4.0);
    cairo_stroke(cr);

    set_cairo_hex(Config::get().get_color_on_secondary(), 0.89f, 0.91f, 0.98f);
    cairo_move_to(cr, menu_btn_x + (menu_btn_w - menu_text_w) / 2, btn_y + (btn_height - menu_text_h) / 2);
    pango_cairo_show_layout(cr, layout);

    m_menu_btn = { menu_btn_x, btn_y, menu_btn_w, btn_height };

    // 3. Center: Centered Workspaces (1..6)
    size_t active_ws = m_server->get_workspace_manager()->get_active_workspace_id();
    const size_t num_ws = 6;
    const int ws_btn_w = 28;
    const int ws_spacing = 6;
    int total_ws_w = static_cast<int>(num_ws * ws_btn_w + (num_ws - 1) * ws_spacing);
    int start_ws_x = (m_width - total_ws_w) / 2;

    int current_ws_x = start_ws_x;
    for (size_t id = 1; id <= num_ws; ++id) {
        std::string ws_label = std::to_string(id);
        pango_layout_set_text(layout, ws_label.c_str(), -1);

        int text_w, text_h;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        if (id == active_ws) {
            // Active badge
            set_cairo_hex(Config::get().get_color_primary(), 0.54f, 0.71f, 0.98f);
            draw_rounded_rect(cr, current_ws_x, btn_y, ws_btn_w, btn_height, 4.0);
            cairo_fill(cr);

            set_cairo_hex(Config::get().get_color_on_primary(), 0.07f, 0.07f, 0.11f);
        } else {
            // Inactive badge
            set_cairo_hex(Config::get().get_color_surface_variant(), 0.16f, 0.16f, 0.23f);
            draw_rounded_rect(cr, current_ws_x, btn_y, ws_btn_w, btn_height, 4.0);
            cairo_fill(cr);

            set_cairo_hex(Config::get().get_color_on_surface_variant(), 0.65f, 0.68f, 0.80f);
        }

        cairo_move_to(cr, current_ws_x + (ws_btn_w - text_w) / 2, btn_y + (btn_height - text_h) / 2);
        pango_cairo_show_layout(cr, layout);

        m_buttons.push_back({ current_ws_x, btn_y, ws_btn_w, btn_height, id });
        current_ws_x += ws_btn_w + ws_spacing;
    }

    // 4. Right: Clock & Date
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
    set_cairo_hex(Config::get().get_color_on_surface(), 0.90f, 0.93f, 0.98f);
    cairo_move_to(cr, clock_x, (m_height - clock_h) / 2);
    pango_cairo_show_layout(cr, layout);

    // 5. Left of Workspaces: Active Window Title
    View* focused = m_server->get_focused_view();
    std::string title = "";
    if (focused && focused->get_xdg_toplevel() && focused->get_xdg_toplevel()->title) {
        title = focused->get_xdg_toplevel()->title;
    }

    if (!title.empty()) {
        int avail_title_w = start_ws_x - (menu_btn_x + menu_btn_w + 24);
        if (avail_title_w > 60) {
            pango_layout_set_width(layout, avail_title_w * PANGO_SCALE);
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
            pango_layout_set_text(layout, title.c_str(), -1);

            int tw, th;
            pango_layout_get_pixel_size(layout, &tw, &th);

            set_cairo_hex(Config::get().get_color_on_surface(), 0.80f, 0.84f, 0.96f);
            cairo_move_to(cr, menu_btn_x + menu_btn_w + 14, (m_height - th) / 2);
            pango_cairo_show_layout(cr, layout);
        }
    }

    g_object_unref(layout);

    wlr_scene_buffer_set_buffer(m_scene_buffer, m_buffer->get_wlr_buffer());
}

} // namespace biway
