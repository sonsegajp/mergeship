#include "SohMenu.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/AnchorNet.h> // MergeShip2: the Anchor options both games share
#include <mergeship/EngineApi.h> // MergeShip2: MS_CVAR_SHARE_FIRST_CYCLE
#ifdef MERGESHIP_HOSTED
#include <mergeship/MenuMirror.hpp> // MergeShip2 (BUG-0096): Termina's settings in this menu
// soh/MergeShip/EngineExport.cpp: 2Ship's menu as plain data and its CVars (NULL before 2Ship has started).
const MsMenuMirror* MergeShip_OtherMenuMirror();
Ship::ConsoleVariable* MergeShip_OtherConsoleVariables();
#endif

extern "C" {
#include "variables.h" // MergeShip2: IS_RANDO (the singleplayer options are locked on randomizer files)
}

// MergeShip2 -- the "Mergeship" menu: every Majora's Mask feature MergeShip adds is an optional
// enhancement toggled here. Each option is the CVar its feature already reads; defaults match.

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

// MergeShip2 (soh_fd): the dropdowns of the "Forms & Fierce Deity" page. The values are the ones MergeShipSohFd.h
// names (MERGESHIP_FLIPS_*, MERGESHIP_MOMENTUM_*, MERGESHIP_DOGS_*); keep them in sync.
static const std::map<int32_t, const char*> sMMFlipsValues = {
    { 0, "Off" },
    { 1, "Majora's Mask" },
    { 2, "Cosmetic" },
};
static const std::map<int32_t, const char*> sMMLedgeMomentumValues = {
    { 0, "Off" },
    { 1, "All" },
    { 2, "Forms Only" },
};
static const std::map<int32_t, const char*> sMMRollDamageValues = {
    { 0, "Off" },
    { 1, "Human" },
    { 2, "Forms" },
    { 3, "All" },
};
static const std::map<int32_t, const char*> sBunnyHoodFitValues = {
    { 0, "Off" },
    { 1, "Adult" },
    { 2, "Fierce Deity" },
    { 3, "Both" },
};
static const std::map<int32_t, const char*> sDogReactionValues = {
    { 0, "None" },
    { 1, "All" },
    { 2, "All But Richard" },
    { 3, "Only Richard" },
};
// MergeShip2 (soh_fd): MergeShipFormColors.cpp's CVAR_FORM_COLORS_* values.
static const std::map<int32_t, const char*> sFormTunicColorsValues = {
    { 0, "Off" },
    { 1, "Deku, Goron, Zora" },
    { 2, "All" },
};
static const std::map<int32_t, const char*> sFdUnrestrictItemsValues = {
    { 0, "Off" },
    { 1, "Deku Nuts, Bombs & Spells" },
    { 2, "All (Except Swords + Shields)" },
};

