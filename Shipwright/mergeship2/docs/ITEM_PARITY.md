# Item, equipment and song parity: gaps and where to work (2026-09-23)

Paths are under `Shipwright/soh/` (SoH) or `2ship2harkinian/mm/` (MM).

## Adding a new MM item to SoH (the Chateau Romani pattern)
1. Give it the next id after the masks and move `ITEM_MM_LAST` to it (`MergeShipForms.h`). Free SoH ids are
   0xB8–0xFB; `ITEM_LAST_USED` is 0xFC.
2. Add an icon to `gItemIcons` (`z_inventory.c`). Icons are `mm_gItemIcon*Tex` from
   `assets/mm/archives/icon_item_static`; names are in `.../item_name_static`.
3. Add a rule to `sOoTItemMMRule` (`MergeShipItems.c`) and an age requirement in `z_play.c`.
4. Add an `Item_Give` branch (`z_parameter.c`).
5. Register a get-item entry at runtime, above `GI_MAX` (e.g. `MERGESHIP_GI_MM_CHATEAU 0x180`).
6. Bottle contents also need `MERGESHIP_ITEM_IS_BOTTLE` and a `PLAYER_BOTTLE_*` index.

## MM items SoH lacks
- **Swords:** Razor and Gilded, with Razor wear (MM `func_8083FFEC`, `z_player.c:10606-10624`).
  - Model DLs: `mm_gLinkHumanLeftHandHoldingRazorSwordDL` / `...GildedSwordDL`, the sheathed DLs, and the sheaths.
- **Great Fairy's Sword:** a two-handed C item.
  - Model: `mm_gLinkHumanLeftHandHoldingGreatFairysSwordDL`.
  - 2S2H's B-button enhancement shows how to put it on B.
- **Shields:** Hero's and Mirror (MM), with `mm_gLinkHumanHeros/MirrorShieldWithMtxDL` and the hand DLs.
- **Powder Keg:** MM `PLAYER_IA_POWDER_KEG`, the keg branch of `En_Bom`, and `sPlayerExplosiveInfo`. Goron only.
- **Pictograph Box:** first person via MM `func_80831814`, framebuffer capture, and `z_snap.c`. In Hyrule it's only
  a camera.
- **Bottle contents:** Deku Princess, Zora Egg, Gold Dust, Mushroom, Seahorse, Hylian Loach, Spring Water and Hot
  Spring Water.
  - Hot water cools on a timer (MM `Interface_UpdateBottleTimers`).
  - Pouring spawns `Obj_Aqua`.
  - The catch and release actors aren't in OoT.
- **Trade items:** Moon's Tear, the four Deeds, Room Key, Letter to Mama, Letter to Kafei, Pendant. They need
  slots, but OoT's 24-slot grid is full, so they need a new pause face or a cycle.
- **Quest items:**
  - Remains (`GID_REMAINS_*`).
  - Bomber's Notebook (show-only).
- **Collectibles:**
  - Stray fairies (`inventory.strayFairies[10]`, MM `En_Elforg`). Great Fairy's Mask detection reads
    `gMergeShipStrayFairyNearby` (`MergeShipMMMasks.c`), which is never set yet.
  - Skulltula tokens are counted separately in each game (MM swamp and ocean halves vs OoT gsTokens).
- **MM songs:** Sonata, Goron Lullaby (and its intro), New Wave, Elegy, Oath, Healing, Soaring, Inverted and
  Double Song of Time.
- **Optional:** MM's ice arrows freezing water into platforms.

## OoT items 2Ship lacks
- **Slingshot and seeds:** the icon and `ARROW_TYPE_SLINGSHOT` exist; the player action and slot don't.
- **Longshot:** an icon exists. MM's own hookshot already has Longshot reach (26).
- **Spells:** Din's Fire, Farore's Wind and Nayru's Love (port `Magic_Fire/Wind/Dark` and the spell path; Farore's
  needs `fw` / `RESPAWN_MODE_TOP`).
- **Boomerang:** MM's `En_Boom` is the Zora's fins.
- **Megaton Hammer.**
- **Tunics and boots:** MM keeps boots per form, so the human needs iron and hover rows. MM's hazard check already
  reports hot rooms.
- **Upgrades:** strength (bracelet and gauntlets) and scales (dive depth).
- **Swords:** Master Sword, Biggoron's Sword and Giant's Knife.
- **Shields:** Deku and Hylian.
- **Masks:** Skull, Spooky and Gerudo.
- **Quest items and trade items:** the OoT trade items, medallions and stones, Stone of Agony, Gerudo card.
- **Bottle contents:** Ruto's Letter.
- **Save space:** MM stores only the *equipped* sword and shield and has no owned-equipment field, so it needs a
  2S2H save extension.

## Pause-screen A toggles
- **SoH:** the cycle mechanism already exists: `KaleidoScope_HandleItemCycleExtras` (`z_kaleido_item.c:319-375`)
  and `HandleItemCycles` (`:392`), with Deku Beans as the MergeShip precedent at `:280-317`.
  - The equipment page is `z_kaleido_equipment.c`; the A press is at `:544-645`, and wrong-age items are greyed out
    at `:498`.
  - Age overrides can follow `TimelessEquipment.cpp`.
- **MM:** there is no equipment page (sword and shield are on the quest page, `z_kaleido_collect.c`), and A shows
  the item description. The toggle needs another button or a new equipment page.
- **Pairs:**
  - Kokiri / Razor / Gilded (/ Master)
  - Biggoron's Sword / Great Fairy's Sword
  - Hylian / Hero's shield, and OoT Mirror / MM Mirror shield (Deku is separate)
  - Fairy Bow / Hero's Bow: the same item with a model flag
  - Hookshot / Longshot / MM Hookshot

## Songs
- **SoH ocarina state:** the song flags are u16 (`AudioOcarina_Start` in `code_800EC960.c:1607`). They must widen
  to u32, as MM's already are, before MM songs fit.
  - Song ids: `z64.h:443-460`.
  - Warp songs: `z_message_PAL.c:3756` and `z_player.c:15389` (`sWarpSongEntrances`).
- **MM Song of Soaring:** `z_message.c:5974` → pause `PAUSE_STATE_OWL_WARP_*` → `z_kaleido_scope_NES.c:4035` →
  `En_Test7` `sOwlWarpEntrances`.
  - In SoH it needs new pause states that draw `mm_gWorldMapImageTex` with the owl icons.
  - Actually warping needs the engine switch.
- **OoT warp songs in MM:** extend MM's song table, keep the learned flags in a save extension, and add a warp
  prompt. Actually warping needs the engine switch.
- No MM note sequence is the same as an OoT one.
