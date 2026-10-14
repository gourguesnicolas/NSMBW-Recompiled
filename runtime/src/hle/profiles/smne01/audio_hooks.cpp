// Audio hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// Same arrangement as os_hooks.cpp: the implementations are in runtime/src/hle/audio and keep the
// Mario Kart Wii address in their names; only the registration is per game.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

extern "C" {
void AIClockInit_801A1138(uint32_t clock_mode);
void OSInitAudioSystem_801A1358();
void OSStopAudioSystem_801A1520();
}

// The three OS functions that reach the DSP and AI registers (OSAudioSystem.c). __OSInitAudioSystem
// calls __AIClockInit first and then boots the DSP, so the order below is the order in memory.
PPC_NATIVE_OVERRIDE_VOID(801ABF60, AIClockInit_801A1138, (uint32_t clock_mode), (clock_mode));
PPC_NATIVE_OVERRIDE_VOID(801AC180, OSInitAudioSystem_801A1358, (), ());
PPC_NATIVE_OVERRIDE_VOID(801AC350, OSStopAudioSystem_801A1520, (), ());

#endif // MKW_HOOK_PROFILE_SMNE01
