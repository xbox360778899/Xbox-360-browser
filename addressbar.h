#pragma once
#include "input.h"
#include "keyboard.h"
#include "store.h"
#include <cstdint>
#include <string>
#include <vector>

// Top chrome: back / forward / address field / refresh / favorites / about / close,
// in display mode and edit mode (field + suggestion tiles + keyboard target).
class AddressBar : public IKeyboardTarget {
public:
    enum class Result { None, Back, Forward, Reload, ToggleFav, About, Exit, Edit };

    explicit AddressBar(Store& store) : m_store(store) {}

    // Display mode (PAGE / HUB).
    Result update(const Pad& pad, bool link_focus_active);
    void draw_display(const std::string& url, bool can_back, bool can_fwd, bool is_fav) const;

    // Edit mode.
    void begin_edit(const std::string& url);
    void end_edit() { m_editing = false; m_sug_focus = -1; }
    bool editing() const { return m_editing; }
    const std::string& text() const { return m_text; }
    bool enter_pending() const { return m_enter_pending; }
    void clear_enter() { m_enter_pending = false; }

    int suggestion_count() const;
    std::string suggestion_url(int i) const;
    bool suggestions_focused() const { return m_sug_focus >= 0; }
    void suggestions_focus(bool on) { m_sug_focus = on ? 0 : -1; }
    void move_suggestion(int dir);

    void draw_edit() const;

    // IKeyboardTarget.
    void on_char(const std::string& s) override;
    void on_backspace() override;
    void on_enter() override { m_enter_pending = true; }
    void on_left() override;
    void on_right() override;
    void on_hide() override {}

private:
    void recompute();
    size_t sel_start() const;
    size_t sel_end() const;
    void erase_selection();
    void draw_button(int index, const char* glyph, bool enabled, uint32_t color = 0) const;
    void draw_about_button() const;

    Store& m_store;
    std::string m_text;
    size_t m_caret = 0;
    int m_anchor = -1;             // selection: [anchor, caret), -1 = none
    bool m_editing = false;
    bool m_enter_pending = false;
    std::vector<Entry> m_matches;  // up to 3; the Bing tile is implicit
    int m_sug_focus = -1;          // -1 = keyboard has focus, else tile index
    mutable int m_frame = 0;
    int m_focus = 2;               // display-mode focus: 0 back,1 fwd,2 field,3 refresh,4 fav,5 about,6 close
};
