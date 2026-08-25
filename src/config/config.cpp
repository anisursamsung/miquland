#include "config/config.hpp"
#include "common/util.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

namespace biway {

namespace fs = std::filesystem;

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"'");
    return str.substr(first, (last - first + 1));
}

Config& Config::get() {
    static Config instance;
    return instance;
}

Config::Config() {
    set_defaults();
    load();
}

void Config::set_defaults() {
    m_keybindings.clear();
    uint32_t mod = WLR_MODIFIER_LOGO;
    uint32_t mod_shift = WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT;
    uint32_t mod_alt = WLR_MODIFIER_LOGO | WLR_MODIFIER_ALT;

    const char* env_term = getenv("TERMINAL");
    std::string term = (env_term && *env_term) ? env_term : "kitty || foot || alacritty || wezterm || weston-terminal || xterm";
    m_terminal = term;

    // Application Launchers
    m_keybindings.push_back({ mod, XKB_KEY_space, "menu", "Super+Space" });
    m_keybindings.push_back({ mod, XKB_KEY_f, "firefox", "Super+F" });
    m_keybindings.push_back({ mod, XKB_KEY_y, "kitty -e yazi || foot -e yazi || yazi", "Super+Y" });

    // Window Management
    m_keybindings.push_back({ mod, XKB_KEY_q, "close", "Super+Q" });
    m_keybindings.push_back({ mod, XKB_KEY_b, "toggle_bar", "Super+B" });

    // Switch focus between windows in active workspace
    m_keybindings.push_back({ mod, XKB_KEY_1, "focus_win_1", "Super+1" });
    m_keybindings.push_back({ mod, XKB_KEY_2, "focus_win_2", "Super+2" });
    m_keybindings.push_back({ mod, XKB_KEY_Left, "toggle_focus", "Super+Left" });
    m_keybindings.push_back({ mod, XKB_KEY_Right, "toggle_focus", "Super+Right" });
    m_keybindings.push_back({ mod, XKB_KEY_Up, "toggle_focus", "Super+Up" });
    m_keybindings.push_back({ mod, XKB_KEY_Down, "toggle_focus", "Super+Down" });

    // Toggle between Horizontal (Left/Right) and Vertical (Top/Bottom) split
    m_keybindings.push_back({ mod_alt, XKB_KEY_space, "toggle_split", "Super+Alt+Space" });

    // Workspace Navigation (Super + Shift + ...)
    m_keybindings.push_back({ mod_shift, XKB_KEY_Left, "prev_ws", "Super+Shift+Left" });
    m_keybindings.push_back({ mod_shift, XKB_KEY_Right, "next_ws", "Super+Shift+Right" });

    // Jump directly to workspace 1..9
    for (int i = 1; i <= 9; ++i) {
        xkb_keysym_t sym = XKB_KEY_0 + i;
        m_keybindings.push_back({ mod_shift, sym, "ws_" + std::to_string(i), "Super+Shift+" + std::to_string(i) });
    }
}

void Config::add_or_update_binding(uint32_t mods, xkb_keysym_t sym, const std::string& action, const std::string& combo) {
    xkb_keysym_t norm_sym = (sym >= XKB_KEY_A && sym <= XKB_KEY_Z) ? (sym - XKB_KEY_A + XKB_KEY_a) : sym;

    for (auto& kb : m_keybindings) {
        xkb_keysym_t kb_norm = (kb.keysym >= XKB_KEY_A && kb.keysym <= XKB_KEY_Z) ? (kb.keysym - XKB_KEY_A + XKB_KEY_a) : kb.keysym;
        if (kb.modifiers == mods && kb_norm == norm_sym) {
            kb.action = action;
            kb.combo_str = combo;
            return;
        }
    }
    m_keybindings.push_back({ mods, norm_sym, action, combo });
}

