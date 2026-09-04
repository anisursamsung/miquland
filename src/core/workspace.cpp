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

    if (m_server->get_ext_workspace_manager()) {
        std::string id_str = std::to_string(m_id);
        m_ext_handle = wlr_ext_workspace_handle_v1_create(
            m_server->get_ext_workspace_manager(),
            id_str.c_str(),
            EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE
        );
        if (m_ext_handle) {
            if (m_server->get_ext_workspace_group()) {
                wlr_ext_workspace_handle_v1_set_group(m_ext_handle, m_server->get_ext_workspace_group());
            }
            wlr_ext_workspace_handle_v1_set_name(m_ext_handle, id_str.c_str());
            bool is_act = (m_server->get_workspace_manager()) ? (m_server->get_workspace_manager()->get_active_workspace_id() == m_id) : (m_id == 1);
            wlr_ext_workspace_handle_v1_set_active(m_ext_handle, is_act);
            wlr_ext_workspace_handle_v1_set_hidden(m_ext_handle, true);
            m_ext_handle->data = this;
        }
    }
}

Workspace::~Workspace() {
    if (m_ext_handle) {
        wlr_ext_workspace_handle_v1_destroy(m_ext_handle);
        m_ext_handle = nullptr;
    }
    if (m_scene_tree) {
        wlr_scene_node_destroy(&m_scene_tree->node);
    }
}

void Workspace::update_ext_state() {
    if (m_ext_handle) {
        wlr_ext_workspace_handle_v1_set_hidden(m_ext_handle, is_empty());
    }
}

bool Workspace::add_view(View* view) {
    if (!view) return false;

    if (view->is_dialog() || view->is_floating()) {
        if (std::find(m_floating_views.begin(), m_floating_views.end(), view) == m_floating_views.end()) {
            m_floating_views.push_back(view);
            view->set_workspace(this);
            update_ext_state();
            return true;
        }
        return false;
    }

    if (std::find(m_tiled_views.begin(), m_tiled_views.end(), view) == m_tiled_views.end()) {
        m_tiled_views.push_back(view);
        view->set_workspace(this);
        update_ext_state();
        return true;
    }
    return false;
}

