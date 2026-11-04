// MergeShip2 -- entrance shuffle over both worlds, 2 Ship 2 Harkinian's side (MergeShipEntrances.h).

#include "2s2h/MergeShip/MergeShipEntrances.h"
#include "2s2h/MergeShip/MergeShipHost.h"

#include <mergeship/EngineApi.h>
#include <mergeship/MsEntrances.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <variant>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "BenPort.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MergeShip/UnifiedSeed.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

// Enhancements/Cutscenes/StoryCutscenes/SkipGiantsChamber.cpp: the Giants' Chamber's bookkeeping when its cutscene is
// skipped (a giant freed, Oath to Order's check).
void handleGiantsCheck(SceneId sceneId);

namespace {

// ---- the generated data (tools/gen_mm_entrances.py) --------------------------------------------------------------------

struct MeExit {
    s16 key;
    s32 fromScene;
    u16 entrance;
};
struct MeGrotto {
    s32 n;
    u16 runtimeEntrance; // the spawn MM loads (4 and 10 stand for several of 2Ship's grottos)
    u8 data;             // the respawn data a hole gives (generic grottos: which one it is)
    u8 hasData;
    u16 outside;    // ENTRANCE(scene, 0) of the scene the hole is in
    u16 outsideAlt; // the same scene in another season, or 0xFFFF
    u16 returnEntrance;
};
struct MeBoss {
    s32 boss;
    s32 lairScene;
    s32 doorScene;
    s16 keyWarp;
    u16 warp;
    u16 warpAlt;
    u16 dungeonEntrance;
    u16 clearedFlag;
};

// clang-format off
const MeExit kExits[] = {
#define MS_ME_EXIT(key, from, entrance) { (s16)(key), from, (u16)(entrance) },
#define MS_ME_ARRIVE(...)
#define MS_ME_GROTTO(...)
#define MS_ME_BOSS(...)
#include "MergeShipEntranceData.inc"
#undef MS_ME_EXIT
#undef MS_ME_ARRIVE
#undef MS_ME_GROTTO
#undef MS_ME_BOSS
};
const MeGrotto kGrottos[] = {
#define MS_ME_EXIT(...)
#define MS_ME_ARRIVE(...)
#define MS_ME_GROTTO(n, runtime, data, hasData, outside, outsideAlt, ret) \
    { n, (u16)(runtime), (u8)(data), hasData, (u16)(outside), (u16)(outsideAlt), (u16)(ret) },
#define MS_ME_BOSS(...)
#include "MergeShipEntranceData.inc"
#undef MS_ME_EXIT
#undef MS_ME_ARRIVE
#undef MS_ME_GROTTO
#undef MS_ME_BOSS
};
const MeBoss kBosses[] = {
#define MS_ME_EXIT(...)
#define MS_ME_ARRIVE(...)
#define MS_ME_GROTTO(...)
#define MS_ME_BOSS(b, lair, door, kw, warp, warpAlt, dungeon, flag) \
    { b, lair, door, (s16)(kw), (u16)(warp), (u16)(warpAlt), (u16)(dungeon), (u16)(flag) },
#include "MergeShipEntranceData.inc"
#undef MS_ME_EXIT
#undef MS_ME_ARRIVE
#undef MS_ME_GROTTO
#undef MS_ME_BOSS
};
// clang-format on

// ---- state -------------------------------------------------------------------------------------------------------------

std::map<s16, s16> sOverrides;  // Termina key -> its override (a Termina key, or an Ocarina of Time index)
bool sHaveTable = false;        // a table came with a trip (or was read from the seed file)
bool sTriedSeedFile = false;
s32 sCurrentGrotto = -1;        // 2Ship's number of the grotto the player is in
bool sHoleSynthetic = false;    // the grotto respawn point is ours (arrived from Hyrule), not a hole's

void Log(const char* fmt, ...) {
    char line[400];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    SPDLOG_INFO("MergeShip entrances: {}", line);
    MergeShipHostTest_Log("ENTR %s", line);
}

u16 GrottoEntrance(s32 n) {
    return (u16)(((ENTR_SCENE_GROTTOS & 0x7F) << 9) | ((n & 0x1F) << 4));
}

bool SameScene(u16 a, u16 b) {
    return (b != 0xFFFF) && ((a >> 9) == (b >> 9));
}

// The seasons: 2Ship's logic (and so the generated rows) names Mountain Village, Goron Village, the Path to Goron Village
// and the Southern Swamp in winter / poisoned; MM swaps the scene at Play_Init (z_play.c) and a spring scene's exits may
// name either form. Exits are compared in the rows' form.
s32 SeasonScene(s32 sceneId) {
    switch (sceneId) {
        case SCENE_10YUKIYAMANOMURA2:
            return SCENE_10YUKIYAMANOMURA;
        case SCENE_11GORONNOSATO2:
            return SCENE_11GORONNOSATO;
        case SCENE_17SETUGEN2:
            return SCENE_17SETUGEN;
        case SCENE_20SICHITAI2:
            return SCENE_20SICHITAI;
        default:
            return sceneId;
    }
}

u16 SeasonEntrance(u16 entrance) {
    switch (entrance >> 9) {
        case ENTR_SCENE_MOUNTAIN_VILLAGE_SPRING:
            return (u16)((ENTR_SCENE_MOUNTAIN_VILLAGE_WINTER << 9) | (entrance & 0x1FF));
        case ENTR_SCENE_GORON_VILLAGE_SPRING:
            return (u16)((ENTR_SCENE_GORON_VILLAGE_WINTER << 9) | (entrance & 0x1FF));
        case ENTR_SCENE_PATH_TO_GORON_VILLAGE_SPRING:
            return (u16)((ENTR_SCENE_PATH_TO_GORON_VILLAGE_WINTER << 9) | (entrance & 0x1FF));
        case ENTR_SCENE_SOUTHERN_SWAMP_CLEARED:
            return (u16)((ENTR_SCENE_SOUTHERN_SWAMP_POISONED << 9) | (entrance & 0x1FF));
        default:
            return entrance;
    }
}

const MeGrotto* FindGrotto(s32 n) {
    for (const MeGrotto& g : kGrottos) {
        if (g.n == n) {
            return &g;
        }
    }
    return nullptr;
}

bool HoleIsOf(const MeGrotto& g, u16 hole) {
    return SameScene(hole, g.outside) || SameScene(hole, g.outsideAlt);
}

// Which of 2Ship's grottos a hole leads into: MM's spawn, and for spawns 4 and 10 (several grottos) the hole's scene
// and respawn data (2Ship's own numbering, Rando/Logic/Logic.cpp GetRegionIdFromEntrance).
s32 GrottoNumber(u16 runtimeEntrance, u16 hole, u8 data) {
    const u16 spawn = runtimeEntrance & ~0xF;
    int rows = 0;
    const MeGrotto* only = nullptr;
    const MeGrotto* sceneMatch = nullptr;
    for (const MeGrotto& g : kGrottos) {
        if (g.runtimeEntrance != spawn) {
            continue;
        }
        rows++;
        only = &g;
        if (HoleIsOf(g, hole)) {
            if (g.hasData && g.data == data) {
                return g.n;
            }
            if (sceneMatch == nullptr) {
                sceneMatch = &g;
            }
        }
    }
    if (rows == 1) {
        return only->n;
    }
    return (sceneMatch != nullptr) ? sceneMatch->n : -1;
}

// No trip brought a table this session: the seed file's "entrances" section, if the host named one.
void EnsureTable() {
    if (sHaveTable || sTriedSeedFile) {
        return;
    }
    sTriedSeedFile = true;
    // The seed the last trip from Hyrule named (or, before any trip, MERGESHIP_UNIFIED_SEED_CVAR): UnifiedSeed.h.
    const std::filesystem::path path = Rando::MergeShip::MergedSeedPath();
    if (path.empty()) {
        return;
    }
    try {
        std::ifstream stream(path);
        if (!stream.is_open()) {
            return;
        }
        nlohmann::json seed;
        stream >> seed;
        if (!seed.contains("entrances") || seed["entrances"].value("version", 0) != MS_ENTR_SEED_VERSION) {
            return;
        }
        for (const auto& pair : seed["entrances"].value("termina", nlohmann::json::array())) {
            if (pair.is_array() && pair.size() == 2) {
                sOverrides[(s16)pair[0].get<int>()] = (s16)pair[1].get<int>();
            }
        }
        sHaveTable = true;
        Log("%zu overrides read from the seed file %s", sOverrides.size(), path.string().c_str());
    } catch (const std::exception& e) { Log("seed file %s unreadable: %s", path.string().c_str(), e.what()); }
}

// A blue warp that leads elsewhere does what MM's warp and 2Ship's Giants' Chamber skip would have done on the way.
void FinishBlueWarp(PlayState* play, const MeBoss& boss) {
    if ((play->nextEntrance & ~0xF) == ENTRANCE(GIANTS_CHAMBER, 0)) {
        // 2Ship's randomizer queued the warp's real destination to skip the chamber at its Play_Init; it is not going
        // there, so take the queued transition out and do the chamber's bookkeeping now.
        auto& events = GameInteractor::Instance->events;
        auto it = std::find_if(events.begin(), events.end(),
                               [](const GIEvent& e) { return std::holds_alternative<GIEventTransition>(e); });
        if (it != events.end()) {
            const GIEventTransition transition = std::get<GIEventTransition>(*it);
            events.erase(it);
            handleGiantsCheck((SceneId)transition.transitionType);
        }
    }
    SET_WEEKEVENTREG(boss.clearedFlag);
    if (boss.boss == MS_ENTR_BOSS_ODOLWA) {
        SET_WEEKEVENTREG(WEEKEVENTREG_ENTERED_WOODFALL_TEMPLE_PRISON);
    }
    // The clear cutscene belongs to the destination MM had in mind.
    gSaveContext.nextCutsceneIndex = 0xFFEF;
}

u16 ArriveImpl(s16 key, bool fromHole, bool boot) {
    if (MS_ENTR_IS_GROTTO_RETURN(key)) {
        const MeGrotto* g = FindGrotto(MS_ENTR_KEY_INDEX(key));
        const RespawnData& hole = gSaveContext.respawn[RESPAWN_MODE_UNK_3];
        sCurrentGrotto = -1;
        if (!boot && !sHoleSynthetic && (g != nullptr) && HoleIsOf(*g, hole.entrance)) {
            // Back out of the hole the player went in through, as MM's grotto exit does.
            gSaveContext.respawnFlag = 4;
            return hole.entrance;
        }
        gSaveContext.respawnFlag = 0;
        return (g != nullptr) ? g->returnEntrance : (u16)ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    }
    if (MS_ENTR_IS_BOSS_RETURN(key)) {
        sCurrentGrotto = -1;
        gSaveContext.respawnFlag = 0;
        for (const MeBoss& b : kBosses) {
            if (b.boss == MS_ENTR_KEY_INDEX(key)) {
                return b.dungeonEntrance;
            }
        }
        return ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    }
    const u16 entrance = MS_ENTR_KEY_TO_MM(key);
    gSaveContext.respawnFlag = 0;
    if ((entrance >> 9) != ENTR_SCENE_GROTTOS) {
        sCurrentGrotto = -1;
        return entrance;
    }
    const s32 n = (entrance >> 4) & 0x1F;
    const MeGrotto* g = FindGrotto(n);
    sCurrentGrotto = n;
    if (g == nullptr) {
        return entrance;
    }
    RespawnData& respawn = gSaveContext.respawn[RESPAWN_MODE_UNK_3];
    if (g->hasData) {
        respawn.data = (s8)g->data; // which generic grotto this is (its chest, its cows, its grass)
    }
    if (!fromHole) {
        // No hole of this session leads here (the player came from Hyrule): the grotto's way out goes where the seed
        // says (Arrive(GROTTO_RETURN) never trusts the respawn point then). The point itself is left as it was, so
        // MM's own use of it still finds a position that belongs to its scene.
        sHoleSynthetic = true;
    }
    return g->runtimeEntrance;
}

} // namespace

