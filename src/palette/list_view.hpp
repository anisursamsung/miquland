#pragma once

#include "model_list_item.hpp"
#include "list_item_view.hpp"
#include <vector>
#include <memory>
#include <functional>

namespace biway {

class ListView {
public:
    using ItemClickCallback = std::function<void(const ModelListItem& item)>;

    explicit ListView(ListItemStyle style = ListItemStyle::Grid);
    ~ListView();

    void set_items(std::vector<std::shared_ptr<ModelListItem>> items);
    const std::vector<std::shared_ptr<ModelListItem>>& get_items() const { return m_items; }

    void set_grid_columns(int cols) { m_cols = cols; }
    int get_grid_columns() const { return m_cols; }

    void set_item_size(int w, int h) { m_item_width = w; m_item_height = h; }

    void set_on_item_click(ItemClickCallback cb) { m_on_item_click = std::move(cb); }

    int get_selected_index() const { return m_selected_index; }
    void set_selected_index(int idx);
    std::shared_ptr<ModelListItem> get_selected_item() const;

    bool handle_key_down(xkb_keysym_t sym);
    bool handle_mouse_move(int lx, int ly, int view_x, int view_y, int view_w, int view_h);
    bool handle_mouse_click(int lx, int ly, int view_x, int view_y, int view_w, int view_h);
    bool handle_scroll(int delta_y);

    void render(cairo_t* cr, int x, int y, int width, int height);

    void reset_selection();

private:
    int get_item_index_at(int lx, int ly, int view_x, int view_y, int view_w, int view_h) const;
    void ensure_selected_visible(int visible_rows);

    std::vector<std::shared_ptr<ModelListItem>> m_items;
    std::unique_ptr<ListItemView> m_item_view;

    int m_cols = 5;
    int m_item_width = 100;
    int m_item_height = 100;
    int m_spacing_x = 10;
    int m_spacing_y = 10;

    int m_selected_index = 0;
    int m_hovered_index = -1;
    int m_scroll_row = 0;

    ItemClickCallback m_on_item_click;
};

} // namespace biway
