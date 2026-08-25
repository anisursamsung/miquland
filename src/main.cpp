#include "server.hpp"
#include "config.hpp"
#include <csignal>
#include <iostream>
#include <string>

static biway::Server* g_server = nullptr;

static void handle_signal(int sig) {
    if (g_server) {
        g_server->terminate();
    }
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --setwallpaper <path>  Set wallpaper image path and update ~/.config/biway/biway.conf\n"
              << "  --no-bar               Start with the top status/menu bar hidden\n"
              << "  -s, --startup <cmd>    Execute startup command after compositor initializes\n"
              << "  -h, --help             Show this help message\n\n"
              << "Configuration File:\n"
              << "  ~/.config/biway/biway.conf\n"
              << "  (Supports: wallpaper = /path/to/img.png, show_bar = true/false, bar_height = 30)\n\n"
              << "Keybindings:\n"
              << "  Super + Return         Spawn terminal ($TERMINAL, foot, alacritty, kitty)\n"
              << "  Super + D / Space      Spawn launcher (fuzzel, wofi, bemenu-run)\n"
              << "  Super + B              Toggle top status/menu bar\n"
              << "  Super + Q              Close active window\n"
              << "  Super + H / Left       Focus left / previous window\n"
              << "  Super + L / Right/Tab  Focus right / next window\n"
              << "  Super + [1..9]         Switch to workspace 1..9\n"
              << "  Super + Shift + [1..9] Move focused window to workspace 1..9\n"
              << "  Super + Shift + E      Exit biway compositor\n\n"
              << "Touchpad Gestures:\n"
              << "  3-Finger Swipe Left    Switch to next workspace\n"
              << "  3-Finger Swipe Right   Switch to previous workspace\n";
}

int main(int argc, char* argv[]) {
    std::string startup_cmd;
    bool set_wallpaper_only = false;

    // Load configuration file
    biway::Config::get().load();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--setwallpaper" && i + 1 < argc) {
            std::string wp_path = argv[++i];
            biway::Config::get().set_wallpaper_path(wp_path);
            std::cout << "biway: wallpaper configuration updated to " << wp_path << std::endl;
            if (argc == 3) {
                // If only --setwallpaper was passed, exit after updating configuration
                set_wallpaper_only = true;
            }
        } else if (arg == "--no-bar") {
            biway::Config::get().set_bar_visible(false);
        } else if ((arg == "-s" || arg == "--startup") && i + 1 < argc) {
            startup_cmd = argv[++i];
        }
    }

    if (set_wallpaper_only) {
        return 0;
    }

    biway::Server server;
    g_server = &server;

    struct sigaction sa = {};
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    if (!startup_cmd.empty()) {
        server.set_startup_command(startup_cmd);
    }

    if (!server.init()) {
        std::cerr << "biway: failed to initialize server\n";
        return 1;
    }

    server.run();

    return 0;
}
