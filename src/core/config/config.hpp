#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server-protocol.h>

namespace miquland {

struct MonitorRule {
    std::string name;             // eDP-1, HDMI-A-1, or "" / "*" for default
    bool disabled = false;        // true if "disable"
    int width = 0;                // 0 = preferred
    int height = 0;
    double refresh_rate = 0.0;    // 0 = preferred, e.g. 60.0
    int x = -1;                   // -1 = auto placement
    int y = -1;
    double scale = 1.0;
    enum wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;
};

struct KeyBinding {
    uint32_t modifiers = 0;
    xkb_keysym_t keysym = XKB_KEY_NoSymbol;
    std::string action; // e.g. "firefox", "foot -e yazi", or built-in "menu", "close", "toggle_bar", "prev_window", "next_window", "ws_1", etc.
    std::string combo_str;
};

struct GestureBinding {
    std::string pattern; // e.g. "swipe:3:left"
    std::string action;  // e.g. "next_ws"
};

struct WindowRule {
    std::string rule;    // e.g. "float", "workspace", "opacity"
    std::string target;  // app_id or title
    std::string extra;   // e.g. workspace id or opacity float string
};

class Config {
public:
    static Config& get();

    void load();
    void save();

    bool is_focus_follows_mouse_enabled() const { return m_focus_follows_mouse; }
    void set_focus_follows_mouse_enabled(bool enabled) { m_focus_follows_mouse = enabled; }

    bool is_smart_gaps_enabled() const { return m_smart_gaps; }
    void set_smart_gaps_enabled(bool enabled) { m_smart_gaps = enabled; }

    bool is_xwayland_force_zero_scaling_enabled() const { return m_xwayland_force_zero_scaling; }
    void set_xwayland_force_zero_scaling_enabled(bool enabled) { m_xwayland_force_zero_scaling = enabled; }

    const std::string& get_cursor_theme() const { return m_cursor_theme; }
    void set_cursor_theme(const std::string& theme) { m_cursor_theme = theme; }

    int get_cursor_size() const { return m_cursor_size; }
    void set_cursor_size(int size) { m_cursor_size = std::clamp(size, 8, 128); }

    double get_default_split_ratio() const { return m_default_split_ratio; }
    void set_default_split_ratio(double ratio) { m_default_split_ratio = std::clamp(ratio, 0.1, 0.9); }

    bool is_workspace_cycle_enabled() const { return m_workspace_cycle; }
    void set_workspace_cycle_enabled(bool enabled) { m_workspace_cycle = enabled; }

    bool is_tap_to_click_enabled() const { return m_tap_to_click; }
    void set_tap_to_click_enabled(bool enabled) { m_tap_to_click = enabled; }

    bool is_natural_scroll_enabled() const { return m_natural_scroll; }
    void set_natural_scroll_enabled(bool enabled) { m_natural_scroll = enabled; }

    bool is_dwt_enabled() const { return m_dwt; }
    void set_dwt_enabled(bool enabled) { m_dwt = enabled; }

    double get_accel_speed() const { return m_accel_speed; }
    void set_accel_speed(double speed) { m_accel_speed = std::clamp(speed, -1.0, 1.0); }

    const std::string& get_accel_profile() const { return m_accel_profile; }
    void set_accel_profile(const std::string& profile) { m_accel_profile = profile; }

    const std::string& get_touch_output() const { return m_touch_output; }
    void set_touch_output(const std::string& output) { m_touch_output = output; }

    const std::string& get_kb_layout() const { return m_kb_layout; }
    void set_kb_layout(const std::string& l) { m_kb_layout = l; }

    const std::string& get_kb_variant() const { return m_kb_variant; }
    void set_kb_variant(const std::string& v) { m_kb_variant = v; }

    const std::string& get_kb_options() const { return m_kb_options; }
    void set_kb_options(const std::string& opt) { m_kb_options = opt; }

    const std::string& get_kb_model() const { return m_kb_model; }
    void set_kb_model(const std::string& m) { m_kb_model = m; }

    int get_repeat_rate() const { return m_repeat_rate; }
    void set_repeat_rate(int rate) { m_repeat_rate = std::max(1, rate); }

