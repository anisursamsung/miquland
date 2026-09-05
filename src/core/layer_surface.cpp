#include "core/layer_surface.hpp"
#include "core/popup.hpp"
#include "core/server.hpp"
#include "core/output.hpp"
#include "core/input/input.hpp"
#include "core/config/config.hpp"
#include <algorithm>

namespace miquland {

LayerSurface::LayerSurface(Server* server, struct wlr_layer_surface_v1* layer_surface)
    : m_server(server), m_wlr_layer_surface(layer_surface)
{
    m_current_layer = layer_surface->pending.layer;

    // Default to primary output if not specified by client
    if (!layer_surface->output && server->get_output_manager()) {
        auto* out = server->get_output_manager()->get_primary_output();
        if (out) {
            layer_surface->output = out->get_wlr_output();
        }
    }

    struct wlr_scene_tree* layer_tree = server->get_layer_tree(m_current_layer);
    m_scene_layer_surface = wlr_scene_layer_surface_v1_create(layer_tree, layer_surface);

    m_map_listener.notify = handle_map;
    wl_signal_add(&layer_surface->surface->events.map, &m_map_listener);

    m_unmap_listener.notify = handle_unmap;
    wl_signal_add(&layer_surface->surface->events.unmap, &m_unmap_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&layer_surface->events.destroy, &m_destroy_listener);

    m_surface_commit_listener.notify = handle_surface_commit;
    wl_signal_add(&layer_surface->surface->events.commit, &m_surface_commit_listener);

    m_new_popup_listener.notify = handle_new_popup;
    wl_signal_add(&layer_surface->events.new_popup, &m_new_popup_listener);

    log_info("New layer surface created: namespace=" + std::string(layer_surface->_namespace ? layer_surface->_namespace : ""));
}

LayerSurface::~LayerSurface() {
    m_popups.clear();
    wl_list_remove(&m_map_listener.link);
    wl_list_remove(&m_unmap_listener.link);
    wl_list_remove(&m_destroy_listener.link);
    wl_list_remove(&m_surface_commit_listener.link);
    wl_list_remove(&m_new_popup_listener.link);
    m_blur_node = nullptr;
}

void LayerSurface::configure(const struct wlr_box* full_area, struct wlr_box* usable_area) {
    if (!m_scene_layer_surface) return;
    wlr_scene_layer_surface_v1_configure(m_scene_layer_surface, full_area, usable_area);
}

void LayerSurface::update_tree() {
    if (!m_wlr_layer_surface || !m_scene_layer_surface) return;

    if (m_wlr_layer_surface->current.layer != m_current_layer) {
        m_current_layer = m_wlr_layer_surface->current.layer;
        struct wlr_scene_tree* target_tree = m_server->get_layer_tree(m_current_layer);
        if (target_tree) {
            wlr_scene_node_reparent(&m_scene_layer_surface->tree->node, target_tree);
        }
    }
}

void LayerSurface::update_blur() {
    if (!Config::get().is_blur_enabled()) {
        if (m_blur_node) {
            wlr_scene_node_set_enabled(&m_blur_node->node, false);
        }
        return;
    }

    if (!m_scene_layer_surface || !m_wlr_layer_surface || !is_mapped()) {
        if (m_blur_node) {
            wlr_scene_node_set_enabled(&m_blur_node->node, false);
        }
        return;
    }

    // Wallpapers on BACKGROUND layer should not be blurred
    if (m_current_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
        if (m_blur_node) {
            wlr_scene_node_set_enabled(&m_blur_node->node, false);
        }
        return;
    }

    // For layer surfaces, blur is opt-in via layer rules (matching namespace)
    const char* ns = m_wlr_layer_surface->_namespace;
    if (!ns || !Config::get().is_layer_blur_enabled(ns)) {
        if (m_blur_node) {
            wlr_scene_node_set_enabled(&m_blur_node->node, false);
        }
        return;
    }

    int w = m_wlr_layer_surface->surface->current.width;
    int h = m_wlr_layer_surface->surface->current.height;
    if (w <= 0 || h <= 0) return;

    if (!m_blur_node) {
        m_blur_node = wlr_scene_blur_create(m_scene_layer_surface->tree, w, h);
        if (m_blur_node) {
            wlr_scene_node_lower_to_bottom(&m_blur_node->node);
        }
    }

    if (m_blur_node) {
        wlr_scene_blur_set_size(m_blur_node, w, h);
        wlr_scene_node_set_enabled(&m_blur_node->node, true);

        // Find the scene buffer for this layer surface and hook transparency mask
        struct wlr_scene_buffer* surf_buf = nullptr;
        wlr_scene_node_for_each_buffer(&m_scene_layer_surface->tree->node,
            [](struct wlr_scene_buffer* buffer, int sx, int sy, void* user_data) {
                auto** out = static_cast<struct wlr_scene_buffer**>(user_data);
                if (!*out) *out = buffer;
            }, &surf_buf);

        if (surf_buf) {
            wlr_scene_blur_set_transparency_mask_source(m_blur_node, surf_buf);
        }
    }
}

void LayerSurface::handle_map(struct wl_listener* listener, void* data) {
    LayerSurface* surface = wl_container_of(listener, surface, m_map_listener);

    surface->m_server->arrange_layers(surface->m_wlr_layer_surface->output);
    surface->update_blur();

    // If layer surface requires keyboard interaction (e.g. rofi, fuzzel, swaylock), focus it
    if (surface->m_wlr_layer_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
        surface->m_server->focus_layer_surface(surface);
    }
}

void LayerSurface::handle_unmap(struct wl_listener* listener, void* data) {
    LayerSurface* surface = wl_container_of(listener, surface, m_unmap_listener);

    if (surface->m_server->get_focused_layer_surface() == surface) {
        surface->m_server->focus_layer_surface(nullptr);
    }

    if (surface->m_blur_node) {
        wlr_scene_node_set_enabled(&surface->m_blur_node->node, false);
    }

    surface->m_server->arrange_layers(surface->m_wlr_layer_surface->output);
}

void LayerSurface::handle_destroy(struct wl_listener* listener, void* data) {
    LayerSurface* surface = wl_container_of(listener, surface, m_destroy_listener);
    surface->m_server->remove_layer_surface(surface);
}

void LayerSurface::handle_surface_commit(struct wl_listener* listener, void* data) {
    LayerSurface* surface = wl_container_of(listener, surface, m_surface_commit_listener);
    struct wlr_layer_surface_v1* wlr_surface = surface->m_wlr_layer_surface;

    if (!wlr_surface->initialized) return;

    surface->update_tree();
    surface->update_blur();

    uint32_t committed = wlr_surface->current.committed;
    if (committed & (WLR_LAYER_SURFACE_V1_STATE_LAYER |
                     WLR_LAYER_SURFACE_V1_STATE_EXCLUSIVE_ZONE |
                     WLR_LAYER_SURFACE_V1_STATE_MARGIN |
                     WLR_LAYER_SURFACE_V1_STATE_DESIRED_SIZE |
                     WLR_LAYER_SURFACE_V1_STATE_ANCHOR)) {
        surface->m_server->arrange_layers(wlr_surface->output);
    }

    if (wlr_surface->surface->mapped &&
        wlr_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE &&
        surface->m_server->get_focused_layer_surface() != surface) {
        surface->m_server->focus_layer_surface(surface);
    }
}

void LayerSurface::handle_new_popup(struct wl_listener* listener, void* data) {
    LayerSurface* surface = wl_container_of(listener, surface, m_new_popup_listener);
    auto* popup = static_cast<struct wlr_xdg_popup*>(data);

    if (!surface->m_scene_layer_surface) return;

    auto p = std::make_unique<Popup>(popup, surface->m_scene_layer_surface->tree, surface, [surface](Popup* target) {
        for (auto it = surface->m_popups.begin(); it != surface->m_popups.end(); ++it) {
            if (it->get() == target) {
                surface->m_popups.erase(it);
                break;
            }
        }
    });
    surface->m_popups.push_back(std::move(p));
}

} // namespace miquland
