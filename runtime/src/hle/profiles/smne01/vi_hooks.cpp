// Video interface hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// The implementations are in runtime/src/hle/vi.cpp and keep the Mario Kart Wii address in their names;
// only the registration is per game. The addresses come from docs/nsmbw/hle-hooks.md; the VI statics
// they keep consistent are in runtime/src/hle/os/os_layout.h.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"
#include "memory.h"

extern "C" {
void VIInit_HLE_801b94a4(CpuContext* ctx);
void __VIInit_HLE_801b9294(CpuContext* ctx);
void VISetPreRetraceCallback_HLE_801b90f4(CpuContext* ctx);
void VISetPostRetraceCallback_HLE_801b9138(CpuContext* ctx);
void VIGetDTVStatus_HLE_801bad38(CpuContext* ctx);
void VIConfigure_HLE_801b9f6c(CpuContext* ctx);
void VIFlush_HLE_801ba9a4(CpuContext* ctx);
void VISetNextFrameBuffer_HLE_801baab8(CpuContext* ctx);
void VIGetNextFrameBuffer_HLE_801bab24(CpuContext* ctx);
void VISetBlack_HLE_801bab2c(CpuContext* ctx);
void VIGetRetraceCount_HLE_801baba4(CpuContext* ctx);
void VIGetNextField_HLE_801babac(CpuContext* ctx);
void VIGetCurrentLine_HLE_801bac48(CpuContext* ctx);
void VIWaitForRetrace_HLE_801b99ec(CpuContext* ctx);
}

PPC_NATIVE_OVERRIDE_VOID(801BC7A0, VIInit_HLE_801b94a4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BC5A0, __VIInit_HLE_801b9294, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BC3E0, VISetPreRetraceCallback_HLE_801b90f4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BC430, VISetPostRetraceCallback_HLE_801b9138, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BE070, VIGetDTVStatus_HLE_801bad38, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BD280, VIConfigure_HLE_801b9f6c, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDCB0, VIFlush_HLE_801ba9a4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDDD0, VISetNextFrameBuffer_HLE_801baab8, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDE40, VIGetNextFrameBuffer_HLE_801bab24, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDE50, VISetBlack_HLE_801bab2c, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDED0, VIGetRetraceCount_HLE_801baba4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDEE0, VIGetNextField_HLE_801babac, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BDF80, VIGetCurrentLine_HLE_801bac48, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801BCCF0, VIWaitForRetrace_HLE_801b99ec, (CpuContext* ctx), (ctx));

// OSSetProgressiveMode (0x801B00F0), reimplemented from the translated code. The original caches the
// VI DTV status (bit 1 of the register at 0xCC00206E) on first use; there is no component cable here,
// so the status reads as zero. Everything else is the same: the cache is a halfword at r13 - 24920
// (0xFFFF = not read yet), the mode argument is stored there when it is 0..5, and the handler pointer at
// r13 - 20424 is set to 0x801B05A0, or to 0x801B06A0 for modes 3..5.
extern "C" void OSSetProgressiveMode_HLE_801b00f0(CpuContext* ctx)
{
    const uint32_t r13 = ctx->gpr[13];
    const uint32_t mode = ctx->gpr[3];
    uint32_t value = ::Memory::Read16(r13 - 24920u);
    if (value == 0xFFFFu) {
        value = 0;
        ::Memory::Write16(r13 - 24920u, static_cast<uint16_t>(value));
        ::Memory::Write32(r13 - 20424u, 0x801B05A0u);
    }
    if (mode <= 5u) {
        ::Memory::Write16(r13 - 24920u, static_cast<uint16_t>(mode));
        if (mode >= 3u) {
            ::Memory::Write32(r13 - 20424u, 0x801B06A0u);
        }
    }
    ctx->gpr[3] = value;
}
PPC_NATIVE_OVERRIDE_VOID(801B00F0, OSSetProgressiveMode_HLE_801b00f0, (CpuContext* ctx), (ctx));

#endif // MKW_HOOK_PROFILE_SMNE01
