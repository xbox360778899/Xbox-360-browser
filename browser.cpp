#include "browser.h"
#include "gfx.h"
#include "theme.h"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
constexpr float kMarginX = 48;
constexpr float kTextW = 1280 - 2 * kMarginX;
constexpr size_t kMaxBody = 2 * 1024 * 1024;
constexpr int kIoTimeoutMs = 10000;

std::string to_lower(const std::string& s)
{
    std::string out = s;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool split_url(const std::string& url, std::string& host, std::string& port, std::string& path)
{
    if (url.rfind("http://", 0) != 0)
        return false;
    const size_t start = 7;
    const size_t slash = url.find('/', start);
    const std::string authority = url.substr(start, slash == std::string::npos ? slash : slash - start);
    path = slash == std::string::npos ? "/" : url.substr(slash);
    const size_t colon = authority.find(':');
    if (colon == std::string::npos) {
        host = authority;
        port = "80";
    } else {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    }
    return !host.empty();
}

std::string resolve_href(const std::string& base, std::string href)
{
    const size_t hash = href.find('#');
    if (hash != std::string::npos)
        href.erase(hash);
    if (href.empty())
        return base;
    if (href.rfind("http://", 0) == 0 || href.rfind("https://", 0) == 0)
        return href;
    if (href.rfind("//", 0) == 0)
        return "http:" + href;
    std::string host, port, path;
    if (!split_url(base, host, port, path))
        return href;
    const std::string origin = "http://" + host + (port == "80" ? "" : ":" + port);
    if (href[0] == '/')
        return origin + href;
    const size_t slash = path.rfind('/');
    const std::string dir = slash == std::string::npos ? "/" : path.substr(0, slash + 1);
    return origin + dir + href;
}

void decode_entities(std::string& s)
{
    size_t i = 0;
    while ((i = s.find('&', i)) != std::string::npos) {
        const size_t semi = s.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 10)
            break;
        const std::string ent = s.substr(i + 1, semi - i - 1);
        std::string rep;
        if (ent == "amp") rep = "&";
        else if (ent == "lt") rep = "<";
        else if (ent == "gt") rep = ">";
        else if (ent == "quot") rep = "\"";
        else if (ent == "apos" || ent == "#39") rep = "'";
        else if (ent == "nbsp") rep = " ";
        else if (!ent.empty() && ent[0] == '#') {
            unsigned long cp = 0;
            if (ent.size() > 2 && (ent[1] == 'x' || ent[1] == 'X'))
                cp = std::strtoul(ent.c_str() + 2, nullptr, 16);
            else
                cp = std::strtoul(ent.c_str() + 1, nullptr, 10);
            if (cp > 0 && cp < 0x110000)
                rep = gfx::utf8_of(static_cast<uint32_t>(cp));
        }
        if (rep.empty()) {
            i = semi + 1;
        } else {
            s.replace(i, semi - i + 1, rep);
            i += rep.size();
        }
    }
}

size_t skip_past(const std::string& s, size_t from, const std::string& close_tag)
{
    const std::string hay = to_lower(s.substr(from));
    const size_t at = hay.find(close_tag);
    return at == std::string::npos ? std::string::npos : from + at + close_tag.size();
}

