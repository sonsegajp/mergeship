# MergeShip2 — Architecture

MergeShip2 is a redo of MergeShip with **two runtime asset consumers**: Ship of
Harkinian (OoT) and 2 Ship 2 Harkinian (MM). MM assets must be usable in OoT, and
OoT assets must be usable in MM. This direction was explicitly confirmed on
2026-09-22. The earlier single-OoT-engine assumption is superseded.

All four MM transformation masks and forms (Deku, Goron, Zora, Fierce Deity) must
be fully functional in OoT. A loaded skeleton or a CVar-selected model does not
satisfy that requirement. See `STATUS.md` for evidence and unfinished work.

Status legend: **[v]** verified against the source in this tree · **[p]** planned.

## Engines

| | Upstream | Version | Asset toolchain |
|---|---|---|---|
| `Shipwright/` | HarbourMasters/Shipwright | 9.2.3-471-gd54a69833 | torch |
| `2ship2harkinian/` | HarbourMasters/2ship2harkinian | 5.0.1-45-g61384c2c8 | ZAPDTR + OTRExporter |

Both sit on their own `mergeship2` branch so upstream stays mergeable. Both were renormalized
to LF (`core.autocrlf=false`); Git for Windows' system default of `autocrlf=true` breaks
libultraship's dependency patches, so CMake is invoked with `GIT_CONFIG_*` overrides rather
than changing the machine's global git config.

## The asset problem, and why mm.o2r is namespaced

**[v]** OoT and MM name **2562 resources identically** — `objects/object_link_child`,
`objects/object_link_boy`, 623 entries under `objects/gameplay_keep`, and the whole
`audio/audio` table among them. libultraship's resource namespace is flat and
last-mounted-wins, so a stock `mm.o2r` mounted beside `oot.o2r` silently replaces Hyrule's
assets with Termina's. Mounting it at lower priority inverts the problem: the collided MM
assets — including MM's `gameplay_keep` and its instrument tables — become unreachable.

So MM is exported under an `mm/` namespace: `mm/objects/object_link_goron/gLinkGoronSkel`.

**[v]** Both the archive entry name and every reference embedded *inside* exported data are
built by the same function, `OTRExporter_DisplayList::GetPrefix` (MM branch), reached via
`GetPathToRes`/`GetParentFolderName`. Prefixing there keeps entries and internal references
consistent by construction instead of by discipline. Two paths are spelled out literally
rather than built there and are prefixed individually; the one that matters is
`misc/link_animetion`, which carries the player animations the transformations need.

`mm.o2r` remains a single archive, built from the user's own ROM, loaded whole at
run time. MM's runtime now mounts it through `include/mergeship/AssetArchive.h`:
its generated root asset paths are aliases of the `mm/` entries, while embedded
references retain their explicit `mm/` paths. Stock MM archives still work as
native MM inputs.

The inverse uses `soh-torch --namespace oot` to produce `oot-assets.o2r`. Torch
prefixes resource directories before resolving pointers or computing display-list
hashes, and prefixes soundfont sample references too. Stock OoT extraction keeps
its original paths and archive names. Foreign archive metadata is hidden from the
host's version list; unnamespaced or mixed foreign archives are rejected before
mounting, so they cannot overwrite the host game's resources.

At run time no separate archive is needed: 2Ship mounts SoH's own `oot.o2r` with
`AssetArchiveMode::Rebase`, which lists every entry under `oot/` and rewrites each
resource's references as it loads (display-list hashes, and length-prefixed strings
that name an entry of the archive). Over a whole `oot.o2r` this reproduces the
`--namespace oot` export byte for byte (`tools/verify_asset_namespace.py --rebase`),
so one extraction from the player's ROM serves both games. `oot-assets.o2r` remains
a fallback for a standalone 2ship.exe package (`tools/build_cross_assets.ps1`).

## Loading MM assets from SoH

**[v]** SoH already has a by-path loader for every asset class this project needs, so no new
resource plumbing is required — only the namespace that makes the paths unambiguous:

| Asset class | Loader |
|---|---|
| Skeletons | `ResourceMgr_LoadSkeletonByName` |
| Player animations (`link_animetion`) | `ResourceMgr_LoadPlayerAnimByName` |
| Animations | `ResourceMgr_LoadAnimByName` |
| Display lists | `ResourceMgr_LoadGfxByName` |
| Textures | `ResourceMgr_LoadTexOrDListByName` |
| Instruments / soundfonts | `ResourceMgr_LoadAudioSoundFontByName` |
| Sequences | `ResourceMgr_LoadSeqByName` |
| Collision, vertices, cutscenes | `ResourceMgr_LoadColByName`, `LoadVtxByName`, `LoadCSByName` |

**[v]** Foreign mounting uses `MergeShip::AssetArchive`, validates its complete
namespace before `AddArchive(archive)`, and hides its root metadata. Simply calling
`AddArchive(path)` first and checking afterward can already contaminate native assets.

**Display lists must be loaded through the resource manager, never handed to `gSPDisplayList`
as a raw `__OTR__` path.** A raw path resolves and executes, then dies in
`gfx_vtx_handler_f3dex2` on the first `G_VTX` because its internal vertex references were
never retargeted. Textures are leaf resources and do not have this problem. (Inherited from
the previous attempt; re-verify before relying on it.)

## Transformations

**[v]** MM's player is structurally OoT's player generalized from *age* to *form*:

- OoT: `PlayerAgeProperties sAgeProperties[2]`, indexed by `LINK_AGE_ADULT/CHILD`.
- MM: `PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX]`, indexed by form
  (`FIERCE_DEITY`, `GORON`, `ZORA`, `DEKU`, `HUMAN`).

