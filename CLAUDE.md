# CLAUDE.md

NSMBW-Recompiled is a static recompilation of New Super Mario Bros. Wii (USA, `SMNE01` revision 2),
forked from WiiCompiled (GPL-3.0). Read `docs/nsmbw/architecture.md` and `docs/nsmbw/roadmap.md`
before making design decisions.

## Hard rules

- **Never commit game data.** `Assets/`, `generated/`, disc images, `.dol`, `.rel`, `.LZ` and any
  bytes extracted from the game stay out of git. Only hashes, addresses and sizes are allowed.
  CI ("Repository contains no game data") rejects violations. See `docs/LEGAL.md`.
- **Always work on a branch and open a pull request.** `main` is protected and requires the checks
  "Repository contains no game data" and "Translator (build + test)".
- **Keep generic translator changes proposable upstream** (WiiCompiled). Keep them isolated from
  NSMBW-specific changes in separate PRs. Upstream requires PR descriptions written by the user,
  not generated: draft notes, but leave the description to them.
- `allow_unsupported_instructions` is `false` in `projects/nsmbw-usa/recomp.yml`. Keep it that way:
  an instruction the translator cannot handle must fail the build, not become a runtime trap.
- Runtime changes must respect the seven determinism rules in `docs/nsmbw/architecture.md`.
- **Keep this file current.** After every step (a merged or opened PR, a new stop point, a located
  function group), update "Status and next steps" below in the same PR, so a new session can resume
  without the chat history.

## Project facts (SMNE01 rev 2)

- Entry point `0x80004050`; `r13 = 0x8042F680`, `r2 = 0x80433080`.
- Manifest: `projects/nsmbw-usa/recomp.yml`; function seeds: `projects/nsmbw-usa/MAP.txt`;
  pinned hashes and layout: `projects/nsmbw-usa/README.md`.
- Four REL modules, relocated together, loaded back to back. BSS is at load address + `fixSize`
  (rounded to 32 bytes), so the manifest gives `bss_address` explicitly.

| Module | Load address | BSS |
| --- | --- | --- |
| d_profileNP (1) | 0x80768500 | 0x8076D4A0 |
| d_basesNP (2) | 0x8076D6C0 | 0x80990840 |
| d_enemiesNP (3) | 0x809A2CE0 | 0x80B11460 |
| d_en_bossNP (4) | 0x80B1C960 | 0x80B89AE0 |

## Commands

```powershell
dotnet build translator/Translator.sln -c Release
dotnet test translator/Translator.sln -c Release
$t = 'translator/src/Translator.Cli/bin/Release/net8.0/Translator.Cli.dll'
dotnet $t info --project projects/nsmbw-usa/recomp.yml
dotnet $t translate-recursive 0x80004050 --project projects/nsmbw-usa/recomp.yml
```

Inputs are extracted locally into `Assets/` (see `projects/nsmbw-usa/README.md`).

## Status and next steps

Last updated: 2026-10-14 (PR #66 open; audio hooks and function discovery in review).

### Where we are
- **M0 done. M1 done**: the translator reaches 41,000+ functions in strict mode; the executable compiles and
  links (#13). `runtime.native_hooks: false` keeps the Mario Kart Wii (MKW) hooks off; this game's hooks live
  in `runtime/src/hle/profiles/smne01` (profile `smne01`, selected by `runtime.hook_profile`).
- **M2 in progress (#16, first launch)**: the executable passes `OSInit`, DVD, VI, GXInit, the Wii Remote/KPAD
  start-up, the ROM font, the audio set-up (AI, DSP, AX task) and the game loads its own REL modules from the
  disc (`d_basesNP__prolog` ran at the manifest address). It now runs without a crash: after about 20 s the
  hang dumps show the main thread inside `0x800E4E40` -> `0x800E46E0` (the game's frame/scene loop) and
  the AX/DSP mix thread running. Nothing is drawn yet; whether the loop is waiting (VI retrace, NAND, an
  IOS call) or running is not established.
- **Function discovery**: `scripts/coverage.py` now also counts `lis`/`addi` relocations (ADDR16_HA/LO) as
  function pointers: a constructor passed to `__construct_array` was a function start no call reached
  (190 new starts in `MAP.txt`).
- **Open PRs**: `port-modules` (#66), `port-audio` (audio hooks + discovery).
- **Open issues to keep open**: #16 (first launch), #51 (hook port), #14 (SDK inventory), #7 (templates,
  until a real run confirms the log paths), #15 (determinism audit), #52 (stale "REL data: 0 bytes" message),
  #6 (licence of the symbol sources).

### How to run it locally
1. Build: `translate-recursive ... --output-metadata generated/base_translation_output.json`, then
   `generate-data-init`, `emit-build-shards`, then `cmake --build build/nsmbw --target WiiCompiled`
   (details in `projects/nsmbw-usa/README.md`). The toolchain is in `Launcher/artifacts/portable-tools`.
2. Run from `build/nsmbw` with a portable layout: an empty `portable.txt` next to the exe and
   `UserData/Config.toml` containing `[paths]` / `dvd_root = "<extracted disc dir>"` (the directory that holds
   `files/` and `sys/fst.bin`). Everything there is git-ignored.
3. Read `stderr` and `UserData/Logs/<run>/`: a stop prints the guest address (`lr=`, "guest address:").
   For a run that neither crashes nor ends, set `MKW_HANG_DUMP_SECONDS=20`: the CPU state is printed three
   times (last call site in `lr`).
   To list MMIO-touching functions still reachable from `__start`, scan `generated/functions` for the
   `0xCC00xxxx`/`0xCD00xxxx` base constants and subtract the hooked addresses.

### Method that works (repeat it)
Run, read the stop, find the function by order and size against its MKW address (the SDK is laid out the
same way, with small size differences), check its body in `generated/functions/func_XXXXXXXX.cpp`, register
it in the profile, rebuild, run. MKW hook addresses are keyed in `runtime/src/hle/**`; SMNE01 addresses go in
`runtime/src/hle/profiles/smne01/*.cpp` (the translator reads that directory). Per-game statics and boot
values go in `runtime/src/hle/os/os_layout.h`; never change the MKW values there.

### Next steps (in order)
1. **Find what the main loop waits for**: sample with `MKW_HANG_DUMP_SECONDS`, then look for the first VI
   retrace wait, the first IOS/NAND call (`IOS_Open`, `/shared2`), or a thread blocked on a message queue.
2. **IOS hooks** (MKW `runtime/src/hle/ios.cpp`, `nand_isfs.cpp`): `IOS_Open/Close/Read/Write/Seek/Ioctl/
   Ioctlv` and Async forms near `0x80224xxx`-`0x80226xxx`; NAND has MKW `r13` offsets to parametrize.
3. First frame: check the GX path draws (nothing validated on screen yet), VI retrace callbacks.
4. GameCube pad (`PADInit`/`PADRead`), `WPADGetDpdSensitivity`, `KPADGetUnifiedWpadStatus`, the KPADStatus
   layout the game reads; the AXFX reverb callback and `AIGetDSPSampleRate` were not located.
5. Determinism audit (#15): `mftb`/`OSGetTime` read the host clock.
6. M2 exit: first frame on screen, stable main loop.

### Do not forget
- Remove nothing from the MKW profile; keep `allow_unsupported_instructions: false`.
- Never commit anything from `Assets/`, `generated/`, `build/`, or `UserData/`.
- Open a PR for every change; the user merges.
