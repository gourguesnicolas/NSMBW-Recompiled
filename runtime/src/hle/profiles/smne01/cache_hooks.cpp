// Cache hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// The implementations are in runtime/src/hle/os/os_cache.cpp and keep the Mario Kart Wii address in their
// names; only the registration is per game. The data-cache calls tell the graphics layer which guest ranges
// the CPU rewrote (textures, display lists), and the locked-cache calls perform the DMA that the guest
// starts by writing the DMA_U/DMA_L registers (SPR 922/923), which the runtime does not model. The
// addresses were read from the game's OSCache code (docs/nsmbw/hle-hooks.md): the functions are in the
// order of the SDK source and each address is the distance from its neighbours that Mario Kart Wii has.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

extern "C" {
void DCInvalidateRange_801a1600(uint32_t addr, uint32_t length);
void DCFlushRange_801a162c(uint32_t addr, uint32_t length);
void DCStoreRange_801a165c(uint32_t addr, uint32_t length);
void DCFlushRangeNoSync_801a168c(uint32_t addr, uint32_t length);
void DCStoreRangeNoSync_801a16b8(uint32_t addr, uint32_t length);
void Cache_Maintenance_Stub();
void DCZeroRange_HLE_801a16e4(CpuContext* ctx);
void LCLoadBlocks_HLE_801a1894(CpuContext* ctx);
void LCStoreBlocks_HLE_801a18b8(CpuContext* ctx);
uint32_t LCStoreData_HLE_801a18dc(CpuContext* ctx);
uint32_t LCQueueLength_HLE_801a197c(CpuContext* ctx);
void LCQueueWait_HLE_801a1988(CpuContext* ctx);
}

PPC_NATIVE_OVERRIDE_VOID(801AC420, Cache_Maintenance_Stub, (), ()); // DCEnable
PPC_NATIVE_OVERRIDE_VOID(801AC440, DCInvalidateRange_801a1600, (uint32_t addr, uint32_t length), (addr, length));
PPC_NATIVE_OVERRIDE_VOID(801AC470, DCFlushRange_801a162c, (uint32_t addr, uint32_t length), (addr, length));
PPC_NATIVE_OVERRIDE_VOID(801AC4A0, DCStoreRange_801a165c, (uint32_t addr, uint32_t length), (addr, length));
PPC_NATIVE_OVERRIDE_VOID(801AC4D0, DCFlushRangeNoSync_801a168c, (uint32_t addr, uint32_t length), (addr, length));
PPC_NATIVE_OVERRIDE_VOID(801AC500, DCStoreRangeNoSync_801a16b8, (uint32_t addr, uint32_t length), (addr, length));
PPC_NATIVE_OVERRIDE_VOID(801AC530, DCZeroRange_HLE_801a16e4, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AC560, Cache_Maintenance_Stub, (), ()); // ICInvalidateRange
PPC_NATIVE_OVERRIDE_VOID(801AC5A0, Cache_Maintenance_Stub, (), ()); // ICFlashInvalidate
PPC_NATIVE_OVERRIDE_VOID(801AC5B0, Cache_Maintenance_Stub, (), ()); // ICEnable
PPC_NATIVE_OVERRIDE_VOID(801AC5D0, Cache_Maintenance_Stub, (), ()); // __LCEnable
PPC_NATIVE_OVERRIDE_VOID(801AC6A0, Cache_Maintenance_Stub, (), ()); // LCEnable
PPC_NATIVE_OVERRIDE_VOID(801AC6E0, Cache_Maintenance_Stub, (), ()); // LCDisable
PPC_NATIVE_OVERRIDE_VOID(801AC710, LCLoadBlocks_HLE_801a1894, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AC740, LCStoreBlocks_HLE_801a18b8, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(801AC770, LCStoreData_HLE_801a18dc, uint32_t, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(801AC810, LCQueueLength_HLE_801a197c, uint32_t, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AC820, LCQueueWait_HLE_801a1988, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(801AC840, Cache_Maintenance_Stub, (), ()); // __OSCacheInit

// Two further copies of LCLoadBlocks and LCStoreBlocks sit in the game's own code (0x8022B3C0, 0x8022B3D0).
PPC_NATIVE_OVERRIDE_VOID(8022B3C0, LCLoadBlocks_HLE_801a1894, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(8022B3D0, LCStoreBlocks_HLE_801a18b8, (CpuContext* ctx), (ctx));

#endif // MKW_HOOK_PROFILE_SMNE01
