#include "server.hpp"
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
              << "  -s, --startup <cmd>    Execute startup command after compositor initializes\n"
              << "  -h, --help             Show this help message\n\n"
              << "Keybindings:\n"
              << "  Super + Return         Spawn terminal ($TERMINAL, foot, alacritty, kitty)\n"
              << "  Super + D / Space      Spawn launcher (fuzzel, wofi, bemenu-run)\n"
              << "  Super + Q              Close active window\n"
              << "  Super + H / Left       Focus left / previous window\n"
              << "  Super + L / Right/Tab  Focus right / next window\n"
              << "  Super + [1..9]         Switch to workspace 1..9\n"
              << "  Super + Shift + [1..9] Move focused window to workspace 1..9\n"
              << "  Super + Shift + E      Exit biway compositor\n";
}

int main(int argc, char* argv[]) {
    std::string startup_cmd;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
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
