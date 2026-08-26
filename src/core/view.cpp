#include "core/view.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "input/input.hpp"
#include "config/config.hpp"
#include "ui/wallpaper/wallpaper.hpp"
#include <cmath>
#include <algorithm>

namespace biway {

static void draw_rounded_rectangle(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    r = std::min(r, std::min(w / 2.0, h / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

View::View(Server* server, struct wlr_xdg_toplevel* toplevel)
    : m_server(server), m_xdg_toplevel(toplevel)
{
    // Create view container scene tree under root scene (reparented to workspace later)
    m_scene_tree = wlr_scene_tree_create(&server->get_scene()->tree);
    m_scene_tree->node.data = this;

    // Create xdg surface scene tree under view container tree (Child 1 - bottom layer)
    m_xdg_scene_tree = wlr_scene_xdg_surface_create(m_scene_tree, toplevel->base);
    m_xdg_scene_tree->node.data = this;
    toplevel->base->data = m_xdg_scene_tree;

    // Create border scene buffer on top of xdg surface (Child 2 - top overlay layer)
    m_border_scene_buffer = wlr_scene_buffer_create(m_scene_tree, nullptr);
    m_border_scene_buffer->point_accepts_input = [](struct wlr_scene_buffer*, double*, double*) -> bool {
        return false;
    };

    // Connect event listeners
    m_map_listener.notify = handle_map;
    wl_signal_add(&toplevel->base->surface->events.map, &m_map_listener);

    m_unmap_listener.notify = handle_unmap;
    wl_signal_add(&toplevel->base->surface->events.unmap, &m_unmap_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&toplevel->events.destroy, &m_destroy_listener);

    m_commit_listener.notify = handle_commit;
    wl_signal_add(&toplevel->base->surface->events.commit, &m_commit_listener);

    m_request_fullscreen_listener.notify = handle_request_fullscreen;
    wl_signal_add(&toplevel->events.request_fullscreen, &m_request_fullscreen_listener);

    m_request_maximize_listener.notify = handle_request_maximize;
    wl_signal_add(&toplevel->events.request_maximize, &m_request_maximize_listener);

    m_set_title_listener.notify = handle_set_title;
    wl_signal_add(&toplevel->events.set_title, &m_set_title_listener);

    m_set_app_id_listener.notify = handle_set_app_id;
    wl_signal_add(&toplevel->events.set_app_id, &m_set_app_id_listener);

    // Create foreign toplevel handle for taskbars and notification daemons
    if (server->get_foreign_toplevel_manager()) {
        m_foreign_toplevel = wlr_foreign_toplevel_handle_v1_create(server->get_foreign_toplevel_manager());
        if (m_foreign_toplevel) {
            if (toplevel->title) wlr_foreign_toplevel_handle_v1_set_title(m_foreign_toplevel, toplevel->title);
            if (toplevel->app_id) wlr_foreign_toplevel_handle_v1_set_app_id(m_foreign_toplevel, toplevel->app_id);

            m_foreign_request_activate_listener.notify = handle_foreign_request_activate;
            wl_signal_add(&m_foreign_toplevel->events.request_activate, &m_foreign_request_activate_listener);

            m_foreign_request_close_listener.notify = handle_foreign_request_close;
            wl_signal_add(&m_foreign_toplevel->events.request_close, &m_foreign_request_close_listener);
        }
    }
}

View::~View() {
    wl_list_remove(&m_map_listener.link);
    wl_list_remove(&m_unmap_listener.link);
    wl_list_remove(&m_destroy_listener.link);
    wl_list_remove(&m_commit_listener.link);
    wl_list_remove(&m_request_fullscreen_listener.link);
    wl_list_remove(&m_request_maximize_listener.link);
    wl_list_remove(&m_set_title_listener.link);
    wl_list_remove(&m_set_app_id_listener.link);

    if (m_foreign_toplevel) {
        wl_list_remove(&m_foreign_request_activate_listener.link);
        wl_list_remove(&m_foreign_request_close_listener.link);
        wlr_foreign_toplevel_handle_v1_destroy(m_foreign_toplevel);
        m_foreign_toplevel = nullptr;
    }

    if (m_scene_tree) {
        wlr_scene_node_destroy(&m_scene_tree->node);
        m_scene_tree = nullptr;
    }
}

void View::set_workspace(Workspace* ws) {
    if (m_workspace == ws) return;
    m_workspace = ws;
    if (m_workspace && m_scene_tree) {
        wlr_scene_node_reparent(&m_scene_tree->node, m_workspace->get_scene_tree());
    }
}

bool View::is_focused() const {
    return m_server && m_server->get_focused_view() == this;
}

void View::set_geometry(int x, int y, int width, int height) {
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;

    if (m_scene_tree) {
        wlr_scene_node_set_position(&m_scene_tree->node, x, y);
    }

    int bw = Config::get().get_window_border_width();
    int client_w = std::max(1, width - 2 * bw);
    int client_h = std::max(1, height - 2 * bw);

    if (m_xdg_scene_tree) {
        wlr_scene_node_set_position(&m_xdg_scene_tree->node, bw, bw);
    }
    if (m_xdg_toplevel) {
        wlr_xdg_toplevel_set_size(m_xdg_toplevel, client_w, client_h);
    }

    update_border();
}

void View::update_border() {
    if (!m_mapped || m_width <= 0 || m_height <= 0) {
        return;
    }

    int bw = Config::get().get_window_border_width();
    int radius = Config::get().get_window_border_radius();

    // If both border width and radius are 0, we can disable the overlay buffer
    if (bw <= 0 && radius <= 0) {
        if (m_border_scene_buffer) {
            wlr_scene_node_set_enabled(&m_border_scene_buffer->node, false);
        }
        return;
    }

    wlr_scene_node_set_enabled(&m_border_scene_buffer->node, true);

    if (!m_border_buffer) {
        m_border_buffer = std::make_unique<CairoBuffer>(m_width, m_height);
    } else {
        m_border_buffer->resize(m_width, m_height);
    }

    cairo_t* cr = m_border_buffer->get_cairo();
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // 1. Mask exterior corners if radius > 0 so square client corners don't protrude!
    if (radius > 0) {
        cairo_save(cr);
        cairo_rectangle(cr, 0, 0, m_width, m_height);
        draw_rounded_rectangle(cr, 0, 0, m_width, m_height, radius);
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_clip(cr);

        Wallpaper* wp = m_server->get_wallpaper();
        cairo_surface_t* wp_surf = wp ? wp->get_surface() : nullptr;
        if (wp_surf) {
            cairo_set_source_surface(cr, wp_surf, -m_x, -m_y);
            cairo_paint(cr);
        } else {
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_paint(cr);
        }
        cairo_restore(cr);
    }

    // 2. Draw border stroke if bw > 0
    if (bw > 0) {
        float r = 0.0f, g = 0.8f, b = 1.0f, a = 1.0f;
        const std::string& color_str = is_focused()
            ? Config::get().get_window_border_color_active()
            : Config::get().get_window_border_color_inactive();

        if (!Config::parse_hex_color(color_str, r, g, b, a)) {
            if (is_focused()) {
                r = 0.0f; g = 0.82f; b = 1.0f; a = 1.0f; // #00d2ff
            } else {
                r = 0.16f; g = 0.16f; b = 0.21f; a = 1.0f; // #2a2a36
            }
        }

        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_set_line_width(cr, bw);

        double half_bw = bw / 2.0;
        double draw_x = half_bw;
        double draw_y = half_bw;
        double draw_w = std::max(1.0, (double)m_width - bw);
        double draw_h = std::max(1.0, (double)m_height - bw);
        double draw_r = std::max(0.0, (double)radius - half_bw);

        draw_rounded_rectangle(cr, draw_x, draw_y, draw_w, draw_h, draw_r);
        cairo_stroke(cr);
    }

    wlr_scene_buffer_set_buffer(m_border_scene_buffer, m_border_buffer->get_wlr_buffer());
}

void View::focus() {
    if (!m_mapped || !m_xdg_toplevel) return;

    // Deactivate currently focused view
    View* prev = m_server->get_focused_view();
    if (prev && prev != this) {
        if (prev->get_xdg_toplevel()) {
            wlr_xdg_toplevel_set_activated(prev->get_xdg_toplevel(), false);
        }
        if (prev->m_foreign_toplevel) {
            wlr_foreign_toplevel_handle_v1_set_activated(prev->m_foreign_toplevel, false);
        }
    }

    if (m_scene_tree) {
        wlr_scene_node_raise_to_top(&m_scene_tree->node);
    }
    wlr_xdg_toplevel_set_activated(m_xdg_toplevel, true);
    if (m_foreign_toplevel) {
        wlr_foreign_toplevel_handle_v1_set_activated(m_foreign_toplevel, true);
    }
    m_server->set_focused_view(this);

    if (prev && prev != this) {
        prev->update_border();
    }
    update_border();

    // Pass keyboard focus via seat
    struct wlr_seat* seat = m_server->get_input_manager()->get_seat();
    struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
    if (keyboard) {
        wlr_seat_keyboard_notify_enter(seat, m_xdg_toplevel->base->surface,
            keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
}

void View::close() {
    if (m_xdg_toplevel) {
        wlr_xdg_toplevel_send_close(m_xdg_toplevel);
    }
}

void View::handle_map(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_map_listener);
    view->m_mapped = true;
    view->m_server->get_workspace_manager()->add_view_auto(view);
    view->focus();
}

void View::handle_unmap(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_unmap_listener);
    view->m_mapped = false;
    view->m_server->get_workspace_manager()->remove_view(view);
}

void View::handle_destroy(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_destroy_listener);
    view->m_server->remove_view(view);
}

void View::handle_commit(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_commit_listener);
    if (view->m_xdg_toplevel->base->initial_commit) {
        int bw = Config::get().get_window_border_width();
        int client_w = std::max(1, view->m_width - 2 * bw);
        int client_h = std::max(1, view->m_height - 2 * bw);
        wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, client_w, client_h);
    }
}

void View::handle_request_fullscreen(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_fullscreen_listener);
    if (view->m_xdg_toplevel->base->surface->mapped) {
        // Keep biway tiling layout
        wlr_xdg_toplevel_set_fullscreen(view->m_xdg_toplevel, false);
    }
}

void View::handle_request_maximize(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_maximize_listener);
    if (view->m_xdg_toplevel->base->surface->mapped) {
        wlr_xdg_toplevel_set_maximized(view->m_xdg_toplevel, false);
    }
}

void View::handle_set_title(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_title_listener);
    if (view->m_foreign_toplevel && view->m_xdg_toplevel->title) {
        wlr_foreign_toplevel_handle_v1_set_title(view->m_foreign_toplevel, view->m_xdg_toplevel->title);
    }
}

void View::handle_set_app_id(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_app_id_listener);
    if (view->m_foreign_toplevel && view->m_xdg_toplevel->app_id) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->m_foreign_toplevel, view->m_xdg_toplevel->app_id);
    }
}

void View::handle_foreign_request_activate(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_foreign_request_activate_listener);
    view->focus();
}

void View::handle_foreign_request_close(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_foreign_request_close_listener);
    view->close();
}

} // namespace biway
