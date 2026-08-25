#pragma once

#include "common/util.hpp"
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string>
#include <map>

namespace biway {

class ImageView {
public:
    ImageView();
    explicit ImageView(std::string path_or_name);
    ~ImageView() = default;

    void set_source(std::string path_or_name) { m_source = std::move(path_or_name); }
    const std::string& get_source() const { return m_source; }

    void set_size(int width, int height) { m_width = width; m_height = height; }
    void set_opacity(float opacity) { m_opacity = opacity; }

    void render(cairo_t* cr, int x, int y, int width = -1, int height = -1) const;

    static void clear_cache();
    static cairo_surface_t* load_or_cache_surface(const std::string& path_or_name, int target_size);
    static std::string search_icon_path(const std::string& icon_name);

private:
    std::string m_source;
    int m_width = 32;
    int m_height = 32;
    float m_opacity = 1.0f;
};

} // namespace biway
