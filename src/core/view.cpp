#include "core/view.hpp"
#include "core/popup.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/input/input.hpp"
#include "core/config/config.hpp"
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
    : m_server(server), m_type(ViewType::Xdg), m_xdg_toplevel(toplevel)
{
    // Create view container scene tree under root scene (reparented to workspace later)
    m_scene_tree = wlr_scene_tree_create(&server->get_scene()->tree);
    m_scene_tree->node.data = this;

    // Create xdg surface scene tree under view container tree (Child 1 - bottom layer)
    m_surface_scene_tree = wlr_scene_xdg_surface_create(m_scene_tree, toplevel->base);
    m_surface_scene_tree->node.data = this;
    toplevel->base->data = m_surface_scene_tree;

    // Create border scene buffer on top of xdg surface (Child 2 - top overlay layer)
    m_border_scene_buffer = wlr_scene_buffer_create(m_scene_tree, nullptr);
    m_border_scene_buffer->point_accepts_input = [](struct wlr_scene_buffer*, double*, double*) -> bool {
        return false;
    };

    // Connect XDG event listeners
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

    m_set_parent_listener.notify = handle_set_parent;
    wl_signal_add(&toplevel->events.set_parent, &m_set_parent_listener);

    // Wire up XDG popup listener for context menus and dropdowns
    m_new_popup_listener.notify = handle_new_popup;
    wl_signal_add(&toplevel->base->events.new_popup, &m_new_popup_listener);

    update_parent_relationship();
    setup_foreign_toplevel();
}

View::View(Server* server, struct wlr_xwayland_surface* xsurface)
    : m_server(server), m_type(ViewType::XWayland), m_xwayland_surface(xsurface),
      m_is_override_redirect(xsurface->override_redirect)
{
    // Create view container scene tree
    m_scene_tree = wlr_scene_tree_create(&server->get_scene()->tree);
    m_scene_tree->node.data = this;

    if (m_is_override_redirect) {
        m_x = xsurface->x;
        m_y = xsurface->y;
        m_width = xsurface->width;
        m_height = xsurface->height;
        wlr_scene_node_set_position(&m_scene_tree->node, m_x, m_y);
    } else {
        // Create border scene buffer on top of surface
        m_border_scene_buffer = wlr_scene_buffer_create(m_scene_tree, nullptr);
        m_border_scene_buffer->point_accepts_input = [](struct wlr_scene_buffer*, double*, double*) -> bool {
            return false;
        };
    }

    // Connect XWayland event listeners
    m_associate_listener.notify = handle_xwayland_associate;
    wl_signal_add(&xsurface->events.associate, &m_associate_listener);

    m_dissociate_listener.notify = handle_xwayland_dissociate;
    wl_signal_add(&xsurface->events.dissociate, &m_dissociate_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&xsurface->events.destroy, &m_destroy_listener);

    m_request_configure_listener.notify = handle_xwayland_request_configure;
    wl_signal_add(&xsurface->events.request_configure, &m_request_configure_listener);

    m_request_activate_listener.notify = handle_xwayland_request_activate;
    wl_signal_add(&xsurface->events.request_activate, &m_request_activate_listener);

    m_request_fullscreen_listener.notify = handle_request_fullscreen;
    wl_signal_add(&xsurface->events.request_fullscreen, &m_request_fullscreen_listener);

    m_request_maximize_listener.notify = handle_request_maximize;
    wl_signal_add(&xsurface->events.request_maximize, &m_request_maximize_listener);

    m_set_title_listener.notify = handle_set_title;
    wl_signal_add(&xsurface->events.set_title, &m_set_title_listener);

    m_set_class_listener.notify = handle_xwayland_set_class;
    wl_signal_add(&xsurface->events.set_class, &m_set_class_listener);

    m_set_parent_listener.notify = handle_xwayland_set_parent;
    wl_signal_add(&xsurface->events.set_parent, &m_set_parent_listener);

    m_set_geometry_listener.notify = handle_xwayland_set_geometry;
    wl_signal_add(&xsurface->events.set_geometry, &m_set_geometry_listener);

    m_set_override_redirect_listener.notify = handle_xwayland_set_override_redirect;
    wl_signal_add(&xsurface->events.set_override_redirect, &m_set_override_redirect_listener);

    // If surface was already associated at view creation time
    if (xsurface->surface) {
        handle_xwayland_associate(&m_associate_listener, nullptr);
    }

    setup_foreign_toplevel();
}

