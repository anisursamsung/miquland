#include "toolkit/widgets/card_view.hpp"
#include "core/config/config.hpp"
#include <cmath>
#include <algorithm>

namespace biway {

CardView::CardView() = default;

void CardView::set_bg_color_hex(const std::string& hex) {
    Config::parse_hex_color(hex, m_bg_r, m_bg_g, m_bg_b, m_bg_a);
}

void CardView::set_border_color_hex(int width, const std::string& hex) {
    m_border_width = width;
    Config::parse_hex_color(hex, m_border_r, m_border_g, m_border_b, m_border_a);
}

void CardView::draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, w, h);
        return;
    }
    r = std::min(r, std::min(w / 2.0, h / 2.0));
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

void CardView::render(cairo_t* cr, int x, int y, int width, int height) const {
    if (width <= 0 || height <= 0) return;

    // Background fill
    cairo_set_source_rgba(cr, m_bg_r, m_bg_g, m_bg_b, m_bg_a);
    draw_rounded_rect(cr, x, y, width, height, m_corner_radius);
    cairo_fill(cr);

    // Border stroke
    if (m_border_width > 0 && m_border_a > 0.0f) {
        cairo_set_source_rgba(cr, m_border_r, m_border_g, m_border_b, m_border_a);
        cairo_set_line_width(cr, m_border_width);

        double half_bw = m_border_width / 2.0;
        draw_rounded_rect(cr, x + half_bw, y + half_bw,
                          std::max(1.0, (double)width - m_border_width),
                          std::max(1.0, (double)height - m_border_width),
                          std::max(0.0, (double)m_corner_radius - half_bw));
        cairo_stroke(cr);
    }
}

} // namespace biway
