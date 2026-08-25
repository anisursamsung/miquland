#pragma once

#include "common/util.hpp"
#include <vector>

namespace biway {

class Server;

class Output {
public:
    Output(Server* server, struct wlr_output* wlr_output);
    ~Output();

    struct wlr_output* get_wlr_output() const { return m_wlr_output; }
    struct wlr_scene_output* get_scene_output() const { return m_scene_output; }

private:
    static void handle_frame(struct wl_listener* listener, void* data);
    static void handle_request_state(struct wl_listener* listener, void* data);
    static void handle_destroy(struct wl_listener* listener, void* data);

    Server* m_server = nullptr;
    struct wlr_output* m_wlr_output = nullptr;
    struct wlr_scene_output* m_scene_output = nullptr;

    struct wl_listener m_frame_listener;
    struct wl_listener m_request_state_listener;
    struct wl_listener m_destroy_listener;
};

class OutputManager {
public:
    explicit OutputManager(Server* server);
    ~OutputManager();

    struct wlr_output_layout* get_layout() const { return m_output_layout; }
    struct wlr_box get_primary_geometry() const;

    void add_output(Output* output);
    void remove_output(Output* output);

private:
    static void handle_new_output(struct wl_listener* listener, void* data);

    Server* m_server = nullptr;
    struct wlr_output_layout* m_output_layout = nullptr;
    std::vector<std::unique_ptr<Output>> m_outputs;

    struct wl_listener m_new_output_listener;
};

} // namespace biway
