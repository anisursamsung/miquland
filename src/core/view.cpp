#include "core/view.hpp"
#include "core/popup.hpp"
#include "core/server.hpp"
#include "core/output.hpp"
#include "core/workspace.hpp"
#include "core/input/input.hpp"
#include "core/config/config.hpp"
#include <cmath>
#include <algorithm>

namespace miquland {

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
    wlr_scene_node_set_enabled(&m_scene_tree->node, false);

    // Create xdg surface scene tree under view container tree (Child 1 - bottom layer)
    m_surface_scene_tree = wlr_scene_xdg_surface_create(m_scene_tree, toplevel->base);
    m_surface_scene_tree->node.data = this;
    toplevel->base->data = m_surface_scene_tree;

    // Create border scene buffer on top of xdg surface (Child 2 - top overlay layer)
    m_border_scene_buffer = wlr_scene_buffer_create(m_scene_tree, nullptr);
    m_border_scene_buffer->point_accepts_input = [](struct wlr_scene_buffer* buffer, double* sx, double* sy) -> bool {
        if (!Config::get().is_resize_on_border_enabled()) {
            return false;
        }
        if (!buffer || !buffer->node.parent || !buffer->node.parent->node.data) {
            return false;
        }
        auto* view = static_cast<View*>(buffer->node.parent->node.data);
        if (!view || view->is_fullscreen()) {
            return false;
        }
        int grab = std::max(Config::get().get_window_border_width(), Config::get().get_border_grab_area());
        int w = view->get_width();
        int h = view->get_height();
        if (w <= 0 || h <= 0) return false;
        if (*sx < grab || *sx >= w - grab || *sy < grab || *sy >= h - grab) {
            return true;
        }
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

    m_request_move_listener.notify = handle_request_move;
    wl_signal_add(&toplevel->events.request_move, &m_request_move_listener);

    m_request_resize_listener.notify = handle_request_resize;
    wl_signal_add(&toplevel->events.request_resize, &m_request_resize_listener);

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
        wlr_scene_node_set_enabled(&m_scene_tree->node, false);
        // Create border scene buffer on top of surface
        m_border_scene_buffer = wlr_scene_buffer_create(m_scene_tree, nullptr);
        m_border_scene_buffer->point_accepts_input = [](struct wlr_scene_buffer* buffer, double* sx, double* sy) -> bool {
            if (!Config::get().is_resize_on_border_enabled()) {
                return false;
            }
            if (!buffer || !buffer->node.parent || !buffer->node.parent->node.data) {
                return false;
            }
            auto* view = static_cast<View*>(buffer->node.parent->node.data);
            if (!view || view->is_fullscreen()) {
                return false;
            }
            int grab = std::max(Config::get().get_window_border_width(), Config::get().get_border_grab_area());
            int w = view->get_width();
            int h = view->get_height();
            if (w <= 0 || h <= 0) return false;
            if (*sx < grab || *sx >= w - grab || *sy < grab || *sy >= h - grab) {
                return true;
            }
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

    m_xwayland_request_move_listener.notify = handle_xwayland_request_move;
    wl_signal_add(&xsurface->events.request_move, &m_xwayland_request_move_listener);

    m_xwayland_request_resize_listener.notify = handle_xwayland_request_resize;
    wl_signal_add(&xsurface->events.request_resize, &m_xwayland_request_resize_listener);

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
    if (m_server && m_server->get_input_manager()) {
        m_server->get_input_manager()->notify_view_destroyed(this);
    }

    if (m_workspace) {
        m_workspace->remove_view(this);
        m_workspace = nullptr;
    }

    if (m_type == ViewType::Xdg) {
        wl_list_remove(&m_map_listener.link);
        wl_list_remove(&m_unmap_listener.link);
        wl_list_remove(&m_destroy_listener.link);
        wl_list_remove(&m_commit_listener.link);
        wl_list_remove(&m_request_fullscreen_listener.link);
        wl_list_remove(&m_request_maximize_listener.link);
        wl_list_remove(&m_request_move_listener.link);
        wl_list_remove(&m_request_resize_listener.link);
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
        wl_list_remove(&m_xwayland_request_move_listener.link);
        wl_list_remove(&m_xwayland_request_resize_listener.link);
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

            if (m_server->get_output_manager()) {
                auto* out = m_server->get_output_manager()->get_primary_output();
                if (out && out->get_wlr_output()) {
                    wlr_foreign_toplevel_handle_v1_output_enter(m_foreign_toplevel, out->get_wlr_output());
                }
            }

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
        if (m_xdg_toplevel) {
            if (!is_dialog() && !is_floating()) {
                wlr_xdg_toplevel_set_tiled(m_xdg_toplevel, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
            } else {
                wlr_xdg_toplevel_set_tiled(m_xdg_toplevel, 0);
            }
            if (m_xdg_toplevel->resource && wl_resource_get_version(m_xdg_toplevel->resource) >= 4) {
                wlr_xdg_toplevel_set_bounds(m_xdg_toplevel, client_w, client_h);
            }
            if (size_changed) {
                wlr_xdg_toplevel_set_size(m_xdg_toplevel, client_w, client_h);
            }
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
    update_corner_radius();
    update_child_dialog_geometries();
}

void View::set_fullscreen(bool fullscreen) {
    if (m_is_fullscreen == fullscreen) return;

    auto* output_mgr = m_server->get_output_manager();
    if (!output_mgr) return;
    auto* primary_out = output_mgr->get_primary_output();
    if (!primary_out || !primary_out->get_wlr_output()) return;

    if (fullscreen) {
        // --- Enter fullscreen ---
        m_saved_geometry.x = m_x;
        m_saved_geometry.y = m_y;
        m_saved_geometry.w = m_width;
        m_saved_geometry.h = m_height;
        m_saved_geometry.workspace_tree = m_workspace ? m_workspace->get_scene_tree() : nullptr;

        // Reparent to the overlay scene tree so it sits above everything
        if (m_scene_tree) {
            wlr_scene_node_reparent(&m_scene_tree->node, m_server->get_layer_overlay_tree());
            wlr_scene_node_raise_to_top(&m_scene_tree->node);
        }

        // Cover the entire output (no border offset)
        struct wlr_box output_box = {};
        wlr_output_effective_resolution(primary_out->get_wlr_output(), &output_box.width, &output_box.height);
        // Account for output layout position
        struct wlr_box layout_box = output_mgr->get_primary_geometry();
        output_box.x = layout_box.x;
        output_box.y = layout_box.y;

        m_x = output_box.x;
        m_y = output_box.y;
        m_width = output_box.width;
        m_height = output_box.height;

        if (m_scene_tree) {
            wlr_scene_node_set_position(&m_scene_tree->node, m_x, m_y);
        }

        if (m_type == ViewType::Xdg && m_xdg_toplevel) {
            if (m_surface_scene_tree) {
                wlr_scene_node_set_position(&m_surface_scene_tree->node, 0, 0);
            }
            wlr_xdg_toplevel_set_fullscreen(m_xdg_toplevel, true);
            wlr_xdg_toplevel_set_size(m_xdg_toplevel, m_width, m_height);
        } else if (m_type == ViewType::XWayland && m_xwayland_surface) {
            if (m_surface_scene_tree) {
                wlr_scene_node_set_position(&m_surface_scene_tree->node, 0, 0);
            }
            wlr_xwayland_surface_set_fullscreen(m_xwayland_surface, true);
            wlr_xwayland_surface_configure(m_xwayland_surface, m_x, m_y, m_width, m_height);
        }

        // Disable border in fullscreen
        if (m_border_scene_buffer) {
            wlr_scene_node_set_enabled(&m_border_scene_buffer->node, false);
        }

        m_is_fullscreen = true;
        update_corner_radius();
        focus();

    } else {
        // --- Exit fullscreen ---
        m_is_fullscreen = false;

        // Reparent back to workspace scene tree
        struct wlr_scene_tree* parent_tree = m_saved_geometry.workspace_tree;
        if (!parent_tree && m_workspace) {
            parent_tree = m_workspace->get_scene_tree();
        }
        if (parent_tree && m_scene_tree) {
            wlr_scene_node_reparent(&m_scene_tree->node, parent_tree);
        }

        if (m_type == ViewType::Xdg && m_xdg_toplevel) {
            if (m_surface_scene_tree) {
                int bw = Config::get().get_window_border_width();
                wlr_scene_node_set_position(&m_surface_scene_tree->node, bw, bw);
            }
            wlr_xdg_toplevel_set_fullscreen(m_xdg_toplevel, false);
        } else if (m_type == ViewType::XWayland && m_xwayland_surface) {
            if (m_surface_scene_tree) {
                int bw = Config::get().get_window_border_width();
                wlr_scene_node_set_position(&m_surface_scene_tree->node, bw, bw);
            }
            wlr_xwayland_surface_set_fullscreen(m_xwayland_surface, false);
        }

        // Re-enable border
        if (m_border_scene_buffer) {
            wlr_scene_node_set_enabled(&m_border_scene_buffer->node, true);
        }

        update_corner_radius();

        if (m_is_floating) {
            set_geometry(m_saved_geometry.x, m_saved_geometry.y, m_saved_geometry.w, m_saved_geometry.h);
        } else {
            // Trigger workspace relayout to reassign geometry
            m_server->get_workspace_manager()->recalculate_layout();
        }
    }
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
    update_corner_radius();
}

void View::update_opacity() {
    if (!m_mapped || !m_surface_scene_tree) return;

    float opacity = is_focused()
        ? Config::get().get_window_opacity_active()
        : Config::get().get_window_opacity_inactive();

    opacity = std::clamp(opacity, 0.0f, 1.0f);

    wlr_scene_node_for_each_buffer(&m_surface_scene_tree->node, [](struct wlr_scene_buffer* buffer, int sx, int sy, void* data) {
        float val = *static_cast<float*>(data);
        wlr_scene_buffer_set_opacity(buffer, val);
    }, &opacity);
}

void View::update_corner_radius() {
    if (!m_mapped || !m_surface_scene_tree) return;

    int bw = Config::get().get_window_border_width();
    int client_w = std::max(1, m_width - 2 * bw);
    int client_h = std::max(1, m_height - 2 * bw);

    // Follow Sway / dwl standard wlroots clipping for window geometry & CSD
    struct wlr_box clip = {
        .x = 0,
        .y = 0,
        .width = client_w,
        .height = client_h,
    };

    if (m_type == ViewType::Xdg && m_xdg_toplevel && m_xdg_toplevel->base) {
        auto* xdg_surf = m_xdg_toplevel->base;
        clip.x = xdg_surf->current.geometry.x;
        clip.y = xdg_surf->current.geometry.y;
    }

    wlr_scene_subsurface_tree_set_clip(&m_surface_scene_tree->node, &clip);

    int radius = 0;
    if (!m_is_fullscreen && !m_is_override_redirect) {
        int r = Config::get().get_window_border_radius();
        radius = (bw > 0) ? std::max(0, r - bw) : std::max(0, r);
    }

    wlr_scene_node_for_each_buffer(&m_surface_scene_tree->node, [](struct wlr_scene_buffer* buffer, int sx, int sy, void* user_data) {
        int r = *static_cast<int*>(user_data);
        wlr_scene_buffer_set_corner_radius(buffer, r);
    }, &radius);
}

void View::focus() {
    if (!m_mapped) return;

    if (m_workspace && m_server->get_workspace_manager()) {
        if (m_workspace->get_id() != m_server->get_workspace_manager()->get_active_workspace_id()) {
            m_server->get_workspace_manager()->switch_to_workspace(m_workspace->get_id(), this);
            return;
        }
    }

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
        prev->update_opacity();
    }
    update_border();
    update_opacity();

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

    if (view->m_foreign_toplevel) {
        std::string title = view->get_title();
        std::string app_id = view->get_app_id();
        if (!title.empty()) wlr_foreign_toplevel_handle_v1_set_title(view->m_foreign_toplevel, title.c_str());
        if (!app_id.empty()) wlr_foreign_toplevel_handle_v1_set_app_id(view->m_foreign_toplevel, app_id.c_str());
        if (view->m_server->get_output_manager()) {
            auto* out = view->m_server->get_output_manager()->get_primary_output();
            if (out && out->get_wlr_output()) {
                wlr_foreign_toplevel_handle_v1_output_enter(view->m_foreign_toplevel, out->get_wlr_output());
            }
        }
    }

    if (view->m_type == ViewType::XWayland) {
        if (view->m_xwayland_surface && view->m_xwayland_surface->surface) {
            view->m_surface_scene_tree = wlr_scene_subsurface_tree_create(view->m_scene_tree, view->m_xwayland_surface->surface);
            view->m_surface_scene_tree->node.data = view;
            view->m_xwayland_surface->surface->data = view->m_surface_scene_tree;
        }

        if (view->is_override_redirect()) {
            wlr_scene_node_set_position(&view->m_scene_tree->node, view->m_xwayland_surface->x, view->m_xwayland_surface->y);
            wlr_scene_node_raise_to_top(&view->m_scene_tree->node);
            wlr_scene_node_set_enabled(&view->m_scene_tree->node, true);
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
    if (view->m_workspace && view->m_workspace->is_visible()) {
        wlr_scene_node_set_enabled(&view->m_scene_tree->node, true);
    }
    view->update_opacity();
    view->update_corner_radius();
    view->focus();
}

void View::handle_unmap(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_unmap_listener);

    // If this view is fullscreen, restore scene tree parent before normal teardown
    if (view->m_is_fullscreen) {
        view->m_is_fullscreen = false;
        struct wlr_scene_tree* parent_tree = view->m_saved_geometry.workspace_tree;
        if (!parent_tree && view->m_workspace) {
            parent_tree = view->m_workspace->get_scene_tree();
        }
        if (parent_tree && view->m_scene_tree) {
            wlr_scene_node_reparent(&view->m_scene_tree->node, parent_tree);
        }
    }

    view->m_mapped = false;

    if (view->m_scene_tree) {
        wlr_scene_node_set_enabled(&view->m_scene_tree->node, false);
    }

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
    if (view->m_type == ViewType::Xdg) {
        if (view->m_xdg_toplevel->base->initial_commit) {
            view->update_parent_relationship();
            if (view->is_dialog()) {
                wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, 0, 0);
            } else {
                auto* ws_mgr = view->m_server->get_workspace_manager();
                auto* ws = ws_mgr ? ws_mgr->get_active_workspace() : nullptr;
                if (ws && view->m_server->get_output_manager()) {
                    struct wlr_box usable = view->m_server->get_output_manager()->get_primary_usable_geometry();
                    if (usable.width <= 0 || usable.height <= 0) {
                        usable = { .x = 0, .y = 0, .width = 1920, .height = 1080 };
                    }
                    struct wlr_box next_box = ws->calculate_tiled_geometry_for_new_view(usable);
                    int bw = Config::get().get_window_border_width();
                    int client_w = std::max(1, next_box.width - 2 * bw);
                    int client_h = std::max(1, next_box.height - 2 * bw);

                    if (view->m_scene_tree) {
                        wlr_scene_node_set_position(&view->m_scene_tree->node, next_box.x, next_box.y);
                    }
                    if (view->m_surface_scene_tree) {
                        wlr_scene_node_set_position(&view->m_surface_scene_tree->node, bw, bw);
                    }
                    wlr_xdg_toplevel_set_tiled(view->m_xdg_toplevel, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
                    if (view->m_xdg_toplevel->resource && wl_resource_get_version(view->m_xdg_toplevel->resource) >= 4) {
                        wlr_xdg_toplevel_set_bounds(view->m_xdg_toplevel, client_w, client_h);
                    }
                    wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, client_w, client_h);
                } else {
                    wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, 0, 0);
                }
            }
            return;
        }

        if (view->m_mapped && view->is_dialog()) {
            auto* xdg_surf = view->m_xdg_toplevel->base;
            int gw = xdg_surf->current.geometry.width;
            int gh = xdg_surf->current.geometry.height;
            if (gw <= 0 && xdg_surf->surface) {
                gw = xdg_surf->surface->current.width;
                gh = xdg_surf->surface->current.height;
            }
            int bw = Config::get().get_window_border_width();
            if (gw > 0 && gh > 0 && (gw + 2 * bw != view->m_width || gh + 2 * bw != view->m_height)) {
                if (view->m_workspace) {
                    view->m_server->get_workspace_manager()->recalculate_layout();
                }
            }
        }
    }

    if (view->m_mapped) {
        view->update_opacity();
        view->update_corner_radius();
    }
}

void View::handle_request_fullscreen(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_fullscreen_listener);
    if (view->m_type == ViewType::Xdg) {
        if (view->m_xdg_toplevel && view->m_xdg_toplevel->base->surface->mapped) {
            bool requested = view->m_xdg_toplevel->requested.fullscreen;
            view->set_fullscreen(requested);
        }
    } else if (view->m_type == ViewType::XWayland && view->m_xwayland_surface) {
        if (!view->is_override_redirect()) {
            bool requested = view->m_xwayland_surface->fullscreen;
            view->set_fullscreen(requested);
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

void View::handle_request_move(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_move_listener);
    if (!view || !view->m_mapped || view->m_is_fullscreen) return;
    if (view->m_server && view->m_server->get_input_manager()) {
        view->m_server->get_input_manager()->begin_interactive(view, CursorMode::Move, 0);
    }
}

void View::handle_request_resize(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_request_resize_listener);
    auto* event = static_cast<struct wlr_xdg_toplevel_resize_event*>(data);
    if (!view || !view->m_mapped || view->m_is_fullscreen) return;
    if (view->m_server && view->m_server->get_input_manager()) {
        view->m_server->get_input_manager()->begin_interactive(view, CursorMode::Resize, event ? event->edges : 0);
    }
}

void View::handle_xwayland_request_move(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_xwayland_request_move_listener);
    if (!view || !view->m_mapped || view->m_is_fullscreen) return;
    if (view->m_server && view->m_server->get_input_manager()) {
        view->m_server->get_input_manager()->begin_interactive(view, CursorMode::Move, 0);
    }
}

void View::handle_xwayland_request_resize(struct wl_listener* listener, void* data) {
    View* view = wl_container_of(listener, view, m_xwayland_request_resize_listener);
    auto* event = static_cast<struct wlr_xwayland_resize_event*>(data);
    if (!view || !view->m_mapped || view->m_is_fullscreen) return;
    if (view->m_server && view->m_server->get_input_manager()) {
        view->m_server->get_input_manager()->begin_interactive(view, CursorMode::Resize, event ? event->edges : 0);
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
    if (view->m_workspace && view->m_server->get_workspace_manager()) {
        view->m_server->get_workspace_manager()->switch_to_workspace(view->m_workspace->get_id(), view);
    } else {
        view->focus();
    }
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
        Workspace* target_ws = m_parent_view->get_workspace();
        if (target_ws && m_workspace != target_ws) {
            if (m_workspace) {
                m_workspace->remove_view(this);
            }
            target_ws->add_view(this);
        } else if (m_workspace) {
            // View was already on this workspace (likely added to m_tiled_views).
            // Now that it has a parent and is a dialog, move it to m_floating_views!
            m_workspace->remove_view(this);
            m_workspace->add_view(this);
            auto* out_mgr = m_server->get_output_manager();
            if (out_mgr) {
                m_workspace->recalculate_layout(out_mgr->get_primary_usable_geometry());
            }
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

} // namespace miquland
