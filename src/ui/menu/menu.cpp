#include "ui/menu/menu.hpp"
#include "core/server.hpp"
#include "core/output.hpp"
#include "input/input.hpp"
#include "config/config.hpp"
#include "ui/widgets/image_view.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <set>

namespace biway {

namespace fs = std::filesystem;

static std::string clean_exec(const std::string& raw) {
    std::string result;
    std::stringstream ss(raw);
    std::string token;
    while (ss >> token) {
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
    m_table_view = std::make_unique<TableView>(TableLayoutMode::Grid);
    m_table_view->set_columns(5);
    m_table_view->set_cell_size(100, 100);

    m_table_view->set_on_item_click([this](const CellItemModel& item) {
        launch_selected_app(item);
    });

    m_search_input.set_placeholder("Type to search applications...");
    m_search_input.set_on_text_changed([this](const std::string& query) {
        m_search_query = query;
        m_table_view->set_filter(query);
        schedule_redraw();
    });

    m_scene_buffer = wlr_scene_buffer_create(server->get_bar_tree(), nullptr);
    wlr_scene_node_set_enabled(&m_scene_buffer->node, false);

    scan_desktop_files();
}

Menu::~Menu() = default;

void Menu::open() {
    m_visible = true;
    m_search_query.clear();
    m_search_input.clear();
    m_table_view->set_filter("");
    m_table_view->reset_selection();
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
    if (m_server && m_server->get_input_manager()) {
        m_server->get_input_manager()->set_cursor_icon("default");
    }
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

void Menu::launch_selected_app(const CellItemModel& item) {
    if (!item.get_exec_cmd().empty()) {
        m_server->get_input_manager()->spawn_command(item.get_exec_cmd().c_str());
    }
    close();
}

std::string Menu::resolve_icon_path(const std::string& icon_name) {
    return ImageView::search_icon_path(icon_name);
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
        if (!fs::exists(dir)) continue;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".desktop") continue;

            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            std::string line;
            bool is_entry = false;
            std::string name, generic_name, comment, icon, exec;
            bool no_display = false;
            bool terminal = false;

            while (std::getline(file, line)) {
                if (line == "[Desktop Entry]") {
                    is_entry = true;
                    continue;
                }
                if (line.size() > 1 && line[0] == '[' && line != "[Desktop Entry]") {
                    is_entry = false;
                }
                if (!is_entry) continue;

                size_t eq = line.find('=');
                if (eq == std::string::npos) continue;

                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);

                if (key == "Name" && name.empty()) name = val;
                else if (key == "GenericName" && generic_name.empty()) generic_name = val;
                else if (key == "Comment" && comment.empty()) comment = val;
                else if (key == "Icon" && icon.empty()) icon = val;
                else if (key == "Exec" && exec.empty()) exec = val;
                else if (key == "NoDisplay" && val == "true") no_display = true;
                else if (key == "Terminal" && val == "true") terminal = true;
            }

            if (no_display || name.empty() || exec.empty()) {
                continue;
            }

            if (seen_names.find(name) != seen_names.end()) {
                continue;
            }
            seen_names.insert(name);

            std::string clean_command = clean_exec(exec);
            if (terminal) {
                clean_command = Config::get().get_terminal() + " -e " + clean_command;
            }

            std::string resolved_icon = resolve_icon_path(icon);
            std::string subtitle = !generic_name.empty() ? generic_name : comment;

            m_all_items.push_back(std::make_shared<CellItemModel>(
                entry.path().filename().string(),
                name,
                subtitle,
                icon,
                resolved_icon,
                clean_command
            ));
        }
    }

    std::sort(m_all_items.begin(), m_all_items.end(), [](const auto& a, const auto& b) {
        std::string name_a = a->get_title();
        std::string name_b = b->get_title();
        std::transform(name_a.begin(), name_a.end(), name_a.begin(), ::tolower);
        std::transform(name_b.begin(), name_b.end(), name_b.begin(), ::tolower);
        return name_a < name_b;
    });

    m_table_view->set_items(m_all_items);
}

void Menu::reload_applications() {
    ImageView::clear_cache();
    scan_desktop_files();
    if (m_visible) {
        schedule_redraw();
    }
}

void Menu::filter_items() {
    m_table_view->set_filter(m_search_query);
}

