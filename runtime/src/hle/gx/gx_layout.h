#pragma once

// Guest addresses the GX HLE keeps consistent, per game (runtime.hook_profile in the manifest).
//
// The GXData structure and the FIFO objects sit next to each other in the game's .bss, in the same order in
// every build of the SDK, so the SMNE01 addresses are the Mario Kart Wii ones shifted by 0x4C440 (GXData
// 0x8038FC00 was read from the game's own GXInit). The small-data statics were matched one by one against
// the game's GX functions; see docs/nsmbw/hle-hooks.md.

#include <cstdint>

namespace GxLayout {

#if defined(MKW_HOOK_PROFILE_SMNE01)

constexpr uint32_t kGXDataPtr = 0x8042E188u;          // r2 - 20216
constexpr uint32_t kFifoObj = 0x8038FB80u;            // passed to GXInitFifoBase by GXInit
constexpr uint32_t kGXData = 0x8038FC00u;             // 0x600 bytes
constexpr uint32_t kGpFifoObj = 0x80390200u;          // right after GXData
constexpr uint32_t kCpuFifoObj = 0x80390224u;
constexpr uint32_t kDlFifo = 0x803904D0u;             // display list recording FIFO object
constexpr uint32_t kDlSave = 0x80390550u;             // GXData copy taken by GXBeginDisplayList
constexpr uint32_t kSavedCpuFifo = 0x80390B50u;       // CPU FIFO saved around a display list
constexpr uint32_t kGxFifoWrapFlag = 0x8042A8B8u;     // r13 - 19912, two bytes
constexpr uint32_t kGxThreadQueue = 0x8042A8C8u;      // r13 - 19896
constexpr uint32_t kGxCurrentThreadPtr = 0x8042A8CCu; // r13 - 19892
constexpr uint32_t kPeThreadQueue = 0x8042A8D8u;      // r13 - 19880
constexpr uint32_t kGxDrawDoneFlag = 0x8042A8E0u;     // r13 - 19872
constexpr uint32_t kCpInterruptHandler = 0x801C2B80u;
constexpr uint32_t kPeTokenHandler = 0x801C5120u;
constexpr uint32_t kPeFinishHandler = 0x801C5200u;

#else // Mario Kart Wii

constexpr uint32_t kGXDataPtr = 0x803886C8u;
constexpr uint32_t kFifoObj = 0x80343740u;
constexpr uint32_t kGXData = 0x803437C0u;
constexpr uint32_t kGpFifoObj = 0x80343DC0u;
constexpr uint32_t kCpuFifoObj = 0x80343DE4u;
constexpr uint32_t kDlFifo = 0x80344090u;
constexpr uint32_t kDlSave = 0x80344110u;
constexpr uint32_t kSavedCpuFifo = 0x80344710u;
constexpr uint32_t kGxFifoWrapFlag = 0x803867B0u;
constexpr uint32_t kGxThreadQueue = 0x803867C0u;
constexpr uint32_t kGxCurrentThreadPtr = 0x803867C4u;
constexpr uint32_t kPeThreadQueue = 0x803867D0u;
constexpr uint32_t kGxDrawDoneFlag = 0x803867D8u;
constexpr uint32_t kCpInterruptHandler = 0x8016C668u;
constexpr uint32_t kPeTokenHandler = 0x8016ECCCu;
constexpr uint32_t kPeFinishHandler = 0x8016ED94u;

#endif

// Offsets inside the display list FIFO object (the same SDK structure in both games).
constexpr uint32_t kDlWritePtrOffset = 0x14u;
constexpr uint32_t kDlCountOffset = 0x1Cu;

} // namespace GxLayout
