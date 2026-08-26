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
        float bgr = 0.20f, bgg = 0.28f, bgb = 0.40f, bga = 0.95f;
        Config::parse_hex_color(Config::get().get_color_primary_container(), bgr, bgg, bgb, bga);
        card.set_bg_color(bgr, bgg, bgb, bga);

        float r = 0.54f, g = 0.71f, b = 0.98f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_color_primary(), r, g, b, a);
        card.set_border(2, r, g, b, a);
        card.set_corner_radius(8);
        card.render(cr, x, y, width, height);
    } else if (is_hovered) {
        CardView card;
        float bgr = 0.18f, bgg = 0.18f, bgb = 0.26f, bga = 0.80f;
        Config::parse_hex_color(Config::get().get_color_surface_variant(), bgr, bgg, bgb, bga);
        card.set_bg_color(bgr, bgg, bgb, bga);

        float r = 0.35f, g = 0.35f, b = 0.48f, a = 0.6f;
        Config::parse_hex_color(Config::get().get_color_outline(), r, g, b, a);
        card.set_border(1, r, g, b, a);
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
        float tr = 1.0f, tg = 1.0f, tb = 1.0f, ta = 1.0f;
        Config::parse_hex_color(Config::get().get_color_on_primary_container(), tr, tg, tb, ta);
        title.set_color(tr, tg, tb, ta);
    } else {
        float tr = 0.85f, tg = 0.85f, tb = 0.95f, ta = 1.0f;
        Config::parse_hex_color(Config::get().get_color_on_surface(), tr, tg, tb, ta);
        title.set_color(tr, tg, tb, ta);
    }
    title.render(cr, x + 6, y + 54, width - 12, height - 58);
}

void CellItemView::render_list_row(cairo_t* cr, const CellItemModel& item, int x, int y, int width, int height,
                                   bool is_hovered, bool is_selected) const {
    // 1. Background highlight
    if (is_selected) {
        CardView card;
        float bgr = 0.20f, bgg = 0.28f, bgb = 0.40f, bga = 0.95f;
        Config::parse_hex_color(Config::get().get_color_primary_container(), bgr, bgg, bgb, bga);
        card.set_bg_color(bgr, bgg, bgb, bga);

        float r = 0.54f, g = 0.71f, b = 0.98f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_color_primary(), r, g, b, a);
        card.set_border(1, r, g, b, a);
        card.set_corner_radius(6);
        card.render(cr, x, y, width, height);
    } else if (is_hovered) {
        CardView card;
        float bgr = 0.18f, bgg = 0.18f, bgb = 0.26f, bga = 0.70f;
        Config::parse_hex_color(Config::get().get_color_surface_variant(), bgr, bgg, bgb, bga);
        card.set_bg_color(bgr, bgg, bgb, bga);
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
    if (is_selected) {
        float tr = 1.0f, tg = 1.0f, tb = 1.0f, ta = 1.0f;
        Config::parse_hex_color(Config::get().get_color_on_primary_container(), tr, tg, tb, ta);
        title.set_color(tr, tg, tb, ta);
    } else {
        float tr = 0.92f, tg = 0.92f, tb = 1.0f, ta = 1.0f;
        Config::parse_hex_color(Config::get().get_color_on_surface(), tr, tg, tb, ta);
        title.set_color(tr, tg, tb, ta);
    }
    title.render(cr, text_x, y + 6, text_w, 20);

    if (!item.get_subtitle().empty()) {
        TextView sub(item.get_subtitle());
        sub.set_font_size(9);
        sub.set_max_width(text_w);
        float sr = 0.6f, sg = 0.6f, sb = 0.7f, sa = 0.9f;
        Config::parse_hex_color(Config::get().get_color_on_surface_variant(), sr, sg, sb, sa);
        sub.set_color(sr, sg, sb, sa);
        sub.render(cr, text_x, y + 26, text_w, 18);
    }
}

} // namespace biway
