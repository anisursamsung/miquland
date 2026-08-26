#include "ui/widgets/image_view.hpp"
#include "ui/widgets/card_view.hpp"
#include "config/config.hpp"
#include <filesystem>
#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>
#include <map>

namespace biway {

namespace fs = std::filesystem;

static std::map<std::string, cairo_surface_t*> s_surface_cache;
static std::map<std::string, std::string> s_path_cache;

static std::string trim_str(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

static std::string str_to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

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
    s_path_cache.clear();
}

static std::vector<std::string> get_icon_base_roots() {
    std::vector<std::string> roots;
    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";

    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home && *xdg_data_home) {
        roots.push_back(std::string(xdg_data_home) + "/icons");
    } else if (!home_str.empty()) {
        roots.push_back(home_str + "/.local/share/icons");
    }

    if (!home_str.empty()) {
        roots.push_back(home_str + "/.icons");
    }

    const char* xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs && *xdg_data_dirs) {
        std::stringstream ss(xdg_data_dirs);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            dir = trim_str(dir);
            if (!dir.empty()) {
                roots.push_back(dir + "/icons");
            }
        }
    }

    // Standard system locations
    roots.push_back("/usr/share/icons");
    roots.push_back("/usr/local/share/icons");
    roots.push_back("/var/lib/flatpak/exports/share/icons");

    // Remove duplicates and non-existent roots
    std::vector<std::string> unique_roots;
    std::set<std::string> seen;
    for (const auto& r : roots) {
        if (seen.find(r) == seen.end() && fs::exists(r)) {
            seen.insert(r);
            unique_roots.push_back(r);
        }
    }
    return unique_roots;
}

static std::string find_theme_directory(const std::string& theme_name, const std::vector<std::string>& base_roots) {
    if (theme_name.empty()) return "";

    // 1. Exact match
    for (const auto& root : base_roots) {
        std::string dir = root + "/" + theme_name;
        if (fs::exists(dir) && fs::is_directory(dir)) {
            return dir;
        }
    }

    // 2. Case-insensitive match (e.g. "papirus" -> "Papirus")
    std::string lower_theme = str_to_lower(theme_name);
    for (const auto& root : base_roots) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (entry.is_directory()) {
                if (str_to_lower(entry.path().filename().string()) == lower_theme) {
                    return entry.path().string();
                }
            }
        }
    }

    return "";
}

static void parse_index_theme_info(const std::string& theme_dir,
                                   std::vector<std::string>& out_inherits,
                                   std::vector<std::string>& out_directories) {
    std::string theme_file = theme_dir + "/index.theme";
    if (!fs::exists(theme_file)) return;

    std::ifstream file(theme_file);
    if (!file.is_open()) return;

    std::string line;
    bool in_icon_theme_section = false;

    while (std::getline(file, line)) {
        line = trim_str(line);
        if (line.empty() || line[0] == '#') continue;

        if (line[0] == '[') {
            in_icon_theme_section = (line == "[Icon Theme]");
            continue;
        }

        if (!in_icon_theme_section) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim_str(line.substr(0, eq));
        std::string val = trim_str(line.substr(eq + 1));

        if (key == "Inherits") {
            std::stringstream ss(val);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trim_str(item);
                if (!item.empty()) {
                    out_inherits.push_back(item);
                }
            }
        } else if (key == "Directories" || key == "ScaledDirectories") {
            std::stringstream ss(val);
            std::string item;
            while (std::getline(ss, item, ',')) {
                item = trim_str(item);
                if (!item.empty()) {
                    out_directories.push_back(item);
                }
            }
        }
    }
}

