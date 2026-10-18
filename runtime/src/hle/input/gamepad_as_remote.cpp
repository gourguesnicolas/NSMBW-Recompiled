// A virtual Wii Remote for games that only read Wii Remotes (New Super Mario Bros. Wii).
//
// Mario Kart Wii also accepts a GameCube pad, so its keyboard and gamepad bindings go through the PAD
// library. A game without GameCube pad support sees nothing from them. When no real Wii Remote is paired
// this turns the first gamepad (and a default keyboard layout) into a Wii Remote held sideways: the
// directions, "2" (jump), "1" (run), A, B, + and -. The game's own sideways handling then sees ordinary
// KPAD input. Channel 0 only.
#include "gamepad_as_remote.h"

#include "input_bindings.h"

#include <SDL3/SDL_keyboard.h>
#include <dolphin/pad.h>

#include <algorithm>
#include <cstdint>

namespace {

// WPAD_BUTTON_* bits as KPADStatus.hold carries them.
constexpr uint32_t kWpadLeft = 0x0001, kWpadRight = 0x0002, kWpadDown = 0x0004, kWpadUp = 0x0008,
                   kWpadPlus = 0x0010, kWpadTwo = 0x0100, kWpadOne = 0x0200, kWpadB = 0x0400,
                   kWpadA = 0x0800, kWpadMinus = 0x1000, kWpadHome = 0x8000;

constexpr int kStickThreshold = 40;

// Held sideways (buttons 1 and 2 on the right), the remote's physical Up points to the player's left.
// The game's sideways handling expects raw bits of the remote as held, so the player's directions are
// the remote's rotated ones.
void AddDirections(uint32_t& hold, bool left, bool right, bool up, bool down) {
    if (left) hold |= kWpadUp;
    if (right) hold |= kWpadDown;
    if (up) hold |= kWpadRight;
    if (down) hold |= kWpadLeft;
}

} // namespace

namespace GamepadAsRemote {

bool Read(uint32_t chan, WiiRemoteInput::KpadSample& sample) {
    if (chan != 0) {
        return false;
    }
    static bool padInitialised = false;
    if (!padInitialised) {
        PADInit();
        padInitialised = true;
    }

    PADStatus statuses[PAD_CHANMAX]{};
    PADRead(statuses);
    InputBindings::Apply(statuses);

    uint32_t hold = 0;
    const PADStatus& pad = statuses[0];
    if (pad.err == PAD_ERR_NONE) {
        const uint16_t b = pad.button;
        AddDirections(hold, (b & PAD_BUTTON_LEFT) || pad.stickX < -kStickThreshold,
                      (b & PAD_BUTTON_RIGHT) || pad.stickX > kStickThreshold,
                      (b & PAD_BUTTON_UP) || pad.stickY > kStickThreshold,
                      (b & PAD_BUTTON_DOWN) || pad.stickY < -kStickThreshold);
        if (b & PAD_BUTTON_A) hold |= kWpadTwo;
        if (b & PAD_BUTTON_B) hold |= kWpadOne;
        if (b & PAD_BUTTON_X) hold |= kWpadA;
        if (b & PAD_BUTTON_Y) hold |= kWpadB;
        if (b & PAD_BUTTON_START) hold |= kWpadPlus;
        if (b & PAD_TRIGGER_Z) hold |= kWpadMinus;
        if (b & PAD_TRIGGER_L) hold |= kWpadHome;
    }
    const float stickX = pad.err == PAD_ERR_NONE ? pad.substickX / 100.0f : 0.0f;
    const float stickY = pad.err == PAD_ERR_NONE ? pad.substickY / 100.0f : 0.0f;

    int keyCount = 0;
    const bool* keys = SDL_GetKeyboardState(&keyCount);
    const auto down = [&](SDL_Scancode code) { return keys != nullptr && code < keyCount && keys[code]; };
    AddDirections(hold, down(SDL_SCANCODE_LEFT), down(SDL_SCANCODE_RIGHT), down(SDL_SCANCODE_UP),
                  down(SDL_SCANCODE_DOWN));
    if (down(SDL_SCANCODE_SPACE) || down(SDL_SCANCODE_X)) hold |= kWpadTwo;
    if (down(SDL_SCANCODE_LSHIFT) || down(SDL_SCANCODE_Z)) hold |= kWpadOne;
    if (down(SDL_SCANCODE_RETURN)) hold |= kWpadA;
    if (down(SDL_SCANCODE_BACKSPACE)) hold |= kWpadB;
    if (down(SDL_SCANCODE_P)) hold |= kWpadPlus;
    if (down(SDL_SCANCODE_M)) hold |= kWpadMinus;
    if (down(SDL_SCANCODE_H)) hold |= kWpadHome;

    sample = {};
    sample.hold = hold;
    sample.acc[1] = -1.0f; // at rest, as the real remote reports it
    // The right stick moves the IR pointer (PointerInput).
    sample.pointerStick[0] = std::clamp(stickX, -1.0f, 1.0f);
    sample.pointerStick[1] = std::clamp(stickY, -1.0f, 1.0f);
    return true;
}

} // namespace GamepadAsRemote
