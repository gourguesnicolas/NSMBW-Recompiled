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

Registered in `profiles/smne01/cache_hooks.cpp` (they were listed here but not registered until the world map
stayed empty: nothing performed the locked-cache DMA, so the 3D code read uninitialised matrices):
`DCEnable` `0x801AC420`, `DCInvalidateRange` `0x801AC440`, `DCFlushRange` `0x801AC470`, `DCStoreRange`
`0x801AC4A0`, `DCFlushRangeNoSync` `0x801AC4D0`, `DCStoreRangeNoSync` `0x801AC500`, `DCZeroRange`
`0x801AC530`, `ICInvalidateRange` `0x801AC560`, `ICFlashInvalidate` `0x801AC5A0`, `ICEnable` `0x801AC5B0`,
`__LCEnable` `0x801AC5D0`, `LCEnable` `0x801AC6A0`, `LCDisable` `0x801AC6E0`, `__OSCacheInit` `0x801AC840`;
each is at the distance from its neighbours that Mario Kart Wii has. The game also carries its own copies of
`LCLoadBlocks` and `LCStoreBlocks` at `0x8022B3C0` and `0x8022B3D0`.

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
| `DVDLowClearCoverInterrupt` | `0x80166964` | `0x801D3700` | high | `IOS_IoctlAsync` on the DI fd (`r13 - 24632`) with command `0x86`, one argument (the callback) |
| `DVDLowInquiry` | `0x80165A30` | `0x801D2470` | high | same wrapper with command `0x12` and a 0x20-byte output (the drive info), arguments (info, callback) |

`DVDFsInit` is `0x801CA650` (inlined into `DVDInit`, also a function of its own). The DVD request records
are 0x20 bytes at `0x80395BE0`, the waiting queues at `0x803958B0`. The other `DVDLow*` functions
(`0x801D14A0`..`0x801D3E30`, each one an IOS ioctl wrapper with its own command code) are not hooked.

`OSInit` starts the drive state machine, which sends `ClearCoverInterrupt` then `Inquiry`. Unhooked, both went
to the DI fd, which is -1 because the HLE never opens `/dev/di`; the reply was not "transfer complete", so
the driver read the status and control registers (`0x95`, `0x96`), asked for the error (`0x801D22D0`,
command `0xE3`) and `cbForStateError` (`0x801CB1B0`) set the fatal-error flag. `DVDGetDriveStatus`
(`0x801CEF60`) returns -1 while that flag is set; it reads the fatal flag at `r13 - 19796` (`0x8042A92C`),
the pausing flag at `r13 - 19800` (`0x8042A928`) and the executing command block at `r13 - 19664`
(`0x8042A9B0`). The nw4r stream player checks the drive status every sound frame and holds a stream in
"load wait" (voices paused) unless it is 0 or 1, which is why no music played while sound effects did.

## Video interface and the drive check, registered in `profiles/smne01/vi_hooks.cpp` and `dvd_hooks.cpp` (#16)

The 14 VI hooks use the addresses of the table in "Alarms and video" above. Found while running:

| Function | Mario Kart Wii | SMNE01 rev 2 | Confidence | Evidence |
| --- | --- | --- | --- | --- |
| `OSSetProgressiveMode` | n/a (inlined) | `0x801B00F0` | medium | reads bit 1 of `0xCC00206E`, caches it in a halfword at `r13 - 24920`; reimplemented in `vi_hooks.cpp` with the status read as zero |
| `DVDCheckDevice` (the drive check `main` runs first) | `0x801643FC` | `0x801D0CF0` | medium | sends a DVDLow request, spins on its callback, compares the reply with a dev-kit signature (`RVT`), ends in the fatal screen at `0x801D0800` |

VI statics (all in `os_layout.h`): `r13 - 20160` initialized flag, `-20148` timing guard, `-20048` TV format,
`-20056` next frame buffer, `-20036` / `-20032` post / pre retrace callbacks, `-20024` retrace queue,
`-19988` retrace count. They sit at the same distances from each other as in Mario Kart Wii (offset
`+0xA3C88` from its addresses), which is how they were found; each was then matched with a function that
reads or writes it. The HorVer mirror of the render and XFB sizes is not written for this game.

## Graphics library (`GX*`), registered in `profiles/smne01/gx_hooks.cpp` (#16)

119 hooks of `runtime/src/hle/gx` are registered for this game. The GX library is laid out the same way
in both games, so the Mario Kart Wii hook list was aligned with this game's function starts by order and
size (a sequence alignment, then checked by hand):

