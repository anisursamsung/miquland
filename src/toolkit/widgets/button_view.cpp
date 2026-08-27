#include "toolkit/widgets/button_view.hpp"
#include "toolkit/widgets/card_view.hpp"
#include "core/config/config.hpp"
#include <pango/pangocairo.h>

namespace biway {

ButtonView::ButtonView(const std::string& text)
    : m_text(text)
{
}

void ButtonView::set_text(const std::string& text) {
    if (m_text != text) {
        m_text = text;
        m_preferred_width = 0;
        m_preferred_height = 0;
    }
}

void ButtonView::set_font_size(int size_pt) {
    if (m_font_size != size_pt) {
        m_font_size = size_pt;
        m_preferred_width = 0;
        m_preferred_height = 0;
    }
}

void ButtonView::set_font_bold(bool bold) {
    if (m_font_bold != bold) {
        m_font_bold = bold;
        m_preferred_width = 0;
        m_preferred_height = 0;
    }
}

void ButtonView::calculate_preferred_size(cairo_t* cr) {
    if (m_text.empty()) {
        m_preferred_width = m_pad_h * 2;
        m_preferred_height = m_pad_v * 2;
        return;
    }

    PangoLayout* layout = pango_cairo_create_layout(cr);
    std::string font_desc_str = "Sans " + std::to_string(m_font_size) + (m_font_bold ? " Bold" : "");
    PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, m_text.c_str(), -1);

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);

    pango_font_description_free(desc);
    g_object_unref(layout);

    m_preferred_width = text_w + m_pad_h * 2;
    m_preferred_height = text_h + m_pad_v * 2;
}

void ButtonView::render(cairo_t* cr) {
    if (!m_visible || m_width <= 0 || m_height <= 0) return;

    Config& cfg = Config::get();
    float bg_r = 0.9f, bg_g = 0.9f, bg_b = 0.9f, bg_a = 1.0f;
    float fg_r = 0.1f, fg_g = 0.1f, fg_b = 0.1f, fg_a = 1.0f;

    if (m_use_custom_colors) {
        Config::parse_hex_color(m_custom_bg, bg_r, bg_g, bg_b, bg_a);
        Config::parse_hex_color(m_custom_fg, fg_r, fg_g, fg_b, fg_a);
    } else {
        if (m_selected) {
            Config::parse_hex_color(cfg.get_color_primary(), bg_r, bg_g, bg_b, bg_a);
            Config::parse_hex_color(cfg.get_color_on_primary(), fg_r, fg_g, fg_b, fg_a);
        } else if (m_hovered) {
            Config::parse_hex_color(cfg.get_color_primary_container(), bg_r, bg_g, bg_b, bg_a);
            Config::parse_hex_color(cfg.get_color_on_primary_container(), fg_r, fg_g, fg_b, fg_a);
        } else {
            Config::parse_hex_color(cfg.get_color_secondary(), bg_r, bg_g, bg_b, bg_a);
            Config::parse_hex_color(cfg.get_color_on_secondary(), fg_r, fg_g, fg_b, fg_a);
        }
    }

    // Draw button background pill
    CardView::draw_rounded_rect(cr, m_x, m_y, m_width, m_height, m_corner_radius);
    cairo_set_source_rgba(cr, bg_r, bg_g, bg_b, bg_a);
    cairo_fill(cr);

    // Draw button text
    if (!m_text.empty()) {
        PangoLayout* layout = pango_cairo_create_layout(cr);
        std::string font_desc_str = "Sans " + std::to_string(m_font_size) + (m_font_bold ? " Bold" : "");
        PangoFontDescription* desc = pango_font_description_from_string(font_desc_str.c_str());
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_text(layout, m_text.c_str(), -1);

        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        double text_x = m_x + (m_width - text_w) / 2.0;
        double text_y = m_y + (m_height - text_h) / 2.0;

        cairo_move_to(cr, text_x, text_y);
        cairo_set_source_rgba(cr, fg_r, fg_g, fg_b, fg_a);
        pango_cairo_show_layout(cr, layout);

        pango_font_description_free(desc);
        g_object_unref(layout);
    }
}

bool ButtonView::handle_mouse_button(int x, int y, uint32_t button, bool pressed) {
    if (!m_visible) return false;

    if (!contains(x, y)) {
        if (m_pressed) {
            m_pressed = false;
            return true;
        }
        return false;
    }

    if (pressed) {
        m_pressed = true;
        return true;
    } else {
        if (m_pressed) {
            m_pressed = false;
            if (m_on_click) {
                m_on_click();
            }
            return true;
        }
    }
    return false;
}

} // namespace biway
