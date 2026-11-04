// MergeShip2 -- scripted in-game test harness.
//
// Booting the game and reading a log line proves only that the player was *built* as a form. It
// says nothing about whether a Goron can roll or a Deku can hop. This harness drives the real game
// with scripted controller input and records what the player actually did, frame by frame, so a
// transformation ability can be shown to work rather than assumed to.
//
// It is inert unless MERGESHIP_TEST_SCRIPT names a script file, so nothing it does reaches a normal
// session or the user's saved configuration.
//
// Script: one command per line, '#' starts a comment.
//
//   boot <entrance> [adult|child]   start a debug save at an entrance (hex or decimal)
//   waitplay [frames]               wait until gameplay is running, then N more frames
//   wait <frames>                   neutral input for N frames
//   hold <buttons> <frames>         hold buttons, e.g. "hold A+B 10"
//   stick <x> <y> <frames> [buttons] hold the stick (and optional buttons)
//   press <buttons>                 one frame down, one frame up
//   form <n>                        debug: become PlayerForm n at once, without the mask
//   givemask <mmForm>               own a transformation mask (0 FD, 1 Goron, 2 Zora, 3 Deku)
//   infront <actorId> [dist] [n]    stand dist (default 120) in front of the n-th (default 0) actor with that id,
//                                   facing it; logs every actor of that id with its position and action state
//   actors <actorId>                log every actor of that id: position, yaw, distance to the player
//   givemmmask <mmMask|all>         own one of MM's other masks (MMMask: 1 Truth .. 20 Giant), or all of them
//   give <item>                     Item_Give
//   rgive <rg>                      hold up a randomizer item (GiveItemEntryWithoutActor); rg is a number, an RG_
//                                   name (RG_MM_MAGIC_JAR_BIG) or mmdeku / mmgoron / mmzora / mmfd / goronmask / ...
//   rspawn <rg> <x> <y> <z>         a freestanding randomizer item (EnItem00 drawn from its GetItemEntry)
//   rrow <dist> <rg...>             freestanding randomizer items in a row dist in front of the player, 45 apart
//   ritemdraw [rg...]               log how each item's get-item model is drawn (its draw function, its Termina
//                                   row); no argument: every Majora's Mask item, FALLBACK for any with no model
//   randofile                       after randogen: the debug file plays that seed (IS_RANDO), for this run only
//   rplace <rc> <rg>                after randogen: check rc (RC_ name or number) holds rg, not found yet
//   rdrop <rc> [dist]               check rc's item dropped dist in front of the player (as a pot / grass / beehive
//                                   drops it: EnItem00 ITEM00_SOH_DUMMY with the check); a stray fairy flies free
//   masks                           log the owned transformation masks, the child trade slot, MM's other masks and
//                                   equipment owned, and the D-pad items
//   giveall                         the whole of Hyrule: every item at its best, every song, medallion, stone, the
//                                   Stone of Agony, Gerudo Card, 100 tokens, all equipment and upgrades, every mask the
//                                   Happy Mask Shop lends, every trade item (randomizer trade flags), MM's songs; 20
//                                   hearts, double magic and defence, 500 rupees (masks: givemask / givemmmask)
//   strip                           the debug file owns nothing: no item, ocarina, sword, shield, upgrade, quest item,
//                                   mask, MM equipment or magic meter (then give what a test needs)
//   gear                            log the ocarina / bow / hookshot slots, swords and shields, MM equipment, magic
//   shared                          capture into the file's shared save (as the door does) and log it
//   randogen <seed>                 generate a randomizer seed from the rando CVars (set them with cvar) and log
//   randoterm                       after randogen: the unified randomizer's summary of that seed (MergeShipTerminaRando)
//                                   where the transformation masks and their paired items were placed
//   randogoal                       after randogen: the unified goal of that seed (MergeShipGoal.h): settings, where the
//                                   Triforce and Triforce pieces are, advancement, the playthrough's Triforce sphere
//   goal [state|ganon|majora|pieces <n>|check|capture|clear]
//                                   the unified goal in game: Ganon falls (VB_SLAY_GANON), Majora falls (what 2Ship
//                                   writes), set Hyrule's Triforce pieces, run CheckTriggers; each logs the goal state
//   cbutton <slot> <item>           put an item on C-left(1)/C-down(2)/C-right(3)
//   pos <x> <y> <z> [yaw]           place the player
//   warp <entrance>                 change scene (same age); follow with waitplay
//   health <n> | magic <n>          set health (sixteenths) or magic
//   trace on|off                    per-frame player state
//   cvar <name> <int>               set an integer CVar for this run (never saved: see quit)
//   cvarf <name> <float>            set a float CVar for this run
//   cvarlog <name...>               log CVars as the game reads them (integer and float)
//   cvarf <name> <float>            set a float CVar for this run
//   time <dayTime>                  set the time of day (0x4000 06:00, 0xC000 18:00)
//   spawn <id> <x> <y> <z> [params] [yaw]   spawn an actor
//   spawnrel <id> <dist> [params]   spawn an actor dist in front of the player
//   cvarcolor <name> <r> <g> <b>    set a colour CVar (e.g. gCosmetics.Link.KokiriTunic.Value)
//   anchorloop <r> <g> <b> [dx dz]  Anchor: mirror the player as another Anchor player in colour r,g,b
//   anchor <command ...>            Anchor: enable/disable/state/loop/give/send (MergeShipAnchorHost.cpp)
//   equip <type> <value> [bItem]    OoT equipment: type 0 sword, 1 shield, 2 tunic, 3 boots; bItem sets B
//   exits [n]                       log the scene's exit polygons (exit n, or every exit), grouped into openings
//   walkexit <n> <frames> [k]       walk (steering the stick) into the k-th nearest opening of exit n
//   toexit <n> [dist] [k]           stand dist (default 150) in front of the k-th nearest opening of exit n
//   walkto <x> <z> <frames>         walk (steering the stick) towards a point
//   route <x1> <z1> [<x2> <z2> ...] <frames> [buttons] [key=value ...]
//                                   follow the path from where the player stands through each point, steering the stick
//                                   as walkto does at a point `look` ahead along the path, holding the buttons all the way
//                                   (A as a Goron: the roll; A as the Zora in the water: the fast swim); blocks like
//                                   "wait" until within `arrive` of the last point or the frames run out, then lets the
//                                   stick go and keeps the buttons until the next command sets its own. Keys: look=<units>
//                                   (60), arrive=<units> (30), mag=<0-100> stick tilt (100), magic=1 (meter kept full)
//   moveto <x> <z> <frames> [buttons] [arriveDist]
//                                   a route to one point
//   swimto <x1> <z1> [...] <frames> [buttons] [key=value ...]
//                                   a route for the Zora's fast swim, where stick X turns and stick Y pitches: holds
//                                   depth=<units> (60) below the surface, and with leap=<frames> dives to
//                                   leapdepth=<units> (140) and pulls up into a dolphin leap every so many frames of
//                                   cruising; look defaults to 120
//   autofight <frames> [style] [maxDist]
//                                   the fight autopilot, for filming; blocks like "wait". Every frame: the nearest living
//                                   enemy or boss in the room within maxDist (default 1200; kept until it dies or leaves
//                                   range), locked on with Z (Hold targeting while it runs), steered to with the stick as
//                                   walkto steers, fought with the style's moves -- auto (by form), sword (B combos, stabs,
//                                   Z+A jump attack, spin attack, sidehops and backflips when hit or charged), fd (the same,
//                                   and beams from 200-700 away), goron (punches, ground pound, a roll in from range), zora
//                                   (punches, jump kick, fins from range), deku (spin, bubble from range). Ranged moves
//                                   only with a clear line to a target locked onto or faced; melee only at the target's
//                                   height; a blocked approach detours round, and an unreachable target is set aside.
//                                   Never steps, rolls or dodges off a ledge or into lava, a void, a loading zone or
//                                   deep water. Health and magic stay full. Logs AUTOFIGHT lines: each target (id,
//                                   distance), each move, hits taken, idling
//   autotarget [maxDist]            log every enemy and boss loaded, nearest first: id, position, distance, health, and
//                                   whether autofight would fight it (or why not)
//   strayfairy [area] [dist] | log | touch
//                                   a free MM stray fairy in front of the player / log every stray fairy / step onto
//                                   the first one
//   mark <text>                     a marker in the log
//   shot <name>                     screenshot of the game window, <name>.bmp
//   quit                            flush and exit without touching the saved configuration
//   randoer | ertable | ertake <index|cross [dungeon|boss|grotto]>
//                                   entrance shuffle over both worlds (M8, MergeShipEntrances.cpp): after randogen, the
//                                   Termina entrances and cross-world overrides; the runtime table; take an entrance
//   unified test [seed] | off       the unified randomizer's ledger for the debug file (M3, MergeShipUnified.h)
//   ledger                          log the shared save's ledger (every check found in either world)
//   ledgeradd <hyrule|termina> <item> [check] [flags]   an entry as the other engine would write it
//   hyrulefind <item> <check>       <check> holds <item> (MsItems.def key or RG_ name) and is found
//   project                         give now what Termina found for Hyrule; then unifiedstate
//   unifiedstate                    log what Hyrule holds of the items that cross
//   hints                           after randogen: every hint of the seed, the world it points into, and the Way of
//                                   the Hero / foolish areas of both worlds (M6, MergeShipUnifiedHints.h)
//   hintarea <RC_MM_...>            a Termina check's hint area and an item hint's text about it
//   hintitem <RG_...>               the name an item hint gives an item (" (MM)" for Majora's Mask's)
//   truthhint [n] | truthcandidates the Mask of Truth's extra hint of the first n stones; its Termina candidates
//   trackterm | trackitems          the check tracker's Termina section / the Termina items window, from the ledger
//   catalog [key...]                the Hyrule catalog 2Ship gets with the next trip (M7, MergeShipUnifiedTracker.h);
//                                   with keys (MsItems.def), where the seed placed each of them
//
// Buttons: A B Z R L S CU CD CL CR DU DD DL DR, joined with '+'.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32 // MergeShip2 Android: Windows only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#else
#include <strings.h>
#include <unistd.h>
#define _stricmp strcasecmp
#endif

#include <spdlog/spdlog.h>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Enhancements/randomizer/item_category_adj.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/MergeShip/MergeShipTerminaGiDraw.h"
#include "soh/Enhancements/randomizer/3drando/menu.hpp"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipEntrances.h"
#include "soh/MergeShip/MergeShipGoal.h"
#include "soh/MergeShip/MergeShipUnified.h"
#include "soh/MergeShip/MergeShipUnifiedHints.h"
#include "soh/MergeShip/MergeShipUnifiedTracker.h"
#include "soh/MergeShip/MergeShipSharedSave.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <mergeship/SharedSave.h>
#include <mergeship/CineCam.hpp> // the film camera ("cinecam")

extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "z64.h"
#include "soh/MergeShip/MergeShipForms.h"
s32 Object_Spawn(ObjectContext* objectCtx, s16 objectId); // z_scene.c (loadobj)
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/MergeShip/MergeShipSongs.h"
#include "soh/MergeShip/MergeShipTermina.h" // owlmap (BUG-0128)
#include "soh/MergeShip/MergeShipStrayFairy.h"
void Sram_InitDebugSave(void);
#include "soh/MergeShip/MergeShipDekuBeans.h" // beans
#include "soh/MergeShip/MergeShipOwnership.h" // ownbit
#include "src/overlays/actors/ovl_MM_En_Osn_Travel/z_mm_en_osn_travel.h" // spnear
#include "soh/MergeShip/MergeShipSingleplayer.h" // spclock, spwhere
#include "src/overlays/actors/ovl_En_Horse/z_en_horse.h" // horse
#include "soh/MergeShip/MergeShipEpona.h"
#include "src/overlays/actors/ovl_MM_En_Torch2/z_mm_en_torch2.h" // spawnelegy (BUG-0144)
extern u8 gSlotAgeReqs[]; // z_kaleido_scope_PAL.c (beans: the pause page's age rule)
void MergeShipAnchor_TestLoopback(u8 r, u8 g, u8 b, f32 dx, f32 dz);
void MergeShipAnchor_TestCommand(const char* args);
extern PlayState* gPlayState;
// The MM form actions route / swimto steer by (forms/MergeShipZora.inc, forms/MergeShipGoron.inc, in z_player.c).
void MergeShip_Zora_Action_28(Player* player, PlayState* play);  // MM Player_Action_28: the breach (dolphin leap)
void MergeShip_Zora_Action_56(Player* player, PlayState* play);  // MM Player_Action_56: the fast swim
void MergeShip_Goron_Action_96(Player* player, PlayState* play); // MM Player_Action_96: the roll
}
extern void EnItem00_DrawRandomizedItem(EnItem00* enItem00, PlayState* play); // randomizer/hook_handlers.cpp

namespace {

struct Command {
    std::string op;
    std::vector<std::string> args;
    int line;
};

struct Harness {
    bool active = false;
    std::vector<Command> commands;
    size_t pc = 0;
    int waitFrames = 0;       // frames left on the current timed command
    bool waitingForPlay = false;
    int afterPlayFrames = 0;
    u16 buttons = 0;
    u16 latched = 0; // "latch": held on every frame whatever the other commands do, until "unlatch"
    s8 stickX = 0;
    s8 stickY = 0;
    bool pressRelease = false; // second frame of a "press"
    bool trace = false;
    u64 frame = 0;
    FILE* log = nullptr;
    std::string dir;
    bool booted = false;
    int steerFrames = 0; // walkto / walkexit: frames left
    Vec3f steerTarget = {};
    s32 bootEntrance = -1;
    s32 bootAge = LINK_AGE_ADULT;
    bool symbolsReady = false;
};

Harness sH;
// The player's master volume (gSettings.Volume.Master, an integer 0..100), muted while a script runs ("release" restores
// it; a host-muted run started at 0, so then full volume).
s32 sPrevMasterVolume = 100;

void Log(const char* fmt, ...) {
    if (sH.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sH.log, "[%06llu] ", (unsigned long long)sH.frame);
    vfprintf(sH.log, fmt, ap);
    fputc('\n', sH.log);
    va_end(ap);
    fflush(sH.log);
}

std::string SymbolName(void* addr) {
#ifdef _WIN32
    if (addr == nullptr) {
        return "NULL";
    }
    if (!sH.symbolsReady) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        sH.symbolsReady = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    }
    char buf[sizeof(SYMBOL_INFO) + 256] = {};
    SYMBOL_INFO* info = (SYMBOL_INFO*)buf;
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (sH.symbolsReady && SymFromAddr(GetCurrentProcess(), (DWORD64)addr, &disp, info)) {
        return info->Name;
    }
    char hex[32];
    snprintf(hex, sizeof(hex), "%p", addr);
    return hex;
#else
    if (addr == nullptr) {
        return "NULL";
    }
    char hex[32];
    snprintf(hex, sizeof(hex), "%p", addr);
    return hex;
#endif
}

u16 ParseButtons(const std::string& s) {
    static const struct {
        const char* name;
        u16 bit;
    } kButtons[] = {
        { "A", BTN_A },   { "B", BTN_B },   { "Z", BTN_Z },   { "R", BTN_R },    { "L", BTN_L },
        { "S", BTN_START }, { "CU", BTN_CUP }, { "CD", BTN_CDOWN }, { "CL", BTN_CLEFT }, { "CR", BTN_CRIGHT },
        { "DU", BTN_DUP }, { "DD", BTN_DDOWN }, { "DL", BTN_DLEFT }, { "DR", BTN_DRIGHT },
    };
    u16 mask = 0;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        bool found = false;
        for (auto& b : kButtons) {
            if (_stricmp(tok.c_str(), b.name) == 0) {
                mask |= b.bit;
                found = true;
            }
        }
        if (!found && !tok.empty() && tok != "-") {
            Log("WARN unknown button '%s'", tok.c_str());
        }
    }
    return mask;
}

long ParseInt(const std::string& s) {
    return strtol(s.c_str(), nullptr, 0);
}

RandomizerGet ParseRandoGet(const std::string& s) {
    static const struct {
        const char* name;
        RandomizerGet rg;
    } kNames[] = {
        { "mmdeku", RG_MM_DEKU_MASK },   { "mmgoron", RG_MM_GORON_MASK }, { "mmzora", RG_MM_ZORA_MASK },
        { "mmfd", RG_MM_FIERCE_DEITY_MASK }, { "goronmask", RG_GORON_MASK }, { "zoramask", RG_ZORA_MASK },
        // Majora's Mask's other 20 masks
        { "mmtruth", RG_MM_MASK_OF_TRUTH },       { "mmkafei", RG_MM_KAFEIS_MASK },
        { "mmallnight", RG_MM_ALL_NIGHT_MASK },   { "mmbunny", RG_MM_BUNNY_HOOD },
        { "mmkeaton", RG_MM_KEATON_MASK },        { "mmgaro", RG_MM_GAROS_MASK },
        { "mmromani", RG_MM_ROMANIS_MASK },       { "mmcircus", RG_MM_CIRCUS_LEADERS_MASK },
        { "mmpostman", RG_MM_POSTMANS_HAT },      { "mmcouple", RG_MM_COUPLES_MASK },
        { "mmgreatfairy", RG_MM_GREAT_FAIRYS_MASK }, { "mmgibdo", RG_MM_GIBDO_MASK },
        { "mmdongero", RG_MM_DON_GEROS_MASK },    { "mmkamaro", RG_MM_KAMAROS_MASK },
        { "mmcaptain", RG_MM_CAPTAINS_HAT },      { "mmstone", RG_MM_STONE_MASK },
        { "mmbremen", RG_MM_BREMEN_MASK },        { "mmblast", RG_MM_BLAST_MASK },
        { "mmscents", RG_MM_MASK_OF_SCENTS },     { "mmgiant", RG_MM_GIANTS_MASK },
        // Majora's Mask's equipment, items, songs, quest items and stray fairies (MergeShipRandoItems.cpp)
        { "mmsword", RG_PROGRESSIVE_MM_SWORD }, { "mmgfsword", RG_MM_GREAT_FAIRYS_SWORD },
        { "mmheroshield", RG_MM_HEROS_SHIELD }, { "mmmirror", RG_MM_MIRROR_SHIELD },
        { "mmbow", RG_MM_HEROS_BOW }, { "mmhookshot", RG_MM_HOOKSHOT },
        { "mmkeg", RG_MM_POWDER_KEG }, { "mmpicto", RG_MM_PICTOGRAPH_BOX },
        { "mmprincess", RG_MM_BOTTLE_WITH_DEKU_PRINCESS }, { "mmzoraegg", RG_MM_BOTTLE_WITH_ZORA_EGG },
        { "mmgolddust", RG_MM_BOTTLE_WITH_GOLD_DUST }, { "mmmushroom", RG_MM_BOTTLE_WITH_MUSHROOM },
        { "mmseahorse", RG_MM_BOTTLE_WITH_SEAHORSE }, { "mmloach", RG_MM_BOTTLE_WITH_HYLIAN_LOACH },
        { "mmspringwater", RG_MM_BOTTLE_WITH_SPRING_WATER }, { "mmhotwater", RG_MM_BOTTLE_WITH_HOT_SPRING_WATER },
        { "mmchateau", RG_MM_BOTTLE_WITH_CHATEAU_ROMANI }, { "mmmoonstear", RG_MM_MOONS_TEAR },
        { "mmlanddeed", RG_MM_LAND_TITLE_DEED }, { "mmswampdeed", RG_MM_SWAMP_TITLE_DEED },
        { "mmmountaindeed", RG_MM_MOUNTAIN_TITLE_DEED }, { "mmoceandeed", RG_MM_OCEAN_TITLE_DEED },
        { "mmroomkey", RG_MM_ROOM_KEY }, { "mmlettermama", RG_MM_LETTER_TO_MAMA },
        { "mmletterkafei", RG_MM_LETTER_TO_KAFEI }, { "mmpendant", RG_MM_PENDANT_OF_MEMORIES },
        { "mmsonata", RG_MM_SONATA_OF_AWAKENING }, { "mmlullaby", RG_PROGRESSIVE_GORON_LULLABY },
        { "mmnewwave", RG_MM_NEW_WAVE_BOSSA_NOVA }, { "mmelegy", RG_MM_ELEGY_OF_EMPTINESS },
        { "mmoath", RG_MM_OATH_TO_ORDER }, { "mmhealing", RG_MM_SONG_OF_HEALING },
        { "mmsoaring", RG_MM_SONG_OF_SOARING }, { "mmodolwa", RG_MM_ODOLWAS_REMAINS },
        { "mmgoht", RG_MM_GOHTS_REMAINS }, { "mmgyorg", RG_MM_GYORGS_REMAINS },
        { "mmtwinmold", RG_MM_TWINMOLDS_REMAINS }, { "mmnotebook", RG_MM_BOMBERS_NOTEBOOK },
        { "mmfairyct", RG_MM_CLOCK_TOWN_STRAY_FAIRY }, { "mmfairywf", RG_MM_WOODFALL_STRAY_FAIRY },
        { "mmfairysh", RG_MM_SNOWHEAD_STRAY_FAIRY }, { "mmfairygb", RG_MM_GREAT_BAY_STRAY_FAIRY },
        { "mmfairyst", RG_MM_STONE_TOWER_STRAY_FAIRY },
    };
    for (auto& n : kNames) {
        if (_stricmp(s.c_str(), n.name) == 0) {
            return n.rg;
        }
    }
    if (s.rfind("RG_", 0) == 0) {
        for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
            if (EnumToString((RandomizerGet)rg) == s) {
                return (RandomizerGet)rg;
            }
        }
    }
    return (RandomizerGet)ParseInt(s);
}

