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

## Not ported

- **Interrupt handler table pointer** (`kInterruptHandlerTablePtrAddr` is 0): only the interrupt-mask and
  exception hooks use it, and they are not registered for this profile.
- **Time functions.** `OSGetTime` and `__OSGetSystemTime` run as translated code and read the host clock
  through `mftb`, which breaks determinism rule 3 (#15). The sleep and alarm HLE use their own
  `ReadSystemTime`, so the two clocks can disagree until that is fixed.
- **Mario Kart Wii tweaks.** The THP decoder thread special case and the "scheduler start-up flag"
  step in `OSCreateThread` are switched off for this profile.
- **Not run.** None of this has executed: the executable has not been started (#16).
