#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;

// z_sram_NES.c: the bits a Song of Time keeps. (Its struct is local to that file; the layout is the same.)
typedef struct AnchorPersistentCycleSceneFlags {
    u32 switch0;
    u32 switch1;
    u32 chest;
    u32 collectible;
} AnchorPersistentCycleSceneFlags;
extern AnchorPersistentCycleSceneFlags sPersistentCycleSceneFlags[SCENE_MAX];
extern u16 sPersistentCycleWeekEventRegs[100];
}

/**
 * MM_SET_FLAG / MM_UNSET_FLAG (SoH: SET_FLAG / UNSET_FLAG)
 *
 * Fired when a Majora's Mask flag is set or cleared: week events and the randomizer's flags (sceneNum -1), and the
 * current scene's switch, chest, cleared-room and collectible flags.
 *
 * Cycle-aware. Majora's Mask forgets most flags at a Song of Time (z_sram_NES.c, Sram_SaveEndOfCycle); each player
 * plays his own three days. A flag that survives the Song of Time ("persistent": a boss beaten, an owl statue, a
 * chest the game keeps) is shared at once; one that does not (a door opened on the first day, a cycle's quest step)
 * only between players in the same cycle (threeDayResetCount). Event flags that only mean something for a moment
 * (eventInf, the horse race state) are not sent.
 */

u16 MergeShipAnchor_Cycle() {
    return gSaveContext.save.saveInfo.playerData.threeDayResetCount;
}

bool MergeShipAnchor_FlagPersists(s16 sceneId, s16 flagType, s32 flag) {
    switch (flagType) {
        case FLAG_WEEK_EVENT_REG: {
            const s32 index = flag >> 8;
            const s32 mask = flag & 0xFF;
            if (index < 0 || index >= (s32)ARRAY_COUNT(sPersistentCycleWeekEventRegs)) {
                return false;
            }
            for (s32 bit = 0; bit < 8; bit++) {
                if (mask & (1 << bit)) {
                    return ((sPersistentCycleWeekEventRegs[index] >> (2 * bit)) & 3) != 0;
                }
            }
            return false;
        }
        case FLAG_RANDO_INF:
            return true;
        default:
            break;
    }

    if (sceneId < 0) {
        return false;
    }
    const s16 original = Play_GetOriginalSceneId(sceneId);
    if (original < 0 || original >= SCENE_MAX) {
        return false;
    }
    const AnchorPersistentCycleSceneFlags& keep = sPersistentCycleSceneFlags[original];
    switch (flagType) {
        case FLAG_CYCL_SCENE_SWITCH:
            if (flag >= 0 && flag < 0x20) {
                return (keep.switch0 & (1u << flag)) != 0;
            }
            if (flag >= 0x20 && flag < 0x40) {
                return (keep.switch1 & (1u << (flag - 0x20))) != 0;
            }
            return false;
        case FLAG_CYCL_SCENE_CHEST:
            return (flag >= 0 && flag < 0x20) && (keep.chest & (1u << flag)) != 0;
        case FLAG_CYCL_SCENE_COLLECTIBLE:
            return (flag >= 0 && flag < 0x20) && (keep.collectible & (1u << flag)) != 0;
        default:
            return false; // cleared rooms never outlive the cycle
    }
}

static bool FlagIsSynced(s16 flagType) {
    switch (flagType) {
        case FLAG_WEEK_EVENT_REG:
        case FLAG_RANDO_INF:
        case FLAG_CYCL_SCENE_SWITCH:
        case FLAG_CYCL_SCENE_CHEST:
        case FLAG_CYCL_SCENE_CLEARED_ROOM:
        case FLAG_CYCL_SCENE_COLLECTIBLE:
            return true;
        default:
            return false;
    }
}

static void SetBit(u32& word, s32 bit, bool set) {
    if (set) {
        word |= (1u << bit);
    } else {
        word &= ~(1u << bit);
    }
}

