#include "core/workspace.hpp"
#include "core/server.hpp"
#include "core/view.hpp"
#include "core/output.hpp"
#include "shell/bar/bar.hpp"
#include "core/config/config.hpp"
#include <algorithm>

namespace biway {

Workspace::Workspace(Server* server, size_t id)
    : m_server(server), m_id(id)
{
    m_scene_tree = wlr_scene_tree_create(server->get_workspaces_tree());
    set_visible(false);
}

Workspace::~Workspace() {
    // Scene tree nodes are cleaned up automatically when destroyed
}

bool Workspace::add_view(View* view) {
    if (contains_view(view)) {
        return false;
    }

    if (view->is_dialog()) {
        m_floating_views.push_back(view);
        view->set_workspace(this);
        return true;
    }

    if (m_tiled_views.size() >= 2) {
        return false;
    }
    m_tiled_views.push_back(view);
    view->set_workspace(this);
    return true;
}

bool Workspace::remove_view(View* view) {
    auto it_tile = std::find(m_tiled_views.begin(), m_tiled_views.end(), view);
    if (it_tile != m_tiled_views.end()) {
        m_tiled_views.erase(it_tile);
        view->set_workspace(nullptr);
        return true;
    }

    auto it_float = std::find(m_floating_views.begin(), m_floating_views.end(), view);
    if (it_float != m_floating_views.end()) {
        m_floating_views.erase(it_float);
        view->set_workspace(nullptr);
        return true;
    }

    return false;
}

bool Workspace::contains_view(View* view) const {
    return std::find(m_tiled_views.begin(), m_tiled_views.end(), view) != m_tiled_views.end() ||
           std::find(m_floating_views.begin(), m_floating_views.end(), view) != m_floating_views.end();
}

void Workspace::set_visible(bool visible) {
    m_visible = visible;
    if (m_scene_tree) {
        wlr_scene_node_set_enabled(&m_scene_tree->node, visible);
    }
}

View* Workspace::get_view(size_t index) const {
    if (index < m_tiled_views.size()) {
        return m_tiled_views[index];
    }
    return nullptr;
}

void Workspace::recalculate_layout(const struct wlr_box& usable_box) {
    int pad = Config::get().get_screen_edge_padding();
    int gap = Config::get().get_space_between_windows();

    int base_x = usable_box.x + pad;
    int base_y = usable_box.y + pad;
    int base_w = std::max(50, usable_box.width - 2 * pad);
    int base_h = std::max(50, usable_box.height - 2 * pad);

    if (m_tiled_views.size() == 1) {
        // 1 Window: takes 100% padded usable screen
        m_tiled_views[0]->set_geometry(base_x, base_y, base_w, base_h);
    } else if (m_tiled_views.size() == 2) {
        if (m_split_mode == SplitMode::Horizontal) {
            // Horizontal split: 50% left, 50% right with gap between windows
            int total_w = std::max(50, base_w - gap);
            int half_w = total_w / 2;
            int rest_w = total_w - half_w;

            m_tiled_views[0]->set_geometry(base_x, base_y, half_w, base_h);
            m_tiled_views[1]->set_geometry(base_x + half_w + gap, base_y, rest_w, base_h);
        } else {
            // Vertical split: 50% top, 50% bottom with gap between windows
            int total_h = std::max(50, base_h - gap);
            int half_h = total_h / 2;
            int rest_h = total_h - half_h;

            m_tiled_views[0]->set_geometry(base_x, base_y, base_w, half_h);
            m_tiled_views[1]->set_geometry(base_x, base_y + half_h + gap, base_w, rest_h);
        }
    }

    int bw = Config::get().get_window_border_width();

    // Position floating / dialog windows
    for (auto* fview : m_floating_views) {
        if (!fview || !fview->is_mapped()) continue;

        int req_w = fview->get_width() > 0 ? fview->get_width() : 750;
        int req_h = fview->get_height() > 0 ? fview->get_height() : 500;

        if (fview->get_type() == ViewType::Xdg && fview->get_xdg_toplevel()) {
            auto* xdg_surf = fview->get_xdg_toplevel()->base;
            int gw = xdg_surf->current.geometry.width;
            int gh = xdg_surf->current.geometry.height;
            if (gw <= 0 && xdg_surf->surface) {
                gw = xdg_surf->surface->current.width;
                gh = xdg_surf->surface->current.height;
            }
            if (gw > 0) req_w = gw + 2 * bw;
            if (gh > 0) req_h = gh + 2 * bw;
        }

        View* parent = fview->get_parent_view();
        if (parent && parent->is_mapped() && parent->get_width() > 0 && parent->get_height() > 0) {
            int pw = parent->get_width();
            int ph = parent->get_height();
            int px = parent->get_x();
            int py = parent->get_y();

            int max_w = std::max(50, pw - 20);
            int max_h = std::max(50, ph - 20);

            int dw = std::min(req_w, max_w);
            int dh = std::min(req_h, max_h);

            int dx = px + (pw - dw) / 2;
            int dy = py + (ph - dh) / 2;

            fview->set_geometry(dx, dy, dw, dh);
        } else {
            int dw = std::min(req_w, base_w);
            int dh = std::min(req_h, base_h);
            int dx = base_x + (base_w - dw) / 2;
            int dy = base_y + (base_h - dh) / 2;

            fview->set_geometry(dx, dy, dw, dh);
        }
    }
}

WorkspaceManager::WorkspaceManager(Server* server)
    : m_server(server)
{
    // Start with workspace 1 active
    Workspace* ws1 = get_or_create_workspace(1);
    ws1->set_visible(true);
}

WorkspaceManager::~WorkspaceManager() = default;

Workspace* WorkspaceManager::get_workspace(size_t id) {
    auto it = m_workspaces.find(id);
    if (it != m_workspaces.end()) {
        return it->second.get();
    }
    return nullptr;
}

Workspace* WorkspaceManager::get_or_create_workspace(size_t id) {
    auto it = m_workspaces.find(id);
    if (it != m_workspaces.end()) {
        return it->second.get();
    }
    auto ws = std::make_unique<Workspace>(m_server, id);
    Workspace* ptr = ws.get();
    if (id == m_active_workspace_id) {
        ptr->set_visible(true);
    }
    m_workspaces[id] = std::move(ws);
    return ptr;
}

Workspace* WorkspaceManager::get_active_workspace() {
    return get_or_create_workspace(m_active_workspace_id);
}

void WorkspaceManager::switch_to_workspace(size_t id) {
    if (id == m_active_workspace_id || id == 0) return;

    Workspace* current = get_workspace(m_active_workspace_id);
    if (current) {
        current->set_visible(false);
    }

    m_active_workspace_id = id;
    Workspace* next = get_or_create_workspace(id);
    next->set_visible(true);

    recalculate_layout();

    if (next->view_count() > 0) {
        next->get_view(0)->focus();
    } else {
        m_server->set_focused_view(nullptr);
    }

    if (m_server->get_bar()) {
        m_server->get_bar()->schedule_redraw();
    }
}

void WorkspaceManager::prev_workspace() {
    if (m_active_workspace_id > 1) {
        switch_to_workspace(m_active_workspace_id - 1);
    }
}

void WorkspaceManager::next_workspace() {
    switch_to_workspace(m_active_workspace_id + 1);
}

void WorkspaceManager::add_view_auto(View* view) {
    if (view->is_dialog()) {
        View* parent = view->get_parent_view();
        Workspace* target_ws = (parent && parent->get_workspace()) ? parent->get_workspace() : get_active_workspace();
        target_ws->add_view(view);
        if (target_ws != get_active_workspace()) {
            switch_to_workspace(target_ws->get_id());
        } else {
            recalculate_layout();
        }
        return;
    }

    Workspace* active_ws = get_active_workspace();
    if (active_ws->can_accept_view()) {
        active_ws->add_view(view);
        recalculate_layout();
    } else {
        // Find next workspace with available slot
        size_t next_id = m_active_workspace_id + 1;
        while (true) {
            Workspace* candidate = get_or_create_workspace(next_id);
            if (candidate->can_accept_view()) {
                switch_to_workspace(next_id);
                candidate->add_view(view);
                recalculate_layout();
                break;
            }
            next_id++;
        }
    }
}

void WorkspaceManager::remove_view(View* view) {
    Workspace* ws = view->get_workspace();
    View* parent = view->get_parent_view();

    if (ws) {
        ws->remove_view(view);
        recalculate_layout();

        if (ws->is_visible()) {
            if (parent && parent->is_mapped() && parent->get_workspace() == ws) {
                parent->focus();
            } else if (ws->view_count() > 0) {
                ws->get_view(0)->focus();
            }
        }
    }
}

void WorkspaceManager::move_view_to_workspace(View* view, size_t target_ws_id) {
    if (target_ws_id == 0) return;
    Workspace* current = view->get_workspace();
    if (!current || current->get_id() == target_ws_id) return;

    Workspace* target = get_or_create_workspace(target_ws_id);
    if (target->can_accept_view()) {
        current->remove_view(view);
        target->add_view(view);
        recalculate_layout();

        if (current->is_visible()) {
            if (current->view_count() > 0) {
                current->get_view(0)->focus();
            } else {
                m_server->set_focused_view(nullptr);
            }
        }
    }
}

void WorkspaceManager::focus_next_view() {
    Workspace* ws = get_active_workspace();
    if (!ws || ws->view_count() < 2) return;

    View* cur = m_server->get_focused_view();
    if (cur == ws->get_view(0)) {
        ws->get_view(1)->focus();
    } else {
        ws->get_view(0)->focus();
    }
}

void WorkspaceManager::focus_prev_view() {
    focus_next_view();
}

void WorkspaceManager::focus_window_index(size_t index) {
    Workspace* ws = get_active_workspace();
    if (ws && index < ws->view_count()) {
        View* v = ws->get_view(index);
        if (v) {
            v->focus();
        }
    }
}

void WorkspaceManager::toggle_active_split() {
    Workspace* ws = get_active_workspace();
    if (ws) {
        ws->toggle_split_mode();
        recalculate_layout();
    }
}

void WorkspaceManager::recalculate_layout() {
    struct wlr_box geom = m_server->get_output_manager()->get_primary_geometry();
    if (geom.width <= 0 || geom.height <= 0) {
        geom = { .x = 0, .y = 0, .width = 1920, .height = 1080 };
    }

    if (m_server->get_bar() && m_server->get_bar()->is_visible()) {
        int bar_h = m_server->get_bar()->get_height();
        geom.y += bar_h;
        geom.height -= bar_h;
    }

    Workspace* active_ws = get_active_workspace();
    if (active_ws) {
        active_ws->recalculate_layout(geom);
    }

    if (m_server->get_bar()) {
        m_server->get_bar()->schedule_redraw();
    }
}

} // namespace biway
