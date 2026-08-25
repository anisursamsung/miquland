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

    std::vector<KeyBinding> m_keybindings;
};

} // namespace biway
