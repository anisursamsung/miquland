#include "ui/widgets/image_view.hpp"
#include "config/config.hpp"
#include <filesystem>
#include <algorithm>
#include <vector>

namespace biway {

namespace fs = std::filesystem;

static std::map<std::string, cairo_surface_t*> s_surface_cache;

ImageView::ImageView() = default;

ImageView::ImageView(std::string path_or_name)
    : m_source(std::move(path_or_name))
{
}

void ImageView::clear_cache() {
    for (auto& pair : s_surface_cache) {
        if (pair.second) {
            cairo_surface_destroy(pair.second);
        }
    }
    s_surface_cache.clear();
}

std::string ImageView::search_icon_path(const std::string& icon_name) {
    if (icon_name.empty()) return "";
    if (icon_name[0] == '/' && fs::exists(icon_name)) return icon_name;

    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";

    std::string cfg_theme = Config::get().get_icon_theme();
    if (cfg_theme.empty()) {
        cfg_theme = "hicolor";
    }

    std::vector<std::string> themes;
    themes.push_back(cfg_theme);
    if (cfg_theme != "hicolor") {
        themes.push_back("hicolor"); // system default fallback
    }

    std::vector<std::string> base_roots = {
        home_str + "/.local/share/icons",
        home_str + "/.icons",
        "/usr/share/icons",
        "/usr/local/share/icons",
        "/var/lib/flatpak/exports/share/icons"
    };

    std::vector<std::string> subdirs = {
        "scalable/apps", "48x48/apps", "64x64/apps", "32x32/apps",
        "128x128/apps", "256x256/apps", "512x512/apps", "symbolic/apps",
        "apps", "scalable/categories", "48x48/categories", "scalable/devices",
        "scalable/places", "48x48/places", "scalable/mimetypes",
        "48x48", "64x64", "32x32", ""
    };

    std::vector<std::string> exts = { ".svg", ".png", ".xpm", ".jpg", "" };

    std::string lower_icon = icon_name;
    std::transform(lower_icon.begin(), lower_icon.end(), lower_icon.begin(), ::tolower);

    // 1. Search in configured theme & hicolor fallback
    for (const auto& th : themes) {
        for (const auto& root : base_roots) {
            std::string theme_dir = root + "/" + th;
            if (!fs::exists(theme_dir)) continue;

            for (const auto& sub : subdirs) {
                std::string search_dir = sub.empty() ? theme_dir : (theme_dir + "/" + sub);
                if (!fs::exists(search_dir)) continue;

                for (const auto& ext : exts) {
                    std::string candidate = search_dir + "/" + icon_name + ext;
                    if (fs::exists(candidate)) return candidate;

                    if (lower_icon != icon_name) {
                        std::string lower_candidate = search_dir + "/" + lower_icon + ext;
                        if (fs::exists(lower_candidate)) return lower_candidate;
                    }
                }
            }
        }
    }

    // 2. Direct fallback to /usr/share/pixmaps
    const std::vector<std::string> pixmap_dirs = {
        "/usr/share/pixmaps",
        "/usr/local/share/pixmaps",
        home_str + "/.local/share/pixmaps"
    };

    for (const auto& pdir : pixmap_dirs) {
        if (!fs::exists(pdir)) continue;
        for (const auto& ext : exts) {
            std::string candidate = pdir + "/" + icon_name + ext;
            if (fs::exists(candidate)) return candidate;

            if (lower_icon != icon_name) {
                std::string lower_candidate = pdir + "/" + lower_icon + ext;
                if (fs::exists(lower_candidate)) return lower_candidate;
            }
        }
    }

    return "";
}

cairo_surface_t* ImageView::load_or_cache_surface(const std::string& path_or_name, int target_size) {
    if (path_or_name.empty() || target_size <= 0) return nullptr;

    std::string cache_key = path_or_name + "@" + std::to_string(target_size);
    auto it = s_surface_cache.find(cache_key);
    if (it != s_surface_cache.end()) {
        return it->second;
    }

    std::string resolved_path = path_or_name;
    if (!fs::exists(resolved_path)) {
        resolved_path = search_icon_path(path_or_name);
    }

    if (resolved_path.empty() || !fs::exists(resolved_path)) {
        s_surface_cache[cache_key] = nullptr;
        return nullptr;
    }

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(resolved_path.c_str(), target_size, target_size, TRUE, &error);
    if (!pixbuf) {
        if (error) g_error_free(error);
        s_surface_cache[cache_key] = nullptr;
        return nullptr;
    }

    int p_width = gdk_pixbuf_get_width(pixbuf);
    int p_height = gdk_pixbuf_get_height(pixbuf);
    int p_stride = gdk_pixbuf_get_rowstride(pixbuf);
    int p_channels = gdk_pixbuf_get_n_channels(pixbuf);
    const guchar* p_pixels = gdk_pixbuf_get_pixels(pixbuf);

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, p_width, p_height);
    unsigned char* c_data = cairo_image_surface_get_data(surf);
    int c_stride = cairo_image_surface_get_stride(surf);

    cairo_surface_flush(surf);
    for (int y = 0; y < p_height; ++y) {
        const guchar* src = p_pixels + y * p_stride;
        auto* dst = reinterpret_cast<uint32_t*>(c_data + y * c_stride);
        for (int x = 0; x < p_width; ++x) {
            uint8_t r = src[0];
            uint8_t g = src[1];
            uint8_t b = src[2];
            uint8_t a = (p_channels == 4) ? src[3] : 255;
            uint8_t pr = (r * a) / 255;
            uint8_t pg = (g * a) / 255;
            uint8_t pb = (b * a) / 255;
            dst[x] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(pr) << 16) |
                     (static_cast<uint32_t>(pg) << 8) | static_cast<uint32_t>(pb);
            src += p_channels;
        }
    }
    cairo_surface_mark_dirty(surf);
    g_object_unref(pixbuf);

    s_surface_cache[cache_key] = surf;
    return surf;
}

void ImageView::render(cairo_t* cr, int x, int y, int width, int height) const {
    int target_w = (width > 0) ? width : m_width;
    int target_h = (height > 0) ? height : m_height;
    int max_dim = std::max(target_w, target_h);

    cairo_surface_t* surf = load_or_cache_surface(m_source, max_dim);
    if (!surf) return;

    int surf_w = cairo_image_surface_get_width(surf);
    int surf_h = cairo_image_surface_get_height(surf);

    double draw_x = x + (target_w - surf_w) / 2.0;
    double draw_y = y + (target_h - surf_h) / 2.0;

    cairo_save(cr);
    cairo_set_source_surface(cr, surf, draw_x, draw_y);
    if (m_opacity < 1.0f) {
        cairo_paint_with_alpha(cr, m_opacity);
    } else {
        cairo_paint(cr);
    }
    cairo_restore(cr);
}

} // namespace biway
