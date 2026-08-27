#include "toolkit/layout/row_view.hpp"
#include "toolkit/widgets/button_view.hpp"

namespace biway {

RowView::RowView() = default;

void RowView::add_left(std::shared_ptr<Widget> child) {
    if (child) m_left_children.push_back(std::move(child));
}

void RowView::add_center(std::shared_ptr<Widget> child) {
    if (child) m_center_children.push_back(std::move(child));
}

void RowView::add_right(std::shared_ptr<Widget> child) {
    if (child) m_right_children.push_back(std::move(child));
}

void RowView::clear() {
    m_left_children.clear();
    m_center_children.clear();
    m_right_children.clear();
}

void RowView::perform_layout(cairo_t* cr) {
    int content_h = m_height - (m_pad_top + m_pad_bottom);

    // 1. Layout Left children
    int cur_x = m_x + m_pad_left;
    for (auto& child : m_left_children) {
        if (!child || !child->is_visible()) continue;

        auto btn = std::dynamic_pointer_cast<ButtonView>(child);
        if (btn) {
            btn->calculate_preferred_size(cr);
            int child_w = btn->get_preferred_width();
            int child_h = btn->get_preferred_height();
            if (child_h <= 0 || child_h > content_h) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            btn->set_bounds(cur_x, child_y, child_w, child_h);
            cur_x += child_w + m_spacing;
        } else {
            int child_w = child->get_width();
            int child_h = child->get_height();
            if (child_h <= 0) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            child->set_bounds(cur_x, child_y, child_w, child_h);
            cur_x += child_w + m_spacing;
        }
    }

    // 2. Layout Right children
    int total_right_w = 0;
    for (auto& child : m_right_children) {
        if (!child || !child->is_visible()) continue;
        auto btn = std::dynamic_pointer_cast<ButtonView>(child);
        if (btn) {
            btn->calculate_preferred_size(cr);
            total_right_w += btn->get_preferred_width();
        } else {
            total_right_w += child->get_width();
        }
    }
    if (!m_right_children.empty()) {
        total_right_w += static_cast<int>(m_right_children.size() - 1) * m_spacing;
    }

    int right_cur_x = m_x + m_width - m_pad_right - total_right_w;
    for (auto& child : m_right_children) {
        if (!child || !child->is_visible()) continue;
        auto btn = std::dynamic_pointer_cast<ButtonView>(child);
        if (btn) {
            int child_w = btn->get_preferred_width();
            int child_h = btn->get_preferred_height();
            if (child_h <= 0 || child_h > content_h) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            btn->set_bounds(right_cur_x, child_y, child_w, child_h);
            right_cur_x += child_w + m_spacing;
        } else {
            int child_w = child->get_width();
            int child_h = child->get_height();
            if (child_h <= 0) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            child->set_bounds(right_cur_x, child_y, child_w, child_h);
            right_cur_x += child_w + m_spacing;
        }
    }

    // 3. Layout Center children
    int total_center_w = 0;
    for (auto& child : m_center_children) {
        if (!child || !child->is_visible()) continue;
        auto btn = std::dynamic_pointer_cast<ButtonView>(child);
        if (btn) {
            btn->calculate_preferred_size(cr);
            total_center_w += btn->get_preferred_width();
        } else {
            total_center_w += child->get_width();
        }
    }
    if (!m_center_children.empty()) {
        total_center_w += static_cast<int>(m_center_children.size() - 1) * m_spacing;
    }

    int center_cur_x = m_x + (m_width - total_center_w) / 2;
    for (auto& child : m_center_children) {
        if (!child || !child->is_visible()) continue;
        auto btn = std::dynamic_pointer_cast<ButtonView>(child);
        if (btn) {
            int child_w = btn->get_preferred_width();
            int child_h = btn->get_preferred_height();
            if (child_h <= 0 || child_h > content_h) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            btn->set_bounds(center_cur_x, child_y, child_w, child_h);
            center_cur_x += child_w + m_spacing;
        } else {
            int child_w = child->get_width();
            int child_h = child->get_height();
            if (child_h <= 0) child_h = content_h;
            int child_y = m_y + m_pad_top + (content_h - child_h) / 2;
            child->set_bounds(center_cur_x, child_y, child_w, child_h);
            center_cur_x += child_w + m_spacing;
        }
    }
}

void RowView::render(cairo_t* cr) {
    if (!m_visible) return;

    perform_layout(cr);

    for (auto& child : m_left_children) {
        if (child && child->is_visible()) child->render(cr);
    }
    for (auto& child : m_center_children) {
        if (child && child->is_visible()) child->render(cr);
    }
    for (auto& child : m_right_children) {
        if (child && child->is_visible()) child->render(cr);
    }
}

bool RowView::handle_mouse_motion(int x, int y) {
    if (!m_visible) return false;
    bool changed = false;

    auto check_children = [&](std::vector<std::shared_ptr<Widget>>& list) {
        for (auto& child : list) {
            if (child && child->is_visible()) {
                if (child->handle_mouse_motion(x, y)) {
                    changed = true;
                }
            }
        }
    };

    check_children(m_left_children);
    check_children(m_center_children);
    check_children(m_right_children);

    return changed;
}

bool RowView::handle_mouse_button(int x, int y, uint32_t button, bool pressed) {
    if (!m_visible) return false;
    bool handled = false;

    auto check_click = [&](std::vector<std::shared_ptr<Widget>>& list) {
        for (auto& child : list) {
            if (child && child->is_visible()) {
                if (child->handle_mouse_button(x, y, button, pressed)) {
                    handled = true;
                }
            }
        }
    };

    check_click(m_left_children);
    check_click(m_center_children);
    check_click(m_right_children);

    return handled;
}

void RowView::handle_mouse_leave() {
    auto leave_children = [&](std::vector<std::shared_ptr<Widget>>& list) {
        for (auto& child : list) {
            if (child) child->handle_mouse_leave();
        }
    };
    leave_children(m_left_children);
    leave_children(m_center_children);
    leave_children(m_right_children);
}

} // namespace biway
