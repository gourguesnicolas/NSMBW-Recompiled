# nsmbw-usa project

Translator manifest for the North American release (`SMNE01`).

`recomp.template.yml` is a template, not a working manifest: the values marked `TODO` must be read
from **your own** dump and filled in. Copy it to `recomp.yml` once they are known.

Nothing in this folder may contain game data. Addresses, sizes and hashes are fine; bytes of the
game are not. See [LEGAL.md](../../LEGAL.md).

## What needs to be determined (milestones M0 and M1)

1. Which USA revision your disc is, and the SHA-256 of `main.dol` and of each decompressed REL.
2. The entry point and the `r13` / `r2` Small Data Area bases (see the comments in
   `projects/examples/generic-dol.yml`).
3. The names and load addresses of the REL modules. The PAL release has four
   (`d_profileNP`, `d_basesNP`, `d_enemiesNP`, `d_en_bossNP`), shipped LZ-compressed; the USA
   names are expected to differ in their suffix.
4. A function map. Community symbols target the PAL v1 binaries and must be ported to USA
   addresses; ask the NSMBW-Community before embedding their symbol list.

The upstream manifest schema takes a single `inputs.rel`. Supporting several REL modules is
tracked as an M1 issue.
