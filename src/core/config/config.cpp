#include "core/config/config.hpp"
#include "core/common/util.hpp"
#include <xkbcommon/xkbcommon-keysyms.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>

namespace miquland {

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

    // Material Design 3 Neon Light Theme Color Defaults
  m_theme_source = "theme/theme_mode.conf";
    m_color_primary = "#0066ff";
    m_color_on_primary = "#ffffff";
    m_color_primary_container = "#cce5ff";
    m_color_on_primary_container = "#002b66";
    m_color_secondary = "#e6f0fa";
    m_color_on_secondary = "#0f172a";
    m_color_background = "#f4f8fc";
    m_color_surface = "#ffffff";
    m_color_surface_variant = "#e6eff8";
    m_color_on_surface = "#0f172a";
    m_color_on_surface_variant = "#475569";
    m_color_outline = "#99c2ff";
    m_color_outline_variant = "#dbeafe";

    // Application Launchers
    m_keybindings.push_back({ mod, XKB_KEY_space, "menu", "Super+Space" });
    m_keybindings.push_back({ mod, XKB_KEY_f, "firefox", "Super+F" });
    m_keybindings.push_back({ mod, XKB_KEY_y, "kitty -e yazi || foot -e yazi || yazi", "Super+Y" });

    // Window Management & Layouts
    m_keybindings.push_back({ mod, XKB_KEY_q, "close", "Super+Q" });
    m_keybindings.push_back({ mod, XKB_KEY_l, "toggle_layout", "Super+L" });
    m_keybindings.push_back({ mod, XKB_KEY_Return, "swap_main", "Super+Return" });

    // Switch focus between windows in active workspace
    m_keybindings.push_back({ mod, XKB_KEY_j, "next_window", "Super+J" });
    m_keybindings.push_back({ mod, XKB_KEY_k, "prev_window", "Super+K" });
    m_keybindings.push_back({ mod, XKB_KEY_1, "focus_win_1", "Super+1" });
    m_keybindings.push_back({ mod, XKB_KEY_2, "focus_win_2", "Super+2" });
    m_keybindings.push_back({ mod, XKB_KEY_Left, "prev_window", "Super+Left" });
    m_keybindings.push_back({ mod, XKB_KEY_Right, "next_window", "Super+Right" });
    m_keybindings.push_back({ mod, XKB_KEY_Up, "prev_window", "Super+Up" });
    m_keybindings.push_back({ mod, XKB_KEY_Down, "next_window", "Super+Down" });

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
    if (hex.empty()) return false;
    std::string s = hex;
    // Strip surrounding whitespace and quotes
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '"' || s.front() == '\'')) {
        s.erase(0, 1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '"' || s.back() == '\'')) {
        s.pop_back();
    }
    // Strip trailing comment if present
    size_t comment = s.find('#', 1);
    if (comment != std::string::npos) {
        s = s.substr(0, comment);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    }

    if (s.empty()) return false;
    if (s[0] == '#') {
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
    } else if (s.length() == 3) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            r = (((val >> 8) & 0xF) * 17) / 255.0f;
            g = (((val >> 4) & 0xF) * 17) / 255.0f;
            b = ((val & 0xF) * 17) / 255.0f;
            a = 1.0f;
            return true;
        } catch (...) {
            return false;
        }
    } else if (s.length() == 4) {
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            r = (((val >> 12) & 0xF) * 17) / 255.0f;
            g = (((val >> 8) & 0xF) * 17) / 255.0f;
            b = (((val >> 4) & 0xF) * 17) / 255.0f;
            a = ((val & 0xF) * 17) / 255.0f;
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
        return std::string(xdg) + "/miquland";
    }
    const char* home = getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.config/miquland";
    }
    return "/tmp/miquland";
}

std::string Config::get_config_file_path() {
    return get_config_dir_path() + "/miquland.conf";
}

std::string Config::resolve_path(const std::string& path) const {
    if (path.empty()) return "";
    std::string p = path;

    // Expand ~ home directory
    if (p[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            p = std::string(home) + p.substr(1);
        }
    } else if (p[0] != '/') {
        // Relative path -> relative to config directory
        p = get_config_dir_path() + "/" + p;
    }
    return p;
}