    int get_repeat_delay() const { return m_repeat_delay; }
    void set_repeat_delay(int delay) { m_repeat_delay = std::max(100, delay); }

    const std::string& get_terminal() const { return m_terminal; }
    void set_terminal(const std::string& term) { m_terminal = term; }

    int get_window_border_width() const { return m_window_border_width; }
    void set_window_border_width(int w) { m_window_border_width = w; }

    int get_window_border_radius() const { return m_window_border_radius; }
    void set_window_border_radius(int r) { m_window_border_radius = r; }

    bool is_resize_on_border_enabled() const { return m_resize_on_border; }
    void set_resize_on_border_enabled(bool enabled) { m_resize_on_border = enabled; }

    int get_border_grab_area() const { return m_border_grab_area; }
    void set_border_grab_area(int area) { m_border_grab_area = area; }

    const std::string& get_window_border_color_active() const { return m_window_border_color_active; }
    void set_window_border_color_active(const std::string& color) { m_window_border_color_active = color; }

    const std::string& get_window_border_color_inactive() const { return m_window_border_color_inactive; }
    void set_window_border_color_inactive(const std::string& color) { m_window_border_color_inactive = color; }

    enum class LayoutMode {
        Spiral, // Recursive binary space partitioning (Fibonacci / BSP)
        Stack   // Main prominent window on left, vertical stack on right
    };

    LayoutMode get_layout_mode() const { return m_layout_mode; }
    void set_layout_mode(LayoutMode mode) { m_layout_mode = mode; }
    void toggle_layout_mode() { m_layout_mode = (m_layout_mode == LayoutMode::Spiral) ? LayoutMode::Stack : LayoutMode::Spiral; }

    int get_space_between_windows() const { return m_space_between_windows; }
    void set_space_between_windows(int space) { m_space_between_windows = space; }

    int get_screen_edge_padding() const { return m_screen_edge_padding; }
    void set_screen_edge_padding(int pad) { m_screen_edge_padding = pad; }

    float get_window_opacity_active() const { return m_window_opacity_active; }
    void set_window_opacity_active(float op) { m_window_opacity_active = std::clamp(op, 0.0f, 1.0f); }

    float get_window_opacity_inactive() const { return m_window_opacity_inactive; }
    void set_window_opacity_inactive(float op) { m_window_opacity_inactive = std::clamp(op, 0.0f, 1.0f); }

    bool is_blur_enabled() const { return m_blur_enabled; }
    void set_blur_enabled(bool enabled) { m_blur_enabled = enabled; }

    int get_blur_radius() const { return m_blur_radius; }
    void set_blur_radius(int r) { m_blur_radius = r; }

    int get_blur_num_passes() const { return m_blur_num_passes; }
    void set_blur_num_passes(int p) { m_blur_num_passes = p; }

    float get_blur_noise() const { return m_blur_noise; }
    void set_blur_noise(float n) { m_blur_noise = n; }

    float get_blur_brightness() const { return m_blur_brightness; }
    void set_blur_brightness(float b) { m_blur_brightness = b; }

    float get_blur_contrast() const { return m_blur_contrast; }
    void set_blur_contrast(float c) { m_blur_contrast = c; }

    float get_blur_saturation() const { return m_blur_saturation; }
    void set_blur_saturation(float s) { m_blur_saturation = s; }

    bool is_layer_blur_enabled(const std::string& ns) const;
    void add_blurred_layer(const std::string& ns);
    const std::vector<std::string>& get_blurred_layers() const { return m_blurred_layers; }

    static bool parse_hex_color(const std::string& hex, float& r, float& g, float& b, float& a);

    const std::vector<KeyBinding>& get_keybindings() const { return m_keybindings; }
    const std::vector<GestureBinding>& get_gesture_bindings() const { return m_gesture_bindings; }
    std::string find_gesture_action(const std::string& pattern) const;
    bool has_gesture_for_fingers(int fingers) const;
    void add_or_update_gesture_binding(const std::string& pattern, const std::string& action);
    double get_swipe_threshold() const { return m_swipe_threshold; }
    void set_swipe_threshold(double threshold) { m_swipe_threshold = threshold; }

