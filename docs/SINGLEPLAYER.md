# MergeShip singleplayer mode

The user's design (2026-10-07). One Mergeship menu option turns it on ("Singleplayer"); it never applies to a
randomizer or unified-seed file (the option is locked there and every check below also requires a non-rando file).

## 1. The traveling Happy Mask Salesman (Hyrule only)

- MM's own model and animations (mm.o2r `object_osn`): the skeleton, eye/mouth textures, head tracking.
- Walks only on ground, on a seamless loop cut from MM's `gHappyMaskSalesmanWalkAwayAnim`: frames 51..69 return to
  the same pose (an 18-frame stride, ~2.6 units/frame); frames 0..~24 are the turn-around start, used when he leaves.
  The root's forward drift is removed in OverrideLimbDraw and the actor moves itself.
- Where he is: a timer re-rolls his place at random: his shop in Castle Town Market, or roaming one of Castle Town,
  Kakariko, Death Mountain (Trail), Zora's River, Zora's Domain, Hyrule Field. The roll is per file and per time slot,
  so reloading does not move him. While he is out, the Happy Mask Shop is closed (its door reads closed, as the
  Market's shops do at night). He never appears in Termina.
- Talking: his MM theme (MM sequence 0x0A, "Song of Healing (HMS)") plays; his poses switch abruptly from line to line
  as in MM (bows, hands clasped, think, believe, shake head, hand out, arms out...).
- Stock: all 23 MM masks -- the 20 regular masks (Postman's Hat .. Giant's Mask) and Deku, Goron, Zora. Each bought
  once, for rupees.
- Adult Mask: only sold when he meets Link as an adult, with a remark that Link has grown up.
- Fierce Deity's Mask: his gift once every mask is bought. Menu option: "salesman only" (the moon child no longer
  gives it) or "salesman or moon".
- Masks bought in Hyrule are already Termina's too, so they need not be found again on the MM side.

## 2. The end of Ocarina of Time

- After Ganon, no credits: after talking to Zelda, Link becomes a child again and is in the Temple of Time.
- Navi does not fly away; she suggests visiting the Lost Woods.
- The Termina pause pages (C-Up: Termina items, map, owl warps) are enabled only once OoT is completed.

## 3. The way to Termina

- Termina can only be entered after beating OoT.
- The Happy Mask Salesman blocks the Lost Woods door. He asks whether to bring the masks: yes -> he walks away
  (MM's walk-away animation); no -> he buys all the masks for rupees, then walks away.

## 4. Majora's Mask as New Game Plus

- Ocarina of Time's items usable from the start, and the Adult Mask from the start.
- Navi replaces Tatl wholesale: model colours, textures, voice (OoT's Navi sounds), and dialog -- her hint lines in
  Navi's voice and the story scenes (intro, Tael, Skull Kid, the moon) rewritten so Navi fits as Link's partner.

## How it is built (2026-10-08)

- **Option**: `gEnhancements.MergeShip.SPMode` ("Singleplayer Mode"), `gEnhancements.MergeShip.SPFierceDeityFromMoon`;
  `MergeShip_Singleplayer_Active()` also needs a non-rando file in slots 1-3. Debug: `SPDebugPlace` pins his place.
- **Schedule** (Shipwright `soh/MergeShip/MergeShipSingleplayer.cpp`): his own clock (frames of play, saved), stays of
  4800 frames, then he walks the route to the next place on paths planned from the scenes' collision
  (`tools/sp_navplan.py`); Zora's River <-> Zora's Domain and the shop door are fade portals. As a child every third
  stay is his shop (the Market shop door open; otherwise it reads "Closed"); the adult's places leave out the ruined
  Market. Masks: three at a time from a per-file shuffle that shows every unowned mask before any repeats.
- **Actor** (`ovl_MM_En_Osn_Travel`): the walk clip's root travel moves the actor each frame (feet planted, the
  collision goes with him); no blend between walk and idle (the clip's root carries a stride and a half turn). He keeps
  walking unless Link talks to him; he only pauses at a spot when Link is within ~300 units.
- **Ending**: `z_demo.c` terminator 117 (the end of the sealing / Zelda's farewell, before the credits) ->
  `MergeShip_Singleplayer_OnEnding`: child Link to the Temple of Time (spawn 4), the file saved, Navi speaks (text
  0x7E40, forced with a negative `naviTextId`, which is why its id is below 0x8000).
- **Termina gate**: `TerminaLocked()` keeps the C-Up Termina pages, the Lost Woods door and owl warps shut until the
  ending; the salesman waits ahead of Link in the Lost Woods (talk with A) until the meeting is done.
- **Transfer**: SoH sends `MS_TRANSFER_FLAG_SINGLEPLAYER` (and `..._SP_FD_FROM_MOON`) on every trip
  (`include/mergeship/EngineApi.h`); 2Ship's `MergeShipSingleplayer.cpp` turns on Navi: resting colours, OoT's Navi
  samples in MM's fairy voice entries (font 0 sfx 0x2B/0x2C/0x2F/0x30/0x34), her call, the "Navi" C-Up label, every
  Tatl line rewritten (`MergeShipNaviLines.inc`), and the moon's child keeps the Fierce Deity's Mask unless allowed.

## Phases

1. Option + traveling salesman + closed shop + shop menu (this file's section 1).
2. OoT ending -> child in the Temple of Time, Navi stays; C-Up Termina pages after completion.
3. Termina gate and the salesman at the Lost Woods door.
4. MM new game plus start; Navi as Tatl (model, voice, then dialog).
