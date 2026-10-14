# Runtime HLE hooks: where each function is in SMNE01 rev 2

The inherited runtime replaces 583 functions with host implementations (`PPC_NATIVE_OVERRIDE*` and
`REGISTER_NATIVE_FUNCTION*`), keyed by Mario Kart Wii addresses (issue #51). About 55 of them are named
`*_HLE_<address>`; the rest are GX (about 330, most of them in `gx_fatal_stubs.cpp`), audio, NAND,
input and OS hooks. This file records where the same functions are in this game's
`main.dol`. Addresses and public SDK function names only.

**Method.** The SDK object files keep their function order between games but not their exact sizes,
so a constant offset does not work. Each function was located by (1) community symbols mapped to this
revision (`scripts/port_symbols.py`) used as anchors, (2) the order and size pattern inside the same
SDK object file, and (3) the structure of the code: structure fields touched, special registers,
instruction sequence. Confidence: **high** means the code matches the SDK function's source;
**medium** means order and size agree but the body was not read in full; **low** is a guess to check.

## Threads (`OSThread.c`)

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `OSCreateThread` | `0x801A9E84` | `0x801B5130` | high | community symbol |
| `OSExitThread` | `0x801AA0F0` | `0x801B53A0` | high | clears the context (`+0x1A0/+0x1A2`), reads the current thread at `0x800000E4`, tests the detach bit (`+0x2CA`) |
| `OSCancelThread` | `0x801AA1D4` | `0x801B5490` | medium | follows Exit in order, size `0x1E0` vs `0x1D8`, touches state/suspend/priority |
| `OSJoinThread` | `0x801AA3AC` | `0x801B5670` | medium | follows Cancel, size `0x150` vs `0x140`, touches the join queue (`+0x2E8`) and active list (`+0x2FC/+0x300`) |
| `OSDetachThread` | `0x801AA4EC` | `0x801B57C0` | high | sets the detach bit, compares the state with 8 (moribund), unlinks from the active list |
| `OSResumeThread` | `0x801AA58C` | `0x801B5860` | high | community symbol |
| `OSSuspendThread` | `0x801AA6A8` | `0x801B5B00` | high | community symbol |
| `OSSleepThread` | `0x801AA9B8` | `0x801B5C90` | medium | reads the current thread, sets the waiting state, enqueues on a thread queue (`+0x2DC..+0x2E4`) |
| `OSWakeupThread` | `0x801AAAA4` | `0x801B5D80` | medium | walks a thread queue and makes each thread ready |
| `OSSetThreadPriority` (not hooked) | n/a | `0x801B5E80` | high | community symbol, bounds the other thread functions |

## Message queues (`OSMessage.c`) and mutexes (`OSMutex.c`)

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `OSInitMessageQueue` (`OS__InitMessageQueue`) | `0x801A72FC` | `0x801B2390` | high | size `0x5C` in both |
| `OSSendMessage` | `0x801A735C` | `0x801B23F0` | high | size `0xC8` in both |
| `OSReceiveMessage` | `0x801A7424` | `0x801B24C0` | high | size `0xDC` in both |
| `OSJamMessage` | `0x801A7500` | **absent** | medium | no function of that shape follows Receive; probably dropped by the linker as unreferenced |
| `OSLockMutex` | `0x801A7EE4` | `0x801B2E60` | high | order Init (`0x801B2E20`), Lock, Unlock |
| `OSUnlockMutex` | `0x801A7FC0` | `0x801B2F40` | high | size `0xD0` vs `0xDC`, same fields |

## Caches and locked cache (`OSCache.c`)

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `DCZeroRange` | `0x801A16E4` | `0x801AC530` | high | `dcbz` loop, the only one outside locked-cache code |
| `LCLoadBlocks` | `0x801A1894` | `0x801AC710` | high | writes `DMA_U`/`DMA_L` (SPR 922/923), memory address from `r4` (source) |
| `LCStoreBlocks` | `0x801A18B8` | `0x801AC740` | high | same, memory address from `r3` (destination) |
| `LCStoreData` | `0x801A18DC` | `0x801AC770` | high | loop of 128-byte blocks calling `LCStoreBlocks` |
| `LCQueueLength` | `0x801A197C` | `0x801AC810` | high | reads HID2 (SPR 920), extracts the queue length |
| `LCQueueWait` | `0x801A1988` | `0x801AC820` | high | reads HID2, compares and loops |
| `PPCMfhid2` | `0x8012E630` | `0x801A9C70` | high | `mfspr r3,920; blr` |

## Alarms (`OSAlarm.c`) and video (`vi.c`)

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `OSSetAlarm` | `0x801A0870` | `0x801AB540` | medium | zeroes the period, calls `__OSGetSystemTime`, inserts through the static helper at `0x801AB2F0` |
| `VIWaitForRetrace` | `0x801B99EC` | `0x801BCCF0` | high | community symbol |
| `VIConfigure` | `0x801B9F6C` | `0x801BD280` | medium | offset from `VIWaitForRetrace` agrees within alignment |
| `VIFlush` | `0x801BA9A4` | `0x801BDCB0` | medium | size `0x120` vs `0x114` |
| `VISetNextFrameBuffer` | `0x801BAAB8` | `0x801BDDD0` | medium | size `0x70` vs `0x6C` |
| `VIGetNextFrameBuffer` | `0x801BAB24` | `0x801BDE40` | medium | size `0x10` vs `0x08` plus alignment |
| `VISetBlack` | `0x801BAB2C` | `0x801BDE50` | medium | size `0x80` vs `0x78` |
| `VIGetRetraceCount` | `0x801BABA4` | `0x801BDED0` | medium | size `0x10` vs `0x08` plus alignment |
| `VIGetNextField` | `0x801BABAC` | `0x801BDEE0` | medium | size `0xA0` vs `0x9C` |
| `VIGetCurrentLine` | `0x801BAC48` | `0x801BDF80` | medium | size `0x90` |
| `VIInit` | `0x801B94A4` | `0x801BC7A0` | medium | the only large function where predicted; may have absorbed `__VIInit` |
| `VISetPreRetraceCallback` | `0x801B90F4` | `0x801BC3E0` | high | called by `OSFatal` to clear the callback; size `0x50` |
| `VISetPostRetraceCallback` | `0x801B9138` | `0x801BC430` | high | called by `OSFatal` to clear the callback; size `0x50` |
| `__VIInit` | `0x801B9294` | `0x801BC5A0` | high | called by `VIInit`, size `0x200` vs `0x210`, writes the VI registers |
| `VIGetDTVStatus` | `0x801BAD38` | `0x801BE070` | medium | exactly `0xF0` after `VIGetCurrentLine`, as in Mario Kart Wii |
| `OSFatal` | `0x801A4EC4` | `0x801AF5D0` | high | takes `(fg, bg, message)`, clears both retrace callbacks, calls `VIInit`, `VISetBlack`, `VIFlush`; draws through `0x801AF7C0` |
| `__OSInitSTM` | `0x801AB848` | `0x801B6820` | high | the only function that references both `/dev/stm/immediate` and `/dev/stm/eventhook` |
| `DVDReadAsyncPrio` (`DVD__ReadAsyncPrio`) | `0x8015E74C` | `0x801CAA30` | high | holds the string `DVDReadAsync(): specified area is out of the file` |
| `DVDReadPrio` | `0x8015E834` | `0x801CAB20` | high | `0xF0` after the async one in both games, blocks through `OSSleepThread` |
| `DVDReadAbsAsyncPrio` (`DVD__ReadAbsAsyncPrio`) | `0x801628CC` | `0x801CECC0` | high | the callee shared by both functions above |
| `ISFS_OpenLib` | `0x80169BCC` | `0x80226490` | medium | the only function using `/dev/fs`; the body was not read in full |

## Interrupt controller, EXI, SI, STM, IPC and audio start-up (found while running, #16)

These ran as translated code and stopped on a hardware register during `OSInit`. Each is registered in
`runtime/src/hle/profiles/smne01`.

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `__OSSetInterruptHandler` | `0x801A65F8` | `0x801B11B0` | high | indexes the table whose address is at `r13 - 20392`, returns the old entry |
| `__OSInterruptInit` | `0x801A661C` | `0x801B11E0` | high | writes `0xCC003004` and `0xCD000034`, the table at `0x80003040`, the masks at `0x800000C4/C8` |
| `SetInterruptMask` | `0x801A66E0` | `0x801B12B0` | high | size `0x260` vs `0x25C`; reads and writes the PI and MI mask registers |
| `__OSMaskInterrupts` | `0x801A693C` | `0x801B1510` | high | size `0x80`; clears bits in `0x800000C4`, loops on `SetInterruptMask` |
| `__OSUnmaskInterrupts` | `0x801A69BC` | `0x801B1590` | high | same shape, sets bits |
| `EXIInit` | `0x80168FA0` | `0x801B9BA0` | high | waits on the three EXI control registers, masks EXI interrupts, clears the status registers |
| `SetExiInterruptMask` | `0x80167E78` | `0x801B8A30` | medium | switch on channel 0, 1, 2 over the control block array at `0x8038F710` |
| `EXIImm` | `0x80167F68` | `0x801B8B20` | medium | `0xF0` after the previous one in both games |
| `EXIDma` | `0x80168288` | `0x801B8E50` | medium | follows `EXIImm` and its helper |
| `EXISync` | `0x80168380` | `0x801B8F50` | medium | polls bit 0 of the control register |
| `EXISelect` | `0x801689D0` | `0x801B95C0` | medium | three arguments, control block index `channel << 6` |
| `EXIDeselect` | `0x80168B00` | `0x801B96F0` | high | exactly `0x130` after `EXISelect` in both games |
| `EXIUnlock` | `0x80169260` | `0x801B9E70` | medium | same prologue as `EXIDeselect`, disables interrupts, tests the lock flag |
| `SIInit` | `0x801B2DE0` | `0x801BAEF0` | medium | writes `0xCD006434`, called right after `EXIInit` by `OSInit` |
| `SISetSamplingRate` | `0x801B3ACC` | `0x801BB840` | high | clamps the argument to 11, stores it at `r13 - 20176`, reads the VI DTV status |
| `__OSInitSTM` | `0x801AB848` | `0x801B6820` | high | state words at `r13 - 20324` (flag), `-20320`, `-20316` |
| `__OSInitMemoryProtection` | `0x801A7DFC` | `0x801B2D50` | medium | writes the MI protection registers `0xCC004010/4020` |
| `OSGetResetCode` | `0x801A8A50` | `0x801B3C90` | high | reads `0xCC003024 >> 3` |
| `IPCCltInit` | `0x80193478` | `0x802247D0` | high | installs handler 27, unmasks `0x10`, writes IPC register 1; calls `IPCInit` at `0x802242A0` |
| `__AIClockInit` | `0x801A1138` | `0x801ABF60` | medium | exactly `0x220` before `__OSInitAudioSystem` in both games |
| `__OSInitAudioSystem` | `0x801A1358` | `0x801AC180` | high | boots the DSP: `0xCC005000` registers, copies a 128 byte block to `0x81000000` |
| `__OSStopAudioSystem` | `0x801A1520` | `0x801AC350` | medium | `0x1C8` after `__OSInitAudioSystem` in both games |

Not hooked, found on the way: `OSReport` is `0x8015F730` (varargs wrapper), the `OSInit` banner and its
version checks sit at `0x801AA800`.

## DVD (`DVD.c`, `dvdlow.c`), registered in `profiles/smne01/dvd_hooks.cpp` (#16)

| SDK function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `DVDInit` | `0x8015EA1C` | `0x801CAD30` | high | community symbol; builds the four waiting queues, inlines `DVDFsInit`, ends with the disc ID copy |
| `DVDReadAsyncPrio` | `0x8015E74C` | `0x801CAA30` | high | see the table above |
| `DVDReadPrio` | `0x8015E834` | `0x801CAB20` | high | see the table above |
| `DVDReadAbsAsyncPrio` | `0x801628CC` | `0x801CECC0` | high | see the table above |
| `DVDLowInit` | `0x80164848` | `0x801D1230` | high | called by `DVDInit`; calls `IPCCltInit` and opens `/dev/di` through IOS |

`DVDFsInit` is `0x801CA650` (inlined into `DVDInit`, also a function of its own). The DVD request records
are 0x20 bytes at `0x80395BE0`, the waiting queues at `0x803958B0`. The other `DVDLow*` functions
(`0x801D14A0`..`0x801D3700`, each one an IOS ioctl wrapper with its own command code, `Inquiry` is the
one writing `0x12`) are not hooked: they are only reached from the drive state machine that `DVDInit` no
longer starts.

## Not found, or no counterpart

- `OS__SleepTicks`: no function calls both `OSSetAlarm` and `OSSleepThread`; probably inlined.
- `ConsoleWrite`, `GKI_delay`, `BTM_IsDeviceUp`, `NAND_IOS_OpenBody`, the two `EGG::` hooks and
  `TaskThread_run`: not located yet.
- `RFLiIsWorking`, `NHTTPStartup_Reentrant`: the game contains no `RFL` or `NHTTP` string, so these
  libraries are most likely not linked (Mario Kart Wii only).

## Hook addresses that land on unrelated SMNE01 functions

Of the 583 hook addresses, 13 are the start of a different function in this game. Registered as they
are, the runtime would replace those functions with Mario Kart Wii code, so the hook table has to
become per game before anything runs (#51):

`80048C30 801240B0 80166330 8016D3C0 8016D580 8016DA60 8016DAA0 80171E10 80171FD0 80172E00 80173920 80197380 801A00E0`