bool http_fetch(const std::string& url, std::string& body, std::string& final_url,
    std::string& title, std::string& error, int depth = 0)
{
    if (depth > 5) {
        error = "Too many redirects";
        return false;
    }
    std::string host, port, path;
    if (!split_url(url, host, port, path)) {
        error = url.rfind("https://", 0) == 0
            ? "HTTPS needs mbedtls-xenon; this build does HTTP only"
            : "Unsupported address";
        return false;
    }
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
        error = "Can't resolve " + host;
        return false;
    }
    const int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        error = "No socket available";
        return false;
    }
    timeval tv = { kIoTimeoutMs / 1000, (kIoTimeoutMs % 1000) * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(fd);
        error = "Can't connect to " + host;
        return false;
    }
    freeaddrinfo(res);
    const std::string req = "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: Mozilla/5.0 (Xbox; XenonIE/1.0)\r\n"
        "Accept: text/html,application/xhtml+xml;q=0.9,*/*;q=0.5\r\n"
        "Accept-Language: en-GB,en;q=0.8\r\n"
        "Connection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < req.size()) {
        const ssize_t n = send(fd, req.c_str() + sent, req.size() - sent, 0);
        if (n <= 0) {
            close(fd);
            error = "Send failed";
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    std::string raw;
    char buf[4096];
    ssize_t n = 0;
    while ((n = recv(fd, buf, sizeof buf, 0)) > 0) {
        raw.append(buf, static_cast<size_t>(n));
        if (raw.size() > kMaxBody + 65536)
            break;
    }
    close(fd);
    if (raw.empty()) {
        error = "No response from " + host;
        return false;
    }
    const size_t head_end = raw.find("\r\n\r\n");
    if (head_end == std::string::npos) {
        error = "Malformed response";
        return false;
    }
    const std::string headers = to_lower(raw.substr(0, head_end));
    body = raw.substr(head_end + 4);
    int status = 0;
    if (std::sscanf(headers.c_str(), "http/%*d.%*d %d", &status) != 1) {
        error = "Malformed status line";
        return false;
    }
    auto header = [&](const char* name) -> std::string {
        const std::string key = std::string("\n") + name + ":";
        const size_t at = headers.find(key);
        if (at == std::string::npos)
            return "";
        const size_t begin = at + key.size();
        const size_t end = headers.find("\r\n", begin);
        std::string v = headers.substr(begin, end == std::string::npos ? end : end - begin);
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
            v.erase(v.begin());
        return v;
    };
    if (status >= 301 && status <= 308) {
        std::string loc = header("location");
        if (!loc.empty())
            return http_fetch(resolve_href(url, loc), body, final_url, title, error, depth + 1);
    }
    if (status != 200) {
        error = "HTTP " + std::to_string(status);
        return false;
    }
    const std::string low_body = to_lower(body);
    const size_t t0 = low_body.find("<title");
    if (t0 != std::string::npos) {
        const size_t t1 = low_body.find('>', t0);
        const size_t t2 = low_body.find("</title>", t1 == std::string::npos ? t0 : t1);
        if (t1 != std::string::npos && t2 != std::string::npos) {
            title = body.substr(t1 + 1, t2 - t1 - 1);
            while (!title.empty() && std::isspace(static_cast<unsigned char>(title.front())))
                title.erase(title.begin());
            while (!title.empty() && std::isspace(static_cast<unsigned char>(title.back())))
                title.pop_back();
        }
    }
    const std::string te = header("transfer-encoding");
    if (te.find("chunked") != std::string::npos) {
        std::string decoded;
        size_t at = 0;
        while (at < body.size()) {
            const size_t line_end = body.find("\r\n", at);
            if (line_end == std::string::npos)
                break;
            const unsigned long chunk = std::strtoul(body.c_str() + at, nullptr, 16);
            if (chunk == 0)
                break;
            at = line_end + 2;
            if (at + chunk > body.size()) {
                decoded.append(body.substr(at));
                break;
            }
            decoded.append(body, at, chunk);
            at += chunk + 2;
        }
        body.swap(decoded);
    }
    final_url = url;
    if (body.size() > kMaxBody)
        body.resize(kMaxBody);
    return true;
}
} // namespace

const std::string& Browser::url() const
{
    static const std::string empty;
    return m_index >= 0 ? m_stack[static_cast<size_t>(m_index)].url : empty;
}

const std::string& Browser::title() const
{
    static const std::string empty;
    return m_index >= 0 ? m_stack[static_cast<size_t>(m_index)].title : empty;
}

void Browser::navigate(const std::string& url)
{
    m_stack.resize(static_cast<size_t>(m_index + 1));
    m_stack.push_back({ url, url });
    m_index = static_cast<int>(m_stack.size()) - 1;
    m_pending = url;
    m_loading = true;
    m_status = "Connecting to " + url;
    reset_view();
}

void Browser::back()
{
    if (m_index > 0) {
        m_index--;
        m_pending = m_stack[static_cast<size_t>(m_index)].url;
        m_loading = true;
        m_status = "Connecting...";
        reset_view();
    }
}

void Browser::forward()
{
    if (can_forward()) {
        m_index++;
        m_pending = m_stack[static_cast<size_t>(m_index)].url;
        m_loading = true;
        m_status = "Connecting...";
        reset_view();
    }
}

void Browser::reload()
{
    if (m_index >= 0) {
        m_pending = url();
        m_loading = true;
        m_status = "Connecting...";
        reset_view();
    }
}

void Browser::reset_view()
{
    m_blocks.clear();
    m_lines.clear();
    m_links.clear();
    m_line_y.clear();
    m_total_h = 0;
    m_scroll = 0;
    m_focus_link = -1;
}

