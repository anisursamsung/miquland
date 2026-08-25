#include "server.hpp"
#include "output.hpp"
#include "workspace.hpp"
#include "input.hpp"
#include "wallpaper.hpp"
#include "bar.hpp"
#include "view.hpp"
#include <algorithm>

namespace biway {

Server::Server() = default;

Server::~Server() {
    m_views.clear();
    m_bar.reset();
    m_wallpaper.reset();
    m_input_manager.reset();
    m_workspace_manager.reset();
    m_output_manager.reset();

    if (m_xdg_shell) {
        wl_list_remove(&m_new_xdg_toplevel_listener.link);
    }
    if (m_wlr_allocator) wlr_allocator_destroy(m_wlr_allocator);
    if (m_wlr_renderer) wlr_renderer_destroy(m_wlr_renderer);
    if (m_wlr_backend) wlr_backend_destroy(m_wlr_backend);
    if (m_wl_display) wl_display_destroy(m_wl_display);
}

bool Server::init() {
    wlr_log_init(WLR_DEBUG, nullptr);

    m_wl_display = wl_display_create();
    if (!m_wl_display) {
        log_error("Failed to create Wayland display");
        return false;
    }

    m_wl_event_loop = wl_display_get_event_loop(m_wl_display);

    m_wlr_backend = wlr_backend_autocreate(m_wl_event_loop, nullptr);
    if (!m_wlr_backend) {
        log_error("Failed to create wlr_backend");
        return false;
    }

    m_wlr_renderer = wlr_renderer_autocreate(m_wlr_backend);
    if (!m_wlr_renderer) {
        log_error("Failed to create wlr_renderer");
        return false;
    }

    wlr_renderer_init_wl_display(m_wlr_renderer, m_wl_display);

    m_wlr_allocator = wlr_allocator_autocreate(m_wlr_backend, m_wlr_renderer);
    if (!m_wlr_allocator) {
        log_error("Failed to create wlr_allocator");
        return false;
    }

    m_wlr_compositor = wlr_compositor_create(m_wl_display, 6, m_wlr_renderer);
    m_wlr_subcompositor = wlr_subcompositor_create(m_wl_display);
    m_wlr_data_device_manager = wlr_data_device_manager_create(m_wl_display);

    m_scene = wlr_scene_create();
    if (!m_scene) {
        log_error("Failed to create wlr_scene");
        return false;
    }

    // Layered scene tree hierarchy
    m_bg_tree = wlr_scene_tree_create(&m_scene->tree);
    m_workspaces_tree = wlr_scene_tree_create(&m_scene->tree);
    m_bar_tree = wlr_scene_tree_create(&m_scene->tree);

    m_output_manager = std::make_unique<OutputManager>(this);
    m_workspace_manager = std::make_unique<WorkspaceManager>(this);
    m_input_manager = std::make_unique<InputManager>(this);
    m_wallpaper = std::make_unique<Wallpaper>(this);
    m_bar = std::make_unique<Bar>(this);

    m_xdg_shell = wlr_xdg_shell_create(m_wl_display, 3);
    m_new_xdg_toplevel_listener.notify = handle_new_xdg_toplevel;
    wl_signal_add(&m_xdg_shell->events.new_toplevel, &m_new_xdg_toplevel_listener);

    m_socket_name = wl_display_add_socket_auto(m_wl_display);
    if (!m_socket_name) {
        log_error("Failed to add Wayland socket");
        return false;
    }

    setenv("WAYLAND_DISPLAY", m_socket_name, 1);
    log_info("biway Wayland compositor started on WAYLAND_DISPLAY=" + std::string(m_socket_name));

    return true;
}

void Server::run() {
    if (!wlr_backend_start(m_wlr_backend)) {
        log_error("Failed to start wlr_backend");
        return;
    }

    if (!m_startup_cmd.empty()) {
        m_input_manager->spawn_command(m_startup_cmd.c_str());
    }

    log_info("Running Wayland event loop");
    wl_display_run(m_wl_display);
}

void Server::terminate() {
    log_info("Terminating biway compositor");
    if (m_wl_display) {
        wl_display_terminate(m_wl_display);
    }
}

void Server::add_view(std::unique_ptr<View> view) {
    m_views.push_back(std::move(view));
    if (m_bar) m_bar->schedule_redraw();
}

void Server::remove_view(View* view) {
    if (m_focused_view == view) {
        m_focused_view = nullptr;
    }

    for (auto it = m_views.begin(); it != m_views.end(); ++it) {
        if (it->get() == view) {
            m_views.erase(it);
            break;
        }
    }
    if (m_bar) m_bar->schedule_redraw();
}

void Server::set_focused_view(View* view) {
    m_focused_view = view;
    if (m_bar) m_bar->schedule_redraw();
}

View* Server::view_at(double lx, double ly, struct wlr_surface** surface, double* sx, double* sy) {
    struct wlr_scene_node* node = wlr_scene_node_at(&m_scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
        return nullptr;
    }

    auto* scene_buffer = wlr_scene_buffer_from_node(node);
    auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface) {
        return nullptr;
    }

    *surface = scene_surface->surface;

    // Find the View associated with this scene node hierarchy
    struct wlr_scene_tree* tree = node->parent;
    while (tree != nullptr) {
        if (tree->node.data != nullptr) {
            return static_cast<View*>(tree->node.data);
        }
        tree = tree->node.parent;
    }

    return nullptr;
}

void Server::handle_new_xdg_toplevel(struct wl_listener* listener, void* data) {
    Server* server = wl_container_of(listener, server, m_new_xdg_toplevel_listener);
    auto* toplevel = static_cast<struct wlr_xdg_toplevel*>(data);

    auto view = std::make_unique<View>(server, toplevel);
    server->add_view(std::move(view));
}

} // namespace biway
