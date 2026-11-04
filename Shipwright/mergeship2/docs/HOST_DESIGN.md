# One exe, both engines: host design (2026-09-23)

SW = Shipwright, 2S = 2ship2harkinian, LUS = libultraship.

## Decision
One `libultraship.dll` is shared by `soh.dll` (OoT) and `2ship.dll` (MM), both hosted by `mergeship.exe` in one window.

**Shared subsystems:** Window, Fast3D interpreter, Gui/ImGui context, AudioPlayer, ControlDeck, Logger and
CrashHandler.

**Per engine:** each engine has its own `Ship::Context`, which gives it its own ResourceManager/Loader,
Config/CVars, Console, FileDropMgr and EventSystem. The host swaps the active Context on every switch.

### Rejected alternatives
- **One ResourceManager:** the resource factories collide. The type ids are the same but the binary formats
  differ: Cutscene, SoundFont, SkeletonLimb, CollisionHeader, Skeleton and Path all differ, and MM has 4 extra
  scene commands. `RegisterResourceFactory` keeps whichever factory registers first. On top of that, 2562 asset
  paths and about 1000 CVar literals are named the same in both games.
- **Both games linked into one module:** their C symbol names collide.

## libultraship
- **Versions:** SW's LUS is at `62e973ae`, 2S's at `7f9b86a5`. The first is an ancestor of the second, 4 fix
  commits apart, and no public header differs. Use one LUS fork branch `mergeship2` at `7f9b86a5` for both repos.
- **Build patches:**
  - **P1:** a `LUS_SHARED` option instead of the hard-coded STATIC library (`LUS/src/CMakeLists.txt:3`); `__DLL__`
    as an INTERFACE define so `API_EXPORT` becomes dllimport for consumers; `/MD`; export the data symbol
    `Fast::g_exec_stack`.
  - **P2:** `Context::CreateSharedInstance(name, short, cfg, shareFrom)`, and `SetActiveInstance(Context*)` with
    `mContext` changed to an owned list plus an active pointer. Only the owning Context saves window config.
    Register the GuiTexture factory in every engine's ResourceManager.
  - **P3:** `Gui::SwapEngineWindows`, `Gui::SwapMenu` (without re-running Init) and `Gui_GetImGuiContext()`.
- **Runtime and vcpkg:** all modules use `/MD`, via vcpkg triplet `x64-windows-static-md` with a dynamic sdl2
  port. SoH calls SDL joystick functions directly, so SDL must be a single DLL. Use one shared `VCPKG_ROOT`.
- **Per-module globals:** each engine calls `ImGui::SetCurrentContext` and sets the ImGui allocators, and sets
  spdlog's default logger to the Context's logger.
- **Duplicate symbols:** SoH Release links with `/FORCE:MULTIPLE`. Audit libultra stub duplicates against the
  DLL's exports.

## Engines
- **SoH entry:** `soh/src/code/main.c:47-141`; frame loop `RunFrame` in `graph.c:437`, which is already a
  one-frame-per-call coroutine; the loop itself is `Graph_ThreadEntry`.
  - `exit(0)` at `graph.c:506` and at `OTRGlobals.cpp:1000` must become return values.
  - The IrqMgr objects at `main.c:75-77` are stack locals and must become static.
  - The audio thread self-pumps every 5 ms (`OTRGlobals.cpp:1111`), so Suspend must park it.
- **2Ship entry:** `mm/src/code/main.c:56-149` and `graph.c:371`. Its init is in `BenPort.cpp` (148-200,
  614-747, 969-1046), with an `exit(1)` at 744. Its audio thread only runs on frame signals.
- **Suspend:** stop calling RunFrame, park audio, save CVars, swap the GUI out, and clear texture/blend caches
  (they're keyed by name).
- **Resume:** set the engine's Context active, swap its GUI in, set mouse capture, set audio DesiredBuffered
  (SoH 4096, 2Ship 1680), un-park audio, and ignore input for one frame. Both engines are initialized at startup.
- **Shaders:** they're compiled from the active ResourceManager's port archive, so regenerate soh.o2r and
  2ship.o2r from the same LUS commit.

## Build (Windows)
There are four builds, not one CMake tree (target names collide).
1. **SoH** with `-DMERGESHIP_HOSTED=ON`: `add_library(soh SHARED)`, no `SDL2main`, no `/SUBSYSTEM:WINDOWS`.
2. **2Ship** with `-DMERGESHIP_HOSTED=ON`: the same changes.
3. **`host/`** (new): builds `libultraship.dll` from the fork and `mergeship.exe`.
4. **`tools/build_host.ps1`:** uses one VCPKG_ROOT, checks both LUS HEADs match, and stages `build/host` with
   the exe, the DLLs, and the o2r files.

The standalone exes stay unchanged when the option is OFF.

**Assets:** each engine keeps its current mounts (see "First run: archives" for where each comes from).
- SoH: soh.o2r, oot.o2r, mm.o2r under `mm/`, and mergeship2.o2r (`mergeship2/`).
- 2Ship: 2ship.o2r, mm.o2r as a native mount with root aliases, and SoH's oot.o2r under `oot/` (re-namespaced as
  it is read).

2Ship doesn't need `mm/` added to its asset paths.

## Engine API (`include/mergeship/EngineApi.h`, pure C)
- `MsEngine_GetAbi`: returns the LUS commit and option hash; the host refuses to run on a mismatch.
- `MsEngine_Init(MsHostServices*, argc, argv)`
- `MsEngine_RunFrame` → RUNNING / TRANSFER / EXITED / ERROR
- `MsEngine_Suspend`
- `MsEngine_Resume(MsTransfer* arrival)`
- `MsEngine_Shutdown`

