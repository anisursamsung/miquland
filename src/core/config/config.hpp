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
    void set_wallpaper_path(const std::string& path) { m_wallpaper_path = path; }

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

    const std::string& get_window_border_color_active() const { return m_color_primary; }
    void set_window_border_color_active(const std::string& color) { m_color_primary = color; }

    const std::string& get_window_border_color_inactive() const { return m_color_outline; }
    void set_window_border_color_inactive(const std::string& color) { m_color_outline = color; }

    // Material Color Roles
    const std::string& get_color_primary() const { return m_color_primary; }
    void set_color_primary(const std::string& c) { m_color_primary = c; }

    const std::string& get_color_on_primary() const { return m_color_on_primary; }
    void set_color_on_primary(const std::string& c) { m_color_on_primary = c; }

    const std::string& get_color_primary_container() const { return m_color_primary_container; }
    void set_color_primary_container(const std::string& c) { m_color_primary_container = c; }

    const std::string& get_color_on_primary_container() const { return m_color_on_primary_container; }
    void set_color_on_primary_container(const std::string& c) { m_color_on_primary_container = c; }

    const std::string& get_color_secondary() const { return m_color_secondary; }
    void set_color_secondary(const std::string& c) { m_color_secondary = c; }

    const std::string& get_color_on_secondary() const { return m_color_on_secondary; }
    void set_color_on_secondary(const std::string& c) { m_color_on_secondary = c; }

    const std::string& get_color_background() const { return m_color_background; }
    void set_color_background(const std::string& c) { m_color_background = c; }

    const std::string& get_color_surface() const { return m_color_surface; }
    void set_color_surface(const std::string& c) { m_color_surface = c; }

    const std::string& get_color_surface_variant() const { return m_color_surface_variant; }
    void set_color_surface_variant(const std::string& c) { m_color_surface_variant = c; }

    const std::string& get_color_on_surface() const { return m_color_on_surface; }
    void set_color_on_surface(const std::string& c) { m_color_on_surface = c; }

    const std::string& get_color_on_surface_variant() const { return m_color_on_surface_variant; }
    void set_color_on_surface_variant(const std::string& c) { m_color_on_surface_variant = c; }

    const std::string& get_color_outline() const { return m_color_outline; }
    void set_color_outline(const std::string& c) { m_color_outline = c; }

    const std::string& get_color_outline_variant() const { return m_color_outline_variant; }
    void set_color_outline_variant(const std::string& c) { m_color_outline_variant = c; }

    const std::string& get_theme_source() const { return m_theme_source; }
    void set_theme_source(const std::string& src) { m_theme_source = src; }

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
    void ensure_default_files();
    void load_file(const std::string& path, std::vector<KeyBinding>& file_bindings, bool& has_bindings_in_file, int depth = 0);
    std::string resolve_path(const std::string& path) const;

    std::string m_wallpaper_path;
    bool m_show_bar = true;
    int m_bar_height = 30;
    bool m_tap_to_click = true;
    bool m_natural_scroll = true;
    std::string m_icon_theme = "Papirus";
    std::string m_terminal = "foot";
    std::string m_theme_source = "~/.config/biway/light.conf";

    int m_window_border_width = 2;
    int m_window_border_radius = 8;
    int m_space_between_windows = 8;
    int m_screen_edge_padding = 10;

    // Material Design 3 Palette
    std::string m_color_primary = "#0066ff";
    std::string m_color_on_primary = "#ffffff";
    std::string m_color_primary_container = "#cce5ff";
    std::string m_color_on_primary_container = "#002b66";
    std::string m_color_secondary = "#e6f0fa";
    std::string m_color_on_secondary = "#0f172a";
    std::string m_color_background = "#f4f8fc";
    std::string m_color_surface = "#ffffff";
    std::string m_color_surface_variant = "#e6eff8";
    std::string m_color_on_surface = "#0f172a";
    std::string m_color_on_surface_variant = "#475569";
    std::string m_color_outline = "#99c2ff";
    std::string m_color_outline_variant = "#dbeafe";

    std::vector<KeyBinding> m_keybindings;
};

} // namespace biway
