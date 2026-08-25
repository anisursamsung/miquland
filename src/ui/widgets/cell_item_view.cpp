#include "ui/widgets/cell_item_view.hpp"
#include "config/config.hpp"

namespace biway {

CellItemView::CellItemView(CellLayoutMode mode)
    : m_layout_mode(mode)
{
}

void CellItemView::render(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                          bool is_hovered, bool is_selected) const {
    if (m_layout_mode == CellLayoutMode::GridTile) {
        render_grid_tile(cr, item, x, y, width, height, is_hovered, is_selected);
    } else {
        render_list_row(cr, item, x, y, width, height, is_hovered, is_selected);
    }
}

void CellItemView::render_grid_tile(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                                    bool is_hovered, bool is_selected) const {
    // 1. Draw card background for hover or selection
    if (is_selected) {
        CardView card;
        card.set_bg_color(0.20f, 0.28f, 0.40f, 0.95f);
        float r = 0.0f, g = 0.82f, b = 1.0f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_window_border_color_active(), r, g, b, a);
        card.set_border(2, r, g, b, a);
        card.set_corner_radius(8);
        card.render(cr, x, y, width, height);
    } else if (is_hovered) {
        CardView card;
        card.set_bg_color(0.18f, 0.18f, 0.26f, 0.80f);
        card.set_border(1, 0.35f, 0.35f, 0.48f, 0.6f);
        card.set_corner_radius(8);
        card.render(cr, x, y, width, height);
    }

    // 2. Draw Icon centered on top
    int icon_size = 40;
    int icon_x = x + (width - icon_size) / 2;
    int icon_y = y + 10;
    ImageView img(item.get_icon_path().empty() ? item.get_icon_name() : item.get_icon_path());
    img.render(cr, icon_x, icon_y, icon_size, icon_size);

    // 3. Draw Title centered below
    TextView title(item.get_title());
    title.set_font_size(10);
    title.set_bold(is_selected);
    title.set_alignment(TextAlignment::Center);
    title.set_max_width(width - 12);
    if (is_selected) {
        title.set_color(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        title.set_color(0.85f, 0.85f, 0.95f, 1.0f);
    }
    title.render(cr, x + 6, y + 54, width - 12, height - 58);
}

void CellItemView::render_list_row(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                                   bool is_hovered, bool is_selected) const {
    // 1. Background highlight
    if (is_selected) {
        CardView card;
        card.set_bg_color(0.20f, 0.28f, 0.40f, 0.95f);
        float r = 0.0f, g = 0.82f, b = 1.0f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_window_border_color_active(), r, g, b, a);
        card.set_border(1, r, g, b, a);
        card.set_corner_radius(6);
        card.render(cr, x, y, width, height);
    } else if (is_hovered) {
        CardView card;
        card.set_bg_color(0.18f, 0.18f, 0.26f, 0.70f);
        card.set_border(0, 0, 0, 0, 0);
        card.set_corner_radius(6);
        card.render(cr, x, y, width, height);
    }

    // 2. Icon on left
    int icon_size = 32;
    int icon_x = x + 10;
    int icon_y = y + (height - icon_size) / 2;
    ImageView img(item.get_icon_path().empty() ? item.get_icon_name() : item.get_icon_path());
    img.render(cr, icon_x, icon_y, icon_size, icon_size);

    // 3. Title & Subtitle stacked on right
    int text_x = x + icon_size + 20;
    int text_w = width - (icon_size + 30);

    TextView title(item.get_title());
    title.set_font_size(11);
    title.set_bold(true);
    title.set_max_width(text_w);
    title.set_color(is_selected ? 1.0f : 0.92f, is_selected ? 1.0f : 0.92f, 1.0f, 1.0f);
    title.render(cr, text_x, y + 6, text_w, 20);

    if (!item.get_subtitle().empty()) {
        TextView sub(item.get_subtitle());
        sub.set_font_size(9);
        sub.set_max_width(text_w);
        sub.set_color(0.6f, 0.6f, 0.7f, 0.9f);
        sub.render(cr, text_x, y + 26, text_w, 18);
    }
}

} // namespace biway
