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