void Browser::fetch_pending()
{
    if (m_pending.empty())
        return;
    const std::string target = m_pending;
    m_pending.clear();
    std::string body, final_url, title, error;
    if (!http_fetch(target, body, final_url, title, error)) {
        m_loading = false;
        m_status = error;
        return;
    }
    Page& p = m_stack[static_cast<size_t>(m_index)];
    p.url = final_url;
    p.title = title.empty() ? final_url : title;
    m_loading = false;
    m_status.clear();
    parse(body, final_url);
    layout();
}

void Browser::parse(const std::string& html, const std::string& base)
{
    int cur_scale = 1;
    std::string buf;
    int buf_link = -1;
    auto flush = [&](int before) {
        while (!buf.empty() && buf.back() == ' ')
            buf.pop_back();
        if (!buf.empty()) {
            m_blocks.push_back({ buf, cur_scale, buf_link, before });
            buf.clear();
        }
        buf_link = -1;
    };

    size_t i = 0;
    bool in_title = false;
    while (i < html.size()) {
        if (html[i] != '<') {
            const size_t begin = i;
            while (i < html.size() && html[i] != '<')
                i++;
            std::string text = html.substr(begin, i - begin);
            decode_entities(text);
            for (char c : text) {
                if (std::isspace(static_cast<unsigned char>(c))) {
                    if (!in_title && !buf.empty() && buf.back() != ' ')
                        buf += ' ';
                } else if (!in_title) {
                    buf += c;
                }
            }
            continue;
        }
        if (i + 4 <= html.size() && html.compare(i, 4, "<!--") == 0) {
            const size_t end = html.find("-->", i + 4);
            i = end == std::string::npos ? html.size() : end + 3;
            continue;
        }
        size_t j = i + 1;
        const bool closing = j < html.size() && html[j] == '/';
        if (closing)
            j++;
        size_t name_end = j;
        while (name_end < html.size()
            && (std::isalnum(static_cast<unsigned char>(html[name_end])) || html[name_end] == '!'))
            name_end++;
        const std::string name = to_lower(html.substr(j, name_end - j));
        const size_t gt = html.find('>', name_end);
        if (gt == std::string::npos)
            break;
        const std::string inside = to_lower(html.substr(name_end, gt - name_end));

        if (name == "script" || name == "style") {
            if (!closing) {
                const size_t past = skip_past(html, gt + 1, "</" + name);
                i = past == std::string::npos ? html.size() : past;
                continue;
            }
        } else if (name == "title") {
            in_title = !closing;
        } else if (in_title) {
            // ignore the rest of <head>
        } else if (name == "h1" || name == "h2") {
            flush(14);
            cur_scale = 2;
        } else if (name == "h3") {
            flush(12);
        } else if (name == "p" || name == "div" || name == "section" || name == "article"
            || name == "tr" || name == "table" || name == "ul" || name == "ol"
            || name == "header" || name == "footer" || name == "blockquote") {
            flush(8);
            cur_scale = 1;
        } else if (name == "br") {
            flush(0);
            cur_scale = 1;
        } else if (name == "li") {
            flush(4);
            cur_scale = 1;
            if (!closing)
                buf = "- ";
        } else if (name == "a") {
            if (!closing) {
                const size_t href_at = inside.find("href");
                const size_t eq = href_at == std::string::npos ? std::string::npos : inside.find('=', href_at);
                const size_t q1 = eq == std::string::npos ? std::string::npos : inside.find_first_of("\"'", eq);
                if (q1 != std::string::npos) {
                    const char q = inside[q1];
                    const size_t q2 = inside.find(q, q1 + 1);
                    if (q2 != std::string::npos) {
                        const std::string raw_href = html.substr(name_end + q1 + 1, q2 - q1 - 1);
                        m_links.push_back(resolve_href(base, raw_href));
                        buf_link = static_cast<int>(m_links.size()) - 1;
                    }
                }
            } else {
                flush(0);
            }
        } else if (name == "img" && !closing) {
            const size_t alt_at = inside.find("alt");
            const size_t eq = alt_at == std::string::npos ? std::string::npos : inside.find('=', alt_at);
            const size_t q1 = eq == std::string::npos ? std::string::npos : inside.find_first_of("\"'", eq);
            if (q1 != std::string::npos) {
                const char q = inside[q1];
                const size_t q2 = inside.find(q, q1 + 1);
                if (q2 != std::string::npos) {
                    const std::string alt = html.substr(name_end + q1 + 1, q2 - q1 - 1);
                    if (!alt.empty()) {
                        if (!buf.empty() && buf.back() != ' ')
                            buf += ' ';
                        buf += "[image: " + alt + "]";
                    }
                }
            }
        } else if ((name == "html" || name == "body" || name == "head") && closing) {
            flush(8);
            cur_scale = 1;
        }
        i = gt + 1;
    }
    flush(0);
}

