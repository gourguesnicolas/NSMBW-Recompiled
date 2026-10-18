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
int32_t DVDLowClearCoverInterrupt_80166964(uint32_t cb);
int32_t DVDLowInquiry_80165A30(uint32_t b, uint32_t c);
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

// OSInit starts the driver's state machine, which sends these two to /dev/di (ioctls 0x86 and 0x12). The
// DVD HLE never opens /dev/di, so they went to fd -1, the reply was not "transfer complete", the driver
// asked for the error code and set its fatal-error flag (r13 - 19796). DVDGetDriveStatus then returned -1
// for the whole run, and the stream player kept every music stream paused waiting for the disc.
// Same stubs as Mario Kart Wii: report the request as issued and leave IOS out of it.
PPC_NATIVE_OVERRIDE(801D3700, DVDLowClearCoverInterrupt_80166964, int32_t, (uint32_t cb), (cb));
PPC_NATIVE_OVERRIDE(801D2470, DVDLowInquiry_80165A30, int32_t, (uint32_t b, uint32_t c), (b, c));

#endif // MKW_HOOK_PROFILE_SMNE01
