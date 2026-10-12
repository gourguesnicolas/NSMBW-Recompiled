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

M0 is done. M1: the translator reaches 40,958 functions in strict mode and the executable compiles and
links (#13). `runtime.native_hooks: false` keeps the runtime's Mario Kart Wii hooks off. The hooks for
this game live in `runtime/src/hle/profiles/smne01` (OS, interrupts, EXI, SI, STM, IPC, audio start-up;
addresses in `docs/nsmbw/hle-hooks.md` and `docs/nsmbw/os-layout.md`). M2 (#16): the executable starts,
passes `OSInit` and stops in the DVD layer. Run it with a portable layout in `build/nsmbw` (`portable.txt`
and `UserData/Config.toml` with `[paths] dvd_root` set to the extracted disc). Low-memory boot values
are per profile in `runtime/src/hle/os/os_layout.h`. The rest of the hooks (#51) and the inventory in
`docs/nsmbw/sdk-inventory.md` (#14) come next, then the determinism audit (#15).
