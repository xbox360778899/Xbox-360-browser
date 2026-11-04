#pragma once
#include <cstdint>
#include <xsdk/kernel.h>  // INPUT_STATE, INPUT_GAMEPAD_*, XamInputGetState

class Pad {
public:
    void update();
    bool connected() const { return m_connected; }
    bool held(uint32_t button) const;
    bool hit(uint32_t button) const;      // pressed this frame
    int nav_x() const { return m_navx; }  // dpad or left stick, with auto-repeat
    int nav_y() const { return m_navy; }
    int stick_ry() const { return m_state.gamepad.thumb_ry; }

private:
    static int step(int dir, int& prev_dir, int& frames);
    INPUT_STATE m_state = {};
    uint32_t m_prev = 0;
    int m_dir_x = 0, m_dir_y = 0;
    int m_frames_x = 0, m_frames_y = 0;
    int m_navx = 0, m_navy = 0;
    bool m_connected = false;
};