View::~View() {
    if (m_type == ViewType::Xdg) {
        wl_list_remove(&m_map_listener.link);
        wl_list_remove(&m_unmap_listener.link);
        wl_list_remove(&m_destroy_listener.link);
        wl_list_remove(&m_commit_listener.link);
        wl_list_remove(&m_request_fullscreen_listener.link);
        wl_list_remove(&m_request_maximize_listener.link);
        wl_list_remove(&m_set_title_listener.link);
        wl_list_remove(&m_set_app_id_listener.link);
        wl_list_remove(&m_set_parent_listener.link);
        wl_list_remove(&m_new_popup_listener.link);
    } else {
        wl_list_remove(&m_associate_listener.link);
        wl_list_remove(&m_dissociate_listener.link);
        wl_list_remove(&m_destroy_listener.link);
        wl_list_remove(&m_request_configure_listener.link);
        wl_list_remove(&m_request_activate_listener.link);
        wl_list_remove(&m_request_fullscreen_listener.link);
        wl_list_remove(&m_request_maximize_listener.link);
        wl_list_remove(&m_set_title_listener.link);
        wl_list_remove(&m_set_class_listener.link);
        wl_list_remove(&m_set_parent_listener.link);
        wl_list_remove(&m_set_geometry_listener.link);
        wl_list_remove(&m_set_override_redirect_listener.link);

        if (m_xwayland_surface && m_xwayland_surface->surface) {
            wl_list_remove(&m_map_listener.link);
            wl_list_remove(&m_unmap_listener.link);
        }
    }

    if (m_parent_view) {
        auto& children = m_parent_view->m_child_dialogs;
        children.erase(std::remove(children.begin(), children.end(), this), children.end());
        m_parent_view = nullptr;
    }
    for (auto* child : m_child_dialogs) {
        if (child) child->m_parent_view = nullptr;
    }
    m_child_dialogs.clear();

    if (m_foreign_toplevel) {
        wl_list_remove(&m_foreign_request_activate_listener.link);
        wl_list_remove(&m_foreign_request_close_listener.link);
        wlr_foreign_toplevel_handle_v1_destroy(m_foreign_toplevel);
        m_foreign_toplevel = nullptr;
    }

    m_popups.clear();

    if (m_scene_tree) {
        wlr_scene_node_destroy(&m_scene_tree->node);
        m_scene_tree = nullptr;
    }
}

void View::setup_foreign_toplevel() {
    if (m_is_override_redirect) return;

    if (m_server->get_foreign_toplevel_manager()) {
        m_foreign_toplevel = wlr_foreign_toplevel_handle_v1_create(m_server->get_foreign_toplevel_manager());
        if (m_foreign_toplevel) {
            std::string title = get_title();
            std::string app_id = get_app_id();
            if (!title.empty()) wlr_foreign_toplevel_handle_v1_set_title(m_foreign_toplevel, title.c_str());
            if (!app_id.empty()) wlr_foreign_toplevel_handle_v1_set_app_id(m_foreign_toplevel, app_id.c_str());

            m_foreign_request_activate_listener.notify = handle_foreign_request_activate;
            wl_signal_add(&m_foreign_toplevel->events.request_activate, &m_foreign_request_activate_listener);

            m_foreign_request_close_listener.notify = handle_foreign_request_close;
            wl_signal_add(&m_foreign_toplevel->events.request_close, &m_foreign_request_close_listener);
        }
    }
}

std::string View::get_title() const {
    if (m_type == ViewType::Xdg) {
        if (m_xdg_toplevel && m_xdg_toplevel->title) return m_xdg_toplevel->title;
    } else {
        if (m_xwayland_surface && m_xwayland_surface->title) return m_xwayland_surface->title;
    }
    return "";
}

