#pragma once

#include "core/common/util.hpp"
#include <vector>
#include <memory>

namespace miquland {

class Server;
class Popup;

class LayerSurface {
public:
    LayerSurface(Server* server, struct wlr_layer_surface_v1* layer_surface);
    ~LayerSurface();

    struct wlr_layer_surface_v1* get_wlr_layer_surface() const { return m_wlr_layer_surface; }
    struct wlr_scene_layer_surface_v1* get_scene_layer_surface() const { return m_scene_layer_surface; }
    enum zwlr_layer_shell_v1_layer get_layer() const { return m_current_layer; }
    bool is_mapped() const { return m_wlr_layer_surface && m_wlr_layer_surface->surface->mapped; }

    void configure(const struct wlr_box* full_area, struct wlr_box* usable_area);
    void update_tree();
    void update_blur();

private:
    static void handle_map(struct wl_listener* listener, void* data);
    static void handle_unmap(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);
    static void handle_surface_commit(struct wl_listener* listener, void* data);
    static void handle_new_popup(struct wl_listener* listener, void* data);

    Server* m_server = nullptr;
    struct wlr_layer_surface_v1* m_wlr_layer_surface = nullptr;
    struct wlr_scene_layer_surface_v1* m_scene_layer_surface = nullptr;
    struct wlr_scene_blur* m_blur_node = nullptr;
    struct wlr_scene_tree* m_popups_tree = nullptr;
    enum zwlr_layer_shell_v1_layer m_current_layer = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;

    struct wl_listener m_map_listener;
    struct wl_listener m_unmap_listener;
    struct wl_listener m_destroy_listener;
    struct wl_listener m_surface_commit_listener;
    struct wl_listener m_new_popup_listener;

    std::vector<std::unique_ptr<Popup>> m_popups;
};

} // namespace miquland