void Config::ensure_default_files() {
    std::string dir = get_config_dir_path();
    std::string theme_dir = dir + "/theme";
    std::error_code ec;
    
    // Create ~/.config/miquland/ and ~/.config/miquland/theme/
    fs::create_directories(dir, ec);
    fs::create_directories(theme_dir, ec);

    // 1. Copy theme files (scan theme directories to ensure all files under theme of assets are copied)
    const std::vector<std::string> theme_source_dirs = {
        "assets/theme",
        "/usr/share/miquland/theme",
        "/usr/local/share/miquland/theme",
        "/etc/miquland/theme"
    };

    for (const auto& base_dir : theme_source_dirs) {
        if (fs::exists(base_dir, ec) && fs::is_directory(base_dir, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(base_dir, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) break;
                auto rel_path = fs::relative(entry.path(), base_dir, ec);
                if (ec) continue;
                auto dest_path = fs::path(theme_dir) / rel_path;

                if (entry.is_directory(ec)) {
                    fs::create_directories(dest_path, ec);
                } else if (entry.is_regular_file(ec)) {
                    if (!fs::exists(dest_path, ec)) {
                        fs::copy_file(entry.path(), dest_path, fs::copy_options::overwrite_existing, ec);
                    }
                }
            }
        }
    }

    // Fallback check for critical default theme files if directory iteration was unavailable
    const std::vector<std::string> fallback_theme_files = {
        "light.conf", "dark.conf", "theme_mode.conf",
        "darkmodescript.sh", "lightmodescript.sh"
    };
    for (const auto& file : fallback_theme_files) {
        std::string dest_path = theme_dir + "/" + file;
        if (!fs::exists(dest_path, ec)) {
            for (const auto& base_dir : theme_source_dirs) {
                std::string src_path = base_dir + "/" + file;
                if (fs::exists(src_path, ec)) {
                    fs::copy_file(src_path, dest_path, fs::copy_options::overwrite_existing, ec);
                    if (!ec) break;
                }
            }
        }
    }

    // 2. Copy main miquland.conf
    std::string config_path = get_config_file_path();
    if (!fs::exists(config_path)) {
        bool copied = false;
        for (const char* t_dir : {"assets", "/usr/share/miquland", "/usr/local/share/miquland", "/etc/miquland"}) {
            std::string cand = std::string(t_dir) + "/miquland.conf";
            if (fs::exists(cand)) {
                fs::copy_file(cand, config_path, fs::copy_options::overwrite_existing, ec);
                if (!ec) { copied = true; break; }
            }
        }
        // Absolute last resort if assets are completely missing
        if (!copied) save(); 
    }
}

static std::string resolve_exec_command(const std::string& raw_cmd) {
    std::string cmd = trim(raw_cmd);
    if (cmd.empty()) return "";

    // Find the first token (binary or script path)
    size_t space_pos = cmd.find_first_of(" \t");
    std::string first_token = (space_pos == std::string::npos) ? cmd : cmd.substr(0, space_pos);
    std::string rest = (space_pos == std::string::npos) ? "" : cmd.substr(space_pos);

    // If first_token is quoted, unquote for path checking
    bool is_quoted = false;
    char q = '\0';
    if (first_token.size() >= 2 && (first_token.front() == '"' || first_token.front() == '\'') && first_token.back() == first_token.front()) {
        is_quoted = true;
        q = first_token.front();
        first_token = first_token.substr(1, first_token.size() - 2);
    }

    // Check if first_token starts with ~
    if (first_token[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            first_token = std::string(home) + first_token.substr(1);
        }
    } else if (first_token[0] != '/') {
        // Check if it exists relative to the miquland config directory
        std::string config_rel = Config::get_config_dir_path() + "/" + first_token;
        if (fs::exists(config_rel)) {
            first_token = config_rel;
        }
    }

    // If it is a file on disk, make sure it can be executed
    if (fs::exists(first_token) && fs::is_regular_file(first_token)) {
        std::error_code ec;
        auto perms = fs::status(first_token, ec).permissions();
        if ((perms & fs::perms::owner_exec) == fs::perms::none &&
            (perms & fs::perms::group_exec) == fs::perms::none &&
            (perms & fs::perms::others_exec) == fs::perms::none) {
            return "sh \"" + first_token + "\"" + rest;
        }
        return "\"" + first_token + "\"" + rest;
    }

    if (is_quoted) {
        return std::string(1, q) + first_token + std::string(1, q) + rest;
    }
    return cmd;
}

