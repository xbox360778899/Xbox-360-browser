#include "addressbar.h"
#include "gfx.h"
#include "theme.h"

#include <algorithm>
#include <cmath>

namespace {
// Top bar geometry.
constexpr float kBtnY = 8, kBtnH = 40;
constexpr float kBackX = 24, kFwdX = 72, kFieldX = 124, kFieldW = 920;
constexpr float kRefreshX = 1052, kFavX = 1100, kAboutX = 1148, kCloseX = 1196;
constexpr float kTextX = kFieldX + 16;

// Suggestion tiles: up to 4 across.
constexpr float kSugW = 292, kSugGap = 12;
}

void AddressBar::begin_edit(const std::string& url)
{
    m_text = url;
    m_caret = m_text.size();
    m_anchor = 0;  // whole URL pre-selected, as in the photo
    m_editing = true;
    m_sug_focus = -1;
    m_enter_pending = false;
    recompute();
}

size_t AddressBar::sel_start() const
{
    return m_anchor < 0 ? m_caret : std::min(m_anchor, static_cast<int>(m_caret));
}

size_t AddressBar::sel_end() const
{
    return m_anchor < 0 ? m_caret : std::max(m_anchor, static_cast<int>(m_caret));
}

void AddressBar::erase_selection()
{
    if (m_anchor < 0)
        return;
    m_text.erase(sel_start(), sel_end() - sel_start());
    m_caret = sel_start();
    m_anchor = -1;
}

void AddressBar::on_char(const std::string& s)
{
    erase_selection();
    m_text.insert(m_caret, s);
    m_caret += s.size();
    recompute();
}

void AddressBar::on_backspace()
{
    if (m_anchor >= 0) {
        erase_selection();
    } else if (m_caret > 0) {
        size_t i = m_caret - 1;
        while (i > 0 && (static_cast<unsigned char>(m_text[i]) & 0xC0) == 0x80)
            i--;
        m_text.erase(i, m_caret - i);
        m_caret = i;
    }
    recompute();
}

void AddressBar::on_left()
{
    if (m_anchor >= 0) {
        m_caret = sel_start();
        m_anchor = -1;
    } else if (m_caret > 0) {
        size_t i = m_caret - 1;
        while (i > 0 && (static_cast<unsigned char>(m_text[i]) & 0xC0) == 0x80)
            i--;
        m_caret = i;
    }
}

void AddressBar::on_right()
{
    if (m_anchor >= 0) {
        m_caret = sel_end();
        m_anchor = -1;
    } else if (m_caret < m_text.size()) {
        gfx::next_cp(m_text, m_caret);
    }
}

void AddressBar::recompute()
{
    m_matches = m_store.suggest(m_text, 3);
}

int AddressBar::suggestion_count() const
{
    return static_cast<int>(m_matches.size()) + 1;  // matches + the Bing tile
}

std::string AddressBar::suggestion_url(int i) const
{
    if (i < static_cast<int>(m_matches.size()))
        return m_matches[static_cast<size_t>(i)].url;
    std::string q;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : m_text) {
        const bool alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum || c == '-' || c == '_' || c == '.' || c == '~') {
            q += static_cast<char>(c);
        } else {
            q += '%';
            q += hex[c >> 4];
            q += hex[c & 15];
        }
    }
    return "http://www.bing.com/search?q=" + q;
}

void AddressBar::move_suggestion(int dir)
{
    m_sug_focus = std::clamp(m_sug_focus + dir, 0, suggestion_count() - 1);
}

AddressBar::Result AddressBar::update(const Pad& pad, bool link_focus_active)
{
    if (const int dx = pad.nav_x()) {
        m_focus = std::clamp(m_focus + dx, 0, 6);
        return Result::None;
    }
    if (!pad.hit(INPUT_GAMEPAD_A) || link_focus_active)
        return Result::None;
    switch (m_focus) {
    case 0: return Result::Back;
    case 1: return Result::Forward;
    case 2: return Result::Edit;
    case 3: return Result::Reload;
    case 4: return Result::ToggleFav;
    case 5: return Result::About;
    default: return Result::Exit;
    }
}

void AddressBar::draw_display(const std::string& url, bool can_back, bool can_fwd, bool is_fav) const
{
    gfx::fill_rect(0, 0, gfx::width(), theme::kTopBarH, theme::kTopBarBg);
    draw_button(0, "←", can_back);
    draw_button(1, "→", can_fwd);
    const bool field_focus = m_focus == 2;
    if (field_focus)
        gfx::fill_rect(kFieldX - 2, kBtnY - 2, kFieldW + 4, kBtnH + 4, theme::kGreen);
    gfx::fill_rect(kFieldX, kBtnY, kFieldW, kBtnH, theme::kFieldBorder);
    gfx::fill_rect(kFieldX + 2, kBtnY + 2, kFieldW - 4, kBtnH - 4, theme::kFieldBg);
    gfx::text(kTextX, kBtnY + 10, 1, theme::kTextDim,
        gfx::truncate_to(url.empty() ? "Search or enter address" : url, 1, kFieldW - 32).c_str());
    draw_button(3, "↻", true);
    draw_button(4, "●", true, is_fav ? theme::kGreen : theme::kTextDim);
    draw_about_button();
    draw_button(6, "X", true);
}

