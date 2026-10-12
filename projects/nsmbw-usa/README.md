# nsmbw-usa project

Translator manifest for the North American release (`SMNE01`), **revision 2**.

`recomp.yml` is the translator manifest. Every value in it is pinned below. `MAP.txt` seeds the
function starts the call-graph walk cannot reach on its own (the modules' entry points).

Nothing in this folder may contain game data. Addresses, sizes and hashes are fine; bytes of the
game are not. See [LEGAL.md](../../docs/LEGAL.md).

## Pinned revision

| Item | Value |
| --- | --- |
| Game ID | `SMNE01` |
| Disc revision | 2 (disc header version byte `0x02`) |
| Source of the reference dump | WBFS image (scrubbed). It cannot be matched against Redump, which hashes full images |
| Dolphin integrity check | No errors in any block used by game files. Errors only in 123,915 unused (scrubbed) blocks, plus the missing update partition and the WBFS size field: all expected for a scrubbed WBFS |

## Translator inputs

All REL modules ship LZ-compressed (Nintendo LZ77 type `0x11`) under `files/rels/` and are
decompressed with `scripts/lz_decompress.py`. The module names keep the `NP` suffix used by the PAL
release.

| File | Module id | Size (decompressed) | SHA-256 |
| --- | --- | --- | --- |
| `main.dol` (`sys/main.dol`) | n/a | 0x3547E0 | `0373b4dbffd7c9da9d27eb7257a7677877a3e33ceb81818bab1daa93560b9907` |
| `d_profileNP.rel` | 1 | 0x5EB4 | `d6b738eb779d0b0b48f0b6d350e8955d51cbbdf0fb46ffdb621d9538c8fd67c7` |
| `d_basesNP.rel` | 2 | 0x2E1210 | `7f0fb0d0f91e4e82e3d01b9aee3546182fce7acfcd142e3f900dea9a5472f128` |
| `d_enemiesNP.rel` | 3 | 0x209D50 | `0f3af075bb64e86abfbcdb31def285f67557814656afde8aefa1dc3016e23f87` |
| `d_en_bossNP.rel` | 4 | 0x93D70 | `8c5d5649d45b461061cd6e633909b9bd8bb9b0380f45745de0cf81cf2c5ca277` |

## main.dol layout

| Fact | Value |
| --- | --- |
| Entry point (`__start`) | `0x80004050` |
| `__init_registers` (first `bl` from the entry point) | `0x800041C0` |
| `_SDA_BASE_` (`r13`) | `0x8042F680` (`.sdata` at `0x80427680` + 0x8000) |
| `_SDA2_BASE_` (`r2`) | `0x80433080` (`.sdata2` at `0x8042B080` + 0x8000) |
| Initial stack pointer (`r1`) | `0x8043FC40` |
| Text | `0x80004000`–`0x800066C0`, `0x80006780`–`0x802ED9E0` |
| BSS | `0x80351680`–`0x8042FC3C` |
| Highest address used by the DOL | `0x8042FC3C` |

## REL modules

All four modules are REL version 3, with `_prolog` at section 1 + 0x0, `_epilog` at + 0x30 and
`_unresolved` at + 0x60. Each has one executable section (s1), four data sections (s2 to s5) and a
BSS section (s6).

| Module | s1 code size | BSS size | Imports from modules |
| --- | --- | --- | --- |
| `d_profileNP` (1) | 0x1FB8 | 0x208 | 0, 1, 2, 3, 4 |
| `d_basesNP` (2) | 0x1C5FD4 | 0x12484 | 0, 2, 3, 4 |
| `d_enemiesNP` (3) | 0x12A428 | 0xB4E0 | 0, 2, 3 |
| `d_en_bossNP` (4) | 0x5702C | 0x487C | 0, 2, 4 |

Module 0 is `main.dol`. The modules reference each other, so they must be relocated together.

## Where the game loads the modules

Read from the OS module list (`0x800030C8` head, `0x800030CC` tail) in Dolphin at the title screen,
then cross-checked against the files. The game loads the four modules once, back to back, in id
order.

| Module | Load address (header) | Code s1 | BSS s6 | BSS end |
| --- | --- | --- | --- | --- |
| `d_profileNP` (1) | `0x80768500` | `0x807685E0` | `0x8076D4A0` | `0x8076D6A8` |
| `d_basesNP` (2) | `0x8076D6C0` | `0x8076D7B0` | `0x80990840` | `0x809A2CC4` |
| `d_enemiesNP` (3) | `0x809A2CE0` | `0x809A2DD0` | `0x80B11460` | `0x80B1C940` |
| `d_en_bossNP` (4) | `0x80B1C960` | `0x80B1CA50` | `0x80B89AE0` | `0x80B8E35C` |

- Every non-BSS section sits at load address + its file offset.
- **BSS is not placed after the whole file.** The game links the modules with `OSLinkFixed`, which
  frees the relocation tables, and puts BSS at load address + `fixSize` (header offset 0x48),
  rounded up to 32 bytes. This holds for all four modules. The translator's current single-REL
  layout places BSS after the whole file instead, so the manifest must give each BSS address
  explicitly.
- Everything ends at `0x80B8E35C`, well inside MEM1.

Still worth confirming once: that these addresses are the same after a reset and inside a level.

## Still to determine

1. ~~REL load addresses~~: done, see the table above.
2. ~~Multi-REL support~~: done in #40 and #43.
3. **Guest memory size.** MEM1 is 24 MiB (`0x01800000`); confirm whether anything outside it must
   be mapped.
4. **Function map.** Community symbols target PAL v1 and must be ported to these addresses.
5. **Matching a full dump.** Dolphin's integrity check found the file data intact, so the hashes
   above are treated as final. (`nodtool extract --validate` had reported an invalid H0 hash in one
   effect file; Dolphin found no error in any used block, so this looks like a tool quirk on scrubbed
   WBFS images.) A full, unscrubbed dump would also match Redump, but is not required: the manifest
   pins `main.dol` and the modules, not the disc image.

## Reproducing the inputs from your own dump

```sh
# nodtool: https://github.com/encounter/nod (the version WiiCompiled pins: v2.0.0-alpha.10)
nodtool extract -p data <your image> <out>
mkdir -p Assets
cp <out>/sys/main.dol Assets/main.dol
for m in d_profileNP d_basesNP d_enemiesNP d_en_bossNP; do
  python3 scripts/lz_decompress.py <out>/files/rels/$m.rel.LZ Assets/$m.rel
done
sha256sum Assets/main.dol Assets/*.rel   # compare with the table above
python3 scripts/print_layout.py Assets/main.dol Assets/*.rel
```

`Assets/` is ignored by git and rejected by CI. Keep it that way.

## Function coverage

`MAP.txt` is generated, not hand-written (names such as `__start` are kept). The call-graph walk from
`__start` alone reaches about 2,000 functions, almost all in `main.dol`: the modules are entered through
pointers. `scripts/coverage.py` finds the other function starts from three sources: the REL relocation
tables, the pointers in `main.dol` data sections, and the `stwu r1` + `mflr r0` prologue.

```sh
python3 scripts/coverage.py projects/nsmbw-usa/recomp.yml generated/functions --emit-map projects/nsmbw-usa/MAP.txt
```

| Result | Functions |
| --- | --- |
| Walk from `__start` only | 2,023 |
| With the generated map | 40,942 (`main.dol` 17,781; modules 1 to 4: 80, 12,987, 7,680, 2,414) |

Leaf functions with no prologue and never referenced by a call or a pointer are not found by this
method; the script lists how many candidates remain untranslated.

## Symbol names

`scripts/port_symbols.py` ports the community's PAL v1 symbols to this revision. It reads two public
files from the NSMBW-Community organisation (`NSMBW-Maps/address-map.txt` and `NSMBW-Decomp/syms.txt`)
and composes the address map P1 -> E1 -> E2. Of 1,045 symbols, 1,044 map; 694 land on a function
start already in `MAP.txt` and 350 are new starts.

**The committed `MAP.txt` holds addresses only.** The community's names are their work and neither
repository declares a licence (see issue #6), so they are not stored here. For readable crash reports
and for working on the code, write a named copy on your own machine:

```sh
python3 scripts/port_symbols.py --fetch <dir>
python3 scripts/port_symbols.py <dir> projects/nsmbw-usa/MAP.txt                       # addresses
python3 scripts/port_symbols.py <dir> projects/nsmbw-usa/MAP.txt --with-names named.txt  # local only
```

Those symbols are mostly game code (`d*`, `da*`, `m*`). Only about 20 are SDK functions
(`OSCreateThread`, `OSResumeThread`, `OSSuspendThread`, `VIWaitForRetrace`, ...), so most of the 55
runtime HLE hooks (issue #51) have to be located in this DOL by other means.
