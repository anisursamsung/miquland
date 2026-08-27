#pragma once

#include "toolkit/widgets/cell_item_view.hpp"
#include "toolkit/widgets/cell_item_model.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace biway {

enum class TableLayoutMode {
    Grid,        // N columns, M rows (GridTile cells)
    RowsOnly,    // 1 column (vertical list, ListRow cells)
    ColumnsOnly  // 1 row (horizontal bar, GridTile cells)
};

class TableView {
public:
    using ItemClickCallback = std::function<void(const CellItemModel& item)>;

    explicit TableView(TableLayoutMode mode = TableLayoutMode::Grid);
    ~TableView() = default;

    void set_layout_mode(TableLayoutMode mode);
    TableLayoutMode get_layout_mode() const { return m_mode; }

    void set_items(std::vector<std::shared_ptr<CellItemModel>> items);
    const std::vector<std::shared_ptr<CellItemModel>>& get_all_items() const { return m_all_items; }
    const std::vector<std::shared_ptr<CellItemModel>>& get_visible_items() const { return m_filtered_items; }

    void set_filter(const std::string& query);
    const std::string& get_filter() const { return m_filter_query; }

    void set_columns(int cols) { m_cols = cols; }
    int get_columns() const { return m_cols; }

    void set_cell_size(int w, int h) { m_cell_width = w; m_cell_height = h; }
    void set_spacing(int gap_x, int gap_y) { m_spacing_x = gap_x; m_spacing_y = gap_y; }

    void set_on_item_click(ItemClickCallback cb) { m_on_item_click = std::move(cb); }

    int get_selected_index() const { return m_selected_index; }
    void set_selected_index(int idx);
    std::shared_ptr<CellItemModel> get_selected_item() const;

    bool handle_key_down(xkb_keysym_t sym);
    bool handle_mouse_move(int lx, int ly, int view_x, int view_y, int view_w, int view_h);
    bool handle_mouse_click(int lx, int ly, int view_x, int view_y, int view_w, int view_h);
    bool handle_scroll(int delta_y);

    void render(cairo_t* cr, int x, int y, int width, int height);
    void reset_selection();

private:
    int get_item_index_at(int lx, int ly, int view_x, int view_y, int view_w, int view_h) const;
    void ensure_selected_visible(int visible_rows);

    TableLayoutMode m_mode = TableLayoutMode::Grid;
    std::vector<std::shared_ptr<CellItemModel>> m_all_items;
    std::vector<std::shared_ptr<CellItemModel>> m_filtered_items;
    std::string m_filter_query;

    CellItemView m_cell_view;

    int m_cols = 5;
    int m_cell_width = 100;
    int m_cell_height = 100;
    int m_spacing_x = 10;
    int m_spacing_y = 10;

    int m_selected_index = 0;
    int m_hovered_index = -1;
    int m_scroll_row = 0;

    ItemClickCallback m_on_item_click;
};

} // namespace biway
