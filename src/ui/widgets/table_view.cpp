#include "ui/widgets/table_view.hpp"
#include "ui/widgets/text_view.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cmath>

namespace biway {

TableView::TableView(TableLayoutMode mode)
    : m_mode(mode)
{
    set_layout_mode(mode);
}

void TableView::set_layout_mode(TableLayoutMode mode) {
    m_mode = mode;
    if (m_mode == TableLayoutMode::RowsOnly) {
        m_cols = 1;
        m_cell_width = 500;
        m_cell_height = 54;
        m_spacing_x = 0;
        m_spacing_y = 6;
        m_cell_view.set_layout_mode(CellLayoutMode::ListRow);
    } else if (m_mode == TableLayoutMode::ColumnsOnly) {
        m_cols = std::max(1, (int)m_filtered_items.size());
        m_cell_width = 100;
        m_cell_height = 100;
        m_spacing_x = 8;
        m_spacing_y = 0;
        m_cell_view.set_layout_mode(CellLayoutMode::GridTile);
    } else {
        m_cols = 5;
        m_cell_width = 100;
        m_cell_height = 100;
        m_spacing_x = 10;
        m_spacing_y = 10;
        m_cell_view.set_layout_mode(CellLayoutMode::GridTile);
    }
}

void TableView::set_items(std::vector<std::shared_ptr<CellItemModel>> items) {
    m_all_items = std::move(items);
    set_filter(m_filter_query);
}

void TableView::set_filter(const std::string& query) {
    m_filter_query = query;
    m_selected_index = 0;
    m_hovered_index = -1;
    m_scroll_row = 0;

    if (query.empty()) {
        m_filtered_items = m_all_items;
    } else {
        m_filtered_items.clear();
        std::string q = query;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);

        for (const auto& item : m_all_items) {
            std::string title = item->get_title();
            std::string sub = item->get_subtitle();
            std::string cmd = item->get_exec_cmd();
            std::transform(title.begin(), title.end(), title.begin(), ::tolower);
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if (title.find(q) != std::string::npos ||
                sub.find(q) != std::string::npos ||
                cmd.find(q) != std::string::npos) {
                m_filtered_items.push_back(item);
            }
        }
    }

    if (m_mode == TableLayoutMode::ColumnsOnly) {
        m_cols = std::max(1, (int)m_filtered_items.size());
    }
}

void TableView::set_selected_index(int idx) {
    if (m_filtered_items.empty()) {
        m_selected_index = 0;
        return;
    }
    m_selected_index = std::clamp(idx, 0, (int)m_filtered_items.size() - 1);
}

std::shared_ptr<CellItemModel> TableView::get_selected_item() const {
    if (m_selected_index >= 0 && m_selected_index < (int)m_filtered_items.size()) {
        return m_filtered_items[m_selected_index];
    }
    return nullptr;
}

void TableView::reset_selection() {
    m_selected_index = 0;
    m_hovered_index = -1;
    m_scroll_row = 0;
}

void TableView::ensure_selected_visible(int visible_rows) {
    if (m_cols <= 0 || visible_rows <= 0) return;
    int target_row = m_selected_index / m_cols;

    if (target_row < m_scroll_row) {
        m_scroll_row = target_row;
    } else if (target_row >= m_scroll_row + visible_rows) {
        m_scroll_row = target_row - visible_rows + 1;
    }
}

bool TableView::handle_key_down(xkb_keysym_t sym) {
    if (m_filtered_items.empty()) return false;

    int total = (int)m_filtered_items.size();
    if (sym == XKB_KEY_Right) {
        if (m_selected_index + 1 < total) {
            m_selected_index++;
            return true;
        }
    } else if (sym == XKB_KEY_Left) {
        if (m_selected_index > 0) {
            m_selected_index--;
            return true;
        }
    } else if (sym == XKB_KEY_Down) {
        if (m_selected_index + m_cols < total) {
            m_selected_index += m_cols;
            return true;
        }
    } else if (sym == XKB_KEY_Up) {
        if (m_selected_index - m_cols >= 0) {
            m_selected_index -= m_cols;
            return true;
        }
    } else if (sym == XKB_KEY_Return || sym == XKB_KEY_KP_Enter) {
        auto item = get_selected_item();
        if (item && m_on_item_click) {
            m_on_item_click(*item);
            return true;
        }
    }
    return false;
}