RandomizerCheck ParseRandoCheck(const std::string& s) {
    if (s.rfind("RC_", 0) == 0) {
        for (int rc = RC_UNKNOWN_CHECK + 1; rc < RC_MAX; rc++) {
            if (EnumToString((RandomizerCheck)rc) == s) {
                return (RandomizerCheck)rc;
            }
        }
        return RC_UNKNOWN_CHECK;
    }
    return (RandomizerCheck)ParseInt(s);
}

// A randomizer item lying in the world: an EnItem00 drawn from its GetItemEntry (EnItem00_DrawRandomizedItem), as
// SoH's randomizer draws a check's item that a pot, grass or beehive dropped.
Actor* SpawnRandoItem00(const GetItemEntry& entry, f32 x, f32 y, f32 z, RandomizerCheck rc) {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    Actor* a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_ITEM00, x, y, z, 0, 0, 0, ITEM00_RUPEE_GREEN);
    if (a != nullptr) {
        EnItem00* item00 = (EnItem00*)a;
        item00->itemEntry = entry;
        item00->actor.params = ITEM00_SOH_DUMMY;
        item00->ogParams = ITEM00_SOH_DUMMY; // a drop, not a freestanding item
        item00->randoCheck = rc;
        // A grass tuft's or beehive's drop carries its check's RandomizerInf (ShuffleGrass.cpp); none otherwise.
        item00->randoInf = RAND_INF_MAX;
        if ((rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX)) {
            const auto& check = Rando::StaticData::GetLocation(rc)->GetCollectionCheck();
            if (check.type == SPOILER_CHK_RANDOMIZER_INF) {
                item00->randoInf = (RandomizerInf)check.flag;
            }
        }
        item00->actor.draw = (ActorFunc)EnItem00_DrawRandomizedItem;
    }
    return a;
}

// randogen: where the transformation masks, and the items whose weight each takes, were placed.
void LogRandoPlacements() {
    auto ctx = Rando::Context::GetInstance();
    Log("RANDOGEN settings shuffleTransformationMasks=%d transformationMasksInLogic=%d itemPool=%d shuffleMMMasks=%d",
        (int)ctx->GetOption(RSK_SHUFFLE_TRANSFORMATION_MASKS).Get(),
        (int)ctx->GetOption(RSK_TRANSFORMATION_MASKS_IN_LOGIC).Get(), (int)ctx->GetOption(RSK_ITEM_POOL).Get(),
        (int)ctx->GetOption(RSK_SHUFFLE_MM_MASKS).Get());
    Log("RANDOGEN mmEquipment=%d mmItems=%d mmSongs=%d strayFairies=%d", (int)ctx->GetOption(RSK_SHUFFLE_MM_EQUIPMENT).Get(),
        (int)ctx->GetOption(RSK_SHUFFLE_MM_ITEMS).Get(), (int)ctx->GetOption(RSK_SHUFFLE_MM_SONGS).Get(),
        (int)ctx->GetOption(RSK_SHUFFLE_STRAY_FAIRIES).Get());
    static const RandomizerGet kItems[] = { RG_MM_DEKU_MASK,   RG_MM_GORON_MASK,  RG_MM_ZORA_MASK,
                                            RG_MM_FIERCE_DEITY_MASK, RG_STONE_OF_AGONY, RG_MEGATON_HAMMER,
                                            RG_IRON_BOOTS,     RG_BIGGORON_SWORD,
                                            // Majora's Mask's other 20 masks
                                            RG_MM_MASK_OF_TRUTH, RG_MM_KAFEIS_MASK, RG_MM_ALL_NIGHT_MASK,
                                            RG_MM_BUNNY_HOOD, RG_MM_KEATON_MASK, RG_MM_GAROS_MASK,
                                            RG_MM_ROMANIS_MASK, RG_MM_CIRCUS_LEADERS_MASK, RG_MM_POSTMANS_HAT,
                                            RG_MM_COUPLES_MASK, RG_MM_GREAT_FAIRYS_MASK, RG_MM_GIBDO_MASK,
                                            RG_MM_DON_GEROS_MASK, RG_MM_KAMAROS_MASK, RG_MM_CAPTAINS_HAT,
                                            RG_MM_STONE_MASK, RG_MM_BREMEN_MASK, RG_MM_BLAST_MASK,
                                            RG_MM_MASK_OF_SCENTS, RG_MM_GIANTS_MASK,
                                            // Majora's Mask's equipment and one item, song and fairy of each kind
                                            RG_PROGRESSIVE_MM_SWORD, RG_MM_GREAT_FAIRYS_SWORD, RG_MM_HEROS_SHIELD,
                                            RG_MM_MIRROR_SHIELD, RG_MM_HEROS_BOW, RG_MM_HOOKSHOT, RG_MM_POWDER_KEG,
                                            RG_MM_BOTTLE_WITH_CHATEAU_ROMANI, RG_MM_MOONS_TEAR,
                                            RG_PROGRESSIVE_GORON_LULLABY, RG_MM_ODOLWAS_REMAINS,
                                            RG_MM_WOODFALL_STRAY_FAIRY };
    for (RandomizerGet rg : kItems) {
        Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
        int count = 0;
        for (int rc = 1; rc < RC_MAX; rc++) {
            if (ctx->GetItemLocation((RandomizerCheck)rc)->GetPlacedRandomizerGet() == rg) {
                count++;
                Log("RANDO %s at \"%s\" type=%d adv=%d major=%d category=%d hint=%d playthrough=%d",
                    item.GetName().GetEnglish().c_str(),
                    Rando::StaticData::GetLocation((RandomizerCheck)rc)->GetName().c_str(), (int)item.GetItemType(),
                    (int)item.IsAdvancement(), (int)item.IsMajorItem(), (int)item.GetCategory(),
                    (int)item.GetHintKey(), (int)item.IsPlaythrough());
            }
        }
        if (count == 0) {
            Log("RANDO %s not placed", item.GetName().GetEnglish().c_str());
        }
    }
}

// Capture the game window as the compositor last presented it. PW_RENDERFULLCONTENT reads the
// DWM surface, which is what makes this work for a Direct3D swap chain; a plain BitBlt of the
// window DC returns black for one.
// "giveall": everything Hyrule can hold, for the door's "carry everything" test (build/tests/door_carry_all).
void GiveAll() {
    // Ocarina of Time's item page, SLOT_* order: every item at its best (Ruto's Letter in a bottle, the last trade
    // items in the trade slots).
    static const u8 kItems[24] = {
        ITEM_STICK,     ITEM_NUT,          ITEM_BOMB,    ITEM_BOW,         ITEM_ARROW_FIRE,  ITEM_DINS_FIRE,
        ITEM_SLINGSHOT, ITEM_OCARINA_TIME, ITEM_BOMBCHU, ITEM_LONGSHOT,    ITEM_ARROW_ICE,   ITEM_FARORES_WIND,
        ITEM_BOOMERANG, ITEM_LENS,         ITEM_BEAN,    ITEM_HAMMER,      ITEM_ARROW_LIGHT, ITEM_NAYRUS_LOVE,
        ITEM_BOTTLE,    ITEM_POTION_RED,   ITEM_FAIRY,   ITEM_LETTER_RUTO, ITEM_CLAIM_CHECK, ITEM_LETTER_ZELDA,
    };
    for (s32 i = 0; i < 24; i++) {
        gSaveContext.inventory.items[i] = kItems[i];
    }
    Inventory_ChangeUpgrade(UPG_QUIVER, 3);
    Inventory_ChangeUpgrade(UPG_BOMB_BAG, 3);
    Inventory_ChangeUpgrade(UPG_STRENGTH, 3);
    Inventory_ChangeUpgrade(UPG_SCALE, 2);
    Inventory_ChangeUpgrade(UPG_WALLET, 2);
    Inventory_ChangeUpgrade(UPG_BULLET_BAG, 3);
    Inventory_ChangeUpgrade(UPG_STICKS, 3);
    Inventory_ChangeUpgrade(UPG_NUTS, 3);
    AMMO(ITEM_STICK) = 30;
    AMMO(ITEM_NUT) = 40;
    AMMO(ITEM_BOMB) = 40;
    AMMO(ITEM_BOW) = 50;
    AMMO(ITEM_SLINGSHOT) = 50;
    AMMO(ITEM_BOMBCHU) = 50;
    AMMO(ITEM_BEAN) = 10;
    // Kokiri, Master, Biggoron's; Deku, Hylian, Mirror; every tunic and boots.
    gSaveContext.inventory.equipment = 0x7777;
    gSaveContext.bgsFlag = 1;
    gSaveContext.swordHealth = 8;
    // Songs, medallions, Spiritual Stones, Stone of Agony, Gerudo Card, the token icon; 100 tokens.
    gSaveContext.inventory.questItems |= 0x00FFFFFF;
    gSaveContext.inventory.gsTokens = 100;
    // Every mask the Happy Mask Shop lends (all four sold), the Mask of Truth taken; every trade item.
    for (s32 flag : { ITEMGETINF_38, ITEMGETINF_39, ITEMGETINF_3A, ITEMGETINF_3B, ITEMGETINF_3F }) {
        Flags_SetItemGetInf(flag);
    }
    for (s32 i = 0; i <= RAND_INF_CHILD_TRADES_HAS_MASK_TRUTH - RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG; i++) {
        Flags_SetRandomizerInf((RandomizerInf)(RAND_INF_CHILD_TRADES_HAS_WEIRD_EGG + i));
    }
    for (s32 i = 0; i <= RAND_INF_ADULT_TRADES_HAS_CLAIM_CHECK - RAND_INF_ADULT_TRADES_HAS_POCKET_EGG; i++) {
        Flags_SetRandomizerInf((RandomizerInf)(RAND_INF_ADULT_TRADES_HAS_POCKET_EGG + i));
    }
    // Majora's Mask's songs (SoH MergeShipSongs).
    gMergeShipSave.mmSongs = (u16)((1u << MERGESHIP_SONG_COUNT) - 1);
    // Life, magic, money.
    gSaveContext.healthCapacity = gSaveContext.health = 0x140;
    gSaveContext.isDoubleDefenseAcquired = 1;
    gSaveContext.inventory.defenseHearts = 20;
    gSaveContext.isMagicAcquired = 1;
    gSaveContext.isDoubleMagicAcquired = 1;
    gSaveContext.magicLevel = 0; // the HUD grows the meter
    gSaveContext.magic = 0x60;
    gSaveContext.rupees = 500;
}

void LogShared(const MsSharedSave* shared) {
    if (shared == nullptr) {
        Log("SHARED none");
        return;
    }
    Log("SHARED own hyrule=0x%02X termina=0x%02X hyruleBeans=%d/%d magicBeans=%d bean=0x%02X/%d", shared->hyruleOwn,
        shared->terminaOwn, shared->hyruleBeans, shared->hyruleBeansWriter, shared->magicBeans, INV_CONTENT(ITEM_BEAN),
        AMMO(ITEM_BEAN));
    Log("SHARED gen=%u writer=%d slot=%d hp=%d/%d rupees=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X",
        shared->generation, shared->lastWriter, shared->saveSlot, shared->health, shared->healthCapacity,
        shared->rupees, shared->transformationMasks, shared->mmMasks, shared->items, shared->songs);
    Log("SHARED hyrule block=0x%X quest=0x%06X items=0x%06X trade=%d/%d seeds=%d tokens=%d swords=0x%02X "
        "shields=0x%02X equipment=0x%04X upgrades=0x%06X bottles=%d,%d,%d,%d,%d,%d",
        shared->hyruleBlock, shared->hyruleQuest, shared->hyruleItems, shared->hyruleTradeChild,
        shared->hyruleTradeAdult, shared->hyruleSeeds, shared->gsTokens, shared->swords, shared->shields,
        shared->ootOwnedEquipment, shared->ootUpgrades, shared->bottles[0], shared->bottles[1], shared->bottles[2],
        shared->bottles[3], shared->bottles[4], shared->bottles[5]);
}

void Screenshot(const std::string& name) {
#ifdef _WIN32
    struct Find {
        DWORD pid;
        HWND best;
        LONG bestArea;
    } find = { GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            Find* f = (Find*)lp;
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != f->pid || !IsWindowVisible(hwnd)) {
                return TRUE;
            }
            RECT r;
            GetClientRect(hwnd, &r);
            LONG area = (r.right - r.left) * (r.bottom - r.top);
            if (area > f->bestArea) {
                f->bestArea = area;
                f->best = hwnd;
            }
            return TRUE;
        },
        (LPARAM)&find);
    if (find.best == nullptr) {
        Log("SHOT %s failed: no window", name.c_str());
        return;
    }

    RECT rc;
    GetClientRect(find.best, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HDC screen = GetDC(find.best);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BOOL ok = PrintWindow(find.best, mem, PW_CLIENTONLY | 0x2 /* PW_RENDERFULLCONTENT */);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    int stride = ((w * 3 + 3) & ~3);
    std::vector<unsigned char> pixels((size_t)stride * h);
    GetDIBits(mem, bmp, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(find.best, screen);

    std::string path = sH.dir + "\\" + name + ".bmp";
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        Log("SHOT %s failed: cannot write %s", name.c_str(), path.c_str());
        return;
    }
    BITMAPFILEHEADER bf = {};
    bf.bfType = 0x4D42;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bf.bfSize = bf.bfOffBits + (DWORD)pixels.size();
    fwrite(&bf, sizeof(bf), 1, f);
    fwrite(&bi, sizeof(bi), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
    Log("SHOT %s %dx%d printwindow=%d", path.c_str(), w, h, ok);
#else
    (void)name; // (window capture: Windows only)
#endif
}

void TracePlayer() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* p = GET_PLAYER(gPlayState);
    if (p == nullptr) {
        return;
    }
    const char* anim = "";
    if (p->skelAnime.animation != nullptr) {
        const char* a = (const char*)p->skelAnime.animation;
        anim = (strncmp(a, "__OTR__", 7) == 0) ? a + 7 : "?";
    }
    {
        Camera* cam = gPlayState->cameraPtrs[gPlayState->activeCamera];
        if (cam != nullptr) {
            Log("C id=%d set=%d mode=%d anim=%d fov=%.1f roll=%d at=(%.1f,%.1f,%.1f) eye=(%.1f,%.1f,%.1f)",
                gPlayState->activeCamera, cam->setting, cam->mode, cam->animState, cam->fov, cam->roll, cam->at.x,
                cam->at.y, cam->at.z, cam->eye.x, cam->eye.y, cam->eye.z);
        }
    }
    Log("P scene=%d form=%d tf=%d cam=%d fill=%d/%d age=%d pos=(%.1f,%.1f,%.1f) yaw=%d spd=%.2f vy=%.2f act=%s upper=%s "
        "s1=%08X s2=%08X s3=%02X bg=%04X hp=%d mp=%d mask=%d anim=%s",
        gPlayState->sceneNum, p->mergeShipForm, p->mergeShipTransforming, gPlayState->activeCamera, MREG(64), MREG(68),
        gSaveContext.linkAge, p->actor.world.pos.x, p->actor.world.pos.y,
        p->actor.world.pos.z, p->actor.shape.rot.y, p->linearVelocity, p->actor.velocity.y,
        SymbolName((void*)p->actionFunc).c_str(), SymbolName((void*)p->upperActionFunc).c_str(), p->stateFlags1,
        p->stateFlags2, p->stateFlags3, p->actor.bgCheckFlags, gSaveContext.health, gSaveContext.magic,
        p->currentMask, anim);
    Log("H lh=%d rh=%d lhDL=%s rhDL=%s upperAnim=%s ocMode=%d msgMode=%d", p->leftHandType, p->rightHandType,
        (p->leftHandDLists && p->leftHandDLists[0]) ? (const char*)p->leftHandDLists[0] + 7 : "-",
        (p->rightHandDLists && p->rightHandDLists[0]) ? (const char*)p->rightHandDLists[0] + 7 : "-",
        (p->upperSkelAnime.animation) ? (const char*)p->upperSkelAnime.animation + 7 : "-",
        gPlayState->msgCtx.ocarinaMode, gPlayState->msgCtx.msgMode);
    // Geometry (form audit): ledge/wall/water/ceiling state, the collision cylinder, feet, focus and shadow.
    Log("G ledge=%.1f climb=%d wall=%.1f floorY=%.1f water=%.1f ceilH=%.1f wallR=%.1f cyl=%d/%d/%d feetY=%.1f/%.1f "
        "focusY=%.1f shadow=%.1f scale=%.4f lod=%d",
        p->yDistToLedge, p->ledgeClimbType, p->distToInteractWall, p->actor.floorHeight, p->actor.yDistToWater,
        p->ageProperties->ceilingCheckHeight, p->ageProperties->wallCheckRadius, p->cylinder.dim.radius,
        p->cylinder.dim.height, p->cylinder.dim.yShift, p->actor.shape.feetPos[0].y, p->actor.shape.feetPos[1].y,
        p->actor.focus.pos.y, p->actor.shape.shadowScale, p->actor.scale.y, p->actor.projectedPos.z < 320.0f);
    // Model placement: the root joint (animation root translation), the model's y offset, the head.
    Log("R root=(%d,%d,%d) yOff=%.1f headY=%.1f unk6AD=%d", p->skelAnime.jointTable[0].x, p->skelAnime.jointTable[0].y,
        p->skelAnime.jointTable[0].z, p->actor.shape.yOffset, p->bodyPartsPos[PLAYER_BODYPART_HEAD].y, p->unk_6AD);
    // Audio: is a fanfare playing (mask marches/dances must stop theirs), and the time of day.
    Log("A fanfare=%d seq=0x%02X time=0x%04X", gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].enabled,
        gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE].seqId, gSaveContext.dayTime);
}

// A scene exit's polygons, grouped into openings (a doorway, a gap in a hedge): centroids within 250 units merge.
struct ExitOpening {
    Vec3f center;
    Vec3f normal;
    int polys;
    f32 dist; // from the player
};

std::vector<ExitOpening> FindExitOpenings(s32 exitIndex) {
    std::vector<ExitOpening> openings;
    if (gPlayState == nullptr || gPlayState->colCtx.colHeader == nullptr) {
        return openings;
    }
    CollisionHeader* header = gPlayState->colCtx.colHeader;
    Player* player = GET_PLAYER(gPlayState);
    for (s32 i = 0; i < header->numPolygons; i++) {
        CollisionPoly* poly = &header->polyList[i];
        if ((s32)SurfaceType_GetSceneExitIndex(&gPlayState->colCtx, poly, BGCHECK_SCENE) != exitIndex) {
            continue;
        }
        Vec3f c = { 0.0f, 0.0f, 0.0f };
        for (s32 v = 0; v < 3; v++) {
            Vec3s* vtx = &header->vtxList[poly->vtxData[v] & 0x1FFF];
            c.x += vtx->x / 3.0f;
            c.y += vtx->y / 3.0f;
            c.z += vtx->z / 3.0f;
        }
        ExitOpening* found = nullptr;
        for (auto& o : openings) {
            if (Math_Vec3f_DistXZ(&o.center, &c) < 250.0f) {
                found = &o;
            }
        }
        if (found == nullptr) {
            openings.push_back({ c, { 0, 0, 0 }, 0, 0.0f });
            found = &openings.back();
        } else {
            f32 n = (f32)found->polys;
            found->center.x = (found->center.x * n + c.x) / (n + 1);
            found->center.y = (found->center.y * n + c.y) / (n + 1);
            found->center.z = (found->center.z * n + c.z) / (n + 1);
        }
        found->normal.x += COLPOLY_GET_NORMAL(poly->normal.x);
        found->normal.y += COLPOLY_GET_NORMAL(poly->normal.y);
        found->normal.z += COLPOLY_GET_NORMAL(poly->normal.z);
        found->polys++;
    }
    for (auto& o : openings) {
        o.dist = (player != nullptr) ? Math_Vec3f_DistXZ(&o.center, &player->actor.world.pos) : 0.0f;
    }
    std::sort(openings.begin(), openings.end(),
              [](const ExitOpening& a, const ExitOpening& b) { return a.dist < b.dist; });
    return openings;
}