// Writes the flag directly (not through Flags_Set*, whose hooks would send it back).
static void ApplyMMFlag(bool isSaveLoaded, s16 sceneId, s16 flagType, s32 flag, bool set) {
    switch (flagType) {
        case FLAG_WEEK_EVENT_REG: {
            const s32 index = flag >> 8;
            const u8 mask = (u8)(flag & 0xFF);
            if (index < 0 || index >= (s32)ARRAY_COUNT(gSaveContext.save.saveInfo.weekEventReg)) {
                return;
            }
            if (set) {
                gSaveContext.save.saveInfo.weekEventReg[index] |= mask;
            } else {
                gSaveContext.save.saveInfo.weekEventReg[index] &= (u8)~mask;
            }
            return;
        }
        case FLAG_RANDO_INF: {
            const s32 index = flag >> 4;
            if (index < 0 || index >= (s32)ARRAY_COUNT(gSaveContext.save.shipSaveInfo.rando.randoInf)) {
                return;
            }
            if (set) {
                gSaveContext.save.shipSaveInfo.rando.randoInf[index] |= (1 << (flag & 0xF));
            } else {
                gSaveContext.save.shipSaveInfo.rando.randoInf[index] &= ~(1 << (flag & 0xF));
            }
            return;
        }
        default:
            break;
    }

    if (sceneId < 0 || sceneId >= SCENE_MAX) {
        return;
    }
    const bool current = isSaveLoaded && (gPlayState->sceneId == sceneId);
    const s16 original = Play_GetOriginalSceneId(sceneId);
    if (original < 0 || original >= SCENE_MAX) {
        return;
    }
    CycleSceneFlags& cycle = gSaveContext.cycleSceneFlags[original];
    // (Play_SaveCycleSceneFlags keeps Inverted Stone Tower Temple's collectibles and cleared rooms under its own id)
    CycleSceneFlags& cycleOwn = gSaveContext.cycleSceneFlags[sceneId];

    switch (flagType) {
        case FLAG_CYCL_SCENE_SWITCH:
            if (flag < 0 || flag >= 0x80) {
                return;
            }
            if (current) {
                SetBit(gPlayState->actorCtx.sceneFlags.switches[flag >> 5], flag & 0x1F, set);
            } else if (flag < 0x20) {
                SetBit(cycle.switch0, flag, set);
            } else if (flag < 0x40) {
                SetBit(cycle.switch1, flag - 0x20, set);
            }
            return;
        case FLAG_CYCL_SCENE_CHEST:
            if (flag < 0 || flag >= 0x20) {
                return;
            }
            if (current) {
                SetBit(gPlayState->actorCtx.sceneFlags.chest, flag, set);
            } else {
                SetBit(cycle.chest, flag, set);
            }
            return;
        case FLAG_CYCL_SCENE_CLEARED_ROOM:
            if (flag < 0 || flag >= 0x20) {
                return;
            }
            if (current) {
                SetBit(gPlayState->actorCtx.sceneFlags.clearedRoom, flag, set);
            } else {
                SetBit(cycleOwn.clearedRoom, flag, set);
            }
            return;
        case FLAG_CYCL_SCENE_COLLECTIBLE:
            if (flag < 0 || flag >= 0x80) {
                return;
            }
            if (current) {
                SetBit(gPlayState->actorCtx.sceneFlags.collectible[flag >> 5], flag & 0x1F, set);
            } else if (flag < 0x20) {
                SetBit(cycleOwn.collectible, flag, set);
            }
            return;
        default:
            return;
    }
}

static nlohmann::json PrepFlagPacket(const std::string& type, s16 sceneId, s16 flagType, s32 flag) {
    nlohmann::json payload;
    payload["type"] = type;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["sceneNum"] = sceneId;
    payload["flagType"] = flagType;
    payload["flag"] = flag;
    payload["cycle"] = MergeShipAnchor_Cycle();
    payload["persistent"] = MergeShipAnchor_FlagPersists(sceneId, flagType, flag);
    return payload;
}

void Anchor::SendPacket_MMSetFlag(s16 sceneId, s16 flagType, s32 flag) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || isProcessingIncomingPacket || !FlagIsSynced(flagType)) {
        return;
    }
    SendJsonToRemote(PrepFlagPacket(MM_SET_FLAG, sceneId, flagType, flag));
}

void Anchor::SendPacket_MMUnsetFlag(s16 sceneId, s16 flagType, s32 flag) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || isProcessingIncomingPacket || !FlagIsSynced(flagType)) {
        return;
    }
    SendJsonToRemote(PrepFlagPacket(MM_UNSET_FLAG, sceneId, flagType, flag));
}

static bool ShouldApply(const nlohmann::json& payload) {
    const bool persistent = payload.value("persistent", false);
    const u16 cycle = payload.value("cycle", (u16)0xFFFF);
    return persistent || cycle == MergeShipAnchor_Cycle();
}

void Anchor::HandlePacket_MMSetFlag(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !ShouldApply(payload)) {
        return;
    }
    const s16 flagType = payload.at("flagType").get<s16>();
    if (!FlagIsSynced(flagType)) {
        return;
    }
    ApplyMMFlag(true, payload.at("sceneNum").get<s16>(), flagType, payload.at("flag").get<s32>(), true);
}

void Anchor::HandlePacket_MMUnsetFlag(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !ShouldApply(payload)) {
        return;
    }
    const s16 flagType = payload.at("flagType").get<s16>();
    if (!FlagIsSynced(flagType)) {
        return;
    }
    ApplyMMFlag(true, payload.at("sceneNum").get<s16>(), flagType, payload.at("flag").get<s32>(), false);
}
