#include "memory.h"
#include "hle/audio/ax_dsp.h"
#include "hle_stubs.h"
#include "ppc_runtime.h"
#include "runtime_log.h"
#include "system_bridge.h"
#include "os_power.h"
#include "platform/host_platform.h"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>
#include <chrono>
#include <thread>

namespace {

static std::string ReadGuestStringSafe(uint32_t addr)
{
    if (addr == 0) {
        return "";
    }
    std::string out;
    out.reserve(64);
    try {
        for (size_t i = 0; i < 512; ++i) {
            uint8_t b = Memory::Read8(addr + static_cast<uint32_t>(i));
            if (b == 0) break;
            out.push_back(static_cast<char>(b));
        }
    } catch (const Memory::AccessViolation&) {
        out = "<invalid_guest_ptr>";
    }
    return out;
}

// Stops the mix worker before the process ends: it holds resolved host pointers into the guest regions.
void ShutdownBeforeExit()
{
    AxDspHle::ShutdownMixWorker();
    Memory::Reset();
}

} // namespace

void RuntimeGuestPowerOff(const char* reason)
{
    RT_LOGF(RT_TAG_OS, "%s: closing the runtime\n", reason);
    std::fflush(stderr);
    ShutdownBeforeExit();
    SetRuntimeExitCode(0);
    std::exit(EXIT_SUCCESS);
}

void RuntimeGuestRestart(const char* reason)
{
    const bool started = RuntimePlatform::StartNewInstance();
    RT_LOGF(RT_TAG_OS, "%s: %s\n", reason,
            started ? "restarting the runtime" : "could not start a new instance; closing the runtime");
    std::fflush(stderr);
    ShutdownBeforeExit();
    SetRuntimeExitCode(0);
    std::exit(EXIT_SUCCESS);
}

// The OSReset.c entry points a game leaves through, for games whose SDK has them (registered per game).
// The SDK's own path first runs every registered shutdown function in a busy loop until all report ready;
// some wait for asynchronous NAND or DVD completions that the runtime only delivers between guest thread
// switches, so the loop never ends. These replace the whole call, as OSResetSystem does above for Mario
// Kart Wii: the runtime writes NAND files as they are written, so nothing is lost.
extern "C" void OSReturnToMenu_HLE(CpuContext*) { RuntimeGuestPowerOff("OSReturnToMenu (Wii Menu)"); }
extern "C" void OSReturnToDataManager_HLE(CpuContext*) { RuntimeGuestPowerOff("OSReturnToDataManager"); }
extern "C" void OSReturnToMenuInternal_HLE(CpuContext*) { RuntimeGuestPowerOff("__OSReturnToMenu"); }
extern "C" void OSReturnToMenuForError_HLE(CpuContext*) { RuntimeGuestPowerOff("return to the Wii Menu after an error"); }
extern "C" void OSShutdownSystem_HLE(CpuContext*) { RuntimeGuestPowerOff("OSShutdownSystem (power off)"); }
extern "C" void OSRestart_HLE(CpuContext*) { RuntimeGuestRestart("OSRestart (reset)"); }
extern "C" void OSRebootSystem_HLE(CpuContext*) { RuntimeGuestRestart("OSRebootSystem (reset)"); }
extern "C" void OSHotReset_HLE(CpuContext*) { RuntimeGuestRestart("__OSHotReset (reset)"); }

// 0x8012E5A4 -> PPCHalt
extern "C" void PPCHalt_8012E5A4()
{
    // A halt is never a clean shutdown: the guest reaches it after OSPanic or an
    // unrecoverable OS error. Report it like every other fatal path so the run
    // folder gets the same artifact set, and exit non-zero.
    RT_LOGF(RT_TAG_OS, "PPCHalt called, exiting process\n");
    std::fflush(stderr);
    RuntimeCrash::WriteCrashArtifacts("halt", "The guest executed PPCHalt.");
    SetRuntimeExitCode(EXIT_FAILURE);
    ShowRuntimeFatalPopup("the guest operating system halted the console",
                          "The game executed PPCHalt, which the console only reaches after an "
                          "unrecoverable error.");
    MarkFatalErrorReported();
    std::exit(EXIT_FAILURE);
}

