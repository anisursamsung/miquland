#include "cairo_buffer.hpp"

namespace biway {

const struct wlr_buffer_impl CairoBuffer::s_buffer_impl = {
    .destroy = buffer_destroy,
    .get_dmabuf = nullptr,
    .get_shm = nullptr,
    .begin_data_ptr_access = buffer_begin_data_ptr_access,
    .end_data_ptr_access = buffer_end_data_ptr_access,
};

CairoBuffer::CairoBuffer(int width, int height)
    : m_width(width), m_height(height)
{
    wlr_buffer_init(&m_base, &s_buffer_impl, width, height);
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    m_cr = cairo_create(m_surface);
}

CairoBuffer::~CairoBuffer() {
    if (m_cr) cairo_destroy(m_cr);
    if (m_surface) cairo_surface_destroy(m_surface);
    wlr_buffer_drop(&m_base);
}

void CairoBuffer::resize(int width, int height) {
    if (m_width == width && m_height == height && m_surface) {
        return;
    }

    if (m_cr) cairo_destroy(m_cr);
    if (m_surface) cairo_surface_destroy(m_surface);

    m_width = width;
    m_height = height;
    m_base.width = width;
    m_base.height = height;

    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    m_cr = cairo_create(m_surface);
}

void CairoBuffer::buffer_destroy(struct wlr_buffer* buffer) {
    // Handled by CairoBuffer destructor
}

bool CairoBuffer::buffer_begin_data_ptr_access(struct wlr_buffer* buffer, uint32_t flags,
    void** data, uint32_t* format, size_t* stride)
{
    CairoBuffer* buf = wl_container_of(buffer, buf, m_base);
    if (!buf->m_surface) return false;

    cairo_surface_flush(buf->m_surface);
    *data = cairo_image_surface_get_data(buf->m_surface);
    *stride = static_cast<size_t>(cairo_image_surface_get_stride(buf->m_surface));
    *format = DRM_FORMAT_ARGB8888;
    return true;
}

void CairoBuffer::buffer_end_data_ptr_access(struct wlr_buffer* buffer) {
    CairoBuffer* buf = wl_container_of(buffer, buf, m_base);
    if (buf->m_surface) {
        cairo_surface_mark_dirty(buf->m_surface);
    }
}

} // namespace biway