void LogExitOpenings(s32 exitIndex) {
    auto openings = FindExitOpenings(exitIndex);
    for (size_t k = 0; k < openings.size(); k++) {
        const auto& o = openings[k];
        Log("EXIT %d (entrance 0x%X) opening %zu: center=(%.1f,%.1f,%.1f) polys=%d normal=(%.2f,%.2f,%.2f) dist=%.1f",
            exitIndex, (gPlayState->setupExitList != nullptr) ? (u16)gPlayState->setupExitList[exitIndex - 1] : 0xFFFF,
            k, o.center.x, o.center.y, o.center.z, o.polys, o.normal.x, o.normal.y, o.normal.z, o.dist);
    }
}

// The stick that moves the player towards a world yaw: the player turns towards the stick's angle plus the
// camera's input yaw; the stick's angle is atan2(-rel_x, rel_y), 0 = up (func_80077D10, Math_Atan2S), where each rel
// axis is the raw axis less a dead zone of 7, clamped to 60 (PadUtils_UpdateRelXY). The raw stick is built from the
// rel stick wanted, so the angle comes out exact: a raw stick of length 100 would clamp each axis on its own and bend
// the angle by up to 6 degrees. magnitude 100 is full tilt (rel 60; 55 and up counts as a direction for sidehops).
// The raw stick axis the game reads as relative axis r (-60..60): PadUtils_UpdateRelXY's dead zone of 7 added back.
s8 StickRawFromRel(f32 r) {
    const s32 v = (s32)floorf(r + 0.5f);
    if (v > 0) {
        return (s8)std::min(v + 7, 0x43);
    }
    if (v < 0) {
        return (s8)std::max(v - 7, -0x43);
    }
    return 0;
}

void StickTowardYaw(s16 worldYaw, f32 magnitude) {
    Camera* camera = gPlayState->cameraPtrs[gPlayState->activeCamera];
    if (camera == nullptr) {
        sH.stickX = sH.stickY = 0;
        return;
    }
    s16 angle = (s16)(worldYaw - Camera_GetInputDirYaw(camera));
    const f32 rel = std::clamp(magnitude, 0.0f, 100.0f) * 0.6f;
    sH.stickX = StickRawFromRel(-Math_SinS(angle) * rel);
    sH.stickY = StickRawFromRel(Math_CosS(angle) * rel);
}

// The stick that walks the player towards steerTarget.
void Steer() {
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        sH.stickX = sH.stickY = 0;
        return;
    }
    StickTowardYaw(Math_Vec3f_Yaw(&player->actor.world.pos, &sH.steerTarget), 100.0f);
}

bool InPlay() {
    return gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr &&
           gPlayState->transitionTrigger == TRANS_TRIGGER_OFF && gPlayState->transitionMode == TRANS_MODE_OFF;
}

// ---- autofight: the fight autopilot, for filming ---------------------------------------------------------------
//
// "autofight <frames> [style] [maxDist]" plays a fight the way a player would, with the controller alone: it picks the
// nearest living enemy or boss, locks on with Z (so the game's own camera frames the fight), steers towards it with
// the stick converted from a world direction exactly as walkto does, and attacks with the form's own moves in a
// varied, readable pattern -- ranged moves only with a clear line to a target it faces or is locked onto. It never
// places the player or touches an enemy. For filming it keeps the player alive (health and magic full every frame)
// and plays with Hold targeting (Z held keeps a lock, released drops it), restored when it ends.

enum FightStyle { FIGHT_AUTO, FIGHT_SWORD, FIGHT_FD, FIGHT_GORON, FIGHT_ZORA, FIGHT_DEKU, FIGHT_STYLE_COUNT };
const char* const kFightStyleNames[FIGHT_STYLE_COUNT] = { "auto", "sword", "fd", "goron", "zora", "deku" };

// A move's stick, relative to the direction of the target.
enum FightStick : u8 { FSTICK_NONE, FSTICK_TOWARD, FSTICK_AWAY, FSTICK_LEFT, FSTICK_RIGHT };

// A move's Z: held while locked on (pressed afresh when Navi is over the target); held regardless (Z-parallel when
// there is no lock: the Fierce Deity's slash then fires its beam, the Deku's bubble and the Zora's fins stay out of
// first person); or released (the Goron curls up only without a hostile lock-on).
enum FightZ : u8 { FZ_AUTO, FZ_ON, FZ_OFF };

enum FightMove {
    MV_SLASH,
    MV_STAB,
    MV_SPIN,
    MV_JUMP,
    MV_SIDEHOP,
    MV_BACKFLIP,
    MV_BEAMS,
    MV_PUNCH,
    MV_POUND,
    MV_ROLL,
    MV_JUMPKICK,
    MV_FINS,
    MV_DEKUSPIN,
    MV_BUBBLE,
    MV_COUNT,
    MV_NONE = MV_COUNT,
};
const char* const kFightMoveNames[MV_COUNT] = { "slash combo", "stab combo", "spin attack", "jump attack",
                                                "sidehop",     "backflip",   "beams",       "punch combo",
                                                "ground pound", "roll",      "jump kick",   "fins",
                                                "deku spin",   "bubble" };

struct FightSeg {
    u16 buttons;
    FightStick stick;
    FightZ z;
    s16 frames;
    bool untilUnlocked; // kept past its frames (up to 20 more) while the player still has a lock-on
};

// Where the target is, this frame.
struct FightView {
    f32 dist = 0.0f; // XZ, centre to centre
    f32 edge = 0.0f; // XZ, to the target's collision cylinder
    f32 dy = 0.0f;   // target above the player
    s16 yawTo = 0;
    bool locked = false;
    bool facing = false;   // within 22.5 degrees: enough for melee
    bool aimed = false;    // within 8 degrees: enough to shoot without a lock
    bool heightOk = false; // the target's body within the height a melee attack sweeps
    bool safeLeft = false; // a dodge that way lands on safe floor (worked out only when a dodge is possible)
    bool safeRight = false;
    bool safeBack = false;
};

struct AutoFight {
    int framesLeft = 0;
    FightStyle style = FIGHT_AUTO;
    f32 maxDist = 1200.0f;
    Actor* target = nullptr; // found in the actor lists again before every use: never read once it is gone
    s16 targetId = 0;
    std::vector<FightSeg> move; // the move being played, one input segment after another
    size_t seg = 0;
    int segFrame = 0;
    FightMove last[2] = { MV_NONE, MV_NONE };
    u32 rng = 0x2545F491u;
    bool zDown = false;     // Z held on the previous frame: a lock-on needs a fresh press
    int zRetry = 0;         // frames until the next lock-on press
    Vec3f lastPos = {};     // approach: the player's position a frame ago
    int blocked = 0;        // approach frames with the stick pushed and the player not moving
    int detourFrames = 0;   // walking round what blocks the way
    s16 detourYaw = 0;
    int detourSide = 1;
    int detours = 0;        // detours for this target; too many and it is out of reach
    int contactFrames = 0;  // pressed against the target's body: in reach whatever its size (King Dodongo)
    int closeInFrames = 0;  // after a ranged move: walk in rather than shoot again
    int dodgeCooldown = 0;
    int hitCooldown = 0; // after a hit, further health drops (a burn, knockback) are not new hits for a while
    bool pendingDodge = false;
    Actor* banned = nullptr; // out of reach, for bannedFrames
    int bannedFrames = 0;
    u8 savedZTargetSetting = 0;
    bool idleLogged = false;
    bool pausedLogged = false;
    int moves = 0;
    int targets = 0;
    int hitsTaken = 0;
};

AutoFight sFight;

// xorshift32: the fight's own choices, independent of the game's random numbers.
u32 FightRand() {
    u32 x = sFight.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return sFight.rng = x;
}

bool FightInLists(PlayState* play, Actor* actor) {
    if (actor == nullptr) {
        return false;
    }
    for (s32 cat : { ACTORCAT_ENEMY, ACTORCAT_BOSS }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            if (a == actor) {
                return true;
            }
        }
    }
    return false;
}

// Why an enemy or boss (already found in the actor lists) cannot be fought, or nullptr when it can: it must be alive,
// drawn, targetable (most enemies drop that while burrowed, hidden or dying), in the player's room, with health left,
// within maxDist. A boss keeps its own health (King Dodongo's colChkInfo.health is never set), so only enemies are
// held to colChkInfo.health; a boss must be hostile (the boss category also holds effects and cutscene actors).
const char* FightRejects(PlayState* play, Player* p, Actor* a, f32 maxDist) {
    if (a->update == nullptr) {
        return "dead";
    }
    if (a->draw == nullptr) {
        return "hidden";
    }
    if (!(a->flags & ACTOR_FLAG_ATTENTION_ENABLED)) {
        return "untargetable";
    }
    if ((a->room >= 0) && (a->room != play->roomCtx.curRoom.num)) {
        return "other room";
    }
    if (a->category == ACTORCAT_BOSS) {
        if (!(a->flags & ACTOR_FLAG_HOSTILE)) {
            return "not hostile";
        }
    } else if (a->colChkInfo.health == 0) {
        return "no health";
    }
    if (Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos) > maxDist) {
        return "out of range";
    }
    return nullptr;
}

// A clear line (no wall, floor -- a hill in between -- or ceiling) from the player's head to the target's focus.
bool FightLineOfSight(PlayState* play, Player* p, Actor* a) {
    Vec3f from = p->actor.focus.pos;
    Vec3f to = a->focus.pos;
    Vec3f hit;
    CollisionPoly* poly = nullptr;
    s32 bgId = BGCHECK_SCENE;
    return !BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bgId);
}

// What makes the floor dist away towards yaw no place to step: no floor, a drop of more than 100, lava (Ocarina of
// Time floor types 2 and 3 hurt, 9 burns; unless the player already stands on the same), a void-out floor (floor
// properties 5 and 12), a loading zone, or deep water. nullptr: safe.
const char* FightFloorHazard(PlayState* play, Player* p, s16 yaw, f32 dist) {
    Vec3f probe = p->actor.world.pos;
    probe.x += Math_SinS(yaw) * dist;
    probe.z += Math_CosS(yaw) * dist;
    probe.y += 50.0f;
    CollisionPoly* poly = nullptr;
    s32 bgId = BGCHECK_SCENE;
    const f32 floorY = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &probe);
    if ((poly == nullptr) || (floorY <= BGCHECK_Y_MIN + 1.0f)) {
        return "no floor";
    }
    if (floorY < p->actor.world.pos.y - 100.0f) {
        return "a drop";
    }
    const u32 type = SurfaceType_GetFloorType(&play->colCtx, poly, bgId);
    if ((type == 2) || (type == 3) || (type == 9)) {
        const u32 ownType = (p->actor.floorPoly != nullptr)
                                ? SurfaceType_GetFloorType(&play->colCtx, p->actor.floorPoly, p->actor.floorBgId)
                                : 0;
        if (ownType != type) {
            return "lava";
        }
    }
    const u32 property = func_80041EA4(&play->colCtx, poly, bgId);
    if ((property == FLOOR_PROPERTY_5) || (property == FLOOR_PROPERTY_12)) {
        return "a void";
    }
    if (SurfaceType_GetSceneExitIndex(&play->colCtx, poly, bgId) != 0) {
        return "a loading zone";
    }
    f32 surface = 0.0f;
    WaterBox* water = nullptr;
    // (A water box is looked up by x and z alone: one whose surface is above the player's head is another level's.)
    if (WaterBox_GetSurface1(play, &play->colCtx, probe.x, probe.z, &surface, &water) && (surface < probe.y) &&
        (surface - floorY > 50.0f)) {
        return "deep water";
    }
    return nullptr;
}

// A sidehop or backflip towards yaw lands on safe floor (checked halfway and where it lands).
bool FightDodgeSafe(PlayState* play, Player* p, s16 yaw) {
    return (FightFloorHazard(play, p, yaw, 40.0f) == nullptr) && (FightFloorHazard(play, p, yaw, 80.0f) == nullptr);
}

// The ground from the player to dist along yaw is safe to roll over (every 60).
bool FightPathSafe(PlayState* play, Player* p, s16 yaw, f32 dist) {
    for (f32 d = 40.0f; d <= dist; d += 60.0f) {
        if (FightFloorHazard(play, p, yaw, d) != nullptr) {
            return false;
        }
    }
    return true;
}

// The nearest one to fight: XZ distance, with a height difference and a wall in between counting against it.
Actor* FightPickTarget(PlayState* play, Player* p) {
    Actor* best = nullptr;
    f32 bestScore = 0.0f;
    for (s32 cat : { ACTORCAT_ENEMY, ACTORCAT_BOSS }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            if ((FightRejects(play, p, a, sFight.maxDist) != nullptr) ||
                ((a == sFight.banned) && (sFight.bannedFrames > 0))) {
                continue;
            }
            f32 dy = fabsf(a->world.pos.y - p->actor.world.pos.y);
            f32 score = Math_Vec3f_DistXZ(&a->world.pos, &p->actor.world.pos) + std::max(0.0f, dy - 100.0f) * 2.0f +
                        (FightLineOfSight(play, p, a) ? 0.0f : 400.0f);
            if ((best == nullptr) || (score < bestScore)) {
                best = a;
                bestScore = score;
            }
        }
    }
    return best;
}

void FightEndMove();

// A new target ends the move in progress: it was aimed at the old one (a beam volley would go on firing where that
// one stood).
void FightSetTarget(Player* p, Actor* a, const char* why) {
    FightEndMove();
    sFight.target = a;
    sFight.targetId = a->id;
    sFight.targets++;
    sFight.blocked = 0;
    sFight.detours = 0;
    sFight.detourFrames = 0;
    sFight.contactFrames = 0;
    sFight.lastPos = p->actor.world.pos;
    Log("AUTOFIGHT target 0x%03X %s params=0x%04X (%s) dist=%.0f dy=%.0f hp=%d", a->id,
        (a->category == ACTORCAT_BOSS) ? "boss" : "enemy", (u16)a->params, why,
        Math_Vec3f_DistXZ(&a->world.pos, &p->actor.world.pos), a->world.pos.y - p->actor.world.pos.y,
        a->colChkInfo.health);
}

FightStyle FightStyleFor(Player* p) {
    if (sFight.style != FIGHT_AUTO) {
        return sFight.style;
    }
    switch (p->mergeShipForm) {
        case PLAYER_FORM_DEKU:
            return FIGHT_DEKU;
        case PLAYER_FORM_GORON:
            return FIGHT_GORON;
        case PLAYER_FORM_ZORA:
            return FIGHT_ZORA;
        case PLAYER_FORM_FIERCE_DEITY:
            return FIGHT_FD;
        default:
            return FIGHT_SWORD;
    }
}

// How close (to the target's cylinder) a melee attack lands.
f32 FightReach(FightStyle style, Player* p) {
    switch (style) {
        case FIGHT_FD:
            return 80.0f;
        case FIGHT_GORON:
            return 45.0f;
        case FIGHT_ZORA:
            return 50.0f;
        case FIGHT_DEKU:
            return 40.0f;
        default:
            if (Player_HoldsTwoHandedWeapon(p)) {
                return 75.0f;
            }
            return (gSaveContext.linkAge == LINK_AGE_CHILD) ? 45.0f : 60.0f;
    }
}

// How high above the player's feet a melee attack sweeps.
f32 FightReachHeight(FightStyle style) {
    switch (style) {
        case FIGHT_FD:
            return 140.0f;
        case FIGHT_GORON:
            return 80.0f;
        case FIGHT_ZORA:
            return 90.0f;
        case FIGHT_DEKU:
            return 50.0f;
        default:
            return (gSaveContext.linkAge == LINK_AGE_CHILD) ? 70.0f : 100.0f;
    }
}

// The target's body (feet to the top of its cylinder or its focus) within the height a melee attack sweeps: a
// Skulltula hanging overhead or an enemy on a ledge above or below is not in melee reach, however close. A boss's
// cylinder says little of its size (King Dodongo's is 0): always.
bool FightMeleeHeightOk(Player* p, Actor* t, f32 top) {
    if (t->category == ACTORCAT_BOSS) {
        return true;
    }
    const f32 py = p->actor.world.pos.y;
    const f32 lo = std::min(t->world.pos.y, t->focus.pos.y) - 10.0f;
    const f32 hi = std::max(t->world.pos.y + t->colChkInfo.cylHeight, t->focus.pos.y) + 10.0f;
    return (hi >= py - 30.0f) && (lo <= py + top);
}

// The port's condition for the Goron's leap into a ground pound (MergeShipGoron.inc; MM Inventory_GetBtnBItem() <
// ITEM_FD): without a B item he only curls up and uncurls.
bool FightGoronCanPound() {
    const u8 b = gSaveContext.equips.buttonItems[0];
    return (b != ITEM_NONE) && (b != ITEM_NONE_FE) && (b != ITEM_MM_FD);
}

// Filming: the meter never runs dry. Only while no consumption is stepping it down to a target it must meet exactly
// (MAGIC_STATE_CONSUME); FLASH_1 is a consumption finished, FLASH_3 the Goron's continuous drain.
void FightTopUpMagic() {
    if (!gSaveContext.isMagicAcquired || (gSaveContext.magicLevel == 0) || (gSaveContext.magicCapacity == 0)) {
        // As "magic": the meter too, or the game empties the magic.
        gSaveContext.isMagicAcquired = true;
        if (gSaveContext.magicLevel == 0) {
            gSaveContext.magicLevel = gSaveContext.isDoubleMagicAcquired ? 2 : 1;
        }
        gSaveContext.magicCapacity = gSaveContext.magicLevel * MAGIC_NORMAL_METER;
    }
    const s16 state = gSaveContext.magicState;
    if (((state == MAGIC_STATE_IDLE) || (state == MAGIC_STATE_METER_FLASH_1) || (state == MAGIC_STATE_METER_FLASH_3)) &&
        (gSaveContext.magic < gSaveContext.magicCapacity)) {
        gSaveContext.magic = (s8)gSaveContext.magicCapacity;
    }
}

void FightEndMove() {
    sFight.move.clear();
    sFight.seg = 0;
    sFight.segFrame = 0;
    sFight.blocked = 0;
}

void FightStickFor(FightStick stick, s16 yawTo) {
    switch (stick) {
        case FSTICK_TOWARD:
            StickTowardYaw(yawTo, 100.0f);
            break;
        case FSTICK_AWAY:
            StickTowardYaw((s16)(yawTo + 0x8000), 100.0f);
            break;
        case FSTICK_LEFT:
            StickTowardYaw((s16)(yawTo + 0x4000), 100.0f);
            break;
        case FSTICK_RIGHT:
            StickTowardYaw((s16)(yawTo - 0x4000), 100.0f);
            break;
        default:
            sH.stickX = sH.stickY = 0;
            break;
    }
}

// Whether a Z press now would lock onto something worth fighting: Navi's arrow on the target, or on another enemy
// autofight would fight (the fight then switches to it: that is the one Navi offers, in front of the player). Never an
// actor that cannot be locked onto (ACTOR_FLAG_LOCK_ON_DISABLED): the press would only snap the camera behind the
// player (Z-parallel), every retry.
bool FightArrowLockable(PlayState* play, Actor* t) {
    Actor* arrow = play->actorCtx.targetCtx.arrowPointedActor; // cleared by Actor_Delete
    if (arrow == nullptr) {
        return false;
    }
    if (arrow != t) {
        Player* p = GET_PLAYER(play);
        if ((p == nullptr) || ((arrow == sFight.banned) && (sFight.bannedFrames > 0)) || !FightInLists(play, arrow) ||
            (FightRejects(play, p, arrow, sFight.maxDist) != nullptr)) {
            return false;
        }
    }
    return !(arrow->flags & ACTOR_FLAG_LOCK_ON_DISABLED);
}