std::string ImageView::search_icon_path(const std::string& icon_name) {
    if (icon_name.empty()) return "";

    // Direct path check
    if ((icon_name[0] == '/' || icon_name[0] == '.') && fs::exists(icon_name)) {
        return icon_name;
    }

    std::string cfg_theme = Config::get().get_icon_theme();
    if (cfg_theme.empty()) {
        cfg_theme = "Papirus";
    }

    std::string cache_key = cfg_theme + ":" + icon_name;
    auto cache_it = s_path_cache.find(cache_key);
    if (cache_it != s_path_cache.end()) {
        return cache_it->second;
    }

    auto base_roots = get_icon_base_roots();

    // Strip known image extensions if specified in desktop file (e.g. "kitty.png" -> "kitty")
    std::string clean_name = icon_name;
    std::string lower_clean = str_to_lower(clean_name);
    for (const char* ext : {".svg", ".png", ".xpm", ".jpg", ".jpeg", ".ico"}) {
        std::string ext_str(ext);
        if (lower_clean.size() > ext_str.size() &&
            lower_clean.substr(lower_clean.size() - ext_str.size()) == ext_str) {
            clean_name = clean_name.substr(0, clean_name.size() - ext_str.size());
            break;
        }
    }

    // Candidate names to try
    std::vector<std::string> name_candidates;
    name_candidates.push_back(clean_name);
    if (str_to_lower(clean_name) != clean_name) {
        name_candidates.push_back(str_to_lower(clean_name));
    }
    if (clean_name.size() > 9 && clean_name.substr(clean_name.size() - 9) == "-symbolic") {
        name_candidates.push_back(clean_name.substr(0, clean_name.size() - 9));
    } else {
        name_candidates.push_back(clean_name + "-symbolic");
    }

    // Build theme search hierarchy
    std::vector<std::string> theme_search_list;
    std::set<std::string> visited_themes;

    auto add_theme = [&](const std::string& t) {
        if (t.empty()) return;
        std::string lt = str_to_lower(t);
        if (visited_themes.find(lt) == visited_themes.end()) {
            visited_themes.insert(lt);
            theme_search_list.push_back(t);
        }
    };

    add_theme(cfg_theme);

    // Resolve theme inherits recursively
    for (size_t i = 0; i < theme_search_list.size(); ++i) {
        std::string t = theme_search_list[i];
        std::string t_dir = find_theme_directory(t, base_roots);
        if (!t_dir.empty()) {
            std::vector<std::string> inherits, dirs;
            parse_index_theme_info(t_dir, inherits, dirs);
            for (const auto& inh : inherits) {
                add_theme(inh);
            }
        }
    }

    // Always append system standard fallbacks
    add_theme("hicolor");
    add_theme("Adwaita");
    add_theme("AdwaitaLegacy");

    // Standard extensions to search
    const std::vector<std::string> exts = { ".svg", ".png", ".xpm", ".jpg", "" };

    // Standard subdirectories ordered by preferred application icon sizes
    const std::vector<std::string> standard_subdirs = {
        "48x48/apps", "scalable/apps", "64x64/apps", "32x32/apps", "128x128/apps",
        "256x256/apps", "512x512/apps", "24x24/apps", "22x22/apps", "16x16/apps", "96x96/apps",
        "apps",
        "48x48/categories", "scalable/categories", "64x64/categories", "32x32/categories", "24x24/categories", "22x22/categories", "16x16/categories",
        "categories",
        "48x48/devices", "scalable/devices", "64x64/devices", "32x32/devices", "24x24/devices", "22x22/devices", "16x16/devices", "128x128/devices",
        "devices",
        "48x48/places", "scalable/places", "64x64/places", "32x32/places", "24x24/places", "22x22/places", "16x16/places", "128x128/places",
        "places",
        "48x48/mimetypes", "scalable/mimetypes", "64x64/mimetypes", "32x32/mimetypes", "24x24/mimetypes", "22x22/mimetypes", "16x16/mimetypes", "128x128/mimetypes",
        "mimetypes",
        "48x48/actions", "scalable/actions", "64x64/actions", "32x32/actions", "24x24/actions", "22x22/actions", "16x16/actions",
        "actions",
        "48x48/status", "scalable/status", "64x64/status", "32x32/status", "24x24/status", "22x22/status", "16x16/status",
        "status",
        "48x48/panel", "24x24/panel", "22x22/panel", "16x16/panel",
        "panel",
        "48x48/emblems", "32x32/emblems", "24x24/emblems", "22x22/emblems", "16x16/emblems",
        "emblems",
        "48x48/emotes", "32x32/emotes", "24x24/emotes", "22x22/emotes", "16x16/emotes",
        "emotes",
        "48x48/legacy", "32x32/legacy", "24x24/legacy", "22x22/legacy", "16x16/legacy", "scalable/legacy",
        "legacy",
        "symbolic/apps", "symbolic/categories", "symbolic/devices", "symbolic/places", "symbolic/mimetypes", "symbolic/actions", "symbolic/status", "symbolic",
        "16x16/symbolic/apps", "16x16/symbolic/categories", "16x16/symbolic/devices", "16x16/symbolic/places", "16x16/symbolic/mimetypes", "16x16/symbolic/actions", "16x16/symbolic/status",
        "24x24/symbolic/apps", "24x24/symbolic/categories", "24x24/symbolic/devices", "24x24/symbolic/places", "24x24/symbolic/mimetypes", "24x24/symbolic/actions", "24x24/symbolic/status",
        "22x22/symbolic/apps", "22x22/symbolic/categories", "22x22/symbolic/devices", "22x22/symbolic/places", "22x22/symbolic/mimetypes", "22x22/symbolic/actions", "22x22/symbolic/status",
        "32x32/symbolic/apps", "32x32/symbolic/categories", "32x32/symbolic/devices", "32x32/symbolic/places", "32x32/symbolic/mimetypes", "32x32/symbolic/actions", "32x32/symbolic/status",
        "48x48", "64x64", "32x32", "24x24", "22x22", "16x16", "scalable", ""
    };

    // 1. Search across theme chain
    for (const auto& th : theme_search_list) {
        std::string theme_dir = find_theme_directory(th, base_roots);
        if (theme_dir.empty()) continue;

        std::vector<std::string> inherits, theme_subdirs;
        parse_index_theme_info(theme_dir, inherits, theme_subdirs);

        std::vector<std::string> subdirs_to_check = standard_subdirs;
        for (const auto& d : theme_subdirs) {
            if (std::find(subdirs_to_check.begin(), subdirs_to_check.end(), d) == subdirs_to_check.end()) {
                subdirs_to_check.push_back(d);
            }
        }

        for (const auto& name_cand : name_candidates) {
            for (const auto& sub : subdirs_to_check) {
                std::string search_dir = sub.empty() ? theme_dir : (theme_dir + "/" + sub);
                if (!fs::exists(search_dir)) continue;

                for (const auto& ext : exts) {
                    std::string candidate = search_dir + "/" + name_cand + ext;
                    if (fs::exists(candidate) && !fs::is_directory(candidate)) {
                        s_path_cache[cache_key] = candidate;
                        return candidate;
                    }
                }
            }
        }
    }

    // 2. Direct fallback to pixmap directories
    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";
    std::vector<std::string> pixmap_dirs = {
        "/usr/share/pixmaps",
        "/usr/local/share/pixmaps",
        home_str + "/.local/share/pixmaps"
    };

    const char* xdg_data_home = getenv("XDG_DATA_HOME");
    if (xdg_data_home && *xdg_data_home) {
        pixmap_dirs.push_back(std::string(xdg_data_home) + "/pixmaps");
    }

    for (const auto& pdir : pixmap_dirs) {
        if (!fs::exists(pdir)) continue;
        for (const auto& name_cand : name_candidates) {
            for (const auto& ext : exts) {
                std::string candidate = pdir + "/" + name_cand + ext;
                if (fs::exists(candidate) && !fs::is_directory(candidate)) {
                    s_path_cache[cache_key] = candidate;
                    return candidate;
                }
            }
        }
    }

    s_path_cache[cache_key] = "";
    return "";
}

