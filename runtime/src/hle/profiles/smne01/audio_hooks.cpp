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
void AIInit_801240b0(uint32_t callback_stack_switch);
uint32_t AICheckInit_80124094();
void AIInitDMA_80123fcc(uint32_t start_addr, uint32_t length);
uint32_t AIRegisterDMACallback_80123f88(uint32_t callback);
void AIStartDMA_80124048();
uint32_t AIGetDMABytesLeft_8012405c();
uint32_t AIGetDMAStartAddr_8012406c();
uint32_t AIGetDMALength_80124084();
void DSPInit_8015d444();
uint32_t DSPCheckInit_8015d504();
uint32_t DSPAddTask_8015d50c(uint32_t task_ptr);
void __DSP_boot_task_8015dc60(uint32_t task_ptr);
void __AXOutInitDSP_801269bc(CpuContext* ctx);
void DSPSendMailToDSP_8015d430(uint32_t mail);
uint32_t DSPCheckMailToDSP_8015d3fc();
uint32_t DSPCheckMailFromDSP_8015d40c();
uint32_t DSPReadMailFromDSP_8015d41c();
uint32_t DSPAssertTask_8015d57c(uint32_t taskPtr);
}

// The three OS functions that reach the DSP and AI registers (OSAudioSystem.c). __OSInitAudioSystem
// calls __AIClockInit first and then boots the DSP, so the order below is the order in memory.
PPC_NATIVE_OVERRIDE_VOID(801ABF60, AIClockInit_801A1138, (uint32_t clock_mode), (clock_mode));
PPC_NATIVE_OVERRIDE_VOID(801AC180, OSInitAudioSystem_801A1358, (), ());
PPC_NATIVE_OVERRIDE_VOID(801AC350, OSStopAudioSystem_801A1520, (), ());

// Audio interface (AI.c). The register accessors are at 0x8019F0B0..0x8019F1E0, AIInit follows them.
PPC_NATIVE_OVERRIDE_VOID(8019F1F0, AIInit_801240b0, (uint32_t callback_stack_switch), (callback_stack_switch));
REGISTER_NATIVE_FUNCTION(0x8019F1E0, AICheckInit_80124094);
PPC_NATIVE_OVERRIDE(8019F0B0, AIRegisterDMACallback_80123f88, uint32_t, (uint32_t callback), (callback));
PPC_NATIVE_OVERRIDE_VOID(8019F100, AIInitDMA_80123fcc, (uint32_t start_addr, uint32_t length), (start_addr, length));
PPC_NATIVE_OVERRIDE_VOID(8019F180, AIStartDMA_80124048, (), ());
PPC_NATIVE_OVERRIDE(8019F1A0, AIGetDMABytesLeft_8012405c, uint32_t, (), ());
PPC_NATIVE_OVERRIDE(8019F1B0, AIGetDMAStartAddr_8012406c, uint32_t, (), ());
PPC_NATIVE_OVERRIDE(8019F1D0, AIGetDMALength_80124084, uint32_t, (), ());

// DSP driver (dsp.c, dsp_task.c) at 0x801D56E0..0x801D5F70. The AX task is set up by __AXOutInitDSP.
PPC_NATIVE_OVERRIDE(801D56E0, DSPCheckMailToDSP_8015d3fc, uint32_t, (), ());
REGISTER_NATIVE_FUNCTION(0x801D56F0, DSPCheckMailFromDSP_8015d40c);
REGISTER_NATIVE_FUNCTION(0x801D5700, DSPReadMailFromDSP_8015d41c);
PPC_NATIVE_OVERRIDE_VOID(801D5720, DSPSendMailToDSP_8015d430, (uint32_t mail), (mail));
PPC_NATIVE_OVERRIDE_VOID(801D5740, DSPInit_8015d444, (), ());
REGISTER_NATIVE_FUNCTION(0x801D5800, DSPCheckInit_8015d504);
REGISTER_NATIVE_FUNCTION(0x801D5810, DSPAddTask_8015d50c);
PPC_NATIVE_OVERRIDE(801D5880, DSPAssertTask_8015d57c, uint32_t, (uint32_t taskPtr), (taskPtr));
PPC_NATIVE_OVERRIDE_VOID(801D5F70, __DSP_boot_task_8015dc60, (uint32_t task_ptr), (task_ptr));
PPC_NATIVE_OVERRIDE_VOID(801A1CE0, __AXOutInitDSP_801269bc, (CpuContext* ctx), (ctx));

#endif // MKW_HOOK_PROFILE_SMNE01