void Browser::layout()
{
    m_lines.clear();
    m_line_y.clear();
    float y = 0;
    for (const Block& b : m_blocks) {
        y += static_cast<float>(b.before);
        const auto ls = gfx::wrap(b.text, b.scale, kTextW);
        for (const std::string& l : ls) {
            m_lines.push_back({ l, b.scale, b.link >= 0 ? theme::kLink : theme::kPageText, b.link });
            m_line_y.push_back(y);
            y += static_cast<float>(20 * b.scale + 6);
        }
    }
    m_total_h = y;
}

std::string Browser::update(const Pad& pad)
{
    fetch_pending();
    if (m_loading || m_lines.empty())
        return "";
    const float view_h = 720 - theme::kTopBarH;
    const float max_scroll = std::max(0.0f, m_total_h - view_h + 40);
    m_scroll = std::clamp(m_scroll - pad.stick_ry() / 2048.0f, 0.0f, max_scroll);

    if (!m_links.empty()) {
        if (pad.nav_y()) {
            if (m_focus_link < 0)
                m_focus_link = pad.nav_y() > 0 ? 0 : static_cast<int>(m_links.size()) - 1;
            else
                m_focus_link = std::clamp(m_focus_link + pad.nav_y(), 0,
                    static_cast<int>(m_links.size()) - 1);
            for (size_t i = 0; i < m_lines.size(); i++)
                if (m_lines[i].link == m_focus_link) {
                    const float ly = m_line_y[i];
                    if (ly < m_scroll + 80)
                        m_scroll = std::max(0.0f, ly - 80);
                    else if (ly > m_scroll + view_h - 120)
                        m_scroll = std::min(max_scroll, ly - view_h + 120);
                    break;
                }
        }
        if (pad.hit(INPUT_GAMEPAD_A) && m_focus_link >= 0
            && m_focus_link < static_cast<int>(m_links.size()))
            return m_links[static_cast<size_t>(m_focus_link)];
    }
    return "";
}

void Browser::draw(float bottom) const
{
    const float top = theme::kTopBarH;
    if (m_loading || !has_page()) {
        gfx::fill_rect(0, top, gfx::width(), bottom - top, theme::kBg);
        gfx::text(kMarginX, 300, 1, theme::kText, m_status.c_str());
        return;
    }
    if (!m_status.empty()) {
        gfx::fill_rect(0, top, gfx::width(), bottom - top, theme::kBg);
        gfx::text(kMarginX, 280, 1, theme::kHintB, m_status.c_str());
        gfx::text(kMarginX, 312, 1, theme::kTextDim, "Press B to go back.");
        return;
    }
    gfx::fill_rect(0, top, gfx::width(), bottom - top, theme::kPageBg);
    if (m_lines.empty()) {
        gfx::text(kMarginX, 280, 1, theme::kPageText, "This page rendered no text.");
        return;
    }
    const float title_y = top + 16 - m_scroll;
    if (title_y >= top + 2)
        gfx::text(kMarginX, title_y, 2, theme::kPageText,
            gfx::truncate_to(title(), 2, kTextW).c_str());
    for (size_t i = 0; i < m_lines.size(); i++) {
        const Line& l = m_lines[i];
        const float y = top + 60 + m_line_y[i] - m_scroll;
        if (y > bottom - 20 || y + 20 * l.scale < top + 40)
            continue;
        const bool focused = l.link >= 0 && l.link == m_focus_link;
        if (focused) {
            const float w = std::min(gfx::text_width(l.text, l.scale) + 8, kTextW);
            gfx::fill_rect(kMarginX - 4, y - 2, w, 20 * l.scale + 4, theme::kGreen);
        }
        gfx::text(kMarginX, y, l.scale, focused ? theme::kText : l.color, l.text.c_str());
    }
    const float view_h = bottom - top - 60;
    if (m_total_h > view_h && bottom > 600) {
        const float track = bottom - top - 100;
        const float thumb = std::max(20.0f, track * view_h / m_total_h);
        const float ty = top + 70 + (track - thumb)
            * (m_scroll / std::max(1.0f, m_total_h - view_h));
        gfx::fill_rect(1262, ty, 6, thumb, theme::kTextDim);
    }
}
