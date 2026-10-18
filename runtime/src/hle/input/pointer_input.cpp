#include "pointer_input.h"

#include "runtime_config.h"

#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace PointerInput {
namespace {

// WPAD_BUTTON_* bits as KPADStatus.hold carries them.
constexpr uint32_t kWpadA = 0x0800, kWpadB = 0x0400;

// How long the pointer stays on screen after the last movement.
constexpr std::chrono::seconds kIdleTimeout{5};
// Right-stick speed: full deflection crosses the screen in about a second (60 reads per second).
constexpr float kStickSpeed = 2.0f / 60.0f;
constexpr float kStickDeadZone = 0.2f;

struct Rect {
    float x, y, w, h;
};

// Where the game picture is inside the window: the whole window when it is stretched (video.widescreen),
// otherwise a centred 16:9 (video.force_16_9) or 4:3 area, as main.cpp presents it.
Rect PictureRect(float windowWidth, float windowHeight) {
    if (RuntimeConfigFile::WidescreenEnabled(true) && !RuntimeConfigFile::ForceAspect169Enabled(false)) {
        return {0.0f, 0.0f, windowWidth, windowHeight};
    }
    const float aspect = RuntimeConfigFile::ForceAspect169Enabled(false) ? 16.0f / 9.0f : 4.0f / 3.0f;
    float w = windowWidth;
    float h = w / aspect;
    if (h > windowHeight) {
        h = windowHeight;
        w = h * aspect;
    }
    return {(windowWidth - w) / 2.0f, (windowHeight - h) / 2.0f, w, h};
}

struct State {
    float pos[2] = {};
    float lastMouse[2] = {-1.0f, -1.0f};
    bool everMoved = false;
    std::chrono::steady_clock::time_point lastMove{};
};

State g_state;

} // namespace

void Apply(WiiRemoteInput::KpadSample& sample) {
    const auto now = std::chrono::steady_clock::now();
    bool overPicture = false;

    // Mouse: only while the cursor is over this process's window.
    if (SDL_Window* window = SDL_GetMouseFocus()) {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);
        float mx = 0.0f;
        float my = 0.0f;
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mx, &my);
        if (width > 0 && height > 0) {
            const Rect picture = PictureRect(static_cast<float>(width), static_cast<float>(height));
            const float x = (mx - picture.x) / picture.w * 2.0f - 1.0f;
            const float y = (my - picture.y) / picture.h * 2.0f - 1.0f;
            overPicture = x >= -1.0f && x <= 1.0f && y >= -1.0f && y <= 1.0f;
            if (mx != g_state.lastMouse[0] || my != g_state.lastMouse[1]) {
                g_state.lastMouse[0] = mx;
                g_state.lastMouse[1] = my;
                if (overPicture) {
                    g_state.pos[0] = x;
                    g_state.pos[1] = y;
                    g_state.everMoved = true;
                    g_state.lastMove = now;
                } else {
                    // Moved off the picture: the remote points away from the screen.
                    g_state.lastMove = {};
                }
            }
            if (overPicture) {
                if (buttons & SDL_BUTTON_LMASK) sample.hold |= kWpadA;
                if (buttons & SDL_BUTTON_RMASK) sample.hold |= kWpadB;
            }
        }
    }

    // Right stick: moves the pointer from where it is.
    const float sx = sample.pointerStick[0];
    const float sy = sample.pointerStick[1];
    if (std::fabs(sx) > kStickDeadZone || std::fabs(sy) > kStickDeadZone) {
        g_state.pos[0] = std::clamp(g_state.pos[0] + sx * kStickSpeed, -1.0f, 1.0f);
        g_state.pos[1] = std::clamp(g_state.pos[1] - sy * kStickSpeed, -1.0f, 1.0f);
        g_state.everMoved = true;
        g_state.lastMove = now;
    }

    sample.pointerValid = g_state.everMoved && now - g_state.lastMove < kIdleTimeout;
    sample.pointer[0] = g_state.pos[0];
    sample.pointer[1] = g_state.pos[1];
}

} // namespace PointerInput