std::string View::get_app_id() const {
    if (m_type == ViewType::Xdg) {
        if (m_xdg_toplevel && m_xdg_toplevel->app_id) return m_xdg_toplevel->app_id;
    } else {
        if (m_xwayland_surface) {
            if (m_xwayland_surface->_class) return m_xwayland_surface->_class;
            if (m_xwayland_surface->instance) return m_xwayland_surface->instance;
        }
    }
    return "";
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
    if (m_x == x && m_y == y && m_width == width && m_height == height && m_mapped) {
        return;
    }

    bool size_changed = (m_width != width || m_height != height);
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

    if (m_type == ViewType::Xdg) {
        if (m_surface_scene_tree) {
            wlr_scene_node_set_position(&m_surface_scene_tree->node, bw, bw);
        }
        if (m_xdg_toplevel && size_changed) {
            wlr_xdg_toplevel_set_size(m_xdg_toplevel, client_w, client_h);
        }
    } else {
        if (m_surface_scene_tree) {
            wlr_scene_node_set_position(&m_surface_scene_tree->node, bw, bw);
        }
        if (m_xwayland_surface && size_changed) {
            wlr_xwayland_surface_configure(m_xwayland_surface, x + bw, y + bw, client_w, client_h);
        }
    }

    update_border();
    update_child_dialog_geometries();
}

