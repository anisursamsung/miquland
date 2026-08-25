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

add_definitions(-DWLR_USE_UNSTABLE)

add_executable(biway
    src/main.cpp
    src/server.cpp
    src/workspace.cpp
    src/view.cpp
    src/output.cpp
    src/input.cpp
    src/config.cpp
    src/cairo_buffer.cpp
    src/wallpaper.cpp
    src/bar.cpp
    src/menu.cpp
    src/palette/list_item_view.cpp
    src/palette/list_view.cpp
)

target_include_directories(biway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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
install(FILES assets/biway.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/biway)
install(FILES assets/wallpaper.png DESTINATION ${CMAKE_INSTALL_DATADIR}/backgrounds/biway)
install(FILES assets/wallpaper.png DESTINATION ${CMAKE_INSTALL_DATADIR}/biway)
install(FILES assets/biway.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
