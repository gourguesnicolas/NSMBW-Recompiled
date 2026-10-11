# Roadmap

Ten milestones in three phases. Each gate must be passed before the next phase opens. M6 can run
in parallel with M7. Durations are deliberately not committed to.

| Phase | Milestones | Gate to leave the phase |
| --- | --- | --- |
| 1. Make the game run | M0 to M3 | **Gate A:** World 1 can be completed with a controller |
| 2. Make it reliable and public | M4 to M6 | **Gate B:** a full-game input replay runs with no divergence from Dolphin |
| 3. Play online | M7 to M9 | 1.0 release |

| Milestone | Content | Exit criterion |
| --- | --- | --- |
| M0 Foundations | Fork, CI, disc extraction, `SMNE01` revision hash, translator build, legal rules | `info` runs on the user's `main.dol`; CI green with no game data |
| M1 Translation | USA manifest, SDA bases, LZ decompression, multi-REL loading and relocation, function map | Linked executable, zero unsupported instructions |
| M2 Boot | Game start-up, threads, heaps, disc reads, virtual NAND, `InputFrame` and `snapshot` interfaces | First frame on screen, stable main loop |
| M3 Playable, 1 player | Wii Remote emulation (buttons, tilt and shake from sensors or alternative controls, pointer), audio, saves | World 1 completable (gate A) |
| M4 Accuracy | Replay recorder, per-tick hash, Dolphin comparison, 4 local players, full playthrough | Full-game replay with no divergence (gate B) |
| M5 Public alpha | Launcher taking the user's own image, in-game settings, remapping, logs, crash reports | 10 external testers install successfully |
| M6 Modern rendering | 4K, 8K, texture packs, 120 Hz then 360 Hz and above, low-latency mode | Stable 360 fps at 4K on a reference machine |
| M7 Network foundations | Fast snapshots, rollback tested locally, synced LAN play for 2 then 4 | 30 minutes, 4 players on LAN, no desync |
| M8 Online | Lobby server, invite codes, friends list, matchmaking with strangers, NAT traversal, relay, reporting | Open beta: matches between strangers across regions |
| M9 1.0 | Steam Deck polish, documentation, start of mod support | Stable release |

Milestones M0 to M3 are broken into detailed issues. M4 to M9 are tracked as one epic each and are
broken down when the preceding gate is passed.
