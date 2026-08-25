#pragma once

#include "common/util.hpp"
#include "render/cairo_buffer.hpp"

namespace biway {

class Server;
class Workspace;

class View {
public:
    View(Server* server, struct wlr_xdg_toplevel* toplevel);
    ~View();

    void set_workspace(Workspace* ws);
    Workspace* get_workspace() const { return m_workspace; }

    void set_geometry(int x, int y, int width, int height);
    void focus();
    void close();

    void update_border();
    bool is_focused() const;

    struct wlr_xdg_toplevel* get_xdg_toplevel() const { return m_xdg_toplevel; }
    struct wlr_scene_tree* get_scene_tree() const { return m_scene_tree; }
    bool is_mapped() const { return m_mapped; }

    int get_x() const { return m_x; }
    int get_y() const { return m_y; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

private:
    static void handle_map(struct wl_listener* listener, void* data);
    static void handle_unmap(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);
    static void handle_commit(struct wl_listener* listener, void* data);
    static void handle_request_fullscreen(struct wl_listener* listener, void* data);
    static void handle_request_maximize(struct wl_listener* listener, void* data);

    Server* m_server = nullptr;
    Workspace* m_workspace = nullptr;
    struct wlr_xdg_toplevel* m_xdg_toplevel = nullptr;

    struct wlr_scene_tree* m_scene_tree = nullptr;
    struct wlr_scene_buffer* m_border_scene_buffer = nullptr;
    struct wlr_scene_tree* m_xdg_scene_tree = nullptr;
    std::unique_ptr<CairoBuffer> m_border_buffer;

    bool m_mapped = false;
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;

    struct wl_listener m_map_listener;
    struct wl_listener m_unmap_listener;
    struct wl_listener m_destroy_listener;
    struct wl_listener m_commit_listener;
    struct wl_listener m_request_fullscreen_listener;
    struct wl_listener m_request_maximize_listener;
};

} // namespace biway
