#include "core/output.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include <ctime>

namespace biway {

Output::Output(Server* server, struct wlr_output* wlr_output)
    : m_server(server), m_wlr_output(wlr_output)
{
    wlr_output_init_render(wlr_output, server->get_allocator(), server->get_renderer());

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    struct wlr_output_mode* mode = wlr_output_preferred_mode(wlr_output);
    if (mode != nullptr) {
        wlr_output_state_set_mode(&state, mode);
    }

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    m_usable_area = {
        .x = 0,
        .y = 0,
        .width = wlr_output->width,
        .height = wlr_output->height
    };

    m_scene_output = wlr_scene_output_create(server->get_scene(), wlr_output);
    wlr_output_layout_add_auto(server->get_output_manager()->get_layout(), wlr_output);

    m_frame_listener.notify = handle_frame;
    wl_signal_add(&wlr_output->events.frame, &m_frame_listener);

    m_request_state_listener.notify = handle_request_state;
    wl_signal_add(&wlr_output->events.request_state, &m_request_state_listener);

    m_destroy_listener.notify = handle_destroy;
    wl_signal_add(&wlr_output->events.destroy, &m_destroy_listener);
}

Output::~Output() {
    wl_list_remove(&m_frame_listener.link);
    wl_list_remove(&m_request_state_listener.link);
    wl_list_remove(&m_destroy_listener.link);
}

void Output::handle_frame(struct wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, m_frame_listener);

    wlr_scene_output_commit(output->m_scene_output, nullptr);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(output->m_scene_output, &now);
}

void Output::handle_request_state(struct wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, m_request_state_listener);
    auto* event = static_cast<struct wlr_output_event_request_state*>(data);
    wlr_output_commit_state(output->m_wlr_output, event->state);
}

void Output::handle_destroy(struct wl_listener* listener, void* data) {
    Output* output = wl_container_of(listener, output, m_destroy_listener);
    output->m_server->get_output_manager()->remove_output(output);
}

OutputManager::OutputManager(Server* server)
    : m_server(server)
{
    m_output_layout = wlr_output_layout_create(server->get_display());
    wlr_scene_attach_output_layout(server->get_scene(), m_output_layout);

    m_new_output_listener.notify = handle_new_output;
    wl_signal_add(&server->get_backend()->events.new_output, &m_new_output_listener);
}

OutputManager::~OutputManager() {
    wl_list_remove(&m_new_output_listener.link);
}

struct wlr_box OutputManager::get_primary_geometry() const {
    struct wlr_box box = { 0, 0, 0, 0 };
    if (m_output_layout) {
        wlr_output_layout_get_box(m_output_layout, nullptr, &box);
    }
    return box;
}

struct wlr_box OutputManager::get_primary_usable_geometry() const {
    auto* primary = get_primary_output();
    if (primary) {
        const auto& area = primary->get_usable_area();
        if (area.width > 0 && area.height > 0) {
            return area;
        }
    }
    return get_primary_geometry();
}

Output* OutputManager::find_output(struct wlr_output* wlr_out) const {
    for (const auto& out : m_outputs) {
        if (out && out->get_wlr_output() == wlr_out) {
            return out.get();
        }
    }
    return nullptr;
}

void OutputManager::add_output(Output* output) {
    m_outputs.emplace_back(output);
    m_server->get_workspace_manager()->recalculate_layout();
}

void OutputManager::remove_output(Output* output) {
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        if (it->get() == output) {
            m_outputs.erase(it);
            break;
        }
    }
    m_server->get_workspace_manager()->recalculate_layout();
}

void OutputManager::handle_new_output(struct wl_listener* listener, void* data) {
    OutputManager* manager = wl_container_of(listener, manager, m_new_output_listener);
    auto* wlr_output = static_cast<struct wlr_output*>(data);

    Output* output = new Output(manager->m_server, wlr_output);
    manager->add_output(output);
}

} // namespace biway
