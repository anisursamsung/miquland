#pragma once

#include "toolkit/widgets/text_view.hpp"
#include "toolkit/widgets/image_view.hpp"
#include "toolkit/widgets/card_view.hpp"
#include "toolkit/widgets/cell_item_model.hpp"
#include <memory>

namespace biway {

enum class CellLayoutMode {
    GridTile, // Icon centered on top (40px), title below
    ListRow   // Icon on left (32px), title & subtitle stacked on right
};

class CellItemView {
public:
    explicit CellItemView(CellLayoutMode mode = CellLayoutMode::GridTile);
    ~CellItemView() = default;

    void set_layout_mode(CellLayoutMode mode) { m_layout_mode = mode; }
    CellLayoutMode get_layout_mode() const { return m_layout_mode; }

    void render(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                bool is_hovered, bool is_selected) const;

private:
    void render_grid_tile(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                          bool is_hovered, bool is_selected) const;
    void render_list_row(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                         bool is_hovered, bool is_selected) const;

    CellLayoutMode m_layout_mode = CellLayoutMode::GridTile;
};

} // namespace biway
