#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gfx {

bool open();    // GPU, quad shaders, unit-square VB, text, display
void close();
uint32_t width();
uint32_t height();

uint32_t frame_begin();  // waits for a buffer, sets the target; returns resolve address
void frame_end(uint32_t address);

void fill_rect(float x, float y, float w, float h, uint32_t argb);  // opaque only

void text(float x, float y, int scale, uint32_t argb, const char* utf8);
void measure(const char* utf8, int scale, float& w, float& h);
float text_width(const std::string& s, int scale);
std::string truncate_to(const std::string& s, int scale, float max_w);

uint32_t next_cp(const std::string& s, size_t& i);   // decode UTF-8 at i, advance i
std::string utf8_of(uint32_t cp);
std::vector<std::string> wrap(const std::string& s, int scale, float max_w);

} // namespace gfx