// Z for this frame. With Hold targeting Z held keeps a lock, and a lock needs a fresh press while Navi is over the
// target (Player_UpdateZTargeting locks onto targetCtx.arrowPointedActor on the press), so an unlocked FZ_AUTO frame
// presses only then, and lets go again if the press did not lock.
void FightApplyZ(PlayState* play, Actor* t, FightZ z, bool locked) {
    bool held = false;
    if (z == FZ_ON) {
        held = true;
    } else if (z == FZ_AUTO) {
        if (locked) {
            held = true;
        } else if (!sFight.zDown && (sFight.zRetry == 0) && FightArrowLockable(play, t)) {
            held = true;
            sFight.zRetry = 10;
        }
    }
    if (held) {
        sH.buttons |= BTN_Z;
    } else {
        sH.buttons &= (u16)~BTN_Z;
    }
}

void FightAdd(u16 buttons, FightStick stick, int frames, FightZ z = FZ_AUTO, bool untilUnlocked = false) {
    sFight.move.push_back({ buttons, stick, z, (s16)frames, untilUnlocked });
}

// Taps of a button, one frame down and gap - 1 up: each lands as the game takes the next press (a combo's window).
void FightTaps(u16 button, FightStick stick, int taps, int gap, FightZ z = FZ_AUTO) {
    for (int i = 0; i < taps; i++) {
        FightAdd(button, stick, 1, z);
        FightAdd(0, stick, gap - 1, z);
    }
}

// Each move as a player plays it (the forms' controls are MM's, MergeShipPlayerForms.inc):
//   sword/FD  B slashes (stick towards the target: stabs); Z+A a jump attack; B held then released a spin attack;
//             Z + stick sideways/back + A a sidehop/backflip; the Fierce Deity's Z-targeted slash fires a beam
//   Goron     B punches; A held curls up (only without a hostile lock), B while curled leaps into a ground pound,
//             A held while moving rolls
//   Zora      B punches; Z+A a jump kick; B held after a punch aims the fins, released throws them
//   Deku      A spins; B held blows a bubble (magic), released fires it
void FightBuild(FightMove m, const FightView& v) {
    FightEndMove();
    // A sidehop goes to a side whose floor is safe (FightChoose and the dodge checks offer it only with one).
    FightStick side = (FightRand() & 1) ? FSTICK_LEFT : FSTICK_RIGHT;
    if ((side == FSTICK_LEFT) ? !v.safeLeft : !v.safeRight) {
        side = (side == FSTICK_LEFT) ? FSTICK_RIGHT : FSTICK_LEFT;
    }
    const FightZ aim = v.locked ? FZ_AUTO : FZ_ON;
    switch (m) {
        case MV_SLASH:
            FightTaps(BTN_B, FSTICK_NONE, 3 + (int)(FightRand() & 1), 6);
            FightAdd(0, FSTICK_NONE, 6);
            break;
        case MV_STAB:
            FightTaps(BTN_B, FSTICK_TOWARD, 3, 6);
            FightAdd(0, FSTICK_NONE, 6);
            break;
        case MV_SPIN:
            FightAdd(BTN_B, FSTICK_NONE, 28);
            FightAdd(0, FSTICK_NONE, 22);
            break;
        case MV_JUMP:
        case MV_JUMPKICK:
            FightAdd(BTN_A, FSTICK_NONE, 1);
            FightAdd(0, FSTICK_NONE, 24);
            break;
        case MV_SIDEHOP:
            FightAdd(0, side, 2, FZ_ON);
            FightAdd(BTN_A, side, 1, FZ_ON);
            FightAdd(0, side, 10, FZ_ON);
            FightAdd(0, FSTICK_NONE, 3);
            break;
        case MV_BACKFLIP:
            FightAdd(0, FSTICK_AWAY, 2, FZ_ON);
            FightAdd(BTN_A, FSTICK_AWAY, 1, FZ_ON);
            FightAdd(0, FSTICK_NONE, 16, FZ_ON);
            break;
        case MV_BEAMS:
            FightAdd(0, FSTICK_NONE, 2, aim);
            FightTaps(BTN_B, FSTICK_NONE, 2 + (int)(FightRand() & 1), 12, aim);
            FightAdd(0, FSTICK_NONE, 6);
            break;
        case MV_PUNCH:
            FightTaps(BTN_B, FSTICK_NONE, 3, 7);
            FightAdd(0, FSTICK_NONE, 6);
            break;
        case MV_POUND:
            // Z let go first, until the lock has really ended: a hostile lock stops the curl (MergeShip_Goron_
            // ActionHandler_6), and it outlasts Z by 5 frames -- by up to 15 when it has just been made
            // (zTargetActiveTimer). A is read as a curl only when it goes down with no lock.
            FightAdd(0, FSTICK_NONE, 6, FZ_OFF, true);
            // The curl (pg_maru_change, 7 frames at 2/3 speed: about 11) must end before B is read (MM func_80857A44),
            // so B comes at 16, and again 6 later in case that one was early (the leap is in the air by then, where B
            // is not read). A held through the leap and the slam: letting go uncurls (MM func_80857950).
            FightAdd(BTN_A, FSTICK_NONE, 16, FZ_OFF);
            FightAdd(BTN_A | BTN_B, FSTICK_NONE, 1, FZ_OFF);
            FightAdd(BTN_A, FSTICK_NONE, 5, FZ_OFF);
            FightAdd(BTN_A | BTN_B, FSTICK_NONE, 1, FZ_OFF);
            FightAdd(BTN_A, FSTICK_NONE, 30, FZ_OFF);
            FightAdd(0, FSTICK_NONE, 8, FZ_OFF);
            break;
        case MV_ROLL: {
            // Moving when A goes down: the Goron rolls off at once (MM func_80839F98), steered at the target. As for
            // the pound, only once the lock has ended.
            int frames = std::clamp((int)(v.edge / 10.0f) + 18, 22, 55);
            FightAdd(0, FSTICK_TOWARD, 6, FZ_OFF, true);
            FightAdd(BTN_A, FSTICK_TOWARD, frames, FZ_OFF);
            FightAdd(0, FSTICK_NONE, 10, FZ_OFF);
            break;
        }
        case MV_FINS:
            // B held through the whole punch: the fins are aimed as it ends only if B is still down (MM
            // Player_ActionHandler_8, unk_ADC > 0), and thrown when it is let go once they are ready.
            FightAdd(0, FSTICK_NONE, 2, aim);
            FightAdd(BTN_B, FSTICK_NONE, 24, aim);
            FightAdd(0, FSTICK_NONE, 26, aim);
            break;
        case MV_DEKUSPIN:
            FightAdd(BTN_A, FSTICK_TOWARD, 1);
            FightAdd(0, FSTICK_TOWARD, 16);
            FightAdd(0, FSTICK_NONE, 4);
            break;
        case MV_BUBBLE:
            FightAdd(0, FSTICK_NONE, 2, FZ_ON);
            FightAdd(BTN_B, FSTICK_NONE, 22, FZ_ON);
            FightAdd(0, FSTICK_NONE, 16, FZ_ON);
            break;
        default:
            break;
    }
}

struct FightOption {
    FightMove move;
    int weight;
};

// A weighted choice, never the same move three times running.
FightMove FightPick(const std::vector<FightOption>& options) {
    int total = 0;
    for (const FightOption& o : options) {
        total += o.weight;
    }
    if (total <= 0) {
        return MV_NONE;
    }
    int r = (int)(FightRand() % (u32)total);
    size_t i = 0;
    while ((i + 1 < options.size()) && (r >= options[i].weight)) {
        r -= options[i].weight;
        i++;
    }
    if ((options.size() > 1) && (options[i].move == sFight.last[0]) && (options[i].move == sFight.last[1])) {
        i = (i + 1) % options.size();
    }
    return options[i].move;
}

// The next move for the style from where the target is: melee in reach; a ranged or closing move further off, only
// with a clear line to a target the player is locked onto or faces. MV_NONE: keep closing in.
FightMove FightChoose(PlayState* play, Player* p, Actor* t, FightStyle style, const FightView& v) {
    std::vector<FightOption> o;
    auto offer = [&](FightMove m, int weight, bool ok) {
        if (ok) {
            o.push_back({ m, weight });
        }
    };
    const f32 reach = FightReach(style, p);
    // In reach and facing it (a lock-on turns the player to it; unlocked, the approach turns him first), and not
    // overhead or below.
    const bool inReach = ((v.edge <= reach) || (sFight.contactFrames > 0)) && (v.locked || v.facing) && v.heightOk;
    const bool dodged = (sFight.last[0] == MV_SIDEHOP) || (sFight.last[0] == MV_BACKFLIP);
    const bool dodge = v.locked && (style != FIGHT_GORON) && (sFight.dodgeCooldown == 0) && !dodged &&
                       (sFight.last[0] != MV_NONE);
    const bool hop = dodge && (v.safeLeft || v.safeRight);
    const bool flip = dodge && v.safeBack;
    // Unlocked, a ranged move holds Z from its first frame, and that press locks onto whatever Navi is over: only when
    // that is nothing (Z-parallel: straight ahead, where the player faces), the target, or another enemy to fight --
    // never a bystander the beam or bubble would then be aimed at.
    const Actor* arrow = play->actorCtx.targetCtx.arrowPointedActor;
    const bool zSafe = v.locked || (arrow == nullptr) || (arrow == t) || FightArrowLockable(play, t);
    const bool ranged =
        (sFight.closeInFrames == 0) && (v.locked || v.aimed) && zSafe && FightLineOfSight(play, p, t);
    const bool swordOut = Player_GetMeleeWeaponHeld(p) != 0;
    switch (style) {
        case FIGHT_SWORD:
        case FIGHT_FD:
            if (inReach) {
                offer(MV_SLASH, 30, true);
                offer(MV_STAB, 20, true);
                offer(MV_SPIN, 14, true);
                offer(MV_JUMP, 16, v.locked && swordOut);
                offer(MV_SIDEHOP, 12, hop);
                offer(MV_BACKFLIP, 8, flip);
            } else if ((style == FIGHT_FD) && ranged && (v.edge >= 200.0f) && (v.edge <= 700.0f)) {
                offer(MV_BEAMS, 1, true);
            } else if (v.locked && swordOut && v.heightOk && (v.edge <= reach + 40.0f) && (fabsf(v.dy) < 80.0f) &&
                       ((FightRand() & 3) == 0) &&
                       (FightFloorHazard(play, p, v.yawTo, std::clamp(v.edge, 20.0f, 60.0f)) == nullptr)) {
                offer(MV_JUMP, 1, true); // leap in
            }
            break;
        case FIGHT_GORON:
            if (inReach) {
                offer(MV_PUNCH, 60, true);
                offer(MV_POUND, 40, FightGoronCanPound());
            } else if ((v.edge >= 120.0f) && (v.edge <= 700.0f) && (fabsf(v.dy) < 80.0f) &&
                       FightLineOfSight(play, p, t) && FightPathSafe(play, p, v.yawTo, v.edge)) {
                offer(MV_ROLL, 1, true);
            }
            break;
        case FIGHT_ZORA:
            if (inReach) {
                offer(MV_PUNCH, 45, true);
                offer(MV_JUMPKICK, 20, v.locked);
                offer(MV_SIDEHOP, 12, hop);
                offer(MV_BACKFLIP, 8, flip);
            } else if (ranged && (v.edge >= 120.0f) && (v.edge <= 550.0f)) {
                offer(MV_FINS, 1, true);
            }
            break;
        case FIGHT_DEKU:
            if (inReach) {
                offer(MV_DEKUSPIN, 70, true);
                offer(MV_SIDEHOP, 15, hop);
                offer(MV_BACKFLIP, 10, flip);
            } else if (ranged && (v.edge >= 90.0f) && (v.edge <= 500.0f)) {
                offer(MV_BUBBLE, 1, true);
            }
            break;
        default:
            break;
    }
    return FightPick(o);
}

// The target coming at the player: moving, facing him, close.
bool FightIncoming(Player* p, Actor* t, const FightView& v) {
    if ((v.dist > 220.0f) || (t->speedXZ < 3.0f)) {
        return false;
    }
    s16 d = (s16)(t->world.rot.y - Actor_WorldYawTowardActor(t, &p->actor));
    return std::abs((int)d) < 0x2000;
}

void FightStart(FightMove m, const FightView& v, const char* why) {
    FightBuild(m, v);
    sFight.last[1] = sFight.last[0];
    sFight.last[0] = m;
    sFight.moves++;
    sFight.detours = 0; // it is being fought: the detours counted towards setting it aside start over
    if ((m == MV_SIDEHOP) || (m == MV_BACKFLIP)) {
        sFight.dodgeCooldown = 50;
    }
    if (((m == MV_BEAMS) || (m == MV_FINS) || (m == MV_BUBBLE)) && (FightRand() & 1)) {
        sFight.closeInFrames = 30 + (int)(FightRand() % 30); // then walk in
    }
    Log("AUTOFIGHT %s -> 0x%03X dist=%.0f edge=%.0f dy=%.0f locked=%d%s%s%s", kFightMoveNames[m], sFight.targetId,
        v.dist, v.edge, v.dy, v.locked ? 1 : 0, (why != nullptr) ? " (" : "", (why != nullptr) ? why : "",
        (why != nullptr) ? ")" : "");
}

void FightRunSegment(PlayState* play, Player* p, Actor* t, const FightView& v) {
    const FightSeg s = sFight.move[sFight.seg];
    sH.buttons = s.buttons;
    FightStickFor(s.stick, v.yawTo);
    FightApplyZ(play, t, s.z, v.locked);
    ++sFight.segFrame;
    if (s.untilUnlocked && (p->focusActor != nullptr) && (sFight.segFrame < s.frames + 20)) {
        return; // the lock-on has not ended yet
    }
    if (sFight.segFrame >= s.frames) {
        sFight.segFrame = 0;
        if (++sFight.seg >= sFight.move.size()) {
            FightEndMove();
        }
    }
}

// Closing in: the stick at the target, Z-locked when close (far off Link runs: a Z-walk is slow). He never steps off
// a ledge or into lava, a void, a loading zone or deep water (FightFloorHazard, looked at just ahead): that blocks him
// as a wall does. When the stick is pushed and he has not moved for 10 frames he is blocked: against the target's own
// body he is in reach (a boss's cylinder says little of its size) unless it is overhead or below; against a wall, a
// hazard or anything else he walks round it, alternating sides (a side with safe floor), and after four detours
// without landing a move the target is out of reach and set aside for a while.
void FightApproach(PlayState* play, Player* p, Actor* t, const FightView& v) {
    if (sFight.detourFrames > 0) {
        if (FightFloorHazard(play, p, sFight.detourYaw, 60.0f) != nullptr) {
            sFight.detourFrames = 0; // the detour leads onto a hazard: stop it, and approach (or detour) again
        } else {
            sFight.detourFrames--;
            StickTowardYaw(sFight.detourYaw, 100.0f);
            FightApplyZ(play, t, FZ_OFF, v.locked);
            sFight.lastPos = p->actor.world.pos;
            return;
        }
    }
    // The floor just ahead; where the target is closer than that, at its edge (its own floor).
    const char* hazard = FightFloorHazard(play, p, v.yawTo, std::clamp(v.edge, 20.0f, 60.0f));
    f32 moved = Math_Vec3f_DistXZ(&p->actor.world.pos, &sFight.lastPos);
    sFight.lastPos = p->actor.world.pos;
    sFight.blocked = (moved < 1.0f) ? sFight.blocked + 1 : 0;
    if (hazard != nullptr) {
        sFight.blocked = 10; // at once: never a step towards it
    }
    if (sFight.blocked >= 10) {
        sFight.blocked = 0;
        const bool wall = (hazard != nullptr) || ((p->actor.bgCheckFlags & BGCHECKFLAG_WALL) != 0);
        if (!wall && v.heightOk && (v.dist < ((t->category == ACTORCAT_BOSS) ? 350.0f : 120.0f))) {
            sFight.contactFrames = 40;
            Log("AUTOFIGHT against 0x%03X at dist %.0f: in reach", sFight.targetId, v.dist);
        } else if (++sFight.detours > 4) {
            Log("AUTOFIGHT 0x%03X out of reach (%d detours%s%s): set aside", sFight.targetId, sFight.detours - 1,
                (hazard != nullptr) ? ", " : "", (hazard != nullptr) ? hazard : "");
            sFight.banned = t;
            sFight.bannedFrames = 400;
            sFight.target = nullptr;
            FightApplyZ(play, t, FZ_OFF, v.locked);
            return;
        } else {
            sFight.detourSide = -sFight.detourSide;
            sFight.detourYaw = (s16)(v.yawTo + sFight.detourSide * 0x3000);
            if (FightFloorHazard(play, p, sFight.detourYaw, 60.0f) != nullptr) {
                sFight.detourSide = -sFight.detourSide;
                sFight.detourYaw = (s16)(v.yawTo + sFight.detourSide * 0x3000);
            }
            const bool safe = FightFloorHazard(play, p, sFight.detourYaw, 60.0f) == nullptr;
            sFight.detourFrames = safe ? 16 : 0;
            if (hazard != nullptr) {
                sFight.closeInFrames = 0; // no walking in across it: a ranged move may be had from here
            }
            Log("AUTOFIGHT blocked%s%s at dist %.0f: %s%s", (hazard != nullptr) ? " by " : (wall ? " by a wall" : ""),
                (hazard != nullptr) ? hazard : "", v.dist, safe ? "detour to the " : "no safe way round",
                safe ? ((sFight.detourSide > 0) ? "left" : "right") : "");
            if (safe) {
                StickTowardYaw(sFight.detourYaw, 100.0f);
            }
            FightApplyZ(play, t, FZ_OFF, v.locked);
            return;
        }
    }
    StickTowardYaw(v.yawTo, 100.0f);
    const bool close = v.locked ? (v.edge < 450.0f) : (v.edge < 320.0f);
    FightApplyZ(play, t, close ? FZ_AUTO : FZ_OFF, v.locked);
}

