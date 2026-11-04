// MergeShip2 -- entrance shuffle over both worlds, Ship of Harkinian's side (MergeShipEntrances.h).

#include "soh/MergeShip/MergeShipEntrances.h"

#include "soh/Enhancements/randomizer/entrance.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/randomizer_entrance_tracker.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipTerminaLogic.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/ShipInit.hpp"

#include <mergeship/MsEntrances.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <set>

#include <spdlog/spdlog.h>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "soh/Enhancements/randomizer/randomizer_grotto.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

using namespace Rando;

namespace {

// ---- the generated data (tools/gen_mm_entrances.py) ------------------------------------------------------------------

struct TeDungeon {
    int id;
    RandomizerRegion outside;
    RandomizerRegion inside;
    int16_t keyIn;
    int16_t keyOut;
    const char* outsideName;
    const char* insideName;
};
struct TeBoss {
    int boss;
    RandomizerRegion preBoss;
    RandomizerRegion lair;
    int16_t keyDoor;
    int16_t keyReturn;
    RandomizerRegion returnRegion; // the dungeon's entrance, where the way back out of the lair leads
    RandomizerRegion warpRegion;
    int16_t keyWarp;
    int dungeonId;
    int adult; // Cross-Game with SoH's "Age Restricted": the adult bosses' pool
    const char* doorName;
    const char* lairName;
};
struct TeGrotto {
    int n;
    RandomizerRegion outside;
    RandomizerRegion grotto;
    int16_t keyIn;
    int16_t keyReturn;
    const char* outsideName;
    const char* grottoName;
};
struct TePair {
    RandomizerRegion a; // overworld: one side; interior: the overworld side
    RandomizerRegion b; // overworld: the other side; interior: the house, shop or fountain
    int16_t keyIntoB;
    int16_t keyIntoA;
    const char* nameAB;
    const char* nameBA;
};
struct TeMoon {
    int index; // 0-3 the moon children's trials, MOON_ROOF_INDEX the rooftop
    RandomizerRegion from;
    RandomizerRegion to;
    int16_t key;
    const char* exitName;
    const char* destName;
};
struct TeRegion {
    RandomizerRegion rr;
    const char* name;
    const char* mmScene; // 2Ship's SceneId (winter / poisoned form)
};
constexpr int MOON_ROOF_INDEX = 4;

// Each table takes its own rows of MergeShipTerminaEntranceData.inc (every other kind's macro expands to nothing).
// clang-format off
#define MS_TE_DUNGEON(...)
#define MS_TE_BOSS(...)
#define MS_TE_GROTTO(...)
#define MS_TE_OVERWORLD(...)
#define MS_TE_INTERIOR(...)
#define MS_TE_MOON(...)
#define MS_TE_REGION(...)
#undef MS_TE_DUNGEON
#define MS_TE_DUNGEON(id, o, i, ki, ko, on, in) { id, o, i, (int16_t)(ki), (int16_t)(ko), on, in },
const TeDungeon kDungeons[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_DUNGEON
#define MS_TE_DUNGEON(...)
#undef MS_TE_BOSS
#define MS_TE_BOSS(b, pre, lair, kd, kr, ret, warp, kw, did, adult, dn, ln) \
    { b, pre, lair, (int16_t)(kd), (int16_t)(kr), ret, warp, (int16_t)(kw), did, adult, dn, ln },
const TeBoss kBosses[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_BOSS
#define MS_TE_BOSS(...)
#undef MS_TE_GROTTO
#define MS_TE_GROTTO(n, o, g, ki, kr, on, gn) { n, o, g, (int16_t)(ki), (int16_t)(kr), on, gn },
const TeGrotto kGrottos[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_GROTTO
#define MS_TE_GROTTO(...)
#undef MS_TE_OVERWORLD
#define MS_TE_OVERWORLD(a, b, kb, ka, nab, nba) { a, b, (int16_t)(kb), (int16_t)(ka), nab, nba },
const TePair kOverworld[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_OVERWORLD
#define MS_TE_OVERWORLD(...)
#undef MS_TE_INTERIOR
#define MS_TE_INTERIOR(o, i, ki, ko, dn, in) { o, i, (int16_t)(ki), (int16_t)(ko), dn, in },
const TePair kInteriors[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_INTERIOR
#define MS_TE_INTERIOR(...)
#undef MS_TE_MOON
#define MS_TE_MOON(i, from, to, k, xn, dn) { i, from, to, (int16_t)(k), xn, dn },
const TeMoon kMoon[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_MOON
#define MS_TE_MOON(...)
#undef MS_TE_REGION
#define MS_TE_REGION(rr, name, scene) { rr, name, scene },
const TeRegion kRegions[] = {
#include "MergeShipTerminaEntranceData.inc"
};
#undef MS_TE_DUNGEON
#undef MS_TE_BOSS
#undef MS_TE_GROTTO
#undef MS_TE_OVERWORLD
#undef MS_TE_INTERIOR
#undef MS_TE_MOON
#undef MS_TE_REGION
// clang-format on
static_assert(sizeof(kBosses) / sizeof(kBosses[0]) == MS_ENTR_BOSS_COUNT, "one row per MM boss (MsEntrances.h)");

// ---- runtime state ---------------------------------------------------------------------------------------------------

std::vector<int16_t> sTable; // pairs {key, override} of every Termina key the loaded seed overrides
int16_t sPending = -1;       // the Termina key a redirect is waiting to take the player to

// ---- generation state ------------------------------------------------------------------------------------------------

std::set<RandomizerRegion> sScaffolded;          // regions Termina's logic had not built, given a stand-in
Entrance* sWarps[MS_ENTR_BOSS_COUNT] = {};        // each MM boss's blue warp, when registered
Entrance* sBossReturns[MS_ENTR_BOSS_COUNT] = {};  // each MM lair's way back (synthetic), when registered
const Entrance* sWarpConditionFrom[MS_ENTR_BOSS_COUNT] = {};
int sRegistered = 0;
int sSkipped = 0;
// Pairs (the moon: warps) registered per kind, for the summary.
enum Kind { KIND_DUNGEON, KIND_BOSS, KIND_GROTTO, KIND_OVERWORLD, KIND_INTERIOR, KIND_MOON, KIND_COUNT };
int sKindRegistered[KIND_COUNT] = {};
// Fill's attempts (MergeShip_Entrances_BeginFill) and the last generation's failure message.
int sEntranceFailures = 0;
int sPlacementFailures = 0;
std::string sLastFailure;
std::string sLastValidation; // why the last final ValidateWorld refused (MergeShip_Entrances_ValidateWorld)

template <int B> bool WarpCondition() {
    const Entrance* from = sWarpConditionFrom[B];
    return (from == nullptr) || from->GetConditionsMet();
}
const ConditionFn kWarpConditions[MS_ENTR_BOSS_COUNT] = { WarpCondition<0>, WarpCondition<1>, WarpCondition<2>,
                                                          WarpCondition<3> };

void Log(const char* fmt, ...) {
    char line[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip entrances: {}", line);
    MergeShipTest_Log("ENTR %s", line);
}

uint8_t Mode(RandomizerSettingKey key) {
    auto ctx = Rando::Context::GetInstance();
    return (ctx != nullptr && MergeShip_Termina_Enabled()) ? ctx->GetOption(key).Get() : RO_MS_ER_OFF;
}

const char* RegionName(RandomizerRegion rr) {
    for (const TeRegion& r : kRegions) {
        if (r.rr == rr) {
            return r.name;
        }
    }
    return "MM Region";
}

// The Majora's Mask scene of a Termina region of the generated data (nullptr for any other region).
const char* MMScene(RandomizerRegion rr) {
    for (const TeRegion& r : kRegions) {
        if (r.rr == rr) {
            return r.mmScene;
        }
    }
    return nullptr;
}

Entrance* FindExit(RandomizerRegion from, RandomizerRegion to) {
    for (Entrance& exit : RegionTable(from)->exits) {
        if (exit.GetOriginalConnectedRegionKey() == to) {
            return &exit;
        }
    }
    return nullptr;
}

// A Termina region the logic port has not built (M1 has only the door and RR_MM_ROOT).
bool Unported(RandomizerRegion rr) {
    const Region* r = RegionTable(rr);
    return r->exits.empty() && r->locations.empty() && r->events.empty() && r->regionName.empty();
}

bool CanScaffold(RandomizerRegion rr) {
    return (rr != RR_MM_ROOT) && (sScaffolded.count(rr) > 0 || Unported(rr));
}

Entrance* AddExitTo(RandomizerRegion from, RandomizerRegion to, ConditionFn condition, const char* conditionStr) {
    Region* region = RegionTable(from);
    region->AddExit(from, to, condition, conditionStr);
    Entrance* exit = &region->exits.front();
    exit->SetName();
    RegionTable(to)->entrances.push_front(exit);
    return exit;
}

// Until Termina's logic (M4) builds a region, it stands in as a region joined both ways to RR_MM_ROOT, which holds
// every Termina check with no logic (M1): the shuffle has its entrances to move, and the seed stays beatable.
void Scaffold(RandomizerRegion rr) {
    if (!CanScaffold(rr) || sScaffolded.count(rr) > 0) {
        return;
    }
    Region* region = RegionTable(rr);
    region->regionName = RegionName(rr);
    region->scene = SCENE_ID_MAX;
    region->timePass = TIME_DOESNT_PASS;
    region->areas = { RA_NONE };
    region->world = RW_TERMINA;
    region->randomizerRegionKey = rr;
    sScaffolded.insert(rr);
    AddExitTo(RR_MM_ROOT, rr, [] { return true; }, "true");
    AddExitTo(rr, RR_MM_ROOT, [] { return true; }, "true");
}

// Forget the stand-ins of an earlier seed: they live outside RegionTable_Init's reach (it does not rebuild a region
// Termina's logic does not know), so their exits are unhooked from wherever they lead and the regions reset.
void Unscaffold() {
    for (RandomizerRegion rr : sScaffolded) {
        for (Entrance& exit : RegionTable(rr)->exits) {
            if (exit.GetConnectedRegionKey() != RR_NONE) {
                Entrance* pointer = &exit;
                RegionTable(exit.GetConnectedRegionKey())->entrances.remove(pointer);
            }
        }
    }
    for (RandomizerRegion rr : sScaffolded) {
        *RegionTable(rr) = Region();
    }
    sScaffolded.clear();
}

// A Termina region that RegionTable_Init did not rebuild (Termina off this time) keeps the entrance types an earlier
// seed gave it; take them back, so no later seed's shuffle sees them.
void ResetMarks() {
    for (const TeRegion& r : kRegions) {
        for (Entrance& exit : RegionTable(r.rr)->exits) {
            if (MS_ENTR_IS_TERMINA(exit.GetIndex())) {
                exit.SetType(EntranceType::None);
                exit.SetIndex(-1);
            }
        }
    }
    for (int b = 0; b < MS_ENTR_BOSS_COUNT; b++) {
        sWarps[b] = nullptr;
        sBossReturns[b] = nullptr;
        sWarpConditionFrom[b] = nullptr;
    }
}

// An exit from -> to: the logic's own, or (a region not built yet) a stand-in.
Entrance* PairExit(RandomizerRegion from, RandomizerRegion to) {
    Entrance* exit = FindExit(from, to);
    if (exit == nullptr && (CanScaffold(from) || CanScaffold(to))) {
        Scaffold(from);
        Scaffold(to);
        exit = AddExitTo(from, to, [] { return true; }, "true");
    }
    return exit;
}

void Mark(Entrance* exit, int16_t key, EntranceType type, bool primary, bool decoupled,
          std::unordered_map<int16_t, Entrance*>& entranceMap) {
    exit->SetIndex(key);
    exit->SetType(type);
    if (primary) {
        exit->SetAsPrimary();
    }
    if (decoupled) {
        exit->SetDecoupled();
    }
    entranceMap[key] = exit;
}

bool RegisterPair(Entrance* forward, Entrance* back, int16_t keyIn, int16_t keyOut, EntranceType type, bool decoupled,
                  std::unordered_map<int16_t, Entrance*>& entranceMap, const char* what) {
    if (forward == nullptr || back == nullptr) {
        Log("skipped %s: Termina's logic has no %s", what, forward == nullptr ? "way in" : "way back");
        sSkipped++;
        return false;
    }
    Mark(forward, keyIn, type, true, decoupled, entranceMap);
    Mark(back, keyOut, type, false, decoupled, entranceMap);
    forward->BindTwoWay(back);
    sRegistered++;
    return true;
}

void RegisterBlueWarp(const TeBoss& boss, std::unordered_map<int16_t, Entrance*>& entranceMap) {
    Entrance* warp = FindExit(boss.lair, boss.warpRegion);
    if (warp == nullptr) {
        if (CanScaffold(boss.lair) || CanScaffold(boss.warpRegion)) {
            Scaffold(boss.lair);
            Scaffold(boss.warpRegion);
            warp = AddExitTo(boss.lair, boss.warpRegion, [] { return true; }, "true");
        } else {
            // Termina's logic took the warp elsewhere (Goht's spring Mountain Village has no region of its own in
            // 2Ship): a warp of our own, as hard to reach as the logic's (beating the boss).
            for (const Entrance& exit : RegionTable(boss.lair)->exits) {
                if (&exit != sBossReturns[boss.boss]) {
                    sWarpConditionFrom[boss.boss] = &exit;
                    break;
                }
            }
            warp = AddExitTo(boss.lair, boss.warpRegion, kWarpConditions[boss.boss], "MM boss beaten");
        }
    }
    Mark(warp, boss.keyWarp, EntranceType::BlueWarp, true, false, entranceMap);
    // Always has an override (itself until the blue warp targets are decided), so the spoiler and the runtime tables
    // carry it whenever Termina's dungeons or bosses move.
    warp->SetAsShuffled();
    warp->SetReplacement(warp);
    sWarps[boss.boss] = warp;
}

// A condition of Termina's logic that reads MM's time of day (2Ship's time helpers as tools/port_mm_rando.py writes
// them: mm->Between(...), mm->IsNight3(), ...).
bool ReadsMMTime(const std::string& condition) {
    static const char* const kTimeCalls[] = { "mm->Between(",  "mm->Before(",   "mm->After(",    "mm->At(",
                                              "mm->IsDay",     "mm->IsNight",   "mm->FirstDay(", "mm->SecondDay(",
                                              "mm->FinalDay(", "mm->Midnight(" };
    for (const char* call : kTimeCalls) {
        if (condition.find(call) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// The way in of an interior of Termina (a house, shop, fountain, ...) whose door has opening hours: the schedules of
// the people inside follow those hours, so the interior keeps them wherever its door now is.
bool KeepsHours(const Entrance* door) {
    if (door == nullptr || !door->IsPrimary() || !MS_ENTR_IS_TERMINA(door->GetIndex())) {
        return false;
    }
    const EntranceType type = door->GetType();
    const Region* outside = door->GetParentRegion();
    return (type == EntranceType::TerminaInterior || type == EntranceType::Interior) && (outside != nullptr) &&
           (outside->world == RW_TERMINA) && ReadsMMTime(door->GetConditionStr());
}

} // namespace

// ---- runtime -----------------------------------------------------------------------------------------------------------

extern "C" int16_t MergeShipEntrances_Redirect(int16_t index) {
    if (!MS_ENTR_IS_TERMINA(index)) {
        return index;
    }
    if (!MergeShipHost_IsHosted()) {
        // soh.exe has no Termina to go to: the Lost Woods entrance the door would have used.
        Log("redirect 0x%X: not hosted, staying in Hyrule", index);
        return ENTR_MERGESHIP_TERMINA;
    }
    sPending = index;
    // A cutscene of the entrance the override replaced must not play on the stand-in Lost Woods entrance.
    gSaveContext.cutsceneIndex = 0;
    Log("redirect: key 0x%X leads to Termina", index);
    return ENTR_MERGESHIP_TERMINA;
}

extern "C" int16_t MergeShipEntrances_TakePending(void) {
    const int16_t key = sPending;
    sPending = -1;
    return key;
}

extern "C" void MergeShipEntrances_ClearPending(void) {
    sPending = -1;
}

extern "C" void MergeShipEntrances_ResetTable(void) {
    sTable.clear();
}

extern "C" void MergeShipEntrances_RegisterOverride(int16_t index, int16_t override) {
    if (!MS_ENTR_IS_TERMINA(index)) {
        return;
    }
    sTable.push_back(index);
    sTable.push_back(override);
}

extern "C" const int16_t* MergeShipEntrances_Table(int32_t* pairCount) {
    if (pairCount != nullptr) {
        *pairCount = (int32_t)(sTable.size() / 2);
    }
    return sTable.empty() ? nullptr : sTable.data();
}

extern "C" int16_t MergeShipEntrances_ResolveArrival(int16_t key) {
    if ((key < 0) || MS_ENTR_IS_TERMINA(key)) {
        Log("arrival key 0x%X is not Hyrule's: Kokiri Forest", key);
        return ENTR_KOKIRI_FOREST_UPPER_EXIT;
    }
    const int16_t entrance = Grotto_OverrideSpecialEntrance(key);
    Log("arrival from Termina: key 0x%X -> entrance 0x%X", key, entrance);
    return entrance;
}

// A redirect while a scene was loading (a savewarp, a game over, a blue warp's destination) loaded the Lost Woods
// entrance ENTR_MERGESHIP_TERMINA stands for; once its fade-in is over, the held trip starts.
static void RegisterMergeShipEntranceHooks() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
        if ((sPending < 0) || (gPlayState == nullptr) || !MergeShipHost_IsHosted()) {
            return;
        }
        PlayState* play = gPlayState;
        if ((gSaveContext.entranceIndex != ENTR_MERGESHIP_TERMINA) || (gSaveContext.gameMode != GAMEMODE_NORMAL) ||
            (play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF)) {
            return;
        }
        play->nextEntranceIndex = ENTR_MERGESHIP_TERMINA;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        Log("scene loaded through a redirect: starting the trip to key 0x%X", sPending);
    });
}
static RegisterShipInitFunc sInitHooks(RegisterMergeShipEntranceHooks);

// ---- settings ------------------------------------------------------------------------------------------------------------

bool MergeShip_Entrances_Active() {
    return Mode(RSK_MERGESHIP_ER_DUNGEONS) != RO_MS_ER_OFF || Mode(RSK_MERGESHIP_ER_BOSSES) != RO_MS_ER_OFF ||
           Mode(RSK_MERGESHIP_ER_GROTTOS) != RO_MS_ER_OFF || Mode(RSK_MERGESHIP_ER_OVERWORLD) != RO_MS_ER_OFF ||
           Mode(RSK_MERGESHIP_ER_INTERIORS) != RO_MS_ER_OFF || Mode(RSK_MERGESHIP_ER_MOON) != RO_MS_ER_MOON_OFF;
}

void MergeShip_Entrances_FinalizeSettings(Rando::Context* ctx) {
    // Before RegionTable_Init: nothing an earlier seed made of Termina's entrances reaches this one.
    Unscaffold();
    ResetMarks();
    sRegistered = sSkipped = 0;
    if (!ctx->GetOption(RSK_MERGESHIP_TERMINA).Is(RO_GENERIC_ON)) {
        ctx->GetOption(RSK_MERGESHIP_ER_DUNGEONS).Set(RO_MS_ER_OFF);
        ctx->GetOption(RSK_MERGESHIP_ER_BOSSES).Set(RO_MS_ER_OFF);
        ctx->GetOption(RSK_MERGESHIP_ER_GROTTOS).Set(RO_MS_ER_OFF);
        ctx->GetOption(RSK_MERGESHIP_ER_OVERWORLD).Set(RO_MS_ER_OFF);
        ctx->GetOption(RSK_MERGESHIP_ER_INTERIORS).Set(RO_MS_ER_OFF);
        ctx->GetOption(RSK_MERGESHIP_ER_MOON).Set(RO_MS_ER_MOON_OFF);
        return;
    }
    const uint8_t dungeons = ctx->GetOption(RSK_MERGESHIP_ER_DUNGEONS).Get();
    const uint8_t bosses = ctx->GetOption(RSK_MERGESHIP_ER_BOSSES).Get();
    const uint8_t grottos = ctx->GetOption(RSK_MERGESHIP_ER_GROTTOS).Get();
    const uint8_t overworld = ctx->GetOption(RSK_MERGESHIP_ER_OVERWORLD).Get();
    const uint8_t interiors = ctx->GetOption(RSK_MERGESHIP_ER_INTERIORS).Get();
    const uint8_t moon = ctx->GetOption(RSK_MERGESHIP_ER_MOON).Get();
    // Across both games Hyrule's side of each pool is shuffled too.
    if (dungeons == RO_MS_ER_CROSS_GAME &&
        ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Is(RO_DUNGEON_ENTRANCE_SHUFFLE_OFF)) {
        ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Set(RO_DUNGEON_ENTRANCE_SHUFFLE_ON);
    }
    if (bosses == RO_MS_ER_CROSS_GAME &&
        ctx->GetOption(RSK_SHUFFLE_BOSS_ENTRANCES).Is(RO_BOSS_ROOM_ENTRANCE_SHUFFLE_OFF)) {
        ctx->GetOption(RSK_SHUFFLE_BOSS_ENTRANCES).Set(RO_BOSS_ROOM_ENTRANCE_SHUFFLE_FULL);
    }
    if (grottos == RO_MS_ER_CROSS_GAME && ctx->GetOption(RSK_SHUFFLE_GROTTO_ENTRANCES).Is(RO_GENERIC_OFF)) {
        ctx->GetOption(RSK_SHUFFLE_GROTTO_ENTRANCES).Set(RO_GENERIC_ON);
    }
    if (overworld == RO_MS_ER_CROSS_GAME && ctx->GetOption(RSK_SHUFFLE_OVERWORLD_ENTRANCES).Is(RO_GENERIC_OFF)) {
        ctx->GetOption(RSK_SHUFFLE_OVERWORLD_ENTRANCES).Set(RO_GENERIC_ON);
    }
    if (interiors == RO_MS_ER_CROSS_GAME &&
        ctx->GetOption(RSK_SHUFFLE_INTERIOR_ENTRANCES).Is(RO_INTERIOR_ENTRANCE_SHUFFLE_OFF)) {
        ctx->GetOption(RSK_SHUFFLE_INTERIOR_ENTRANCES).Set(RO_INTERIOR_ENTRANCE_SHUFFLE_SIMPLE);
    }
    if (dungeons != RO_MS_ER_OFF || bosses != RO_MS_ER_OFF || grottos != RO_MS_ER_OFF || overworld != RO_MS_ER_OFF ||
        interiors != RO_MS_ER_OFF || moon != RO_MS_ER_MOON_OFF) {
        ctx->GetOption(RSK_SHUFFLE_ENTRANCES).Set(RO_GENERIC_ON);
    }
}

const std::string* MergeShip_Entrances_SettingText(RandomizerSettingKey key, bool description) {
    static const std::string kDungeonsName = "Mergeship: Termina Dungeon Entrances";
    static const std::string kDungeonsDesc =
        "Shuffles the entrances of Termina's dungeons: Woodfall, Snowhead, Great Bay and Stone Tower Temples, the "
        "Inverted Stone Tower Temple, Beneath the Well and the Ancient Castle of Ikana.\n\n"
        "Per Game - among themselves; Hyrule's dungeons follow \"Dungeon Entrances\".\n"
        "Cross-Game - in one pool with Hyrule's dungeons (\"Dungeon Entrances\" is switched on): a dungeon door in "
        "Hyrule can lead into Termina and back.";
    static const std::string kBossesName = "Mergeship: Termina Boss Entrances";
    static const std::string kBossesDesc =
        "Shuffles the boss doors of Odolwa, Goht, Gyorg and Twinmold.\n\n"
        "Per Game - among themselves; Hyrule's boss rooms follow \"Boss Entrances\".\n"
        "Cross-Game - in one pool with Hyrule's boss rooms (\"Boss Entrances\" is switched to Full if it was off).\n\n"
        "A blue warp leads out of the dungeon whose boss door led to the room, in either world. Majora's Mask's lairs "
        "have no way back out; a Hyrule boss room behind one of their doors leads back to that temple's entrance.";
    static const std::string kGrottosName = "Mergeship: Termina Grotto Entrances";
    static const std::string kGrottosDesc =
        "Shuffles Termina's 26 grottos (with the Deku Palace bean grotto and the Lone Peak Shrine).\n\n"
        "Per Game - among themselves; Hyrule's grottos follow \"Grotto Entrances\".\n"
        "Cross-Game - in one pool with Hyrule's grottos and graves (\"Grotto Entrances\" is switched on).";
    static const std::string kOverworldName = "Mergeship: Termina Overworld Entrances";
    static const std::string kOverworldDesc =
        "Shuffles the connections between Termina's overworld areas: Clock Town's gates and districts, Termina Field's "
        "exits, the roads, the swamp, the mountain, the ranch, the coast and Ikana.\n\n"
        "Per Game - among themselves; Hyrule's overworld follows \"Overworld Entrances\".\n"
        "Cross-Game - in one pool with Hyrule's overworld (\"Overworld Entrances\" is switched on): walking out of a "
        "Hyrule area can lead into Termina, and back.\n\n"
        "A passage keeps Majora's Mask's rules wherever it leads: one that only opens at certain hours or days still "
        "does. The Lost Woods door and the Clock Tower's passage to it always stay where they are.";
    static const std::string kInteriorsName = "Mergeship: Termina Interior Entrances";
    static const std::string kInteriorsDesc =
        "Shuffles the doors of Termina's houses, shops, fairy fountains, spider houses and Zora Hall's rooms.\n\n"
        "Per Game - among themselves; Hyrule's interiors follow \"Interior Entrances\".\n"
        "Cross-Game - in one pool with Hyrule's interiors (\"Interior Entrances\" is switched to Simple if it was "
        "off).\n\n"
        "A door keeps its opening hours wherever it leads: the Milk Bar's door still only opens when the bar does.";
    static const std::string kMoonName = "Mergeship: Moon Entrances";
    static const std::string kMoonDesc =
        "Shuffles the moon's entrances, within Termina only.\n\n"
        "Trials - each moon child sends the player to a random one of the four trials (each keeps the child's own "
        "requirement).\n"
        "Trials & Rooftop - the Clock Tower rooftop's Oath to Order warp joins them: it may lead straight into a trial, "
        "whose way back is the moon field.";
    switch (key) {
        case RSK_MERGESHIP_ER_DUNGEONS:
            return description ? &kDungeonsDesc : &kDungeonsName;
        case RSK_MERGESHIP_ER_BOSSES:
            return description ? &kBossesDesc : &kBossesName;
        case RSK_MERGESHIP_ER_GROTTOS:
            return description ? &kGrottosDesc : &kGrottosName;
        case RSK_MERGESHIP_ER_OVERWORLD:
            return description ? &kOverworldDesc : &kOverworldName;
        case RSK_MERGESHIP_ER_INTERIORS:
            return description ? &kInteriorsDesc : &kInteriorsName;
        case RSK_MERGESHIP_ER_MOON:
            return description ? &kMoonDesc : &kMoonName;
        default:
            return nullptr;
    }
}

// ---- generation --------------------------------------------------------------------------------------------------------

void MergeShip_Entrances_SetData(std::unordered_map<int16_t, Entrance*>& entranceMap, bool decoupled) {
    for (auto it = entranceMap.begin(); it != entranceMap.end();) {
        it = MS_ENTR_IS_TERMINA(it->first) ? entranceMap.erase(it) : std::next(it);
    }
    Unscaffold();
    ResetMarks();
    sRegistered = sSkipped = 0;
    std::fill(std::begin(sKindRegistered), std::end(sKindRegistered), 0);
    if (!MergeShip_Entrances_Active()) {
        return;
    }
    const uint8_t dungeons = Mode(RSK_MERGESHIP_ER_DUNGEONS);
    const uint8_t bosses = Mode(RSK_MERGESHIP_ER_BOSSES);
    const uint8_t grottos = Mode(RSK_MERGESHIP_ER_GROTTOS);
    const uint8_t overworld = Mode(RSK_MERGESHIP_ER_OVERWORLD);
    const uint8_t interiors = Mode(RSK_MERGESHIP_ER_INTERIORS);
    const uint8_t moon = Mode(RSK_MERGESHIP_ER_MOON);
    int before = 0;
    auto counted = [&](Kind kind) {
        sKindRegistered[kind] += sRegistered - before;
        before = sRegistered;
    };

    if (dungeons != RO_MS_ER_OFF) {
        const EntranceType type =
            (dungeons == RO_MS_ER_CROSS_GAME) ? EntranceType::Dungeon : EntranceType::TerminaDungeon;
        for (const TeDungeon& d : kDungeons) {
            RegisterPair(PairExit(d.outside, d.inside), PairExit(d.inside, d.outside), d.keyIn, d.keyOut, type,
                         decoupled, entranceMap, d.insideName);
        }
    }
    counted(KIND_DUNGEON);
    if (bosses != RO_MS_ER_OFF) {
        for (const TeBoss& b : kBosses) {
            const EntranceType type =
                (bosses != RO_MS_ER_CROSS_GAME) ? EntranceType::TerminaBoss
                                                : (b.adult ? EntranceType::AdultBoss : EntranceType::ChildBoss);
            Entrance* door = PairExit(b.preBoss, b.lair);
            // Majora's Mask's lairs have no way back out but the blue warp: this one is never walked (false). It is
            // where a Hyrule boss room behind this door leads back to (the temple's entrance), and what SoH's boss
            // shuffle pairs a door with.
            Entrance* back = FindExit(b.lair, b.returnRegion);
            if (back == nullptr && door != nullptr) {
                Scaffold(b.returnRegion);
                back = AddExitTo(b.lair, b.returnRegion, [] { return false; }, "false (no way out of an MM lair)");
            }
            if (RegisterPair(door, back, b.keyDoor, b.keyReturn, type, decoupled, entranceMap, b.lairName)) {
                sBossReturns[b.boss] = back;
            }
        }
    }
    counted(KIND_BOSS);
    if (dungeons != RO_MS_ER_OFF || bosses != RO_MS_ER_OFF) {
        for (const TeBoss& b : kBosses) {
            RegisterBlueWarp(b, entranceMap);
        }
    }
    if (grottos != RO_MS_ER_OFF) {
        const EntranceType type =
            (grottos == RO_MS_ER_CROSS_GAME) ? EntranceType::GrottoGrave : EntranceType::TerminaGrotto;
        for (const TeGrotto& g : kGrottos) {
            Entrance* in = PairExit(g.outside, g.grotto);
            Entrance* out = FindExit(g.grotto, g.outside);
            if (out == nullptr && !CanScaffold(g.grotto) && RegionTable(g.grotto)->exits.size() == 1) {
                // 2Ship's logic names the way out by an entrance two grottos share (Great Bay Coast's); the grotto's
                // only exit is its way out whichever region the port made of it.
                out = &RegionTable(g.grotto)->exits.front();
            }
            if (out == nullptr) {
                out = PairExit(g.grotto, g.outside);
            }
            RegisterPair(in, out, g.keyIn, g.keyReturn, type, decoupled, entranceMap, g.grottoName);
        }
    }
    counted(KIND_GROTTO);
    // Overworld: both ways are Overworld (SoH's own overworld pairs are too); the pool takes both directions.
    if (overworld != RO_MS_ER_OFF) {
        const EntranceType type =
            (overworld == RO_MS_ER_CROSS_GAME) ? EntranceType::Overworld : EntranceType::TerminaOverworld;
        for (const TePair& p : kOverworld) {
            RegisterPair(PairExit(p.a, p.b), PairExit(p.b, p.a), p.keyIntoB, p.keyIntoA, type, decoupled, entranceMap,
                         p.nameAB);
        }
    }
    counted(KIND_OVERWORLD);
    if (interiors != RO_MS_ER_OFF) {
        const EntranceType type =
            (interiors == RO_MS_ER_CROSS_GAME) ? EntranceType::Interior : EntranceType::TerminaInterior;
        for (const TePair& p : kInteriors) {
            RegisterPair(PairExit(p.a, p.b), PairExit(p.b, p.a), p.keyIntoB, p.keyIntoA, type, decoupled, entranceMap,
                         p.nameBA);
        }
    }
    counted(KIND_INTERIOR);
    // The moon: one-way warps (every trial's way back is the moon field, which stays where it is).
    if (moon != RO_MS_ER_MOON_OFF) {
        for (const TeMoon& m : kMoon) {
            if (m.index == MOON_ROOF_INDEX && moon != RO_MS_ER_MOON_ROOFTOP) {
                continue;
            }
            Entrance* warp = PairExit(m.from, m.to);
            if (warp == nullptr) {
                Log("skipped %s: Termina's logic has no such exit", m.exitName);
                sSkipped++;
                continue;
            }
            Mark(warp, m.key, EntranceType::TerminaMoon, true, false, entranceMap);
            sRegistered++;
        }
    }
    counted(KIND_MOON);
    Log("%d Termina entrance pairs registered, %d skipped, %zu regions stood in for (dungeons %d bosses %d grottos %d "
        "overworld %d interiors %d moon %d)",
        sRegistered, sSkipped, sScaffolded.size(), dungeons, bosses, grottos, overworld, interiors, moon);
}

void MergeShip_Entrances_AddPools(std::map<EntranceType, std::vector<Entrance*>>& pools, bool decoupled) {
    auto add = [&](RandomizerSettingKey key, EntranceType type, EntranceType reverseType) {
        if (Mode(key) != RO_MS_ER_PER_GAME) {
            return;
        }
        std::vector<Entrance*> pool = GetShuffleableEntrances(type);
        if (pool.empty()) {
            return;
        }
        if (decoupled) {
            for (Entrance* entrance : pool) {
                pools[reverseType].push_back(entrance->GetReverse());
            }
        }
        pools[type] = std::move(pool);
    };
    add(RSK_MERGESHIP_ER_DUNGEONS, EntranceType::TerminaDungeon, EntranceType::TerminaDungeonReverse);
    add(RSK_MERGESHIP_ER_BOSSES, EntranceType::TerminaBoss, EntranceType::TerminaBossReverse);
    add(RSK_MERGESHIP_ER_GROTTOS, EntranceType::TerminaGrotto, EntranceType::TerminaGrottoReverse);
    add(RSK_MERGESHIP_ER_INTERIORS, EntranceType::TerminaInterior, EntranceType::TerminaInteriorReverse);
    // Overworld as SoH's own: both directions in one pool (coupled, placing one connects the other).
    if (Mode(RSK_MERGESHIP_ER_OVERWORLD) == RO_MS_ER_PER_GAME) {
        std::vector<Entrance*> pool = GetShuffleableEntrances(EntranceType::TerminaOverworld, false);
        if (!pool.empty()) {
            pools[EntranceType::TerminaOverworld] = std::move(pool);
        }
    }
    // The moon's one-way warps, among themselves whatever the mode (there is no Cross-Game).
    if (Mode(RSK_MERGESHIP_ER_MOON) != RO_MS_ER_MOON_OFF) {
        std::vector<Entrance*> pool = GetShuffleableEntrances(EntranceType::TerminaMoon);
        if (!pool.empty()) {
            pools[EntranceType::TerminaMoon] = std::move(pool);
        }
    }
}

bool MergeShip_Entrances_IsOneWayAssumed(EntranceType type) {
    return type == EntranceType::TerminaDungeon || type == EntranceType::TerminaGrotto ||
           type == EntranceType::TerminaInterior;
}

bool MergeShip_Entrances_SameTerminaScene(const Entrance* entrance, const Entrance* target) {
    if (entrance == nullptr || target == nullptr || entrance->GetType() == EntranceType::TerminaMoon) {
        return false;
    }
    const RandomizerRegion to = target->GetConnectedRegionKey();
    if (to == RR_NONE) {
        return false;
    }
    const char* from = MMScene(entrance->GetParentRegionKey());
    const char* into = MMScene(to);
    return (from != nullptr) && (into != nullptr) && (strcmp(from, into) == 0);
}

uint64_t MergeShip_Entrances_OpenTime(const Entrance& exit, uint64_t slices) {
    if (slices == 0 || !exit.IsShuffled() || !MS_ENTR_IS_TERMINA(exit.GetIndex())) {
        return slices;
    }
    // The door's own hours, and those of the interior now behind it (the door that used to lead there).
    const bool doorHours = ReadsMMTime(exit.GetConditionStr());
    const Entrance* vanillaDoor = exit.GetReplacement();
    const bool placeHours = (vanillaDoor != nullptr) && (vanillaDoor != &exit) && KeepsHours(vanillaDoor);
    if (!doorHours && !placeHours) {
        return slices;
    }
    MsTermina::TerminaLogic* mmLogic = MsTermina::mm;
    const uint64_t saved = mmLogic->time;
    uint64_t open = 0;
    for (int slice = 0; slice < MsTermina::MMT_COUNT; slice++) {
        const uint64_t bit = 1ULL << slice;
        if ((slices & bit) == 0) {
            continue;
        }
        mmLogic->time = bit;
        if ((!doorHours || exit.GetConditionsMet()) && (!placeHours || vanillaDoor->GetConditionsMet())) {
            open |= bit;
        }
    }
    mmLogic->time = saved;
    return open;
}

bool MergeShip_Entrances_ValidateWorld() {
    sLastValidation.clear();
    if (!MergeShip_Termina_Enabled()) {
        return true;
    }
    char line[256];
    auto refuse = [&]() {
        sLastValidation = line;
        SPDLOG_INFO("MergeShip entrances: final validation: {}", sLastValidation);
        return false;
    };
    // The world link: the Lost Woods door and the Clock Tower's passage, both ways, never move.
    const std::pair<RandomizerRegion, RandomizerRegion> link[] = {
        { RR_THE_LOST_WOODS, RR_LW_TERMINA_DOOR },
        { RR_MM_CLOCK_TOWER_INTERIOR, RR_LW_TERMINA_DOOR },
        { RR_MM_CLOCK_TOWN_SOUTH, RR_MM_CLOCK_TOWER_INTERIOR },
        { RR_MM_CLOCK_TOWER_INTERIOR, RR_MM_CLOCK_TOWN_SOUTH },
    };
    for (const auto& [from, to] : link) {
        const Entrance* exit = FindExit(from, to);
        if (exit == nullptr) {
            continue; // not in this build's logic
        }
        if (exit->IsShuffled() || exit->GetConnectedRegionKey() != to) {
            snprintf(line, sizeof(line), "the world link %s -> %s moved", RegionTable(from)->regionName.c_str(),
                     RegionTable(to)->regionName.c_str());
            return refuse();
        }
    }
    // Every shuffled Termina entrance leads somewhere, and the seed fits the override table.
    size_t shuffled = 0;
    for (Entrance* entrance : GetShuffleableEntrances(EntranceType::All, false)) {
        if (!entrance->IsShuffled() && entrance->GetType() != EntranceType::BlueWarp) {
            continue;
        }
        shuffled++;
        if (MS_ENTR_IS_TERMINA(entrance->GetIndex()) &&
            (entrance->GetConnectedRegionKey() == RR_NONE || entrance->GetReplacement() == nullptr)) {
            snprintf(line, sizeof(line), "Termina entrance 0x%04X (%s) leads nowhere", (uint16_t)entrance->GetIndex(),
                     entrance->GetName().c_str());
            return refuse();
        }
    }
    if (shuffled > ENTRANCE_OVERRIDES_MAX_COUNT) {
        snprintf(line, sizeof(line), "%zu shuffled entrances, the override table holds %d", shuffled,
                 ENTRANCE_OVERRIDES_MAX_COUNT);
        return refuse();
    }
    return true;
}

void MergeShip_Entrances_BeginFill() {
    sEntranceFailures = sPlacementFailures = 0;
    sLastFailure.clear();
    sLastValidation.clear();
}

void MergeShip_Entrances_AttemptFailed(bool entrances) {
    (entrances ? sEntranceFailures : sPlacementFailures)++;
}

void MergeShip_Entrances_FillFailed() {
    if (!MergeShip_Termina_Enabled()) {
        return;
    }
    auto ctx = Rando::Context::GetInstance();
    static const char* const kModes[] = { "Off", "Per Game", "Cross-Game" };
    static const char* const kMoonModes[] = { "Off", "Trials", "Trials & Rooftop" };
    auto mode = [&](RandomizerSettingKey key) { return kModes[std::min<int>(ctx->GetOption(key).Get(), 2)]; };
    char line[768];
    snprintf(line, sizeof(line),
             "Mergeship: no beatable unified seed after %d attempts (%d failed placing entrances, %d placing items or "
             "reaching the goal). Termina entrances: dungeons %s, bosses %s, grottos %s, overworld %s, interiors %s, "
             "moon %s; decoupled %s, mixed pools %s.%s%s Try fewer Cross-Game pools, or Decoupled / Mixed off.",
             sEntranceFailures + sPlacementFailures, sEntranceFailures, sPlacementFailures,
             mode(RSK_MERGESHIP_ER_DUNGEONS), mode(RSK_MERGESHIP_ER_BOSSES), mode(RSK_MERGESHIP_ER_GROTTOS),
             mode(RSK_MERGESHIP_ER_OVERWORLD), mode(RSK_MERGESHIP_ER_INTERIORS),
             kMoonModes[std::min<int>(ctx->GetOption(RSK_MERGESHIP_ER_MOON).Get(), 2)],
             ctx->GetOption(RSK_DECOUPLED_ENTRANCES) ? "on" : "off",
             ctx->GetOption(RSK_MIXED_ENTRANCE_POOLS) ? "on" : "off", sLastValidation.empty() ? "" : " Last check: ",
             sLastValidation.c_str());
    sLastFailure = line;
    SPDLOG_ERROR("{}", sLastFailure);
    MergeShipTest_Log("RANDOFAIL %s", line);
}

const std::string& MergeShip_Entrances_LastFailure() {
    return sLastFailure;
}

void MergeShip_Entrances_AddBlueWarpChains(std::map<std::string, Entrance*>& bossExits,
                                           std::map<std::string, Entrance*>& dungeonExits,
                                           std::vector<std::pair<Entrance*, Entrance*>>& pairs) {
    for (const TeBoss& b : kBosses) {
        Entrance* warp = sWarps[b.boss];
        if (warp == nullptr) {
            continue;
        }
        const TeDungeon& dungeon = kDungeons[b.dungeonId];
        // The dungeon's way out: its blue warp leads where the dungeon standing at that door has its own.
        Entrance* dungeonExit = FindExit(dungeon.inside, dungeon.outside);
        Entrance* bossReturn = sBossReturns[b.boss];
        if (dungeonExit != nullptr) {
            dungeonExits[dungeonExit->GetName()] = warp;
            if (bossReturn != nullptr) {
                bossExits[bossReturn->GetName()] = dungeonExit;
            }
        }
        Entrance* start = (bossReturn != nullptr) ? bossReturn : dungeonExit;
        if (start != nullptr) {
            pairs.push_back({ warp, start });
        }
    }
}

nlohmann::ordered_json MergeShip_Entrances_SeedSection(const nlohmann::ordered_json& ootSpoiler) {
    nlohmann::ordered_json section;
    section["version"] = MS_ENTR_SEED_VERSION;
    nlohmann::ordered_json termina = nlohmann::ordered_json::array();
    nlohmann::ordered_json hyrule = nlohmann::ordered_json::array();
    if (ootSpoiler.contains("entrances") && ootSpoiler["entrances"].is_array()) {
        for (const auto& entry : ootSpoiler["entrances"]) {
            const int index = entry.value("index", 0);
            const int override = entry.value("override", 0);
            if (MS_ENTR_IS_TERMINA(index)) {
                termina.push_back(nlohmann::ordered_json::array({ index, override }));
            } else if (MS_ENTR_IS_TERMINA(override)) {
                hyrule.push_back(nlohmann::ordered_json::array({ index, override }));
            }
        }
    }
    section["termina"] = std::move(termina);
    section["hyrule"] = std::move(hyrule);
    return section;
}

// ---- test harness ------------------------------------------------------------------------------------------------------

void MergeShip_Entrances_LoadTableFromContext() {
    MergeShipEntrances_ResetTable();
    for (const EntranceOverride& entry : Rando::Context::GetInstance()->GetEntranceShuffler()->entranceOverrides) {
        if (Entrance_EntranceIsNull(const_cast<EntranceOverride*>(&entry))) {
            break;
        }
        MergeShipEntrances_RegisterOverride(entry.index, entry.override);
    }
}

// The kind of a Termina key of the generated data (KIND_COUNT: none).
static int KindOfKey(int16_t key) {
    for (const TeDungeon& d : kDungeons) {
        if (key == d.keyIn || key == d.keyOut) {
            return KIND_DUNGEON;
        }
    }
    for (const TeBoss& b : kBosses) {
        if (key == b.keyDoor || key == b.keyReturn || key == b.keyWarp) {
            return KIND_BOSS;
        }
    }
    for (const TeGrotto& g : kGrottos) {
        if (key == g.keyIn || key == g.keyReturn) {
            return KIND_GROTTO;
        }
    }
    for (const TePair& p : kOverworld) {
        if (key == p.keyIntoA || key == p.keyIntoB) {
            return KIND_OVERWORLD;
        }
    }
    for (const TePair& p : kInteriors) {
        if (key == p.keyIntoA || key == p.keyIntoB) {
            return KIND_INTERIOR;
        }
    }
    for (const TeMoon& m : kMoon) {
        if (key == m.key) {
            return KIND_MOON;
        }
    }
    return KIND_COUNT;
}

std::vector<std::string> MergeShip_Entrances_Summary() {
    std::vector<std::string> out;
    auto ctx = Rando::Context::GetInstance();
    char line[512];
    snprintf(line, sizeof(line),
             "options dungeons=%d bosses=%d grottos=%d (soh dungeons=%d bosses=%d grottos=%d mixed=%d decoupled=%d) "
             "registered=%d skipped=%d standIns=%zu",
             Mode(RSK_MERGESHIP_ER_DUNGEONS), Mode(RSK_MERGESHIP_ER_BOSSES), Mode(RSK_MERGESHIP_ER_GROTTOS),
             (int)ctx->GetOption(RSK_SHUFFLE_DUNGEON_ENTRANCES).Get(), (int)ctx->GetOption(RSK_SHUFFLE_BOSS_ENTRANCES).Get(),
             (int)ctx->GetOption(RSK_SHUFFLE_GROTTO_ENTRANCES).Get(), (int)ctx->GetOption(RSK_MIXED_ENTRANCE_POOLS).Get(),
             (int)ctx->GetOption(RSK_DECOUPLED_ENTRANCES).Get(), sRegistered, sSkipped, sScaffolded.size());
    out.push_back(line);
    int total = 0, terminaToTermina = 0, terminaToHyrule = 0, hyruleToTermina = 0;
    std::vector<std::string> cross;
    for (const EntranceOverride& entry : ctx->GetEntranceShuffler()->entranceOverrides) {
        if (Entrance_EntranceIsNull(const_cast<EntranceOverride*>(&entry))) {
            break;
        }
        total++;
        const bool fromTermina = MS_ENTR_IS_TERMINA(entry.index);
        const bool toTermina = MS_ENTR_IS_TERMINA(entry.override);
        if (!fromTermina && !toTermina) {
            continue;
        }
        (fromTermina ? (toTermina ? terminaToTermina : terminaToHyrule) : hyruleToTermina)++;
        const EntranceData* src = EntranceTracker::GetEntranceData(entry.index);
        const EntranceData* dst = EntranceTracker::GetEntranceData(entry.override);
        snprintf(line, sizeof(line), "%s 0x%04X \"%s\" -> 0x%04X \"%s\" (type %d)",
                 fromTermina == toTermina ? "same" : "CROSS", (uint16_t)entry.index,
                 src != nullptr ? src->source.c_str() : "?", (uint16_t)entry.override,
                 dst != nullptr ? dst->destination.c_str() : "?", (int)entry.type);
        cross.push_back(line);
    }
    snprintf(line, sizeof(line), "overrides=%d terminaToTermina=%d terminaToHyrule=%d hyruleToTermina=%d table=%zu", total,
             terminaToTermina, terminaToHyrule, hyruleToTermina, sTable.size() / 2);
    out.push_back(line);
    // Overworld, interiors and the moon (a line of its own: the one above keeps its format for older checks).
    snprintf(line, sizeof(line),
             "more overworld=%d interiors=%d moon=%d (soh overworld=%d interiors=%d) kinds dungeon=%d boss=%d grotto=%d "
             "overworld=%d interior=%d moon=%d",
             Mode(RSK_MERGESHIP_ER_OVERWORLD), Mode(RSK_MERGESHIP_ER_INTERIORS), Mode(RSK_MERGESHIP_ER_MOON),
             (int)ctx->GetOption(RSK_SHUFFLE_OVERWORLD_ENTRANCES).Get(),
             (int)ctx->GetOption(RSK_SHUFFLE_INTERIOR_ENTRANCES).Get(), sKindRegistered[KIND_DUNGEON],
             sKindRegistered[KIND_BOSS], sKindRegistered[KIND_GROTTO], sKindRegistered[KIND_OVERWORLD],
             sKindRegistered[KIND_INTERIOR], sKindRegistered[KIND_MOON]);
    out.push_back(line);
    // Per kind of Termina entrance: how many overrides it has, how many lead to another kind of entrance of Termina,
    // how many cross to Hyrule (either way).
    int moved[KIND_COUNT + 1] = {}, otherKind[KIND_COUNT + 1] = {}, crossed[KIND_COUNT + 1] = {};
    for (const EntranceOverride& entry : ctx->GetEntranceShuffler()->entranceOverrides) {
        if (Entrance_EntranceIsNull(const_cast<EntranceOverride*>(&entry))) {
            break;
        }
        const bool fromTermina = MS_ENTR_IS_TERMINA(entry.index);
        const bool toTermina = MS_ENTR_IS_TERMINA(entry.override);
        if (!fromTermina && !toTermina) {
            continue;
        }
        const int kind = KindOfKey(fromTermina ? entry.index : entry.override);
        moved[kind]++;
        if (fromTermina != toTermina) {
            crossed[kind]++;
        } else if (KindOfKey(entry.override) != kind) {
            otherKind[kind]++;
        }
    }
    static const char* const kKindNames[KIND_COUNT + 1] = { "dungeon", "boss", "grotto", "overworld",
                                                            "interior", "moon", "other" };
    for (int kind = 0; kind <= KIND_COUNT; kind++) {
        snprintf(line, sizeof(line), "kind %s overrides=%d otherKind=%d cross=%d", kKindNames[kind], moved[kind],
                 otherKind[kind], crossed[kind]);
        out.push_back(line);
    }
    if (!sLastFailure.empty()) {
        out.push_back("failure " + sLastFailure);
    }
    // Doors and passages Majora's Mask opens only at some times: each keeps that condition wherever it now leads.
    int timed = 0;
    auto timedLine = [&](const TePair& p) {
        const Entrance* door = EntranceShuffler::GetEntranceByIndex(p.keyIntoB);
        if (door == nullptr || timed >= 12) {
            return;
        }
        const std::string& cond = door->GetConditionStr();
        if (cond.find("mm->Between") == std::string::npos && cond.find("mm->Before") == std::string::npos &&
            cond.find("mm->After") == std::string::npos && cond.find("mm->Is") == std::string::npos &&
            cond.find("Day()") == std::string::npos) {
            return;
        }
        const Entrance* to = door->GetReplacement();
        // hours=1: the interior now behind the door keeps its own opening hours too (MergeShip_Entrances_OpenTime)
        snprintf(line, sizeof(line), "timed 0x%04X \"%s\" -> 0x%04X \"%s\" hours=%d cond=%.160s", (uint16_t)p.keyIntoB,
                 p.nameAB, to != nullptr ? (uint16_t)to->GetIndex() : 0xFFFF,
                 door->GetConnectedRegion() != nullptr ? door->GetConnectedRegion()->regionName.c_str() : "?",
                 (to != nullptr && to != door && KeepsHours(to)) ? 1 : 0, cond.c_str());
        out.push_back(line);
        timed++;
    };
    for (const TePair& p : kInteriors) {
        timedLine(p);
    }
    for (const TePair& p : kOverworld) {
        timedLine(p);
    }
    // The moon's warps: where each now leads.
    for (const TeMoon& m : kMoon) {
        const Entrance* warp = EntranceShuffler::GetEntranceByIndex(m.key);
        if (warp == nullptr) {
            continue;
        }
        const Entrance* to = warp->GetReplacement();
        snprintf(line, sizeof(line), "moon %d 0x%04X \"%s\" -> 0x%04X \"%s\"", m.index, (uint16_t)m.key, m.exitName,
                 to != nullptr ? (uint16_t)to->GetIndex() : 0xFFFF,
                 warp->GetConnectedRegion() != nullptr ? warp->GetConnectedRegion()->regionName.c_str() : "?");
        out.push_back(line);
    }
    for (const std::string& l : cross) {
        out.push_back(l);
    }
    return out;
}

// Test harness commands (MergeShipTest.cpp):
//   randoer                      after randogen: the options, the Termina entrances registered and every override that
//                                touches Termina (MergeShip_Entrances_Summary)
//   ertable                      the runtime table from the last generated seed, as Entrance_Init builds it for a file
//   ertake <index|cross [dungeon|boss|grotto|overworld|interior]>
//                                take a shuffled entrance as the player would: its override, through
//                                Grotto_OverrideSpecialEntrance (a Termina one becomes the trip there). "cross": the
//                                first Hyrule entrance (of that kind) the seed sends to Termina.
bool MergeShip_Entrances_TestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) { return i < args.size() ? args[i] : std::string(); };
    if (op == "randoer") {
        for (const std::string& line : MergeShip_Entrances_Summary()) {
            MergeShipTest_Log("RANDOER %s", line.c_str());
        }
        return true;
    }
    if (op == "ertable") {
        MergeShip_Entrances_LoadTableFromContext();
        MergeShipTest_Log("ERTABLE %zu Termina overrides", sTable.size() / 2);
        for (size_t i = 0; i + 1 < sTable.size(); i += 2) {
            MergeShipTest_Log("ERTABLE 0x%04X -> 0x%04X", (uint16_t)sTable[i], (uint16_t)sTable[i + 1]);
        }
        return true;
    }
    if (op == "ertake") {
        MergeShip_Entrances_LoadTableFromContext();
        const auto& overrides = Rando::Context::GetInstance()->GetEntranceShuffler()->entranceOverrides;
        const EntranceOverride* chosen = nullptr;
        const std::string kind = arg(1);
        // The way in (a door in Hyrule's overworld), not the way back out of the place it leads to.
        auto entryIsForward = [](const EntranceOverride& entry) {
            const Entrance* entrance = EntranceShuffler::GetEntranceByIndex(entry.index);
            return (entrance != nullptr) && entrance->IsPrimary();
        };
        for (const EntranceOverride& entry : overrides) {
            if (Entrance_EntranceIsNull(const_cast<EntranceOverride*>(&entry))) {
                break;
            }
            if (arg(0) == "cross") {
                if (MS_ENTR_IS_TERMINA(entry.index) || !MS_ENTR_IS_TERMINA(entry.override)) {
                    continue;
                }
                const EntranceType type = (EntranceType)entry.type;
                const bool dungeon = type == EntranceType::Dungeon || type == EntranceType::GanonDungeon;
                const bool boss = type == EntranceType::ChildBoss || type == EntranceType::AdultBoss ||
                                  type == EntranceType::Boss;
                const bool grotto = type == EntranceType::GrottoGrave;
                const bool overworld = type == EntranceType::Overworld;
                const bool interior = type == EntranceType::Interior || type == EntranceType::SpecialInterior;
                if ((kind == "dungeon" && !dungeon) || (kind == "boss" && !boss) || (kind == "grotto" && !grotto) ||
                    (kind == "overworld" && !overworld) || (kind == "interior" && !interior) ||
                    !entryIsForward(entry)) {
                    continue;
                }
            } else if (entry.index != (int16_t)strtol(arg(0).c_str(), nullptr, 0)) {
                continue;
            }
            chosen = &entry;
            break;
        }
        if (chosen == nullptr || gPlayState == nullptr) {
            MergeShipTest_Log("ERTAKE %s %s: none", arg(0).c_str(), kind.c_str());
            return true;
        }
        const int16_t next = Grotto_OverrideSpecialEntrance(chosen->override);
        gPlayState->nextEntranceIndex = next;
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        const EntranceData* src = EntranceTracker::GetEntranceData(chosen->index);
        const EntranceData* dst = EntranceTracker::GetEntranceData(chosen->override);
        MergeShipTest_Log("ERTAKE index=0x%04X \"%s\" override=0x%04X \"%s\" -> next=0x%X", (uint16_t)chosen->index,
                          src != nullptr ? src->source.c_str() : "?", (uint16_t)chosen->override,
                          dst != nullptr ? dst->destination.c_str() : "?", next);
        return true;
    }
    return false;
}