`MsTransfer` is `{size, from, to, entrance, flags, sharedSave, sharedSaveSize}`. The host services provide
`RequestTransfer`.

**Transfer hooks:**
- **SoH:** at the fade-out commit in `z_play.c:911-912` (and 965, 1007, 1054). A reserved sentinel
  `nextEntranceIndex` on one Lost Woods exit (`entrance_table.h` `ENTR_LOST_WOODS_*`) triggers the transfer.
- **2Ship:** arrives at `ENTRANCE(CLOCK_TOWER_INTERIOR, n)` (`z64scene.h:755`). It needs a boot-to-gameplay path
  that skips the title screen.

## Milestones
- **M0:** LUS fork with P1; `/MD`; each game still builds as an exe but links `libultraship.dll`. Estimated 3-5 days.
- **M1:** EngineApi; both games split into DLL targets; `mergeship.exe` boots one engine. Estimated about 1 week.
- **M2:** P2 and P3; a hotkey switches between the SoH and 2Ship title screens in one window, with no audio bleed
  and 50 switches without a crash. Estimated 1.5-2 weeks.
- **M3:** the Lost Woods ↔ Clock Tower handoff. Estimated about 2 weeks.
- **M4:** host-owned shared save blob. Estimated 3-4 weeks.
- **M5+:** cross-game items, unified randomizer, Anchor through the host.

## Implementation notes (M0-M2, 2026-09-23)
What building it taught, in addition to (and where it corrects) the plan above.

- **Build layout.** `tools/build_host.ps1` configures `host/` into `build/host-lus` (libultraship.dll +
  mergeship.exe), Shipwright into `build/host-soh` and 2ship2harkinian into `build/host-2ship`, all with
  `VCPKG_ROOT=build/vcpkg`, triplet `x64-windows-static-md` from `tools/triplets/` (SDL2 dynamic), then
  stages `build/host`. The shared vcpkg root has no `.git`: both games' automate-vcpkg `git pull` it on every
  configure, and a port bump between the two game builds would link two spdlog/fmt versions into one process.
- **Outputs.** `use_props` puts every target's output in `<source>/x64/<config>` - the standalone builds'
  directories. Hosted trees walk all targets and redirect outputs into their own build tree.
- **/MD everywhere.** torch and prism spell `/MT` out in `target_compile_options`, not just through
  `CMAKE_MSVC_RUNTIME_LIBRARY`; the hosted trees rewrite those options. libultraship's archive headers
  `#undef _DLL` to steer StormLib's auto-link; with the DLL CRT that flips the STL's CRT mismatch checks, so a
  shared build keeps `_DLL` and defines `STORMLIB_NO_AUTO_LINK` instead.
- **ABI check.** `LUS_GetAbiId()` (libultraship.dll) and `MsEngineAbi.lusAbiId` (each engine) are
  `<LUS commit>-<hash of CONTROLLERBUTTONS_T, GBI_UCODE, MPQ, gfx debugger, scripting, pointer size>`. The id
  is compiled into one file per module (not a PUBLIC define) so a libultraship commit does not rebuild both
  games. LUS CVar *names* are not part of it; SoH's and 2Ship's differ only in the unused
  `CVAR_CONTROLLER_CONFIGURATION_WINDOW_OPEN`.
- **ImGui is linked into every module** (it is a static library libultraship links PUBLIC). Each game module
  registers `Gui::AddImGuiContextListener` before init, which selects libultraship's context in the game's copy
  the moment `Gui::Init` creates it (the first engine's init uses ImGui right after `InitWindow`). With one CRT
  heap the default ImGui allocators need no change.
- **The second engine's own window.** Each game constructs a `Fast3dWindow` in its `OTRGlobals` constructor.
  In a shared Context `InitWindow` adopts that window's Gui contents (the game's input editor, console, stats,
  file browser and overlay), registers the GuiTexture and Font factories in the game's ResourceManager, points
  Fast3D's interpreter global back at the shared window (its constructor had re-targeted it) and keeps the
  unused window alive forever: its destructor would shut down the shared ImGui backend.
- **Per-engine GUI state** swapped by `Gui::SwapEngineWindows`/`SwapMenu`: windows, overlay (fonts), menu
  bar, menu, ImGui style, `io.FontDefault`, `io.FontGlobalScale`, named GUI textures, and the interpreter's
  blended ("masked") textures, which are registered by resource path. A game starting after another gets
  ImGui's default style. Both engines initialize before the first frame, so all fonts are in the atlas before
  it is first built.
- **SoH audio.** `audio` in `OTRAudio.h` is a `static` in a header (one copy per translation unit), so nothing
  outside OTRGlobals.cpp can park it. Suspend stops the thread (`OTRAudio_Exit`), Resume starts a new one
  (`OTRAudio_Init`, which re-walks the already cached `audio/` directory); a new thread waits for the first
  frame before it pumps. Samples already queued in the shared device (at most DesiredBuffered, ~128 ms) still
  play after a switch; there is no flush API.
- **Shared, not per engine:** Window/interpreter, Gui, Audio, ControlDeck (both games pass identical extra
  button bitmasks, so the first engine's deck serves both), CrashHandler (each Resume re-registers its
  callback) and the logger (one log file, named after the first engine).
- **Exit.** No engine's `DeinitOTR` can run while the other's objects point into the shared window, ImGui
  context and audio device. `MsEngine_Shutdown` saves CVars, the window placement and the log; the host then
  ends the process.
