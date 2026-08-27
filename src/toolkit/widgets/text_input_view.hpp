#pragma once

#include "core/common/util.hpp"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <xkbcommon/xkbcommon.h>
#include <string>
#include <functional>

namespace biway {

class TextInputView {
public:
    using TextChangedCallback = std::function<void(const std::string& new_text)>;

    TextInputView();
    ~TextInputView() = default;

    void set_text(std::string text);
    const std::string& get_text() const { return m_text; }

    void set_placeholder(std::string placeholder) { m_placeholder = std::move(placeholder); }
    const std::string& get_placeholder() const { return m_placeholder; }

    void set_on_text_changed(TextChangedCallback cb) { m_on_text_changed = std::move(cb); }

    void clear();

    bool handle_key(uint32_t modifiers, xkb_keysym_t sym);
    void render(cairo_t* cr, int x, int y, int width, int height, bool is_focused) const;

private:
    std::string m_text;
    std::string m_placeholder = "Type to search...";
    int m_cursor_pos = 0;

    TextChangedCallback m_on_text_changed;
};

} // namespace biway