extern "C" s32 MergeShipEntrances_TableSize(void) {
    return (s32)sOverrides.size();
}

extern "C" u16 MergeShipEntrances_Arrive(s16 key, s32 boot) {
    EnsureTable();
    const u16 entrance = ArriveImpl(key, false, boot != 0);
    // No cutscene of its own ("none"): also what lets Play_Init swap a winter / poisoned arrival to the scene's spring /
    // cleansed form when that temple is cleared (z_play.c), as for any of MM's own exits.
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    Log("arrival from Hyrule: key 0x%X -> entrance 0x%X%s", key, entrance, boot ? " (loading the file)" : "");
    return entrance;
}

extern "C" s32 MergeShipEntrances_OnCommit(PlayState* play) {
    EnsureTable();
    if (sOverrides.empty() || (gSaveContext.gameMode != GAMEMODE_NORMAL)) {
        return -1;
    }
    const u16 next = play->nextEntrance;
    const u16 nextBase = next & ~0xF;
    const s32 respawn = gSaveContext.respawnFlag;
    const bool exitTaken = (respawn == 0) || (respawn == -2);
    const bool inGrotto = (play->sceneId == SCENE_KAKUSIANA);
    bool enteringGrotto = false;
    const MeBoss* warp = nullptr;
    s16 key = -1;

    if (!inGrotto && exitTaken && ((next >> 9) == ENTR_SCENE_GROTTOS)) {
        // Into a grotto: Door_Ana has just recorded the hole as the grotto's way back.
        const RespawnData& hole = gSaveContext.respawn[RESPAWN_MODE_UNK_3];
        const s32 n = GrottoNumber(next, hole.entrance, (u8)hole.data);
        enteringGrotto = true;
        sHoleSynthetic = false;
        sCurrentGrotto = n;
        if (n >= 0) {
            key = MS_ENTR_KEY_MM(GrottoEntrance(n));
        }
    } else if (inGrotto && (exitTaken || respawn == 4)) {
        // Out of a grotto (its way out, or a real exit of one).
        if (sCurrentGrotto >= 0) {
            key = MS_ENTR_KEY_GROTTO_RETURN(sCurrentGrotto);
        }
        sCurrentGrotto = -1;
    } else {
        for (const MeBoss& b : kBosses) {
            if ((play->sceneId == b.lairScene) &&
                ((nextBase == ENTRANCE(GIANTS_CHAMBER, 0)) || (nextBase == b.warp) || (nextBase == b.warpAlt))) {
                warp = &b;
                key = b.keyWarp;
                break;
            }
        }
        if ((warp == nullptr) && exitTaken) {
            // Overworld, interior and moon exits too: a spring scene's exit is its winter row (SeasonScene).
            const s32 fromScene = SeasonScene(play->sceneId);
            const u16 exitBase = SeasonEntrance(nextBase);
            for (const MeExit& e : kExits) {
                if ((e.fromScene == fromScene) && (e.entrance == exitBase)) {
                    key = e.key;
                    break;
                }
            }
        }
    }
    if (key < 0) {
        return -1;
    }
    auto it = sOverrides.find(key);
    if ((it == sOverrides.end()) || (it->second == key)) {
        return -1; // not moved
    }
    const s16 target = it->second;
    if (warp != nullptr) {
        FinishBlueWarp(play, *warp);
    }
    // A cutscene the exit queued (a temple's entry, a warp's clear scene) belongs to the entrance it replaces.
    gSaveContext.nextCutsceneIndex = 0xFFEF;
    if (MS_ENTR_IS_TERMINA(target)) {
        play->nextEntrance = ArriveImpl(target, enteringGrotto, false);
        Log("scene %d entrance 0x%X (key 0x%X) -> Termina key 0x%X: entrance 0x%X", play->sceneId, next, key, target,
            play->nextEntrance);
        return -1;
    }
    sCurrentGrotto = -1;
    Log("scene %d entrance 0x%X (key 0x%X) -> Hyrule entrance 0x%X", play->sceneId, next, key, target);
    return target;
}

