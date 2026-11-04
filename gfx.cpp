// gfx.cpp: frame begin/end, filled rectangles through the quad shaders, text helpers.
#include "gfx.h"
#include "xenon_ie_shaders.h"

#include <xsdk/gpu.h>
#include <xsdk/kernel.h>
#include <xsdk/log.h>
#include <xsdk/present.h>
#include <xsdk/shader.h>
#include <xsdk/text.h>

#include <unordered_map>

namespace gfx {
namespace {

XsSDKShader g_vs;  // zero-initialized; xsdk_shader_load fills .microcode
XsSDKShader g_ps;
volatile uint32_t* g_vb = nullptr;
uint32_t g_vb_address = 0;
uint32_t g_w = 0;
uint32_t g_h = 0;
std::unordered_map<uint32_t, float> g_widths;  // key = code point * 16 + scale

void upload(volatile uint32_t* dst, const void* src, size_t bytes)
{
    for (size_t i = 0; i < bytes / 4; i++)
        dst[i] = static_cast<const uint32_t*>(src)[i];
    xsdk_gpu_invalidate(xsdk_gpu_address(const_cast<uint32_t*>(dst)), bytes);
}

} // namespace

bool open()
{
    if (!NT_SUCCESS(xsdk_gpu_open()))
        return false;
    NTSTATUS status = xsdk_shader_load(&g_vs, quad_vert.container, quad_vert.container_bytes);
    if (NT_SUCCESS(status))
        status = xsdk_shader_load(&g_ps, quad_frag.container, quad_frag.container_bytes);
    const XsdkVertexElement layout[] = { { XSDK_USAGE_POSITION, XSDK_VERTEX_FLOAT2, 0 } };
    const uint32_t strides[] = { 8 };
    if (NT_SUCCESS(status))
        status = xsdk_shader_set_vertex_layout(&g_vs, layout, 1, strides);
    if (NT_SUCCESS(status))
        g_vb = static_cast<volatile uint32_t*>(xsdk_gpu_alloc(64 * 1024));
    if (!NT_SUCCESS(status) || !g_vb) {
        xsdk_log_write("gfx: shader/vertex setup failed: 0x%08X", (unsigned)status);
        return false;
    }
    static const float unit_square[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    upload(g_vb, unit_square, sizeof unit_square);
    g_vb_address = xsdk_gpu_address(const_cast<uint32_t*>(g_vb));
    if (!NT_SUCCESS(xsdk_text_open())) {
        xsdk_log_write("gfx: text_open failed");
        return false;
    }
    status = xsdk_present_open(XSDK_DISPLAY_720P, 2, 1, &g_w, &g_h);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("gfx: present_open failed: 0x%08X", (unsigned)status);
        return false;
    }
    return true;
}

void close()
{
    xsdk_present_close();
    xsdk_text_close();
    xsdk_shader_free(&g_ps);
    xsdk_shader_free(&g_vs);
    if (g_vb)
        xsdk_gpu_free(const_cast<uint32_t*>(g_vb));
    xsdk_gpu_close();
}

uint32_t width() { return g_w; }
uint32_t height() { return g_h; }

uint32_t frame_begin()
{
    uint32_t address = 0;
    const NTSTATUS status = xsdk_present_begin(&address, 5000);
    if (!NT_SUCCESS(status))
        xsdk_log_write("gfx: present_begin failed: 0x%08X", (unsigned)status);
    xsdk_gpu_set_target(g_w, g_h);
    return address;
}

void frame_end(uint32_t address)
{
    xsdk_gpu_resolve(address, g_w, g_h, 0xFF000000);
    xsdk_present_end();
}

void fill_rect(float x, float y, float w, float h, uint32_t argb)
{
    if (w <= 0 || h <= 0)
        return;
    xsdk_gpu_set_shaders(&g_vs, &g_ps);
    xsdk_built_shader_set_literals(&quad_vert);
    xsdk_built_shader_set_literals(&quad_frag);
    const float v[8] = { x, y, w, h, 1.0f / g_w, 1.0f / g_h, 0.0f, 0.0f };
    xsdk_gpu_set_vertex_constants(QUAD_VERT_PUSH, v, 2);  // 2 float constants = 8 floats
    const float a = (argb >> 24 & 0xFF) / 255.0f;
    const float r = (argb >> 16 & 0xFF) / 255.0f;
    const float g = (argb >> 8 & 0xFF) / 255.0f;
    const float b = (argb & 0xFF) / 255.0f;
    const float c[4] = { r, g, b, a };
    xsdk_gpu_set_pixel_constants(QUAD_FRAG_PUSH, c, 1);
    xsdk_gpu_set_vertices(0, g_vb_address, 32);
    xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, 4);
}

