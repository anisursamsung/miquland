#include "workspace.hpp"
#include "server.hpp"
#include "view.hpp"
#include "output.hpp"
#include "bar.hpp"
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
    if (contains_view(view) || m_views.size() >= 2) {
        return false;
    }
    m_views.push_back(view);
    view->set_workspace(this);
    return true;
}

bool Workspace::remove_view(View* view) {
    auto it = std::find(m_views.begin(), m_views.end(), view);
    if (it != m_views.end()) {
        m_views.erase(it);
        view->set_workspace(nullptr);
        return true;
    }
    return false;
}

bool Workspace::contains_view(View* view) const {
    return std::find(m_views.begin(), m_views.end(), view) != m_views.end();
}

void Workspace::set_visible(bool visible) {
    m_visible = visible;
    if (m_scene_tree) {
        wlr_scene_node_set_enabled(&m_scene_tree->node, visible);
    }
}

View* Workspace::get_view(size_t index) const {
    if (index < m_views.size()) {
        return m_views[index];
    }
    return nullptr;
}

void Workspace::recalculate_layout(const struct wlr_box& usable_box) {
    if (m_views.empty()) return;

    if (m_views.size() == 1) {
        // 1 Window: takes 100% full usable screen
        m_views[0]->set_geometry(usable_box.x, usable_box.y, usable_box.width, usable_box.height);
    } else if (m_views.size() == 2) {
        if (m_split_mode == SplitMode::Horizontal) {
            // Horizontal split: 50% left, 50% right
            int half_w = usable_box.width / 2;
            int rest_w = usable_box.width - half_w;

            m_views[0]->set_geometry(usable_box.x, usable_box.y, half_w, usable_box.height);
            m_views[1]->set_geometry(usable_box.x + half_w, usable_box.y, rest_w, usable_box.height);
        } else {
            // Vertical split: 50% top, 50% bottom
            int half_h = usable_box.height / 2;
            int rest_h = usable_box.height - half_h;

            m_views[0]->set_geometry(usable_box.x, usable_box.y, usable_box.width, half_h);
            m_views[1]->set_geometry(usable_box.x, usable_box.y + half_h, usable_box.width, rest_h);
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
    if (ws) {
        ws->remove_view(view);
        recalculate_layout();

        if (ws->is_visible() && ws->view_count() > 0) {
            ws->get_view(0)->focus();
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
