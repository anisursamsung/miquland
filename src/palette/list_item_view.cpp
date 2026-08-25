#include "list_item_view.hpp"
#include <cmath>
#include <map>
#include <functional>

namespace biway {

static std::map<std::string, cairo_surface_t*> s_surface_cache;

static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

ListItemView::ListItemView(ListItemStyle style)
    : m_style(style) {}

ListItemView::~ListItemView() = default;

void ListItemView::clear_icon_cache() {
    for (auto& [key, surf] : s_surface_cache) {
        if (surf) {
            cairo_surface_destroy(surf);
        }
    }
    s_surface_cache.clear();
}

cairo_surface_t* ListItemView::load_or_cache_surface(const std::string& path, int target_size) {
    if (path.empty()) return nullptr;

    std::string key = path + "@" + std::to_string(target_size);
    auto it = s_surface_cache.find(key);
    if (it != s_surface_cache.end()) {
        return it->second;
    }

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(path.c_str(), target_size, target_size, TRUE, &error);
    if (error) {
        g_error_free(error);
        pixbuf = nullptr;
    }

    if (!pixbuf) {
        s_surface_cache[key] = nullptr;
        return nullptr;
    }

    int p_width = gdk_pixbuf_get_width(pixbuf);
    int p_height = gdk_pixbuf_get_height(pixbuf);
    int p_stride = gdk_pixbuf_get_rowstride(pixbuf);
    int p_channels = gdk_pixbuf_get_n_channels(pixbuf);
    const guchar* p_pixels = gdk_pixbuf_get_pixels(pixbuf);

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
            dst[x] = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(pr) << 16) |
                     (static_cast<uint32_t>(pg) << 8) | static_cast<uint32_t>(pb);
            src += p_channels;
        }
    }
    cairo_surface_mark_dirty(img_surf);
    g_object_unref(pixbuf);

    s_surface_cache[key] = img_surf;
    return img_surf;
}

void ListItemView::render(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                          bool is_hovered, bool is_selected) {
    if (m_style == ListItemStyle::Grid) {
        render_grid_item(cr, item, x, y, width, height, is_hovered, is_selected);
    } else {
        render_list_item(cr, item, x, y, width, height, is_hovered, is_selected);
    }
}