void SohMenu::AddMenuMergeShip() {
    AddMenuEntry("Mergeship", CVAR_SETTING("Menu.MergeShipSidebarSection"));

    WidgetPath path = { "Mergeship", "Majora's Mask", SECTION_COLUMN_1 };
    AddSidebarEntry("Mergeship", path.sidebarName, 2);

    // Transformation masks
    AddWidget(path, "Transformation Masks", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TransformationMasks"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Lets the Deku, Goron, Zora and Fierce Deity masks transform Link, with Majora's Mask's cutscene, "
            "abilities, sounds and HUD. When off, a mask can still be taken off but none can be put on."));
    AddWidget(path, "Masks on D-pad", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("MergeShipDpadMasks"))
        .Options(CheckboxOptions().Tooltip(
            "On loading a file, and at once when turned on with a file loaded, grants the four transformation "
            "masks and puts them on the D-pad: Up Deku, Down Goron, Left Zora, Right Fierce Deity. In a randomizer "
            "file nothing is granted: masks already found go onto empty D-pad buttons."));
    AddWidget(path, "Adult Mask", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.AdultMask")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Every file starts with the Adult Mask, on the Fierce Deity's Mask's cell of the Masks page. Put on by "
            "young Link it transforms him, with Majora's Mask's transformation cutscene, into adult Link, where he "
            "stands (the world keeps its era). While he is the adult it is the Child Mask -- the Skull Kid's Link "
            "mask -- and putting it on plays the same cutscene and turns him into young Link. Once you have the "
            "Fierce Deity's Mask too, press A then Right on that cell to swap which mask it holds. Termina has the "
            "same mask."));
    AddWidget(path, "Hyrule's Goron and Zora Masks Transform", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MergeOoTMasks"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Ocarina of Time's Goron Mask and Zora Mask are Majora's Mask's transformation masks: borrowing one at "
            "the Happy Mask Shop also gives the transformation mask for good, and wearing it transforms. Turned on "
            "while holding one, its transformation mask is given at once. The shop still lends and swaps its masks "
            "as before. In a randomizer seed with Shuffle Transformation Masks on, "
            "the shop's masks stay cosmetic."));
    AddWidget(path, "Fierce Deity Anywhere", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("MergeShipFierceDeityAnywhere"))
        .Options(CheckboxOptions().Tooltip(
            "Lets you use the Fierce Deity's Mask anywhere. When off, it only works in boss rooms and the fishing "
            "hole (as Majora's Mask 3D lets the Fierce Deity fish), and you turn back when you leave, matching "
            "Majora's Mask."));
    AddWidget(path, "Majora's Mask Camera for Forms", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMFormCamera"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Transformed, the normal camera follows the direction of travel as in Majora's Mask (no swing "
            "during the Deku spin, no idle swing behind the player)."));
    AddWidget(path, "Deku Beans", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.DekuBeans"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "With the Magic Beans and the Deku Mask, press A on the Magic Beans in the pause menu to switch them to "
            "Deku Beans. Deku Link plants a Deku Bean anywhere in front of him and a Majora's Mask Deku Flower "
            "sprouts at once. Deku Beans do not use up Magic Beans; up to three flowers stand at a time."));
    AddWidget(path, "MM Button Textures", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMHud.Buttons"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The B and C buttons are drawn with Majora's Mask's button texture."));
    AddWidget(path, "MM Heart Textures", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMHud.Hearts"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The life meter's hearts (and Double Defense's) are Majora's Mask's."));
    AddWidget(path, "MM Rupee Icon", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMHud.Rupees"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip("The rupee counter's icon is Majora's Mask's."));
    AddWidget(path, "MM Link Icon On The Dungeon Map", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMHud.MapLink"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The pause screen's dungeon map marks Link with Majora's Mask's head icon."));
    AddWidget(path, "Deku Beans Are Ammo", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.DekuBeanAmmo"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Deku Beans and Gold Deku Beans hold up to 10 each (full when first obtained) and planting uses one. "
            "Once you have them, cut grass sometimes hides more. Off: unlimited."));
    AddWidget(path, "Zora Waterfall Gate", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ZoraWaterfallGate"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Zora cannot swim or breach up a waterfall into the higher water above it (Hyrule's rivers and "
            "falls are steps of water). Off, the Zora swims up any water as in Majora's Mask."));
    AddWidget(path, "Goron Breaks Bomb Walls", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.GoronBreaksBombWalls"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Goron's punch and its spiked roll break walls and boulders that bombs break. Dodongo's "
            "Cavern's entrance boulder still needs a bomb or a bomb flower."));
    AddWidget(path, "Goron Breaks Megaton Hammer Objects", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.GoronHammerObjects"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Goron's punch and its ground pound work what the Megaton Hammer works: rusted floor switches, the "
            "Fire Temple's hammer steps, sinking pillars and totems, and bronze boulders. Boulders, crates and "
            "hidden grottos that a bomb also opens stay breakable while this or Goron Breaks Bomb Walls is on."));

    // Majora's Mask's other masks
    AddWidget(path, "Majora's Mask's Masks", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Majora's Mask's Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMMasks"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Majora's Mask's twenty other masks can be worn from the Masks page, by young and adult Link, and do "
            "what they do in Majora's Mask. When off, a mask can still be taken off but none can be put on."));
    AddWidget(path, "All Majora's Mask Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("MergeShipAllMMMasks"))
        .Options(CheckboxOptions().Tooltip(
            "On loading a file (not a randomizer file), and at once when turned on with one loaded, grants Majora's "
            "Mask's twenty other masks, to try them from the Masks page."));
    AddWidget(path, "Giant's Mask in Boss Lairs", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.GiantsMaskInLairs"))
        .Options(CheckboxOptions().Tooltip(
            "Majora's Mask only allows the Giant's Mask in Twinmold's lair, which Hyrule does not have. This allows "
            "it in Hyrule's boss lairs."));
    AddWidget(path, "Stone Mask", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.StoneMask"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Stone Mask, the enemies that ignore it in Majora's Mask ignore Link (Bubbles, Guays, Keese, "
            "Mad Scrubs, Dodongos, Leevers, Like Likes, Skulltulas, Tektites, Beamos), and Gerudo guards cannot see "
            "him (they still hear him running)."));
    AddWidget(path, "Bremen Mask Followers", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.BremenFollowers"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Marching with the Bremen Mask, a dog and nearby cuccos fall in behind Link, as Majora's Mask's dogs and "
            "chicks do."));
    AddWidget(path, "Captain's Hat", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.CaptainsHat"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Captain's Hat, Stalchildren take Link for their captain: they stop attacking and can be "
            "talked to."));
    AddWidget(path, "Captain's Hat Hints", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.CaptainsHatHints"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Randomizer: a Stalchild spoken to under the Captain's Hat gives a hint."));
    AddWidget(path, "Gibdo Mask", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.GibdoMask"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Gibdo Mask, Gibdos and ReDeads take Link for one of their own and leave him alone."));
    AddWidget(path, "Majora's Mask Equipment", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMEquipment"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Razor and Gilded Swords, Great Fairy's Sword, Hero's Shield, Mirror Shield, Hero's Bow and "
            "Hookshot from Majora's Mask; young Link can use them all. Swords and shields: press C-Up on the pause "
            "menu's Equipment page for the Termina Equipment view and A to equip one (A on a Hyrule sword or shield "
            "switches back). Bow and Hookshot: press A on them in Select Item to switch between the variants you "
            "own."));
    AddWidget(path, "Timeless Equipment for Variants", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TimelessMMGear"))
        .Options(CheckboxOptions().Tooltip(
            "Hyrule's own swords and shields (Kokiri, Master, Biggoron, Hylian, Mirror) can be used at either age "
            "too, so young Link can switch to them as well."));
    AddWidget(path, "Unbreakable Razor Sword", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.UnbreakableRazorSword"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Razor Sword never wears out (it does after 100 hits in Majora's Mask; always unbreakable in the "
            "randomizer). On by default."));
    AddWidget(path, "Don't Auto-Equip New Swords and Shields", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.NoAutoEquipGear"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "A sword or shield you get (either game's, here or brought from Termina) is added to your equipment but "
            "not put on: equip it from the pause menu. Off: the games' own way (the Master Sword and Majora's Mask's "
            "swords and shields go on when you get them). On by default."));
    AddWidget(path, "All Majora's Mask Equipment", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_GENERAL("MergeShipAllMMEquipment"))
        .Options(CheckboxOptions().Tooltip(
            "On loading a file (not a randomizer file), and at once when turned on with one loaded, grants Majora's "
            "Mask's swords, shields, bow and hookshot."));
    AddWidget(path, "Majora's Mask Items", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMItems"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Majora's Mask's items (Powder Keg, Pictograph Box, bottle contents, deeds, letters, Moon's Tear...) "
            "work in Hyrule, on an \"Items (Termina)\" pause page laid out as in Majora's Mask."));
    AddWidget(path, "Hot Spring Water Cools", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMItems.HotWaterCools"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Hot Spring Water turns into Spring Water after Majora's Mask's 60 seconds."));
    AddWidget(path, "Powder Keg Carries Over", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMItems.KegCarriesOver"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "A Powder Keg the Goron is carrying comes along through loading zones, as in Majora's Mask."));
    AddWidget(path, "Pictograph Box", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMItems.Pictograph"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Pictograph Box takes and keeps a picture, as in Majora's Mask."));
    AddWidget(path, "Majora's Mask Songs", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMSongs"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Majora's Mask's songs can be learned and played in Hyrule with their Majora's Mask effects and "
            "fanfares; the Song of Soaring opens Termina's owl map."));
    AddWidget(path, "All Owl Statues Active", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMSongs.AllOwls"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The Song of Soaring can soar to every owl statue. Off, only statues Link has struck or whose owl statue "
            "item he has found, as in Majora's Mask."));
    AddWidget(path, "Termina Quest Page", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMSongs.QuestPage"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "C-Down on the Quest Status page shows Majora's Mask's remains, songs and Bomber's Notebook."));
    AddWidget(path, "Elegy of Emptiness Anywhere", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ElegyAnywhere"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The Elegy of Emptiness leaves the shell of Link's form wherever it is played, in Hyrule and in "
            "Termina, instead of only in Ikana (Majora's Mask: \"Your notes echoed far... but nothing happened.\"). "
            "The shells stand on switches and can be hookshotted, as in Majora's Mask."));
    AddWidget(path, "Stray Fairies", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.StrayFairies"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Majora's Mask's stray fairies can appear in Hyrule (free, in bubbles, held by enemies, in pots, crates "
            "and chests), are collected as in Majora's Mask and counted per temple on the Masks page. The Great "
            "Fairy's Mask senses them and draws free ones to Link. Turned off, fairies already out stop and vanish "
            "until it is on again. Turned on in a room, the pots, crates, chests and enemies already there get "
            "their fairies the next time the room is loaded."));
    AddWidget(path, "Chateau Romani from Cows", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.RomaniChateau"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Playing Epona's Song to a cow while wearing Romani's Mask, with an empty bottle, fills it with "
            "Chateau Romani instead of milk."));
    AddWidget(path, "Chateau Romani Magic", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ChateauMagic"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Drinking Chateau Romani makes magic free, as in Majora's Mask, until Link dies or the file is loaded "
            "again. Off, it only refills health and magic."));
    AddWidget(path, "ReDeads Dance", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.RedeadDance"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Wearing the Gibdo Mask, Captain's Hat or Garo's Mask, ReDeads dance instead of attacking, as Majora's "
            "Mask's ReDeads do."));
    AddWidget(path, "Mask of Truth Reads Gossip Stones", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TruthGossipStones"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Majora's Mask's Mask of Truth reads Gossip Stones like Hyrule's."));
    AddWidget(path, "Mask of Truth Extra Hint", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TruthExtraHint"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Randomizer: Majora's Mask's Mask of Truth reads Gossip Stones like Hyrule's, and each stone gives one "
            "extra hint."));
    // MergeShip2: the unified randomizer's hints and trackers over both worlds (MergeShipUnifiedHints.h,
    // MergeShipUnifiedTracker.h)
    AddWidget(path, "Mask of Truth Hints Point into Termina", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TruthTerminaHint"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Unified randomizer (Hyrule and Termina in one seed): half of the Gossip Stones give, as the Mask of "
            "Truth's extra hint, where one of Termina's major items is (\"... can be found at Clock Town\")."));
    AddWidget(path, "Check Tracker Shows Termina", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TrackTermina"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Unified randomizer: the Check Tracker lists Termina's checks after Hyrule's, by Termina's regions (Clock "
            "Town, Woodfall Temple ...). A check found in Termina shows as found here, with the item it held."));
    AddWidget(path, "Item Tracker: Both Worlds' Items", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ItemTrackerBothWorlds"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The Item Tracker's main window has two columns: Hyrule's items and, beside them, Termina's -- Majora's "
            "Mask's swords, shields and items, the remains, its songs and every mask (the transformation masks move "
            "there from Hyrule's column). Termina's own tracker shows Hyrule's items the same way."));
    AddWidget(path, "Termina Items Tracker", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ItemTrackerTermina"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Unified randomizer: with the Item Tracker open, a \"Termina Items Tracker\" window shows Termina's "
            "remains, masks, songs, each temple's keys, maps and stray fairies, and the rest of Termina's items, "
            "found in either world."));

    // Young Link
    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Young Link", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Majora's Mask Young Link", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMChildLink"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Young Link uses Majora's Mask's model, animations and voice, with Ocarina of Time's equipment."));
    AddWidget(path, "Young Link Wears Adult Gear", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMChildAdultGear"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "When Majora's Mask Young Link may use adult equipment (Timeless Equipment, Timeless Equipment for "
            "Variants), he holds the Master Sword or Biggoron's Sword he really has, in his bare fist and "
            "two-handed as in Ocarina of Time, carries the Master Sword's scabbard, and wears the Silver or Golden "
            "Gauntlets' plates, as in 2 Ship 2 Harkinian. Off, he shows Ocarina of Time's child models (the "
            "Kokiri Sword) as before."));
    AddWidget(path, "Young Link Uses the Mirror Shield", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ChildMirrorShield"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Young Link can equip Ocarina of Time's Mirror Shield on the equipment screen and use it as adult Link "
            "does, held in his hand and carried on his back (with Young Link Wears Adult Gear)."));
    AddWidget(path, "Young Link Wears Every Tunic", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ChildTunics"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Young Link can equip the Goron and Zora Tunics in Hyrule, as he does in Termina, with their protection "
            "from heat and water."));
    AddWidget(path, "Sprint (Hold A Out of a Roll)", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.Sprint")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Young Link and adult Link sprint when A is still held as a roll ends, for as long as A and the stick "
            "stay held: faster, leaning forward, arms pumping. Jumps keep the sprint's speed and it goes on after "
            "landing; a Bunny Hood adds its 1.5x on top. Hyrule and Termina share it."));
    AddWidget(path, "Sprint Speed", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gEnhancements.MergeShip.SprintSpeed")
        .Options(FloatSliderOptions().Min(1.1f).Max(2.0f).DefaultValue(1.5f).Format("%.2fx").Tooltip(
            "The sprint's speed, as a multiple of the run's."));
    AddWidget(path, "Young Link Rides Young Epona", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.YoungEpona"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Once young Link knows Epona's Song he can call Epona in Hyrule Field, Lake Hylia, Gerudo Valley and the "
            "Fortress, and ride her as Majora's Mask's young Epona (her model, size, sounds and mount). One Epona: "
            "she is young for young Link and Ocarina of Time's adult Epona for adult Link. In randomizer seeds he also needs an "
            "ocarina and the song's notes, as the adult does."));

    // Colours and online play
    AddWidget(path, "Colours & Anchor", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Form Tunic Colours", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FormTunicColors"))
        .Options(ComboboxOptions()
                     .ComboMap(sFormTunicColorsValues)
                     .DefaultIndex(2)
                     .Tooltip("Every green the Deku, Goron and Zora wear takes the Kokiri Tunic colour (Cosmetics "
                              "Editor, Link), and with \"All\" the Fierce Deity's tunic takes the Fierce Deity Tunic "
                              "colour. In Anchor, other players' forms wear their player colour. At the default "
                              "colours the forms look exactly as in Majora's Mask."));
    AddWidget(path, "Transformation Forms in Anchor", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.AnchorForms"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Anchor (Network menu) sends which form you are in, and shows other players transformed: their form's "
            "model, size and animations, the Goron ball, the Deku flower and the Zora barrier."));
    AddWidget(path, "One Anchor Connection for Both Worlds", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_ANCHOR_SHARED_CONNECTION)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In MergeShip2 (mergeship.exe), Hyrule and Termina share one Anchor connection: going through the door "
            "never leaves the room, you keep your place and your client, and the world you are not in keeps "
            "receiving its items and flags. Takes effect the next time Anchor is enabled. No effect in soh.exe."));
    AddWidget(path, "Receive Items From the Other World", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Anchor: an item a teammate finds in Termina reaches you in Hyrule (and the other way round), through "
            "the shared inventory both worlds keep. Items only one world can hold wait for you there."));
    AddWidget(path, "Anchor Text Chat", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_ANCHOR_CHAT)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "While connected to Anchor, press T to chat: a chat box opens in the bottom left and the game ignores "
            "your input until you press Enter to send (Enter on an empty line just closes it). Messages show in the "
            "chat box with each player's name in their Anchor colour, and above the sender's head for a few "
            "seconds. Hyrule and Termina share one conversation."));

    // Randomizer
    AddWidget(path, "Randomizer", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Masks Get Their Own Container Style", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MaskContainerStyle"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "With Containers Match Contents (Enhancements menu) on, a chest, pot, crate, bush or grass holding a "
            "mask takes 2 Ship 2 Harkinian's mask style (white, with a mask's face), and rocks, signs, icicles, red "
            "ice and wonder spots holding one sparkle maroon and white. An ice trap disguised as a mask looks the "
            "same. Off, masks look like any other major item."));
    AddWidget(path, "Stray Fairies Get Their Own Container Style", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.StrayFairyContainerStyle"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "With Containers Match Contents (Enhancements menu) on, a chest, pot, crate, tree, bush or grass holding "
            "one of Majora's Mask's stray fairies takes 2 Ship 2 Harkinian's stray fairy style, and rocks, signs, "
            "icicles, red ice and wonder spots holding one sparkle pink. An ice trap disguised as a stray fairy looks "
            "the same. Off, stray fairies look like lesser items."));

    // Termina: the door between the two engines (mergeship.exe only; MergeShipHost.h)
    AddWidget(path, "Termina", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Lost Woods Lead to Termina", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.TerminaDoor"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In MergeShip2 (mergeship.exe, with Majora's Mask loaded), taking a wrong turn in the Lost Woods leads "
            "to Termina instead of back to Kokiri Forest, as Link got lost there in Majora's Mask. Each file has "
            "its own Majora's Mask file: the first visit plays Majora's Mask's opening, later ones arrive in South "
            "Clock Town from the Clock Tower. The way back is the passage behind the Clock Tower's stone doors. "
            "What Link carries goes with him. Has no effect in soh.exe."));
    AddWidget(path, "Share Items During Termina's First Cycle", WIDGET_CVAR_CHECKBOX)
        .CVar(MS_CVAR_SHARE_FIRST_CYCLE)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Link arrives in Termina with everything he has in Hyrule, even during Majora's Mask's first cycle "
            "(before its first Song of Time), and what he gets there comes back with him: every item, song and mask. "
            "Majora's Mask's opening still plays. Off, nothing is shared until the first cycle is over. Termina's Mergeship menu has the same option; either one "
            "turns it off. Has no effect in soh.exe."));
    // Settings sharing (include/mergeship/SettingsSync.h MS_CVAR_SHARE_SETTINGS; soh/MergeShip/MergeShipSettingsSync.h)
    AddWidget(path, "Share Settings with Termina", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.ShareSettings"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "In MergeShip2 (mergeship.exe), the options set here apply in Majora's Mask too, each time Link goes to "
            "Termina: frame rate, V-Sync, anti-aliasing, resolution and aspect ratio, volumes, D-pad items, "
            "assignable boots (as D-pad boots), D-pad and right-stick ocarina, aiming and camera options, the "
            "cutscene skips and other time savers, difficulty options, cheats, Link's and the HUD's colours, and "
            "these Mergeship options. One changed in Termina's menu changes here too. Options Majora's Mask has no "
            "counterpart for stay its own. Has no effect in soh.exe."));
    // MergeShip2 (soh_fd): the forms' rules, the Fierce Deity's options and Majora's Mask's moves, carried over from
    // the soh_fd fork. MergeShipSohFd.c reads each CVar with the default given here.
    path.sidebarName = "Forms & Fierce Deity";
    AddSidebarEntry("Mergeship", path.sidebarName, 2);
    path.column = SECTION_COLUMN_1;

    AddWidget(path, "Form Rules", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Forms Wear Trade Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FormsWearTradeMasks"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Lets your transformed forms wear the child trade masks (Bunny Hood, Mask of Truth, and so on) "
            "without turning back or having them unequipped."));
    AddWidget(path, "Stuck Safeguards (Navi turn-back prompt)", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.StuckSafeguards"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Has Navi offer to turn the Fierce Deity back into Link wherever he could get stuck in the water, like "
            "the Water Temple, so you can't soft-lock."));
    AddWidget(path, "Scene-Start Safeguards", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.SceneStartSafeguards"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "A Deku or Goron that would start in (or be handed control back by a cutscene in) water it can only "
            "sink in, or the Deku on a burning floor, turns back into Link on the spot -- for example in Zora's "
            "Fountain after Barinade's blue warp."));
    AddWidget(path, "Prevent Restricted Actions (\"current form!\")", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.PreventRestrictedActions"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Stops forms from pulling the Master Sword or riding Epona. Navi says you can't do that in your "
            "current form."));
    AddWidget(path, "Dogs React to Deku Link", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.DogsReactDeku"))
        .Options(ComboboxOptions()
                     .ComboMap(sDogReactionValues)
                     .DefaultIndex(2)
                     .Tooltip("Majora's Mask behaviour: dogs dislike the Deku Scrub. Once you move, they chase "
                              "you down barking and lunge to knock you over, backing off to line up first if you "
                              "face them.\n\n"
                              "All But Richard exempts the lost white dog in the night-time Market, so returning "
                              "him to his owner still works while you are wearing the mask.\n\n"
                              "Only Richard flips it around: every other dog ignores you, and that one dog "
                              "holds a personal grudge."));
    AddWidget(path, "Dogs React to Goron Link", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.DogsReactGoron"))
        .Options(ComboboxOptions()
                     .ComboMap(sDogReactionValues)
                     .DefaultIndex(1)
                     .Tooltip("Majora's Mask behaviour: dogs are afraid of the Goron. They back away groaning, "
                              "then turn and run whimpering, barking at you from a distance."));
    AddWidget(path, "Dogs React to Zora Link", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.DogsReactZora"))
        .Options(ComboboxOptions()
                     .ComboMap(sDogReactionValues)
                     .DefaultIndex(1)
                     .Tooltip("Majora's Mask behaviour: dogs love the Zora. They run up to you with a bark and "
                              "sit beside you, whining affectionately."));

    AddWidget(path, "Fierce Deity", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "FD Can Play Ocarina", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdCanPlayOcarina"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Lets Fierce Deity play the ocarina instead of graying it out."));
    AddWidget(path, "FD Can Use Transformation Masks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdCanUseMasks"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Lets Fierce Deity put on the Deku, Goron and Zora masks directly, as soh_fd does. Off, he takes his own "
            "mask off first, as in Majora's Mask."));
    AddWidget(path, "FD Can Sheathe Sword", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdCanSheathe"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Lets Fierce Deity put his sword away, which he normally keeps drawn."));
    AddWidget(path, "FD Beams Break Rocks", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdBeamsBreakRocks"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Lets Fierce Deity's sword beams shatter bomb-breakable rocks."));
    AddWidget(path, "FD Beams Light Fire", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdBeamsLightFire"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Lets Fierce Deity's sword beams light torches and burn webs. Also makes the beams hit as hard "
            "as fire arrows."));
    AddWidget(path, "FD Magic Spin", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdMagicSpin"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Gives Fierce Deity's spin attack its glowing energy disk. When off, his spin is a plain "
            "magicless sweep, matching Majora's Mask."));
    AddWidget(path, "FD Increased Strength", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdIncreasedStrength"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Gives Fierce Deity the lifting strength of the Gold Gauntlets, so he can pick up the heaviest "
            "objects."));
    AddWidget(path, "Unrestrict Items for FD", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FdUnrestrictItems"))
        .Options(ComboboxOptions()
                     .ComboMap(sFdUnrestrictItemsValues)
                     .DefaultIndex(0)
                     .Tooltip("Lets Fierce Deity use items he'd normally have grayed out. \"Deku Nuts, Bombs & "
                              "Spells\" allows just those. \"All\" un-grays every Ocarina of Time item except swords "
                              "and shields, like Timeless Equipment for the deity form; Majora's Mask's masks and "
                              "items keep their own rules. Off keeps his normal item restrictions."));

    path.column = SECTION_COLUMN_2;
    AddWidget(path, "Majora's Mask Moves", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "MM Jump Flips", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMFlips"))
        .Options(ComboboxOptions()
                     .ComboMap(sMMFlipsValues)
                     .DefaultIndex(0)
                     .Tooltip("Running off a ledge, Young Link, Adult Link and the Fierce Deity randomly do one of "
                              "Majora's Mask's three jumps: the front flip, the side flip or the running jump. The "
                              "Deku, Goron and Zora never flip.\n\n"
                              "Majora's Mask: the jump is Majora's Mask's, and as there no jump attack or ledge "
                              "grab can come out of a flip.\n\n"
                              "Cosmetic: only the animation changes; the jump, its jump attack and its ledge grab "
                              "stay Ocarina of Time's."));
    AddWidget(path, "MM Ledge Momentum", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMLedgeMomentum"))
        .Options(ComboboxOptions()
                     .ComboMap(sMMLedgeMomentumValues)
                     .DefaultIndex(2)
                     .Tooltip("Gives Majora's Mask's airborne feel: instead of instantly capping your horizontal "
                              "speed the moment you leave the ground, above-cap momentum from running off a ledge "
                              "or a running jump is carried and bled off gradually.\n\n"
                              "All: Young Link, Adult Link and every form. Forms Only: the Deku, Goron, Zora and "
                              "Fierce Deity. Off keeps Ocarina of Time's hard speed cap in the air."));
    AddWidget(path, "MM Roll Attack Damage", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.MMRollDamage"))
        .Options(ComboboxOptions()
                     .ComboMap(sMMRollDamageValues)
                     .DefaultIndex(2)
                     .Tooltip("Lets the chosen player(s) deal light contact damage while rolling, like in "
                              "Majora's Mask (the roll's body-check hits for a small amount mid-roll). Off keeps "
                              "Ocarina of Time's damage-less roll."));

    AddWidget(path, "Masks, Sounds & Icons", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Forms on the Equipment Screen", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FormPauseModel"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The pause menu's equipment screen shows the form you are in -- the Deku, Goron, Zora or Fierce Deity -- "
            "instead of Link."));
    AddWidget(path, "Bunny Hood Fit", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.BunnyHoodFit"))
        .Options(ComboboxOptions()
                     .ComboMap(sBunnyHoodFitValues)
                     .DefaultIndex(3)
                     .Tooltip("The Bunny Hood is made for Young Link's head, so on the taller Adult and Fierce "
                              "Deity heads it sinks down into the head. This lifts it back up for the chosen "
                              "form(s). Only the Bunny Hood is affected -- the other trade masks already sit "
                              "correctly on every form and are left untouched, and Young Link is never changed. "
                              "Off = vanilla placement."));
    AddWidget(path, "Get-a-Mask Fanfare", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.GetMaskFanfare"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Getting a transformation mask plays Majora's Mask's \"Get a Mask\" fanfare."));
    AddWidget(path, "Adult Link Transformation Scream", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.AdultTransformScream"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Adult Link screams with his own voice when a mask transforms him, instead of Majora's Mask's young "
            "Link. Needs mergeship2.o2r."));
    AddWidget(path, "Form Instrument Icons", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("MergeShip.FormInstrumentIcons"))
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "While a Deku, Goron or Zora, the ocarina's item icon shows the instrument that form plays instead: "
            "the Deku Pipes, the Goron Drums or the Zora Guitar. Needs mergeship2.o2r."));

    // MergeShip2: singleplayer mode (docs/SINGLEPLAYER.md, soh/MergeShip/MergeShipSingleplayer.h). Never on a
    // randomizer file: locked while one is loaded, and every part of it checks for a non-randomizer file too.
    path.sidebarName = "Singleplayer";
    AddSidebarEntry("Mergeship", path.sidebarName, 2);
    path.column = SECTION_COLUMN_1;
    static auto lockedInRando = [](WidgetInfo& info) {
        if (IS_RANDO) {
            info.options->disabled = true;
            info.options->disabledTooltip = "Singleplayer mode is not available on randomizer files.";
        }
    };
    AddWidget(path, "Singleplayer", WIDGET_SEPARATOR_TEXT);
    AddWidget(path, "Singleplayer Mode", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.SPMode")
        .PreFunc(lockedInRando)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "MergeShip's singleplayer story (normal files only, never randomizer files). The Happy Mask Salesman "
            "travels Hyrule -- Castle Town, Kakariko, Death Mountain, Zora's River, Zora's Domain, Hyrule Field -- "
            "selling Majora's Mask's masks, three at a time; his shop is closed while he is out. More of the mode "
            "(the post-game road to Termina) comes in later updates."));
    AddWidget(path, "Fierce Deity's Mask Also From the Moon", WIDGET_CVAR_CHECKBOX)
        .CVar("gEnhancements.MergeShip.SPFierceDeityFromMoon")
        .PreFunc(lockedInRando)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Off: the Fierce Deity's Mask comes only from the Happy Mask Salesman, once every mask he sells is "
            "bought. On: Majora's Mask's moon child gives it too."));