// One frame of the fight: decides this frame's buttons and stick.
void AutoFightFrame() {
    PlayState* play = gPlayState;
    sH.buttons = 0;
    sH.stickX = sH.stickY = 0;
    Player* p = (play != nullptr) ? GET_PLAYER(play) : nullptr;
    if (p == nullptr) {
        sFight.zDown = false;
        return;
    }

    // Filming: the fight never ends in a game over or an empty meter. A health drop since last frame is a hit taken --
    // one per 20 frames, so a burn or a hot floor hurting every few frames does not cut every move short.
    if (gSaveContext.health < gSaveContext.healthCapacity) {
        if (sFight.hitCooldown == 0) {
            sFight.hitsTaken++;
            sFight.pendingDodge = true;
            FightEndMove();
            Log("AUTOFIGHT hit taken (%d)", sFight.hitsTaken);
        }
        sFight.hitCooldown = 20;
    }
    gSaveContext.health = gSaveContext.healthCapacity;
    FightTopUpMagic();

    for (int* timer : { &sFight.zRetry, &sFight.bannedFrames, &sFight.contactFrames, &sFight.closeInFrames,
                        &sFight.dodgeCooldown, &sFight.hitCooldown }) {
        if (*timer > 0) {
            (*timer)--;
        }
    }

    if (!InPlay() || (play->csCtx.state != CS_STATE_IDLE) || (play->msgCtx.msgMode != MSGMODE_NONE) ||
        (p->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING | PLAYER_STATE1_IN_CUTSCENE |
                           PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_IN_ITEM_CS))) {
        if (!sFight.pausedLogged) {
            Log("AUTOFIGHT paused: cutscene or message");
            sFight.pausedLogged = true;
        }
        FightEndMove();
        sFight.zDown = false;
        return;
    }
    if (sFight.pausedLogged) {
        Log("AUTOFIGHT resumed");
        sFight.pausedLogged = false;
    }

    // The target: kept until it dies or leaves range. If the game's lock-on took another live enemy, that one.
    // (Found in the lists by address: an actor spawned where a dead one was is another actor unless its id matches.)
    if (sFight.target != nullptr) {
        const char* why = !FightInLists(play, sFight.target)            ? "gone"
                          : (sFight.target->id != sFight.targetId) ? "gone (another actor in its place)"
                                                                   : FightRejects(play, p, sFight.target,
                                                                                  sFight.maxDist * 1.25f);
        if (why != nullptr) {
            Log("AUTOFIGHT target 0x%03X lost: %s", sFight.targetId, why);
            sFight.target = nullptr;
        }
    }
    Actor* focus = p->focusActor;
    if ((focus != nullptr) && (focus != sFight.target) && !((focus == sFight.banned) && (sFight.bannedFrames > 0)) &&
        FightInLists(play, focus) && (FightRejects(play, p, focus, sFight.maxDist * 1.25f) == nullptr)) {
        FightSetTarget(p, focus, "lock-on");
    }
    if (sFight.target == nullptr) {
        Actor* t = FightPickTarget(play, p);
        if (t != nullptr) {
            FightSetTarget(p, t, "nearest");
        }
    }
    if (sFight.target == nullptr) {
        if (!sFight.idleLogged) {
            Log("AUTOFIGHT no target within %.0f: idle", sFight.maxDist);
            sFight.idleLogged = true;
        }
        FightEndMove();
        sFight.zDown = false;
        return;
    }
    sFight.idleLogged = false;

    Actor* t = sFight.target;
    const FightStyle style = FightStyleFor(p);
    FightView v;
    v.dist = Math_Vec3f_DistXZ(&p->actor.world.pos, &t->world.pos);
    v.edge = std::max(0.0f, v.dist - (f32)t->colChkInfo.cylRadius);
    v.dy = t->world.pos.y - p->actor.world.pos.y;
    v.yawTo = Math_Vec3f_Yaw(&p->actor.world.pos, &t->world.pos);
    v.locked = (p->focusActor == t);
    const int facingDiff = std::abs((int)(s16)(p->actor.shape.rot.y - v.yawTo));
    v.facing = facingDiff < 0x1000;
    v.aimed = facingDiff < 0x600;
    v.heightOk = FightMeleeHeightOk(p, t, FightReachHeight(style));

    if (p->stateFlags1 & PLAYER_STATE1_DAMAGED) {
        // Knocked back: only keep the lock.
        FightApplyZ(play, t, FZ_AUTO, v.locked);
    } else if (!sFight.move.empty()) {
        FightRunSegment(play, p, t, v);
    } else if ((p->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_HANGING_OFF_LEDGE |
                                  PLAYER_STATE1_CLIMBING_LEDGE)) ||
               (p->stateFlags2 & PLAYER_STATE2_HOPPING)) {
        // Still in the air: a new move's first press would be lost.
        FightApplyZ(play, t, FZ_AUTO, v.locked);
    } else {
        FightMove m = MV_NONE;
        const char* why = nullptr;
        const bool canDodge = v.locked && (style != FIGHT_GORON) && (sFight.dodgeCooldown == 0);
        if (canDodge) {
            // Where a dodge would land (no sidehop or backflip off a ledge or into lava).
            v.safeLeft = FightDodgeSafe(play, p, (s16)(v.yawTo + 0x4000));
            v.safeRight = FightDodgeSafe(play, p, (s16)(v.yawTo - 0x4000));
            v.safeBack = FightDodgeSafe(play, p, (s16)(v.yawTo + 0x8000));
        }
        // A dodge: a backflip one time in three, else a sidehop -- whichever lands safely.
        auto dodgeMove = [&]() -> FightMove {
            const bool hop = v.safeLeft || v.safeRight;
            if (v.safeBack && (!hop || ((FightRand() % 3) == 0))) {
                return MV_BACKFLIP;
            }
            return hop ? MV_SIDEHOP : MV_NONE;
        };
        if (canDodge && sFight.pendingDodge) {
            m = dodgeMove();
            why = "hit taken";
        } else if (canDodge && FightIncoming(p, t, v)) {
            m = dodgeMove();
            why = "incoming";
        }
        if (m == MV_NONE) {
            why = nullptr;
            m = FightChoose(play, p, t, style, v);
        }
        sFight.pendingDodge = false;
        if (m != MV_NONE) {
            FightStart(m, v, why);
            FightRunSegment(play, p, t, v);
        } else {
            FightApproach(play, p, t, v);
        }
    }
    sFight.zDown = (sH.buttons & BTN_Z) != 0;
}

void AutoFightEnd(const char* why) {
    sH.buttons = 0;
    sH.stickX = sH.stickY = 0;
    gSaveContext.zTargetSetting = sFight.savedZTargetSetting;
    Log("AUTOFIGHT done (%s): %d moves, %d targets, %d hits taken", why, sFight.moves, sFight.targets,
        sFight.hitsTaken);
    sFight.framesLeft = 0;
    sFight.move.clear();
    sFight.target = nullptr;
    TracePlayer();
}

// "autotarget [maxDist]": every enemy and boss loaded, nearest first -- where it is, its health, and whether
// autofight would pick it (or why not). For scouting a take.
void AutoTargetLog(f32 maxDist) {
    PlayState* play = gPlayState;
    Player* p = (play != nullptr) ? GET_PLAYER(play) : nullptr;
    if (p == nullptr) {
        Log("AUTOTARGET not in play");
        return;
    }
    std::vector<std::pair<f32, Actor*>> rows;
    for (s32 cat : { ACTORCAT_ENEMY, ACTORCAT_BOSS }) {
        for (Actor* a = play->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
            rows.push_back({ Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos), a });
        }
    }
    std::sort(rows.begin(), rows.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    Actor* navi = play->actorCtx.targetCtx.arrowPointedActor;
    Log("AUTOTARGET %zu enemies/bosses; player pos=(%.0f,%.0f,%.0f) room=%d form=%d navi=%s0x%03X lock=%s0x%03X",
        rows.size(), p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, play->roomCtx.curRoom.num,
        p->mergeShipForm, (navi != nullptr) ? "" : "-", (navi != nullptr) ? navi->id : 0,
        (p->focusActor != nullptr) ? "" : "-", (p->focusActor != nullptr) ? p->focusActor->id : 0);
    int n = 0;
    for (const auto& row : rows) {
        if (n >= 32) {
            Log("AUTOTARGET ... %zu more", rows.size() - n);
            break;
        }
        Actor* a = row.second;
        const char* why = FightRejects(play, p, a, maxDist);
        Log("AUTOTARGET #%d id=0x%03X %s params=0x%04X pos=(%.0f,%.0f,%.0f) dist=%.0f dy=%.0f hp=%d cyl=%d room=%d "
            "flags=0x%08X los=%d -> %s",
            n, a->id, (a->category == ACTORCAT_BOSS) ? "boss" : "enemy", (u16)a->params, a->world.pos.x,
            a->world.pos.y, a->world.pos.z, row.first, a->world.pos.y - p->actor.world.pos.y, a->colChkInfo.health,
            a->colChkInfo.cylRadius, a->room, a->flags, FightLineOfSight(play, p, a) ? 1 : 0,
            (why != nullptr) ? why : "ok");
        n++;
    }
}

// ---- route / moveto / swimto: following a path, for filming --------------------------------------------------
//
// "route <x1> <z1> [<x2> <z2> ...] <frames> [buttons] [key=value ...]" follows the polyline that starts where the
// player stands and runs through each point, with the stick alone (a world direction converted as walkto converts
// it) and the buttons held all the way: A as a Goron is MM's roll (MergeShip_Goron_Action_96: A held keeps the ball
// rolling, full tilt with magic charges the spikes), A as the Zora in the water MM's fast swim. It steers at the point
// `look` units further along the path than the player's projection onto it (pure pursuit), so a ball or a swimmer that
// turns in an arc keeps to the path through its bends rather than circling a waypoint. It blocks like "wait" until
// the player is within `arrive` of the last point (or past it), or the frames run out, then lets the stick go and
// keeps the buttons held until the next command sets its own: a roll or a fast swim carries on into the next route
// without a frame off. A scene change ends it. Keys: look=<units> (60; swimto 120), arrive=<units> (30),
// mag=<0..100> (stick tilt off the fast swim, 100), magic=1 (the meter kept full: the spiked roll never runs dry).
//
// "swimto" is a route for the Zora's fast swim (MM Player_Action_56), where the stick no longer points the way: stick
// X turns the swimmer -- the yaw changes by (1 - cos(x * 0x10E)) * 1100 a frame (MM func_80850BF8, x the relative
// stick, right turning right) -- and stick Y pitches it, pushed forward diving. So in the fast swim stick X is the
// turn that brings the yaw onto the path, and stick Y holds depth=<units> (60) below the surface (the game keeps a
// fast swimmer at least 46 down unless it is pitched more than 30 degrees up). With leap=<frames> it leaps like a
// dolphin every so many frames of cruising: dives to leapdepth=<units> (140), then pulls up hard until the Zora
// breaches (MM func_8083B3B4 throws it out of the water when it nears the surface pitched more than 30 degrees up;
// MM Player_Action_28 is the flight, with no steering) -- put off while the water is not deep enough to dive for it.
// Off the fast swim (on land, treading water, swimming, the roll into the fast swim) it steers as route does.

struct RouteState {
    bool active = false;
    bool swim = false;     // swimto
    std::vector<f32> x, z; // the path; point 0 is where the player stood when it began
    size_t seg = 0;        // the segment followed: point seg to point seg + 1
    int framesLeft = 0;
    int frames = 0;
    f32 look = 60.0f;
    f32 arrive = 30.0f;
    f32 mag = 100.0f;
    u16 buttons = 0;
    bool magic = false;
    f32 maxOff = 0.0f; // the furthest the player strayed from the path
    // swimto
    f32 depth = 60.0f;
    int leapEvery = 0;
    f32 leapDepth = 140.0f;
    enum { SWIM_CRUISE, SWIM_DIVE, SWIM_PULL, SWIM_AIR } phase = SWIM_CRUISE;
    int phaseFrames = 0;
    int leaps = 0;
};

RouteState sRoute;

// Where on segment seg (0 = its start, 1 = its end) the point (px, pz) projects; *off is its distance from the segment.
f32 RouteProject(size_t seg, f32 px, f32 pz, f32* off) {
    const RouteState& r = sRoute;
    const f32 ax = r.x[seg];
    const f32 az = r.z[seg];
    const f32 dx = r.x[seg + 1] - ax;
    const f32 dz = r.z[seg + 1] - az;
    const f32 len2 = (dx * dx) + (dz * dz);
    const f32 t = (len2 > 0.01f) ? ((((px - ax) * dx) + ((pz - az) * dz)) / len2) : 1.0f;
    const f32 tc = std::clamp(t, 0.0f, 1.0f);
    *off = hypotf(px - (ax + (dx * tc)), pz - (az + (dz * tc)));
    return t;
}

// The point dist further along the path than parameter t of segment seg (the path's end at most).
void RouteAhead(size_t seg, f32 t, f32 dist, f32* outX, f32* outZ) {
    const RouteState& r = sRoute;
    f32 x = r.x[seg] + ((r.x[seg + 1] - r.x[seg]) * t);
    f32 z = r.z[seg] + ((r.z[seg + 1] - r.z[seg]) * t);
    size_t s = seg;
    while (true) {
        const f32 ex = r.x[s + 1];
        const f32 ez = r.z[s + 1];
        const f32 left = hypotf(ex - x, ez - z);
        if ((dist <= left) || ((s + 2) >= r.x.size())) {
            const f32 k = (left > 0.01f) ? std::min(dist / left, 1.0f) : 1.0f;
            *outX = x + ((ex - x) * k);
            *outZ = z + ((ez - z) * k);
            return;
        }
        dist -= left;
        x = ex;
        z = ez;
        s++;
    }
}

void RouteEnd(const char* why) {
    RouteState& r = sRoute;
    r.active = false;
    sH.stickX = sH.stickY = 0; // the buttons stay held until the next command
    Player* p = (gPlayState != nullptr) ? GET_PLAYER(gPlayState) : nullptr;
    if (p != nullptr) {
        Log("ROUTE end (%s) after %d frames at (%.0f,%.0f,%.0f) point %zu of %zu, furthest off the path %.0f, leaps %d",
            why, r.frames, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, r.seg + 1,
            r.x.size() - 1, r.maxOff, r.leaps);
    }
    TracePlayer();
}

// swimto in the fast swim: this frame's stick, or false off the fast swim (the stick then points the way).
bool RouteSwimFrame(Player* p, s16 yawTo) {
    RouteState& r = sRoute;
    if (p->actionFunc == MergeShip_Zora_Action_28) {
        if (r.phase != RouteState::SWIM_AIR) {
            r.phase = RouteState::SWIM_AIR;
            r.leaps++;
            Log("SWIMTO leap %d out of the water at (%.0f,%.0f,%.0f) speed %.1f vy %.1f pitch %d", r.leaps,
                p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, p->linearVelocity,
                p->actor.velocity.y, p->unk_6C2);
        }
        sH.stickX = sH.stickY = 0; // the flight is not steered
        return true;
    }
    if ((p->actionFunc != MergeShip_Zora_Action_56) || (p->av2.actionVar2 != 0) || (p->unk_B86[0] != 0)) {
        return false;
    }
    if (r.phase == RouteState::SWIM_AIR) {
        Log("SWIMTO back in the water at (%.0f,%.0f,%.0f)", p->actor.world.pos.x, p->actor.world.pos.y,
            p->actor.world.pos.z);
        r.phase = RouteState::SWIM_CRUISE;
        r.phaseFrames = 0;
    }
    r.phaseFrames++;
    const f32 depth = p->actor.yDistToWater;
    const f32 water = depth + (p->actor.world.pos.y - p->actor.floorHeight); // surface down to the floor
    switch (r.phase) {
        case RouteState::SWIM_CRUISE:
            if ((r.leapEvery > 0) && (r.phaseFrames >= r.leapEvery)) {
                if (water >= (r.leapDepth + 60.0f)) {
                    r.phase = RouteState::SWIM_DIVE;
                    r.phaseFrames = 0;
                } else if ((r.phaseFrames % 20) == 0) {
                    Log("SWIMTO leap put off: the water is %.0f deep at (%.0f,%.0f)", water, p->actor.world.pos.x,
                        p->actor.world.pos.z);
                }
            }
            break;
        case RouteState::SWIM_DIVE:
            if ((depth >= r.leapDepth) || ((depth + 60.0f) >= water) || (r.phaseFrames > 45)) {
                r.phase = RouteState::SWIM_PULL;
                r.phaseFrames = 0;
            }
            break;
        case RouteState::SWIM_PULL:
            if (r.phaseFrames > 60) {
                Log("SWIMTO no breach after pulling up 60 frames (depth %.0f pitch %d)", depth, p->unk_6C2);
                r.phase = RouteState::SWIM_CRUISE;
                r.phaseFrames = 0;
            }
            break;
        default:
            break;
    }
    f32 relY;
    if (r.phase == RouteState::SWIM_DIVE) {
        relY = 60.0f;
    } else if (r.phase == RouteState::SWIM_PULL) {
        relY = -60.0f;
    } else {
        // Hold the depth: pitched down (stick forward) when too shallow, up when too deep, damped by the rate the
        // depth changes (velocity.y up is the depth shrinking). The pitch follows the stick (Y * 0xC8) smoothly.
        const f32 floorRoom = water - 50.0f; // never steer into the floor
        const f32 want = std::min(r.depth, std::max(floorRoom, 0.0f));
        relY = std::clamp(((want - depth) * 0.6f) + (p->actor.velocity.y * 4.0f), -40.0f, 40.0f);
    }
    // The yaw change wanted this frame, a share of the way onto the path; the stick X that makes it.
    const s16 err = (s16)(yawTo - p->yaw);
    const f32 turn = std::clamp((f32)err * 0.3f, -1050.0f, 1050.0f);
    f32 relX = 0.0f;
    if (fabsf(turn) >= 1.0f) {
        const f32 c = std::clamp(1.0f - (fabsf(turn) / 1100.0f), -1.0f, 1.0f);
        relX = std::min(acosf(c) * (65536.0f / (2.0f * 3.14159265f * 270.0f)), 60.0f);
        if (turn > 0.0f) {
            relX = -relX; // stick X to the right turns right (the yaw goes down)
        }
    }
    sH.stickX = StickRawFromRel(relX);
    sH.stickY = StickRawFromRel(relY);
    return true;
}

// One frame of route / moveto / swimto: picks the segment, ends at the last point, steers.
void RouteFrame() {
    RouteState& r = sRoute;
    Player* p = GET_PLAYER(gPlayState);
    if (p == nullptr) {
        RouteEnd("no player");
        return;
    }
    const f32 px = p->actor.world.pos.x;
    const f32 pz = p->actor.world.pos.z;
    const size_t last = r.x.size() - 1;
    f32 off = 0.0f;
    f32 t = RouteProject(r.seg, px, pz, &off);
    while ((r.seg + 1) < last) {
        // On to the next segment once past this one's end, or close to it.
        if ((t < 1.0f) && (hypotf(r.x[r.seg + 1] - px, r.z[r.seg + 1] - pz) > r.arrive)) {
            break;
        }
        r.seg++;
        Log("ROUTE point %zu passed at (%.0f,%.0f,%.0f) frame %d speed %.1f", r.seg, px, p->actor.world.pos.y, pz,
            r.frames, p->linearVelocity);
        t = RouteProject(r.seg, px, pz, &off);
    }
    r.maxOff = std::max(r.maxOff, off);
    if (((r.seg + 1) == last) && ((hypotf(r.x[last] - px, r.z[last] - pz) <= r.arrive) || (t >= 1.0f))) {
        RouteEnd("arrived");
        return;
    }
    f32 aimX;
    f32 aimZ;
    RouteAhead(r.seg, std::clamp(t, 0.0f, 1.0f), r.look, &aimX, &aimZ);
    Vec3f aim = { aimX, p->actor.world.pos.y, aimZ };
    const s16 yawTo = Math_Vec3f_Yaw(&p->actor.world.pos, &aim);
    sH.buttons = r.buttons;
    if (r.magic) {
        FightTopUpMagic();
    }
    if (!r.swim || !RouteSwimFrame(p, yawTo)) {
        StickTowardYaw(yawTo, r.mag);
    }
    if ((r.frames % 10) == 0) {
        Log("ROUTE f=%d seg=%zu pos=(%.0f,%.0f,%.0f) off=%.0f aim=(%.0f,%.0f) yaw=%d spd=%.1f depth=%.0f act=%s", r.frames,
            r.seg, px, p->actor.world.pos.y, pz, off, aimX, aimZ, p->yaw, p->linearVelocity, p->actor.yDistToWater,
            SymbolName((void*)p->actionFunc).c_str());
    }
    r.frames++;
}

// route / moveto / swimto: parse, start, and decide the first frame. False when it does not start.
bool RouteStart(const std::vector<std::string>& args, const std::string& op, int line) {
    RouteState r;
    r.swim = (op == "swimto");
    r.look = r.swim ? 120.0f : 60.0f;
    std::vector<f32> nums;
    size_t i = 0;
    for (; i < args.size(); i++) {
        const char* s = args[i].c_str();
        char* end = nullptr;
        const f32 v = strtof(s, &end);
        if ((end == s) || (*end != '\0')) {
            break;
        }
        nums.push_back(v);
    }
    int frames = 0;
    if (op == "moveto") {
        // moveto <x> <z> <frames> [buttons] [arriveDist]
        if (nums.size() < 3) {
            Log("WARN line %d: moveto <x> <z> <frames> [buttons] [arriveDist]", line);
            return false;
        }
        r.x.push_back(nums[0]);
        r.z.push_back(nums[1]);
        frames = (int)nums[2];
        if (nums.size() > 3) {
            r.arrive = nums[3];
        } else if (i < args.size()) {
            r.buttons = ParseButtons(args[i]);
            if ((i + 1) < args.size()) {
                r.arrive = (f32)atof(args[i + 1].c_str());
            }
        }
    } else {
        if ((nums.size() < 3) || ((nums.size() % 2) == 0)) {
            Log("WARN line %d: %s <x1> <z1> [<x2> <z2> ...] <frames> [buttons] [key=value ...]", line, op.c_str());
            return false;
        }
        for (size_t k = 0; (k + 1) < nums.size(); k += 2) {
            r.x.push_back(nums[k]);
            r.z.push_back(nums[k + 1]);
        }
        frames = (int)nums.back();
        for (; i < args.size(); i++) {
            const std::string& a = args[i];
            const size_t eq = a.find('=');
            if (eq == std::string::npos) {
                r.buttons = ParseButtons(a);
                continue;
            }
            const std::string key = a.substr(0, eq);
            const f32 v = (f32)atof(a.c_str() + eq + 1);
            if (key == "look") {
                r.look = std::max(v, 1.0f);
            } else if (key == "arrive") {
                r.arrive = v;
            } else if (key == "mag") {
                r.mag = std::clamp(v, 0.0f, 100.0f);
            } else if (key == "magic") {
                r.magic = (v != 0.0f);
            } else if (key == "depth") {
                r.depth = v;
            } else if (key == "leap") {
                r.leapEvery = (int)v;
            } else if (key == "leapdepth") {
                r.leapDepth = v;
            } else {
                Log("WARN line %d: %s key '%s' unknown (look arrive mag magic depth leap leapdepth)", line, op.c_str(),
                    key.c_str());
            }
        }
    }
    // (In play: a transition under way would end it on its first frame -- waitplay first.)
    if ((frames <= 0) || !InPlay()) {
        Log("%s skipped: %s", op.c_str(), (frames <= 0) ? "no frames" : "not in play");
        return false;
    }
    Player* p = GET_PLAYER(gPlayState);
    r.x.insert(r.x.begin(), p->actor.world.pos.x);
    r.z.insert(r.z.begin(), p->actor.world.pos.z);
    r.framesLeft = frames;
    r.active = true;
    sRoute = r;
    std::string pts;
    for (size_t k = 1; k < sRoute.x.size(); k++) {
        char buf[48];
        snprintf(buf, sizeof(buf), " (%.0f,%.0f)", sRoute.x[k], sRoute.z[k]);
        pts += buf;
    }
    Log("ROUTE %s from (%.0f,%.0f,%.0f) through%s in %d frames buttons=0x%04X look=%.0f arrive=%.0f mag=%.0f magic=%d"
        " depth=%.0f leap=%d leapdepth=%.0f form=%d",
        op.c_str(), p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, pts.c_str(), frames,
        sRoute.buttons, sRoute.look, sRoute.arrive, sRoute.mag, sRoute.magic ? 1 : 0, sRoute.depth, sRoute.leapEvery,
        sRoute.leapDepth, p->mergeShipForm);
    RouteFrame();
    return sRoute.active;
}