bool Workspace::remove_view(View* view) {
    auto it_tile = std::find(m_tiled_views.begin(), m_tiled_views.end(), view);
    if (it_tile != m_tiled_views.end()) {
        m_tiled_views.erase(it_tile);
        view->set_workspace(nullptr);
        update_ext_state();
        return true;
    }

    auto it_float = std::find(m_floating_views.begin(), m_floating_views.end(), view);
    if (it_float != m_floating_views.end()) {
        m_floating_views.erase(it_float);
        view->set_workspace(nullptr);
        update_ext_state();
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

bool Workspace::swap_views(View* view1, View* view2) {
    if (!view1 || !view2 || view1 == view2) return false;
    auto it1 = std::find(m_tiled_views.begin(), m_tiled_views.end(), view1);
    auto it2 = std::find(m_tiled_views.begin(), m_tiled_views.end(), view2);
    if (it1 != m_tiled_views.end() && it2 != m_tiled_views.end()) {
        std::iter_swap(it1, it2);
        auto* out_mgr = m_server->get_output_manager();
        if (out_mgr) {
            recalculate_layout(out_mgr->get_primary_usable_geometry());
        }
        return true;
    }
    return false;
}

void Workspace::toggle_floating(View* view) {
    if (!view || view->is_dialog()) return;

    auto it_tile = std::find(m_tiled_views.begin(), m_tiled_views.end(), view);
    if (it_tile != m_tiled_views.end()) {
        m_tiled_views.erase(it_tile);
        m_floating_views.push_back(view);
        view->set_floating(true);

        if (view->get_scene_tree()) {
            wlr_scene_node_raise_to_top(&view->get_scene_tree()->node);
        }

        auto* out_mgr = m_server->get_output_manager();
        if (out_mgr) {
            recalculate_layout(out_mgr->get_primary_usable_geometry());
        }
    } else {
        auto it_float = std::find(m_floating_views.begin(), m_floating_views.end(), view);
        if (it_float != m_floating_views.end()) {
            m_floating_views.erase(it_float);
            m_tiled_views.push_back(view);
            view->set_floating(false);

            auto* out_mgr = m_server->get_output_manager();
            if (out_mgr) {
                recalculate_layout(out_mgr->get_primary_usable_geometry());
            }
        }
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

struct wlr_box Workspace::calculate_tiled_geometry_for_new_view(const struct wlr_box& usable_box) const {
    int pad = Config::get().get_screen_edge_padding();
    int gap = Config::get().get_space_between_windows();

    int base_x = usable_box.x + pad;
    int base_y = usable_box.y + pad;
    int base_w = std::max(50, usable_box.width - 2 * pad);
    int base_h = std::max(50, usable_box.height - 2 * pad);

    size_t n = m_tiled_views.size() + 1;
    if (n == 1) {
        return { base_x, base_y, base_w, base_h };
    }

    if (Config::get().get_layout_mode() == Config::LayoutMode::Stack) {
        int total_w = std::max(50, base_w - gap);
        int master_w = static_cast<int>(total_w * 0.55);
        int stack_w = total_w - master_w;

        size_t stack_count = n - 1;
        int total_stack_gaps = static_cast<int>(stack_count - 1) * gap;
        int avail_stack_h = std::max(20, base_h - total_stack_gaps);
        int item_h = avail_stack_h / static_cast<int>(stack_count);

        int cur_y = base_y;
        int stack_x = base_x + master_w + gap;

        for (size_t i = 1; i < n; ++i) {
            int win_h = (i == n - 1) ? (base_y + base_h - cur_y) : item_h;
            if (i == n - 1) {
                return { stack_x, cur_y, stack_w, std::max(20, win_h) };
            }
            cur_y += win_h + gap;
        }
        return { stack_x, cur_y, stack_w, std::max(20, base_y + base_h - cur_y) };
    } else {
        int cur_x = base_x;
        int cur_y = base_y;
        int cur_w = base_w;
        int cur_h = base_h;

        for (size_t i = 0; i < n; ++i) {
            if (i == n - 1) {
                return { cur_x, cur_y, std::max(20, cur_w), std::max(20, cur_h) };
            }

            bool split_horizontal = (i % 2 == 0);
            if (m_split_mode == SplitMode::Vertical) {
                split_horizontal = !split_horizontal;
            }

            if (split_horizontal) {
                int total_w = std::max(40, cur_w - gap);
                int half_w = total_w / 2;
                int rest_w = total_w - half_w;

                cur_x += half_w + gap;
                cur_w = rest_w;
            } else {
                int total_h = std::max(40, cur_h - gap);
                int half_h = total_h / 2;
                int rest_h = total_h - half_h;

                cur_y += half_h + gap;
                cur_h = rest_h;
            }
        }
        return { cur_x, cur_y, std::max(20, cur_w), std::max(20, cur_h) };
    }
}

WorkspaceManager::WorkspaceManager(Server* server)
    : m_server(server)
{
    Workspace* ws1 = get_or_create_workspace(1);
    ws1->set_visible(true);
}

WorkspaceManager::~WorkspaceManager() = default;

void WorkspaceManager::prune_workspace(size_t id) {
    if (id == m_active_workspace_id) return;

    auto it = m_workspaces.find(id);
    if (it != m_workspaces.end() && it->second->is_empty()) {
        m_workspaces.erase(it);
    }
}

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

void WorkspaceManager::switch_to_workspace(size_t id, View* focus_view) {
    if (id == 0 || id == m_active_workspace_id) {
        if (focus_view) {
            focus_view->focus();
        }
        return;
    }

    size_t old_id = m_active_workspace_id;
    Workspace* current = get_workspace(old_id);
    if (current) {
        current->set_visible(false);
        if (current->get_ext_handle()) {
            wlr_ext_workspace_handle_v1_set_active(current->get_ext_handle(), false);
        }
    }

    m_active_workspace_id = id;
    Workspace* target = get_or_create_workspace(id);
    target->set_visible(true);
    if (target->get_ext_handle()) {
        wlr_ext_workspace_handle_v1_set_active(target->get_ext_handle(), true);
    }

    recalculate_layout();

    // Auto-prune previous workspace if it was left empty
    prune_workspace(old_id);

    if (focus_view) {
        focus_view->focus();
    } else if (target->view_count() > 0) {
        target->get_view(0)->focus();
    } else {
        m_server->set_focused_view(nullptr);
    }
}

void WorkspaceManager::prev_workspace() {
    auto it = m_workspaces.find(m_active_workspace_id);
    if (it != m_workspaces.end() && it != m_workspaces.begin()) {
        switch_to_workspace(std::prev(it)->first);
    } else if (m_active_workspace_id > 1) {
        switch_to_workspace(m_active_workspace_id - 1);
    }
}

void WorkspaceManager::next_workspace() {
    auto it = m_workspaces.find(m_active_workspace_id);
    if (it != m_workspaces.end() && std::next(it) != m_workspaces.end()) {
        switch_to_workspace(std::next(it)->first);
    } else {
        switch_to_workspace(m_active_workspace_id + 1);
    }
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
        size_t ws_id = ws->get_id();
        ws->remove_view(view);
        recalculate_layout();

        if (ws->is_visible()) {
            if (parent && parent->is_mapped() && parent->get_workspace() == ws) {
                parent->focus();
            } else if (ws->view_count() > 0) {
                ws->get_view(0)->focus();
            }
        } else if (ws->is_empty()) {
            prune_workspace(ws_id);
        }
    }
}

void WorkspaceManager::move_view_to_workspace(View* view, size_t target_ws_id) {
    if (target_ws_id == 0) return;
    Workspace* current = view->get_workspace();
    if (!current || current->get_id() == target_ws_id) return;

    size_t current_id = current->get_id();
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
    } else if (current->is_empty()) {
        prune_workspace(current_id);
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

void WorkspaceManager::toggle_floating_active() {
    View* cur = m_server->get_focused_view();
    if (cur && cur->get_workspace()) {
        cur->get_workspace()->toggle_floating(cur);
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
