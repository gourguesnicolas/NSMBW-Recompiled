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

Last updated: 2026-10-18 (#78 merged, #19 closed; PR #81 open: SDK inventory, closes #14 and #51).

### Where we are
- **M0 done. M1 done**: the translator reaches 41,000+ functions in strict mode; the executable compiles and
  links (#13). `runtime.native_hooks: false` keeps the Mario Kart Wii (MKW) hooks off; this game's hooks live
  in `runtime/src/hle/profiles/smne01` (profile `smne01`, selected by `runtime.hook_profile`).
- **M2 nearly done (#16, first launch)**: the game boots, accepts input and **draws the 3D world map**
  (Peach's castle, Mario, the paths: screenshot, 60 fps), the file select and the level intro. What made the
  3D scenes empty: the cache hooks were never registered, so the locked-cache DMA (`DMA_U`/`DMA_L`, SPR
  922/923) was not performed and the 3D code read uninitialised matrices (`profiles/smne01/cache_hooks.cpp`).
  The user played a cave level (tiles, Goomba, coins, POW block, HUD all drawn); the title-screen demo of 1-1
  runs (screenshot).
- **Animated tiles (coins, ? blocks, bricks) were frozen; fixed.** The game redraws each animated tile's
  current frame every frame and copies it with `GXCopyTex` *into a 32x32 sub-rectangle of the level tileset*
  (a 1024x1024 RGB5A3 texture): `GXSetTexCopyDst(1024, 1024, ...)` sets the row stride and the destination
  address points inside the tileset. The runtime made a separate texture per copy that nothing sampled, so the
  tileset kept its first image. Now `gx_copy.cpp` finds the texture containing the destination
  (`FindTextureContainingCopy`, by stride width and format) and aurora's `GXCopyTexToRegion` keeps a GPU
  image of that texture (seeded from RAM on the first copy) and blits each copy into it at the right texel.
  Generic: any game doing partial copies benefits; proposable upstream (user writes the description).
  Follow-up: after a pipe the new area showed the previous area's tiles until a window resize, because the
  GPU image was seeded from RAM only once and the game loads the next tileset to the same address. The RAM
  texture is now resolved on every copy (the static texture cache returns a new upload only after guest
  writes) and a new upload refills the GPU image (about once per area load). Confirmed by the user.
- **Input**: a real Wii Remote is used when paired (SDL HIDAPI). Otherwise
  `runtime/src/hle/input/gamepad_as_remote.cpp` makes a virtual remote held sideways: arrows or D-pad/stick
  move, Space or X (pad A) = "2" (jump), Left Shift or Z (pad B) = "1" (run), Enter = A, Backspace = B,
  P = +, M = -, H = Home. It is compiled in only for the `smne01` profile. This is what lets the save dialog
  be answered; `MKW_TRACE_INPUT=1` prints what the game is told.
- **Music (BRSTM streams) was silent; fixed.** Sound effects played, music did not. `OSInit` starts the SDK
  drive state machine, which sent `DVDLowClearCoverInterrupt`/`DVDLowInquiry` to `/dev/di` on fd -1 (the
  DVD HLE never opens it); the bad reply ended in `cbForStateError` setting the driver's fatal-error flag,
  so `DVDGetDriveStatus` returned -1 for the whole run and the nw4r stream player kept every stream in
  "load wait" (voices paused after one frame, only 4 blocks ever read). Both functions are now hooked at
  SMNE01 addresses with the Mario Kart Wii stubs (`dvd_hooks.cpp`; details in `docs/nsmbw/hle-hooks.md`).
  Measured on the title demo: streams read block after block, the two stream voices stay running.
  How it was found: `MKW_TRACE_DVD=1` showed the 4-block stall; temporary prints in the generated shards
  (local only) followed the stream player's `Start` -> voice pause -> `UpdatePauseStatus` -> drive status.
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
  Audited by body since: `GXSetTevColor`/`S10`/`KColor`/`KColorSel`/`KAlphaSel`, the indirect group
  (`GXSetTevIndirect` incl. its two stack arguments, `GXSetIndTexMtx`, `GXSetIndTexCoordScale`,
  `GXSetIndTexOrder`, `GXSetNumIndStages`), `GXLoadTexMtxImm`, `GXSetTexCoordGen2`: all correct. Still
  unaudited: the vertex-attribute group (`0x801C37C0`-`0x801C4780`), FIFO group, the rest of the TEV and
  transform groups.
- **The EFB strip below line 456**: each frame the game renders 20 small 32x32 textures there (a constant
  32x32 RGBA8 source, 64x4 I8 ramps and indirect matrices that do not change from frame to frame), most
  likely lighting textures, plus each animated tile's frame at (0,456)/(0,488) before copying it into the
  tileset. An earlier report of this strip showing at the bottom of the picture was not seen again in the
  latest screenshots; the VI/EFB size mirrors (`kViRenderHeightAddr` etc.) are still not located for this
  game.
- **Startup screen**: the runtime covers the game with an opaque black "WiiCompiled" screen until
  `settings_overlay::NotifyStrapInputAccepted()`; for SMNE01 `vi.cpp` calls it after 30 presented frames
  (stand-in for MKW's strap-scene hook).
- **Console data and saves (#19)**: the save was lost at every boot. ISFS ReadDir on a file answered -106
  instead of -101, so the NAND library took `wiimj2d.sav` for missing, the game created it again and CreateFile
  truncated it; both fixed (`storage/nand_isfs.cpp`). `/dev/es` GetTitleDir (29), DIGetTicketView (27) and
  GetConsumption (22) are answered; GetTitleDir failing had put the save at the NAND root. The save is now in
  `/title/00010000/534d4e45/data` (title ID upper half read from `<dvd_root>/ticket.bin`). A new NAND gets a
  `SYSCONF` (`runtime/include/nand_sysconf.h`) and a `setting.txt` for the disc's region (USA here, from
  `sys/boot.bin`); the SDK now parses `SYSCONF` (the `SCCheckStatus` hook used to skip the parse).
  `[system] language = "french"` in `Config.toml` puts the game in French (seen: strap screen, save dialog).
  `MKW_TRACE_NAND=1` traces NAND traffic. `play_rec.dat` (the system menu's play log) is written on the first open of each run, as the menu does before booting a disc: banner name, game ID, guest time base; the SDK then updates its time every minute (checked).
  **Confirmed by the user**: progress is kept after finishing a level and relaunching. #19's three criteria
  (title directory, system settings, save creation flow) are met.
- **SDK inventory (#14, #51)**: `docs/nsmbw/sdk-inventory.md` ("Runtime hook coverage", generated by
  `scripts/sdk_inventory.py ... --hooks runtime/src/hle`) gives every one of the 583 Mario Kart Wii hooks a state:
  250 ported, 205 GX stops not needed, 56 covered by a lower layer, 50 run translated, 12 Mario Kart Wii only,
  10 to port, filed as #79 (exit/reset/fatal/report), #80 (two input functions) and #25 (sample rate, reverb).
  The game calls 315 SDK functions directly; 94 are hooked. A new hook missing from `NOT_PORTED` shows up as
  "Unclassified" when the report is regenerated.
- **Open PRs**: #81 (SDK inventory, closes #14 and #51).
- **Open issues to keep open**: #16 (first launch), #79 (exit paths), #80 (input), #7 (templates,
  until a real run confirms the log paths), #15 (determinism audit),
  #6 (licence of the symbol sources).

### How to run it locally
1. Build: `translate-recursive ... --output-metadata generated/base_translation_output.json`, then
   `generate-data-init`, `emit-build-shards`, then `cmake --build build/nsmbw --target WiiCompiled`
   (details in `projects/nsmbw-usa/README.md`). The toolchain is in `Launcher/artifacts/portable-tools`.
2. Run from `build/nsmbw` with a portable layout: an empty `portable.txt` next to the exe and
   `UserData/Config.toml` containing `[paths]` / `dvd_root = "<extracted disc dir>"` (the directory that holds
   `files/` and `sys/fst.bin`). Everything there is git-ignored.
3. Trace switches: `MKW_TRACE_DVD=1` (every disc file read), `MKW_TRACE_NAND=1` (every NAND open, ISFS
   ioctl and ioctlv with its result), `MKW_TRACE_VI=1` (a line per 60 frame-buffer submissions). Optional
   `[system] language = "french"` (or "english", "spanish", ...) in `Config.toml` sets the console language. Close any running `WiiCompiled.exe` before rebuilding: it locks the executable.
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
1. **Check the picture** in levels against the real game (animated tiles after the fix, enemies, effects);
   audit the remaining GX hooks by body (list above).
2. **Audio by ear**: the user confirms music in menus and levels; check tempo, pitch and the reverb
   (the AXFX callback is not located).
3. **#79**: exit, reset and fatal-error paths (HOME menu "Wii Menu"/"Reset" must close or restart cleanly).
4. Faithful strap-scene hook; GameCube pad when reached.
5. Not located: `AIGetDSPSampleRate`, the AXFX reverb callback, `WPADGetDpdSensitivity`,
   `KPADGetUnifiedWpadStatus`, the NAND API level.
6. Determinism audit (#15); M2 exit: the title screen and a level are playable.

### Do not forget
- **Mario Kart Wii addresses in shared runtime code**: anything that reads or writes a fixed `0x80xxxxxx`
  address outside `profiles/` and the `*_layout.h` files is a bug for this game (found so far: dynamic aspect,
  music attenuation, fiber manager, now guarded or per-game). `grep` for them when something is wrong.
- **A frozen or missing animation with correct inputs**: check the EFB copies first. A `GXCopyTex` whose
  `GXSetTexCopyDst` width is larger than the copied width writes into part of another texture; it must reach
  that texture (see the animated-tile fix). `MKW_TRACE_GX=1` prints each traced frame's copies.
- Remove nothing from the MKW profile; keep `allow_unsupported_instructions: false`.
- Never commit anything from `Assets/`, `generated/`, `build/`, or `UserData/`.
- Open a PR for every change; the user merges.