- anchors from the community's names: `GXSetProjection` `0x801C9840`, `GXLoadPosMtxImm` `0x801C9940`,
  `GXSetCurrentMtx` `0x801C9A60`, `GXLoadTexMtxImm` `0x801C9A80`, `GXSetViewport` `0x801C9C10`,
  `GXSetScissor` `0x801C9C60`, `GXGetScissor` `0x801C9CD0`;
- read from this game's own code: `GXInit` `0x801C1DB0` (calls `GXInitFifoBase` `0x801C2CE0`,
  `GXSetCPUFifo` `0x801C2E70`, `GXSetGPFifo` `0x801C3050`), `__GXFifoInit` `0x801C3540`,
  `GXGetCPUFifo` `0x801C3440`, `GXFlush` `0x801C4AD0`, `GXBeginDisplayList` `0x801C9470`,
  `__GXPEInit` `0x801C5280`, `__GXSetDirtyState` `0x801C52F0`; the interrupt handlers registered by the
  FIFO and PE set-up are `0x801C2B80` (CP), `0x801C5120` (PE token), `0x801C5200` (PE finish);
- the rest is positional (`GXInitTexObj` `0x801C6D90`, `GXSetVtxDesc` `0x801C37C0`,
  `GXSetVtxAttrFmtv` `0x801C420C`, `GXSetTevOp` `0x801C8250`, ... all within `0x10` of the size ratio).

The full list is the registration file itself. Jump-table case targets (`caseD`) are not registered: the
parent function is hooked as a whole. Not registered: `GXSetZScaleOffset` (absent here), `GXSetDrawSync`
(not found, probably inlined) and the two `GXEnd` copies in Mario Kart Wii's game code.

GX statics (`gx_layout.h`): `GXData` `0x8038FC00` (read from `GXInit`), its pointer at `r2 - 20216`, the FIFO
objects right after it (`0x80390200`, `0x80390224`), the display-list FIFO `0x803904D0` (read from
`GXBeginDisplayList`), and the small-data flags and thread queues at `r13 - 19912..-19872`, found by their
distance to each other and confirmed in `GXInit`, `__GXFifoInit` and `GXDrawDone`.

## Wii Remote and KPAD, registered in `profiles/smne01/input_hooks.cpp` (#16)

The Wii Remote library is split differently here: the public WPAD calls are thin wrappers
(`0x801DFAC0`..`0x801DFB30`) over a global control block at `0x803A3120`, and the per-channel calls
(`0x801E0F40`..`0x801E1630`) look the channel's block up in a pointer table at `0x8039E230`.
Identified by what they read and write and by the library's own warning strings
("Please call KPADSetConnectCallback instead of WPADSetConnectCallback"):

- `0x801DF7F0` WPADInit (control blocks, sensor bar settings, and the sensor bar power bit in the
  Hollywood GPIO register `0xCD0000C0`, which is why it has to be replaced); `0x801DFA50` is its public
  entry (registers the library version, calls `0x801F3190`, then this);
- `0x801F9E10` Bluetooth stack start-up: it sleeps until the predicate at `0x801FCF30` (state == 5) is true;
- `0x801DFAC0` start simple sync, `0x801DFAD0` stop simple sync, `0x801DFAF0` set sync-device callback
  (swaps the word at `+4`, returns the old one), `0x801DFB30` library status (signed byte at `+1800`);
- `0x801E0F40` WPADProbe (writes the device type byte at `+2241` of the channel block, status at `+2236`),
  `0x801E1150` GetDataFormat, `0x801E11A0` SetDataFormat, `0x801E12C0` GetInfoAsync (stores the callback at
  `+2440`), `0x801E14E0` ControlMotor, `0x801E1630` ControlLed (by position: it follows the motor-enable
  helpers `0x801E15B0`/`0x801E15F0`, as in Mario Kart Wii);
- KPAD: `0x801ED3A0` KPADRead and `0x801ED3B0` KPADReadEx are two entries into `0x801ED3C0`
  (`r6` = error word pointer, `r7` = "extended"); the hooks fill entry 0 from the host controller and the
  Ex form also writes the error word. KPAD's channel blocks are 1400 bytes each at `0x803A1AC8`.

Not hooked yet: `WPADGetDpdSensitivity`, `KPADGetUnifiedWpadStatus` (not called by the game's EGG code
that was read), the second simple-sync entry `0x801DFAE0`, and the GameCube pad calls (`PADInit` and
friends: not reached so far).

## Audio interface, DSP and AX task, registered in `profiles/smne01/audio_hooks.cpp` (#16)

Same implementations as Mario Kart Wii; the statics are per game (`audio.cpp`, `ax_internal.h`).

