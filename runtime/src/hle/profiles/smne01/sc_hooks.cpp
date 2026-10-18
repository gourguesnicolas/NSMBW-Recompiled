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
#include "memory.h"

extern "C" {
uint32_t SCGetAspectRatio_HLE();
void func_801DB8E0(CpuContext* ctx);
}

bool NandProcessPendingCallbacks(CpuContext* cpu, int maxToProcess);

namespace {
// SC library state byte (r13 - 19504): 1 while the asynchronous read of SYSCONF runs, 3 once it has
// completed and the file waits to be parsed, 0 after a good parse, 2 on error.
constexpr uint32_t kScStatusAddr = 0x8042F680u - 19504u;
constexpr uint8_t kScStatusBusy = 1;
} // namespace

// SCCheckStatus (0x801DB8E0): OSInit polls it until the SYSCONF read is done; when the state says the
// read has completed, this is also the function that parses the file into the item table every SC
// getter reads. The read is open -> read -> close through the asynchronous IOS calls, whose guest
// callbacks the runtime only delivers from its alarm pump, so deliver them here first and then run
// the original function. If the read still has not finished (nothing left to deliver), answer
// "success" as before rather than let OSInit spin; the getters then keep their built-in defaults.
extern "C" void SCCheckStatus_SMNE01(CpuContext* ctx) {
    for (int round = 0; round < 8 && Memory::Read8(kScStatusAddr) == kScStatusBusy; ++round) {
        if (!NandProcessPendingCallbacks(ctx, 64)) {
            break;
        }
    }
    func_801DB8E0(ctx);
    if (ctx->gpr[3] == kScStatusBusy) {
        ctx->gpr[3] = 0;
    }
}
REGISTER_NATIVE_FUNCTION_AS(0x801DB8E0, SCCheckStatus_SMNE01, "SCCheckStatus_SMNE01");

// SCGetAspectRatio (0x801DD1D0): the video.widescreen setting of the runtime, not the SYSCONF item.
PPC_NATIVE_OVERRIDE(801DD1D0, SCGetAspectRatio_HLE, uint32_t, (), ());

#endif // MKW_HOOK_PROFILE_SMNE01
