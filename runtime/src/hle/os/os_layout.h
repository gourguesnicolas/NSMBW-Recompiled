#pragma once

// OS data and guest functions the OS HLE relies on, per game.
//
// The offsets inside SDK structures (OSThread, OSMessageQueue, OSMutex) and the low-memory globals
// at 0x800000xx are the same in every Wii SDK and stay in os_internal.h. What differs between games
// is where the linker placed the OS's own static variables and its internal functions, so those are
// chosen here by the project's hook profile (runtime.hook_profile in the manifest).
//
// The Mario Kart Wii block is the original set. The SMNE01 block was read from the game's own OS
// code; each address is explained in docs/nsmbw/os-layout.md.

#include <cstdint>

namespace OsHleInternal {

#if defined(MKW_HOOK_PROFILE_SMNE01)

// Not located yet: only the interrupt hooks use it, and they are not registered for this profile.
constexpr uint32_t kInterruptHandlerTablePtrAddr = 0u;

constexpr uint32_t kDefaultThreadContextAddr = 0x8038E938u;   // DefaultThread, set up by OSInitThreads
constexpr uint32_t kIdleThreadContextAddr = 0x8038ED50u;      // IdleThread, right after RunQueue
constexpr uint32_t kThreadQueueArrayAddr = 0x8038EC50u;       // RunQueue[32]
constexpr uint32_t kSwitchThreadCallbackPtrAddr = 0x80429530u; // SwitchThreadCallback
constexpr uint32_t kSchedulerIdleFlagAddr = 0x8042A700u;      // Reschedule: scheduler disable count
constexpr uint32_t kSchedulerReschedCounterAddr = 0x8042A704u; // RunQueueHint
constexpr uint32_t kSchedulerPendingFlagAddr = 0x8042A708u;   // RunQueueBits
constexpr uint32_t kAlarmQueueOffsetFromR13 = 0x5000u;        // AlarmQueue at r13 - 0x5000

// Guest functions the HLE calls back into, and the address stored as a new thread's return address.
constexpr uint32_t kGuestOsInitContext = 0x801ACFB0u;
constexpr uint32_t kGuestOsSaveContext = 0x801ACDB0u;
constexpr uint32_t kGuestOsUnlockAllMutex = 0x801B3010u;
constexpr uint32_t kGuestOsExitThread = 0x801B53A0u;
constexpr uint32_t kGuestOsInsertAlarm = 0x801AB2F0u;    // static InsertAlarm; OSSetAlarm is 0x250 after it
constexpr uint32_t kGuestOsTimeHelper = 0x801B6020u;     // called by OSSetPeriodicAlarm before InsertAlarm

// Mario Kart Wii only: these two tweaks matched its THP decoder threads and its scheduler start-up.
// A zero address makes the code that reads them skip the step.
constexpr uint32_t kSchedulerInitFlagAddr = 0u;
constexpr uint32_t kThreadAttrSourceAddr = 0u;
constexpr bool kHasThpDecoderThreads = false;

#else // Mario Kart Wii

constexpr uint32_t kInterruptHandlerTablePtrAddr = 0x803868f8u;

constexpr uint32_t kDefaultThreadContextAddr = 0x80347498u;
constexpr uint32_t kIdleThreadContextAddr = 0x803478b0u;
constexpr uint32_t kThreadQueueArrayAddr = 0x803477b0u;
constexpr uint32_t kSwitchThreadCallbackPtrAddr = 0x80385ae0u;
constexpr uint32_t kSchedulerIdleFlagAddr = 0x80386918u;
constexpr uint32_t kSchedulerReschedCounterAddr = 0x8038691cu;
constexpr uint32_t kSchedulerPendingFlagAddr = 0x80386920u;
constexpr uint32_t kAlarmQueueOffsetFromR13 = 0x6360u;

constexpr uint32_t kGuestOsInitContext = 0x801A20BCu;
constexpr uint32_t kGuestOsSaveContext = 0x801A1ED8u;
constexpr uint32_t kGuestOsUnlockAllMutex = 0x801A8088u;
constexpr uint32_t kGuestOsExitThread = 0x801AA0F0u;
constexpr uint32_t kGuestOsInsertAlarm = 0x801A0620u;
constexpr uint32_t kGuestOsTimeHelper = 0x801AADE0u;

constexpr uint32_t kSchedulerInitFlagAddr = 0x80347130u;
constexpr uint32_t kThreadAttrSourceAddr = 0x80385AA8u;
constexpr bool kHasThpDecoderThreads = true;

#endif

} // namespace OsHleInternal
