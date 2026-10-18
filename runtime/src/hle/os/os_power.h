#pragma once

// What the runtime does when the guest asks the console to leave the game. The runtime is the console,
// so returning to the Wii Menu or powering off closes it, and a reset starts a new instance.
// Neither returns. `reason` goes to the log.
[[noreturn]] void RuntimeGuestPowerOff(const char* reason);
[[noreturn]] void RuntimeGuestRestart(const char* reason);