// Runs commands until one consumes frames. Returns when the current frame's input is decided.

// "cinecam" (include/mergeship/CineCam.hpp): the film camera, a sub camera of the harness's own placed each frame.
MsCineCam sCine;
s16 sCineCamId = SUBCAM_NONE;
bool sHudOff = false; // "hud off": the HUD hidden (as the game hides it for cutscenes), every frame until "hud on"

void UpdateCineCam() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        sCineCamId = SUBCAM_NONE;
        return;
    }
    if (sHudOff) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
    }
    const bool alive = (sCineCamId != SUBCAM_NONE) && (play->cameraPtrs[sCineCamId] != nullptr);
    if (sCine.mode == MsCineCam::OFF) {
        if (alive) {
            Play_ClearCamera(play, sCineCamId);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
        }
        sCineCamId = SUBCAM_NONE;
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return;
    }
    if (!alive) {
        sCineCamId = Play_CreateSubCamera(play);
        if (sCineCamId == SUBCAM_NONE) {
            return;
        }
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, sCineCamId, CAM_STAT_ACTIVE);
        sCine.primed = false;
    }
    float at[3];
    float eye[3];
    sCine.Update(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z,
                 player->actor.shape.rot.y, at, eye);
    Vec3f vAt = { at[0], at[1], at[2] };
    Vec3f vEye = { eye[0], eye[1], eye[2] };
    Play_CameraSetAtEye(play, sCineCamId, &vAt, &vEye);
}