- **Data imports.** Besides `Fast::g_exec_stack`, the games read libultraship's `__osMaxControllers`
  through their own plain `extern` declarations, which cannot bind to DLL data; each engine module defines
  its own copy (nothing writes it). A static libultraship also passed its PRIVATE dependencies to the
  game's link: hosted 2Ship links `libzip::zip` and `stb` itself, and OTRExporter's headers `#undef _DLL`
  too (fixed there by force-including `yvals.h` first).
- **Unattended runs.** With the displays asleep, DWM paces V-Sync'd presents at ~7.6 Hz, so both games
  crawl (also standalone). `tools/test_host.ps1` turns V-Sync off in the staged configs.

### Verified (M1, M2)
`tools/build_host.ps1` then `tools/test_host.ps1` (results in `build/tests/host_*`):
- `--only soh` / `--only 2ship`: each game alone reaches its title sequence in mergeship.exe.
- Default (both engines, SoH first): SoH title, switch, 2Ship's opening and title attract (Clock Town),
  switch back, SoH title, then 50 automated switches (3-10 ms each) and a clean exit.
- Ctrl+F12, pressed through the real input path (focused window, key held), switches both ways.
- The default (non-hosted) configure of both games is unchanged: Application, `/MT`, `/SUBSYSTEM:WINDOWS`,
  outputs in `<source>/x64/<config>`, static libultraship, no EngineExport.cpp.

### Open after M2
- One of ~20 unattended runs ended with 0xC000041D (an exception inside a window procedure) while SoH was
  still creating the window, before any switch (`build/tests/host_switch_20260923-075013`); the next
  runs, including five back-to-back startups of both engines, were clean. Not yet reproduced or explained.
- Audio bleed is prevented by stopping SoH's audio thread and by 2Ship mixing only on frames, but the tests
  run muted; nothing measures the device output across a switch, and up to DesiredBuffered samples of the
  outgoing game still play after it.
- libultra's process-wide event table (`__osEventStateTab`, `osSetEventMesg`, `osViSetEvent`) lives in
  libultraship.dll, so both games' registrations share it; harmless for the title screens, unaudited for
  gameplay. Same for other libultra stubs defined both in a game and in the DLL.
- The shared ControlDeck keeps the first engine's mappings; per-game controller configs are not swapped.

## M3: the door between the worlds (2026-09-23)
User decision: a Lost Woods exit leads to Termina's Clock Town by way of the Clock Tower, and Termina's Clock Tower
leads back. Everything is optional: `CVAR_ENHANCEMENT("MergeShip.TerminaDoor")`, i.e.
`gEnhancements.MergeShip.TerminaDoor` (default on), is the same CVar name in both games' configs
(`MS_CVAR_TERMINA_DOOR` in EngineApi.h): SoH's Mergeship menu, "Termina" section, and 2Ship's new "Mergeship" menu,
"Termina" page (`2s2h/MergeShip/MergeShipHostMenu.cpp`). soh.exe and 2ship.exe keep their games' own exits (the
door code is compiled there as stubs).

### Which exits
- **Hyrule -> Termina.** In `spot10` (the Lost Woods) every wrong turn is scene exit 1, entrance `0x286`
  `ENTR_KOKIRI_FOREST_UPPER_EXIT`: Ocarina of Time throws the player back out to Kokiri Forest. That loop is the
  door: getting lost in the Lost Woods is how Majora's Mask begins. Exit 1 taken anywhere but at the opening the
  Kokiri Forest entrance leads in through (within 400 units of spawn 0's start position, where exit 1 is the real
  way out to Kokiri Forest; see "status" below for the measured openings) becomes the reserved entrance `ENTR_MERGESHIP_TERMINA` (`0x614`-`0x617`, appended to `entrance_table.h`: scene data
  holds entrance numbers, so nothing can be inserted beside `ENTR_LOST_WOODS_*`). `Player_HandleExitsAndVoids`
  substitutes it before entrance rando's override, so entrance rando cannot shuffle the loop away.
- **Termina's arrival.** The player comes from Hyrule's Lost Woods:
  - no Majora's Mask file for this Hyrule file yet: it is created as MM's file select creates one
    (`Sram_InitSave`: `Sram_InitNewSave`, `newf`, checksum, `OnSaveInit`, written to both new-cycle pages) and named
    after the Hyrule file; Majora's Mask then begins as it always does, with its opening in Termina's Lost Woods
    (`ENTRANCE(CUTSCENE, 0)`), the Skull Kid, the Deku curse and the fall into the Clock Tower (1:1, nothing
    skipped unless 2Ship's own Skip Intro options are on);
  - a file exists: it is loaded as MM loads it (`Sram_OpenSave`: a first-cycle save continues in South Clock Town
    out of the Clock Tower's door, `ENTRANCE(SOUTH_CLOCK_TOWN, 0)`; after a Song of Time MM plays its dawn of the
    first day; an owl save continues at its statue);
  - the player was already in Termina this session (left through the door, came back): the cycle continues, same
    time of day, and they come out of the Clock Tower's door, `ENTRANCE(SOUTH_CLOCK_TOWN, 0)`
    (`MS_ENTRANCE_2SHIP_CLOCK_TOWN`).
  One Majora's Mask file per Hyrule file: the same slot number (2Ship's `saves/fileN.json`); SoH's unsaved debug file
  (`fileNum` 0xFF, the test harness) uses the third.
