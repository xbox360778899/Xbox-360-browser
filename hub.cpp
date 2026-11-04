#include "hub.h"
#include "gfx.h"
#include "theme.h"

#include <algorithm>

namespace {
constexpr float kTabY = 92;
constexpr float kTileY = 140, kTileW = 240, kTileH = 340, kTileGap = 10;
constexpr int kVisible = 5;

const uint32_t kBrandColors[] = { 0xFF0066CC, 0xFFC8232C, 0xFF1DA1F2, 0xFF1877F2,
    0xFF107C10, 0xFF0063DC, 0xFF6E2C91, 0xFFCC2200 };

const Entry kFeatured[] = {
    { "Xbox.com", "http://www.xbox.com" },
    { "Bing", "http://www.bing.com" },
    { "MSN", "http://www.msn.com" },
    { "Wikipedia", "http://www.wikipedia.org" },
    { "Facebook", "http://www.facebook.com" },
    { "Twitter", "http://www.twitter.com" },
    { "GitHub", "http://www.github.com" },
    { "Example", "http://example.com" },
};

uint32_t tile_color(const std::string& url)
{
    if (url.find("xbox.com") != std::string::npos) return 0xFF107C10;
    if (url.find("twitter") != std::string::npos) return 0xFF1DA1F2;
    if (url.find("facebook") != std::string::npos) return 0xFF1877F2;
    if (url.find("bing.com") != std::string::npos) return 0xFF00809D;
    uint32_t h = 0;
    for (char c : url)
        h = h * 31 + static_cast<unsigned char>(c);
    return kBrandColors[h % (sizeof kBrandColors / sizeof kBrandColors[0])];
}

std::string display_url(const std::string& url)
{
    if (url.rfind("http://", 0) == 0)
        return url.substr(7);
    if (url.rfind("https://", 0) == 0)
        return url.substr(8);
    return url;
}
} // namespace

const std::vector<Entry>& Hub::entries(const Store& store, std::vector<Entry>& scratch) const
{
    switch (m_tab) {
    case Recent:
        return store.history();
    case Featured:
        scratch.assign(kFeatured, kFeatured + sizeof kFeatured / sizeof kFeatured[0]);
        return scratch;
    default:
        return store.favorites();
    }
}

HubAction Hub::update(const Pad& pad, Store& store, bool page_loaded)
{
    HubAction a;
    if (pad.hit(INPUT_GAMEPAD_B)) {
        if (m_edit)
            m_edit = false;
        else if (page_loaded)
            a.hide_hub = true;
        return a;
    }
    if (pad.nav_y()) {
        m_tab = static_cast<Tab>((static_cast<int>(m_tab) + (pad.nav_y() > 0 ? 1 : 2)) % 3);
        m_focus = 0;
        m_first = 0;
        m_edit = false;
    }
    std::vector<Entry> scratch;
    const std::vector<Entry>& list = entries(store, scratch);
    const int n = static_cast<int>(list.size());
    if (n == 0)
        return a;
    if (pad.nav_x())
        m_focus = std::clamp(m_focus + pad.nav_x(), 0, n - 1);
    if (m_focus < m_first)
        m_first = m_focus;
    if (m_focus >= m_first + kVisible)
        m_first = m_focus - kVisible + 1;
    if (pad.hit(INPUT_GAMEPAD_A)) {
        if (m_edit && m_tab == Favorites) {
            store.remove_favorite(static_cast<size_t>(m_focus));
            m_focus = std::min(m_focus, static_cast<int>(store.favorites().size()) - 1);
            if (m_focus < 0)
                m_edit = false;
        } else {
            a.navigate = list[static_cast<size_t>(m_focus)].url;
        }
    }
    if (pad.hit(INPUT_GAMEPAD_X) && m_tab == Favorites && !store.favorites().empty())
        m_edit = !m_edit;
    return a;
}

void Hub::draw(const Store& store) const
{
    static const char* kIcons[3] = { "●", "◆", "▲" };
    static const char* kLabels[3] = { "favorites", "recent", "featured" };
    float x = 24;
    for (int t = 0; t < 3; t++) {
        const bool active = static_cast<int>(m_tab) == t;
        const uint32_t c = active ? theme::kText : theme::kTextDim;
        gfx::text(x, kTabY, 1, c, kIcons[t]);
        x += gfx::text_width(kIcons[t], 1) + 8;
        gfx::text(x, kTabY, 1, c, kLabels[t]);
        x += gfx::text_width(kLabels[t], 1) + 48;
    }
    if (m_edit)
        gfx::text(gfx::width() - 320, kTabY, 1, theme::kHintB, "A on a tile removes it");

    std::vector<Entry> scratch;
    const std::vector<Entry>& list = entries(store, scratch);
    if (list.empty()) {
        gfx::text(24, kTileY + 40, 1, theme::kTextDim,
            m_tab == Favorites ? "No favorites yet. Open a page and press X to add one."
                               : "Nothing here yet.");
        return;
    }
    for (int i = m_first; i < m_first + kVisible && i < static_cast<int>(list.size()); i++) {
        const Entry& e = list[static_cast<size_t>(i)];
        const float tx = 24 + (i - m_first) * (kTileW + kTileGap);
        const bool focused = i == m_focus;
        if (focused)
            gfx::fill_rect(tx - 3, kTileY - 3, kTileW + 6, kTileH + 6, theme::kGreen);
        gfx::fill_rect(tx, kTileY, kTileW, kTileH, theme::kTileBg);
        gfx::fill_rect(tx + 8, kTileY + 8, kTileW - 16, 150, 0xFFF4F4F4);
        const std::string word = gfx::truncate_to(e.name, 2, kTileW - 48);
        const float ww = gfx::text_width(word, 2);
        gfx::text(tx + (kTileW - ww) / 2, kTileY + 70, 2, tile_color(e.url), word.c_str());
        gfx::text(tx + 12, kTileY + 170, 1, theme::kTileText,
            gfx::truncate_to(e.name, 1, kTileW - 24).c_str());
        gfx::text(tx + 12, kTileY + kTileH - 28, 1, theme::kTileUrl,
            gfx::truncate_to(display_url(e.url), 1, kTileW - 24).c_str());
    }
}
