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
#include "memory.h"

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
uint32_t __OSSetInterruptHandler_801a65f8_hle(uint32_t interrupt, uint32_t handler);
uint32_t OS____InterruptInit_801a661c(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r8);
uint32_t SetInterruptMask_801a66e0(uint32_t mask, uint32_t enable);
uint32_t OS____MaskInterrupts_801a693c(uint32_t mask, uint32_t unmask);
uint32_t __OSUnmaskInterrupts_801a69bc_hle(uint32_t mask);
uint32_t EXIInit_80168fa0();
void SetExiInterruptMask_80167e78(uint32_t channel, uint32_t exi_struct_ptr);
uint32_t EXIImm_80167f68(uint32_t channel, uint32_t buffer, uint32_t length, uint32_t type, uint32_t callback);
uint32_t EXIDma_80168288(uint32_t channel, uint32_t buffer, uint32_t length, uint32_t type, uint32_t callback);
uint32_t EXISync_80168380(uint32_t channel);
uint32_t EXISelect_801689d0(uint32_t channel, uint32_t device, uint32_t frequency);
uint32_t EXIDeselect_80168b00(uint32_t channel);
uint32_t EXIUnlock_80169260(uint32_t channel);
void SIInit_801b2de0();
int32_t IPCCltInit_80193478(CpuContext* ctx);
uint32_t OS____InitMemoryProtection_801a7dfc(uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r8);
uint32_t OSGetResetCode_801a8a50();
void HLE_SISetSamplingRate_801b3acc(uint32_t msec);
uint32_t __OSInitSTM_HLE_801ab848(CpuContext* ctx);
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
// __OSInterruptInit and the controller mask code, which read and write the PI/MI registers
// (0xCC003004, 0xCC004010) that the runtime does not emulate.
REGISTER_NATIVE_FUNCTION(0x801B11B0, __OSSetInterruptHandler_801a65f8_hle);
PPC_NATIVE_OVERRIDE(801B11E0, OS____InterruptInit_801a661c, uint32_t, (uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r8), (r3, r4, r5, r6, r7, r8));
PPC_NATIVE_OVERRIDE(801B12B0, SetInterruptMask_801a66e0, uint32_t, (uint32_t mask, uint32_t enable), (mask, enable));
PPC_NATIVE_OVERRIDE(801B1510, OS____MaskInterrupts_801a693c, uint32_t, (uint32_t mask, uint32_t unmask), (mask, unmask));
REGISTER_NATIVE_FUNCTION(0x801B1590, __OSUnmaskInterrupts_801a69bc_hle);

// EXI (EXI.c): the memory card, RTC and SRAM ports. Their registers are at 0xCD006800 and nothing in
// this runtime answers them, so the same stubs as for Mario Kart Wii report success. The control block
// array they index is at 0x8038F710 (0x40 bytes per channel).
PPC_NATIVE_OVERRIDE(801B9BA0, EXIInit_80168fa0, uint32_t, (), ());
PPC_NATIVE_OVERRIDE_VOID(801B8A30, SetExiInterruptMask_80167e78, (uint32_t channel, uint32_t exi_struct_ptr), (channel, exi_struct_ptr));
PPC_NATIVE_OVERRIDE(801B8B20, EXIImm_80167f68, uint32_t, (uint32_t channel, uint32_t buffer, uint32_t length, uint32_t type, uint32_t callback), (channel, buffer, length, type, callback));
PPC_NATIVE_OVERRIDE(801B8E50, EXIDma_80168288, uint32_t, (uint32_t channel, uint32_t buffer, uint32_t length, uint32_t type, uint32_t callback), (channel, buffer, length, type, callback));
PPC_NATIVE_OVERRIDE(801B8F50, EXISync_80168380, uint32_t, (uint32_t channel), (channel));
PPC_NATIVE_OVERRIDE(801B95C0, EXISelect_801689d0, uint32_t, (uint32_t channel, uint32_t device, uint32_t frequency), (channel, device, frequency));
PPC_NATIVE_OVERRIDE(801B96F0, EXIDeselect_80168b00, uint32_t, (uint32_t channel), (channel));
PPC_NATIVE_OVERRIDE(801B9E70, EXIUnlock_80169260, uint32_t, (uint32_t channel), (channel));

