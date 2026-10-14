// IOS client hooks for New Super Mario Bros. Wii, North American release, revision 2 (SMNE01).
//
// The NAND, the network and the other IOS devices are reached through the IOS_* calls, which post a
// request through the IPC registers and wait for IOS to answer. This runtime has no IOS, so the calls are
// replaced and answered from the host (runtime/src/hle/ios.cpp and runtime/src/hle/storage/nand_isfs.cpp
// keep the Mario Kart Wii address in their names; only the registration is per game).
//
// The eight request functions come in (asynchronous, synchronous) pairs, in the order of the IOS command
// codes they send: open 1, close 2, read 3, write 4, seek 5, ioctl 6, ioctlv 7. See docs/nsmbw/hle-hooks.md.
#if defined(MKW_HOOK_PROFILE_SMNE01)

#undef MKW_NATIVE_HOOKS
#define MKW_NATIVE_HOOKS 1

#include "hle_stubs.h"

extern "C" {
void IOS_OpenAsync_HLE(CpuContext* ctx);
void IOS_CloseAsync_HLE(CpuContext* ctx);
void IOS_ReadAsync_HLE(CpuContext* ctx);
void IOS_WriteAsync_HLE(CpuContext* ctx);
void IOS_SeekAsync_HLE(CpuContext* ctx);
void IOS_IoctlAsync_80194158(CpuContext* ctx);
void IOS_IoctlvAsync_HLE(CpuContext* ctx);
int32_t NAND_IOS_Open_HLE(uint32_t pathPtr, uint32_t mode);
int32_t NAND_IOS_Close_HLE(uint32_t fd);
int32_t NAND_IOS_Read_HLE(uint32_t fd, uint32_t bufferPtr, uint32_t length);
int32_t NAND_IOS_Write_HLE(uint32_t fd, uint32_t bufferPtr, uint32_t length);
int32_t NAND_IOS_Seek_HLE(uint32_t fd, int32_t offset, int32_t whence);
void NAND_IOS_Ioctl_Entry_HLE(CpuContext* ctx);
void NAND_IOS_Ioctlv_Entry_HLE(CpuContext* ctx);
}

PPC_NATIVE_OVERRIDE_VOID(80224B50, IOS_OpenAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(80224C70, NAND_IOS_Open_HLE, int32_t, (uint32_t pathPtr, uint32_t mode), (pathPtr, mode));
PPC_NATIVE_OVERRIDE_VOID(80224DA0, IOS_CloseAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(80224E60, NAND_IOS_Close_HLE, int32_t, (uint32_t fd), (fd));
PPC_NATIVE_OVERRIDE_VOID(80224F10, IOS_ReadAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(80225010, NAND_IOS_Read_HLE, int32_t, (uint32_t fd, uint32_t bufferPtr, uint32_t length), (fd, bufferPtr, length));
PPC_NATIVE_OVERRIDE_VOID(80225120, IOS_WriteAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(80225220, NAND_IOS_Write_HLE, int32_t, (uint32_t fd, uint32_t bufferPtr, uint32_t length), (fd, bufferPtr, length));
PPC_NATIVE_OVERRIDE_VOID(80225330, IOS_SeekAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE(80225410, NAND_IOS_Seek_HLE, int32_t, (uint32_t fd, int32_t offset, int32_t whence), (fd, offset, whence));
PPC_NATIVE_OVERRIDE_VOID(80225500, IOS_IoctlAsync_80194158, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(80225640, NAND_IOS_Ioctl_Entry_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(802258B0, IOS_IoctlvAsync_HLE, (CpuContext* ctx), (ctx));
PPC_NATIVE_OVERRIDE_VOID(802259A0, NAND_IOS_Ioctlv_Entry_HLE, (CpuContext* ctx), (ctx));

#endif // MKW_HOOK_PROFILE_SMNE01