void ListItemView::render_grid_item(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                                    bool is_hovered, bool is_selected) {
    int padding = 4;
    int rx = x + padding;
    int ry = y + padding;
    int rw = width - padding * 2;
    int rh = height - padding * 2;

    // Background highlight for hover / selected
    if (is_selected) {
        cairo_set_source_rgba(cr, 0.54, 0.71, 0.98, 0.35); // #89b4fa
        draw_rounded_rect(cr, rx, ry, rw, rh, 8.0);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 0.54, 0.71, 0.98);
        cairo_set_line_width(cr, 1.5);
        draw_rounded_rect(cr, rx, ry, rw, rh, 8.0);
        cairo_stroke(cr);
    } else if (is_hovered) {
        cairo_set_source_rgba(cr, 0.28, 0.30, 0.45, 0.35);
        draw_rounded_rect(cr, rx, ry, rw, rh, 8.0);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.45, 0.50, 0.70, 0.5);
        cairo_set_line_width(cr, 1.0);
        draw_rounded_rect(cr, rx, ry, rw, rh, 8.0);
        cairo_stroke(cr);
    }

    // Icon (48x48 centered in upper half)
    const int icon_size = 48;
    int icon_x = x + (width - icon_size) / 2;
    int icon_y = y + 10;

    cairo_surface_t* surf = load_or_cache_surface(item.get_icon_path(), icon_size);
    if (surf) {
        int pw = cairo_image_surface_get_width(surf);
        int ph = cairo_image_surface_get_height(surf);
        int px = icon_x + (icon_size - pw) / 2;
        int py = icon_y + (icon_size - ph) / 2;
        cairo_set_source_surface(cr, surf, px, py);
        cairo_paint(cr);
    } else {
        // Fallback: stylized circular badge with initial letter
        double center_x = icon_x + icon_size / 2.0;
        double center_y = icon_y + icon_size / 2.0;
        double radius = icon_size / 2.0 - 2.0;

        // Generate consistent hue from title
        std::hash<std::string> hasher;
        size_t hash = hasher(item.get_title());
        double r = 0.3 + 0.5 * ((hash & 0xFF) / 255.0);
        double g = 0.3 + 0.5 * (((hash >> 8) & 0xFF) / 255.0);
        double b = 0.3 + 0.5 * (((hash >> 16) & 0xFF) / 255.0);

        cairo_set_source_rgb(cr, r, g, b);
        cairo_arc(cr, center_x, center_y, radius, 0, 2.0 * M_PI);
        cairo_fill(cr);

        // Letter
        std::string letter = item.get_title().empty() ? "?" : item.get_title().substr(0, 1);
        std::transform(letter.begin(), letter.end(), letter.begin(), ::toupper);

        PangoLayout* letter_layout = pango_cairo_create_layout(cr);
        PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 16");
        pango_layout_set_font_description(letter_layout, font_desc);
        pango_font_description_free(font_desc);
        pango_layout_set_text(letter_layout, letter.c_str(), -1);

        int lw, lh;
        pango_layout_get_pixel_size(letter_layout, &lw, &lh);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, center_x - lw / 2.0, center_y - lh / 2.0);
        pango_cairo_show_layout(cr, letter_layout);
        g_object_unref(letter_layout);
    }

    // App Label below icon
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans 9");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_width(layout, (width - 12) * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_text(layout, item.get_title().c_str(), -1);

    int text_x = x + 6;
    int text_y = icon_y + icon_size + 6;

    if (is_selected) {
        cairo_set_source_rgb(cr, 0.95, 0.96, 1.0);
    } else if (is_hovered) {
        cairo_set_source_rgb(cr, 0.90, 0.92, 0.98);
    } else {
        cairo_set_source_rgb(cr, 0.75, 0.78, 0.88);
    }

    cairo_move_to(cr, text_x, text_y);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
}

void ListItemView::render_list_item(cairo_t* cr, const ModelListItem& item, int x, int y, int width, int height,
                                    bool is_hovered, bool is_selected) {
    int padding = 4;
    int rx = x + padding;
    int ry = y + padding;
    int rw = width - padding * 2;
    int rh = height - padding * 2;

    if (is_selected) {
        cairo_set_source_rgba(cr, 0.54, 0.71, 0.98, 0.35);
        draw_rounded_rect(cr, rx, ry, rw, rh, 6.0);
        cairo_fill(cr);
    } else if (is_hovered) {
        cairo_set_source_rgba(cr, 0.28, 0.30, 0.45, 0.35);
        draw_rounded_rect(cr, rx, ry, rw, rh, 6.0);
        cairo_fill(cr);
    }

    // Left icon
    int icon_size = height - 12;
    int icon_x = x + 8;
    int icon_y = y + 6;

    cairo_surface_t* surf = load_or_cache_surface(item.get_icon_path(), icon_size);
    if (surf) {
        cairo_set_source_surface(cr, surf, icon_x, icon_y);
        cairo_paint(cr);
    }

    // Title and Subtitle
    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans Bold 10");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    pango_layout_set_text(layout, item.get_title().c_str(), -1);
    cairo_set_source_rgb(cr, 0.92, 0.94, 0.98);
    cairo_move_to(cr, icon_x + icon_size + 10, y + 6);
    pango_cairo_show_layout(cr, layout);

    if (!item.get_subtitle().empty()) {
        PangoFontDescription* sub_font = pango_font_description_from_string("Sans 8");
        pango_layout_set_font_description(layout, sub_font);
        pango_font_description_free(sub_font);
        pango_layout_set_text(layout, item.get_subtitle().c_str(), -1);
        cairo_set_source_rgb(cr, 0.60, 0.64, 0.74);
        cairo_move_to(cr, icon_x + icon_size + 10, y + 22);
        pango_cairo_show_layout(cr, layout);
    }

    g_object_unref(layout);
}

} // namespace biway