// Serial interface and STM (SI.c, OSStateTM.c): both open hardware this runtime does not model.
REGISTER_NATIVE_FUNCTION(0x801BAEF0, SIInit_801b2de0);
PPC_NATIVE_OVERRIDE_VOID(801BB840, HLE_SISetSamplingRate_801b3acc, (uint32_t msec), (msec));
PPC_NATIVE_OVERRIDE(801B6820, __OSInitSTM_HLE_801ab848, uint32_t, (CpuContext* ctx), (ctx));

// Memory protection (MI registers at 0xCC004000) and the reset code (PI register 0xCC003024).
PPC_NATIVE_OVERRIDE(801B2D50, OS____InitMemoryProtection_801a7dfc, uint32_t, (uint32_t r3, uint32_t r4, uint32_t r5, uint32_t r6, uint32_t r7, uint32_t r8), (r3, r4, r5, r6, r7, r8));
PPC_NATIVE_OVERRIDE(801B3C90, OSGetResetCode_801a8a50, uint32_t, (), ());

// IPC client (ipcMain.c): initialises the IPC buffer window, skips the interrupt handler and the
// IPC registers at 0xCD000000.
PPC_NATIVE_OVERRIDE(802247D0, IPCCltInit_80193478, int32_t, (CpuContext* ctx), (ctx));

// Alarms (OSAlarm.c).
PPC_NATIVE_OVERRIDE_VOID(801AB540, OSSetAlarm_HLE_801a0870, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AB5B0, OS__SetPeriodicAlarm_801a08e0, (CpuContext* ctx), (ctx));

// EXILock (EXIBios.c, 0x801B9D70). EXIUnlock is stubbed (above), so the lock bit the real EXILock tests
// would never be cleared and the next user, the ROM font read in OSInitFont, would wait forever. There
// is no other EXI client on this runtime, so the lock is always granted.
extern "C" uint32_t EXILock_HLE_801B9D70(uint32_t channel, uint32_t device, uint32_t callback)
{
    (void)channel;
    (void)device;
    (void)callback;
    return 1;
}
PPC_NATIVE_OVERRIDE(801B9D70, EXILock_HLE_801B9D70, uint32_t, (uint32_t channel, uint32_t device, uint32_t callback), (channel, device, callback));

// __OSReadROM (0x801B4310): copies from the console's boot ROM over EXI channel 0. The ROM holds the
// system fonts the game's dRomFontMgr loads. There is no boot ROM image here (it is not part of the game
// and cannot be shipped), so the read returns zeros, which is what a console without the ROM data gives.
extern "C" uint32_t OSReadROM_HLE_801B4310(uint32_t buffer, uint32_t length, uint32_t offset)
{
    (void)offset;
    for (uint32_t i = 0; i < length; ++i) {
        Memory::Write8(buffer + i, 0);
    }
    return 1;
}
PPC_NATIVE_OVERRIDE(801B4310, OSReadROM_HLE_801B4310, uint32_t, (uint32_t buffer, uint32_t length, uint32_t offset), (buffer, length, offset));

// Font encoding (OSFont.c, OSGetFontEncode at 0x801B00A0). The SDK decides between ANSI and Shift-JIS by
// reading the video interface's DTV status register (0xCC00206E) when the boot info does not say. The
// function is small enough that the translator inlines it at its callers; registering it here keeps the
// inlining off. This is the North American release, so the answer is ANSI (0). The two statics are the
// cached encoding (r13 - 24920, 0xFFFF until first asked) and the word the SDK stores next to it
// (r13 - 20424, the address of the function after it).
extern "C" uint32_t OSGetFontEncode_HLE_801B00A0()
{
    constexpr uint32_t kFontEncodeCache = 0x80429528u;
    constexpr uint32_t kFontEncodeAux = 0x8042A6B8u;
    const uint32_t cached = Memory::Read16(kFontEncodeCache);
    if (cached == 0xFFFFu) {
        Memory::Write16(kFontEncodeCache, 0);
        Memory::Write32(kFontEncodeAux, 0x801B05A0u);
        return 0;
    }
    return cached;
}
PPC_NATIVE_OVERRIDE(801B00A0, OSGetFontEncode_HLE_801B00A0, uint32_t, (), ());

#endif // MKW_HOOK_PROFILE_SMNE01