void Config::load_file(const std::string& path, std::vector<KeyBinding>& file_bindings, bool& has_bindings_in_file,
                       std::vector<std::string>& file_exec_cmds, std::vector<std::string>& file_exec_once_cmds, int depth) {
    if (depth > 5) {
        log_error("Maximum config include depth exceeded for " + path);
        return;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        log_error("Could not open config file: " + path);
        return;
    }

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

        if (key != "bind" && key != "exec" && key != "exec_once" && key != "exec-once" &&
            key != "exec_always" && key != "exec-always" && key != "autostart") {
            size_t comment_pos = std::string::npos;
            if (!value.empty() && value[0] == '#') {
                size_t space_pos = value.find_first_of(" \t");
                if (space_pos != std::string::npos) {
                    comment_pos = value.find('#', space_pos);
                }
            } else {
                comment_pos = value.find('#');
            }

            if (comment_pos != std::string::npos) {
                value = trim(value.substr(0, comment_pos));
            }
        }

        if (key == "source" || key == "include") {
            m_theme_source = value;
            std::string resolved = resolve_path(value);
            if (!resolved.empty() && fs::exists(resolved)) {
                log_info("Sourcing configuration from " + resolved);
                load_file(resolved, file_bindings, has_bindings_in_file, file_exec_cmds, file_exec_once_cmds, depth + 1);
            } else {
                log_error("Config source file not found: " + value + " (resolved to " + resolved + ")");
            }
        } else if (key == "exec" || key == "exec_always" || key == "exec-always") {
            std::string resolved_cmd = resolve_exec_command(value);
            if (!resolved_cmd.empty()) {
                file_exec_cmds.push_back(resolved_cmd);
            }
        } else if (key == "exec_once" || key == "exec-once" || key == "autostart") {
            std::string resolved_cmd = resolve_exec_command(value);
            if (!resolved_cmd.empty()) {
                file_exec_once_cmds.push_back(resolved_cmd);
            }
        } else if (key == "tap_to_click") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            m_tap_to_click = (lower == "true" || lower == "1" || lower == "yes");
        } else if (key == "natural_scroll") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            m_natural_scroll = (lower == "true" || lower == "1" || lower == "yes");
        } else if (key == "icon_theme" || key == "icon-theme" || key == "icons_theme" ||
                   key == "icons-theme" || key == "icontheme" || key == "theme_icons" ||
                   key == "icon" || key == "icons") {
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
        } else if (key == "color_primary" || key == "primary" || key == "accent" ||
                   key == "window_border_color_active" || key == "border_color_active") {
            m_color_primary = value;
        } else if (key == "color_on_primary" || key == "on_primary") {
            m_color_on_primary = value;
        } else if (key == "color_primary_container" || key == "primary_container") {
            m_color_primary_container = value;
        } else if (key == "color_on_primary_container" || key == "on_primary_container") {
            m_color_on_primary_container = value;
        } else if (key == "color_secondary" || key == "secondary") {
            m_color_secondary = value;
        } else if (key == "color_on_secondary" || key == "on_secondary") {
            m_color_on_secondary = value;
        } else if (key == "color_background" || key == "background" || key == "bg_color") {
            m_color_background = value;
        } else if (key == "color_surface" || key == "surface" || key == "menu_bg") {
            m_color_surface = value;
        } else if (key == "color_surface_variant" || key == "surface_variant") {
            m_color_surface_variant = value;
        } else if (key == "color_on_surface" || key == "on_surface" || key == "text_color" || key == "text") {
            m_color_on_surface = value;
        } else if (key == "color_on_surface_variant" || key == "on_surface_variant" || key == "text_muted") {
            m_color_on_surface_variant = value;
        } else if (key == "color_outline" || key == "outline" || key == "border" ||
                   key == "window_border_color_inactive" || key == "border_color_inactive") {
            m_color_outline = value;
        } else if (key == "color_outline_variant" || key == "outline_variant") {
            m_color_outline_variant = value;
        } else if (key == "space_between_windows" || key == "window_spacing" || key == "inner_gap") {
            try {
                m_space_between_windows = std::max(0, std::stoi(value));
            } catch (...) {}
        } else if (key == "layout" || key == "tiling_layout" || key == "tiling_mode" || key == "layout_mode") {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "stack" || lower == "master" || lower == "main") {
                m_layout_mode = LayoutMode::Stack;
            } else {
                m_layout_mode = LayoutMode::Spiral;
            }
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
}

void Config::load() {
    ensure_default_files();

    std::string path = get_config_file_path();
    bool has_bindings_in_file = false;
    std::vector<KeyBinding> file_bindings;
    std::vector<std::string> file_exec_cmds;
    std::vector<std::string> file_exec_once_cmds;

    load_file(path, file_bindings, has_bindings_in_file, file_exec_cmds, file_exec_once_cmds, 0);

    if (has_bindings_in_file) {
        m_keybindings = std::move(file_bindings);
    }
    m_exec_commands = std::move(file_exec_cmds);
    m_exec_once_commands = std::move(file_exec_once_cmds);

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

    file << "# miquland configuration file\n\n";
    file << "[appearance]\n";
    file << "# Icon Theme (e.g. Papirus, Adwaita, Tela-circle; falls back to hicolor/pixmaps)\n";
    file << "icon_theme = " << m_icon_theme << "\n\n";

    file << "[theme]\n";
    file << "# Source a color theme file (e.g. source = light.conf, dark.conf, or absolute path)\n";
    file << "source = " << m_theme_source << "\n\n";

    file << "[input]\n";
    file << "tap_to_click = " << (m_tap_to_click ? "true" : "false") << "\n";
    file << "natural_scroll = " << (m_natural_scroll ? "true" : "false") << "\n\n";

    file << "[windows]\n";
    file << "layout = " << (m_layout_mode == LayoutMode::Stack ? "stack" : "spiral") << "\n";
    file << "window_border_width = " << m_window_border_width << "\n";
    file << "window_border_radius = " << m_window_border_radius << "\n";
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

} // namespace miquland