void View::update_border() {
    if (m_is_override_redirect || !m_mapped || m_width <= 0 || m_height <= 0) {
        if (m_border_scene_buffer) {
            wlr_scene_node_set_enabled(&m_border_scene_buffer->node, false);
        }
        return;
    }

    int bw = Config::get().get_window_border_width();
    int radius = Config::get().get_window_border_radius();

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

    if (bw > 0) {
        float r = 0.0f, g = 0.8f, b = 1.0f, a = 1.0f;
        const std::string& color_str = is_focused()
            ? Config::get().get_window_border_color_active()
            : Config::get().get_window_border_color_inactive();

        if (!Config::parse_hex_color(color_str, r, g, b, a)) {
            if (is_focused()) {
                r = 0.0f; g = 0.82f; b = 1.0f; a = 1.0f;
            } else {
                r = 0.16f; g = 0.16f; b = 0.21f; a = 1.0f;
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
    if (!m_mapped) return;

    if (is_override_redirect()) {
        if (m_scene_tree) {
            wlr_scene_node_raise_to_top(&m_scene_tree->node);
        }
        if (m_xwayland_surface && m_xwayland_surface->surface) {
            struct wlr_seat* seat = m_server->get_input_manager()->get_seat();
            struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
            if (keyboard) {
                wlr_seat_keyboard_notify_enter(seat, m_xwayland_surface->surface,
                    keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
            }
        }
        return;
    }

    View* prev = m_server->get_focused_view();
    if (prev && prev != this) {
        if (prev->m_type == ViewType::Xdg && prev->m_xdg_toplevel) {
            wlr_xdg_toplevel_set_activated(prev->m_xdg_toplevel, false);
        } else if (prev->m_type == ViewType::XWayland && prev->m_xwayland_surface) {
            if (!prev->is_override_redirect()) {
                wlr_xwayland_surface_activate(prev->m_xwayland_surface, false);
            }
        }
        if (prev->m_foreign_toplevel) {
            wlr_foreign_toplevel_handle_v1_set_activated(prev->m_foreign_toplevel, false);
        }
    }

    if (m_scene_tree) {
        wlr_scene_node_raise_to_top(&m_scene_tree->node);
        for (auto* dialog : m_child_dialogs) {
            if (dialog && dialog->is_mapped() && dialog->get_scene_tree()) {
                wlr_scene_node_raise_to_top(&dialog->get_scene_tree()->node);
            }
        }
    }

    struct wlr_surface* target_surface = nullptr;
    if (m_type == ViewType::Xdg && m_xdg_toplevel) {
        wlr_xdg_toplevel_set_activated(m_xdg_toplevel, true);
        target_surface = m_xdg_toplevel->base->surface;
    } else if (m_type == ViewType::XWayland && m_xwayland_surface) {
        if (!is_override_redirect()) {
            wlr_xwayland_surface_activate(m_xwayland_surface, true);
            wlr_xwayland_surface_restack(m_xwayland_surface, nullptr, XCB_STACK_MODE_ABOVE);
        }
        target_surface = m_xwayland_surface->surface;
    }

    if (m_foreign_toplevel) {
        wlr_foreign_toplevel_handle_v1_set_activated(m_foreign_toplevel, true);
    }
    m_server->set_focused_view(this);

    if (prev && prev != this) {
        prev->update_border();
    }
    update_border();

    if (target_surface) {
        struct wlr_seat* seat = m_server->get_input_manager()->get_seat();
        struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
        if (keyboard) {
            wlr_seat_keyboard_notify_enter(seat, target_surface,
                keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
        }
    }
}

void View::close() {
    if (m_type == ViewType::Xdg && m_xdg_toplevel) {
        wlr_xdg_toplevel_send_close(m_xdg_toplevel);
    } else if (m_type == ViewType::XWayland && m_xwayland_surface) {
        if (!is_override_redirect()) {
            wlr_xwayland_surface_close(m_xwayland_surface);
        }
    }
}

void View::handle_map(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_map_listener);
    view->m_mapped = true;
    view->update_parent_relationship();

    if (view->m_type == ViewType::XWayland) {
        if (view->m_xwayland_surface && view->m_xwayland_surface->surface) {
            view->m_surface_scene_tree = wlr_scene_subsurface_tree_create(view->m_scene_tree, view->m_xwayland_surface->surface);
            view->m_surface_scene_tree->node.data = view;
            view->m_xwayland_surface->surface->data = view->m_surface_scene_tree;
        }

        if (view->is_override_redirect()) {
            wlr_scene_node_set_position(&view->m_scene_tree->node, view->m_xwayland_surface->x, view->m_xwayland_surface->y);
            wlr_scene_node_raise_to_top(&view->m_scene_tree->node);
            if (wlr_xwayland_surface_override_redirect_wants_focus(view->m_xwayland_surface)) {
                struct wlr_seat* seat = view->m_server->get_input_manager()->get_seat();
                struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
                if (keyboard && view->m_xwayland_surface->surface) {
                    wlr_seat_keyboard_notify_enter(seat, view->m_xwayland_surface->surface,
                        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
                }
            }
            return;
        }
    }

    view->m_server->get_workspace_manager()->add_view_auto(view);
    view->focus();
}

void View::handle_unmap(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_unmap_listener);
    view->m_mapped = false;

    if (!view->is_override_redirect()) {
        view->m_server->get_workspace_manager()->remove_view(view);
    }

    if (view->m_type == ViewType::XWayland && view->m_surface_scene_tree) {
        wlr_scene_node_destroy(&view->m_surface_scene_tree->node);
        view->m_surface_scene_tree = nullptr;
    }

    if (view->is_override_redirect()) {
        View* focused = view->m_server->get_focused_view();
        if (focused && focused->is_mapped()) {
            struct wlr_surface* target_surface = nullptr;
            if (focused->get_type() == ViewType::Xdg && focused->get_xdg_toplevel()) {
                target_surface = focused->get_xdg_toplevel()->base->surface;
            } else if (focused->get_type() == ViewType::XWayland && focused->get_xwayland_surface()) {
                target_surface = focused->get_xwayland_surface()->surface;
            }
            if (target_surface) {
                struct wlr_seat* seat = view->m_server->get_input_manager()->get_seat();
                struct wlr_keyboard* keyboard = wlr_seat_get_keyboard(seat);
                if (keyboard) {
                    wlr_seat_keyboard_notify_enter(seat, target_surface,
                        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
                }
            }
        }
    }
}

void View::handle_destroy(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_destroy_listener);
    view->m_server->remove_view(view);
}

void View::handle_commit(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_commit_listener);
    if (view->m_type == ViewType::Xdg && view->m_xdg_toplevel->base->initial_commit) {
        wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, 0, 0);
    }
}

void View::handle_request_fullscreen(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_fullscreen_listener);
    if (view->m_type == ViewType::Xdg) {
        if (view->m_xdg_toplevel->base->surface->mapped) {
            wlr_xdg_toplevel_set_fullscreen(view->m_xdg_toplevel, false);
        }
    } else if (view->m_type == ViewType::XWayland && view->m_xwayland_surface) {
        if (!view->is_override_redirect()) {
            wlr_xwayland_surface_set_fullscreen(view->m_xwayland_surface, false);
        }
    }
}

void View::handle_request_maximize(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_maximize_listener);
    if (view->m_type == ViewType::Xdg) {
        if (view->m_xdg_toplevel->base->surface->mapped) {
            wlr_xdg_toplevel_set_maximized(view->m_xdg_toplevel, false);
        }
    } else if (view->m_type == ViewType::XWayland && view->m_xwayland_surface) {
        if (!view->is_override_redirect()) {
            wlr_xwayland_surface_set_maximized(view->m_xwayland_surface, false, false);
        }
    }
}

