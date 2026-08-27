#pragma once

#include "toolkit/widget.hpp"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <string>

namespace biway {

enum class TextAlignment {
    Left,
    Center,
    Right
};

class TextView : public Widget {
public:
    TextView();
    explicit TextView(std::string text);
    ~TextView() override = default;

    void set_text(std::string text) { m_text = std::move(text); }
    const std::string& get_text() const { return m_text; }

    void set_font_family(std::string family) { m_font_family = std::move(family); }
    void set_font_size(int size_pt) { m_font_size = size_pt; }
    void set_bold(bool bold) { m_bold = bold; }
    void set_italic(bool italic) { m_italic = italic; }

    void set_color(float r, float g, float b, float a = 1.0f) {
        m_r = r; m_g = g; m_b = b; m_a = a;
    }
    void set_color_hex(const std::string& hex);

    void set_alignment(TextAlignment align) { m_align = align; }
    void set_ellipsize(bool ellipsize) { m_ellipsize = ellipsize; }
    void set_max_width(int max_w) { m_max_width = max_w; }

    void get_preferred_size(cairo_t* cr, int& out_w, int& out_h) const;
    void render(cairo_t* cr) override;
    void render(cairo_t* cr, int x, int y, int width = -1, int height = -1) const;

private:
    std::string m_text;
    std::string m_font_family = "Sans";
    int m_font_size = 11;
    bool m_bold = false;
    bool m_italic = false;

    float m_r = 1.0f;
    float m_g = 1.0f;
    float m_b = 1.0f;
    float m_a = 1.0f;

    TextAlignment m_align = TextAlignment::Left;
    bool m_ellipsize = true;
    int m_max_width = -1;
};

} // namespace biway
