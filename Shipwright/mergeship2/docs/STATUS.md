# MergeShip2 status and acceptance criteria

Updated 2026-09-22. The user requires assets in both directions and fully functional
MM transformations in OoT. The complete request is **not yet implemented**.

## Verified this session

- The MM runtime builds as `2ship2harkinian/x64/Debug/2ship.exe`.
- `soh-torch --namespace oot` exports `oot-assets.o2r` without modifying the stock OoT archive.
- All 39,030 unique exported resources compare byte-for-byte to the stock OoT
  archive after normalizing 14,950 serialized path references and 102,678 graphics
  hashes. No missing, extra, or otherwise changed resources. Report:
  `build/oot-assets/namespace-report.json`.
- MM's running resource manager loads both OoT player skeletons and every limb,
  plus the Master Sword hand display list, a button texture, a player animation,
  a soundfont (including its sample references), and a sequence.
- OoT path points are converted into MM paths with a verified point count, coordinates,
  and default MM metadata. Both changed OoT C++ files also compile; a full OoT rebuild
  is not verified because of the existing unfinished MM-player integration.
- MM's opening sequence renders with the foreign archive mounted. Direct window
  capture: `build/runtime-mm/mm-window.png`. Logs:
  `build/runtime-mm/logs/2 Ship 2 Harkinian.log`.
- A stock unnamespaced OoT archive is rejected before mounting; MM still reaches
  its native opening scene. Negative-test log: `build/runtime-mm-rejection/logs/2 Ship 2 Harkinian.log`.
- The first isolated package omitted `assets/` and showed an extraction dialog.
  The corrected package includes the folder and passes startup.

These are asset export/parse and native MM startup checks. They do not prove
foreign models rendered in gameplay, foreign audio playback, foreign scenes
executing, or transformation behavior.

## Transformations in OoT (updated 2026-09-22)

MM's form code is ported into OoT's player as `.inc` files (soh/soh/MergeShip/forms/) with small hooks in
z_player.c / z_player_lib.c; MM's sound engine plays MM sounds from mm.o2r. Verified in the scripted
harness (build/tests/*, traces and screenshots) and by the user in play:

- Mask put-on / take-off cutscenes with MM's camera (face close-up), flash, sounds; Link stays in place.
- Deku: spin, bubble (continuous breath sound), guard, pipes drawn. Goron: punches, guard, roll with
  textured ball and spikes, pound, drums drawn. Zora: punches, fins, barrier, guitar drawn.
  Fierce Deity: sword combo, beams, spin attack, jump attack.
- Form heights: MM's per-form root scale (Deku 0.3, Goron 0.74, Zora 1.0) from either OoT age.
- MM voice clips for all forms (voice channel font mapping fixed).
- Pause: Masks page and correct "To <page>" labels.
- Camera: MM's travel-following Normal1 yaw for forms (no swing during the Deku spin).

Fixed classes of bug to watch for: OoT age checks (LINK_IS_ADULT, 0.64f) reaching MM forms; OoT table
offsets sized for 2 ages; SoH enhancements assuming OoT Link (customequipment.cpp); continuous MM sfx
without `- SFX_FLAG`; MM DLs indexing segment 0x0C (use MergeShip_Scene_SetRenderModeXlu).

In progress (parallel agents, see docs/AGENT_BRIEF.md): instrument playing (drum hits, strums, exiting,
song fanfares on the form's instrument), Zora fast swim and breach, MM's full normal camera per form and
the Goron roll camera, a 1:1 audit (heights, materials, hand shapes, enhancement hooks, sounds), and MM
human Link as Young Link (MM body/anims/voice/ledge flips, OoT equipment).

Required acceptance, for each of Deku, Goron, Zora and Fierce Deity:

- Real owned mask items, inventory/pause equip, native button behavior, transformation
  and unmasking, returning to the original OoT age without corrupting equipment.
- Native MM locomotion, animations, collision, combat, health/magic rules, form
  sounds, camera and interaction behavior integrated with OoT's actors.
- Deku: spin, bubbles, water hops, flower entry/launch/glide, pipes.
- Goron: punches, curl/roll/spikes, pound, strength/heat/water rules, drums.
- Zora: punches/kick, fin boomerangs, swimming/diving/breach, electric barrier, guitar.
- Fierce Deity: sword attacks, beams, reach/damage/magic and matching animation.
- Correct HUD/item restrictions, doors, ledges, damage/death, cutscenes, scene changes,
  saving/reloading and returning to human forms.
- In-game input, rendering and audio validation against MM; loading a skeleton,
  surviving startup or passing `tools/test_forms.ps1` is insufficient.

The user explicitly prohibits using the original Mergeship code. Native MM in
`2ship2harkinian` and OoT in `Shipwright` are the implementation references.

## Reproduce the asset test package

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_cross_assets.ps1 -Rom "<your OoT ROM>" -Launch
```

Existing builds/exports can be restaged with `-SkipBuild -SkipExport`. The script
compares against `Shipwright/x64/Debug/oot.o2r`; it assumes that archive and the ROM
are the same supported version. It preserves the runtime's existing saves/config.
The staged game is `build/runtime-mm/2ship.exe`.
