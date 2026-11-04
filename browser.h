#pragma once
#include "input.h"
#include <cstdint>
#include <string>
#include <vector>

// A minimal but real web view: HTTP/1.1 fetch over BSD sockets, redirect
// following, and an HTML subset renderer (title, h1-h3, p, li, br, a, img alt).
class Browser {
public:
    bool has_page() const { return m_index >= 0; }
    const std::string& url() const;
    const std::string& title() const;
    bool loading() const { return m_loading; }
    const std::string& status() const { return m_status; }  // loading or error text

    void navigate(const std::string& url);
    void back();
    void forward();
    void reload();
    bool can_back() const { return m_index > 0; }
    bool can_forward() const { return m_index + 1 < static_cast<int>(m_stack.size()); }

    // Returns a URL to navigate to when A opened a focused link, else empty.
    std::string update(const Pad& pad);
    void draw(float bottom) const;

private:
    struct Page { std::string url, title; };
    struct Block { std::string text; int scale; int link; int before; };
    struct Line { std::string text; int scale; uint32_t color; int link; };

    void fetch_pending();
    void parse(const std::string& html, const std::string& base);
    void layout();
    void reset_view();

    std::vector<Page> m_stack;
    int m_index = -1;
    std::string m_pending;      // URL waiting to load
    bool m_loading = false;
    std::string m_status;

    std::vector<Block> m_blocks;
    std::vector<Line> m_lines;
    std::vector<std::string> m_links;
    std::vector<float> m_line_y;   // pixel y of each laid-out line
    float m_total_h = 0;
    float m_scroll = 0;
    int m_focus_link = -1;
};
