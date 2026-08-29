#include "core/popup.hpp"
#include "core/view.hpp"
#include "core/layer_surface.hpp"
#include "core/server.hpp"
#include "core/output.hpp"
#include "core/config/config.hpp"

namespace miquland {

Popup::Popup(struct wlr_xdg_popup* popup, struct wlr_scene_tree* parent_tree, View* view, std::function<void(Popup*)> on_destroy)
    : m_popup(popup), m_view(view), m_on_destroy(std::move(on_destroy))
{
    struct wlr_scene_tree* target_parent = parent_tree;
    struct wlr_xdg_surface* parent_surface = wlr_xdg_surface_try_from_wlr_surface(popup->parent);
    if (parent_surface && parent_surface->data) {
        target_parent = static_cast<struct wlr_scene_tree*>(parent_surface->data);
    }

    m_scene_tree = wlr_scene_xdg_surface_create(target_parent, popup->base);
    if (m_scene_tree) {
        m_scene_tree->node.data = view;
    }
    popup->base->data = m_scene_tree;

    m_commit_listener.notify = handle_commit;
    wl_signal_add(&popup->base->surface->events.commit, &m_commit_listener);

    m_reposition_listener.notify = handle_reposition;
    wl_signal_add(&popup->events.reposition, &m_reposition_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&popup->events.destroy, &m_destroy_listener);

    m_new_popup_listener.notify = handle_new_popup;
    wl_signal_add(&popup->base->events.new_popup, &m_new_popup_listener);
}

Popup::Popup(struct wlr_xdg_popup* popup, struct wlr_scene_tree* parent_tree, LayerSurface* layer_surface, std::function<void(Popup*)> on_destroy)
    : m_popup(popup), m_layer_surface(layer_surface), m_on_destroy(std::move(on_destroy))
{
    struct wlr_scene_tree* target_parent = parent_tree;
    struct wlr_xdg_surface* parent_surface = wlr_xdg_surface_try_from_wlr_surface(popup->parent);
    if (parent_surface && parent_surface->data) {
        target_parent = static_cast<struct wlr_scene_tree*>(parent_surface->data);
    }

    m_scene_tree = wlr_scene_xdg_surface_create(target_parent, popup->base);
    popup->base->data = m_scene_tree;

    m_commit_listener.notify = handle_commit;
    wl_signal_add(&popup->base->surface->events.commit, &m_commit_listener);

    m_reposition_listener.notify = handle_reposition;
    wl_signal_add(&popup->events.reposition, &m_reposition_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&popup->events.destroy, &m_destroy_listener);

    m_new_popup_listener.notify = handle_new_popup;
    wl_signal_add(&popup->base->events.new_popup, &m_new_popup_listener);
}

Popup::~Popup() {
    wl_list_remove(&m_commit_listener.link);
    wl_list_remove(&m_reposition_listener.link);
    wl_list_remove(&m_destroy_listener.link);
    wl_list_remove(&m_new_popup_listener.link);

    if (m_popup && m_popup->base && m_popup->base->data == m_scene_tree) {
        m_popup->base->data = nullptr;
    }
}

void Popup::unconstrain() {
    if (!m_popup) return;

    if (m_view) {
        Server* server = m_view->get_server();
        if (!server || !server->get_output_manager()) return;

        struct wlr_output_layout* layout = server->get_output_manager()->get_layout();
        if (!layout) return;

        struct wlr_output* output = wlr_output_layout_output_at(
            layout, m_view->get_x() + m_view->get_width() / 2, m_view->get_y() + m_view->get_height() / 2);
        if (!output) {
            output = wlr_output_layout_get_center_output(layout);
        }
        if (!output) return;

        struct wlr_box output_box = {};
        wlr_output_layout_get_box(layout, output, &output_box);

        int bw = Config::get().get_window_border_width();
        struct wlr_box toplevel_space_box = {
            .x = output_box.x - (m_view->get_x() + bw),
            .y = output_box.y - (m_view->get_y() + bw),
            .width = output_box.width,
            .height = output_box.height,
        };

        wlr_xdg_popup_unconstrain_from_box(m_popup, &toplevel_space_box);
    } else if (m_layer_surface) {
        struct wlr_layer_surface_v1* layer_surf = m_layer_surface->get_wlr_layer_surface();
        if (!layer_surf || !layer_surf->output) return;

        struct wlr_box box = {
            .x = 0,
            .y = 0,
            .width = layer_surf->output->width,
            .height = layer_surf->output->height,
        };
        wlr_xdg_popup_unconstrain_from_box(m_popup, &box);
    }
}

void Popup::handle_commit(struct wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, m_commit_listener);
    if (popup->m_popup && popup->m_popup->base && popup->m_popup->base->initial_commit) {
        popup->unconstrain();
    }
}

void Popup::handle_reposition(struct wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, m_reposition_listener);
    popup->unconstrain();
}

void Popup::handle_destroy(struct wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, m_destroy_listener);
    if (popup->m_on_destroy) {
        popup->m_on_destroy(popup);
    }
}

void Popup::handle_new_popup(struct wl_listener* listener, void* data) {
    Popup* popup = wl_container_of(listener, popup, m_new_popup_listener);
    auto* child_popup = static_cast<struct wlr_xdg_popup*>(data);

    if (popup->m_view) {
        auto child = std::make_unique<Popup>(child_popup, popup->m_scene_tree, popup->m_view, [popup](Popup* target) {
            for (auto it = popup->m_child_popups.begin(); it != popup->m_child_popups.end(); ++it) {
                if (it->get() == target) {
                    popup->m_child_popups.erase(it);
                    break;
                }
            }
        });
        popup->m_child_popups.push_back(std::move(child));
    } else if (popup->m_layer_surface) {
        auto child = std::make_unique<Popup>(child_popup, popup->m_scene_tree, popup->m_layer_surface, [popup](Popup* target) {
            for (auto it = popup->m_child_popups.begin(); it != popup->m_child_popups.end(); ++it) {
                if (it->get() == target) {
                    popup->m_child_popups.erase(it);
                    break;
                }
            }
        });
        popup->m_child_popups.push_back(std::move(child));
    }
}

} // namespace miquland