void View::handle_set_title(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_title_listener);
    std::string title = view->get_title();
    if (view->m_foreign_toplevel && !title.empty()) {
        wlr_foreign_toplevel_handle_v1_set_title(view->m_foreign_toplevel, title.c_str());
    }
}

void View::handle_set_app_id(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_app_id_listener);
    std::string app_id = view->get_app_id();
    if (view->m_foreign_toplevel && !app_id.empty()) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->m_foreign_toplevel, app_id.c_str());
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

void View::handle_new_popup(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_new_popup_listener);
    auto* popup = static_cast<struct wlr_xdg_popup*>(data);

    struct wlr_scene_tree* parent_tree = view->m_surface_scene_tree ? view->m_surface_scene_tree : view->get_scene_tree();
    if (!parent_tree) return;

    auto p = std::make_unique<Popup>(popup, parent_tree, view, [view](Popup* target) {
        for (auto it = view->m_popups.begin(); it != view->m_popups.end(); ++it) {
            if (it->get() == target) {
                view->m_popups.erase(it);
                break;
            }
        }
    });
    view->m_popups.push_back(std::move(p));
}

// XWayland Event Handlers
void View::handle_xwayland_associate(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_associate_listener);
    if (!view->m_xwayland_surface || !view->m_xwayland_surface->surface) return;

    view->m_map_listener.notify = handle_map;
    wl_signal_add(&view->m_xwayland_surface->surface->events.map, &view->m_map_listener);

    view->m_unmap_listener.notify = handle_unmap;
    wl_signal_add(&view->m_xwayland_surface->surface->events.unmap, &view->m_unmap_listener);
}

void View::handle_xwayland_dissociate(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_dissociate_listener);
    wl_list_remove(&view->m_map_listener.link);
    wl_list_remove(&view->m_unmap_listener.link);
}

void View::handle_xwayland_request_configure(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_configure_listener);
    auto* ev = static_cast<struct wlr_xwayland_surface_configure_event*>(data);

    if (view->m_mapped && !view->is_override_redirect()) {
        int bw = Config::get().get_window_border_width();
        int client_w = std::max(1, view->m_width - 2 * bw);
        int client_h = std::max(1, view->m_height - 2 * bw);
        wlr_xwayland_surface_configure(view->m_xwayland_surface, view->m_x + bw, view->m_y + bw, client_w, client_h);
    } else {
        view->m_x = ev->x;
        view->m_y = ev->y;
        view->m_width = ev->width;
        view->m_height = ev->height;
        if (view->m_scene_tree) {
            wlr_scene_node_set_position(&view->m_scene_tree->node, ev->x, ev->y);
        }
        wlr_xwayland_surface_configure(view->m_xwayland_surface, ev->x, ev->y, ev->width, ev->height);
    }
}

void View::handle_xwayland_request_activate(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_activate_listener);
    if (view->is_override_redirect()) return;
    view->focus();
}

void View::handle_xwayland_set_geometry(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_geometry_listener);
    if (view->is_override_redirect() && view->m_scene_tree && view->m_xwayland_surface) {
        view->m_x = view->m_xwayland_surface->x;
        view->m_y = view->m_xwayland_surface->y;
        view->m_width = view->m_xwayland_surface->width;
        view->m_height = view->m_xwayland_surface->height;
        wlr_scene_node_set_position(&view->m_scene_tree->node, view->m_x, view->m_y);
    }
}

void View::handle_xwayland_set_class(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_class_listener);
    std::string app_id = view->get_app_id();
    if (view->m_foreign_toplevel && !app_id.empty()) {
        wlr_foreign_toplevel_handle_v1_set_app_id(view->m_foreign_toplevel, app_id.c_str());
    }
}

void View::handle_set_parent(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_parent_listener);
    view->update_parent_relationship();
}

void View::handle_xwayland_set_parent(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_parent_listener);
    view->update_parent_relationship();
    // If needed, restack relative to parent
    if (view->m_xwayland_surface && view->m_xwayland_surface->parent) {
        if (!view->is_override_redirect() && !view->m_xwayland_surface->parent->override_redirect) {
            wlr_xwayland_surface_restack(view->m_xwayland_surface, view->m_xwayland_surface->parent, XCB_STACK_MODE_ABOVE);
        }
    }
}