- **Termina -> Hyrule.** The way back is the way Link came in: the Clock Tower interior's exit 1
  (`ENTRANCE(OPENING_DUNGEON, 1)`, the passage behind the stone doors of the Happy Mask Salesman's room, back towards
  the tree stump). In MM those stone doors (`Bg_Ctower_Rot`) shut behind Link for good; for a player who came from
  Hyrule this session, with the door on, they stand open when he enters from South Clock Town. Only such a player's
  exit becomes the door; anyone else gets Majora's Mask's own exit. Arrival: `ENTR_LOST_WOODS_SOUTH_EXIT` (0x11E,
  `MS_ENTRANCE_SOH_LOST_WOODS`), the woods' first clearing.

### How a trip works
1. The player walks into the exit; the engine marks it (SoH: the sentinel entrance; 2Ship: `func_808354A4` ->
   `MergeShipHost_OnSceneExit`, which leaves `nextEntrance` valid as the fallback). The fade-out runs as for any exit.
2. Where the fade-out commits the next scene (`Play_Update` / `Play_UpdateTransition`: the instance transitions, the
   fill, instant and sandstorm ones), `MergeShipHost_HoldTransition` captures the shared save (M4), calls
   `MsHostServices::RequestTransfer` and returns true: the commit is skipped and the faded-out frame stays.
3. `MsEngine_RunFrame` returns `MS_FRAME_TRANSFER` for that frame. The host (`Host::Transfer`) suspends the requester,
   points the transfer at the slot's shared save and resumes the other engine with it.
4. The receiver's `MsEngine_Resume(arrival)` applies the shared save and either
   - releases its own held transition, which then commits the arrival entrance instead (the engine was waiting at
     the door: the normal case after the first trip), or
   - (2Ship, no session for that file) loads/creates the file before its next frame and switches its game state to
     Play, as `FileSelect_LoadGame` does; on the very first 2Ship frame there is no game state yet, so the console
     logo's first frame runs first.
5. A manual Ctrl+F12 back to an engine that is waiting at the door releases it at its own game's exit (Kokiri
   Forest; the Clock Tower's own passage). A transfer the host cannot make (the other engine is not loaded,
   `--only soh`) is refused by resuming the requester with `entrance -1`: the same release.

`MsTransfer` (API version 2) carries `saveSlot` (the Hyrule file, -1 for the debug file), `playerName` (ASCII),
`MS_TRANSFER_FLAG_TERMINA_DOOR` and `MS_TRANSFER_FLAG_SHARED_SAVE` (the sender captured; only then does the receiver
apply). 2Ship's other transfer (the Hyrule warp songs, `MergeShip_Host_RequestTransfer`) goes through the same host
path.

### The player's body (adult Link in Termina)
`MsTransfer.playerAge` (Ocarina of Time `LINK_AGE_*`) and `playerForm` (`MS_FORM_*`, SoH's `PlayerForm` numbering:
adult 0, child 1, Deku 2, Goron 3, Zora 4, Fierce Deity 5) are appended fields, valid with
`MS_TRANSFER_FLAG_PLAYER_BODY` (a zero-initialized transfer would otherwise read as adult Link) and
`MS_TRANSFER_HAS(transfer, field)`; the host marks them absent for an older, shorter sender.
- SoH sends `gSaveContext.linkAge` and `MergeShip_GetPlayerForm()`.
- 2Ship passes the age to `MergeShip_Hyrule_SetArrivalAge(u8 ootAge)` (adult Link as a 2Ship player form is another
  workstream; until it defines that function, a default does nothing: MSVC `/alternatename` in
  `MergeShipHost.cpp` makes it a weak symbol). After MM's prologue a Deku, Goron or Zora arrives as that form
  (`gSaveContext.save.playerForm`); the Fierce Deity arrives human, as MM allows that mask only in boss lairs.
- Going back, 2Ship sends its form and `MergeShip_Hyrule_GetDepartureAge()` (same weak arrangement; the default is
  the age he came with). SoH restores the age through `play->linkAgeOnLoad` (Play_Destroy then swaps the age's
  equipment, as time travel does) and the form through the MergeShip save's form, if Hyrule's file owns that
  transformation mask (otherwise the human of that age).
- The shared save carries Hyrule's equipped gear for whoever plays Link in Termina: `ootAge`, `ootButtonItems`,
  `ootCButtonSlots`, `ootEquipment`, `ootOwnedEquipment`, `ootUpgrades`. SoH writes them on every capture and
  applies the equipped part back only when `ootGearWriter == MS_ENGINE_2SHIP` (a 2Ship player that uses Hyrule's
  gear sets it).

### Test harnesses
- SoH `MergeShipTest.cpp` in mergeship.exe reads `MERGESHIP_TEST_SCRIPT_SOH` (else `MERGESHIP_TEST_SCRIPT`); new
  commands `exits [n]`, `walkexit <n> <frames> [k]`, `toexit`, `walkto`: exit polygons from the scene's collision,
  grouped into openings, and a stick steered through the camera's input yaw (`atan2(-stick_x, stick_y)` + camera).
- 2Ship `MergeShipHostTest.cpp` (`MERGESHIP_TEST_SCRIPT_2SHIP`, `trace_2ship.log`): the same language, the commands
  the door needs, `inventory`, and an input override in `PadMgr_HandleRetrace`. The two games share one environment,
  so each harness has its own variable (2Ship's own general harness, `MergeShipTest.cpp`, reads
  `MERGESHIP_TEST_SCRIPT` and must stay unset in host tests).
- Host `--transfer-test N [--settle S]`: screenshots 1 s and 4 s after every arrival
  (`host_transfer_<n>_<engine>_<t>s`), passes after the N-th arrival plus S seconds, fails (exit 5) at
  `--quit-after`.
- `tools/test_host.ps1 -Mode transfer | transfer-opening | script` writes the scripts, clears the debug file's
  Majora's Mask file (`build/host/saves/file3.json`) and runs the host.