void text(float x, float y, int scale, uint32_t argb, const char* utf8)
{
    xsdk_text_draw(x, y, scale, argb, utf8);
}

// FIX: Using uint32_t temporarily to match xsdk_text_measure signature
void measure(const char* utf8, int scale, float& w, float& h)
{
    uint32_t uw = 0, uh = 0;
    xsdk_text_measure(utf8, scale, &uw, &uh);
    w = static_cast<float>(uw);
    h = static_cast<float>(uh);
}

uint32_t next_cp(const std::string& s, size_t& i)
{
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { i += 1; return c; }
    int n = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
    else { i += 1; return 0xFFFD; }
    for (int k = 0; k < n; k++) {
        if (i + 1 + static_cast<size_t>(k) >= s.size()
            || (static_cast<unsigned char>(s[i + 1 + k]) & 0xC0) != 0x80) {
            i += 1;
            return 0xFFFD;
        }
        cp = cp << 6 | (static_cast<unsigned char>(s[i + 1 + k]) & 0x3F);
    }
    i += 1 + static_cast<size_t>(n);
    return cp;
}

std::string utf8_of(uint32_t cp)
{
    char b[5] = { 0, 0, 0, 0, 0 };
    if (cp < 0x80) { b[0] = static_cast<char>(cp); return std::string(b, 1); }
    if (cp < 0x800) {
        b[0] = static_cast<char>(0xC0 | cp >> 6);
        b[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return std::string(b, 2);
    }
    if (cp < 0x10000) {
        b[0] = static_cast<char>(0xE0 | cp >> 12);
        b[1] = static_cast<char>(0x80 | (cp >> 6 & 0x3F));
        b[2] = static_cast<char>(0x80 | (cp & 0x3F));
        return std::string(b, 3);
    }
    b[0] = static_cast<char>(0xF0 | cp >> 18);
    b[1] = static_cast<char>(0x80 | (cp >> 12 & 0x3F));
    b[2] = static_cast<char>(0x80 | (cp >> 6 & 0x3F));
    b[3] = static_cast<char>(0x80 | (cp & 0x3F));
    return std::string(b, 4);
}

float text_width(const std::string& s, int scale)
{
    float w = 0;
    for (size_t i = 0; i < s.size();) {
        const uint32_t cp = next_cp(s, i);
        const uint32_t key = cp * 16 + static_cast<uint32_t>(scale);
        auto it = g_widths.find(key);
        if (it == g_widths.end()) {
            // FIX: Using uint32_t here as well
            uint32_t cw_u = 0, ch_u = 0;
            const std::string one = utf8_of(cp);
            xsdk_text_measure(one.c_str(), scale, &cw_u, &ch_u);
            float cw = static_cast<float>(cw_u);
            it = g_widths.emplace(key, cw).first;
        }
        w += it->second;
    }
    return w;
}

std::string truncate_to(const std::string& s, int scale, float max_w)
{
    if (text_width(s, scale) <= max_w)
        return s;
    std::string out = s;
    while (!out.empty() && text_width(out + "...", scale) > max_w) {
        size_t i = out.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(out[i]) & 0xC0) == 0x80)
            i--;
        out.erase(i);
    }
    return out.empty() ? out : out + "...";
}

std::vector<std::string> wrap(const std::string& s, int scale, float max_w)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ')
            i++;
        if (i >= s.size())
            break;
        size_t end = i;
        float w = 0;
        size_t last_space = std::string::npos;
        while (end < s.size()) {
            const size_t before = end;
            const uint32_t cp = next_cp(s, end);
            const float cw = text_width(utf8_of(cp), scale);
            if (w + cw > max_w && end > i) {
                if (cp != ' ' && last_space != std::string::npos && last_space > i) {
                    end = last_space;
                } else {
                    end = before;
                    if (end == i)   // single glyph wider than the line: take it anyway
                        next_cp(s, end);
                }
                break;
            }
            w += cw;
            if (cp == ' ')
                last_space = end;
        }
        out.push_back(s.substr(i, end - i));
        i = end;
    }
    if (out.empty())
        out.push_back("");
    return out;
}

} // namespace gfx