    const std::vector<WindowRule>& get_window_rules() const { return m_window_rules; }
    void add_window_rule(const WindowRule& rule) { m_window_rules.push_back(rule); }
    bool should_float(const std::string& app_id, const std::string& title) const;
    int get_target_workspace(const std::string& app_id, const std::string& title) const;
    float get_rule_opacity(const std::string& app_id, const std::string& title, float default_val) const;

    const std::vector<MonitorRule>& get_monitor_rules() const { return m_monitor_rules; }
    void add_monitor_rule(const MonitorRule& rule) { m_monitor_rules.push_back(rule); }
    const MonitorRule* find_monitor_rule(const std::string& name) const;

    const std::vector<std::string>& get_exec_commands() const { return m_exec_commands; }
    const std::vector<std::string>& get_exec_once_commands() const { return m_exec_once_commands; }
    const std::vector<std::string>& get_plugins() const { return m_plugins; }

    static std::string get_config_file_path();
    static std::string get_config_dir_path();

    static bool parse_binding_combo(const std::string& combo, uint32_t& out_modifiers, xkb_keysym_t& out_keysym);
    void add_or_update_binding(uint32_t mods, xkb_keysym_t sym, const std::string& action, const std::string& combo);

private:
    Config();
    void set_defaults();
    void ensure_default_files();
    void load_file(const std::string& path, std::vector<KeyBinding>& file_bindings, bool& has_bindings_in_file,
                   std::vector<GestureBinding>& file_gestures, bool& has_gestures_in_file,
                   std::vector<WindowRule>& file_rules, bool& has_rules_in_file,
                   std::vector<MonitorRule>& file_monitors, bool& has_monitors_in_file,
                   std::vector<std::string>& file_exec_cmds, std::vector<std::string>& file_exec_once_cmds, int depth = 0);
    std::string resolve_path(const std::string& path) const;

    std::vector<MonitorRule> m_monitor_rules;
    std::vector<std::string> m_exec_commands;
    std::vector<std::string> m_exec_once_commands;

    bool m_focus_follows_mouse = true;
    bool m_smart_gaps = false;
    bool m_xwayland_force_zero_scaling = false;
    std::string m_cursor_theme = "";
    int m_cursor_size = 24;
    double m_default_split_ratio = 0.5;
    bool m_workspace_cycle = true;

    bool m_tap_to_click = true;
    bool m_natural_scroll = true;
    bool m_dwt = true;
    double m_accel_speed = 0.0;
    std::string m_accel_profile = "adaptive";
    std::string m_touch_output = "";
    std::string m_kb_layout = "us";
    std::string m_kb_variant = "";
    std::string m_kb_options = "";
    std::string m_kb_model = "";
    int m_repeat_rate = 25;
    int m_repeat_delay = 600;
    std::string m_terminal = "foot";

    int m_window_border_width = 2;
    int m_window_border_radius = 8;
    bool m_resize_on_border = true;
    int m_border_grab_area = 5;
    int m_space_between_windows = 8;
    int m_screen_edge_padding = 10;
    float m_window_opacity_active = 1.0f;
    float m_window_opacity_inactive = 0.85f;
    bool m_blur_enabled = true;
    int m_blur_radius = 5;
    int m_blur_num_passes = 3;
    float m_blur_noise = 0.02f;
    float m_blur_brightness = 0.9f;
    float m_blur_contrast = 0.9f;
    float m_blur_saturation = 1.1f;
    std::vector<std::string> m_blurred_layers;
    LayoutMode m_layout_mode = LayoutMode::Spiral;

    std::string m_window_border_color_active = "#0066ff";
    std::string m_window_border_color_inactive = "#99c2ff";

    std::vector<KeyBinding> m_keybindings;
    double m_swipe_threshold = 50.0;
    std::vector<GestureBinding> m_gesture_bindings;
    std::vector<WindowRule> m_window_rules;
    std::vector<std::string> m_plugins;
};

} // namespace miquland
