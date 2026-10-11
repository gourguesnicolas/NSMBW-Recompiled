# Architecture

The translated game code only ever talks to the runtime, never to the host hardware. Three modern
layers (netcode, presentation, online services) sit beside the runtime and never touch game code
directly.

```
 user's disc image ──> static translator ──> native game code
 (main.dol + RELs)     (PowerPC -> C++)      (compiled locally with Clang)
                                                      │
              ┌───────────────────────────────────────▼──────────────────────────┐
              │ Runtime (C++, linked into the same executable)                   │
              │  deterministic core │ per-tick inputs │ GX (aurora) │ audio      │
              └───────┬──────────────────────┬──────────────────────────┬────────┘
                      ▼                      ▼                          ▼
                   netcode              presentation             online services
          snapshots, rollback,     interpolation between      lobby, friends,
          per-tick state hash      ticks, 4K/8K, 360 Hz+      matchmaking, relay
```

## What upstream already provides

WiiCompiled is further along than a bare translator. As of the fork point it has, among others:
a generic DOL/REL translator driven by a YAML manifest, an aurora-based GX layer, disc and NAND
storage, an audio backend, `WPAD`/`KPAD` HLE with Wii Remote accelerometer input through SDL, a
controller mapping wizard, experimental frame interpolation, and Windows, Linux and macOS builds.

Known gaps for this port, to be confirmed while working through M0 and M1:

- The manifest accepts a single `inputs.rel`. This game ships four REL modules.
- Tilt and shake emulation for pads without sensors, keyboard and mouse.
- No state snapshot, rollback or input-replay machinery.
- Interpolation is tuned for a 3D racing game and for 120 to 144 Hz.

## The seven determinism rules

Online play works by running the full game on every PC and sending only controller inputs. That
only works if the simulation is bit-for-bit deterministic, so these rules apply to every runtime
change from M2 onwards, long before any network code exists.

1. **Simulation and rendering are separate.** A game tick must be able to run without drawing or
   playing anything. Rollback re-runs several ticks per displayed frame.
2. **One door for inputs.** The game reads one `InputFrame` per tick and per player, never the
   hardware. Whether it came from a local pad, a replay file or the network makes no difference.
3. **No host clock in the simulation.** `OSGetTime`, `OSGetTick` and alarms derive from the tick
   counter. The console real-time clock is fixed or synchronised between peers.
4. **Bit-exact PowerPC floating point.** No fast-math. `fmadd`, the `frsqrte`/`fres` estimates,
   rounding and NaN handling must give the same result on every x86-64 and ARM64 CPU.
5. **Deterministic scheduling.** Guest threads run cooperatively in a fixed order. Asynchronous
   disc and NAND reads complete on a defined tick, not whenever the host disk finishes.
6. **Serialisable state.** All simulation state lives in guest memory or in a runtime structure
   that can be copied. The `snapshot()` / `restore()` interface is defined in M2, even if empty.
7. **State hashes and replays from M4.** A per-tick memory hash and an input recorder serve three
   purposes: regression tests, proof of accuracy against Dolphin, and desync detection online.

Audio and visual effects stay outside the simulation: they must tolerate a tick being re-run or
cancelled.

## Inputs: tilt and shake

The game reads only the Wii Remote accelerometer, so every input source is reduced to the same
acceleration vector stored in `InputFrame`.

| Gesture | Used for | Sensor pads | Pads without sensors | Keyboard and mouse |
| --- | --- | --- | --- | --- |
| Tilt | tilting platforms, remote-controlled lifts, spotlights | measured gravity, re-oriented for a sideways remote | right stick or analog triggers | two keys with a ramp, or horizontal mouse movement |
| Shake | spin jump, propeller flight, picking things up, leaving a bubble | detected peak, replayed as a fixed calibrated pulse | dedicated button | dedicated key |

The shake button stays available with sensor pads too. Because the network carries the vector and
not the device, a keyboard player and a gyro player stay in sync.

## Rendering

- **Resolution** is an internal-resolution factor in aurora: about 4.5x for 4K and 9x for 8K.
  Internal framebuffer copies must follow the factor.
- **Frame rate.** Physics is tied to the 60 Hz tick and must stay there. Higher refresh rates are
  reached by presenting interpolated frames between ticks (six per tick at 360 Hz). The long-term
  approach is transform interpolation: capture object, camera and background positions each tick
  and redraw in-between frames. A no-interpolation low-latency mode must always exist.

## Online

Input-synchronised netcode, in two steps: fixed input delay first (validates determinism on real
machines), then rollback. Peer-to-peer UDP mesh for up to four players with a relay fallback.
Peers exchange inputs and state hashes only, never game memory. The lobby, friends and matchmaking
service lives in `services/`.
