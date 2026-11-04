#include "input.h"

namespace {
constexpr int kDeadzone = 20000;
constexpr int kRepeatDelay = 18;  // frames before auto-repeat starts (~0.3 s)
constexpr int kRepeatRate = 6;    // frames between repeats (~0.1 s)
}

void Pad::update()
{
    m_connected = XamInputGetState(0, 0, &m_state) == ERROR_SUCCESS;
    if (!m_connected)
        m_state = INPUT_STATE{};
    const uint32_t now = m_state.gamepad.buttons;

    int dx = 0;
    if (now & INPUT_GAMEPAD_DPAD_LEFT) dx = -1;
    else if (now & INPUT_GAMEPAD_DPAD_RIGHT) dx = 1;
    else if (m_state.gamepad.thumb_lx < -kDeadzone) dx = -1;
    else if (m_state.gamepad.thumb_lx > kDeadzone) dx = 1;

    int dy = 0;
    if (now & INPUT_GAMEPAD_DPAD_UP) dy = -1;
    else if (now & INPUT_GAMEPAD_DPAD_DOWN) dy = 1;
    else if (m_state.gamepad.thumb_ly > kDeadzone) dy = -1;  // stick up reads positive
    else if (m_state.gamepad.thumb_ly < -kDeadzone) dy = 1;

    m_navx = step(dx, m_dir_x, m_frames_x);
    m_navy = step(dy, m_dir_y, m_frames_y);
    m_prev = now;
}

bool Pad::held(uint32_t button) const
{
    return (m_state.gamepad.buttons & button) != 0;
}

bool Pad::hit(uint32_t button) const
{
    return held(button) && (m_prev & button) == 0;
}

int Pad::step(int dir, int& prev_dir, int& frames)
{
    if (dir == 0) {
        prev_dir = 0;
        frames = 0;
        return 0;
    }
    if (dir != prev_dir) {
        prev_dir = dir;
        frames = 1;
        return dir;
    }
    frames++;
    if (frames > kRepeatDelay && (frames - kRepeatDelay) % kRepeatRate == 0)
        return dir;
    return 0;
}
