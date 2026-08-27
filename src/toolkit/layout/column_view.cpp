#include "toolkit/layout/column_view.hpp"

namespace biway {

ColumnView::ColumnView() = default;

void ColumnView::add(std::shared_ptr<Widget> child) {
    if (child) m_children.push_back(std::move(child));
}

void ColumnView::clear() {
    m_children.clear();
}

void ColumnView::perform_layout(cairo_t* /*cr*/) {
    int content_w = m_width - (m_pad_left + m_pad_right);
    int cur_y = m_y + m_pad_top;

    for (auto& child : m_children) {
        if (!child || !child->is_visible()) continue;
        int child_h = child->get_height();
        child->set_bounds(m_x + m_pad_left, cur_y, content_w, child_h);
        cur_y += child_h + m_spacing;
    }
}

void ColumnView::render(cairo_t* cr) {
    if (!m_visible) return;

    perform_layout(cr);

    for (auto& child : m_children) {
        if (child && child->is_visible()) child->render(cr);
    }
}

bool ColumnView::handle_mouse_motion(int x, int y) {
    if (!m_visible) return false;
    bool changed = false;

    for (auto& child : m_children) {
        if (child && child->is_visible()) {
            if (child->handle_mouse_motion(x, y)) {
                changed = true;
            }
        }
    }
    return changed;
}

bool ColumnView::handle_mouse_button(int x, int y, uint32_t button, bool pressed) {
    if (!m_visible) return false;
    bool handled = false;

    for (auto& child : m_children) {
        if (child && child->is_visible()) {
            if (child->handle_mouse_button(x, y, button, pressed)) {
                handled = true;
            }
        }
    }
    return handled;
}

void ColumnView::handle_mouse_leave() {
    for (auto& child : m_children) {
        if (child) child->handle_mouse_leave();
    }
}

} // namespace biway
