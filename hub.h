#pragma once
#include "input.h"
#include "store.h"
#include <string>
#include <vector>

struct HubAction {
    std::string navigate;   // non-empty: navigate here
    bool hide_hub = false;  // B with a page behind
};

// The Web Hub: favorites / recent / featured tabs with a horizontal tile strip.
class Hub {
public:
    HubAction update(const Pad& pad, Store& store, bool page_loaded);
    void draw(const Store& store) const;
    bool edit_mode() const { return m_edit; }

private:
    enum Tab { Favorites, Recent, Featured };
    const std::vector<Entry>& entries(const Store& store, std::vector<Entry>& scratch) const;

    Tab m_tab = Favorites;
    int m_focus = 0;
    int m_first = 0;   // first visible tile (horizontal scroll)
    bool m_edit = false;
};
