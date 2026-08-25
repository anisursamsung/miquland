#pragma once

// Include all standard C++ library headers FIRST
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <filesystem>

#include <wayland-server-core.h>
#include <pixman.h>
#include <xkbcommon/xkbcommon.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <drm_fourcc.h>

#ifdef __cplusplus
#define static
extern "C" {
#endif

#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/backend/libinput.h>
#include <libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>
#include <wlr/util/box.h>

#ifdef __cplusplus
}
#undef static
#endif
