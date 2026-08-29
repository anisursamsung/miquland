#include "core/workspace.hpp"
#include "core/server.hpp"
#include "core/view.hpp"
#include "core/output.hpp"
#include "core/config/config.hpp"
#include <algorithm>
#include <iostream>

namespace miquland {

Workspace::Workspace(Server* server, size_t id)
    : m_server(server), m_id(id)
{
    m_scene_tree = wlr_scene_tree_create(m_server->get_workspaces_tree());
    wlr_scene_node_set_enabled(&m_scene_tree->node, false);
}

Workspace::~Workspace() {
    if (m_scene_tree) {
        wlr_scene_node_destroy(&m_scene_tree->node);
    }
}

bool Workspace::add_view(View* view) {
    if (!view) return false;

    if (view->is_dialog()) {
        if (std::find(m_floating_views.begin(), m_floating_views.end(), view) == m_floating_views.end()) {
            m_floating_views.push_back(view);
            view->set_workspace(this);
            return true;
        }
        return false;
    }

    if (std::find(m_tiled_views.begin(), m_tiled_views.end(), view) == m_tiled_views.end()) {
        m_tiled_views.push_back(view);
        view->set_workspace(this);
        return true;
    }
    return false;
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

void Workspace::swap_with_main(View* view) {
    if (!view || m_tiled_views.size() < 2) return;
    auto it = std::find(m_tiled_views.begin(), m_tiled_views.end(), view);
    if (it != m_tiled_views.end()) {
        size_t idx = std::distance(m_tiled_views.begin(), it);
        if (idx > 0) {
            std::swap(m_tiled_views[0], m_tiled_views[idx]);
        }
    }
}

void Workspace::swap_views(size_t idx1, size_t idx2) {
    if (idx1 < m_tiled_views.size() && idx2 < m_tiled_views.size()) {
        std::swap(m_tiled_views[idx1], m_tiled_views[idx2]);
    }
}

void Workspace::layout_spiral(int base_x, int base_y, int base_w, int base_h, int gap) {
    size_t n = m_tiled_views.size();
    if (n == 0) return;
    if (n == 1) {
        m_tiled_views[0]->set_geometry(base_x, base_y, base_w, base_h);
        return;
    }

    int cur_x = base_x;
    int cur_y = base_y;
    int cur_w = base_w;
    int cur_h = base_h;

    for (size_t i = 0; i < n; ++i) {
        if (i == n - 1) {
            m_tiled_views[i]->set_geometry(cur_x, cur_y, std::max(20, cur_w), std::max(20, cur_h));
            break;
        }

        bool split_horizontal = (i % 2 == 0);
        if (m_split_mode == SplitMode::Vertical) {
            split_horizontal = !split_horizontal;
        }

        if (split_horizontal) {
            int total_w = std::max(40, cur_w - gap);
            int half_w = total_w / 2;
            int rest_w = total_w - half_w;

            m_tiled_views[i]->set_geometry(cur_x, cur_y, half_w, cur_h);
            cur_x += half_w + gap;
            cur_w = rest_w;
        } else {
            int total_h = std::max(40, cur_h - gap);
            int half_h = total_h / 2;
            int rest_h = total_h - half_h;

            m_tiled_views[i]->set_geometry(cur_x, cur_y, cur_w, half_h);
            cur_y += half_h + gap;
            cur_h = rest_h;
        }
    }
}

void Workspace::layout_stack(int base_x, int base_y, int base_w, int base_h, int gap) {
    size_t n = m_tiled_views.size();
    if (n == 0) return;
    if (n == 1) {
        m_tiled_views[0]->set_geometry(base_x, base_y, base_w, base_h);
        return;
    }

    int total_w = std::max(50, base_w - gap);
    int master_w = static_cast<int>(total_w * 0.55);
    int stack_w = total_w - master_w;

    m_tiled_views[0]->set_geometry(base_x, base_y, master_w, base_h);

    size_t stack_count = n - 1;
    int total_stack_gaps = static_cast<int>(stack_count - 1) * gap;
    int avail_stack_h = std::max(20, base_h - total_stack_gaps);
    int item_h = avail_stack_h / static_cast<int>(stack_count);

    int cur_y = base_y;
    int stack_x = base_x + master_w + gap;

    for (size_t i = 1; i < n; ++i) {
        int win_h = (i == n - 1) ? (base_y + base_h - cur_y) : item_h;
        m_tiled_views[i]->set_geometry(stack_x, cur_y, stack_w, std::max(20, win_h));
        cur_y += win_h + gap;
    }
}

void Workspace::recalculate_layout(const struct wlr_box& usable_box) {
    int pad = Config::get().get_screen_edge_padding();
    int gap = Config::get().get_space_between_windows();

    int base_x = usable_box.x + pad;
    int base_y = usable_box.y + pad;
    int base_w = std::max(50, usable_box.width - 2 * pad);
    int base_h = std::max(50, usable_box.height - 2 * pad);

    if (Config::get().get_layout_mode() == Config::LayoutMode::Stack) {
        layout_stack(base_x, base_y, base_w, base_h, gap);
    } else {
        layout_spiral(base_x, base_y, base_w, base_h, gap);
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
    m_workspaces[id] = std::move(ws);
    return ptr;
}

Workspace* WorkspaceManager::get_active_workspace() {
    return get_or_create_workspace(m_active_workspace_id);
}

void WorkspaceManager::switch_to_workspace(size_t id) {
    if (id == 0 || id == m_active_workspace_id) return;

    Workspace* current = get_workspace(m_active_workspace_id);
    if (current) {
        current->set_visible(false);
    }

    m_active_workspace_id = id;
    Workspace* target = get_or_create_workspace(id);
    target->set_visible(true);

    recalculate_layout();

    if (target->view_count() > 0) {
        target->get_view(0)->focus();
    } else {
        m_server->set_focused_view(nullptr);
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
    active_ws->add_view(view);
    recalculate_layout();
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

void WorkspaceManager::focus_next_view() {
    Workspace* ws = get_active_workspace();
    if (!ws || ws->view_count() < 2) return;

    View* cur = m_server->get_focused_view();
    size_t count = ws->view_count();
    size_t cur_idx = 0;
    for (size_t i = 0; i < count; ++i) {
        if (ws->get_view(i) == cur) {
            cur_idx = i;
            break;
        }
    }
    size_t next_idx = (cur_idx + 1) % count;
    ws->get_view(next_idx)->focus();
}

void WorkspaceManager::focus_prev_view() {
    Workspace* ws = get_active_workspace();
    if (!ws || ws->view_count() < 2) return;

    View* cur = m_server->get_focused_view();
    size_t count = ws->view_count();
    size_t cur_idx = 0;
    for (size_t i = 0; i < count; ++i) {
        if (ws->get_view(i) == cur) {
            cur_idx = i;
            break;
        }
    }
    size_t prev_idx = (cur_idx == 0) ? (count - 1) : (cur_idx - 1);
    ws->get_view(prev_idx)->focus();
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

void WorkspaceManager::swap_with_main() {
    Workspace* ws = get_active_workspace();
    if (!ws || ws->view_count() < 2) return;
    View* cur = m_server->get_focused_view();
    if (cur) {
        ws->swap_with_main(cur);
        recalculate_layout();
        cur->focus();
    }
}

void WorkspaceManager::toggle_layout_mode() {
    Config::get().toggle_layout_mode();
    recalculate_layout();
}

void WorkspaceManager::toggle_active_split() {
    Workspace* ws = get_active_workspace();
    if (ws) {
        ws->toggle_split_mode();
        recalculate_layout();
    }
}

void WorkspaceManager::recalculate_layout() {
    struct wlr_box geom = m_server->get_output_manager()->get_primary_usable_geometry();
    if (geom.width <= 0 || geom.height <= 0) {
        geom = { .x = 0, .y = 0, .width = 1920, .height = 1080 };
    }

    Workspace* active_ws = get_active_workspace();
    if (active_ws) {
        active_ws->recalculate_layout(geom);
    }
}

} // namespace miquland
