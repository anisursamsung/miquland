#include "ui/widgets/text_view.hpp"
#include "config/config.hpp"
#include <sstream>

namespace biway {

TextView::TextView()
    : m_text("")
{
}

TextView::TextView(std::string text)
    : m_text(std::move(text))
{
}

void TextView::set_color_hex(const std::string& hex) {
    Config::parse_hex_color(hex, m_r, m_g, m_b, m_a);
}

void TextView::get_preferred_size(cairo_t* cr, int& out_w, int& out_h) const {
    PangoLayout* layout = pango_cairo_create_layout(cr);
    
    std::string font_desc_str = m_font_family;
    if (m_bold) font_desc_str += " Bold";
    if (m_italic) font_desc_str += " Italic";
    font_desc_str += " " + std::to_string(m_font_size);

    PangoFontDescription* font_desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, m_text.c_str(), -1);
    if (m_max_width > 0) {
        pango_layout_set_width(layout, m_max_width * PANGO_SCALE);
        if (m_ellipsize) {
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        }
    }

    pango_layout_get_pixel_size(layout, &out_w, &out_h);
    g_object_unref(layout);
}

void TextView::render(cairo_t* cr, int x, int y, int width, int height) const {
    if (m_text.empty()) return;

    PangoLayout* layout = pango_cairo_create_layout(cr);

    std::string font_desc_str = m_font_family;
    if (m_bold) font_desc_str += " Bold";
    if (m_italic) font_desc_str += " Italic";
    font_desc_str += " " + std::to_string(m_font_size);

    PangoFontDescription* font_desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, m_text.c_str(), -1);

    int target_w = (width > 0) ? width : m_max_width;
    if (target_w > 0) {
        pango_layout_set_width(layout, target_w * PANGO_SCALE);
        if (m_ellipsize) {
            pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
        }
    }

    if (m_align == TextAlignment::Center) {
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    } else if (m_align == TextAlignment::Right) {
        pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    } else {
        pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    }

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    double draw_y = y;
    if (height > 0 && height > text_h) {
        draw_y += (height - text_h) / 2.0;
    }

    cairo_set_source_rgba(cr, m_r, m_g, m_b, m_a);
    cairo_move_to(cr, x, draw_y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

} // namespace biway
