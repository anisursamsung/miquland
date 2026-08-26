#include "core/server.hpp"
#include "config/config.hpp"
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
              << "  --no-bar               Start with the top status/menu bar hidden\n"
              << "  -s, --startup <cmd>    Execute startup command after compositor initializes\n"
              << "  -h, --help             Show this help message\n\n"
              << "Configuration File:\n"
              << "  ~/.config/biway/biway.conf\n\n"
              << "Keybindings:\n"
              << "  Super + Return         Spawn terminal ($TERMINAL, foot, alacritty, kitty)\n"
              << "  Super + Space          Spawn application menu launcher\n"
              << "  Super + B              Toggle top status/menu bar\n"
              << "  Super + Q              Close active window\n"
              << "  Super + [1..9]         Switch to workspace 1..9\n"
              << "  Super + Shift + Q      Exit biway compositor\n\n";
}

int main(int argc, char* argv[]) {
    std::string startup_cmd;

    // Load configuration file
    biway::Config::get().load();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--no-bar") {
            biway::Config::get().set_bar_visible(false);
        } else if ((arg == "-s" || arg == "--startup") && i + 1 < argc) {
            startup_cmd = argv[++i];
        }
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
