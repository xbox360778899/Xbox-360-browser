#include "keyboard.h"
#include "gfx.h"
#include "theme.h"

#include <xsdk/kernel.h>

#include <algorithm>

namespace {
enum Action { Char, Backspace, Enter, Space, Com, Left, Right, Hide, Page };
struct Key {
    const char* label;
    const char* out;
    Action action;
    int page;
};

#define K(s) { s, s, Char, 0 }

const Key kLower[4][10] = {
    { K("1"), K("2"), K("3"), K("4"), K("5"), K("6"), K("7"), K("8"), K("9"), K("0") },
    { K("q"), K("w"), K("e"), K("r"), K("t"), K("y"), K("u"), K("i"), K("o"), K("p") },
    { K("a"), K("s"), K("d"), K("f"), K("g"), K("h"), K("j"), K("k"), K("l"), K("-") },
    { K("z"), K("x"), K("c"), K("v"), K("b"), K("n"), K("m"), K("/"), K("@"), K(".") },
};
const Key kUpper[4][10] = {
    { K("1"), K("2"), K("3"), K("4"), K("5"), K("6"), K("7"), K("8"), K("9"), K("0") },
    { K("Q"), K("W"), K("E"), K("R"), K("T"), K("Y"), K("U"), K("I"), K("O"), K("P") },
    { K("A"), K("S"), K("D"), K("F"), K("G"), K("H"), K("J"), K("K"), K("L"), K("-") },
    { K("Z"), K("X"), K("C"), K("V"), K("B"), K("N"), K("M"), K("/"), K("@"), K(".") },
};
const Key kSymbols[4][10] = {
    { K("!"), K("@"), K("#"), K("$"), K("%"), K("^"), K("&"), K("*"), K("("), K(")") },
    { K("~"), K("`"), K("_"), K("-"), K("+"), K("="), K("{"), K("}"), K("["), { "\\", "\\", Char, 0 } },
    { K(":"), K(";"), K("\""), K("'"), K("<"), K(">"), K(","), K("."), K("?"), K("/") },
    { K("¢"), K("£"), K("¥"), K("©"), K("®"), K("°"), K("±"), K("×"), K("÷"), K("¿") },
};
const Key kAccents[4][10] = {
    { K("à"), K("á"), K("â"), K("ã"), K("ä"), K("å"), K("æ"), K("ç"), K("è"), K("é") },
    { K("ê"), K("ë"), K("ì"), K("í"), K("î"), K("ï"), K("ñ"), K("ò"), K("ó"), K("ô") },
    { K("õ"), K("ö"), K("ø"), K("ù"), K("ú"), K("û"), K("ü"), K("ý"), K("ÿ"), K("þ") },
    { K("ð"), K("œ"), K("š"), K("ž"), K("À"), K("Á"), K("Â"), K("Ã"), K("Ä"), K("Å") },
};

const Key kRightLower[4] = {
    { "BACKSPACE", nullptr, Backspace, 0 },
    { "SYMBOLS", nullptr, Page, 2 },
    { "ACCENTS", nullptr, Page, 3 },
    { "CAPS", nullptr, Page, 1 },
};
const Key kRightUpper[4] = {
    { "BACKSPACE", nullptr, Backspace, 0 },
    { "SYMBOLS", nullptr, Page, 2 },
    { "ACCENTS", nullptr, Page, 3 },
    { "abc", nullptr, Page, 0 },
};
const Key kRightSymbols[4] = {
    { "BACKSPACE", nullptr, Backspace, 0 },
    { "abc", nullptr, Page, 0 },
    { "ACCENTS", nullptr, Page, 3 },
    { "", nullptr, Char, 0 },
};
const Key kRightAccents[4] = {
    { "BACKSPACE", nullptr, Backspace, 0 },
    { "abc", nullptr, Page, 0 },
    { "SYM", nullptr, Page, 2 },
    { "", nullptr, Char, 0 },
};

const Key kBottom[6] = {
    { "", nullptr, Hide, 0 },
    { "←", nullptr, Left, 0 },
    { "SPACE", " ", Space, 0 },
    { "→", nullptr, Right, 0 },
    { ".COM", ".com", Com, 0 },
    { "ENTER", nullptr, Enter, 0 },
};
const float kBottomUnits[6] = { 1, 1, 6, 1, 2, 2 };

const Key* page_rows(int page)
{
    switch (page) {
    case 1: return &kUpper[0][0];
    case 2: return &kSymbols[0][0];
    case 3: return &kAccents[0][0];
    default: return &kLower[0][0];
    }
}

const Key* page_right(int page)
{
    switch (page) {
    case 1: return kRightUpper;
    case 2: return kRightSymbols;
    case 3: return kRightAccents;
    default: return kRightLower;
    }
}

int max_col(int row) { return row == 4 ? 6 : 11; }

const Key* key_at(int page, int row, int col)
{
    if (row < 4)
        return col < 10 ? page_rows(page) + row * 10 + col : page_right(page) + row;
    return kBottom + (col < 6 ? col : 0);
}

void key_rect(int row, int col, float& x, float& y, float& w, float& h)
{
    h = theme::kKeyH;
    if (row < 4) {
        y = theme::kKeyY + row * (theme::kKeyH + theme::kKeyGap);
        if (col < 10) {
            x = 24.0f + col * 96.0f;
            w = 90.0f;
        } else {
            x = 994.0f;
            w = 266.0f;
        }
    } else {
        y = theme::kKeyY + 4 * (theme::kKeyH + theme::kKeyGap);
        x = 24.0f;
        for (int c = 0; c < col; c++)
            x += kBottomUnits[c] * 90.0f + theme::kKeyGap;
        w = kBottomUnits[col] * 90.0f;
    }
}

// A tiny keyboard glyph for the hide key, echoing the one in the photo.
void draw_keyboard_icon(float x, float y, float w, float h)
{
    const float bw = w * 0.6f, bh = h * 0.42f;
    const float bx = x + (w - bw) / 2, by = y + (h - bh) / 2 - 3;
    gfx::fill_rect(bx, by, bw, bh, theme::kTextDim);
    for (int r = 0; r < 2; r++)
        for (int c = 0; c < 4; c++)
            gfx::fill_rect(bx + 4 + c * (bw - 8) / 4, by + 4 + r * (bh - 8) / 2,
                (bw - 8) / 4 - 3, (bh - 8) / 2 - 3, theme::kKeyBg);
    gfx::fill_rect(x + w / 2 - 10, y + h - 12, 20, 5, theme::kTextDim);
}

void press(const Key& k, IKeyboardTarget& target)
{
    switch (k.action) {
    case Char:
        if (k.out)
            target.on_char(k.out);
        break;
    case Space: target.on_char(" "); break;
    case Com: target.on_char(".com"); break;
    case Backspace: target.on_backspace(); break;
    case Enter: target.on_enter(); break;
    case Left: target.on_left(); break;
    case Right: target.on_right(); break;
    case Hide: target.on_hide(); break;
    case Page: break;  // handled by Keyboard (needs member access)
    }
}

void draw_key(const Key& k, int row, int col, bool focused)
{
    float x, y, w, h;
    key_rect(row, col, x, y, w, h);
    if (focused)
        gfx::fill_rect(x - 3, y - 3, w + 6, h + 6, theme::kGreen);
    gfx::fill_rect(x, y, w, h, theme::kKeyBg);
    if (k.action == Hide && row == 4 && col == 0) {
        draw_keyboard_icon(x, y, w, h);
        return;
    }
    if (!k.label || !*k.label)
        return;
    const float tw = gfx::text_width(k.label, 1);
    gfx::text(x + (w - tw) / 2, y + (h - 20.0f) / 2, 1, theme::kText, k.label);
}
} // namespace

