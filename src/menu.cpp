#include "menu.hpp"
#include "server.hpp"
#include "output.hpp"
#include "input.hpp"
#include "config.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <set>

namespace biway {

namespace fs = std::filesystem;

static void draw_rounded_rect(cairo_t* cr, double x, double y, double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2.0, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2.0);
    cairo_arc(cr, x + r, y + h - r, r, M_PI / 2.0, M_PI);
    cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
    cairo_close_path(cr);
}

static std::string clean_exec(const std::string& raw) {
    std::string result;
    std::istringstream iss(raw);
    std::string token;
    while (iss >> token) {
        if (token.size() >= 2 && token[0] == '%' && (token[1] == 'u' || token[1] == 'U' ||
            token[1] == 'f' || token[1] == 'F' || token[1] == 'i' || token[1] == 'c' ||
            token[1] == 'k' || token[1] == 'v' || token[1] == 'm')) {
            continue;
        }
        if (!result.empty()) result += " ";
        result += token;
    }
    return result;
}

Menu::Menu(Server* server)
    : m_server(server)
{
    m_list_view = std::make_unique<ListView>(ListItemStyle::Grid);
    m_list_view->set_grid_columns(5);
    m_list_view->set_item_size(100, 100);

    m_list_view->set_on_item_click([this](const ModelListItem& item) {
        launch_selected_app(item);
    });

    m_scene_buffer = wlr_scene_buffer_create(server->get_bar_tree(), nullptr);
    wlr_scene_node_set_enabled(&m_scene_buffer->node, false);

    scan_desktop_files();
}

Menu::~Menu() = default;

void Menu::open() {
    m_visible = true;
    m_search_query.clear();
    filter_items();
    m_list_view->reset_selection();
    wlr_scene_node_set_enabled(&m_scene_buffer->node, true);

    auto* output_mgr = m_server->get_output_manager();
    if (output_mgr) {
        struct wlr_box box = output_mgr->get_primary_geometry();
        int sw = box.width > 0 ? box.width : 1920;
        int sh = box.height > 0 ? box.height : 1080;
        render(sw, sh);
    }
}

void Menu::close() {
    if (!m_visible) return;
    m_visible = false;
    wlr_scene_node_set_enabled(&m_scene_buffer->node, false);
}

void Menu::toggle() {
    if (m_visible) {
        close();
    } else {
        open();
    }
}

void Menu::schedule_redraw() {
    if (m_visible) {
        render(m_screen_width, m_screen_height);
    }
}

void Menu::launch_selected_app(const ModelListItem& item) {
    if (!item.get_exec_cmd().empty()) {
        m_server->get_input_manager()->spawn_command(item.get_exec_cmd().c_str());
    }
    close();
}

std::string Menu::resolve_icon_path(const std::string& icon_name) {
    if (icon_name.empty()) return "";

    if (icon_name[0] == '/' && fs::exists(icon_name)) {
        return icon_name;
    }

    std::string theme = Config::get().get_icon_theme();
    if (theme.empty()) theme = "hicolor";

    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";

    std::vector<std::string> base_dirs = {
        home_str + "/.local/share/icons/" + theme,
        home_str + "/.icons/" + theme,
        "/usr/share/icons/" + theme,
        home_str + "/.local/share/icons/hicolor",
        "/usr/share/icons/hicolor",
        "/usr/share/pixmaps"
    };

    std::vector<std::string> sizes = { "48x48/apps", "64x64/apps", "32x32/apps", "128x128/apps", "scalable/apps", "48x48", "apps", "" };
    std::vector<std::string> exts = { ".png", ".svg", ".xpm", "" };

    for (const auto& base : base_dirs) {
        for (const auto& sz : sizes) {
            for (const auto& ext : exts) {
                std::string candidate = base;
                if (!sz.empty()) candidate += "/" + sz;
                candidate += "/" + icon_name + ext;

                if (fs::exists(candidate)) {
                    return candidate;
                }
            }
        }
    }

    return "";
}