void Step() {
    if (sH.steerFrames > 0) {
        if (gPlayState == nullptr || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
            Log("STEER stopped: transition started (door %s)", MergeShipHost_DoorState());
            sH.steerFrames = 0;
            sH.stickX = sH.stickY = 0;
        } else {
            Steer();
            if (--sH.steerFrames > 0) {
                return;
            }
            sH.stickX = sH.stickY = 0;
            Log("STEER done");
            TracePlayer();
        }
    }
    if (sRoute.active) {
        // route / moveto / swimto: blocks the script like "wait"; a scene change ends it.
        if ((gPlayState == nullptr) || (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF)) {
            Log("ROUTE stopped: transition started (door %s)", MergeShipHost_DoorState());
            sRoute.active = false;
            sH.stickX = sH.stickY = 0;
        } else if (--sRoute.framesLeft <= 0) {
            RouteEnd("time");
        } else {
            RouteFrame();
            if (sRoute.active) {
                return;
            }
        }
    }
    if (sFight.framesLeft > 0) {
        // autofight: blocks the script like "wait"; a scene change (a loading zone run into) ends it.
        if ((gPlayState == nullptr) || (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF)) {
            AutoFightEnd("transition started");
        } else if (--sFight.framesLeft > 0) {
            AutoFightFrame();
            return;
        } else {
            AutoFightEnd("time");
        }
    }
    if (sH.pressRelease) {
        sH.pressRelease = false;
        sH.buttons = 0;
        return;
    }
    if (sH.waitingForPlay) {
        if (!InPlay()) {
            return;
        }
        if (sH.afterPlayFrames-- > 0) {
            return;
        }
        sH.waitingForPlay = false;
        Log("PLAY ready scene=%d room=%d entrance=0x%X door=%s", gPlayState->sceneNum,
            gPlayState->roomCtx.curRoom.num, gSaveContext.entranceIndex, MergeShipHost_DoorState());
    }
    if (sH.waitFrames > 0) {
        sH.waitFrames--;
        if (sH.waitFrames > 0) {
            return;
        }
        sH.buttons = 0;
        sH.stickX = sH.stickY = 0;
    }

    while (sH.pc < sH.commands.size()) {
        const Command& c = sH.commands[sH.pc++];
        auto arg = [&](size_t i) -> std::string { return i < c.args.size() ? c.args[i] : std::string(); };

        if (c.op == "boot") {
            // Handled at the title screen; nothing to do once play is running.
            continue;
        } else if (c.op == "waitplay") {
            sH.waitingForPlay = true;
            sH.afterPlayFrames = c.args.empty() ? 0 : (int)ParseInt(arg(0));
            return;
        } else if (c.op == "wait") {
            sH.buttons = 0;
            sH.stickX = sH.stickY = 0;
            sH.waitFrames = (int)ParseInt(arg(0));
            return;
        } else if (c.op == "hold") {
            sH.buttons = ParseButtons(arg(0));
            sH.stickX = sH.stickY = 0;
            sH.waitFrames = (int)ParseInt(arg(1));
            return;
        } else if (c.op == "stick") {
            sH.stickX = (s8)ParseInt(arg(0));
            sH.stickY = (s8)ParseInt(arg(1));
            sH.waitFrames = (int)ParseInt(arg(2));
            sH.buttons = ParseButtons(arg(3));
            return;
        } else if (c.op == "latch") {
            // latch <buttons>: hold them on every frame from now on (never dropped between commands), until unlatch.
            sH.latched |= ParseButtons(arg(0));
            Log("LATCH 0x%04X", sH.latched);
        } else if (c.op == "unlatch") {
            sH.latched = arg(0).empty() ? 0 : (u16)(sH.latched & ~ParseButtons(arg(0)));
            Log("UNLATCH -> 0x%04X", sH.latched);
        } else if (c.op == "press") {
            sH.buttons = ParseButtons(arg(0));
            sH.stickX = sH.stickY = 0;
            sH.pressRelease = true;
            return;
        } else if (c.op == "form") {
            // Debug only: bypasses the mask, to test a form's abilities in isolation.
            MergeShip_DebugSetForm((u8)ParseInt(arg(0)));
            Log("FORM debug %s", arg(0).c_str());
        } else if (c.op == "givemask") {
            // MM form order: 0 Fierce Deity, 1 Goron, 2 Zora, 3 Deku.
            MergeShip_GiveTransformationMask((u8)ParseInt(arg(0)));
            Log("GIVEMASK %s", arg(0).c_str());
        } else if (c.op == "givemmmask") {
            if (arg(0) == "all") {
                for (int m = MM_MASK_TRUTH; m < MM_MASK_MAX; m++) {
                    MergeShip_GiveMMMask((u8)m);
                }
            } else {
                MergeShip_GiveMMMask((u8)ParseInt(arg(0)));
            }
            Log("GIVEMMMASK %s owned=0x%08X", arg(0).c_str(), gMergeShipSave.ownedMMMasks);
        } else if (c.op == "give") {
            if (gPlayState != nullptr) {
                Item_Give(gPlayState, (u8)ParseInt(arg(0)));
            }
            Log("GIVE %s", arg(0).c_str());
        } else if (c.op == "rgive") {
            RandomizerGet rg = ParseRandoGet(arg(0));
            s32 ok = 0;
            if (gPlayState != nullptr) {
                ok = GiveItemEntryWithoutActor(gPlayState, Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy());
            }
            Log("RGIVE %d ok=%d owned=0x%02X", (int)rg, ok, gMergeShipSave.ownedMasks);
        } else if (c.op == "rspawn") {
            RandomizerGet rg = ParseRandoGet(arg(0));
            Actor* a = nullptr;
            if (gPlayState != nullptr) {
                a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_ITEM00, (f32)atof(arg(1).c_str()),
                                (f32)atof(arg(2).c_str()), (f32)atof(arg(3).c_str()), 0, 0, 0, ITEM00_RUPEE_GREEN);
                if (a != nullptr) {
                    EnItem00* item00 = (EnItem00*)a;
                    item00->itemEntry = Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy();
                    item00->actor.params = ITEM00_SOH_DUMMY;
                    item00->actor.draw = (ActorFunc)EnItem00_DrawRandomizedItem;
                }
            }
            Log("RSPAWN %d actor=%p", (int)rg, (void*)a);
        } else if (c.op == "rrow") {
            // rrow <dist> <rg...>: the items side by side, dist in front of the player.
            s32 n = (s32)c.args.size() - 1;
            s32 spawned = 0;
            if ((gPlayState != nullptr) && (n > 0)) {
                Player* p = GET_PLAYER(gPlayState);
                f32 dist = (f32)atof(arg(0).c_str());
                s16 yaw = p->actor.shape.rot.y;
                for (s32 i = 0; i < n; i++) {
                    RandomizerGet rg = ParseRandoGet(c.args[i + 1]);
                    f32 side = (i - (n - 1) / 2.0f) * 45.0f;
                    f32 x = p->actor.world.pos.x + Math_SinS(yaw) * dist + Math_CosS(yaw) * side;
                    f32 z = p->actor.world.pos.z + Math_CosS(yaw) * dist - Math_SinS(yaw) * side;
                    Actor* a = SpawnRandoItem00(Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy(), x,
                                                p->actor.world.pos.y + 10.0f, z, RC_UNKNOWN_CHECK);
                    spawned += (a != nullptr) ? 1 : 0;
                    Log("RROW %s rg=%d terminaRow=%d", c.args[i + 1].c_str(), (int)rg, MergeShip_TerminaGi_Has(rg));
                }
            }
            Log("RROW spawned %d of %d", spawned, n);
        } else if (c.op == "ritemdraw") {
            // ritemdraw [rg...]: how each item's get-item model is drawn. No argument: every Majora's Mask item
            // (RG_MM_*, RG_PROGRESSIVE_MM_*, RG_PROGRESSIVE_GORON_LULLABY), and how many have no model of their own.
            std::vector<RandomizerGet> rgs;
            for (const std::string& a : c.args) {
                rgs.push_back(ParseRandoGet(a));
            }
            if (rgs.empty()) {
                for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
                    std::string_view name = EnumToString((RandomizerGet)rg);
                    if ((name.rfind("RG_MM_", 0) == 0) || (name.find("_MM_") != std::string_view::npos) ||
                        (name == "RG_PROGRESSIVE_GORON_LULLABY")) {
                        rgs.push_back((RandomizerGet)rg);
                    }
                }
            }
            s32 fallbacks = 0;
            for (RandomizerGet rg : rgs) {
                if ((rg <= RG_NONE) || (rg >= RG_MAX)) {
                    Log("RITEMDRAW %d: not an item", (int)rg);
                    continue;
                }
                GetItemEntry e = Rando::StaticData::RetrieveItem(rg).GetGIEntry_Copy();
                const bool terminaDraw = (e.drawFunc == (CustomDrawFunc)MergeShip_DrawGiTerminaItem);
                const bool terminaRow = MergeShip_TerminaGi_Has(rg) != 0;
                // A Majora's Mask item with no draw function of its own shows its placeholder OoT gid: a fallback.
                const bool fallback = (e.drawFunc == nullptr) || (terminaDraw && !terminaRow);
                fallbacks += fallback ? 1 : 0;
                Log("RITEMDRAW %s rg=%d gid=%d object=%d drawFunc=%s terminaRow=%d%s", EnumToString(rg).data(),
                    (int)rg, (int)e.gid, (int)e.objectId,
                    (e.drawFunc == nullptr) ? "none" : (terminaDraw ? "MergeShip_DrawGiTerminaItem" : "custom"),
                    terminaRow ? 1 : 0, fallback ? " FALLBACK" : "");
            }
            Log("RITEMDRAW %d items, %d falling back", (int)rgs.size(), fallbacks);
        } else if (c.op == "randofile") {
            // randofile: after randogen, the debug file plays that seed (the checks, shops and drops read the
            // generated context). For this run only: the harness never saves the file.
            gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
            ShipInit::Init("IS_RANDO"); // the hooks a randomizer file loads with (its get-item texts and icons)
            Log("RANDOFILE IS_RANDO=%d shopsanity=%d", IS_RANDO ? 1 : 0,
                (int)Rando::Context::GetInstance()->GetOption(RSK_SHOPSANITY).Get());
        } else if (c.op == "rplace") {
            // rplace <rc> <rg>: check rc holds rg and is not found yet.
            RandomizerCheck rc = ParseRandoCheck(arg(0));
            RandomizerGet rg = ParseRandoGet(arg(1));
            Rando::ItemLocation* loc = ((rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX))
                                           ? Rando::Context::GetInstance()->GetItemLocation(rc)
                                           : nullptr;
            if ((loc != nullptr) && (rg > RG_NONE) && (rg < RG_MAX)) {
                loc->SetPlacedItem(rg);
                loc->SetCheckStatus(RCSHOW_UNCHECKED);
                const auto& check = Rando::StaticData::GetLocation(rc)->GetCollectionCheck();
                if (check.type == SPOILER_CHK_RANDOMIZER_INF) {
                    Flags_UnsetRandomizerInf((RandomizerInf)check.flag);
                }
            }
            Log("RPLACE %s <- %s: %s", arg(0).c_str(), arg(1).c_str(), (loc != nullptr) ? "ok" : "failed");
        } else if (c.op == "rdrop") {
            // rdrop <rc> [dist]: check rc's item dropped in front of the player, as a pot, grass or beehive drops it.
            RandomizerCheck rc = ParseRandoCheck(arg(0));
            Actor* a = nullptr;
            if ((gPlayState != nullptr) && (rc > RC_UNKNOWN_CHECK) && (rc < RC_MAX)) {
                Player* p = GET_PLAYER(gPlayState);
                f32 dist = arg(1).empty() ? 80.0f : (f32)atof(arg(1).c_str());
                s16 yaw = p->actor.shape.rot.y;
                GetItemEntry entry = Rando::Context::GetInstance()->GetFinalGIEntry(rc, true);
                a = SpawnRandoItem00(entry, p->actor.world.pos.x + Math_SinS(yaw) * dist, p->actor.world.pos.y,
                                     p->actor.world.pos.z + Math_CosS(yaw) * dist, rc);
            }
            Log("RDROP %s -> %s", arg(0).c_str(), (a != nullptr) ? "ok" : "failed");
        } else if (c.op == "camc") {
            // camc [item ...]: each item's Containers Match Contents category, from its item table entry and as the
            // containers draw it (Randomizer_AdjustItemCategory: masks, stray fairies, owned bottles...). No
            // argument: every RandomizerGet. 0 junk 1 lesser 2 health 3 boss key 4 small key 5 token 6 major 7 mask
            // 8 stray fairy.
            std::vector<RandomizerGet> rgs;
            for (const std::string& a : c.args) {
                rgs.push_back(ParseRandoGet(a));
            }
            if (rgs.empty()) {
                for (int rg = RG_NONE + 1; rg < RG_MAX; rg++) {
                    rgs.push_back((RandomizerGet)rg);
                }
            }
            for (RandomizerGet rg : rgs) {
                Rando::Item& item = Rando::StaticData::RetrieveItem(rg);
                GetItemEntry gie = item.GetGIEntry_Copy();
                Log("CAMC %d \"%s\" table=%d adjusted=%d", (int)rg, item.GetName().GetEnglish().c_str(),
                    (int)gie.getItemCategory, (int)Randomizer_AdjustItemCategory(gie));
            }
        } else if (c.op == "giveall") {
            GiveAll();
            Log("GIVEALL items=%02X.. quest=0x%08X equipment=0x%04X upgrades=0x%06X tokens=%d trade=0x%02X/0x%02X",
                gSaveContext.inventory.items[0], gSaveContext.inventory.questItems, gSaveContext.inventory.equipment,
                gSaveContext.inventory.upgrades, gSaveContext.inventory.gsTokens, INV_CONTENT(ITEM_TRADE_CHILD),
                INV_CONTENT(ITEM_TRADE_ADULT));
        } else if (c.op == "shared") {
            MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
#ifdef MERGESHIP_HOSTED
            if (shared != nullptr) {
                MergeShipShared_Capture(shared); // soh.exe has no shared save (MergeShipHost_SharedSave is NULL)
            }
#endif
            LogShared(shared);
        } else if (c.op == "masks") {
            Log("MASKS owned=0x%02X tradeChild=0x%02X form=%d mmMasks=0x%06X mmEquip=0x%02X dpad=%02X,%02X,%02X,%02X",
                gMergeShipSave.ownedMasks, INV_CONTENT(ITEM_TRADE_CHILD),
                (gPlayState != nullptr) ? GET_PLAYER(gPlayState)->mergeShipForm : -1,
                (unsigned)gMergeShipSave.ownedMMMasks, (unsigned)gMergeShipSave.mmEquipOwned,
                gSaveContext.equips.buttonItems[4], gSaveContext.equips.buttonItems[5],
                gSaveContext.equips.buttonItems[6], gSaveContext.equips.buttonItems[7]);
        } else if (c.op == "randogen") {
            Rando::Settings::GetInstance()->SetAllToContext();
            bool ok = GenerateRandomizer({}, {}, arg(0));
            Log("RANDOGEN seed=%s ok=%d spoiler=%s", arg(0).c_str(), (int)ok,
                CVarGetString(CVAR_GENERAL("SpoilerLog"), ""));
            LogRandoPlacements();
        } else if (c.op == "randoterm") {
            for (const std::string& line : MergeShip_Termina_Summary()) {
                Log("RANDOTERM %s", line.c_str());
            }
        } else if (c.op == "randogoal") {
            for (const std::string& line : MergeShip_Goal_Summary()) {
                Log("RANDOGOAL %s", line.c_str());
            }
        } else if (c.op == "goal") {
            for (const std::string& line : MergeShip_Goal_TestCommand(c.args)) {
                Log("GOAL %s", line.c_str());
            }
        } else if (c.op == "cbutton") {
            // cbutton <button> <item> [slot]: the slot as the pause screen records it (0xFF: SLOT_NONE, as the Masks
            // page equips a mask -- it has no inventory slot).
            s32 slot = (s32)ParseInt(arg(0));
            u8 item = (u8)ParseInt(arg(1));
            gSaveContext.equips.buttonItems[slot] = item;
            if (!arg(2).empty() && (slot >= 1)) {
                gSaveContext.equips.cButtonSlots[slot - 1] = (u8)ParseInt(arg(2));
            }
            if (gPlayState != nullptr) {
                Interface_LoadItemIcon1(gPlayState, slot);
            }
            Log("CBUTTON %d = 0x%02X", slot, item);
        } else if (c.op == "pos") {
            if (gPlayState != nullptr) {
                Player* p = GET_PLAYER(gPlayState);
                p->actor.world.pos.x = p->actor.home.pos.x = (f32)atof(arg(0).c_str());
                p->actor.world.pos.y = p->actor.home.pos.y = (f32)atof(arg(1).c_str());
                p->actor.world.pos.z = p->actor.home.pos.z = (f32)atof(arg(2).c_str());
                if (!arg(3).empty()) {
                    p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)ParseInt(arg(3));
                }
            }
            Log("POS %s %s %s", arg(0).c_str(), arg(1).c_str(), arg(2).c_str());
        } else if (c.op == "door") {
            // Stand in front of the nearest EnDoor (arg: distance, default 45), facing it.
            Actor* best = nullptr;
            f32 bestDist = 1e9f;
            if (gPlayState != nullptr) {
                Player* p = GET_PLAYER(gPlayState);
                for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_DOOR].head; a != nullptr; a = a->next) {
                    if (a->id != ACTOR_EN_DOOR) {
                        continue;
                    }
                    f32 d = Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos);
                    if (d < bestDist) {
                        bestDist = d;
                        best = a;
                    }
                }
                if (best != nullptr) {
                    f32 dist = arg(0).empty() ? 45.0f : (f32)atof(arg(0).c_str());
                    s16 yaw = best->shape.rot.y;
                    p->actor.world.pos.x = p->actor.home.pos.x = best->world.pos.x + Math_SinS(yaw) * dist;
                    p->actor.world.pos.y = p->actor.home.pos.y = best->world.pos.y;
                    p->actor.world.pos.z = p->actor.home.pos.z = best->world.pos.z + Math_CosS(yaw) * dist;
                    p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)(yaw + 0x8000);
                }
            }
            Log("DOOR %s at %.1f", best ? "found" : "none", bestDist);
        } else if ((c.op == "infront") || (c.op == "actors")) {
            s16 id = (s16)ParseInt(arg(0));
            int want = (c.op == "infront" && !arg(2).empty()) ? (int)ParseInt(arg(2)) : 0;
            int n = 0;
            Actor* chosen = nullptr;
            if (gPlayState != nullptr) {
                Player* p = GET_PLAYER(gPlayState);
                for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
                    for (Actor* a = gPlayState->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
                        if (a->id != id) {
                            continue;
                        }
                        Log("ACTOR id=0x%03X #%d params=0x%04X pos=(%.1f,%.1f,%.1f) yaw=%d dist=%.1f hp=%d", id, n,
                            (u16)a->params, a->world.pos.x, a->world.pos.y, a->world.pos.z, a->shape.rot.y,
                            Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos), a->colChkInfo.health);
                        if (n == want) {
                            chosen = a;
                        }
                        n++;
                    }
                }
                if ((c.op == "infront") && (chosen != nullptr)) {
                    f32 dist = arg(1).empty() ? 120.0f : (f32)atof(arg(1).c_str());
                    s16 yaw = chosen->shape.rot.y;
                    p->actor.world.pos.x = p->actor.home.pos.x = chosen->world.pos.x + Math_SinS(yaw) * dist;
                    p->actor.world.pos.y = p->actor.home.pos.y = chosen->world.pos.y;
                    p->actor.world.pos.z = p->actor.home.pos.z = chosen->world.pos.z + Math_CosS(yaw) * dist;
                    p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)(yaw + 0x8000);
                }
            }
            Log("%s 0x%03X: %d found%s", c.op.c_str(), id, n, (chosen != nullptr) ? "" : " (none chosen)");
        } else if (c.op == "warp") {
            // Change scene to an entrance (same age); follow with "waitplay".
            if (gPlayState != nullptr) {
                gPlayState->nextEntranceIndex = (s16)ParseInt(arg(0));
                gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                gPlayState->transitionType = TRANS_TYPE_INSTANT;
            }
            Log("WARP %s", arg(0).c_str());
        } else if (c.op == "health") {
            gSaveContext.health = (s16)ParseInt(arg(0));
        } else if (c.op == "strip") {
            // strip: the debug file owns nothing -- no item, ocarina, sword, shield (Kokiri tunic and boots only), no
            // upgrade, quest item, song, mask, MM equipment or magic meter (build/tests/door_no_free_gear).
            for (s32 slot = 0; slot < ARRAY_COUNT(gSaveContext.inventory.items); slot++) {
                gSaveContext.inventory.items[slot] = ITEM_NONE;
            }
            for (s32 slot = 0; slot < ARRAY_COUNT(gSaveContext.inventory.ammo); slot++) {
                gSaveContext.inventory.ammo[slot] = 0;
            }
            gSaveContext.inventory.equipment = (u16)(OWNED_EQUIP_FLAG(EQUIP_TYPE_TUNIC, EQUIP_INV_TUNIC_KOKIRI) |
                                                     OWNED_EQUIP_FLAG(EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_KOKIRI));
            gSaveContext.equips.equipment = 0;
            Inventory_ChangeEquipment(EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI);
            Inventory_ChangeEquipment(EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI);
            for (s32 i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
                gSaveContext.equips.buttonItems[i] = ITEM_NONE;
            }
            for (s32 i = 0; i < ARRAY_COUNT(gSaveContext.equips.cButtonSlots); i++) {
                gSaveContext.equips.cButtonSlots[i] = SLOT_NONE;
            }
            gSaveContext.inventory.upgrades = 0;
            gSaveContext.inventory.questItems = 0;
            gSaveContext.isMagicAcquired = false;
            gSaveContext.isDoubleMagicAcquired = false;
            gSaveContext.magicLevel = 0;
            gSaveContext.magicCapacity = 0;
            gSaveContext.magic = 0;
            gSaveContext.isDoubleDefenseAcquired = false;
            gSaveContext.inventory.defenseHearts = 0;
            gSaveContext.healthCapacity = gSaveContext.health = 0x30;
            gSaveContext.rupees = 0;
            gMergeShipSave.ownedMasks = 0;
            gMergeShipSave.ownedMMMasks = 0;
            gMergeShipSave.mmEquipOwned = 0;
            gMergeShipSave.mmEquipSlotOnlyMM = 0;
            gMergeShipSave.mmSongs = 0;
            gMergeShipSave.mmQuestItems = 0;
            Flags_SetInfTable(INFTABLE_SWORDLESS);
            if (gPlayState != nullptr) {
                Interface_LoadItemIcon1(gPlayState, 0);
                Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
            }
            Log("STRIP items=%02X.. equipment=0x%04X equips=0x%04X upgrades=0x%06X magic=%d/%d",
                gSaveContext.inventory.items[0], gSaveContext.inventory.equipment, gSaveContext.equips.equipment,
                gSaveContext.inventory.upgrades, gSaveContext.isMagicAcquired, gSaveContext.magic);
        } else if (c.op == "gear") {
            // gear: what Hyrule holds of the items both games have (door_no_free_gear): ocarina, bow, hookshot slots,
            // swords and shields owned and in use, MM's equipment (owned / slots held only for it), magic.
            Log("GEAR ocarina=0x%02X bow=0x%02X hookshot=0x%02X equipment=0x%04X equips=0x%04X B=0x%02X "
                "mmEquipOwned=0x%02X mmSlotOnlyMM=0x%02X magic=%d/%d level=%d amount=%d",
                INV_CONTENT(ITEM_OCARINA_FAIRY), INV_CONTENT(ITEM_BOW), INV_CONTENT(ITEM_HOOKSHOT),
                gSaveContext.inventory.equipment, gSaveContext.equips.equipment, gSaveContext.equips.buttonItems[0],
                gMergeShipSave.mmEquipOwned, gMergeShipSave.mmEquipSlotOnlyMM, gSaveContext.isMagicAcquired,
                gSaveContext.isDoubleMagicAcquired, gSaveContext.magicLevel, gSaveContext.magic);
        } else if (c.op == "fileslot") {
            // fileslot <0-2>: the debug file plays as that save slot -- Anchor (Anchor::IsSaveLoaded) and the shared
            // save (its ownership, the one ammo pool, the Bombchu Bag's lock) only act for a real slot. Claims the
            // slot's shared save in the run's own folder (run_final.sh's private copy, never the player's saves).
            gSaveContext.fileNum = (s32)ParseInt(arg(0));
#ifdef MERGESHIP_HOSTED
            MergeShipHost_ClaimSharedSave(gSaveContext.fileNum);
#endif
            Log("FILESLOT %d", gSaveContext.fileNum);
        } else if (c.op == "bstate") {
            // bstate: the B button and the sword equipment (BUG-0046).
            Log("BSTATE B=0x%02X status=0x%02X swordEquip=%d owned=0x%04X mmOwned=0x%02X mmSel=0x%02X swordless=%d",
                gSaveContext.equips.buttonItems[0], gSaveContext.buttonStatus[0],
                CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), gSaveContext.inventory.equipment, gMergeShipSave.mmEquipOwned,
                gMergeShipSave.mmEquipSelected, Flags_GetInfTable(INFTABLE_SWORDLESS));
        } else if (c.op == "warpcs") {
            // warpcs <entrance> <cutsceneIndex>: change scene into an entrance cutscene (e.g. 0x0A0 0xFFF8, Zelda's
            // farewell); follow with "wait", not "waitplay".
            if (gPlayState != nullptr) {
                gPlayState->nextEntranceIndex = (s16)ParseInt(arg(0));
                gSaveContext.nextCutsceneIndex = (u16)ParseInt(arg(1));
                gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                gPlayState->transitionType = TRANS_TYPE_INSTANT;
            }
            Log("WARPCS %s %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "spstate") {
            // spstate <ootBeaten> <doorDone>: the singleplayer post-game flags.
            MergeShip_Singleplayer_DebugSetState((s32)ParseInt(arg(0)), (s32)ParseInt(arg(1)));
            Log("SPSTATE beaten=%d doorDone=%d locked=%d pending=%d", MergeShip_Singleplayer_OotBeaten(),
                (s32)ParseInt(arg(1)), MergeShip_Singleplayer_TerminaLocked(), MergeShip_Singleplayer_DoorPending());
        } else if (c.op == "spclock") {
            // spclock <frames>: the traveling salesman's schedule clock (singleplayer); then where that puts him.
            MergeShip_Singleplayer_SetClock((u32)ParseInt(arg(0)));
            MergeShip_Singleplayer_LogWhere();
        } else if (c.op == "spfind") {
            // spfind <sceneNum> [minOffset] [arriving]: set the salesman's clock to when he next walks through that
            // scene (then where that puts him).
            u32 clk = MergeShip_Singleplayer_FindTravel((s16)ParseInt(arg(0)), arg(1).empty() ? 0.0f : (f32)atof(arg(1).c_str()),
                                                       arg(2).empty() ? 0 : (s32)ParseInt(arg(2)));
            Log("SPFIND scene %s -> clock %u", arg(0).c_str(), clk);
            if (clk != 0) {
                MergeShip_Singleplayer_SetClock(clk);
            }
            MergeShip_Singleplayer_LogWhere();
        } else if (c.op == "spwhere") {
            MergeShip_Singleplayer_LogWhere();
        } else if (c.op == "spnear") {
            // spnear [dist]: Link stands dist (default 90) in front of the traveling salesman, facing him; logs where
            // he is (singleplayer, ovl_MM_En_Osn_Travel).
            Actor* osn = nullptr;
            if ((gPlayState != nullptr) && (gMergeShipActorId_MM_En_Osn_Travel >= 0)) {
                for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_NPC].head; a != nullptr; a = a->next) {
                    if (a->id == gMergeShipActorId_MM_En_Osn_Travel) {
                        osn = a;
                    }
                }
            }
            if (osn != nullptr) {
                f32 dist = arg(0).empty() ? 90.0f : (f32)atof(arg(0).c_str());
                Player* p = GET_PLAYER(gPlayState);
                p->actor.world.pos.x = osn->world.pos.x + Math_SinS(osn->shape.rot.y) * dist;
                p->actor.world.pos.z = osn->world.pos.z + Math_CosS(osn->shape.rot.y) * dist;
                p->actor.world.pos.y = osn->world.pos.y + 10.0f;
                p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw =
                    Math_Vec3f_Yaw(&p->actor.world.pos, &osn->world.pos);
                Log("SPNEAR salesman at (%.0f,%.0f,%.0f) yaw=%d", osn->world.pos.x, osn->world.pos.y, osn->world.pos.z,
                    osn->shape.rot.y);
            } else {
                Log("SPNEAR no salesman in this scene");
            }
        } else if (c.op == "ownbit") {
            // ownbit <item> <hyrule 0|1> <termina 0|1>: which game owns a shared-slot item (MergeShipOwnership.h)
            const u8 item = (u8)ParseInt(arg(0));
            const s32 ok = MergeShip_Own_DebugSet(item, (s32)ParseInt(arg(1)), (s32)ParseInt(arg(2)));
            Log("OWNBIT item=0x%02X ok=%d ootOwns=%d mmOwns=%d", item, ok, MergeShip_Own_OoTOwns(item),
                MergeShip_Own_MMOwns(item));
        } else if (c.op == "beans") {
            // beans [0|1]: the bean slot's Magic Beans (0) / Deku Beans (1), as the pause page switches it; logs whether
            // the slot meets this age's rule (z_kaleido_scope.h CHECK_AGE_REQ_SLOT) and the C buttons (BUG-0009).
            if (!arg(0).empty()) {
                MergeShip_DekuBeans_SetSelected((s32)ParseInt(arg(0)));
            }
            bool ageOk = GameInteractor_Should(VB_SLOT_MEETS_AGE_REQ,
                                               (gSlotAgeReqs[SLOT_BEAN] == 9) || // AGE_REQ_NONE
                                                   (gSlotAgeReqs[SLOT_BEAN] == gSaveContext.linkAge),
                                               SLOT_BEAN);
            Log("BEANS canSelect=%d selected=%d age=%d slotAgeOk=%d C=%02X/%02X/%02X",
                MergeShip_DekuBeans_CanSelect(), MergeShip_DekuBeans_IsSelected(), gSaveContext.linkAge, ageOk ? 1 : 0,
                gSaveContext.equips.buttonItems[1], gSaveContext.equips.buttonItems[2],
                gSaveContext.equips.buttonItems[3]);
        } else if (c.op == "pausecursor") {
            // pausecursor <slot>: the Select Item page's cursor on an inventory slot (6 per row), while paused.
            if (gPlayState != nullptr) {
                PauseContext* pauseCtx = &gPlayState->pauseCtx;
                s32 slot = (s32)ParseInt(arg(0));
                pauseCtx->cursorPoint[PAUSE_ITEM] = slot;
                pauseCtx->cursorX[PAUSE_ITEM] = slot % 6;
                pauseCtx->cursorY[PAUSE_ITEM] = slot / 6;
            }
            Log("PAUSECURSOR %s", arg(0).c_str());
        } else if (c.op == "ammo") {
            // ammo: the counts Hyrule holds (bombs, chus, sticks, nuts, arrows, seeds), the shared pool, and health
            // (capacity, heart pieces) -- the shared ammo pool and Anchor sync tests.
            // ammo <item> <count>: set that item's count first.
            MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
            if (!arg(1).empty()) {
                AMMO((u8)ParseInt(arg(0))) = (s8)ParseInt(arg(1));
            }
            Log("AMMO bombs=%d/%02X chus=%d/%02X sticks=%d nuts=%d arrows=%d seeds=%d healthCap=%d pieces=%d "
                "pool=%d,%d,%d,%d,%d,%d chuBag=%d C=%02X,%02X,%02X",
                AMMO(ITEM_BOMB), INV_CONTENT(ITEM_BOMB), AMMO(ITEM_BOMBCHU), INV_CONTENT(ITEM_BOMBCHU), AMMO(ITEM_STICK),
                AMMO(ITEM_NUT), AMMO(ITEM_BOW), AMMO(ITEM_SLINGSHOT), gSaveContext.healthCapacity,
                (gSaveContext.inventory.questItems & 0xF0000000) >> (QUEST_HEART_PIECE + 4),
                shared ? shared->ammo[0] : -1, shared ? shared->ammo[1] : -1, shared ? shared->ammo[2] : -1,
                shared ? shared->ammo[3] : -1, shared ? shared->ammo[4] : -1, shared ? shared->ammo[5] : -1,
                shared ? shared->chuBagLevel : -1, gSaveContext.equips.buttonItems[1], gSaveContext.equips.buttonItems[2],
                gSaveContext.equips.buttonItems[3]);
        } else if (c.op == "magic") {
            // The meter too: without it the game empties the magic when the player takes control.
            gSaveContext.isMagicAcquired = true;
            if (gSaveContext.magicLevel == 0) {
                gSaveContext.magicLevel = 1;
            }
            gSaveContext.magicCapacity = gSaveContext.magicLevel * MAGIC_NORMAL_METER;
            gSaveContext.magic = (s8)ParseInt(arg(0));
        } else if (c.op == "equip") {
            // equip <equipType> <value> [bItem]: OoT equipment (0 sword, 1 shield, 2 tunic, 3 boots).
            Inventory_ChangeEquipment((s16)ParseInt(arg(0)), (u16)ParseInt(arg(1)));
            if (!arg(2).empty()) {
                gSaveContext.equips.buttonItems[0] = (u8)ParseInt(arg(2));
            }
            if (gPlayState != nullptr) {
                Interface_LoadItemIcon1(gPlayState, 0);
                Player_SetEquipmentData(gPlayState, GET_PLAYER(gPlayState));
            }
            Log("EQUIP %s = %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvar") {
            CVarSetInteger(arg(0).c_str(), (int32_t)ParseInt(arg(1)));
            // As the menu does (UIWidgets.cpp): hooks registered for this CVar re-register with the new value.
            ShipInit::Init(arg(0));
            Log("CVAR %s = %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvarf") {
            // cvarf <name> <float>: a float CVar for this run (never saved)
            CVarSetFloat(arg(0).c_str(), (float)atof(arg(1).c_str()));
            Log("CVARF %s = %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvarlog") {
            // MergeShip2 settings sharing (MergeShipSettingsSync.h): what SoH reads for each named CVar.
            for (const std::string& name : c.args) {
                Log("CVARLOG %s int=%d float=%g", name.c_str(), CVarGetInteger(name.c_str(), -1),
                    CVarGetFloat(name.c_str(), -1.0f));
            }
        } else if (c.op == "time") {
            // time <dayTime>: set the time of day (0x0000 midnight, 0x4000 06:00, 0x8000 noon, 0xC000 18:00).
            gSaveContext.dayTime = (u16)ParseInt(arg(0));
            gSaveContext.skyboxTime = gSaveContext.dayTime;
            Log("TIME 0x%04X", gSaveContext.dayTime);
        } else if (c.op == "loadobj") {
            // loadobj <objectId>: load an object bank into the scene (Object_Spawn, synchronous in SoH) so an actor that
            // needs it can be spawned (spawnrel) in a scene that does not load it -- e.g. an Iron Knuckle in Hyrule Field.
            s32 bank = -1;
            if (gPlayState != nullptr) {
                const s16 objectId = (s16)ParseInt(arg(0));
                bank = Object_GetIndex(&gPlayState->objectCtx, objectId);
                if ((bank < 0) && (gPlayState->objectCtx.num < OBJECT_EXCHANGE_BANK_MAX)) {
                    bank = Object_Spawn(&gPlayState->objectCtx, objectId);
                }
            }
            Log("LOADOBJ %s -> bank %d", arg(0).c_str(), bank);
        } else if (c.op == "spawnrel") {
            // spawnrel <actorId> <dist> [params]: spawn an actor dist in front of the player, facing him.
            Actor* a = nullptr;
            if (gPlayState != nullptr) {
                Player* p = GET_PLAYER(gPlayState);
                f32 dist = (f32)atof(arg(1).c_str());
                s16 yaw = p->actor.shape.rot.y;
                a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, (s16)ParseInt(arg(0)),
                                p->actor.world.pos.x + Math_SinS(yaw) * dist, p->actor.world.pos.y,
                                p->actor.world.pos.z + Math_CosS(yaw) * dist, 0, (s16)(yaw + 0x8000), 0,
                                arg(2).empty() ? 0 : (s16)ParseInt(arg(2)));
            }
            Log("SPAWNREL %s -> %s", arg(0).c_str(), (a != nullptr) ? "ok" : "failed (object not loaded?)");
        } else if (c.op == "strayfairy") {
            // strayfairy [area] [dist]: a free stray fairy of `area` (default 1, Woodfall) dist (default 80) in front
            // of the player, with no scene flag. strayfairy log: every stray fairy, where it is, and the Woodfall count.
            if (arg(0) == "touch") {
                // strayfairy touch: the player steps onto the first stray fairy (MM collects it on touch).
                Actor* fairy = nullptr;
                for (s32 cat = 0; (gPlayState != nullptr) && (cat < ACTORCAT_MAX) && (fairy == nullptr); cat++) {
                    for (Actor* a = gPlayState->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
                        if (a->id == gMergeShipActorId_MM_En_Elforg) {
                            fairy = a;
                            break;
                        }
                    }
                }
                if (fairy != nullptr) {
                    Player* p = GET_PLAYER(gPlayState);
                    p->actor.world.pos = fairy->world.pos;
                    p->actor.world.pos.y -= 20.0f;
                    p->actor.prevPos = p->actor.world.pos;
                    p->actor.home.pos = p->actor.world.pos;
                }
                Log("STRAYFAIRY touch -> %s", (fairy != nullptr) ? "moved onto it" : "no stray fairy");
            } else if (arg(0) == "log") {
                s32 n = 0;
                for (s32 cat = 0; (gPlayState != nullptr) && (cat < ACTORCAT_MAX); cat++) {
                    for (Actor* a = gPlayState->actorCtx.actorLists[cat].head; a != nullptr; a = a->next) {
                        if (a->id != gMergeShipActorId_MM_En_Elforg) {
                            continue;
                        }
                        Log("STRAYFAIRY #%d pos=(%.1f,%.1f,%.1f) yOffset=%.1f", n++, a->world.pos.x, a->world.pos.y,
                            a->world.pos.z, a->shape.yOffset);
                    }
                }
                Log("STRAYFAIRY count=%d enabled=%d found(Woodfall)=%d", n, MergeShip_StrayFairy_IsEnabled(),
                    MergeShip_StrayFairy_GetCount(MM_STRAY_FAIRY_AREA_WOODFALL));
            } else {
                Actor* a = nullptr;
                if (gPlayState != nullptr) {
                    Player* p = GET_PLAYER(gPlayState);
                    f32 dist = arg(1).empty() ? 80.0f : (f32)atof(arg(1).c_str());
                    s16 yaw = p->actor.shape.rot.y;
                    Vec3f pos = { p->actor.world.pos.x + Math_SinS(yaw) * dist, p->actor.world.pos.y + 30.0f,
                                  p->actor.world.pos.z + Math_CosS(yaw) * dist };
                    a = MergeShip_StrayFairy_Spawn(gPlayState, &pos,
                                                   arg(0).empty() ? MM_STRAY_FAIRY_AREA_WOODFALL : (s32)ParseInt(arg(0)),
                                                   MERGESHIP_STRAY_FAIRY_FLAG_NONE);
                }
                Log("STRAYFAIRY spawn -> %s", (a != nullptr) ? "ok" : "none (option off, or mm.o2r missing)");
            }
        } else if (c.op == "inftable") {
            // inftable <flag>: Flags_SetInfTable (BUG-0127: the gate guard has seen Zelda's Letter, 0x76)
            Flags_SetInfTable((s32)ParseInt(arg(0)));
            Log("INFTABLE %s set", arg(0).c_str());
        } else if (c.op == "mmsongact") {
            // mmsongact <song>: what follows Majora's Mask's song `song` (MERGESHIP_SONG_*) once played -- its
            // MergeShip_Songs_OnSongPlayedAct and its box's closing (the harness cannot play notes in Hyrule; BUG-0144's
            // Elegy).
            if (gPlayState != nullptr) {
                MergeShip_Songs_OnSongPlayedAct(gPlayState, (u8)ParseInt(arg(0)));
                s32 handled = MergeShip_Songs_OnTextClosing(gPlayState);
                Log("MMSONGACT %s handled=%d ocarinaMode=0x%X", arg(0).c_str(), handled,
                    gPlayState->msgCtx.ocarinaMode);
            }
        } else if (c.op == "spawnelegy") {
            // spawnelegy shell|beam <params>: one of the Elegy's two actors (BUG-0144) at Link.
            if (gPlayState != nullptr) {
                Player* player = GET_PLAYER(gPlayState);
                s16 id = (arg(0) == "beam") ? gMergeShipActorId_Eff_Change : gMergeShipActorId_En_Torch2;
                Actor* a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, id, player->actor.world.pos.x,
                                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0,
                                       (s16)ParseInt(arg(1)));
                Log("SPAWNELEGY %s id=%d -> %s", arg(0).c_str(), id, (a != nullptr) ? "ok" : "failed");
            }
        } else if (c.op == "owlmap") {
            // owlmap: Termina's owl-warp map, as the Song of Soaring opens it (MergeShip_Songs_OnTextClosing; the
            // ocarina's notes are read from the controller, out of the script's reach)
            if (gPlayState != nullptr) {
                MergeShip_OwlWarp_Open(gPlayState);
            }
            Log("OWLMAP owls=0x%03X", MergeShip_Termina_GetOwlActivationFlags());
        } else if (c.op == "horse") {
            // horse song: Epona's Song's effect (z_message_PAL.c, DREG(53)); horse [log]: every En_Horse and the rider.
            if (arg(0) == "song") {
                DREG(53) = 1;
            }
            if (gPlayState != nullptr) {
                Player* p = GET_PLAYER(gPlayState);
                for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_BG].head; a != nullptr; a = a->next) {
                    if (a->id == ACTOR_EN_HORSE) {
                        EnHorse* h = (EnHorse*)a;
                        if (arg(0) == "side") { // horse side [dist]: stand at her left, facing her, to mount with A
                            f32 dist = arg(1).empty() ? 45.0f : (f32)atof(arg(1).c_str());
                            s16 yaw = (s16)(a->shape.rot.y + 0x4000);
                            p->actor.world.pos.x = p->actor.home.pos.x = a->world.pos.x + Math_SinS(yaw) * dist;
                            p->actor.world.pos.y = p->actor.home.pos.y = a->world.pos.y;
                            p->actor.world.pos.z = p->actor.home.pos.z = a->world.pos.z + Math_CosS(yaw) * dist;
                            p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)(yaw + 0x8000);
                        }
                        Log("HORSE young=%d type=%d action=%d anim=%d scale=%.5f pos=(%.1f,%.1f,%.1f) dist=%.1f "
                            "ridden=%d rideActor=%d onHorse=%d",
                            h->mergeShipYoung, h->type, h->action, h->animationIdx, a->scale.x, a->world.pos.x,
                            a->world.pos.y, a->world.pos.z, Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos),
                            a->child != nullptr, p->rideActor == a, (p->stateFlags1 & PLAYER_STATE1_ON_HORSE) != 0);
                    }
                }
                Log("HORSE %s isYoung=%d owned=%d horseScene=%d", arg(0).c_str(), MergeShip_Epona_IsYoung(),
                    MergeShip_Epona_Owned(), gSaveContext.horseData.scene);
            }
        } else if (c.op == "spawn") {
            // spawn <actorId> <x> <y> <z> [params] [yaw]
            Actor* a = nullptr;
            if (gPlayState != nullptr) {
                a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, (s16)ParseInt(arg(0)), (f32)atof(arg(1).c_str()),
                                (f32)atof(arg(2).c_str()), (f32)atof(arg(3).c_str()), 0,
                                arg(5).empty() ? 0 : (s16)ParseInt(arg(5)), 0,
                                arg(4).empty() ? 0 : (s16)ParseInt(arg(4)));
            }
            Log("SPAWN 0x%s -> %s", arg(0).c_str(), (a != nullptr) ? "ok" : "failed");
        } else if (c.op == "cvarf") {
            CVarSetFloat(arg(0).c_str(), (float)atof(arg(1).c_str()));
            Log("CVARF %s = %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvarcolor") {
            Color_RGB8 color = { (u8)ParseInt(arg(1)), (u8)ParseInt(arg(2)), (u8)ParseInt(arg(3)) };
            CVarSetColor24(arg(0).c_str(), color);
            Log("CVARCOLOR %s = %d %d %d", arg(0).c_str(), color.r, color.g, color.b);
        } else if (c.op == "hud") {
            // hud off|on: the HUD hidden for filming (the game's own cutscene visibility), kept off every frame.
            sHudOff = (arg(0) == "off");
            if (!sHudOff && (gPlayState != nullptr)) {
                Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
            }
            Log("HUD %s", sHudOff ? "off" : "on");
        } else if (c.op == "cinecam") {
            Log("%s", sCine.Command(c.args).c_str()); // include/mergeship/CineCam.hpp
        } else if (c.op == "anchorloop") {
            MergeShipAnchor_TestLoopback((u8)ParseInt(arg(0)), (u8)ParseInt(arg(1)), (u8)ParseInt(arg(2)),
                                         arg(3).empty() ? 80.0f : (f32)atof(arg(3).c_str()),
                                         arg(4).empty() ? 0.0f : (f32)atof(arg(4).c_str()));
            Log("ANCHORLOOP %s %s %s", arg(0).c_str(), arg(1).c_str(), arg(2).c_str());
        } else if (c.op == "anchor") {
            std::string line;
            for (const std::string& a : c.args) {
                line += (line.empty() ? "" : " ") + a;
            }
            MergeShipAnchor_TestCommand(line.c_str());
        } else if (c.op == "trace") {
            sH.trace = (arg(0) == "on");
        } else if (c.op == "exits") {
            if (gPlayState != nullptr) {
                if (!c.args.empty()) {
                    LogExitOpenings((s32)ParseInt(arg(0)));
                } else {
                    for (s32 e = 1; e < 64; e++) {
                        LogExitOpenings(e);
                    }
                }
                Log("EXITS room=%d spawn0room=%d", gPlayState->roomCtx.curRoom.num,
                    gPlayState->setupEntranceList[0].room);
            }
        } else if ((c.op == "walkexit") || (c.op == "toexit")) {
            s32 exitIndex = (s32)ParseInt(arg(0));
            size_t nth = 0;
            f32 dist = 150.0f;
            if (c.op == "walkexit") {
                nth = arg(2).empty() ? 0 : (size_t)ParseInt(arg(2));
            } else {
                dist = arg(1).empty() ? 150.0f : (f32)atof(arg(1).c_str());
                nth = arg(2).empty() ? 0 : (size_t)ParseInt(arg(2));
            }
            auto openings = FindExitOpenings(exitIndex);
            if (nth >= openings.size()) {
                Log("%s %d: no opening %zu (%zu found)", c.op.c_str(), exitIndex, nth, openings.size());
                continue;
            }
            const ExitOpening& o = openings[nth];
            if (c.op == "toexit") {
                Player* p = GET_PLAYER(gPlayState);
                // From the opening towards where the player is, dist away, facing the opening.
                f32 dx = p->actor.world.pos.x - o.center.x;
                f32 dz = p->actor.world.pos.z - o.center.z;
                f32 len = sqrtf(dx * dx + dz * dz);
                if (len < 1.0f) {
                    dx = o.normal.x;
                    dz = o.normal.z;
                    len = sqrtf(dx * dx + dz * dz) + 0.001f;
                }
                Vec3f center = o.center;
                p->actor.world.pos.x = p->actor.home.pos.x = o.center.x + dx / len * dist;
                p->actor.world.pos.z = p->actor.home.pos.z = o.center.z + dz / len * dist;
                p->actor.world.pos.y = p->actor.home.pos.y = p->actor.world.pos.y + 20.0f;
                p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = Math_Vec3f_Yaw(&p->actor.world.pos, &center);
                Log("TOEXIT %d opening %zu center=(%.1f,%.1f,%.1f) -> pos=(%.1f,%.1f,%.1f)", exitIndex, nth, o.center.x,
                    o.center.y, o.center.z, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z);
            } else {
                sH.steerTarget = o.center;
                sH.steerFrames = (int)ParseInt(arg(1));
                sH.buttons = 0;
                Log("WALKEXIT %d opening %zu center=(%.1f,%.1f,%.1f) dist=%.1f room=%d", exitIndex, nth, o.center.x,
                    o.center.y, o.center.z, o.dist, gPlayState->roomCtx.curRoom.num);
                Steer();
                return;
            }
        } else if (c.op == "walkto") {
            sH.steerTarget = { (f32)atof(arg(0).c_str()), 0.0f, (f32)atof(arg(1).c_str()) };
            sH.steerFrames = (int)ParseInt(arg(2));
            sH.buttons = 0;
            if (gPlayState != nullptr) {
                sH.steerTarget.y = GET_PLAYER(gPlayState)->actor.world.pos.y;
                Steer();
            }
            Log("WALKTO %s %s", arg(0).c_str(), arg(1).c_str());
            return;
        } else if ((c.op == "route") || (c.op == "moveto") || (c.op == "swimto")) {
            // Follow a path holding buttons (see RouteFrame); blocks until it ends.
            if (RouteStart(c.args, c.op, c.line)) {
                return;
            }
        } else if (c.op == "autofight") {
            // autofight <frames> [style] [maxDist]: the fight autopilot (see AutoFightFrame).
            const int frames = (int)ParseInt(arg(0));
            FightStyle style = FIGHT_AUTO;
            if (!arg(1).empty() && (arg(1) != "-")) {
                int found = -1;
                for (int s = 0; s < FIGHT_STYLE_COUNT; s++) {
                    if (_stricmp(arg(1).c_str(), kFightStyleNames[s]) == 0) {
                        found = s;
                    }
                }
                if (found < 0) {
                    Log("WARN line %d: autofight style '%s' unknown (auto sword fd goron zora deku): auto", c.line,
                        arg(1).c_str());
                } else {
                    style = (FightStyle)found;
                }
            }
            // (In play: a transition under way would end the fight on its first frame -- waitplay first.)
            if ((frames <= 0) || !InPlay()) {
                Log("AUTOFIGHT skipped: %s", (frames <= 0) ? "no frames" : "not in play");
                continue;
            }
            const u32 seed = sFight.rng ^ (u32)(sH.frame * 2654435761ull);
            sFight = AutoFight();
            sFight.framesLeft = frames;
            sFight.style = style;
            sFight.maxDist = arg(2).empty() ? 1200.0f : (f32)atof(arg(2).c_str());
            sFight.rng = (seed != 0) ? seed : 0x2545F491u;
            // Hold targeting for the fight: Z held keeps a lock, released drops it (the Goron's curl needs it gone).
            sFight.savedZTargetSetting = gSaveContext.zTargetSetting;
            gSaveContext.zTargetSetting = 1;
            gSaveContext.health = gSaveContext.healthCapacity; // the first frame is not a hit taken
            Log("AUTOFIGHT %d frames style=%s maxDist=%.0f form=%d", frames, kFightStyleNames[style], sFight.maxDist,
                GET_PLAYER(gPlayState)->mergeShipForm);
            AutoFightFrame();
            return;
        } else if (c.op == "autotarget") {
            AutoTargetLog(arg(0).empty() ? 1200.0f : (f32)atof(arg(0).c_str()));
        } else if (c.op == "mark") {
            std::string text;
            for (auto& a : c.args) {
                text += a + " ";
            }
            Log("MARK %s", text.c_str());
            TracePlayer();
        } else if (c.op == "shot") {
            Screenshot(arg(0));
        } else if (c.op == "release") {
            // release: the script sets a scene up and hands over -- the player's controller drives again and the
            // sound comes back (the run stays a test run: the trace keeps logging until the game closes).
            // "release mute": the player asked for no sound.
            CVarSetInteger(CVAR_SETTING("Volume.Master"), (arg(0) == "mute") ? 0 : sPrevMasterVolume);
            sH.buttons = 0;
            sH.latched = 0;
            sH.stickX = sH.stickY = 0;
            sH.active = false;
            Log("RELEASE volume %d, the player drives", CVarGetInteger(CVAR_SETTING("Volume.Master"), -1));
        } else if (c.op == "quit") {
            Log("QUIT");
            TracePlayer();
            fflush(sH.log);
            spdlog::default_logger()->flush();
            // Skip the normal shutdown: it would write the harness's CVar changes to the user's
            // configuration.
#ifdef _WIN32
            TerminateProcess(GetCurrentProcess(), 0);
#else
            _exit(0);
#endif
        } else if (MergeShip_Entrances_TestCommand(c.op, c.args)) {
            // MergeShip2 M8: randoer, ertable, ertake (MergeShipEntrances.cpp)
        } else if (MergeShipUnified_TestCommand(c.op, c.args)) {
            // MergeShip2 M3: unified, ledger, ledgeradd, hyrulefind, project, unifiedstate (MergeShipUnified.h)
        } else if (MergeShip_Hints_TestCommand(c.op, c.args)) {
            // MergeShip2 M6: hints, hintarea, hintitem, truthhint, truthcandidates (MergeShipUnifiedHints.h)
        } else if (MergeShip_Tracker_TestCommand(c.op, c.args)) {
            // MergeShip2 M7: trackterm, trackitems, catalog (MergeShipUnifiedTracker.h)
        } else {
            Log("WARN line %d: unknown command '%s'", c.line, c.op.c_str());
        }
    }
}

