#include "list_view.hpp"
#include <algorithm>
#include <cmath>

namespace biway {

ListView::ListView(ListItemStyle style)
    : m_item_view(std::make_unique<ListItemView>(style)) {}

ListView::~ListView() = default;

void ListView::set_items(std::vector<std::shared_ptr<ModelListItem>> items) {
    m_items = std::move(items);
    m_selected_index = m_items.empty() ? -1 : 0;
    m_hovered_index = -1;
    m_scroll_row = 0;
}

void ListView::reset_selection() {
    m_selected_index = m_items.empty() ? -1 : 0;
    m_hovered_index = -1;
    m_scroll_row = 0;
}

void ListView::set_selected_index(int idx) {
    if (m_items.empty()) {
        m_selected_index = -1;
        return;
    }
    m_selected_index = std::clamp(idx, 0, static_cast<int>(m_items.size()) - 1);
}

std::shared_ptr<ModelListItem> ListView::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < static_cast<int>(m_items.size())) {
        return m_items[m_selected_index];
    }
    return nullptr;
}

int ListView::get_item_index_at(int lx, int ly, int view_x, int view_y, int view_w, int view_h) const {
    if (m_items.empty()) return -1;

    int total_grid_w = m_cols * m_item_width + (m_cols - 1) * m_spacing_x;
    int start_x = view_x + std::max(0, (view_w - total_grid_w) / 2);
    int start_y = view_y + 10;

    int rel_x = lx - start_x;
    int rel_y = ly - start_y;

    if (rel_x < 0 || rel_y < 0) return -1;

    int cell_w = m_item_width + m_spacing_x;
    int cell_h = m_item_height + m_spacing_y;

    int col = rel_x / cell_w;
    int col_rem = rel_x % cell_w;
    int row = (rel_y / cell_h) + m_scroll_row;
    int row_rem = rel_y % cell_h;

    if (col < 0 || col >= m_cols || col_rem > m_item_width || row_rem > m_item_height) {
        return -1;
    }

    int idx = row * m_cols + col;
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        return idx;
    }

    return -1;
}

bool ListView::handle_mouse_move(int lx, int ly, int view_x, int view_y, int view_w, int view_h) {
    int old_hover = m_hovered_index;
    m_hovered_index = get_item_index_at(lx, ly, view_x, view_y, view_w, view_h);
    return (old_hover != m_hovered_index);
}

bool ListView::handle_mouse_click(int lx, int ly, int view_x, int view_y, int view_w, int view_h) {
    int idx = get_item_index_at(lx, ly, view_x, view_y, view_w, view_h);
    if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
        m_selected_index = idx;
        if (m_on_item_click) {
            m_on_item_click(*m_items[idx]);
        }
        return true;
    }
    return false;
}

bool ListView::handle_scroll(int delta_y) {
    int total_rows = (static_cast<int>(m_items.size()) + m_cols - 1) / m_cols;
    int old_scroll = m_scroll_row;

    if (delta_y > 0) {
        m_scroll_row = std::min(m_scroll_row + 1, std::max(0, total_rows - 3));
    } else if (delta_y < 0) {
        m_scroll_row = std::max(0, m_scroll_row - 1);
    }

    return (old_scroll != m_scroll_row);
}

void ListView::ensure_selected_visible(int visible_rows) {
    if (m_selected_index < 0) return;
    int sel_row = m_selected_index / m_cols;
    if (sel_row < m_scroll_row) {
        m_scroll_row = sel_row;
    } else if (sel_row >= m_scroll_row + visible_rows) {
        m_scroll_row = sel_row - visible_rows + 1;
    }
}

bool ListView::handle_key_down(xkb_keysym_t sym) {
    if (m_items.empty()) return false;

    int count = static_cast<int>(m_items.size());
    int old_idx = m_selected_index;

    switch (sym) {
    case XKB_KEY_Left:
    case XKB_KEY_h:
        if (m_selected_index > 0) {
            m_selected_index--;
        }
        break;
    case XKB_KEY_Right:
    case XKB_KEY_l:
        if (m_selected_index < count - 1) {
            m_selected_index++;
        }
        break;
    case XKB_KEY_Up:
    case XKB_KEY_k:
        if (m_selected_index - m_cols >= 0) {
            m_selected_index -= m_cols;
        }
        break;
    case XKB_KEY_Down:
    case XKB_KEY_j:
        if (m_selected_index + m_cols < count) {
            m_selected_index += m_cols;
        }
        break;
    case XKB_KEY_Home:
        m_selected_index = 0;
        break;
    case XKB_KEY_End:
        m_selected_index = count - 1;
        break;
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
        if (m_selected_index >= 0 && m_selected_index < count) {
            if (m_on_item_click) {
                m_on_item_click(*m_items[m_selected_index]);
            }
            return true;
        }
        return false;
    default:
        return false;
    }

    ensure_selected_visible(3);
    return (old_idx != m_selected_index);
}

void ListView::render(cairo_t* cr, int x, int y, int width, int height) {
    if (m_items.empty() || width <= 0 || height <= 0) return;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);

    int total_grid_w = m_cols * m_item_width + (m_cols - 1) * m_spacing_x;
    int start_x = x + std::max(0, (width - total_grid_w) / 2);
    int start_y = y + 10;

    int cell_w = m_item_width + m_spacing_x;
    int cell_h = m_item_height + m_spacing_y;

    int visible_rows = (height - 20) / cell_h + 1;
    int start_idx = m_scroll_row * m_cols;
    int end_idx = std::min(static_cast<int>(m_items.size()), start_idx + visible_rows * m_cols);

    for (int i = start_idx; i < end_idx; ++i) {
        int row = (i / m_cols) - m_scroll_row;
        int col = i % m_cols;

        int item_x = start_x + col * cell_w;
        int item_y = start_y + row * cell_h;

        bool is_hovered = (i == m_hovered_index);
        bool is_selected = (i == m_selected_index);

        m_item_view->render(cr, *m_items[i], item_x, item_y, m_item_width, m_item_height,
                            is_hovered, is_selected);
    }

    cairo_restore(cr);
}

} // namespace biway