void Menu::scan_desktop_files() {
    m_all_items.clear();

    const char* home = getenv("HOME");
    std::string home_str = home ? home : "";

    std::vector<std::string> dirs = {
        home_str + "/.local/share/applications",
        "/usr/share/applications",
        "/usr/local/share/applications",
        "/var/lib/flatpak/exports/share/applications"
    };

    std::set<std::string> seen_names;

    for (const auto& dir : dirs) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() != ".desktop") continue;

            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            std::string line;
            bool in_desktop_entry = false;
            std::string name, exec, icon, comment, type;
            bool nodisplay = false;
            bool hidden = false;

            while (std::getline(file, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);

                if (line == "[Desktop Entry]") {
                    in_desktop_entry = true;
                    continue;
                } else if (!line.empty() && line[0] == '[' && in_desktop_entry) {
                    break;
                }

                if (!in_desktop_entry) continue;

                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;

                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);

                if (key == "Name") name = val;
                else if (key == "Exec") exec = val;
                else if (key == "Icon") icon = val;
                else if (key == "Comment") comment = val;
                else if (key == "Type") type = val;
                else if (key == "NoDisplay") nodisplay = (val == "true" || val == "1");
                else if (key == "Hidden") hidden = (val == "true" || val == "1");
            }

            if (nodisplay || hidden || type == "Link" || type == "Directory") continue;
            if (name.empty() || exec.empty()) continue;

            std::string lower_name = name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            if (seen_names.count(lower_name)) continue;
            seen_names.insert(lower_name);

            std::string cleaned_exec = clean_exec(exec);
            std::string icon_path = resolve_icon_path(icon);

            auto item = std::make_shared<ModelListItem>(entry.path().string(), name, comment, icon, icon_path, cleaned_exec);
            m_all_items.push_back(std::move(item));
        }
    }

    std::sort(m_all_items.begin(), m_all_items.end(), [](const auto& a, const auto& b) {
        std::string na = a->get_title();
        std::string nb = b->get_title();
        std::transform(na.begin(), na.end(), na.begin(), ::tolower);
        std::transform(nb.begin(), nb.end(), nb.begin(), ::tolower);
        return na < nb;
    });

    filter_items();
}

void Menu::filter_items() {
    m_filtered_items.clear();

    if (m_search_query.empty()) {
        m_filtered_items = m_all_items;
    } else {
        std::string q = m_search_query;
        std::transform(q.begin(), q.end(), q.begin(), ::tolower);

        for (const auto& item : m_all_items) {
            std::string t = item->get_title();
            std::string e = item->get_exec_cmd();
            std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            std::transform(e.begin(), e.end(), e.begin(), ::tolower);

            if (t.find(q) != std::string::npos || e.find(q) != std::string::npos) {
                m_filtered_items.push_back(item);
            }
        }
    }

    m_list_view->set_items(m_filtered_items);
}

void Menu::reload_applications() {
    scan_desktop_files();
}

bool Menu::handle_key(uint32_t modifiers, xkb_keysym_t keysym) {
    if (!m_visible) return false;

    if (keysym == XKB_KEY_Escape) {
        close();
        return true;
    }

    if (keysym == XKB_KEY_Return || keysym == XKB_KEY_KP_Enter) {
        return m_list_view->handle_key_down(keysym);
    }

    if (keysym == XKB_KEY_Left || keysym == XKB_KEY_Right ||
        keysym == XKB_KEY_Up || keysym == XKB_KEY_Down ||
        keysym == XKB_KEY_Tab || keysym == XKB_KEY_Home || keysym == XKB_KEY_End) {
        bool handled = m_list_view->handle_key_down(keysym);
        if (handled) schedule_redraw();
        return true;
    }

    if (keysym == XKB_KEY_BackSpace) {
        if (!m_search_query.empty()) {
            m_search_query.pop_back();
            filter_items();
            schedule_redraw();
        }
        return true;
    }

    // Printable ascii character input for instant search
    if (keysym >= 32 && keysym <= 126) {
        char ch = static_cast<char>(keysym);
        m_search_query += ch;
        filter_items();
        schedule_redraw();
        return true;
    }

    return true;
}

bool Menu::handle_mouse_move(double lx, double ly) {
    if (!m_visible) return false;

    int palette_x = m_modal_x + 10;
    int palette_y = m_modal_y + 60;
    int palette_w = m_modal_width - 20;
    int palette_h = m_modal_height - 70;

    bool updated = m_list_view->handle_mouse_move(lx, ly, palette_x, palette_y, palette_w, palette_h);
    if (updated) {
        schedule_redraw();
    }
    return true;
}

