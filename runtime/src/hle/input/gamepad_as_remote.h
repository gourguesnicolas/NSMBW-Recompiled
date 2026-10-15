#pragma once

#include "wii_remote_input.h"

#include <cstdint>

namespace GamepadAsRemote {

// Fills `sample` with a virtual Wii Remote made from the first gamepad and the keyboard. True for
// channel 0, false for the others. Used only when no real Wii Remote is present.
bool Read(uint32_t chan, WiiRemoteInput::KpadSample& sample);

} // namespace GamepadAsRemote
