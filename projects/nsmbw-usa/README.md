# nsmbw-usa project

Translator manifest for the North American release (`SMNE01`), **revision 2**.

`recomp.template.yml` is a template, not a working manifest yet: the REL load addresses are still
unknown and the manifest schema does not accept several REL modules (see below). Copy it to
`recomp.yml` once those are settled.

Nothing in this folder may contain game data. Addresses, sizes and hashes are fine; bytes of the
game are not. See [LEGAL.md](../../docs/LEGAL.md).

## Pinned revision

| Item | Value |
| --- | --- |
| Game ID | `SMNE01` |
| Disc revision | 2 (disc header version byte `0x02`) |
| Source of the reference dump | WBFS image (scrubbed). It cannot be matched against Redump, which hashes full images |

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

## Still to determine

1. **REL load addresses.** RELs are loaded into a heap at run time, so their addresses are not in
   the files. Candidates: read them from a Dolphin session at the main menu, or from the NSMBW
   modding community's address maps for this revision.
2. **Multi-REL support** in the manifest schema and the translator (tracked in its own issue).
3. **Guest memory size.** MEM1 is 24 MiB (`0x01800000`); confirm whether anything outside it must
   be mapped.
4. **Function map.** Community symbols target PAL v1 and must be ported to these addresses.
5. **Hash-tree check of the reference dump.** `nodtool extract --validate` reported an invalid H0
   hash in one effect file. The translator inputs above extracted without errors, but the image
   should be verified with Dolphin (Properties > Verify) before the hashes are treated as final.

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
