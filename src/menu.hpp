#pragma once

#include "util.hpp"
#include "cairo_buffer.hpp"
#include "palette/model_list_item.hpp"
#include "palette/list_view.hpp"
#include <memory>
#include <vector>
#include <string>

namespace biway {

class Server;

class Menu {
public:
    explicit Menu(Server* server);
    ~Menu();

    void open();
    void close();
    void toggle();
    bool is_visible() const { return m_visible; }

    void render(int screen_width, int screen_height);
    void schedule_redraw();

    bool handle_key(uint32_t modifiers, xkb_keysym_t keysym);
    bool handle_mouse_move(double lx, double ly);
    bool handle_mouse_click(double lx, double ly);
    bool handle_scroll(double delta_y);

    void reload_applications();

private:
    void scan_desktop_files();
    std::string resolve_icon_path(const std::string& icon_name);
    void filter_items();
    void launch_selected_app(const ModelListItem& item);

    Server* m_server = nullptr;
    struct wlr_scene_buffer* m_scene_buffer = nullptr;
    std::unique_ptr<CairoBuffer> m_buffer;
    std::unique_ptr<ListView> m_list_view;

    bool m_visible = false;
    int m_modal_width = 800;
    int m_modal_height = 400;
    int m_modal_x = 0;
    int m_modal_y = 0;
    int m_screen_width = 1920;
    int m_screen_height = 1080;

    std::string m_search_query;
    std::vector<std::shared_ptr<ModelListItem>> m_all_items;
    std::vector<std::shared_ptr<ModelListItem>> m_filtered_items;
};

} // namespace biway