bool Menu::handle_mouse_click(double lx, double ly) {
    if (!m_visible) return false;

    // Check if clicked outside modal -> close
    if (lx < m_modal_x || lx > m_modal_x + m_modal_width ||
        ly < m_modal_y || ly > m_modal_y + m_modal_height) {
        close();
        return true;
    }

    int palette_x = m_modal_x + 10;
    int palette_y = m_modal_y + 60;
    int palette_w = m_modal_width - 20;
    int palette_h = m_modal_height - 70;

    if (m_list_view->handle_mouse_click(lx, ly, palette_x, palette_y, palette_w, palette_h)) {
        return true;
    }

    return true;
}

bool Menu::handle_scroll(double delta_y) {
    if (!m_visible) return false;

    bool updated = m_list_view->handle_scroll(delta_y > 0 ? 1 : -1);
    if (updated) {
        schedule_redraw();
    }
    return true;
}

void Menu::render(int screen_width, int screen_height) {
    if (screen_width <= 0 || screen_height <= 0 || !m_visible) return;

    m_screen_width = screen_width;
    m_screen_height = screen_height;

    m_modal_width = std::min(800, m_screen_width - 40);
    m_modal_height = std::min(400, m_screen_height - 40);

    m_modal_x = (m_screen_width - m_modal_width) / 2;
    m_modal_y = (m_screen_height - m_modal_height) / 2;

    if (!m_buffer) {
        m_buffer = std::make_unique<CairoBuffer>(m_screen_width, m_screen_height);
    } else {
        m_buffer->resize(m_screen_width, m_screen_height);
    }

    cairo_t* cr = m_buffer->get_cairo();

    // 1. Semi-transparent dark overlay over entire screen
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.08, 0.65);
    cairo_paint(cr);

    // 2. Centered Modal Box Background
    cairo_set_source_rgba(cr, 0.11, 0.11, 0.16, 0.98); // #1e1e2e
    draw_rounded_rect(cr, m_modal_x, m_modal_y, m_modal_width, m_modal_height, 12.0);
    cairo_fill(cr);

    // Modal Border
    cairo_set_source_rgba(cr, 0.35, 0.38, 0.55, 0.8); // #585b70
    cairo_set_line_width(cr, 1.5);
    draw_rounded_rect(cr, m_modal_x, m_modal_y, m_modal_width, m_modal_height, 12.0);
    cairo_stroke(cr);

    // 3. Search Bar Header
    int search_box_x = m_modal_x + 16;
    int search_box_y = m_modal_y + 14;
    int search_box_w = m_modal_width - 32;
    int search_box_h = 36;

    cairo_set_source_rgb(cr, 0.07, 0.07, 0.11); // #11111b
    draw_rounded_rect(cr, search_box_x, search_box_y, search_box_w, search_box_h, 6.0);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.27, 0.28, 0.38); // #45475a
    cairo_set_line_width(cr, 1.0);
    draw_rounded_rect(cr, search_box_x, search_box_y, search_box_w, search_box_h, 6.0);
    cairo_stroke(cr);

    PangoLayout* layout = pango_cairo_create_layout(cr);
    PangoFontDescription* font_desc = pango_font_description_from_string("Sans 11");
    pango_layout_set_font_description(layout, font_desc);
    pango_font_description_free(font_desc);

    if (m_search_query.empty()) {
        pango_layout_set_text(layout, "Type to search applications...", -1);
        cairo_set_source_rgb(cr, 0.45, 0.47, 0.60);
    } else {
        std::string display_query = m_search_query + " |";
        pango_layout_set_text(layout, display_query.c_str(), -1);
        cairo_set_source_rgb(cr, 0.95, 0.96, 1.0);
    }

    int text_w, text_h;
    pango_layout_get_pixel_size(layout, &text_w, &text_h);
    cairo_move_to(cr, search_box_x + 12, search_box_y + (search_box_h - text_h) / 2);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);

    // 4. Render Grid of Applications using ListView palette
    int palette_x = m_modal_x + 10;
    int palette_y = m_modal_y + 60;
    int palette_w = m_modal_width - 20;
    int palette_h = m_modal_height - 70;

    m_list_view->render(cr, palette_x, palette_y, palette_w, palette_h);

    // 5. Update scene buffer
    wlr_scene_buffer_set_buffer(m_scene_buffer, m_buffer->get_wlr_buffer());
    wlr_scene_node_set_position(&m_scene_buffer->node, 0, 0);
}

} // namespace biway
