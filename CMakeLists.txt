cmake_minimum_required(VERSION 3.20)
project(biway LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

# Dependencies
pkg_search_module(WLROOTS REQUIRED IMPORTED_TARGET wlroots-0.20 wlroots-0.19 wlroots-0.18 wlroots)
pkg_check_modules(WAYLAND_SERVER REQUIRED IMPORTED_TARGET wayland-server)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.24)
pkg_check_modules(XKBCOMMON REQUIRED IMPORTED_TARGET xkbcommon)
pkg_check_modules(PIXMAN REQUIRED IMPORTED_TARGET pixman-1)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(GDK_PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
pkg_check_modules(LIBINPUT REQUIRED IMPORTED_TARGET libinput)

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")

# Generate xdg-shell protocol
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header ${XDG_SHELL_XML} ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h
    DEPENDS ${XDG_SHELL_XML}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
    COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_XML} ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
    DEPENDS ${XDG_SHELL_XML}
)

# Generate wlr-layer-shell protocol
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.h
    COMMAND ${WAYLAND_SCANNER} server-header ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c
    COMMAND ${WAYLAND_SCANNER} private-code ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml
)

add_executable(biway
    src/main.cpp
    src/core/server.cpp
    src/core/layer_surface.cpp
    src/core/workspace.cpp
    src/core/view.cpp
    src/core/output.cpp
    src/input/input.cpp
    src/config/config.cpp
    src/render/cairo_buffer.cpp
    src/ui/wallpaper/wallpaper.cpp
    src/ui/bar/bar.cpp
    src/ui/menu/menu.cpp
    src/ui/widgets/text_view.cpp
    src/ui/widgets/card_view.cpp
    src/ui/widgets/image_view.cpp
    src/ui/widgets/text_input_view.cpp
    src/ui/widgets/cell_item_view.cpp
    src/ui/widgets/table_view.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
    ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.h
    ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c
)

target_include_directories(biway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${WLROOTS_INCLUDE_DIRS}
    ${WAYLAND_SERVER_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS}
    ${PIXMAN_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${GDK_PIXBUF_INCLUDE_DIRS}
    ${LIBINPUT_INCLUDE_DIRS}
)

target_link_libraries(biway PRIVATE
    PkgConfig::WLROOTS
    PkgConfig::WAYLAND_SERVER
    PkgConfig::XKBCOMMON
    PkgConfig::PIXMAN
    PkgConfig::CAIRO
    PkgConfig::PANGOCAIRO
    PkgConfig::GDK_PIXBUF
    PkgConfig::LIBINPUT
)

target_compile_options(biway PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Installation targets
include(GNUInstallDirs)
install(TARGETS biway RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/biway.conf assets/light.conf assets/dark.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/biway)
install(FILES assets/lightwallpaper.png assets/darkwallpaper.jpg DESTINATION ${CMAKE_INSTALL_DATADIR}/backgrounds/biway)
install(FILES assets/lightwallpaper.png assets/darkwallpaper.jpg DESTINATION ${CMAKE_INSTALL_DATADIR}/biway)
install(FILES assets/biway.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
