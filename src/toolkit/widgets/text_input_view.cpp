#include "toolkit/widgets/text_input_view.hpp"
#include "toolkit/widgets/card_view.hpp"
#include "core/config/config.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <algorithm>
#include <cstring>

namespace biway {

TextInputView::TextInputView() = default;

void TextInputView::set_text(std::string text) {
    m_text = std::move(text);
    m_cursor_pos = (int)m_text.length();
    if (m_on_text_changed) {
        m_on_text_changed(m_text);
    }
}

void TextInputView::clear() {
    m_text.clear();
    m_cursor_pos = 0;
    if (m_on_text_changed) {
        m_on_text_changed(m_text);
    }
}

bool TextInputView::handle_key(uint32_t modifiers, xkb_keysym_t sym) {
    bool ctrl = (modifiers & WLR_MODIFIER_CTRL) != 0;

    if (ctrl) {
        if (sym == XKB_KEY_u || sym == XKB_KEY_U) {
            clear();
            return true;
        } else if (sym == XKB_KEY_w || sym == XKB_KEY_W) {
            // Delete word before cursor
            while (!m_text.empty() && m_text.back() == ' ') m_text.pop_back();
            while (!m_text.empty() && m_text.back() != ' ') m_text.pop_back();
            m_cursor_pos = (int)m_text.length();
            if (m_on_text_changed) m_on_text_changed(m_text);
            return true;
        }
    }

    if (sym == XKB_KEY_BackSpace) {
        if (m_cursor_pos > 0 && !m_text.empty()) {
            m_text.erase(m_cursor_pos - 1, 1);
            m_cursor_pos--;
            if (m_on_text_changed) m_on_text_changed(m_text);
        }
        return true;
    } else if (sym == XKB_KEY_Delete) {
        if (m_cursor_pos < (int)m_text.length()) {
            m_text.erase(m_cursor_pos, 1);
            if (m_on_text_changed) m_on_text_changed(m_text);
        }
        return true;
    } else if (sym == XKB_KEY_Left) {
        if (m_cursor_pos > 0) m_cursor_pos--;
        return true;
    } else if (sym == XKB_KEY_Right) {
        if (m_cursor_pos < (int)m_text.length()) m_cursor_pos++;
        return true;
    } else if (sym == XKB_KEY_Home) {
        m_cursor_pos = 0;
        return true;
    } else if (sym == XKB_KEY_End) {
        m_cursor_pos = (int)m_text.length();
        return true;
    }

    // Printable character input
    char utf8[32] = {0};
    int res = xkb_keysym_to_utf8(sym, utf8, sizeof(utf8));
    if (res > 1) { // xkb_keysym_to_utf8 includes null terminator in return count
        size_t char_len = std::strlen(utf8);
        if (char_len > 0 && (unsigned char)utf8[0] >= 32 && (unsigned char)utf8[0] != 127) {
            m_text.insert(m_cursor_pos, utf8, char_len);
            m_cursor_pos += (int)char_len;
            if (m_on_text_changed) m_on_text_changed(m_text);
            return true;
        }
    }

    return false;
}

void TextInputView::render(cairo_t* cr, int x, int y, int width, int height, bool is_focused) const {
    if (width <= 0 || height <= 0) return;

    // 1. Draw input box background & border
    CardView bg_card;
    float bg_r = 0.10f, bg_g = 0.10f, bg_b = 0.15f, bg_a = 0.95f;
    Config::parse_hex_color(Config::get().get_color_surface_variant(), bg_r, bg_g, bg_b, bg_a);
    bg_card.set_bg_color(bg_r, bg_g, bg_b, bg_a);
    bg_card.set_corner_radius(8);

    if (is_focused) {
        float r = 0.54f, g = 0.71f, b = 0.98f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_color_primary(), r, g, b, a);
        bg_card.set_border(2, r, g, b, a);
    } else {
        float r = 0.25f, g = 0.25f, b = 0.35f, a = 0.8f;
        Config::parse_hex_color(Config::get().get_color_outline(), r, g, b, a);
        bg_card.set_border(1, r, g, b, a);
    }
    bg_card.render(cr, x, y, width, height);

    // 2. Draw text or placeholder
    int pad_x = 14;
    int inner_w = width - 2 * pad_x;

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans 12");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);
    pango_layout_set_single_paragraph_mode(layout, TRUE);
    pango_layout_set_width(layout, inner_w * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    bool is_empty = m_text.empty();
    const std::string& display_text = is_empty ? m_placeholder : m_text;
    pango_layout_set_text(layout, display_text.c_str(), -1);

    int text_w = 0, text_h = 0;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);
    double draw_y = y + (height - text_h) / 2.0;

    if (is_empty) {
        float pr = 0.5f, pg = 0.5f, pb = 0.6f, pa = 0.7f;
        Config::parse_hex_color(Config::get().get_color_on_surface_variant(), pr, pg, pb, pa);
        cairo_set_source_rgba(cr, pr, pg, pb, pa);
    } else {
        float tr = 0.95f, tg = 0.95f, tb = 1.0f, ta = 1.0f;
        Config::parse_hex_color(Config::get().get_color_on_surface(), tr, tg, tb, ta);
        cairo_set_source_rgba(cr, tr, tg, tb, ta);
    }

    cairo_move_to(cr, x + pad_x, draw_y);
    pango_cairo_show_layout(cr, layout);

    // 3. Draw cursor if focused
    if (is_focused) {
        double cursor_x = x + pad_x;
        double cursor_y = draw_y;
        double cursor_h = (text_h > 0) ? text_h : 18;

        if (!is_empty) {
            int byte_idx = std::clamp(m_cursor_pos, 0, (int)m_text.length());
            PangoRectangle pos;
            pango_layout_index_to_pos(layout, byte_idx, &pos);
            cursor_x = x + pad_x + (pos.x / (double)PANGO_SCALE);
            cursor_y = draw_y + (pos.y / (double)PANGO_SCALE);
            cursor_h = pos.height / (double)PANGO_SCALE;
            if (cursor_h <= 0) cursor_h = text_h;
        }

        float r = 0.54f, g = 0.71f, b = 0.98f, a = 1.0f;
        Config::parse_hex_color(Config::get().get_color_primary(), r, g, b, a);
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_rectangle(cr, cursor_x, cursor_y, 2.0, cursor_h);
        cairo_fill(cr);
    }

    g_object_unref(layout);
}

} // namespace biway
