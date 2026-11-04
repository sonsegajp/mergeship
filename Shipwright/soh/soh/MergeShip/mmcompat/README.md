# Compiling Majora's Mask's player against Ocarina of Time's engine

MM's player is not transcribed here, it is compiled. The two decompilations describe much the
same engine in different vocabulary, so the work is a mapping rather than a rewrite, and the
behaviour that results is MM's own code rather than someone's reading of it.

Measured against MM's 21,738-line `z_player.c`:

| | |
|---|---|
| Distinct functions and macros it calls | 1058 |
| Defined inside `z_player.c` itself | 654 |
| External, already in SoH under the same name | 199 |
| External, needing a mapping | 205 |

The 205 are dominated by renames and small macros: `LinkAnimation_*` became `PlayerAnimation_*`,
`Player_SetupAction` became `Player_SetAction`, and `GIFIELD`, `ANIMSFX`, `ABS_ALT` and
`BEN_ANIM_EQUAL` account for 424 call sites between them.

## Layout

- `MergeShipMMCompat.h` — the vocabulary, force-included into MM-derived translation units only.
  Nothing in it is visible to Ocarina of Time's own code.
- `fallback/` — MM's own headers for things SoH has no equivalent of. **Last** on the include
  path, so anything both games define resolves to SoH's. The core game structs are deliberately
  not copied here; MM's player is meant to compile against OoT's `Player`, not MM's.
- `objects/`, `misc/`, ... — MM source names its assets without a prefix, but the imported
  headers prefix them because 2195 asset identifiers are spelled identically in the two games.
  These map one onto the other, generated, and only for MM translation units.
- `2s2h/` — 2 Ship 2 Harkinian's port layer mapped onto Ship of Harkinian's.
