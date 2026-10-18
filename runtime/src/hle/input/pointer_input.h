#pragma once

#include "wii_remote_input.h"

// The Wii Remote's IR pointer, from the mouse or a gamepad's right stick. The SDL Wii driver gives no IR
// data and the virtual remote has none, so without this the game always sees the remote pointing away
// from the screen, and pointer-only screens (the HOME Menu) cannot be used.
//
// The mouse points where the cursor is over the game picture; the left button is A and the right
// button is B while the cursor is over it. The right stick moves the pointer from where it is. The
// pointer counts as on screen while it is over the picture and was moved in the last few seconds, so
// it disappears when left alone, like a remote put down.
namespace PointerInput {

// Adds the pointer (and the mouse buttons) to `sample`. Call once per KPAD read for channel 0.
void Apply(WiiRemoteInput::KpadSample& sample);

} // namespace PointerInput
