#pragma once

#include "toolkit/widget.hpp"
#include <string>
#include <functional>

namespace biway {

class ButtonView : public Widget {
public:
    ButtonView(const std::string& text = "");
    ~ButtonView() override = default;

    void set_text(const std::string& text);
    const std::string& get_text() const { return m_text; }

    void set_font_size(int size_pt);
    void set_font_bold(bool bold);

    void set_corner_radius(int radius) { m_corner_radius = radius; }
    void set_padding(int pad_h, int pad_v) { m_pad_h = pad_h; m_pad_v = pad_v; }

    void set_selected(bool selected) { m_selected = selected; }
    bool is_selected() const { return m_selected; }

    void set_on_click(std::function<void()> on_click) { m_on_click = std::move(on_click); }

    // Theme color setters (defaults automatically hook to Config Material colors)
    void set_custom_colors(bool custom) { m_use_custom_colors = custom; }
    void set_bg_hex(const std::string& hex) { m_custom_bg = hex; m_use_custom_colors = true; }
    void set_fg_hex(const std::string& hex) { m_custom_fg = hex; m_use_custom_colors = true; }

    int get_preferred_width() const { return m_preferred_width; }
    int get_preferred_height() const { return m_preferred_height; }

    void calculate_preferred_size(cairo_t* cr);

    void render(cairo_t* cr) override;
    bool handle_mouse_button(int x, int y, uint32_t button, bool pressed) override;

private:
    std::string m_text;
    int m_font_size = 11;
    bool m_font_bold = false;
    int m_corner_radius = 6;
    int m_pad_h = 10;
    int m_pad_v = 4;

    bool m_selected = false;
    bool m_use_custom_colors = false;
    std::string m_custom_bg;
    std::string m_custom_fg;

    int m_preferred_width = 0;
    int m_preferred_height = 0;

    std::function<void()> m_on_click;
};

} // namespace biway
