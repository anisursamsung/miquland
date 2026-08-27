#pragma once

#include <cairo.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <functional>
#include <string>

namespace biway {

class Widget : public std::enable_shared_from_this<Widget> {
public:
    Widget() = default;
    virtual ~Widget() = default;

    virtual void set_bounds(int x, int y, int width, int height) {
        m_x = x;
        m_y = y;
        m_width = width;
        m_height = height;
    }

    int get_x() const { return m_x; }
    int get_y() const { return m_y; }
    int get_width() const { return m_width; }
    int get_height() const { return m_height; }

    void set_position(int x, int y) { m_x = x; m_y = y; }
    void set_size(int width, int height) { m_width = width; m_height = height; }

    void set_visible(bool visible) { m_visible = visible; }
    bool is_visible() const { return m_visible; }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    bool is_hovered() const { return m_hovered; }
    bool is_pressed() const { return m_pressed; }

    virtual bool contains(int x, int y) const {
        return (m_visible && x >= m_x && x < m_x + m_width && y >= m_y && y < m_y + m_height);
    }

    virtual void render(cairo_t* cr) = 0;

    virtual bool handle_mouse_motion(int x, int y) {
        if (!m_visible) return false;
        bool inside = contains(x, y);
        if (inside != m_hovered) {
            m_hovered = inside;
            return true; // state changed
        }
        return false;
    }

    virtual bool handle_mouse_button(int x, int y, uint32_t button, bool pressed) {
        if (!m_visible) return false;
        if (!contains(x, y)) {
            if (m_pressed) {
                m_pressed = false;
                return true;
            }
            return false;
        }
        if (m_pressed != pressed) {
            m_pressed = pressed;
            return true;
        }
        return false;
    }

    virtual void handle_mouse_leave() {
        m_hovered = false;
        m_pressed = false;
    }

protected:
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_visible = true;
    bool m_enabled = true;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace biway