#ifdef MERGESHIP_HOSTED
    // MergeShip2 (BUG-0096): Termina's own settings, set from here (include/mergeship/MenuMirror.hpp). The ones both
    // worlds share are not repeated: they are this menu's own.
    path.sidebarName = "Termina Settings";
    AddSidebarEntry("Mergeship", path.sidebarName, 1);
    path.column = SECTION_COLUMN_1;
    AddWidget(path, "Termina Settings##MergeShipMirror", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        MsMenuMirror_Draw(MergeShip_OtherMenuMirror(), MergeShip_OtherConsoleVariables(), true, "Termina");
    });
#endif
}

#ifdef MERGESHIP_HOSTED
// MergeShip2 (BUG-0096): this menu as plain data for Termina's menu ("Hyrule Settings"): every CVar checkbox, slider
// and list, with the separators' titles; buttons, windows and custom widgets are left out (they act in Hyrule).
const MsMenuMirror* MergeShip_BuildMenuMirror() {
    static MsMenuMirrorBuilder sMirror;
    static const MsMenuMirror* sBuilt = nullptr;

    if ((sBuilt != nullptr) || (mSohMenu == nullptr)) {
        return sBuilt;
    }
    const auto& entries = mSohMenu->GetMenuEntries();
    for (const std::string& menuName : mSohMenu->GetMenuOrder()) {
        auto menuIt = entries.find(menuName);
        if ((menuIt == entries.end()) || (menuName == "Dev Tools")) {
            continue;
        }
        const MainMenuEntry& menu = menuIt->second;
        for (const std::string& sidebarName : menu.sidebarOrder) {
            auto sidebarIt = menu.sidebars.find(sidebarName);
            if (sidebarIt == menu.sidebars.end()) {
                continue;
            }
            for (const auto& column : sidebarIt->second.columnWidgets) {
                for (const WidgetInfo& widget : column) {
                    const std::string tooltip = (widget.options != nullptr) ? widget.options->tooltip : "";
                    if (widget.type == WIDGET_SEPARATOR_TEXT) {
                        sMirror.Add(menu.label, sidebarName, widget.name, nullptr, "", MS_MIRROR_HEADER);
                        continue;
                    }
                    if ((widget.cVar == nullptr) || (widget.options == nullptr)) {
                        continue;
                    }
                    switch (widget.type) {
                        case WIDGET_CVAR_CHECKBOX: {
                            auto* options = static_cast<CheckboxOptions*>(widget.options.get());
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_CHECKBOX);
                            item.def = options->defaultValue ? 1.0f : 0.0f;
                            break;
                        }
                        case WIDGET_CVAR_SLIDER_INT: {
                            auto* options = static_cast<IntSliderOptions*>(widget.options.get());
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_INT);
                            item.min = (float)options->min;
                            item.max = (float)options->max;
                            item.def = (float)options->defaultValue;
                            item.format = sMirror.Keep(options->format);
                            break;
                        }
                        case WIDGET_CVAR_SLIDER_FLOAT: {
                            auto* options = static_cast<FloatSliderOptions*>(widget.options.get());
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_FLOAT);
                            item.min = options->min;
                            item.max = options->max;
                            item.def = options->defaultValue;
                            item.percent = options->isPercentage;
                            item.format = sMirror.Keep(options->format);
                            break;
                        }
                        case WIDGET_CVAR_COMBOBOX: {
                            auto* options = static_cast<ComboboxOptions*>(widget.options.get());
                            std::vector<std::pair<int32_t, std::string>> list;
                            for (const auto& [value, label] : options->comboMap) {
                                list.emplace_back(value, (label != nullptr) ? label : "");
                            }
                            MsMenuMirrorItem& item =
                                sMirror.Add(menu.label, sidebarName, widget.name, widget.cVar, tooltip, MS_MIRROR_COMBO);
                            item.def = (float)options->defaultIndex;
                            sMirror.SetOptions(item, list);
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
        }
    }
    sBuilt = sMirror.Finish();
    return sBuilt;
}
#endif

} // namespace SohGui