bool Config::parse_binding_combo(const std::string& combo, uint32_t& out_modifiers, xkb_keysym_t& out_keysym) {
    out_modifiers = 0;
    out_keysym = XKB_KEY_NoSymbol;

    std::string s = combo;
    for (char& c : s) {
        if (c == '+' || c == '-' || c == ',') c = ' ';
    }

    std::stringstream ss(s);
    std::string item;
    std::vector<std::string> parts;
    while (ss >> item) {
        item = trim(item);
        if (!item.empty()) {
            parts.push_back(item);
        }
    }

    if (parts.empty()) return false;

    std::string key_part = parts.back();
    parts.pop_back();

    for (const auto& mod_name : parts) {
        std::string m = mod_name;
        std::transform(m.begin(), m.end(), m.begin(), ::tolower);
        if (m == "super" || m == "logo" || m == "mod4" || m == "win") {
            out_modifiers |= WLR_MODIFIER_LOGO;
        } else if (m == "shift") {
            out_modifiers |= WLR_MODIFIER_SHIFT;
        } else if (m == "ctrl" || m == "control") {
            out_modifiers |= WLR_MODIFIER_CTRL;
        } else if (m == "alt" || m == "mod1") {
            out_modifiers |= WLR_MODIFIER_ALT;
        }
    }

    out_keysym = xkb_keysym_from_name(key_part.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    if (out_keysym == XKB_KEY_NoSymbol) {
        std::string lower = key_part;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "return" || lower == "enter") out_keysym = XKB_KEY_Return;
        else if (lower == "space") out_keysym = XKB_KEY_space;
        else if (lower == "tab") out_keysym = XKB_KEY_Tab;
        else if (lower == "escape" || lower == "esc") out_keysym = XKB_KEY_Escape;
        else if (lower == "backspace") out_keysym = XKB_KEY_BackSpace;
        else if (lower == "left") out_keysym = XKB_KEY_Left;
        else if (lower == "right") out_keysym = XKB_KEY_Right;
        else if (lower == "up") out_keysym = XKB_KEY_Up;
        else if (lower == "down") out_keysym = XKB_KEY_Down;
        else if (lower == "audioplay" || lower == "play") out_keysym = XKB_KEY_XF86AudioPlay;
        else if (lower == "audionext" || lower == "next") out_keysym = XKB_KEY_XF86AudioNext;
        else if (lower == "audioprev" || lower == "prev") out_keysym = XKB_KEY_XF86AudioPrev;
        else if (lower == "audiostop" || lower == "stop") out_keysym = XKB_KEY_XF86AudioStop;
        else if (lower == "audiomute" || lower == "mute") out_keysym = XKB_KEY_XF86AudioMute;
        else if (lower == "audiolowervolume" || lower == "voldown" || lower == "volumedown") out_keysym = XKB_KEY_XF86AudioLowerVolume;
        else if (lower == "audioraisevolume" || lower == "volup" || lower == "volumeup") out_keysym = XKB_KEY_XF86AudioRaiseVolume;
        else if (lower == "monbrightnessup" || lower == "brightnessup" || lower == "brightup") out_keysym = XKB_KEY_XF86MonBrightnessUp;
        else if (lower == "monbrightnessdown" || lower == "brightnessdown" || lower == "brightdown") out_keysym = XKB_KEY_XF86MonBrightnessDown;
        else if (key_part.length() == 1) {
            out_keysym = static_cast<xkb_keysym_t>(key_part[0]);
        }
    }

    if (out_keysym >= XKB_KEY_A && out_keysym <= XKB_KEY_Z) {
        out_keysym = (out_keysym - XKB_KEY_A) + XKB_KEY_a;
    }

    return (out_keysym != XKB_KEY_NoSymbol);
}

bool Config::parse_hex_color(const std::string& hex, float& r, float& g, float& b, float& a) {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }
    if (s.length() == 6) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            r = ((val >> 16) & 0xFF) / 255.0f;
            g = ((val >> 8) & 0xFF) / 255.0f;
            b = (val & 0xFF) / 255.0f;
            a = 1.0f;
            return true;
        } catch (...) {
            return false;
        }
    } else if (s.length() == 8) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            r = ((val >> 24) & 0xFF) / 255.0f;
            g = ((val >> 16) & 0xFF) / 255.0f;
            b = ((val >> 8) & 0xFF) / 255.0f;
            a = (val & 0xFF) / 255.0f;
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

std::string Config::get_config_dir_path() {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        return std::string(xdg) + "/biway";
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/biway";
    }
    return "/tmp/biway";
}

std::string Config::get_config_file_path() {
    return get_config_dir_path() + "/biway.conf";
}

