#pragma once

#include "util.hpp"
#include "cairo_buffer.hpp"
#include <string>
#include <memory>

namespace biway {

class Server;

class Wallpaper {
public:
    explicit Wallpaper(Server* server);
    ~Wallpaper();

    void render(int width, int height);
    void set_wallpaper(const std::string& path);

private:
    Server* m_server = nullptr;
    struct wlr_scene_buffer* m_scene_buffer = nullptr;
    std::unique_ptr<CairoBuffer> m_buffer;
    int m_width = 0;
    int m_height = 0;
};

} // namespace biway