void MergeShipEntrances_OnTransfer(const MsTransfer* arrival) {
    if ((arrival != nullptr) && (arrival->from == MS_ENGINE_SOH) && (arrival->entrance >= 0) &&
        MS_TRANSFER_HAS(arrival, unifiedSeedFile) && !(arrival->flags & MS_TRANSFER_FLAG_ENTRANCE_TABLE)) {
        // A trip from a Hyrule file whose seed shuffles none of Termina's entrances (or that plays no unified seed):
        // an earlier trip's table (another file's) no longer applies.
        if (!sOverrides.empty()) {
            Log("%zu overrides of Termina's entrances dropped: this Hyrule file's trip brought none", sOverrides.size());
        }
        sOverrides.clear();
        sHaveTable = true;
        return;
    }
    if ((arrival == nullptr) || !(arrival->flags & MS_TRANSFER_FLAG_ENTRANCE_TABLE) ||
        !MS_TRANSFER_HAS(arrival, entranceTableCount)) {
        return;
    }
    sOverrides.clear();
    for (int32_t i = 0; (arrival->entranceTable != nullptr) && (i < arrival->entranceTableCount); i++) {
        sOverrides[arrival->entranceTable[i * 2]] = arrival->entranceTable[i * 2 + 1];
    }
    sHaveTable = true;
    Log("%zu overrides of Termina's entrances came from Hyrule", sOverrides.size());
}

