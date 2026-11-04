# MergeShip2 porting guide: Majora's Mask forms inside Ocarina of Time's player

Read `AGENTS.md` first. Never use code from the original Mergeship project. The only references are
MM's native player in `2ship2harkinian/mm` and OoT in `Shipwright/soh`. The user requires every
form to behave **1:1 with Majora's Mask**: same actions, animations, timings, values, sounds, camera
and HUD behavior. Port MM's code faithfully; do not approximate or invent behavior.

## Architecture (already in place)

- OoT's player is the base: `Shipwright/soh/src/overlays/actors/ovl_player_actor/z_player.c`.
  World interaction (items, NPCs, cutscenes, doors, chests) stays OoT's, because the forms live in
  Hyrule. Everything the form itself does (movement, abilities, animations, sounds, camera, HUD
  labels) is MM's, ported function by function.
- Ported MM player code lives in include files that `z_player.c` includes at its very end, so they
  can use every static helper in `z_player.c`:
  - `soh/soh/MergeShip/MergeShipPlayerTransform.inc` – the mask transformation sequence (done).
  - `soh/soh/MergeShip/MergeShipPlayerForms.inc` – shared form helpers, MM A/B labels (done).
  - `soh/soh/MergeShip/forms/MergeShipDeku.inc`, `...Goron.inc`, `...Zora.inc`,
    `...FierceDeity.inc` – one per form (each form's porter owns its file).
  Functions called from earlier in `z_player.c` need a prototype near the top of `z_player.c`
  (there is a block of MergeShip prototypes after the includes).
- `soh/src/code/z_player_lib.c` holds drawing and the per-form tables (skeletons, hand/sheath/waist
  display lists, eye/mouth textures, foot geometry, MM movement table `sMMBootData`).

## Forms and numbering

`soh/soh/MergeShip/MergeShipForms.h`:
- `PlayerForm`: `PLAYER_FORM_ADULT 0, CHILD 1, DEKU 2, GORON 3, ZORA 4, FIERCE_DEITY 5`.
  `player->mergeShipForm` holds this. **Not MM's numbering.**
- `MMPlayerForm`: `MM_FORM_FIERCE_DEITY 0, GORON 1, ZORA 2, DEKU 3, HUMAN 4` – MM's own order. Use it
  for tables copied from MM that are indexed by form: `MMFORM(this)` (defined in
  MergeShipPlayerForms.inc) = `MergeShip_FormToMM(this->mergeShipForm)`.
- `MERGESHIP_FORM_IS_MM(form)` – true for Deku/Goron/Zora/FD.
- MM's `this->transformation == PLAYER_FORM_X` becomes `this->mergeShipForm == PLAYER_FORM_X`
  (OoT-side enum name, same meaning) or `MMFORM(this) == MM_FORM_X`. Both humans (adult, child) are
  MM's HUMAN.
- `sAgeProperties[PLAYER_FORM_MAX]` in z_player.c has MM's `PlayerAgeProperties` layout and MM's
  values for the four forms (field names as MM: `ceilingCheckHeight`, `shadowScale`, `unk_08`...,
  `voiceSfxIdOffset`, `surfaceSfxIdOffset`, `unk_98`, `unk_9C`, `openChestAnim`, ...).
- MM "boots" per form: `gMergeShipPlayerExt.mmBoots` with `MM_BOOTS_*` (MergeShipPlayerExt.h).
  MM's `this->currentBoots` → `gMergeShipPlayerExt.mmBoots` (MM enum), and MM's
  `PLAYER_BOOTS_ZORA_UNDERWATER` etc. → `MM_BOOTS_ZORA_UNDERWATER`. After changing it call
  `Player_SetBootData(play, this)` (dispatches to MM's func_80123140 for forms). OoT's
  `this->currentBoots` stays `PLAYER_BOOTS_KOKIRI` for forms.

## Player struct

OoT's `Player` (soh/include/z64player.h) has every OoT field plus MM-only fields appended at the end
(`unk_B10[6]`, `unk_B86[2]`, `unk_B8A..unk_B96`, `remainingHopsCounter`, `zoraBoomerangActor`,
`unk_B48`, `unk_ABC`, `unk_AC0`, `prevMask`, `lightInfo`, ...). Do **not** edit z64player.h (it forces a
full hour-long rebuild); put new state in `MergeShipPlayerExt` (MergeShipPlayerExt.h, only included by
player code) – that header is cheap to change.

Field names MM and OoT spell differently (same meaning, use OoT's):
| MM | OoT |
|---|---|
| `speedXZ` | `linearVelocity` |
| `unk_AA5` | `unk_6AD` |
| `actor.speed` | `actor.speedXZ` |
| `actor.depthInWater` | `actor.yDistToWater` (MM's is positive below the surface; check OoT's sign use) |
| `lockOnActor`/`focusActor` | `focusActor` |
| `av1.actionVar1`, `av2.actionVar2` | same |
| `unk_B28`/`unk_B0C`... | OoT has `unk_85C`, `unk_860` for the stick/fishing equivalents; MM-only ones exist appended |
Check a field's declaration in both z64player.h files before using it.

`stateFlags3` is widened to u32. Bits 0-7 keep OoT's names; MM's bits 8-31 are defined with MM's names
(`PLAYER_STATE3_100` Deku flower dive, `PLAYER_STATE3_1000` Goron curled, `PLAYER_STATE3_8000` Zora
swimming, `PLAYER_STATE3_200000` Deku hopping, ...). stateFlags1/2 bits line up positionally with MM;
MM's names for bits OoT has named: MM `PLAYER_STATE1_8000000` = OoT `PLAYER_STATE1_IN_WATER`,
`PLAYER_STATE1_400000` = `PLAYER_STATE1_SHIELDING`, `PLAYER_STATE1_800000` = `ON_HORSE`,
`PLAYER_STATE1_20000000` = `IN_CUTSCENE`, `PLAYER_STATE1_10000000` = `IN_ITEM_CS`,
`PLAYER_STATE1_4000000` = `DAMAGED`, `PLAYER_STATE1_200000` = `CLIMBING_LADDER`, `PLAYER_STATE1_2000`
= `HANGING_OFF_LEDGE`, `PLAYER_STATE1_4000` = `CLIMBING_LEDGE`, `PLAYER_STATE1_40000` = `JUMPING`,
`PLAYER_STATE1_80000` = `FREEFALL`, `PLAYER_STATE1_100000` = `FIRST_PERSON`, `PLAYER_STATE1_10` =
`HOSTILE_LOCK_ON` (MM also uses it for the Zora barrier – check each use), `PLAYER_STATE1_2` =
the `mergeShipTransforming` field (OoT gives bit 1 to the swung bottle). stateFlags2: MM
`PLAYER_STATE2_40` = `DISABLE_ROTATION_ALWAYS`, `PLAYER_STATE2_80000` = `HOPPING`, `PLAYER_STATE2_20000`
= `SPIN_ATTACKING`, `PLAYER_STATE2_400` = `UNDERWATER`, `PLAYER_STATE2_800` = `DIVING`,
`PLAYER_STATE2_100000` = `NAVI_ACTIVE`, `PLAYER_STATE2_1` = `DO_ACTION_GRAB`, `PLAYER_STATE2_4` =
`DO_ACTION_CLIMB`. When MM uses a bit for something OoT's actors would misread, say so in a comment.

## Name mapping (MM → OoT)

| MM | OoT |
|---|---|
| `Player_SetAction` | `Player_SetupAction` |
| `Player_SetAction_PreserveItemAction` | `Player_SetupActionPreserveItemAction` |
| `PlayerAnimation_Update/Change/OnFrame` | `LinkAnimation_Update/Change/OnFrame` |
| `Player_Anim_PlayOnce`, `..Adjusted`, `PlayLoop`, `..Adjusted` | `Player_AnimPlayOnce`, `Player_AnimPlayOnceAdjusted`, `Player_AnimPlayLoop`, `Player_AnimPlayLoopAdjusted` |
| `Player_Anim_PlayOnceMorph`, `..MorphAdjusted` | `Player_AnimChangeOnceMorph`, `Player_AnimChangeOnceMorphAdjusted` |
| `D_8085BE84[group][type]` | `GET_PLAYER_ANIM(group, type)` (switches to MM's table for MM forms) |
| `gPlayerAnim_x` | `(LinkAnimationHeader*)mm_gPlayerAnim_x` from `mm/objects/gameplay_keep/gameplay_keep.h` (already included). Other MM objects: `mm/objects/<object>/<object>.h`, symbols prefixed `mm_` |
| `BEN_ANIM_EQUAL(anim, gPlayerAnim_x)` | inside z_player.c: `(anim == (LinkAnimationHeader*)mm_gPlayerAnim_x)`; in any other file compare the path with `strcmp` (each TU has its own copy of the string) |
| `sPlayerControlInput` | `sControlInput` |
| `sPlayerFloorType`, `sPlayerFloorEffect`, `sPlayerYDistToFloor` | `sFloorType`, `sFloorProperty`?, `sYDistToFloor` – check z_player.c statics |
| `Player_PlaySfx(this, NA_SE_x)` | `MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_x)` |
| `Actor_PlaySfx(actor, NA_SE_x)` | `MergeShip_PlayMMSfxActor(actor, MM_NA_SE_x)` |
| `Audio_PlaySfx(NA_SE_x)`, `Lib_PlaySfx_2` | `MergeShip_PlayMMSfxCentered(MM_NA_SE_x)` |
| `AudioSfx_StopById` | `MergeShip_StopMMSfxById` |
| `AudioSfx_PlaySfx(id, pos, token, &freq, &vol, &reverb)`, `Audio_PlaySfx_AtPosWithSyncedFreqAndVolume`, `Audio_PlaySfx_AtPosForMetalEffectsWithSyncedFreqAndVolume`, `AudioSfx_IsPlaying` | `MergeShip_MMSfx_PlaySfx`, `MergeShip_MMSfx_PlayAtPosWithSyncedFreqAndVolume`, `MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume`, `MergeShip_MMSfx_IsPlaying` (MergeShipMMAudio.h; MM ids). These play through MM's own sequence 0 and sound fonts (design: top of MergeShipAudio.cpp). OoT sfx ids requested by shared OoT player code are already translated for MM forms. |
| voice/floor sfx helpers (`Player_AnimSfx_PlayVoice`, `Player_PlaySfx` with `voiceSfxIdOffset`...) | use the shared helpers already in MergeShipPlayerForms.inc: `MergeShip_PlayMMVoiceSfx`, `MergeShip_PlayMMFloorSfx`, `MergeShip_PlayMMFloorSfxByAge`, `MergeShip_PlayMMFloorJumpSfx`, `MergeShip_PlayMMFloorLandSfx`, `MergeShip_PlayMMSfxAndFootstepFlag` (MM func_8082E1F0), `MergeShip_PlayMMAnimSfxList` (MM Player_PlayAnimSfx with MM ids); do not add copies |
| `Distortion_Request/SetDuration/RemoveRequest` | `MergeShip_Distortion_*` with `MERGESHIP_DISTORTION_*` (MergeShipTransformFx.h) |
| `Play_EnableMotionBlur*` | `MergeShip_MotionBlur_*` |
| `R_PLAY_FILL_SCREEN_*` | same names (MergeShipTransformFx.h) |
| `gSaveContext.save.saveInfo.playerData.magic`, `isMagicAcquired` | `gSaveContext.magic`, `gSaveContext.magicLevel != 0` |
| `Magic_Consume(play, n, MAGIC_CONSUME_x)` | OoT `Magic_RequestChange(play, n, MAGIC_CONSUME_x-equivalent)` – check OoT's z_parameter.c magic states and port MM's semantics exactly |
| `Interface_SetAButtonDoAction` | `MergeShip_SetMMAButtonDoAction` (MergeShipHud.h) |
| MM `DO_ACTION_x` | `MM_DO_ACTION_x` |
| `Camera_ChangeMode(cam, CAM_MODE_x)` for MM-only modes (GORONDASH, DEKUFLY, DEKUSHOOT, DEKUHIDE, ZORAFIN, GORONJUMP, DEKUFLYZ, ZORAFINZ, CHARGEZ) | these modes do not exist in OoT's camera; call `MergeShip_Camera_RequestMMMode(play, MM_CAM_MODE_x)` from `soh/MergeShip/MergeShipCamera.h` (include it; the camera porter implements it) |
| `CutsceneManager_*` | see how MergeShipTransformFx.c handled the transformation cutscene |
| `Actor_Spawn(..., ACTOR_EN_ARROW, ... ARROW_TYPE_DEKU_BUBBLE)` etc. | MM-only actor behavior must be ported as a new actor (see below) and spawned by its runtime id |

A pairing of every MM player function with its closest OoT function (by body similarity) is in
`build/player_func_map.tsv` (columns: mm_func, mm_line, oot_func, oot_line, similarity). Pairs under
~0.6 are often wrong; confirm by reading both. Action handler numbers match between the games
(MM `Player_ActionHandler_6` is OoT's `Player_ActionHandler_Roll`).

## Porting rules

1. Port MM's function bodies faithfully (same constants, order, frame numbers). Name each ported
   function `MergeShip_<Form>_<MMname>` or `MM_<name>` and put a comment `// MM <name>` on it.
2. Hook into OoT's shared functions with small, self-contained blocks, e.g.
   `if (this->mergeShipForm == PLAYER_FORM_DEKU) { ...; return MergeShip_Deku_X(...); }` placed where
   MM's branch sits. MM's 310 form branches in shared code are listed in `build/mm_form_sites.txt`
   (MM line numbers); port the ones for your form. Where MM's shared logic differs from OoT's even
   without a form check, but the form path goes through it, port MM's version for MM forms.
3. Other agents are editing `z_player.c` and `z_player_lib.c` at the same time for other forms.
   Use the **Edit tool only** (exact small string replacement) on shared files. Never rewrite a shared
   file wholesale, never run formatters on it, never reorder code, never touch another form's `.inc`.
   Re-read the region right before each edit.
4. Do not edit `z64player.h`, `z64.h`, `z64save.h`, `z64item.h`, `variables.h`, `functions.h`, or any
   header most files include – each change costs an hour-long rebuild. If you truly need one, stop and
   report it instead.
5. New MM actors: create `Shipwright/soh/src/overlays/actors/ovl_MM_<Name>/z_mm_<name>.c/.h` ported
   from MM's actor source, with its own struct. Register at run time with ActorDB (see
   `soh/soh/Enhancements/ExtraModes/IvanCoop.cpp`: an `ActorDBInit` with id -1 gets the next free id).
   Put the registration in a small C++ file next to it with a `RegisterShipInitFunc`, and expose the id
   as `extern "C" s16 gMergeShipActorId_<Name>` for the player to spawn with.
6. Assets: all MM assets come from mm.o2r through generated headers under `soh/assets/mm/...` with
   `mm_` prefixed symbols. Display lists and textures are path strings usable directly.
7. Do not run cmake or build the game. Check your work compiles with:
   `powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\hyper\Mergeship2\tools\syntax_check.ps1 <files...>`
   (a check of z_player.c takes a few minutes while other agents compile too; run it in the
   background or with a long timeout)
   (checks C/C++ with the soh target's includes/defines; `z_player.c` checks all included `.inc`s –
   ignore errors that are in another form's file, fix all of yours).
8. When finished, report: files created/changed, every hook inserted (function + one line), what MM
   behaviors are complete, anything left out and why.
- MM's `func_800C9DDC` (floor poly vertex-B flag 0x4000) is ported as `MergeShip_MM_func_800C9DDC` in MergeShipPlayerForms.inc; OoT has no such function.
- Camera (MergeShipCamera.h): call `MergeShip_Camera_RequestMMMode(play, MM_CAM_MODE_x)` *instead of* OoT's
  `Camera_RequestMode` in the frame MM's `Player_UpdateCamAndSeqModes` picks an MM-only mode, every frame
  (a frame with an OoT request, or with no request, returns to OoT's mode). Where MM sets
  `meleeWeaponAnimation` to an MM value, also call `MergeShip_Camera_SetMMMeleeWeaponAnimation(mmValue)`
  (MM numbering; MM's camera starts the Goron butt punch / Zora kick distortions from it).
