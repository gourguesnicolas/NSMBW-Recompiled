// Guest functions the hand-written OS/audio/network HLE calls back into.
//
// Those routines were written against Mario Kart Wii, so they name its functions by address
// (`func_801A0620`, ...). With MKW_NATIVE_HOOKS=0 the HLE entry points are not registered and the
// translator produces no function at these addresses, so the symbols would be undefined. Define
// them as hard stops: reaching one means a game other than Mario Kart Wii executed HLE code that
// has not been ported to its addresses yet (docs/nsmbw/hle-hooks.md, issue #51).
#include "hle_stubs.h"
#include "runtime_log.h"

#if !MKW_NATIVE_HOOKS

namespace {
[[noreturn]] void HaltMkwCallback(uint32_t address) {
    RT_LOGF(RT_TAG_OS,
            "HLE reached a Mario Kart Wii function at guest address 0x%08X.\n"
            "[os] This routine has not been ported to this game's addresses yet.\n",
            address);
    std::fflush(stderr);
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "HLE code needed the Mario Kart Wii function at 0x%08X, which this game does not "
                  "have. The hook has to be ported to this game's addresses first.", address);
    ShowRuntimeFatalPopup("the runtime used a function that belongs to another game", message);
    std::abort();
}
} // namespace

#define MKW_GUEST_CALLBACK(addr) \
    extern "C" void func_##addr(CpuContext*) { HaltMkwCallback(0x##addr); }

MKW_GUEST_CALLBACK(8012B830) // AXFX reverb callback
MKW_GUEST_CALLBACK(801A0620) // alarm insertion
MKW_GUEST_CALLBACK(801A1ED8) // OSSaveContext, used by the scheduler
MKW_GUEST_CALLBACK(801A961C) // OSInitAlarm
MKW_GUEST_CALLBACK(801AADE0) // time conversion used by periodic alarms
MKW_GUEST_CALLBACK(801D8D30) // NHTTPStartup
MKW_GUEST_CALLBACK(801D9E94) // NHTTP
MKW_GUEST_CALLBACK(8055531C) // StaticRProlog

#undef MKW_GUEST_CALLBACK

#endif // !MKW_NATIVE_HOOKS
