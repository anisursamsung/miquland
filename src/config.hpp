#pragma once

#include <string>
#include <map>

namespace biway {

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

    static std::string get_config_file_path();
    static std::string get_config_dir_path();

private:
    Config();

    std::string m_wallpaper_path;
    bool m_show_bar = true;
    int m_bar_height = 30;
};

} // namespace biway
