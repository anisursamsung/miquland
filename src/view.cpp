#include "view.hpp"
#include "server.hpp"
#include "workspace.hpp"
#include "input.hpp"

namespace biway {

View::View(Server* server, struct wlr_xdg_toplevel* toplevel)
    : m_server(server), m_xdg_toplevel(toplevel)
{
    // Create initial scene tree node under the root scene
    m_scene_tree = wlr_scene_xdg_surface_create(&server->get_scene()->tree, toplevel->base);
    m_scene_tree->node.data = this;
    toplevel->base->data = m_scene_tree;

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
}

View::~View() {
    wl_list_remove(&m_map_listener.link);
    wl_list_remove(&m_unmap_listener.link);
    wl_list_remove(&m_destroy_listener.link);
    wl_list_remove(&m_commit_listener.link);
    wl_list_remove(&m_request_fullscreen_listener.link);
    wl_list_remove(&m_request_maximize_listener.link);
}

void View::set_workspace(Workspace* ws) {
    if (m_workspace == ws) return;
    m_workspace = ws;
    if (m_workspace && m_scene_tree) {
        wlr_scene_node_reparent(&m_scene_tree->node, m_workspace->get_scene_tree());
    }
}

void View::set_geometry(int x, int y, int width, int height) {
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;

    if (m_scene_tree) {
        wlr_scene_node_set_position(&m_scene_tree->node, x, y);
    }
    if (m_xdg_toplevel) {
        wlr_xdg_toplevel_set_size(m_xdg_toplevel, width, height);
    }
}

void View::focus() {
    if (!m_mapped || !m_xdg_toplevel) return;

    // Deactivate currently focused view
    View* prev = m_server->get_focused_view();
    if (prev && prev != this && prev->get_xdg_toplevel()) {
        wlr_xdg_toplevel_set_activated(prev->get_xdg_toplevel(), false);
    }

    if (m_scene_tree) {
        wlr_scene_node_raise_to_top(&m_scene_tree->node);
    }
    wlr_xdg_toplevel_set_activated(m_xdg_toplevel, true);
    m_server->set_focused_view(this);

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
        wlr_xdg_toplevel_set_size(view->m_xdg_toplevel, view->m_width, view->m_height);
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

} // namespace biway
