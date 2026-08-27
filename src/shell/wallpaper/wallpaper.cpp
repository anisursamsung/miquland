#include "shell/wallpaper/wallpaper.hpp"
#include "core/server.hpp"
#include "core/config/config.hpp"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <algorithm>

namespace biway {

Wallpaper::Wallpaper(Server* server)
    : m_server(server)
{
    m_scene_buffer = wlr_scene_buffer_create(server->get_bg_tree(), nullptr);
}

Wallpaper::~Wallpaper() = default;

void Wallpaper::set_wallpaper(const std::string& path) {
    Config::get().set_wallpaper_path(path);
    if (m_width > 0 && m_height > 0) {
        render(m_width, m_height);
    }
}

void Wallpaper::render(int width, int height) {
    if (width <= 0 || height <= 0) return;

    m_width = width;
    m_height = height;

    if (!m_buffer) {
        m_buffer = std::make_unique<CairoBuffer>(width, height);
    } else {
        m_buffer->resize(width, height);
    }

    cairo_t* cr = m_buffer->get_cairo();

    // Default to clean solid black background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    std::string path = Config::get().get_wallpaper_path();
    if (!path.empty() && path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            path = std::string(home) + path.substr(1);
        }
    }

    if (path.empty() || !std::filesystem::exists(path)) {
        // Fallbacks: system installed wallpapers, then local assets
        const std::vector<std::string> fallbacks = {
            "/usr/share/backgrounds/biway/lightwallpaper.png",
            "/usr/share/backgrounds/biway/darkwallpaper.jpg",
            "/usr/share/biway/lightwallpaper.png",
            "/usr/share/biway/darkwallpaper.jpg",
            "/usr/share/backgrounds/biway/wallpaper.png",
            "/usr/share/biway/wallpaper.png",
            "assets/lightwallpaper.png",
            "assets/darkwallpaper.jpg",
            "assets/wallpaper.png"
        };
        for (const auto& fb : fallbacks) {
            if (std::filesystem::exists(fb)) {
                path = fb;
                break;
            }
        }
    }

    if (!path.empty() && std::filesystem::exists(path)) {
        GError* error = nullptr;
        // Load the original unscaled image to preserve aspect ratio data
        GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file(path.c_str(), &error);

        if (pixbuf) {
            int p_width = gdk_pixbuf_get_width(pixbuf);
            int p_height = gdk_pixbuf_get_height(pixbuf);
            int p_stride = gdk_pixbuf_get_rowstride(pixbuf);
            int p_channels = gdk_pixbuf_get_n_channels(pixbuf);
            guchar* p_pixels = gdk_pixbuf_get_pixels(pixbuf);

            cairo_surface_t* img_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, p_width, p_height);
            unsigned char* c_data = cairo_image_surface_get_data(img_surf);
            int c_stride = cairo_image_surface_get_stride(img_surf);

            cairo_surface_flush(img_surf);
            
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
                    dst[x] = (a << 24) | (pr << 16) | (pg << 8) | pb;
                    src += p_channels;
                }
            }
            cairo_surface_mark_dirty(img_surf);

            double img_w = cairo_image_surface_get_width(img_surf);
            double img_h = cairo_image_surface_get_height(img_surf);

            // TODO: Wire this up to Config::get().get_wallpaper_mode() later
            std::string mode = "Fill"; // Options: "Fill", "Contain", "Stretch", "Tile"

            cairo_save(cr);

            if (mode == "Stretch") {
                cairo_scale(cr, (double)width / img_w, (double)height / img_h);
                cairo_set_source_surface(cr, img_surf, 0, 0);
                cairo_paint(cr);
            } 
            else if (mode == "Tile") {
                cairo_pattern_t *pattern = cairo_pattern_create_for_surface(img_surf);
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
                cairo_set_source(cr, pattern);
                cairo_paint(cr);
                cairo_pattern_destroy(pattern);
            } 
            else if (mode == "Contain" || mode == "Fill") {
                double ratio_x = (double)width / img_w;
                double ratio_y = (double)height / img_h;
                
                // Contain uses min() so it fits entirely; Fill uses max() so it covers the whole screen
                double scale = (mode == "Fill") ? std::max(ratio_x, ratio_y) : std::min(ratio_x, ratio_y);
                
                double offset_x = (width - (img_w * scale)) / 2.0;
                double offset_y = (height - (img_h * scale)) / 2.0;
                
                cairo_translate(cr, offset_x, offset_y);
                cairo_scale(cr, scale, scale);
                cairo_set_source_surface(cr, img_surf, 0, 0);
                cairo_paint(cr);
            }

            cairo_restore(cr);
            cairo_surface_destroy(img_surf);
            g_object_unref(pixbuf);
            log_info("Rendered wallpaper from: " + path);
        } else {
            log_error("Failed to load wallpaper: " + path + " (" + (error ? error->message : "unknown error") + ")");
            if (error) g_error_free(error);
        }
    }

    wlr_scene_buffer_set_buffer(m_scene_buffer, m_buffer->get_wlr_buffer());
}

} // namespace biway