bool Keyboard::update(const Pad& pad, IKeyboardTarget& target)
{
    if (pad.nav_y() < 0 && m_row == 0)
        return true;

    if (const int dx = pad.nav_x()) {
        m_col = std::clamp(m_col + dx, 0, max_col(m_row) - 1);
        m_a_frames = 0;
    }
    if (const int dy = pad.nav_y()) {
        m_row = std::clamp(m_row + dy, 0, 4);
        m_col = std::clamp(m_col, 0, max_col(m_row) - 1);
        m_a_frames = 0;
    }

    const Key* k = key_at(m_page, m_row, m_col);
    if (pad.hit(INPUT_GAMEPAD_A)) {
        m_a_frames = 1;
        if (k->action == Page)
            m_page = k->page;
        else
            press(*k, target);
    } else if (pad.held(INPUT_GAMEPAD_A) && k->action == Char) {
        if (++m_a_frames > 18 && m_a_frames % 6 == 0)
            press(*k, target);
    } else {
        m_a_frames = 0;
    }
    return false;
}

void Keyboard::draw() const
{
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 10; c++)
            draw_key(page_rows(m_page)[r * 10 + c], r, c, m_row == r && m_col == c);
        draw_key(page_right(m_page)[r], r, 10, m_row == r && m_col == 10);
    }
    for (int c = 0; c < 6; c++)
        draw_key(kBottom[c], 4, c, m_row == 4 && m_col == c);
}