The struct is nearly the same — 0xD4 in OoT, 0xDC in MM, MM adding `shadowScale` and widening
two `Vec3s` arrays. Roughly forty further per-form tables follow the same shape
(`gPlayerSkeletons`, `sPlayerMass`, `sPlayerStrengths`, the hand/sheath/waist display-list
tables, eye and mouth textures, foot positions). The port therefore widens OoT's age-indexed
tables into form-indexed ones and fills the new rows from MM's values, rather than bolting a
parallel system alongside.

**[v]** `gPlayerSkeletons` names `gLinkFierceDeitySkel`, `gLinkGoronSkel`, `gLinkZoraSkel`,
`gLinkDekuSkel`, `gLinkHumanSkel` — direct symbol references that become
`mm/objects/object_link_*` lookups on the SoH side.

## The pause carousel

**[v]** OoT's pause menu is a four-faced box: `PAUSE_ITEM`, `PAUSE_MAP`, `PAUSE_QUEST`,
`PAUSE_EQUIP`. Each face is drawn by its own hardcoded block in `z_kaleido_scope_PAL.c` —
translate along an axis, then `Matrix_RotateY` by a hardcoded 0, ±1.57, or 3.14 radians.

**[v]** MM's pause menu is *also* four-faced: `PAUSE_ITEM`, `PAUSE_MAP`, `PAUSE_QUEST`,
`PAUSE_MASK` (`PAUSE_WORLD_MAP` is a sub-page of the map, not a face). Its mask page is
`z_kaleido_mask.c`, 1255 lines, including per-form slot restrictions
(`gMaskPlayerFormSlotRestrictions[PLAYER_FORM_MAX][MASK_NUM_SLOTS]`).

A merged OoT needs **five** faces — Item, Map, Quest, Equip, Mask — which no upstream has.
The four hardcoded draw blocks become one loop over pages with the angle and offset computed
from the face count, turning the square prism into a pentagonal one (72° steps). Generalizing
the geometry is the point: a fifth face bolted onto four hardcoded ones would have to be
re-done for the sixth.

Implemented in `soh/MergeShip/MergeShipPause.c` (face position/turn, eye points, page stepping; for
N = 4 it reproduces OoT's `D_8082ABAC/ABCC/ABEC` and `sKaleidoSetup*` tables exactly) and
`KaleidoScope_DrawPageFace` in `z_kaleido_scope_PAL.c`. Ring order: Item, Map, Quest, Equipment,
Masks. The Masks face is `pageIndex` 5 (`MERGESHIP_PAUSE_MASK`; 4 is `PAUSE_WORLD_MAP`, a cursor
index), its cursor lives in `z_kaleido_mask.c` because `PauseContext`'s per-page arrays are sized
for four pages in z64.h, and every per-page array access is guarded against it. The face distance
becomes the pentagon's apothem and the eye moves so each face fills the screen as before.

## Growing enums is the dangerous part

Adding a page to `PauseMenuPage` and forms to the player widens enums that size or bound
arrays elsewhere. In the previous attempt five arrays were sized by a grown enum and **four
failed silently** — NULL display lists faulting in `gSPDisplayList`, positional action tables
read past their end, and item-id-indexed HUD arrays drawing raw memory as a texture. Only the
one guarded by a `static_assert` failed at build time.

So: before growing `PauseMenuPage`, `PLAYER_MASK_MAX`, `PLAYER_IA_MAX` or any item-id enum,
grep for every array bounded by it, and add a `static_assert` tying the array's length to the
enum. The compiler is not a safety net here unless it is made into one.

## Form indexing — keep OoT's ages where they are

**[v]** The seam is two lines in SoH, each the exact mirror of MM:

| | OoT (`soh`) | MM (`mm`) |
|---|---|---|
| Skeleton | `gPlayerSkelHeaders[gSaveContext.linkAge]` | `gPlayerSkeletons[form]` |
| Properties | `&sAgeProperties[gSaveContext.linkAge]` | `&sPlayerAgeProperties[form]` |

MM numbers its forms `FIERCE_DEITY, GORON, ZORA, DEKU, HUMAN`. Adopting that numbering in OoT
would renumber `LINK_AGE_ADULT`/`LINK_AGE_CHILD` and break every `linkAge` and `LINK_IS_ADULT`
use in the OoT codebase — thousands of them, most of which would keep compiling.

So the merged index **extends** OoT's rather than replacing it: adult and child keep indices 0
and 1, and MM's forms take new ones.

```c
typedef enum PlayerForm {
    PLAYER_FORM_ADULT = 0, // == LINK_AGE_ADULT
    PLAYER_FORM_CHILD = 1, // == LINK_AGE_CHILD
    PLAYER_FORM_DEKU,
    PLAYER_FORM_GORON,
    PLAYER_FORM_ZORA,
    PLAYER_FORM_FIERCE_DEITY,
    PLAYER_FORM_MAX
} PlayerForm;
```

Existing OoT code keeps working unchanged because an age *is* a form. MM's form-indexed data is
remapped exactly once, where the tables are defined, rather than at every use. `static_assert`s
pin `PLAYER_FORM_ADULT == LINK_AGE_ADULT` and `PLAYER_FORM_CHILD == LINK_AGE_CHILD` so the
identity cannot rot, and each widened table is asserted to have `PLAYER_FORM_MAX` rows — the
lesson from the previous attempt, where four arrays sized by a grown enum failed silently.
