# MergeShip2

Majora's Mask transformation masks (Deku, Goron, Zora, Fierce Deity) in Ship of Harkinian.
All features are optional and live under the **Mergeship** section of the SoH menu.

This branch (`mergeship2`) is Shipwright with the MergeShip2 work applied. The source for
the forms is in `soh/soh/MergeShip/`, `soh/src/overlays/actors/ovl_MM_*` and the Masks page
in `soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_mask.c`.

## Layout of this folder

- `docs/` - architecture, porting guide, status, agent brief.
- `tools/` - build/test helpers (`locked_build_test.sh`), generators for MM sfx and camera tables.
- `include/mergeship/` - shared header used by the soh build (`AssetArchive.h`).
- `patches/` - changes needed outside this repo:
  - `torch.patch` - apply inside `torch/` (submodule of this repo).
  - `2ship2harkinian.patch`, `2ship2harkinian-OTRExporter.patch`, `2ship2harkinian-ZAPDTR.patch`
    and `2ship2harkinian-new-files/` - the 2ship2harkinian extractor changes that write every
    MM resource under the `mm/` namespace. Build ZAPD from that tree to produce `mm.o2r`.

## Majora's Mask assets

The game needs `mm.o2r` next to the exe. If it is missing, SoH asks for a Majora's Mask ROM
(N64 US or GameCube US) at launch and builds `mm.o2r` with the bundled `mm-extractor/`.
No ROMs or `.o2r` archives are stored in this repository.
