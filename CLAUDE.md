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

Last updated: 2026-10-15 (PR #69 open; virtual Wii Remote, GetTexObjLODAll fix, audio traces in review).

### Where we are
- **M0 done. M1 done**: the translator reaches 41,000+ functions in strict mode; the executable compiles and
  links (#13). `runtime.native_hooks: false` keeps the Mario Kart Wii (MKW) hooks off; this game's hooks live
  in `runtime/src/hle/profiles/smne01` (profile `smne01`, selected by `runtime.hook_profile`).
- **M2 nearly done (#16, first launch)**: the game boots, accepts input, and **reaches the "Select a File" screen
  with its 3D-lit background drawn** (screenshot, 60 fps). Before the GX fixes below only part of the picture
  showed. The remaining visible issues are not catalogued yet: look at the title, the file select, the world map
  and a level, and compare with the real game.
- **Input**: a real Wii Remote is used when paired (SDL HIDAPI). Otherwise
  `runtime/src/hle/input/gamepad_as_remote.cpp` makes a virtual remote held sideways: arrows or D-pad/stick
  move, Space or X (pad A) = "2" (jump), Left Shift or Z (pad B) = "1" (run), Enter = A, Backspace = B,
  P = +, M = -, H = Home. It is compiled in only for the `smne01` profile. This is what lets the save dialog
  be answered; `MKW_TRACE_INPUT=1` prints what the game is told.
- **Audio, measured** (`MKW_TRACE_AUDIO=1`): the AX frame callback runs, voices are "running" (ADPCM), the
  AX ucode CRC `0x4CC52064` is a known one, and the mixed output is silent (peak 0) until about 15 s after
  the title starts, then loud (peaks 3,000-32,768; some clipping). So the music is mixed. Whether it is
  correct, and whether it reaches the speakers, is not established: nobody has listened. Check the device
  and the sample format before blaming the mix.
- **GX hook mistakes found and fixed** (the 119 hooks were aligned by order and size, which goes wrong where
  the two SDK builds have different functions between two hooked ones): `GXGetTexObjLODAll` was on the wrong
  function (crash); `GXSetCopyClamp` was on `GXGetYScaleFactor` (`0x801C5BC0`, real `0x801C5B00`);
  `GXClearBoundingBox` was on `GXInitLightSpot` (real `0x801C63F0`); `GXInitTexObjUserData` was on
  `GXGetTexObjMipMap` (real `0x801C7160`); `GXLoadTlut` was on `GXInitTexCacheRegion` (real `0x801C7550`);
  `GXSetDither`/`GXSetDstAlpha` were one function late (real `0x801C8F90`/`0x801C8FC0`); a switch-artifact
  `GXSetArray` entry replaced a getter; `GXInitTexObjLODBias` does not exist in this SDK; `GXGetTexObjAll` is
  at `0x801C7180`; `GXSetVtxDescv` was on an inner label (real `0x801C3A30`), `GXSetVtxAttrFmtv` on the tail of
  `GXSetVtxAttrFmt` (real `0x801C4250`; unhooked, list-based vertex formats were never applied, which is
  what 3D models use), `GXSetNumTexGens` was on `0x801C4A50` (real `0x801C4A20`); `GXInvalidateTexAll` was on `0x801C7750` (an unrelated
  bit setter; real `0x801C76C0`). How they were found: compare the distance between two consecutive hooks with the distance
  between their Mario Kart Wii counterparts, then read the body (BP register id, `gd+offset`, argument use).
  Still unaudited by body: the vertex-attribute group (`0x801C37C0`-`0x801C4780`), FIFO group, TEV group,
  transform group. They fit the distance check, which is not proof.
- **Seen by the user after #71**: the world map shows only its UI (3D models missing), the first level shows
  its tile atlas as flat pictures at the bottom (tile texture coordinates wrong), sound effects play but not the
  music. The three vertex-format fixes above target the missing 3D; the tile and music problems are open.
- **Startup screen**: the runtime covers the game with an opaque black "WiiCompiled" screen until
  `settings_overlay::NotifyStrapInputAccepted()`; for SMNE01 `vi.cpp` calls it after 30 presented frames
  (stand-in for MKW's strap-scene hook).
- **Still unhandled**: `/dev/es` ioctl 29 and 27, `SYSCONF` and `play_rec.dat` are not created in the managed
  NAND (SC getters other than aspect ratio return defaults).
- **Open PRs**: #70 (virtual remote, LODAll fix, traces) and `gx-hook-audit` (the GX fixes above).
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
3. Trace switches: `MKW_TRACE_DVD=1` (every disc file read), `MKW_TRACE_VI=1` (a line per 60 frame-buffer
   submissions). Close any running `WiiCompiled.exe` before rebuilding: it locks the executable.
4. To see the picture from a script: find the window by title `WiiCompiled` (`FindWindow`; the process
   `MainWindowHandle` can be 0), make the process DPI-aware, `CopyFromScreen` its rectangle. `MKW_TRACE_VI=1` also
   prints the number of `GXBegin` calls so far.
5. Read `stderr` and `UserData/Logs/<run>/`: a stop prints the guest address (`lr=`, "guest address:").
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
1. **Check the picture** on the title, file select, world map and a level against the real game; audit the
   remaining GX hooks by body (list above).
2. **Music**: listen, then check the audio backend output path and the sample format if it is noise or silent.
3. **Fill the missing console data**: `play_rec.dat`, `SYSCONF`, `/dev/es` ioctls 29 and 27.
4. Faithful strap-scene hook; GameCube pad when reached.
5. Not located: `AIGetDSPSampleRate`, the AXFX reverb callback, `WPADGetDpdSensitivity`,
   `KPADGetUnifiedWpadStatus`, the NAND API level.
6. Determinism audit (#15); M2 exit: the title screen and a level are playable.

### Do not forget
- Remove nothing from the MKW profile; keep `allow_unsupported_instructions: false`.
- Never commit anything from `Assets/`, `generated/`, `build/`, or `UserData/`.
- Open a PR for every change; the user merges.