int TableView::get_item_index_at(int lx, int ly, int view_x, int view_y, int view_w, int view_h) const {
    if (lx < view_x || lx > view_x + view_w || ly < view_y || ly > view_y + view_h) {
        return -1;
    }

    int rel_x = lx - view_x;
    int rel_y = ly - view_y;

    int col = rel_x / (m_cell_width + m_spacing_x);
    int row = rel_y / (m_cell_height + m_spacing_y);

    if (col < 0 || col >= m_cols) return -1;

    int item_idx = (m_scroll_row + row) * m_cols + col;
    if (item_idx >= 0 && item_idx < (int)m_filtered_items.size()) {
        return item_idx;
    }
    return -1;
}

bool TableView::handle_mouse_move(int lx, int ly, int view_x, int view_y, int view_w, int view_h) {
    int idx = get_item_index_at(lx, ly, view_x, view_y, view_w, view_h);
    if (idx != m_hovered_index) {
        m_hovered_index = idx;
        return true;
    }
    return false;
}

bool TableView::handle_mouse_click(int lx, int ly, int view_x, int view_y, int view_w, int view_h) {
    int idx = get_item_index_at(lx, ly, view_x, view_y, view_w, view_h);
    if (idx >= 0 && idx < (int)m_filtered_items.size()) {
        m_selected_index = idx;
        if (m_on_item_click) {
            m_on_item_click(*m_filtered_items[idx]);
        }
        return true;
    }
    return false;
}

bool TableView::handle_scroll(int delta_y) {
    if (m_cols <= 0) return false;
    int total_rows = (m_filtered_items.size() + m_cols - 1) / m_cols;

    if (delta_y > 0) {
        if (m_scroll_row > 0) {
            m_scroll_row--;
            return true;
        }
    } else if (delta_y < 0) {
        if (m_scroll_row + 1 < total_rows) {
            m_scroll_row++;
            return true;
        }
    }
    return false;
}

void TableView::render(cairo_t* cr, int x, int y, int width, int height) {
    if (width <= 0 || height <= 0 || m_cols <= 0) return;

    if (m_filtered_items.empty()) {
        TextView empty_msg("No matching applications");
        empty_msg.set_font_size(12);
        empty_msg.set_alignment(TextAlignment::Center);
        empty_msg.set_color(0.55f, 0.55f, 0.65f, 0.9f);
        empty_msg.render(cr, x, y + height / 2 - 15, width, 30);
        return;
    }

    if (m_mode == TableLayoutMode::RowsOnly) {
        m_cell_width = width;
    }

    int visible_rows = height / (m_cell_height + m_spacing_y);
    if (visible_rows <= 0) visible_rows = 1;
    ensure_selected_visible(visible_rows);

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);

    int start_idx = m_scroll_row * m_cols;
    int total_items = (int)m_filtered_items.size();

    for (int i = start_idx; i < total_items; ++i) {
        int local_idx = i - start_idx;
        int row = local_idx / m_cols;
        int col = local_idx % m_cols;

        int item_x = x + col * (m_cell_width + m_spacing_x);
        int item_y = y + row * (m_cell_height + m_spacing_y);

        if (item_y >= y + height) {
            break;
        }

        bool is_selected = (i == m_selected_index);
        bool is_hovered = (i == m_hovered_index);

        m_cell_view.render(cr, *m_filtered_items[i], item_x, item_y, m_cell_width, m_cell_height,
                           is_hovered, is_selected);
    }

    cairo_restore(cr);
}

} // namespace biway