void LoadScript(const char* path) {
    std::ifstream in(path);
    if (!in) {
        SPDLOG_ERROR("MergeShip test: cannot open script {}", path);
        return;
    }
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        n++;
        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::stringstream ss(line);
        Command c;
        c.line = n;
        if (!(ss >> c.op)) {
            continue;
        }
        std::string a;
        while (ss >> a) {
            c.args.push_back(a);
        }
        if (c.op == "boot") {
            sH.bootEntrance = (s32)ParseInt(c.args.empty() ? "0" : c.args[0]);
            sH.bootAge = (c.args.size() > 1 && c.args[1] == "child") ? LINK_AGE_CHILD : LINK_AGE_ADULT;
        }
        sH.commands.push_back(c);
    }

    std::string p(path);
    auto slash = p.find_last_of("\\/");
    sH.dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
    std::string logPath = sH.dir + "\\trace.log";
    sH.log = fopen(logPath.c_str(), "w");
    sH.active = true;
    // Test runs are silent (audio_playback.c reads this every frame); the harness never saves CVars.
#ifdef _WIN32
    // A sleeping display throttles presentation to about a frame a second, stalling a test run overnight: keep it
    // on while the script runs (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED; released with the process).
    SetThreadExecutionState(0x80000003);
#endif
    sPrevMasterVolume = CVarGetInteger(CVAR_SETTING("Volume.Master"), 100);
    if (sPrevMasterVolume <= 0) {
        sPrevMasterVolume = 100;
    }
    CVarSetInteger(CVAR_SETTING("Volume.Master"), 0);
    SPDLOG_INFO("MergeShip test: running {} ({} commands), trace to {}", path, sH.commands.size(), logPath);
}

// The same start as SoH's warp-point boot (Enhancements/Warping.cpp), with the age chosen by the
// script rather than forced to adult.
void Boot(TitleContext* title) {
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFE;
    Sram_InitDebugSave();
    gSaveContext.fileNum = 0xFF;
    gSaveContext.sceneLayer = 0;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.linkAge = sH.bootAge;
    gSaveContext.nightFlag = 0;
    gSaveContext.skyboxTime = gSaveContext.dayTime = 0x8000;
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.buttonStatus); i++) {
        gSaveContext.buttonStatus[i] = BTN_ENABLED;
    }
    gSaveContext.nextHudVisibilityMode = gSaveContext.hudVisibilityMode = gSaveContext.hudVisibilityModeTimer = 0;
    gSaveContext.forceRisingButtonAlphas = 0;
    gSaveContext.entranceIndex = sH.bootEntrance;
    gSaveContext.respawnFlag = 0;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = 0xFF;
    gSaveContext.showTitleCard = false;
    gWeatherMode = 0;
    title->state.running = false;
    SET_NEXT_GAMESTATE(&title->state, Play_Init, PlayState);
    GameInteractor_ExecuteOnLoadGame(gSaveContext.fileNum);
    Log("BOOT entrance=0x%X age=%d", sH.bootEntrance, sH.bootAge);
}

void RegisterMergeShipTest() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    // MergeShip2 host (mergeship.exe): both games share one environment, so SoH's script there is
    // MERGESHIP_TEST_SCRIPT_SOH (2Ship's harnesses read their own variables).
    const char* script = getenv("MERGESHIP_TEST_SCRIPT_SOH");
#ifndef MERGESHIP_HOSTED
    // soh.exe alone: the plain variable too. In mergeship.exe it would also start 2Ship's harness.
    if (script == nullptr || *script == '\0') {
        script = getenv("MERGESHIP_TEST_SCRIPT");
    }
#endif
    if (script == nullptr || *script == '\0') {
        return;
    }
    LoadScript(script);
    if (!sH.active) {
        return;
    }

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnZTitleUpdate>([](void* gameState) {
        if (sH.booted || sH.bootEntrance < 0) {
            return;
        }
        sH.booted = true;
        Boot((TitleContext*)gameState);
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        sH.frame++;
        Step();
        UpdateCineCam();
        if (sH.trace) {
            TracePlayer();
        }
    });
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipTest, {});

} // namespace

// Called by PadMgr_ProcessInputs for controller 1: while a script runs, the script *is* the
// controller, so a stray key press or an unplugged pad cannot change the result.
extern "C" void MergeShipTest_OverrideInput(OSContPad* pad) {
    if (!sH.active) {
        return;
    }
    pad->button = sH.buttons | sH.latched;
    pad->stick_x = sH.stickX;
    pad->stick_y = sH.stickY;
    pad->err_no = 0;
}

// Diagnostics from C code (audio engine, player ports); a no-op unless a script is running.
extern "C" void MergeShipTest_Log(const char* fmt, ...) {
    if (sH.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sH.log, "[%06llu] ", (unsigned long long)sH.frame);
    vfprintf(sH.log, fmt, ap);
    fputc('\n', sH.log);
    va_end(ap);
    fflush(sH.log);
}

extern "C" int MergeShipTest_IsActive(void) {
    return sH.active ? 1 : 0;
}
