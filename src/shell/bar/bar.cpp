#include "shell/bar/bar.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/view.hpp"
#include "core/config/config.hpp"
#include "shell/menu/menu.hpp"
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

    // Setup Toolkit Layout & Widgets
    m_bar_layout = std::make_shared<RowView>();
    m_bar_layout->set_spacing(6);
    m_bar_layout->set_padding(8, 8, 3, 3);

    // 1. Left: [ ☰ Menu ] Button
    m_menu_btn = std::make_shared<ButtonView>("☰ Menu");
    m_menu_btn->set_font_size(10);
    m_menu_btn->set_font_bold(true);
   m_menu_btn->set_corner_radius(Config::get().get_window_border_radius());
    m_menu_btn->set_padding(10, 4);
    m_menu_btn->set_on_click([this]() {
        if (m_server->get_menu()) {
            m_server->get_menu()->toggle();
        }
    });
    m_bar_layout->add_left(m_menu_btn);

    // Left: Active Window Title
    m_title_view = std::make_shared<TextView>("");
    m_title_view->set_font_size(10);
    m_title_view->set_bold(false);
    m_title_view->set_ellipsize(true);
    m_bar_layout->add_left(m_title_view);

    // 2. Center: Workspaces (1..6)
    for (size_t id = 1; id <= 6; ++id) {
        auto btn = std::make_shared<ButtonView>(std::to_string(id));
        btn->set_font_size(10);
        btn->set_font_bold(true);
        btn->set_corner_radius(Config::get().get_window_border_radius());
        btn->set_padding(10, 4);
        btn->set_on_click([this, id]() {
            m_server->get_workspace_manager()->switch_to_workspace(id);
            schedule_redraw();
        });
        m_ws_buttons.push_back(btn);
        m_bar_layout->add_center(btn);
    }

    // 3. Right: Clock
    m_clock_view = std::make_shared<TextView>("");
    m_clock_view->set_font_size(10);
    m_clock_view->set_bold(false);
    m_bar_layout->add_right(m_clock_view);

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

    int ix = static_cast<int>(lx);
    int iy = static_cast<int>(ly);

    m_bar_layout->handle_mouse_button(ix, iy, 0, true);
    m_bar_layout->handle_mouse_button(ix, iy, 0, false);
    schedule_redraw();

    return true;
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
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);


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

    // 2. Update Dynamic Widget Properties
    int live_radius = Config::get().get_window_border_radius();
    m_menu_btn->set_corner_radius(live_radius);
    
    

    // Active workspace pill highlight
    size_t active_ws = m_server->get_workspace_manager()->get_active_workspace_id();
    for (size_t id = 1; id <= m_ws_buttons.size(); ++id) {
        m_ws_buttons[id - 1]->set_selected(id == active_ws);
	m_ws_buttons[id - 1]->set_corner_radius(live_radius);
    }

    // Active Window Title
    View* focused = m_server->get_focused_view();
    std::string title = "";
    if (focused) {
        title = focused->get_title();
    }
    m_title_view->set_text(title);
    m_title_view->set_color_hex(Config::get().get_color_on_surface());

   // Live Clock
    time_t rawtime;
    time(&rawtime);
    struct tm timeinfo = {};
    localtime_r(&rawtime, &timeinfo);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%a %b %d  %H:%M:%S", &timeinfo);
    
    m_clock_view->set_text(time_str);
    m_clock_view->set_color_hex(Config::get().get_color_on_surface());
    
  
    int clock_w, clock_h;
    m_clock_view->get_preferred_size(cr, clock_w, clock_h);
    m_clock_view->set_size(clock_w, clock_h);
    // ----------------------------------------

    // 3. Render Widget Tree via RowView
    m_bar_layout->set_bounds(0, 0, m_width, m_height);
    m_bar_layout->render(cr);

    wlr_scene_buffer_set_buffer(m_scene_buffer, m_buffer->get_wlr_buffer());
}

} // namespace biway