std::vector<std::string> MergeShipEntrances_Describe() {
    EnsureTable();
    std::vector<std::string> out;
    char line[160];
    snprintf(line, sizeof(line), "table=%zu fromTrip=%d grotto=%d holeSynthetic=%d", sOverrides.size(),
             (int)sHaveTable, sCurrentGrotto, (int)sHoleSynthetic);
    out.push_back(line);
    for (const auto& [key, target] : sOverrides) {
        snprintf(line, sizeof(line), "0x%04X -> 0x%04X %s", (u16)key, (u16)target,
                 key == target ? "(itself)" : (MS_ENTR_IS_TERMINA(target) ? "(Termina)" : "(HYRULE)"));
        out.push_back(line);
    }
    return out;
}

bool MergeShipEntrances_ExitOfKey(s16 key, s32* scene, u16* entrance) {
    for (const MeExit& e : kExits) {
        if (e.key == key) {
            *scene = e.fromScene;
            *entrance = e.entrance;
            return true;
        }
    }
    for (const MeBoss& b : kBosses) {
        if (b.keyWarp == key) {
            *scene = b.lairScene;
            *entrance = b.warp;
            return true;
        }
    }
    return false;
}

// Test harness commands (MergeShipHostTest.cpp):
//   ertable            the overrides this Termina knows (from the last trip, or the seed file)
//   erexit [key|hyrule] take the exit of `key` (or, without one, this scene's: a grotto's way out, a boss lair's blue
//                      warp, else the first exit of the generated table that leaves this scene and is overridden; with
//                      "hyrule", the first exit of this scene that leads to Hyrule), as the player would;
//                      MergeShipHost_HoldTransition applies the override when the fade-out commits
//   erwarp <entrance>  a plain scene change (no exit of the table: nothing is overridden), to stand somewhere
bool MergeShipEntrances_TestCommand(const std::string& op, const std::vector<std::string>& args) {
    auto arg = [&](size_t i) { return i < args.size() ? args[i] : std::string(); };
    if (op == "ertable") {
        for (const std::string& line : MergeShipEntrances_Describe()) {
            MergeShipHostTest_Log("ERTABLE %s", line.c_str());
        }
        return true;
    }
    if ((op == "erexit") || (op == "erwarp")) {
        if (gPlayState == nullptr) {
            MergeShipHostTest_Log("%s: no game", op == "erexit" ? "EREXIT" : "ERWARP");
            return true;
        }
        PlayState* play = gPlayState;
        u16 entrance = 0xFFFF;
        s32 scene = SeasonScene(play->sceneId);
        s16 key = arg(0).empty() ? (s16)-1 : (s16)strtol(arg(0).c_str(), nullptr, 0);
        s32 respawnFlag = 0;
        if (op == "erwarp") {
            entrance = (u16)strtol(arg(0).c_str(), nullptr, 0);
            key = -1;
        } else if (arg(0) == "hyrule") {
            // The first exit of this scene whose override leads to Hyrule (the way back of a Cross-Game arrival).
            key = -1;
            for (const MeExit& e : kExits) {
                if ((e.fromScene == SeasonScene(play->sceneId)) && sOverrides.count(e.key) &&
                    !MS_ENTR_IS_TERMINA(sOverrides[e.key])) {
                    entrance = e.entrance;
                    key = e.key;
                    break;
                }
            }
        } else if (key >= 0) {
            if (!MergeShipEntrances_ExitOfKey(key, &scene, &entrance)) {
                MergeShipHostTest_Log("EREXIT key 0x%X: no such exit", key);
                return true;
            }
        } else if (play->sceneId == SCENE_KAKUSIANA) {
            // A grotto's way out: back to the hole's respawn point, as MM's grotto exit does (0xFFFF).
            entrance = gSaveContext.respawn[RESPAWN_MODE_UNK_3].entrance;
            respawnFlag = 4;
        } else {
            for (const MeBoss& b : kBosses) {
                if (b.lairScene == play->sceneId) {
                    entrance = b.warp;
                }
            }
            for (const MeExit& e : kExits) {
                if ((entrance == 0xFFFF) && (e.fromScene == SeasonScene(play->sceneId)) && sOverrides.count(e.key) &&
                    (sOverrides[e.key] != e.key)) {
                    entrance = e.entrance;
                    key = e.key;
                }
            }
        }
        if (entrance == 0xFFFF) {
            MergeShipHostTest_Log("EREXIT scene %d: no shuffled exit here", play->sceneId);
            return true;
        }
        if (scene != SeasonScene(play->sceneId)) {
            MergeShipHostTest_Log("EREXIT key 0x%X leaves scene %d, the player is in scene %d: not taken", key, scene,
                                  play->sceneId);
            return true;
        }
        play->nextEntrance = entrance;
        gSaveContext.respawnFlag = respawnFlag;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = (respawnFlag == 4) ? TRANS_TYPE_FADE_WHITE : TRANS_TYPE_FADE_BLACK;
        MergeShipHostTest_Log("%s scene=%d key=0x%X entrance=0x%X respawnFlag=%d", op == "erexit" ? "EREXIT" : "ERWARP",
                              play->sceneId, key, entrance, respawnFlag);
        return true;
    }
    return false;
}