PPC_NATIVE_OVERRIDE_VOID(8012E5A4, PPCHalt_8012E5A4, (), ());

// 0x801A2660 -> OS::Panic
extern "C" void OS__Panic_801A2660_Cpu(CpuContext* ctx)
{
    const uint32_t file_ptr = ctx ? ctx->gpr[3] : 0;
    const int line = ctx ? static_cast<int>(ctx->gpr[4]) : 0;
    const uint32_t fmt_ptr = ctx ? ctx->gpr[5] : 0;

    const std::string file = ReadGuestStringSafe(file_ptr);
    const std::string fmt = ReadGuestStringSafe(fmt_ptr);

    std::fprintf(stderr, "\n\n");
    std::fprintf(stderr, "========================================\n");
    RT_LOGF(RT_TAG_OS, "OS::Panic called!\n");
    std::fprintf(stderr, "========================================\n");
    RT_LOGF(RT_TAG_OS, "Location: %s:%d\n", file.empty() ? "<null>" : file.c_str(), line);
    if (!fmt.empty()) {
        RT_LOGF(RT_TAG_OS, "Message: %s\n", fmt.c_str());
    }
    std::fprintf(stderr, "========================================\n");
    std::fflush(stderr);
    
    RT_LOGF(RT_TAG_OS, "Dumping CPU state...\n");
    std::fflush(stderr);
    
    
    RT_LOGF(RT_TAG_OS, "CPU registers:\n");
    if (ctx) {
        std::fprintf(stderr, "  PC: 0x%08X  LR: 0x%08X  CTR: 0x%08X\n", ctx->pc, ctx->lr, ctx->ctr);
        std::fprintf(stderr, "  r1: 0x%08X  r2: 0x%08X  r13: 0x%08X\n", ctx->gpr[1], ctx->gpr[2], ctx->gpr[13]);
        std::fprintf(stderr, "  r3: 0x%08X  r4: 0x%08X  r5: 0x%08X\n", ctx->gpr[3], ctx->gpr[4], ctx->gpr[5]);
    } else {
        std::fprintf(stderr, "  (CPU context is NULL)\n");
    }
    
    std::fprintf(stderr, "========================================\n");
    RT_LOGF(RT_TAG_OS, "Flushing output and exiting...\n");
    std::fflush(stderr);
    std::fflush(stdout);

    std::string details = "The game reported an OS panic";
    if (!file.empty()) {
        details += " at ";
        details += file;
        details += ":";
        details += std::to_string(line);
    }
    if (!fmt.empty()) {
        details += "\n\n";
        details += fmt;
    }
    RuntimeCrash::WriteCrashArtifacts("panic", details);
    SetRuntimeExitCode(EXIT_FAILURE);
    ShowRuntimeFatalPopup("the guest operating system reported a panic", details);
    MarkFatalErrorReported();

    // Give extra time for output to flush with PowerShell redirection
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::exit(EXIT_FAILURE);
}

PPC_NATIVE_OVERRIDE_VOID(801A2660, OS__Panic_801A2660_Cpu, (CpuContext* ctx), (ctx));

// 0x801A8A80 -> OSResetSystem
extern "C" uint32_t OSResetSystem()
{
    RuntimeGuestPowerOff("OSResetSystem: simulating console reset");
}

PPC_NATIVE_OVERRIDE(801A8A80, OSResetSystem, uint32_t, (), ());

// 0x801AE58C -> exit(int status)
extern "C" uint32_t Exit_801AE58C(int status)
{
    RT_LOGF(RT_TAG_OS, "exit(status=%d) called via 0x801AE58C\n", status);
    std::fflush(stderr);
    if (status != 0) {
        const std::string details =
            "The game called exit(" + std::to_string(status) + ").";
        RuntimeCrash::WriteCrashArtifacts("exit", details);
        ShowRuntimeFatalPopup("the game exited with an error status", details);
        MarkFatalErrorReported();
    }
    SetRuntimeExitCode(status);
    std::exit(status);
    return static_cast<uint32_t>(status);
}

PPC_NATIVE_OVERRIDE(801AE58C, Exit_801AE58C, uint32_t, (int status), (status));
