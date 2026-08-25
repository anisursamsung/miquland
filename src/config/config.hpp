#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <xkbcommon/xkbcommon.h>

namespace biway {

struct KeyBinding {
    uint32_t modifiers = 0;
    xkb_keysym_t keysym = XKB_KEY_NoSymbol;
    std::string action; // e.g. "firefox", "foot -e yazi", or built-in "menu", "close", "toggle_bar", "prev_window", "next_window", "ws_1", etc.
    std::string combo_str;
};

class Config {
public:
    static Config& get();

    void load();
    void save();

    const std::string& get_wallpaper_path() const { return m_wallpaper_path; }
    void set_wallpaper_path(const std::string& path);

    bool is_bar_visible() const { return m_show_bar; }
    void set_bar_visible(bool visible) { m_show_bar = visible; }

    int get_bar_height() const { return m_bar_height; }

    bool is_tap_to_click_enabled() const { return m_tap_to_click; }
    void set_tap_to_click_enabled(bool enabled) { m_tap_to_click = enabled; }

    bool is_natural_scroll_enabled() const { return m_natural_scroll; }
    void set_natural_scroll_enabled(bool enabled) { m_natural_scroll = enabled; }

    const std::string& get_icon_theme() const { return m_icon_theme; }
    void set_icon_theme(const std::string& theme) { m_icon_theme = theme; }

    const std::string& get_terminal() const { return m_terminal; }
    void set_terminal(const std::string& term) { m_terminal = term; }

    int get_window_border_width() const { return m_window_border_width; }
    void set_window_border_width(int w) { m_window_border_width = w; }

    int get_window_border_radius() const { return m_window_border_radius; }
    void set_window_border_radius(int r) { m_window_border_radius = r; }

    const std::string& get_window_border_color_active() const { return m_window_border_color_active; }
    void set_window_border_color_active(const std::string& color) { m_window_border_color_active = color; }

    const std::string& get_window_border_color_inactive() const { return m_window_border_color_inactive; }
    void set_window_border_color_inactive(const std::string& color) { m_window_border_color_inactive = color; }

    int get_space_between_windows() const { return m_space_between_windows; }
    void set_space_between_windows(int space) { m_space_between_windows = space; }

    int get_screen_edge_padding() const { return m_screen_edge_padding; }
    void set_screen_edge_padding(int pad) { m_screen_edge_padding = pad; }

    static bool parse_hex_color(const std::string& hex, float& r, float& g, float& b, float& a);

    const std::vector<KeyBinding>& get_keybindings() const { return m_keybindings; }

    static std::string get_config_file_path();
    static std::string get_config_dir_path();

    static bool parse_binding_combo(const std::string& combo, uint32_t& out_modifiers, xkb_keysym_t& out_keysym);
    void add_or_update_binding(uint32_t mods, xkb_keysym_t sym, const std::string& action, const std::string& combo);

private:
    Config();
    void set_defaults();

    std::string m_wallpaper_path;
    bool m_show_bar = true;
    int m_bar_height = 30;
    bool m_tap_to_click = true;
    bool m_natural_scroll = true;
    std::string m_icon_theme = "hicolor";
    std::string m_terminal = "foot";

    int m_window_border_width = 2;
    int m_window_border_radius = 8;
    std::string m_window_border_color_active = "#00d2ff";
    std::string m_window_border_color_inactive = "#2a2a36";
    int m_space_between_windows = 8;
    int m_screen_edge_padding = 10;

    std::vector<KeyBinding> m_keybindings;
};

} // namespace biway
