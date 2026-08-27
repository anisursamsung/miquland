#pragma once

#include "core/common/util.hpp"
#include <vector>
#include <memory>
#include <string>

namespace biway {

class OutputManager;
class WorkspaceManager;
class InputManager;
class Wallpaper;
class Bar;
class Menu;
class View;
class LayerSurface;

class Server {
public:
    Server();
    ~Server();

    bool init();
    void run();
    void terminate();

    void set_startup_command(const std::string& cmd) { m_startup_cmd = cmd; }

    struct wl_display* get_display() const { return m_wl_display; }
    struct wl_event_loop* get_event_loop() const { return m_wl_event_loop; }
    struct wlr_backend* get_backend() const { return m_wlr_backend; }
    struct wlr_renderer* get_renderer() const { return m_wlr_renderer; }
    struct wlr_allocator* get_allocator() const { return m_wlr_allocator; }
    struct wlr_scene* get_scene() const { return m_scene; }

    struct wlr_scene_tree* get_bg_tree() const { return m_bg_tree; }
    struct wlr_scene_tree* get_workspaces_tree() const { return m_workspaces_tree; }
    struct wlr_scene_tree* get_bar_tree() const { return m_bar_tree; }

    struct wlr_scene_tree* get_layer_tree(enum zwlr_layer_shell_v1_layer layer) const;
    struct wlr_scene_tree* get_layer_background_tree() const { return m_layer_background_tree; }
    struct wlr_scene_tree* get_layer_bottom_tree() const { return m_layer_bottom_tree; }
    struct wlr_scene_tree* get_layer_top_tree() const { return m_layer_top_tree; }
    struct wlr_scene_tree* get_layer_overlay_tree() const { return m_layer_overlay_tree; }

    OutputManager* get_output_manager() const { return m_output_manager.get(); }
    WorkspaceManager* get_workspace_manager() const { return m_workspace_manager.get(); }
    InputManager* get_input_manager() const { return m_input_manager.get(); }
    Wallpaper* get_wallpaper() const { return m_wallpaper.get(); }
    Bar* get_bar() const { return m_bar.get(); }
    Menu* get_menu() const { return m_menu.get(); }

    void add_view(std::unique_ptr<View> view);
    void remove_view(View* view);
    View* view_at(double lx, double ly, struct wlr_surface** surface, double* sx, double* sy);

    void add_layer_surface(std::unique_ptr<LayerSurface> surface);
    void remove_layer_surface(LayerSurface* surface);
    void arrange_layers(struct wlr_output* output);
    void focus_layer_surface(LayerSurface* surface);
    LayerSurface* get_focused_layer_surface() const { return m_focused_layer_surface; }
    const std::vector<std::unique_ptr<LayerSurface>>& get_layer_surfaces() const { return m_layer_surfaces; }
    struct wlr_foreign_toplevel_manager_v1* get_foreign_toplevel_manager() const { return m_foreign_toplevel_manager; }

    void set_focused_view(View* view);
    View* get_focused_view() const { return m_focused_view; }

    void reload_config();

private:
    static void handle_new_xdg_toplevel(struct wl_listener* listener, void* data);
    static void handle_new_layer_shell_surface(struct wl_listener* listener, void* data);
    static int handle_config_inotify(int fd, uint32_t mask, void* data);
    void setup_config_watcher();

    struct wl_display* m_wl_display = nullptr;
    struct wl_event_loop* m_wl_event_loop = nullptr;
    struct wlr_backend* m_wlr_backend = nullptr;
    struct wlr_renderer* m_wlr_renderer = nullptr;
    struct wlr_allocator* m_wlr_allocator = nullptr;
    struct wlr_compositor* m_wlr_compositor = nullptr;
    struct wlr_subcompositor* m_wlr_subcompositor = nullptr;
    struct wlr_data_device_manager* m_wlr_data_device_manager = nullptr;
    struct wlr_scene* m_scene = nullptr;

    struct wlr_scene_tree* m_layer_background_tree = nullptr;
    struct wlr_scene_tree* m_bg_tree = nullptr;
    struct wlr_scene_tree* m_layer_bottom_tree = nullptr;
    struct wlr_scene_tree* m_workspaces_tree = nullptr;
    struct wlr_scene_tree* m_layer_top_tree = nullptr;
    struct wlr_scene_tree* m_bar_tree = nullptr;
    struct wlr_scene_tree* m_layer_overlay_tree = nullptr;

    struct wlr_xdg_shell* m_xdg_shell = nullptr;
    struct wlr_layer_shell_v1* m_layer_shell = nullptr;
    struct wlr_foreign_toplevel_manager_v1* m_foreign_toplevel_manager = nullptr;

    const char* m_socket_name = nullptr;
    std::string m_startup_cmd;

    int m_inotify_fd = -1;
    int m_inotify_wd = -1;
    struct wl_event_source* m_config_event_source = nullptr;

    std::unique_ptr<OutputManager> m_output_manager;
    std::unique_ptr<WorkspaceManager> m_workspace_manager;
    std::unique_ptr<InputManager> m_input_manager;
    std::unique_ptr<Wallpaper> m_wallpaper;
    std::unique_ptr<Bar> m_bar;
    std::unique_ptr<Menu> m_menu;

    std::vector<std::unique_ptr<View>> m_views;
    View* m_focused_view = nullptr;

    std::vector<std::unique_ptr<LayerSurface>> m_layer_surfaces;
    LayerSurface* m_focused_layer_surface = nullptr;

    struct wl_listener m_new_xdg_toplevel_listener;
    struct wl_listener m_new_layer_shell_surface_listener;
};

} // namespace biway
