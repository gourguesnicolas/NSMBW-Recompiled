# Legal ground rules

This is not legal advice. These are the rules this project follows to stay clear of Nintendo's
code and assets. They apply to commits, pull requests, issues, discussions, releases and any
community space linked to the project.

## Never in this repository

- Game binaries: `main.dol`, `.rel`, `.rel.LZ`, or any part of them.
- Disc images or dumps in any format (ISO, WBFS, RVZ, WIA, GCZ, CISO, NKit, WAD).
- Game assets: archives, models, textures, sounds, music, text, levels, save files.
- **Output of the translator.** The generated C++ is a derivative of the game binary. It stays on
  the machine that produced it.
- Disassembly or decompiled code of the game, beyond the short snippets needed to describe a bug.
- Anything from a leaked Nintendo SDK or leaked source code. If you have read such material, do
  not contribute to the parts of the runtime it covers.
- Links to, or hints about, places to download any of the above.

A CI check (`.github/workflows/no-game-data.yml`) rejects the obvious cases. It is a safety net,
not a substitute for these rules.

## What is fine

- Addresses, function names, sizes and hashes of the game binary.
- Original code that reimplements console behaviour, written from public documentation,
  observation of the game, and Dolphin (GPL, compatible with this project's license).
- Logs and input replay files, as long as they contain no game data.

## Distribution

- Releases ship the toolchain and the runtime only. The translated executable is built on the
  user's machine from the user's own disc image and is never redistributed.
- Exactly one clean, unmodified image per supported manifest is accepted, identified by SHA-256.
- The project accepts no money: no donations, no paid builds, no paid online service.

## Online service

When the online service exists it will collect the minimum needed to run matchmaking, publish a
privacy policy, and delete account data on request. Nothing that travels over the network may
contain game data: peers exchange controller inputs and state hashes only.

## Reporting a problem

If you find game data or leaked material anywhere in this project, open an issue with the `legal`
label without quoting the material, or contact a maintainer privately.
