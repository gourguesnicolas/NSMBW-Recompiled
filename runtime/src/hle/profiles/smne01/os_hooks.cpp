// OS hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// The OS HLE in runtime/src/hle/os replaces SDK functions that cannot run as translated code: thread
// switching, interrupts, message queues. Its registrations there carry Mario Kart Wii addresses. This
// file registers the same implementations at the addresses of this game, which were found by reading
// its OS code (see docs/nsmbw/hle-hooks.md for the evidence and the confidence of each one).
//
// This directory is the project's runtime.native_registration_root: the translator reads the
// registrations from here and nowhere else, so it removes exactly these addresses from the translated
// graph.
#if defined(MKW_HOOK_PROFILE_SMNE01)

// The project turns the Mario Kart Wii hooks off for the rest of the runtime (MKW_NATIVE_HOOKS=0).
// This file is the one place that still wants the registration macros.
#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

// Implementations in runtime/src/hle/os. Their names keep the Mario Kart Wii address they were
// written against; only the registration below is per game.
extern "C" {
void OSCreateThread_HLE_801a9e84(CpuContext* ctx);
void OSExitThread_HLE_801aa0f0(CpuContext* ctx);
void OSCancelThread_HLE_801aa1d4(CpuContext* ctx);
void OSJoinThread_HLE_801aa3ac(CpuContext* ctx);
void OSDetachThread_HLE_801aa4ec(CpuContext* ctx);
void OSSuspendThread_HLE_801aa6a8(CpuContext* ctx);
void OSResumeThread_HLE_801aa58c(CpuContext* ctx);
void OSSleepThread_HLE_801aa9b8(CpuContext* ctx);
void OSWakeupThread_HLE_801aaaa4(CpuContext* ctx);
void SelectThread_801a9c08(CpuContext* ctx);
void OSLockMutex_HLE_801a7ee4(CpuContext* ctx);
void OSUnlockMutex_HLE_801a7fc0(CpuContext* ctx);
void OS__InitMessageQueue_HLE_801a72fc(CpuContext* ctx);
int32_t OS__SendMessage_HLE_801a735c(CpuContext* ctx);
int32_t OS__ReceiveMessage_HLE_801a7424(CpuContext* ctx);
void OS__ClearContext_801a2098(uint32_t contextAddr);
void OS__SetCurrentContext_801a1e70(uint32_t contextAddr);
uint32_t OS__GetCurrentThread_801a98b0_hle();
[[noreturn]] void OS__LoadContext_801a1f58(CpuContext* ctx);
int32_t OS__DisableInterrupts_801a65ac();
int32_t OS__EnableInterrupts_801a65c0();
int32_t OS__RestoreInterrupts_801a65d4(int32_t level);
void OSSetAlarm_HLE_801a0870(CpuContext* ctx);
void OS__SetPeriodicAlarm_801a08e0(CpuContext* ctx);
}

// Threads (OSThread.c).
PPC_NATIVE_OVERRIDE_VOID(801B5130, OSCreateThread_HLE_801a9e84, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B53A0, OSExitThread_HLE_801aa0f0, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5490, OSCancelThread_HLE_801aa1d4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5670, OSJoinThread_HLE_801aa3ac, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B57C0, OSDetachThread_HLE_801aa4ec, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5B00, OSSuspendThread_HLE_801aa6a8, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5860, OSResumeThread_HLE_801aa58c, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5C90, OSSleepThread_HLE_801aa9b8, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B5D80, OSWakeupThread_HLE_801aaaa4, (CpuContext* ctx), (ctx));
// The static SelectThread(BOOL yield): the scheduler's choice of the next thread.
PPC_NATIVE_OVERRIDE_VOID(801B4EA0, SelectThread_801a9c08, (CpuContext* ctx), (ctx));

// Mutexes (OSMutex.c).
PPC_NATIVE_OVERRIDE_VOID(801B2E60, OSLockMutex_HLE_801a7ee4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801B2F40, OSUnlockMutex_HLE_801a7fc0, (CpuContext* ctx), (ctx));

// Message queues (OSMessage.c). OSJamMessage is not in this game's image.
REGISTER_NATIVE_FUNCTION(0x801B2390, OS__InitMessageQueue_HLE_801a72fc);
REGISTER_NATIVE_FUNCTION(0x801B23F0, OS__SendMessage_HLE_801a735c);
REGISTER_NATIVE_FUNCTION(0x801B24C0, OS__ReceiveMessage_HLE_801a7424);

// Contexts (OSContext.c).
PPC_NATIVE_OVERRIDE_VOID(801ACF80, OS__ClearContext_801a2098, (uint32_t contextAddr), (contextAddr));
PPC_NATIVE_OVERRIDE_VOID(801ACD40, OS__SetCurrentContext_801a1e70, (uint32_t contextAddr), (contextAddr));
REGISTER_NATIVE_FUNCTION(0x801B4B30, OS__GetCurrentThread_801a98b0_hle);
PPC_NATIVE_OVERRIDE_VOID(801ACE30, OS__LoadContext_801a1f58, (CpuContext* ctx), (ctx));

// Interrupts (OSInterrupt.c): the three leaf functions the rest of the OS calls constantly.
PPC_NATIVE_OVERRIDE(801B1140, OS__DisableInterrupts_801a65ac, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801B1160, OS__EnableInterrupts_801a65c0, int32_t, (), ());
PPC_NATIVE_OVERRIDE(801B1180, OS__RestoreInterrupts_801a65d4, int32_t, (int32_t level), (level));

// Alarms (OSAlarm.c).
PPC_NATIVE_OVERRIDE_VOID(801AB540, OSSetAlarm_HLE_801a0870, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AB5B0, OS__SetPeriodicAlarm_801a08e0, (CpuContext* ctx), (ctx));

#endif // MKW_HOOK_PROFILE_SMNE01