void View::handle_xwayland_set_override_redirect(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_set_override_redirect_listener);
    if (!view->m_xwayland_surface) return;
    bool old_val = view->m_is_override_redirect;
    view->m_is_override_redirect = view->m_xwayland_surface->override_redirect;

    if (view->m_mapped && old_val != view->m_is_override_redirect) {
        if (view->m_is_override_redirect) {
            view->m_server->get_workspace_manager()->remove_view(view);
            if (view->m_border_scene_buffer) {
                wlr_scene_node_set_enabled(&view->m_border_scene_buffer->node, false);
            }
        } else {
            view->m_server->get_workspace_manager()->add_view_auto(view);
            if (view->m_border_scene_buffer) {
                wlr_scene_node_set_enabled(&view->m_border_scene_buffer->node, true);
            }
        }
    }
}

void View::set_parent_view(View* parent) {
    if (m_parent_view == parent) return;
    if (m_parent_view) {
        auto& children = m_parent_view->m_child_dialogs;
        children.erase(std::remove(children.begin(), children.end(), this), children.end());
    }
    m_parent_view = parent;
    m_is_dialog = (parent != nullptr);
    if (m_parent_view) {
        m_parent_view->m_child_dialogs.push_back(this);
        if (m_parent_view->get_workspace() && m_workspace != m_parent_view->get_workspace()) {
            if (m_workspace) {
                m_workspace->remove_view(this);
            }
            m_parent_view->get_workspace()->add_view(this);
        }
    }
}

void View::update_parent_relationship() {
    if (m_type == ViewType::Xdg && m_xdg_toplevel) {
        if (m_xdg_toplevel->parent) {
            for (const auto& v : m_server->get_views()) {
                if (v.get() != this && v->get_type() == ViewType::Xdg && v->get_xdg_toplevel() == m_xdg_toplevel->parent) {
                    set_parent_view(v.get());
                    return;
                }
            }
        }
        // Heuristic fallback for portal / dialog window app_ids
        std::string app = get_app_id();
        if (app == "xdg-desktop-portal-gtk" || app == "org.freedesktop.impl.portal.desktop.gtk" || app == "zenity") {
            if (!m_parent_view && m_server->get_focused_view() && m_server->get_focused_view() != this) {
                set_parent_view(m_server->get_focused_view());
                return;
            }
        }
    } else if (m_type == ViewType::XWayland && m_xwayland_surface) {
        if (m_xwayland_surface->parent) {
            for (const auto& v : m_server->get_views()) {
                if (v.get() != this && v->get_type() == ViewType::XWayland && v->get_xwayland_surface() == m_xwayland_surface->parent) {
                    set_parent_view(v.get());
                    return;
                }
            }
        }
    }
}

void View::update_child_dialog_geometries() {
    if (m_child_dialogs.empty() || m_width <= 0 || m_height <= 0) return;

    int bw = Config::get().get_window_border_width();

    for (auto* dialog : m_child_dialogs) {
        if (!dialog || !dialog->is_mapped()) continue;

        int req_w = dialog->get_width() > 0 ? dialog->get_width() : 750;
        int req_h = dialog->get_height() > 0 ? dialog->get_height() : 500;

        if (dialog->get_type() == ViewType::Xdg && dialog->get_xdg_toplevel()) {
            auto* xdg_surf = dialog->get_xdg_toplevel()->base;
            int gw = xdg_surf->current.geometry.width;
            int gh = xdg_surf->current.geometry.height;
            if (gw <= 0 && xdg_surf->surface) {
                gw = xdg_surf->surface->current.width;
                gh = xdg_surf->surface->current.height;
            }
            if (gw > 0) req_w = gw + 2 * bw;
            if (gh > 0) req_h = gh + 2 * bw;
        }

        int max_w = std::max(50, m_width - 20);
        int max_h = std::max(50, m_height - 20);

        int dw = std::min(req_w, max_w);
        int dh = std::min(req_h, max_h);

        int dx = m_x + (m_width - dw) / 2;
        int dy = m_y + (m_height - dh) / 2;

        dialog->set_geometry(dx, dy, dw, dh);
    }
}

bool View::has_child_dialogs() const {
    for (auto* d : m_child_dialogs) {
        if (d && d->is_mapped()) return true;
    }
    return false;
}

View* View::get_top_dialog() const {
    for (auto it = m_child_dialogs.rbegin(); it != m_child_dialogs.rend(); ++it) {
        if (*it && (*it)->is_mapped()) {
            return *it;
        }
    }
    return nullptr;
}

} // namespace biway
