// DVD hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// The implementations are in runtime/src/hle/storage/dvd.cpp and keep the Mario Kart Wii address in
// their names; only the registration is per game. On the Wii the DVD driver talks to /dev/di through
// IOS, so replacing DVDInit and the three read entry points keeps every request off the IOS layer.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

extern "C" {
void DVDInit_8015EA1C();
int32_t DVDReadPrio_8015E834(uint32_t f, uint32_t b, int32_t l, int32_t o, int32_t p);
int32_t DVD__ReadAsyncPrio_HLE_8015e74c(uint32_t f, uint32_t b, int32_t l, int32_t o, uint32_t cb, int32_t p);
int32_t DVD__ReadAbsAsyncPrio_HLE_801628cc(uint32_t cb, uint32_t b, int32_t l, int32_t o, uint32_t cbfn, int32_t p);
int32_t DVDLowInit_80164848();
int32_t DVDCheckDevice_801643FC();
}

PPC_NATIVE_OVERRIDE_VOID(801CAD30, DVDInit_8015EA1C, (), ());
PPC_NATIVE_OVERRIDE(801CAB20, DVDReadPrio_8015E834, int32_t, (uint32_t f, uint32_t b, int32_t l, int32_t o, int32_t p), (f, b, l, o, p));
PPC_NATIVE_OVERRIDE(801CAA30, DVD__ReadAsyncPrio_HLE_8015e74c, int32_t,
         (uint32_t f, uint32_t b, int32_t l, int32_t o, uint32_t cb, int32_t p),
         (f, b, l, o, cb, p));
PPC_NATIVE_OVERRIDE(801CECC0, DVD__ReadAbsAsyncPrio_HLE_801628cc, int32_t,
         (uint32_t cb, uint32_t b, int32_t l, int32_t o, uint32_t cbfn, int32_t p),
         (cb, b, l, o, cbfn, p));
// DVDLowInit opens /dev/di through IOS (it calls IPCCltInit and IOS_Open).
PPC_NATIVE_OVERRIDE(801D1230, DVDLowInit_80164848, int32_t, (), ());

// The drive check `main` runs first: it sends a DVDLow request, spins on the callback and compares the
// reply with a dev-kit signature. Without a drive this reports "ready" (the Mario Kart Wii stub).
PPC_NATIVE_OVERRIDE(801D0CF0, DVDCheckDevice_801643FC, int32_t, (), ());

#endif // MKW_HOOK_PROFILE_SMNE01
