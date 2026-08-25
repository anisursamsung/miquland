#pragma once

#include "model_list_item.hpp"
#include "util.hpp"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <memory>
#include <map>

namespace biway {

enum class ListItemStyle {
    Grid, // Icon on top, label below (e.g. 100x100)
    List  // Icon on left, label + subtitle on right
};

class ListItemView {
public:
    explicit ListItemView(ListItemStyle style = ListItemStyle::Grid);
    ~ListItemView();

    void render(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                bool is_hovered, bool is_selected);

    void set_style(ListItemStyle style) { m_style = style; }
    ListItemStyle get_style() const { return m_style; }

    static void clear_icon_cache();

private:
    void render_grid_item(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                          bool is_hovered, bool is_selected);
    void render_list_item(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                          bool is_hovered, bool is_selected);

    static cairo_surface_t* load_or_cache_surface(const std::string& path, int target_size);

    ListItemStyle m_style = ListItemStyle::Grid;
};

} // namespace biway
