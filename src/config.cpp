#include "config.hpp"
#include "util.hpp"
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
    load();
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
    std::string path = get_config_file_path();
    std::ifstream file(path);
    if (!file.is_open()) {
        log_info("No configuration file found at " + path + ", using defaults");
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
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
        }
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
    file << "wallpaper = " << m_wallpaper_path << "\n";
    file << "show_bar = " << (m_show_bar ? "true" : "false") << "\n";
    file << "bar_height = " << m_bar_height << "\n";

    log_info("Saved configuration to " + path);
}

void Config::set_wallpaper_path(const std::string& path) {
    m_wallpaper_path = path;
    save();
}

} // namespace biway