void Config::load() {
    std::string dir = get_config_dir_path();
    std::string path = get_config_file_path();

    if (!fs::exists(path)) {
        log_info("Configuration file not found at " + path + ". Checking asset templates...");
        std::error_code ec;
        fs::create_directories(dir, ec);

        // Check for template asset files in system and local assets
        const std::vector<std::string> template_candidates = {
            "/usr/share/biway/biway.conf",
            "/usr/local/share/biway/biway.conf",
            "/etc/biway/biway.conf",
            "assets/biway.conf"
        };

        bool copied = false;
        for (const auto& candidate : template_candidates) {
            if (fs::exists(candidate)) {
                fs::copy_file(candidate, path, fs::copy_options::overwrite_existing, ec);
                if (!ec) {
                    log_info("Copied default configuration asset from " + candidate + " to " + path);
                    copied = true;
                    break;
                }
            }
        }

        if (!copied) {
            save();
        }
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        log_info("No configuration file found at " + path + ", using defaults");
        return;
    }

    bool has_bindings_in_file = false;
    std::vector<KeyBinding> file_bindings;

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == '[') {
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string value = trim(trimmed.substr(eq_pos + 1));

        if (key == "wallpaper") {
            m_wallpaper_path = value;
        } else if (key == "show_bar") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            m_show_bar = (lower == "true" || lower == "1" || lower == "yes");
        } else if (key == "bar_height") {
            try {
                m_bar_height = std::stoi(value);
            } catch (...) {}
        } else if (key == "tap_to_click") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            m_tap_to_click = (lower == "true" || lower == "1" || lower == "yes");
        } else if (key == "natural_scroll") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            m_natural_scroll = (lower == "true" || lower == "1" || lower == "yes");
        } else if (key == "icon_theme") {
            m_icon_theme = value;
        } else if (key == "terminal") {
            m_terminal = value;
        } else if (key == "window_border_width" || key == "border_width") {
            try {
                m_window_border_width = std::max(0, std::stoi(value));
            } catch (...) {}
        } else if (key == "window_border_radius" || key == "border_radius") {
            try {
                m_window_border_radius = std::max(0, std::stoi(value));
            } catch (...) {}
        } else if (key == "window_border_color_active" || key == "border_color_active") {
            m_window_border_color_active = value;
        } else if (key == "window_border_color_inactive" || key == "border_color_inactive") {
            m_window_border_color_inactive = value;
        } else if (key == "space_between_windows" || key == "window_spacing" || key == "inner_gap") {
            try {
                m_space_between_windows = std::max(0, std::stoi(value));
            } catch (...) {}
        } else if (key == "screen_edge_padding" || key == "screen_padding" || key == "outer_gap") {
            try {
                m_screen_edge_padding = std::max(0, std::stoi(value));
            } catch (...) {}
        } else if (key == "bind") {
            // Format: bind = Super+F, firefox or bind = Super, K, kitty
            size_t comma = value.rfind(',');
            if (comma != std::string::npos) {
                std::string combo = trim(value.substr(0, comma));
                std::string action = trim(value.substr(comma + 1));
                uint32_t mods = 0;
                xkb_keysym_t sym = XKB_KEY_NoSymbol;
                if (parse_binding_combo(combo, mods, sym)) {
                    has_bindings_in_file = true;
                    xkb_keysym_t norm_sym = (sym >= XKB_KEY_A && sym <= XKB_KEY_Z) ? (sym - XKB_KEY_A + XKB_KEY_a) : sym;
                    file_bindings.push_back({ mods, norm_sym, action, combo });
                }
            }
        }
    }

    if (has_bindings_in_file) {
        m_keybindings = std::move(file_bindings);
    }

    log_info("Loaded configuration from " + path);
}

void Config::save() {
    std::string dir = get_config_dir_path();
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::string path = get_config_file_path();
    std::ofstream file(path);
    if (!file.is_open()) {
        log_error("Failed to write configuration file at " + path);
        return;
    }

    file << "# biway configuration file\n\n";
    file << "[appearance]\n";
    file << "wallpaper = " << m_wallpaper_path << "\n";
    file << "show_bar = " << (m_show_bar ? "true" : "false") << "\n";
    file << "bar_height = " << m_bar_height << "\n";
    file << "icon_theme = " << m_icon_theme << "\n\n";

    file << "[input]\n";
    file << "tap_to_click = " << (m_tap_to_click ? "true" : "false") << "\n";
    file << "natural_scroll = " << (m_natural_scroll ? "true" : "false") << "\n\n";

    file << "[windows]\n";
    file << "window_border_width = " << m_window_border_width << "\n";
    file << "window_border_radius = " << m_window_border_radius << "\n";
    file << "window_border_color_active = " << m_window_border_color_active << "\n";
    file << "window_border_color_inactive = " << m_window_border_color_inactive << "\n";
    file << "space_between_windows = " << m_space_between_windows << "\n";
    file << "screen_edge_padding = " << m_screen_edge_padding << "\n\n";

    file << "[applications]\n";
    file << "terminal = " << m_terminal << "\n\n";

    file << "[keybindings]\n";
    file << "# Keybindings format: bind = Combo, command_or_action\n";
    for (const auto& kb : m_keybindings) {
        file << "bind = " << kb.combo_str << ", " << kb.action << "\n";
    }

    log_info("Saved configuration to " + path);
}

void Config::set_wallpaper_path(const std::string& path) {
    m_wallpaper_path = path;
    save();
}

} // namespace biway