void AddressBar::draw_button(int index, const char* glyph, bool enabled, uint32_t color) const
{
    static const float kXs[7] = { kBackX, kFwdX, kFieldX, kRefreshX, kFavX, kAboutX, kCloseX };
    if (index == 2)
        return;  // the field draws itself
    const float x = kXs[index];
    if (m_focus == index)
        gfx::fill_rect(x - 2, kBtnY - 2, kBtnH + 4, kBtnH + 4, theme::kGreen);
    gfx::fill_rect(x, kBtnY, kBtnH, kBtnH, theme::kFieldBg);
    const float tw = gfx::text_width(glyph, 2);
    const uint32_t c = enabled ? (color != 0 ? color : theme::kText) : theme::kTextDim;
    gfx::text(x + (kBtnH - tw) / 2, kBtnY + (kBtnH - 40.0f) / 2, 2, c, glyph);
}

void AddressBar::draw_about_button() const
{
    if (m_focus == 5)
        gfx::fill_rect(kAboutX - 2, kBtnY - 2, kBtnH + 4, kBtnH + 4, theme::kGreen);
    gfx::fill_rect(kAboutX, kBtnY, kBtnH, kBtnH, theme::kFieldBg);
    const float x = kAboutX + kBtnH / 2, y = kBtnY + kBtnH / 2;
    gfx::fill_rect(x - 4, y - 4, 8, 8, theme::kTextDim);
    for (int i = 0; i < 8; i++) {
        const float a = i * 3.14159265f / 4.0f;
        gfx::fill_rect(x + 9.0f * cosf(a) - 2, y + 9.0f * sinf(a) - 2, 4, 4, theme::kTextDim);
    }
}

void AddressBar::draw_edit() const
{
    gfx::fill_rect(0, 0, gfx::width(), theme::kTopBarH, theme::kTopBarBg);
    gfx::fill_rect(kFieldX - 2, kBtnY - 2, kFieldW + 4, kBtnH + 4, theme::kGreen);
    gfx::fill_rect(kFieldX, kBtnY, kFieldW, kBtnH, theme::kFieldBorder);
    gfx::fill_rect(kFieldX + 2, kBtnY + 2, kFieldW - 4, kBtnH - 4, theme::kFieldBg);

    // Keep the caret visible: shift the rendered text left when it's long.
    float shift = 0;
    const std::string upto_caret = m_text.substr(0, m_caret);
    const float caret_w = gfx::text_width(upto_caret, 1);
    if (caret_w > kFieldW - 48)
        shift = caret_w - (kFieldW - 48);

    const size_t ss = sel_start(), se = sel_end();
    const float w1 = gfx::text_width(m_text.substr(0, ss), 1);
    const float w2 = gfx::text_width(m_text.substr(ss, se - ss), 1);
    if (m_anchor >= 0 && w2 > 0)
        gfx::fill_rect(kTextX + w1 - shift, kBtnY + 6, w2, kBtnH - 12, theme::kGreen);
    gfx::text(kTextX - shift, kBtnY + 10, 1, theme::kText, m_text.c_str());
    if (m_anchor < 0 && (m_frame / 30) % 2 == 0)
        gfx::fill_rect(kTextX + caret_w - shift, kBtnY + 8, 2, kBtnH - 16, theme::kText);

    // Suggestion tiles: matches, then "Search for ... on Bing".
    const int n = suggestion_count();
    for (int i = 0; i < n; i++) {
        const float x = 24 + i * (kSugW + kSugGap);
        const bool focused = m_sug_focus == i;
        if (focused)
            gfx::fill_rect(x - 3, theme::kSugY - 3, kSugW + 6, theme::kSugH + 6, theme::kGreen);
        gfx::fill_rect(x, theme::kSugY, kSugW, theme::kSugH, theme::kTileBg);
        std::string title, url;
        if (i < static_cast<int>(m_matches.size())) {
            title = m_matches[static_cast<size_t>(i)].name;
            url = m_matches[static_cast<size_t>(i)].url;
        } else {
            title = "Search for '" + gfx::truncate_to(m_text, 1, kSugW - 60) + "' on Bing";
            url = "www.bing.com";
        }
        const auto lines = gfx::wrap(gfx::truncate_to(title, 1, kSugW - 24), 1, kSugW - 24);
        for (size_t l = 0; l < lines.size() && l < 2; l++)
            gfx::text(x + 12, theme::kSugY + 14 + l * 24, 1, theme::kTileText, lines[l].c_str());
        gfx::text(x + 12, theme::kSugY + theme::kSugH - 28, 1, theme::kTileUrl,
            gfx::truncate_to(url, 1, kSugW - 24).c_str());
    }
    m_frame++;
}
