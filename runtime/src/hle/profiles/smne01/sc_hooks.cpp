// System configuration (SYSCONF) hooks for New Super Mario Bros. Wii, North American release,
// revision 2 (SMNE01).
//
// The SC library reads /shared2/sys/SYSCONF through the NAND and reports its progress through a status
// word that OSInit polls. The implementations are in runtime/src/hle/sc.cpp and keep the Mario Kart Wii
// address in their names; only the registration is per game.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

extern "C" {
uint32_t SCCheckStatus_HLE();
uint32_t SCGetAspectRatio_HLE();
}

// SCCheckStatus (0x801DB8E0): returns the library state, which stays 1 (busy) until the asynchronous
// read of SYSCONF completes. There is no IOS to complete it, so the answer is always "success" (0).
PPC_NATIVE_OVERRIDE(801DB8E0, SCCheckStatus_HLE, uint32_t, (), ());

// SCGetAspectRatio (0x801DD1D0): the video.widescreen setting of the runtime, not the SYSCONF item.
PPC_NATIVE_OVERRIDE(801DD1D0, SCGetAspectRatio_HLE, uint32_t, (), ());

#endif // MKW_HOOK_PROFILE_SMNE01
