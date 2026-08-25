#pragma once

#include <string>
#include <memory>

namespace biway {

class ModelListItem {
public:
    ModelListItem(std::string id, std::string title, std::string subtitle = "",
                  std::string icon_name = "", std::string icon_path = "", std::string exec_cmd = "")
        : m_id(std::move(id)), m_title(std::move(title)), m_subtitle(std::move(subtitle)),
          m_icon_name(std::move(icon_name)), m_icon_path(std::move(icon_path)), m_exec_cmd(std::move(exec_cmd)) {}

    virtual ~ModelListItem() = default;

    const std::string& get_id() const { return m_id; }
    const std::string& get_title() const { return m_title; }
    const std::string& get_subtitle() const { return m_subtitle; }
    const std::string& get_icon_name() const { return m_icon_name; }
    const std::string& get_icon_path() const { return m_icon_path; }
    const std::string& get_exec_cmd() const { return m_exec_cmd; }

    void set_icon_path(const std::string& path) { m_icon_path = path; }

private:
    std::string m_id;
    std::string m_title;
    std::string m_subtitle;
    std::string m_icon_name;
    std::string m_icon_path;
    std::string m_exec_cmd;
};

} // namespace biway
