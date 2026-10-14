# OS layout of SMNE01 rev 2

The OS HLE in `runtime/src/hle/os` needs the addresses of the OS's own variables and of a few internal
functions. They differ between games because the linker places them differently. For SMNE01 they are in
`runtime/src/hle/os/os_layout.h`, selected by the manifest's `hook_profile: smne01`. Addresses only.

Each one was read from this game's code, not guessed from a constant offset.

## Data

| Variable (SDK name) | Address | How it was found |
| --- | --- | --- |
| `RunQueue[32]` | `0x8038EC50` | referenced by thread creation, `SelectThread`, `OSResumeThread` and `OSWakeupThread`; 32 queues of 8 bytes |
| `IdleThread` | `0x8038ED50` | exactly `RunQueue + 0x100`, referenced only by `SelectThread`; same spacing as in Mario Kart Wii |
| `DefaultThread` | `0x8038E938` | referenced only by `OSInitThreads` (`0x801B4890`), `RunQueue - 0x318` (one `OSThread`) |
| `SwitchThreadCallback` | `0x80429530` | small-data variable read by `SelectThread` |
| `Reschedule` (scheduler disable count) | `0x8042A700` | the only variable read by `0x801B4B70`, a 0x40-byte function; counts up and down |
| `RunQueueHint` | `0x8042A704` | written by `OSSleepThread`, `OSExitThread` and `OSResumeThread` when they ask for a reschedule |
| `RunQueueBits` | `0x8042A708` | read and written when a thread enters or leaves a run queue, next to the hint |
| `AlarmQueue` | `r13 - 0x5000` (`0x8042A680`) | the only data `InsertAlarm`, `OSSetAlarm` and the alarm handler touch |
| `__OSActiveThreadQueue` | `0x800000DC` / `0x800000E0` | low memory, same as every Wii game |
| `OSCurrentThread` | `0x800000E4` | low memory |

The three scheduler counters are consecutive words in the same order as in Mario Kart Wii
(`Reschedule`, `RunQueueHint`, `RunQueueBits`).

## Functions

| SDK function | Address | Role |
| --- | --- | --- |
| `OSSetCurrentContext` | `0x801ACD40` | hooked |
| `OSGetCurrentContext` | `0x801ACDA0` | not hooked, plain load |
| `OSSaveContext` | `0x801ACDB0` | called by the HLE scheduler |
| `OSLoadContext` | `0x801ACE30` | hooked; ends with `rfi` |
| `OSClearContext` | `0x801ACF80` | hooked |
| `OSInitContext` | `0x801ACFB0` | called by the HLE when creating a thread |
| `OSDisableInterrupts` / `OSEnableInterrupts` / `OSRestoreInterrupts` | `0x801B1140` / `0x801B1160` / `0x801B1180` | hooked |
| `__OSUnlockAllMutex` | `0x801B3010` | called by the HLE when a thread exits |
| `SelectThread` (static) | `0x801B4EA0` | hooked |
| `InsertAlarm` (static) | `0x801AB2F0` | called by the HLE alarm code; `OSSetAlarm` is `0x250` after it, as in Mario Kart Wii |

The thread, mutex, message queue and alarm entry points are in `hle-hooks.md`.

## Boot values (low memory) and more statics

| Value | Address | Meaning |
| --- | --- | --- |
| Disc ID | `0x80000000` | `SMNE`, then maker `01`, disc 0, revision 2 (`0x30310002`) |
| MEM1 arena low | `0x80000030`, `0x8000310C` | `0`, as the boot code leaves it: `OSInit` then starts the arena after the stack (see below) |
| IOS version | `0x80003140` | IOS 53 revision 5663 (`0x0035161F`); the TMD asks for system version `0x100000035` |
| Required IOS | `0x80003188` | `0x00350000`: `OSInit` compares the major number and the revision with it, and calls `OSFatal` otherwise |
| Interrupt handler table pointer | `r13 - 20392` = `0x8042A6D8` | points at `0x80003040` |
| STM state | `r13 - 20324` = `0x8042A71C` | flag, then the two `/dev/stm` handles |
| IPC buffer low / high | `r13 - 19164` / `-19160` = `0x8042ABA4` / `0x8042ABA8` | window `IPCCltInit` advances by `0x1000` |
| EXI control blocks | `0x8038F710` | `0x40` bytes per channel |
| Arena clear | `0x801AA140`, `0x801AA320` | `OSInit` zeroes `[arena low, arena high)` on a cold boot: with Mario Kart Wii's arena start it wiped the game's own `.sdata`, then `OSReport` jumped through a null pointer |

The four REL modules (`0x80768500..0x80B8E000`) are no longer embedded: the manifest sets
`runtime.preload_modules: false` and the game loads `d_*NP.rel.LZ` from the disc into its own heap. An
earlier step placed them before the game started and seeded the arena at `0x80C00000` to keep the arena
clear off them, which left the game about 12 MiB of the 24 MiB and made its heap creation fail
(`EGG::TextureBuffer::initialize` then called through NULL).

## Not ported

- **`OSExceptionInit`.** The Mario Kart Wii hook that skips the exception vector set-up was not located;
  the translated function runs.
- **Time functions.** `OSGetTime` and `__OSGetSystemTime` run as translated code and read the host clock
  through `mftb`, which breaks determinism rule 3 (#15). The sleep and alarm HLE use their own
  `ReadSystemTime`, so the two clocks can disagree until that is fixed.
- **Mario Kart Wii tweaks.** The THP decoder thread special case and the "scheduler start-up flag"
  step in `OSCreateThread` are switched off for this profile.
- **Run only up to the DVD layer.** `OSInit` now completes; the next stop is the DVD request ring (see #16).
