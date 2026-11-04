#pragma once
#include "input.h"
#include <string>

class IKeyboardTarget {
public:
    virtual ~IKeyboardTarget() = default;
    virtual void on_char(const std::string& utf8) = 0;
    virtual void on_backspace() = 0;
    virtual void on_enter() = 0;
    virtual void on_left() = 0;
    virtual void on_right() = 0;
    virtual void on_hide() = 0;
};

// The on-screen keyboard from the reference photo: 4 glyph rows, a right-hand
// modifier column (BACKSPACE / SYMBOLS / ACCENTS / CAPS) and a bottom row
// ([kb] [<-] SPACE [->] .COM ENTER).
class Keyboard {
public:
    void show() { m_visible = true; reset_focus(); }
    void hide() { m_visible = false; }
    bool visible() const { return m_visible; }
    void reset_focus() { m_row = 1; m_col = 1; m_page = 0; m_a_frames = 0; }

    // Returns true if nav-up was pressed on the top row; the caller then moves
    // focus to the suggestion row above.
    bool update(const Pad& pad, IKeyboardTarget& target);
    void draw() const;

private:
    bool m_visible = false;
    int m_page = 0;   // 0 lower, 1 upper, 2 symbols, 3 accents
    int m_row = 1;    // 0..3 glyph rows, 4 bottom row
    int m_col = 1;    // 0..9, col 10 = right-hand column (rows 0..3), 0..5 on row 4
    int m_a_frames = 0;
};