bool Menu::handle_key(uint32_t modifiers, xkb_keysym_t keysym) {
    if (!m_visible) return false;

    if (keysym == XKB_KEY_Escape) {
        close();
        return true;
    }

    if (keysym == XKB_KEY_Return || keysym == XKB_KEY_KP_Enter) {
        auto item = m_table_view->get_selected_item();
        if (item) {
            launch_selected_app(*item);
        }
        return true;
    }

    if (keysym == XKB_KEY_Left || keysym == XKB_KEY_Right ||
        keysym == XKB_KEY_Up || keysym == XKB_KEY_Down) {
        bool handled = m_table_view->handle_key_down(keysym);
        if (handled) schedule_redraw();
        return true;
    }

    // Pass all text input keys to TextInputView
    bool text_handled = m_search_input.handle_key(modifiers, keysym);
    if (text_handled) {
        return true;
    }

    return true;
}

bool Menu::handle_mouse_move(double lx, double ly) {
    if (!m_visible) return false;

    // Search bar bounds
    int search_box_x = m_modal_x + 16;
    int search_box_y = m_modal_y + 14;
    int search_box_w = m_modal_width - 32;
    int search_box_h = 36;

    if (lx >= search_box_x && lx <= search_box_x + search_box_w &&
        ly >= search_box_y && ly <= search_box_y + search_box_h) {
        if (m_server && m_server->get_input_manager()) {
            m_server->get_input_manager()->set_cursor_icon("text");
        }
    } else {
        int palette_x = m_modal_x + 10;
        int palette_y = m_modal_y + 60;
        int palette_w = m_modal_width - 20;
        int palette_h = m_modal_height - 70;

        if (lx >= palette_x && lx <= palette_x + palette_w &&
            ly >= palette_y && ly <= palette_y + palette_h) {
            if (m_server && m_server->get_input_manager()) {
                m_server->get_input_manager()->set_cursor_icon("pointer");
            }
        } else {
            if (m_server && m_server->get_input_manager()) {
                m_server->get_input_manager()->set_cursor_icon("default");
            }
        }
    }

    int palette_x = m_modal_x + 10;
    int palette_y = m_modal_y + 60;
    int palette_w = m_modal_width - 20;
    int palette_h = m_modal_height - 70;

    bool updated = m_table_view->handle_mouse_move(lx, ly, palette_x, palette_y, palette_w, palette_h);
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

    if (m_table_view->handle_mouse_click(lx, ly, palette_x, palette_y, palette_w, palette_h)) {
        return true;
    }

    return true;
}

bool Menu::handle_scroll(double delta_y) {
    if (!m_visible) return false;

    bool updated = m_table_view->handle_scroll(delta_y > 0 ? 1 : -1);
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

    // 0. Clear old frame content to prevent ghosting
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    // 1. Semi-transparent dark backdrop over entire screen
    cairo_set_source_rgba(cr, 0.05, 0.05, 0.08, 0.65);
    cairo_paint(cr);

    // 2. Centered Modal Card Container
    CardView modal_card;
    float bg_r = 0.11f, bg_g = 0.11f, bg_b = 0.16f, bg_a = 0.98f;
    Config::parse_hex_color(Config::get().get_color_surface(), bg_r, bg_g, bg_b, bg_a);
    modal_card.set_bg_color(bg_r, bg_g, bg_b, bg_a);

    float br_r = 0.35f, br_g = 0.38f, br_b = 0.55f, br_a = 0.8f;
    Config::parse_hex_color(Config::get().get_color_outline(), br_r, br_g, br_b, br_a);
    modal_card.set_border(1, br_r, br_g, br_b, br_a);
    modal_card.set_corner_radius(12);
    modal_card.render(cr, m_modal_x, m_modal_y, m_modal_width, m_modal_height);

    // 3. Search Input Field
    int search_box_x = m_modal_x + 16;
    int search_box_y = m_modal_y + 14;
    int search_box_w = m_modal_width - 32;
    int search_box_h = 36;
    m_search_input.render(cr, search_box_x, search_box_y, search_box_w, search_box_h, true);

    // 4. Render Grid/List of Applications using TableView widget
    int palette_x = m_modal_x + 10;
    int palette_y = m_modal_y + 60;
    int palette_w = m_modal_width - 20;
    int palette_h = m_modal_height - 70;

    m_table_view->render(cr, palette_x, palette_y, palette_w, palette_h);

    // 5. Update scene buffer with damage to ensure repaint
    pixman_region32_t damage;
    pixman_region32_init_rect(&damage, 0, 0, m_screen_width, m_screen_height);
    wlr_scene_buffer_set_buffer_with_damage(m_scene_buffer, m_buffer->get_wlr_buffer(), &damage);
    pixman_region32_fini(&damage);
    wlr_scene_node_set_position(&m_scene_buffer->node, 0, 0);
}

} // namespace biway
