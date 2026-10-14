// Wii Remote and GameCube pad hooks for New Super Mario Bros. Wii, North American release, revision 2
// (SMNE01).
//
// The implementations are in runtime/src/hle/input/*.cpp and keep the Mario Kart Wii address in their
// names; only the registration is per game. The Wii Remote stack talks to the Bluetooth controller
// through IOS, so the library entry points are replaced and the host's controllers feed KPAD instead.
// The addresses come from docs/nsmbw/hle-hooks.md.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"
#include "memory.h"

extern "C" {
int32_t WPADInit_HLE();
int32_t WPADGetStatus_HLE();
int32_t WPADGetDataFormat_HLE(uint32_t chan);
int32_t WPADSetDataFormat_HLE(uint32_t chan, int32_t format);
int32_t WPADProbe_HLE(uint32_t chan, uint32_t typePtr);
void WPADControlMotor_HLE(uint32_t chan, uint32_t command);
int32_t WPADGetInfoAsync_HLE(uint32_t chan, uint32_t infoPtr, uint32_t callback);
int32_t WPADControlLed_HLE(uint32_t chan, uint32_t ledMask, uint32_t callback);
int32_t WPADStartSimpleSync_HLE();
int32_t WPADStopSimpleSync_HLE();
uint32_t WPADSetSyncDeviceCallback_HLE(uint32_t callback);
int32_t KPAD__Read_HLE(uint32_t chan, uint32_t statusPtr, uint32_t count);
}

// The Bluetooth stack start-up (WUDInit, 0x801F9E10) waits, sleeping, until the state machine reports
// "ready" through the predicate at 0x801FCF30; the library init (0x801DF7F0) powers the sensor bar
// through the Hollywood GPIO register 0xCD0000C0, which has no backing store. Both are replaced by the
// library initialisation of the shared implementation.
extern "C" int32_t WUDIsReady_HLE_801FCF30()
{
    return 1;
}

// KPADReadEx: the same read as KPADRead plus an error word. 0 is KPAD_READ_ERR_NONE, -1 NO_DATA.
extern "C" int32_t KPAD__ReadEx_HLE_801ED3B0(uint32_t chan, uint32_t statusPtr, uint32_t count, uint32_t errPtr)
{
    const int32_t entries = KPAD__Read_HLE(chan, statusPtr, count);
    if (errPtr != 0) {
        Memory::Write32(errPtr, entries > 0 ? 0u : 0xFFFFFFFFu);
    }
    return entries;
}

PPC_NATIVE_OVERRIDE(801DF7F0, WPADInit_HLE, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801F9E10, WPADInit_HLE, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801FCF30, WUDIsReady_HLE_801FCF30, int32_t, (), ());

// The WPAD public API: thin wrappers (0x801DFAC0..0x801DFB30) over the control block at 0x803A3120, and
// the per-channel calls (0x801E0F40..0x801E1630) that look the channel's block up in the table at 0x8039E230.
PPC_NATIVE_OVERRIDE(801DFAC0, WPADStartSimpleSync_HLE, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801DFAD0, WPADStopSimpleSync_HLE, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801DFAF0, WPADSetSyncDeviceCallback_HLE, uint32_t, (uint32_t callback), (callback));
PPC_NATIVE_OVERRIDE(801DFB30, WPADGetStatus_HLE, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801E0F40, WPADProbe_HLE, int32_t, (uint32_t chan, uint32_t typePtr), (chan, typePtr));
PPC_NATIVE_OVERRIDE(801E1150, WPADGetDataFormat_HLE, int32_t, (uint32_t chan), (chan));
PPC_NATIVE_OVERRIDE(801E11A0, WPADSetDataFormat_HLE, int32_t, (uint32_t chan, int32_t format), (chan, format));
PPC_NATIVE_OVERRIDE(801E12C0, WPADGetInfoAsync_HLE, int32_t,
         (uint32_t chan, uint32_t infoPtr, uint32_t callback), (chan, infoPtr, callback));
PPC_NATIVE_OVERRIDE_VOID(801E14E0, WPADControlMotor_HLE, (uint32_t chan, uint32_t command), (chan, command));
PPC_NATIVE_OVERRIDE(801E1630, WPADControlLed_HLE, int32_t,
         (uint32_t chan, uint32_t ledMask, uint32_t callback), (chan, ledMask, callback));

// KPADRead (0x801ED3A0) and KPADReadEx (0x801ED3B0) are two entries into the same body (0x801ED3C0).
PPC_NATIVE_OVERRIDE(801ED3A0, KPAD__Read_HLE, int32_t,
         (uint32_t chan, uint32_t statusPtr, uint32_t count), (chan, statusPtr, count));
PPC_NATIVE_OVERRIDE(801ED3B0, KPAD__ReadEx_HLE_801ED3B0, int32_t,
         (uint32_t chan, uint32_t statusPtr, uint32_t count, uint32_t errPtr), (chan, statusPtr, count, errPtr));

#endif // MKW_HOOK_PROFILE_SMNE01