- AI (`0x8019F0B0`..`0x8019F1F0`): RegisterDMACallback `0x8019F0B0` (swaps `r13 - 20880`), InitDMA `0x8019F100`,
  StartDMA `0x8019F180`, BytesLeft `0x8019F1A0`, StartAddr `0x8019F1B0`, Length `0x8019F1D0`, CheckInit
  `0x8019F1E0` (`r13 - 20936`), Init `0x8019F1F0`. Same order and register offsets as the MKW functions.
- DSP (`0x801D56E0`..`0x801D5F70`): mail check/read/send `0x801D56E0/56F0/5700/5720`, DSPInit `0x801D5740`,
  CheckInit `0x801D5800` (`r13 - 19560`), AddTask `0x801D5810`, AssertTask `0x801D5880`, boot task `0x801D5F70`
  (same distance after AssertTask as in MKW). The driver statics keep their MKW distances from CheckInit's word.
- `__AXOutInitDSP` `0x801A1CE0`: fills the AX task at `0x8037CD00` (callbacks `0x801A1C50/1C60/1CC0/1CD0`,
  DRAM length 64, DSP address 3282: identical to MKW, so the same ucode).
- Not found: `AIGetDSPSampleRate`, the AXFX reverb callback.

## IOS client and SYSCONF status, registered in `profiles/smne01/ios_hooks.cpp` and `sc_hooks.cpp` (#16)

The IOS request functions come in (asynchronous, synchronous) pairs, in the order of the command codes
they post (`r0` = 1 open, 2 close, 3 read, 4 write, 5 seek, 6 ioctl, 7 ioctlv), the same order as Mario
Kart Wii's `0x801937E0`..`0x801945E0`: open `0x80224B50`/`0x80224C70`, close `0x80224DA0`/`0x80224E60`, read
`0x80224F10`/`0x80225010`, write `0x80225120`/`0x80225220`, seek `0x80225330`/`0x80225410`, ioctl
`0x80225500`/`0x80225640`, ioctlv `0x802258B0`/`0x802259A0`. `0x80226490` is `ISFS_OpenLib` (it references
`/dev/fs`); it is not replaced and works translated.

SC: `0x801DB8E0` is `SCCheckStatus`, the status poll OSInit and the input code call (state byte at
`r13 - 19504`: 1 = read running, 3 = read done and waiting to be parsed, 0 = parsed, 2 = error). It is also
the function that parses the file (`0x801DBEB0`, which sets the item count at `r13 - 19492` and the size at
`r13 - 19488`) into the table every getter reads through `0x801DC8B0`. It used to be replaced by a stub
returning 0, so the file was never parsed and every getter returned its built-in default (English, ...).
The hook now delivers the queued IOS callbacks of the open -> read -> close chain and runs the original
(`REGISTER_NATIVE_FUNCTION_AS`, so the translator keeps the body). `0x801DD1D0` is `SCGetAspectRatio`,
`0x801DD320` `SCGetLanguage` (item 11, falls back to English unless the font encoding is Shift-JIS). The
runtime creates `/shared2/sys/SYSCONF` on a new NAND (`runtime/include/nand_sysconf.h`); `[system]
language` in `Config.toml` sets `IPL.LNG`.

ES (`/dev/es` ioctlv, SDK wrappers next to `ESP_GetTitleId` `0x801D66E0`): GetTitleDir (29, `0x801D6640`, called
by the NAND library to find the home directory), DIGetTicketView (27, `0x801D64A0`, the disc ticket's view; the
callers read the title ID at offset `0x10`) and GetConsumption (22, `0x801D6780`) are answered in
`storage/nand_isfs.cpp`. Before, GetTitleDir failed and the game wrote its save to the NAND root. The title ID's
upper half comes from `<dvd_root>/ticket.bin` (`00010000` for this game, `00010004` for Mario Kart Wii).

ISFS: ReadDir on a file answers -101 (IOS and Dolphin do); the NAND library's type query relies on it. It
answered -106, so the game took its save for missing and created a new one at every boot, and CreateFile
(which truncated existing files) emptied it. CreateFile now answers "already exists" (-105) instead.
`MKW_TRACE_NAND=1` prints every IOS open, ISFS ioctl and ioctlv with its result.

`/title/00000001/00000002/data/play_rec.dat` is the system menu's play log, which the menu writes before booting
a disc. The SDK's play-record state machine (`0x801B6D60`, string at `0x8033FCC8`) opens it read-write, reads
128 bytes and keeps the u64 at `0x60`, then every minute writes the record back with the time at `0x60` and
the checksum at `0x00` (sum of the 31 words after it) updated; it stops if the time it read is more than 300
seconds behind the time base. The runtime writes the file on the first open of each run (`storage/nand_fs.cpp`):
title name from `opening.bnr` (IMET, console language, 40 UTF-16 characters at `0x04`), start and last time =
the guest time base (`0x58`, `0x60`), game ID from `0x80000000` (`0x68`).

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