## M4: the shared save (2026-09-23)
`include/mergeship/SharedSave.h`: one `MsSharedSave` per Hyrule file slot, owned by the host (`GetSharedSave`,
`CommitSharedSave`), on disk as `Save/mergeship_shared_<slot+1>.bin` next to SoH's `Save/file<N>.sav` (binary: magic,
version, size; written through a temp file). The debug file's lives in memory only. Values are canonical (`MsItem`,
`MsSword`, `MsShield`, `MsSong`, `MsBottle`), not either game's ids.

Projectors, one per engine: SoH `soh/MergeShip/MergeShipSharedSave.cpp`, 2Ship `2s2h/MergeShip/MergeShipSharedSave.cpp`.
- **When.** Capture when leaving through the door; apply when arriving. SoH also applies on loading a file and
  captures on saving it (hooks `OnLoadGame`, `OnSaveFile`), so the shared save is never older than the file and a
  file loaded later gets what Termina gave since.
- **Idempotent.** Nothing is added to a count: values are set, or merged by bit-or / maximum. Applying twice equals
  applying once.
- **Set both ways:** health, heart pieces, magic, rupees, arrows, bombs, bombchus, sticks, nuts, beans, the Powder Keg
  (it comes and goes with Termina's days), bottle contents (a bottle itself is never removed; a content the other
  game cannot hold, Ruto's Letter or Mystery Milk, is left in the shared save and that bottle untouched), the trade
  slots, stray fairies. So Termina's rules reach Hyrule's pockets: after a Song of Time the rupees and ammunition
  MM takes away are gone in Hyrule too.
- **Merged (only gained):** the 24 masks, songs, items (ocarina, bow, magic arrows, hookshot, bombs, bombchus,
  sticks, nuts, beans, lens, Pictograph Box, Great Fairy's Sword, Bomber's Notebook), capacities (quiver, bomb bag,
  sticks, nuts, wallet, magic, double defense), remains, owl statues. An apply adds; it never removes.
- **Equipment.** SoH knows exactly what is owned and sets the lists. MM keeps only the *equipped* sword and shield,
  so its capture adds its upgrades (Razor, Gilded, Mirror) and its apply equips the best of the chain it holds
  (never a downgrade); MM's starting Kokiri Sword and Hero's Shield are the gear Link brought and never become
  Hyrule's. MM's Hero's Shield is Hyrule's Hylian Shield; in Hyrule, Termina's Razor/Gilded/Mirror come as the
  Majora's Mask equipment variants (`MergeShip_MMEquip_Give`).
- **Hyrule's share in Termina (2026-09-23).** 2Ship now holds most of Ocarina of Time's own things natively, in its
  Hyrule views: Items (Hyrule) (`MergeShipOoTItems.h`: Slingshot + Deku Seeds + bullet bag, Boomerang, Hammer,
  Longshot, the three spells, the Skull/Spooky/Gerudo masks, the child and adult trade items), Hyrule's quest status
  (`MergeShipHyrule.h`: the six warp songs and Zelda's Lullaby, medallions, Spiritual Stones, Stone of Agony, Gerudo
  Card; Epona/Saria/Sun/Time/Storms are MM's own bits) and Equipment (`MergeShipOoTEquip.h`: Master, Biggoron's /
  Giant's Knife, Deku/Hylian/Mirror Shields, tunics, boots, strength, scale). The shared save's "Hyrule block"
  (`hyruleQuest`, `hyruleItems` (MsHyruleItem), `hyruleSeeds`, `hyruleTradeChild/Adult`, `gsTokens`, `hyruleBlock`),
  carved from `reserved` (size and version unchanged; `hyruleBlock == 0` = never written, so its zeros are ignored),
  plus the existing `songs`, `swords`, `shields`, `ootOwnedEquipment`, `ootUpgrades` carry them both ways. Masks,
  songs, quest status and equipment are merged; trade items, Deku Seeds and the Deku Shield (it burns) are set both
  ways. Hyrule's masks are what its trade slot holds, the randomizer's trade flags, and what the Happy Mask Shop lends
  (ITEMGETINF_38..3B). A trade item Termina holds goes into an *empty* Hyrule trade slot only: Hyrule's own trade
  sequence is never reordered.
- **Owned but unusable.** What one game cannot hold stays in the shared save untouched: Ocarina of Time's Keaton,
  Bunny, Goron, Zora and Truth masks and the Broken Goron's Sword in Termina (2Ship has no item for them; Termina's
  Keaton, Bunny and Truth are MM's own masks, UNIFIED_RANDO_DESIGN "Masks"), Gold Skulltula tokens (2Ship shows no
  count); Termina's Mystery Milk in Hyrule.
- **Majora's Mask's first cycle.** With "Share items during Termina's first cycle" (`MS_CVAR_SHARE_FIRST_CYCLE`, both
  Mergeship menus, default on; SoH's copy travels as `MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE`, either one off turns it
  off) 2Ship applies and captures from the first arrival. MM's opening still plays. Until MM's scripted first cycle
  is done (`threeDayResetCount > 0` and the Salesman has healed Link: the Deku Mask or the Song of Healing owned; a
  randomizer file starts after it) the apply holds back what would short-circuit that script: MM's Ocarina of Time
  (the Skull Kid has it until the rooftop; with it the Song of Time could reset time early), the Song of Time, the Song
  of Healing and the four transformation masks (any of them would let the cursed Deku take the curse off; the Happy
  Mask Salesman's scenes branch on the ocarina, the Deku Mask and the Song of Healing). `MergeShipHost_BeforeFrame`
  gives them (`MergeShipShared_ApplyHeldBack`) once the script is done, outside a cutscene or message; with 2Ship's
  Skip First Cycle that is the first frame in South Clock Town. Link's arriving form is also MM's to decide until
  then. Known cost: MM's own rules still apply to what Link brought: its first Song of Time empties rupees and
  ammunition as every later one does, and 2Ship's Skip First Cycle sets the Deku Nuts to 0. With the option off,
  2Ship neither applies nor captures before the first Song of Time, and after it holds back the same things until
  the Salesman has healed Link.

## M3/M4 status and how to test (2026-09-23)
Built: `tools/build_host.ps1` compiled all three trees with this code (host, soh.dll, 2ship.dll; the last change,
the Lost Woods opening rule, rebuilt soh.dll alone). Per the user's process change, the door and shared-save tests
are left to the integrator; only the Lost Woods data run below was executed.

- **Measured:** `build/tests/host_explore_lw/trace.log` (`--only soh`, the harness's `exits`): the Lost Woods has ten
  exit-1 openings (floor polygons). The first clearing (room 0, spawn 0 at (-3, 295)) has two: (0, 533), the way back
  to Kokiri Forest, and (0, -533), straight ahead, a wrong turn. The other eight are in the maze rooms (2, 3, 5, 7,
  8 ...). ENTR_LOST_WOODS_2/3/4 all land in room 0 too, so "not room 0" was the wrong rule: the door is exit 1
  anywhere more than 400 units from the Kokiri Forest entrance's start position.
- **mergeship.exe stack:** the games run on the host's main thread; SoH's in-progress `InitTerminaLocationTable`
  (randomizer, another workstream) overflowed the default 1 MB there (0xC00000FD in `__chkstk`). The host now
  reserves 16 MB (`/STACK:16777216`, host/CMakeLists.txt).

Run (after `tools/build_host.ps1`; muted, V-Sync off in the staged configs, the SoH harness keeps the display awake):

    powershell -ExecutionPolicy Bypass -File tools\test_host.ps1 -Mode transfer
    powershell -ExecutionPolicy Bypass -File tools\test_host.ps1 -Mode transfer-opening

Expected, `-Mode transfer` (build/tests/host_transfer_<stamp>/; exit code 0, host log "TRANSFER TEST PASSED: 3
transfers"):
1. `trace.log` (SoH): `DOOR exit: wrong turn out of Lost Woods room 0 at (0, -5xx)`, `SHARED capture`, `DOOR away`;
   host log `transfer requested: soh -> 2ship, entrance 0xD800, flags 0x7, file slot -1 ("Link"), age 1, form 1`.
2. `trace_2ship.log`: `DOOR booting`, `DOOR arrived: created file 3`, `SHARED apply gen=... (first cycle: MM's
   ocarina, Song of Time/Healing and transformation masks held back)` (new file; with "Share items during Termina's
   first cycle" off: `SHARED apply skipped`), `SHARED held back given: N` once the skip has ended the first cycle,
   then 2Ship's Skip Intro/Skip First Cycle put Link in South Clock Town (`PLAY ready scene=111` /
   `mm_01_south_clock_town.png`, `host_transfer_1_2ship_04s.png`); `GIVE 0x39` (Bunny Hood).
3. MM walks into the Clock Tower (`walkexit 1` = South Clock Town exit 1, `ENTRANCE(CLOCK_TOWER_INTERIOR, 1)`),
   `actors 0x061` shows the two stone doors open (`Bg_Ctower_Rot` params 1, 2 at +80), `walkexit 1` = the interior's
   exit 1 (`ENTRANCE(OPENING_DUNGEON, 1)`) -> `DOOR exit: the Clock Tower's passage`, `SHARED capture ... masks=...`
   with bit 4 (Bunny Hood), `DOOR away`.
4. SoH: `DOOR arrived: back from Termina: entrance 0x11E`, `DOOR body: age 1 form 1`, `SHARED apply`, `MASKS
   owned=... ` and `soh_02_back_in_lost_woods.png`, `host_transfer_2_soh_04s.png`: the Lost Woods' first clearing.
5. SoH takes the wrong turn again -> 2Ship `DOOR arrived: back from Hyrule: entrance 0xD800` (the same session, same
   cycle and time), `SHARED apply gen=...` (now after the first cycle), `INVENTORY masks:` with 0x40 in
   `SLOT_MASK_GREAT_FAIRY` (the mask SoH was given with `givemmmask 11`), `mm_04_mask_page.png` (MM's mask page).

Expected, `-Mode transfer-opening`: one transfer; MM creates file 3 and plays its opening untouched:
`mm_opening_05s/15s/30s/60s.png` show Termina's Lost Woods, Link on Epona, the Skull Kid.

`build/tests/door_carry_all/` (script_soh.txt, script_2ship.txt, script_2ship_opening.txt; `-Mode script
-Transfers 1 -Settle 60`): the debug file owns all of Hyrule (`giveall`) and crosses; 2Ship logs every Hyrule view
(`inventory`, `shared`) and screenshots each pause page. The expected lines are in the scripts' headers.

If a step fails, `walkexit` steering is the likeliest culprit (a straight line to the opening's centre; walls in
between stop it): the openings are logged (`EXIT n opening k: center=`), and `toexit n [dist] [k]` teleports in front
of one first.

### Next (M5)
- Termina items in 2Ship's player (Hyrule's items usable in MM: another workstream's `MergeShipOoTItems`), and adult
  Link as a 2Ship player form, which must define `MergeShip_Hyrule_SetArrivalAge` / `_GetDepartureAge` and, if it
  changes Hyrule's gear, write the shared save's `oot*` block with `ootGearWriter = MS_ENGINE_2SHIP`.
- Songs and warps across engines: the Song of Soaring from Hyrule (`MergeShip_RequestTerminaWarp`, still a stub) and
  Hyrule's warp songs from Termina (`MergeShip_RequestHyruleWarp`, 2Ship) as transfers with an entrance; both should
  capture the shared save and set `MS_TRANSFER_FLAG_SHARED_SAVE` / `_PLAYER_BODY` as the door does.
- Owl saves on leaving Termina: today Termina's progress since MM's last save lives only in memory until the process
  ends (MM's own rule); the shared save keeps what crossed.
- Unified rando: `MsItems.def` over the canonical ids of SharedSave.h.

## Anchor through the host (2026-09-23)
User requirement: all of SoH's Anchor in 2Ship too, and players never disconnect when switching worlds.

**One connection per process.** `host/NetService.cpp` owns the Anchor socket (Winsock, TCP keep-alive 30 s / 5 s).
It is offered to both engines as `MsHostServices.net` (`include/mergeship/AnchorNet.h`), appended without an API
bump (`MS_HOST_HAS(services, net)`; an engine without it keeps its own SDL_net socket, as soh.exe and 2ship.exe do).
- `Connect(address, port, hello)`: the first engine to enable Anchor opens it; the host sends the stored hello (the
  HANDSHAKE) first on every connect and reconnects by itself with back-off. `connectionId` counts connects; it does not
  change when the player switches worlds.
- Every received packet is queued for every engine with a process-wide number. Each engine's `Network` is a view of
  the host connection (`UseHostTransport`): a link thread, started at engine init and running while the engine is
  suspended, mirrors `enabled`/`connected` and drains that engine's queue. A parked engine drops per-frame packets
  (PLAYER_UPDATE, sounds, damage, teleports) and keeps the rest for the player's return.
- `Claim(seq)`: what must happen once per process (an item, a team state reply, a notification) is applied by the
  first engine that claims it; UPDATE_TEAM_STATE's replayed queue gets derived numbers so both engines agree.
- Blobs: the room settings the connection was made with (the other engine adopts them), and each world's latest
  team state part.
- Enabling or disabling Anchor in either game's menu does it for both. The host logs `net: Anchor connected ...
  (connection N)` and, at every switch, `net: active engine E, Anchor connection N up`; each engine logs
  `ANCHOR suspended|resumed: engine ..., connection N, clientId X` through the host.

**Protocol additions** (a stock SoH client ignores them): clientVersion `MergeShip2-Anchor-1` in both games;
client state `world` ("oot"/"mm") and `sceneName`; a Termina player's `sceneNum` is `0x1000 + scene` (never a Hyrule
scene); `MM_GIVE_ITEM`, `MM_SET_FLAG`/`MM_UNSET_FLAG`, `MM_UPDATE_DUNGEON_ITEMS`, `MM_SET_CHECK_STATUS`; GIVE_ITEM and
MM_GIVE_ITEM carry `msItem` (MsItems.def key); UPDATE_TEAM_STATE's `state.mergeShipMM` is Termina's part.

**2Ship's Anchor** (`2s2h/Network/`, ported from `soh/Network/`): Network, Anchor, packets, room window and menu
(Network > Anchor; options also in the Mergeship menu). Dummies are Majora's Mask's own Player (built as Kafei is,
`play->playerInit` on the form's skeleton; a form change respawns it), drawn with `Player_Draw` only for Termina
players in the same scene. Where 2Ship lacks SoH's hooks, `PollGameState` reads the scene, dungeon items, keys, stray
fairies, the randomizer's checks and the ocarina note once a frame and sends changes; player sounds come from
`VB_LINK_VOICE_PITCH_MULTIPLIER`. Flags are cycle-aware: a flag a Song of Time keeps (z_sram_NES.c's persistence
masks) is shared at once, any other only between players in the same `threeDayResetCount`.

**Cross-world items.** An item found in the other world is offered to `MergeShipShared_ReceiveRemoteItem(msItemKey)`
(extern "C", one per engine, owned by the shared-save projectors; a weak default returns 0 until they define it). A 1
claims the packet; a 0 leaves it for the item's own engine, which applies it natively when the player returns.

**Test.** `tools\test_anchor.ps1` (after `tools\build_host.ps1`): starts `tools\anchor_mock_server.py`, runs the door
route with Anchor on (both engines' `anchor` harness command, loopback players in both worlds, a mock bot's Hyrule
item arriving while the player is in Termina) and writes `build\tests\anchor_<stamp>\anchor_result.txt`
(`tools\anchor_check.py`): one host connect, no close during the run, connection 1 up at every switch, one client id,
one server connection and handshake, the item applied exactly once. Not run yet (agents do not build).

## First run: archives (2026-09-23)

An install is the staged build (`tools\build_host.ps1`; `-InstallDir <folder>` stages a clean copy) and nothing made
from a ROM. The player supplies their own Ocarina of Time and Majora's Mask ROMs; the first launch of mergeship.exe asks
once for each and makes every archive both games need, then boots. No tool has to be run by hand.

| Archive | Made by | When | Read by |
|---|---|---|---|
| `soh.o2r` | `GenerateSohOtr` (repo assets, `soh-o2r-packer`) | build | SoH |
| `mergeship2.o2r` | `GenerateMergeShip2Otr` (PR #1's `mergeship2/assets/o2r`), chained by `GenerateSohOtr` | build | SoH (`MergeShip_SohFd_MountAssets`) |
| `2ship.o2r` | `Generate2ShipOtr` (repo assets, ZAPD `--norom`) | build | 2Ship |
| `oot.o2r` / `oot-mq.o2r` | SoH's torch, in `soh.dll` (`OTRGlobals::RunExtract`) | first run | SoH; 2Ship under `oot/` |
| `mm.o2r` (namespaced `mm/`) | 2Ship's ZAPD, in `2ship.dll`, asked for by SoH's first-run flow | first run | SoH under `mm/`; 2Ship natively |

`build_host.ps1` builds the three port archives in the hosted trees (targets `soh GenerateSohOtr`,
`2ship Generate2ShipOtr`) and stages them from `build\host-soh\soh\` and `build\host-2ship\mm\`, with both games'
extractor files (`assets\`: SoH's torch ymls, 2Ship's xml/filelists/configs). A missing output fails the stage; it
no longer copies archives from the standalone builds, and stages nothing made from a ROM.

**The flow.** SoH initializes first and owns the window, so its first-run flow (`RunExtract`) does it all:
1. No `oot.o2r`/`oot-mq.o2r`: SoH's own prompts (ROMs found beside the exe, or a file box), extracted in-process by
   torch with a progress bar. Unchanged from SoH.
2. Hosted only (`ES_MM`): no `mm.o2r`, or one without the `mm/` namespace (a stock 2Ship archive, kept as
   `mm.o2r.stock`), and 2ship.dll is loaded: a popup asks for the Majora's Mask ROM (Select ROM / Not now), SoH
   validates it (`MergeShip_PrepareMajorasMaskRom`: size, z64/v64/n64, CRC of US 1.0 or US GameCube) and writes a
   big-endian copy to a temp folder, then `MsHostServices.ExtractRom(MS_ENGINE_2SHIP, ...)` has 2ship.dll build
   `mm.o2r` with its own extractor (`MsEngine_ExtractRom`: `Extractor::RunFileStandalone` + `CallZapd`, exactly
   2Ship's first-run path, before 2Ship's `MsEngine_Init`) on SoH's extraction thread, with the same progress popup.
   The result is checked (`MergeShip_IsNamespacedMajorasMaskArchive`) and installed beside the exe.
3. `OTRGlobals::Initialize` mounts oot.o2r, mm.o2r (`mm/`) and mergeship2.o2r.
4. 2Ship initializes: `RunExtract` finds `mm.o2r` (same exporter, 2Ship's port version) and does not ask; `Initialize`
   mounts it natively and mounts SoH's `oot.o2r` (else `oot-mq.o2r`, else a pre-namespaced `oot-assets.o2r`) through
   `MergeShip::AssetArchive` in `AssetArchiveMode::Rebase`.

**oot-assets.o2r is gone from the hosted build.** `AssetArchiveMode::Rebase` lists every entry of the stock archive
under `oot/`, hides its metadata (so OoT's version never enters 2Ship's version check), and as each resource loads
rewrites its references: in display lists, the 64-bit resource hash after each hash-carrying Fast3D opcode becomes the
hash of the namespaced path; in other non-leaf resources, each length-prefixed string that names an entry of the
archive (optionally `__OTR__`-prefixed) gets `oot/` inserted. Textures, arrays, vertices and samples are left alone.
`tools\verify_asset_namespace.py <oot.o2r> <oot-assets.o2r> --rebase` mirrors that code line for line: over the whole
archive (39055 resources, 13963 rewritten) the result equals torch's `--namespace oot` export byte for byte. One
archive, extracted once, serves both games, and a re-extraction by SoH needs nothing else regenerated.
`tools\build_cross_assets.ps1` still makes `oot-assets.o2r` for a standalone 2ship.exe package; the stage deletes an
old copy from `build\host`.

**Declining.** "Not now" for Majora's Mask: SoH starts without MM content, and 2Ship asks again with its own popup
when it starts (it cannot run without `mm.o2r`; its "No" exits, as standalone). `--only soh` never asks (no 2ship.dll;
SoH's `mm-extractor` offer stays a soh.exe-only path and is skipped when hosted).

**Order.** The first run must be SoH-first (the default). With `--engine 2ship`, 2Ship starts before `oot.o2r` exists,
so it runs without OoT assets until the next launch (its own flow still builds `mm.o2r`).

**Unattended (tests).** `MERGESHIP_OOT_ROM` and `MERGESHIP_MM_ROM` name ROM files (`.z64`/`.n64`/`.v64`) to use
without any question while the archive is missing: SoH's `RunExtract` takes the OoT ROM as a command-line ROM and
skips the "Run SoH?" question; `ES_MM` skips the popup and file box; 2Ship's `RunExtract` and soh.exe's `mm-extractor`
offer honour `MERGESHIP_MM_ROM` too. Failures are logged and the flow continues without the archive. See
`build\tests\first_run_archives\README.txt`.

**API.** `MsHostServices.ExtractRom` (appended, `MS_HOST_HAS`) and the optional export `MsEngine_ExtractRom`
(`MsEngine_ExtractRom_Fn`), resolved by `EngineModule::Load` without failing when absent. `romPath` NULL asks whether
the engine can extract. 2Ship's export copies only its own extractor files to a temp install folder (the shared
`assets\` also holds SoH's 100 MB of ymls, which `CallZapd` would copy) and restores the working directory `CallZapd`
changes.

**Open.** Not built or run yet (agents do not build). `Generate2ShipOtr` needs the ZAPD executable and Python in the
hosted tree; if ZAPD.exe does not link there, build `2ship.o2r` in the standalone tree at the same commit and point the
stage at it. 2Ship's checked-in `mm/assets/custom/shaders` differ from libultraship's, as before.
