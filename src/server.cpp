#include "server.hpp"
#include "output.hpp"
#include "workspace.hpp"
#include "input.hpp"
#include "wallpaper.hpp"
#include "bar.hpp"
#include "menu.hpp"
#include "view.hpp"
#include "config.hpp"
#include <algorithm>
#include <sys/inotify.h>
#include <unistd.h>

namespace biway {

Server::Server() = default;

Server::~Server() {
    if (m_config_event_source) {
        wl_event_source_remove(m_config_event_source);
        m_config_event_source = nullptr;
    }
    if (m_inotify_fd >= 0) {
        if (m_inotify_wd >= 0) {
            inotify_rm_watch(m_inotify_fd, m_inotify_wd);
        }
        close(m_inotify_fd);
        m_inotify_fd = -1;
    }

    m_views.clear();
    m_menu.reset();
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
    m_menu = std::make_unique<Menu>(this);

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

    setup_config_watcher();

    return true;
}

void Server::setup_config_watcher() {
    std::string config_dir = Config::get_config_dir_path();
    std::error_code ec;
    std::filesystem::create_directories(config_dir, ec);

    m_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotify_fd < 0) {
        log_error("Failed to initialize inotify for configuration auto-reload");
        return;
    }

    m_inotify_wd = inotify_add_watch(m_inotify_fd, config_dir.c_str(),
                                     IN_MODIFY | IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (m_inotify_wd < 0) {
        log_error("Failed to add inotify watch on: " + config_dir);
        close(m_inotify_fd);
        m_inotify_fd = -1;
        return;
    }

    m_config_event_source = wl_event_loop_add_fd(m_wl_event_loop, m_inotify_fd,
                                                 WL_EVENT_READABLE, handle_config_inotify, this);
    log_info("Live config auto-reload active on " + config_dir);
}

int Server::handle_config_inotify(int fd, uint32_t mask, void* data) {
    auto* server = static_cast<Server*>(data);

    char buffer[4096];
    bool should_reload = false;
    while (read(fd, buffer, sizeof(buffer)) > 0) {
        should_reload = true;
    }

    if (should_reload) {
        server->reload_config();
    }
    return 0;
}

void Server::reload_config() {
    log_info("Configuration file change detected! Auto-reloading settings...");
    Config::get().load();

    // 1. Live reload wallpaper
    if (m_output_manager && m_wallpaper) {
        struct wlr_box box = m_output_manager->get_primary_geometry();
        if (box.width > 0 && box.height > 0) {
            m_wallpaper->render(box.width, box.height);
        }
    }

    // 2. Live reload bar
    if (m_bar) {
        m_bar->set_visible(Config::get().is_bar_visible());
        m_bar->schedule_redraw();
    }

    // 3. Live reload menu applications & icon theme
    if (m_menu) {
        m_menu->reload_applications();
    }

    // 4. Recalculate layout and borders
    if (m_workspace_manager) {
        m_workspace_manager->recalculate_layout();
    }
    for (auto& view : m_views) {
        view->update_border();
    }

    // 5. Live reload input device settings (natural scroll)
    if (m_input_manager) {
        m_input_manager->reapply_device_config();
    }

    log_info("Live configuration auto-reload complete!");
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
    if (m_focused_view == view) return;
    View* prev = m_focused_view;
    m_focused_view = view;
    if (prev) prev->update_border();
    if (m_focused_view) m_focused_view->update_border();
    if (m_bar) m_bar->schedule_redraw();
}

View* Server::view_at(double lx, double ly, struct wlr_surface** surface, double* sx, double* sy) {
    struct wlr_scene_node* node = wlr_scene_node_at(&m_scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
        return nullptr;
    }

    auto* scene_buffer = wlr_scene_buffer_from_node(node);
    auto* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    if (scene_surface) {
        *surface = scene_surface->surface;
    } else {
        *surface = nullptr;
    }

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
