#pragma once

#include "toolkit/widget.hpp"
#include <vector>
#include <memory>

namespace biway {

class RowView : public Widget {
public:
    RowView();
    ~RowView() override = default;

    void add_left(std::shared_ptr<Widget> child);
    void add_center(std::shared_ptr<Widget> child);
    void add_right(std::shared_ptr<Widget> child);

    void clear();

    void set_spacing(int spacing) { m_spacing = spacing; }
    int get_spacing() const { return m_spacing; }

    void set_padding(int h_pad, int v_pad) {
        m_pad_left = h_pad;
        m_pad_right = h_pad;
        m_pad_top = v_pad;
        m_pad_bottom = v_pad;
    }
    void set_padding(int left, int right, int top, int bottom) {
        m_pad_left = left;
        m_pad_right = right;
        m_pad_top = top;
        m_pad_bottom = bottom;
    }

    void perform_layout(cairo_t* cr);

    void render(cairo_t* cr) override;
    bool handle_mouse_motion(int x, int y) override;
    bool handle_mouse_button(int x, int y, uint32_t button, bool pressed) override;
    void handle_mouse_leave() override;

private:
    std::vector<std::shared_ptr<Widget>> m_left_children;
    std::vector<std::shared_ptr<Widget>> m_center_children;
    std::vector<std::shared_ptr<Widget>> m_right_children;

    int m_spacing = 8;
    int m_pad_left = 8;
    int m_pad_right = 8;
    int m_pad_top = 4;
    int m_pad_bottom = 4;
};

} // namespace biway