cairo_surface_t* ImageView::load_or_cache_surface(const std::string& path_or_name, int target_size) {
    if (path_or_name.empty() || target_size <= 0) return nullptr;

    std::string cache_key = path_or_name + "@" + std::to_string(target_size);
    auto it = s_surface_cache.find(cache_key);
    if (it != s_surface_cache.end() && it->second != nullptr) {
        return it->second;
    }

    std::string resolved_path = path_or_name;
    if (!fs::exists(resolved_path)) {
        resolved_path = search_icon_path(path_or_name);
    }

    if (resolved_path.empty() || !fs::exists(resolved_path)) {
        return nullptr;
    }

    GError* error = nullptr;
    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(resolved_path.c_str(), target_size, target_size, TRUE, &error);
    if (!pixbuf) {
        if (error) g_error_free(error);
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
    if (!surf) {
        // Render a clean fallback app tile
        cairo_save(cr);
        cairo_set_source_rgba(cr, 0.20, 0.22, 0.32, 0.85);
        double r = 6.0;
        CardView::draw_rounded_rect(cr, x + 2, y + 2, target_w - 4, target_h - 4, r);
        cairo_fill(cr);

        cairo_set_source_rgba(cr, 0.38, 0.42, 0.58, 0.9);
        cairo_set_line_width(cr, 1.0);
        CardView::draw_rounded_rect(cr, x + 2, y + 2, target_w - 4, target_h - 4, r);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 0.85, 0.88, 0.98, 0.9);
        cairo_rectangle(cr, x + target_w / 2 - 5, y + target_h / 2 - 5, 10, 10);
        cairo_fill(cr);
        cairo_restore(cr);
        return;
    }

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

