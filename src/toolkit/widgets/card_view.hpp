#pragma once

#include "core/common/util.hpp"
#include <cairo.h>
#include <string>

namespace biway {

class CardView {
public:
    CardView();
    ~CardView() = default;

    void set_bg_color(float r, float g, float b, float a = 1.0f) {
        m_bg_r = r; m_bg_g = g; m_bg_b = b; m_bg_a = a;
    }
    void set_bg_color_hex(const std::string& hex);

    void set_border(int width, float r, float g, float b, float a = 1.0f) {
        m_border_width = width;
        m_border_r = r; m_border_g = g; m_border_b = b; m_border_a = a;
    }
    void set_border_color_hex(int width, const std::string& hex);

    void set_corner_radius(int radius) { m_corner_radius = radius; }
    int get_corner_radius() const { return m_corner_radius; }

    void render(cairo_t* cr, int x, int y, int width, int height) const;

    static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r);

private:
    float m_bg_r = 0.12f;
    float m_bg_g = 0.12f;
    float m_bg_b = 0.18f;
    float m_bg_a = 0.95f;

    int m_border_width = 1;
    float m_border_r = 0.25f;
    float m_border_g = 0.25f;
    float m_border_b = 0.35f;
    float m_border_a = 0.8f;

    int m_corner_radius = 8;
};

} // namespace biway
