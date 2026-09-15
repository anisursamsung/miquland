#include "core/plugin_manager.hpp"
#include "core/server.hpp"
#include "core/workspace.hpp"
#include "core/output.hpp"
#include "core/config/config.hpp"
#include "core/common/util.hpp"

#include <dlfcn.h>
#include <iostream>

namespace miquland {

PluginManager::PluginManager(Server* server)
    : m_server(server) {}

PluginManager::~PluginManager() {
    unload_all();
}

WorkspaceManager* PluginManager::get_workspace_manager() {
    return m_server ? m_server->get_workspace_manager() : nullptr;
}

OutputManager* PluginManager::get_output_manager() {
    return m_server ? m_server->get_output_manager() : nullptr;
}

struct wlr_scene* PluginManager::get_scene() {
    return m_server ? m_server->get_scene() : nullptr;
}

struct wlr_scene_tree* PluginManager::get_workspaces_tree() {
    return m_server ? m_server->get_workspaces_tree() : nullptr;
}

struct wlr_scene_tree* PluginManager::get_layer_overlay_tree() {
    return m_server ? m_server->get_layer_overlay_tree() : nullptr;
}

void PluginManager::register_dispatcher(const std::string& name, std::function<void()> handler) {
    m_dispatchers[name] = std::move(handler);
    log_info("Plugin registered dispatcher: " + name);
}

void PluginManager::register_pointer_button_hook(std::function<bool(double lx, double ly, uint32_t button, bool pressed)> hook) {
    m_pointer_button_hooks.push_back(std::move(hook));
}

void PluginManager::register_pointer_motion_hook(std::function<bool(double lx, double ly)> hook) {
    m_pointer_motion_hooks.push_back(std::move(hook));
}

void PluginManager::register_key_hook(std::function<bool(uint32_t keysym, uint32_t modifiers, bool pressed)> hook) {
    m_key_hooks.push_back(std::move(hook));
}

bool PluginManager::load_plugin(const std::string& path) {
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        log_error("Failed to load plugin " + path + ": " + dlerror());
        return false;
    }

    auto init_func = reinterpret_cast<PluginInitFunc>(dlsym(handle, "miqu_plugin_init"));
    if (!init_func) {
        log_error("Plugin " + path + " missing miqu_plugin_init entry point: " + dlerror());
        dlclose(handle);
        return false;
    }

    PluginInfo* info = init_func(this);
    if (!info) {
        log_error("Plugin " + path + " init returned null info");
        dlclose(handle);
        return false;
    }

    if (info->abi_version != MIQU_PLUGIN_ABI_VERSION) {
        log_error("Plugin " + path + " ABI mismatch (plugin: " + std::to_string(info->abi_version) +
                  ", miquland: " + std::to_string(MIQU_PLUGIN_ABI_VERSION) + ")");
        dlclose(handle);
        return false;
    }

    auto exit_func = reinterpret_cast<PluginExitFunc>(dlsym(handle, "miqu_plugin_exit"));

    LoadedPlugin loaded;
    loaded.handle = handle;
    loaded.info = *info;
    loaded.exit_func = exit_func;
    loaded.path = path;

    log_info("Loaded plugin: " + std::string(info->name ? info->name : path) +
             " v" + std::string(info->version ? info->version : "1.0.0") +
             " by " + std::string(info->author ? info->author : "Unknown"));

    m_loaded_plugins.push_back(loaded);
    return true;
}

void PluginManager::unload_all() {
    m_dispatchers.clear();
    m_pointer_button_hooks.clear();
    m_pointer_motion_hooks.clear();
    m_key_hooks.clear();

    for (auto it = m_loaded_plugins.rbegin(); it != m_loaded_plugins.rend(); ++it) {
        if (it->exit_func) {
            it->exit_func();
        }
        if (it->handle) {
            dlclose(it->handle);
        }
    }
    m_loaded_plugins.clear();
}

void PluginManager::load_configured_plugins() {
    for (const auto& path : Config::get().get_plugins()) {
        load_plugin(path);
    }
}

bool PluginManager::execute_dispatcher(const std::string& name) {
    auto it = m_dispatchers.find(name);
    if (it != m_dispatchers.end()) {
        it->second();
        return true;
    }
    return false;
}

bool PluginManager::has_dispatcher(const std::string& name) const {
    return m_dispatchers.find(name) != m_dispatchers.end();
}

bool PluginManager::dispatch_pointer_button(double lx, double ly, uint32_t button, bool pressed) {
    for (auto it = m_pointer_button_hooks.rbegin(); it != m_pointer_button_hooks.rend(); ++it) {
        if ((*it)(lx, ly, button, pressed)) {
            return true;
        }
    }
    return false;
}

bool PluginManager::dispatch_pointer_motion(double lx, double ly) {
    for (auto it = m_pointer_motion_hooks.rbegin(); it != m_pointer_motion_hooks.rend(); ++it) {
        if ((*it)(lx, ly)) {
            return true;
        }
    }
    return false;
}

bool PluginManager::dispatch_key(uint32_t keysym, uint32_t modifiers, bool pressed) {
    for (auto it = m_key_hooks.rbegin(); it != m_key_hooks.rend(); ++it) {
        if ((*it)(keysym, modifiers, pressed)) {
            return true;
        }
    }
    return false;
}

} // namespace miquland
