#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/JsonConversions.hpp"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip_Own_ChuCapacity, MergeShipShared_BottleFromItem
#include "2s2h/MergeShip/MergeShipSharedSave.h"
#include "2s2h/Rando/Rando.h"
#include <mergeship/AnchorHost.hpp>   // MsAnchorHost::Seq
#include <mergeship/AnchorReplay.hpp> // MsAnchor_ReplaySeq
#include <mergeship/MsDelivery.h>     // MsDelivery_BottleOnePerWorld
#include <mergeship/SharedSaveJson.hpp>
#include <nlohmann/json.hpp>

#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "macros.h"
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
extern u16 sPersistentCycleWeekEventRegs[100]; // z_sram_NES.c
}

/**
 * UPDATE_TEAM_STATE
 *
 * Pushes the current save state to the server for other teammates to use.
 *
 * Fires when the server passes on a REQUEST_TEAM_STATE packet, or when this client saves the game (Majora's Mask: the
 * Song of Time's save)
 *
 * When sending this packet we will assume that the team queue has been emptied for this client, so the queue
 * stored in the server will be cleared.
 *
 * When receiving this packet, if there is items in the team queue, we will play them back in order.
 *
 * One team state on the server holds both worlds: Hyrule's part is SoH's SaveContext, at the top of "state", as SoH
 * sends it; Termina's is "state"."mergeShipMM" (PrepTeamState below). Each game applies its own part and carries the
 * other's along when it sends. Termina's part is merged, never taken away: what outlives a Song of Time (masks, songs,
 * items, heart containers, persistent flags) always, what belongs to three days only between players in the same
 * cycle.
 *
 * MergeShip2 (the user, 2026-09-30: "when anyone joins, they should get EXACTLY what any player has. DOWN to a T"):
 * the state also carries the shared save's progress ("state"."mergeShipShared", SharedSaveJson.hpp) -- what each world
 * holds of the other's, item ownership and the one ammo pool -- merged by the engine the player is in and applied to
 * it at once. A join answer (the answer to our own request, Anchor::MergeShipIsJoinAnswer) also raises the counters
 * that go down as they are spent -- rupees and small keys -- to the team's, never lowering what the player has.
 */

// MergeShip2 (mergeship.exe): the slots Majora's Mask shares with Hyrule (MergeShipOwnership.h), in MsOwnBit order
// MS_OWN_BOMB .. MS_OWN_BOW, with the upgrade that sets each one's capacity and its MS_BAGS field (-1: none).
static const struct {
    u8 item;
    s8 upgrade;
    s8 bagsField;
} kSharedSlots[] = {
    { ITEM_BOMB, UPG_BOMB_BAG, MS_BAGS_FIELD_BOMB_BAG },
    { ITEM_BOMBCHU, -1, -1 },
    { ITEM_DEKU_STICK, UPG_DEKU_STICKS, MS_BAGS_FIELD_STICKS },
    { ITEM_DEKU_NUT, UPG_DEKU_NUTS, MS_BAGS_FIELD_NUTS },
    { ITEM_MAGIC_BEANS, -1, -1 },
    { ITEM_BOW, UPG_QUIVER, MS_BAGS_FIELD_QUIVER },
};
static_assert(std::size(kSharedSlots) == MS_OWN_BOW - MS_OWN_BOMB + 1, "one per MsOwnBit MS_OWN_BOMB .. MS_OWN_BOW");

// The shared save whose ownership bits say which of those Majora's Mask owns itself (terminaOwn, terminaBags), or NULL
// (2ship.exe, a Termina file of no Hyrule file, or bits not written yet): then every item in a slot is Majora's Mask's.
// The same save the ownership code reads (MergeShipOoTItems.c MsOwn_Shared, which writes the bits on the first frame).
static MsSharedSave* OwnershipShared() {
    MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
    return ((shared != nullptr) && (shared->ownValid & MS_OWN_VALID_TERMINA)) ? shared : nullptr;
}

// The kSharedSlots index (MsOwnBit - MS_OWN_BOMB) of an inventory slot, -1 for any other slot.
static s32 SharedSlotIndex(size_t slot) {
    for (s32 i = 0; i < (s32)std::size(kSharedSlots); i++) {
        if ((size_t)SLOT(kSharedSlots[i].item) == slot) {
            return i;
        }
    }
    return -1;
}

// MergeShip2 (mergeship.exe): Majora's Mask's bomb .. bow slots also hold Hyrule's items, usable here
// (MergeShip_Own_Sync), at the better bag either game has found. Termina's part carries only what Majora's Mask owns
// itself, at its own bags (MergeShip_Own_MMUpgrade): Hyrule's are in Hyrule's part, and a teammate without the
// ownership system (2ship.exe) would take a placeholder as Majora's Mask's -- a Hero's Bow, its quiver and arrows for a
// player who has only the Fairy Bow (the mirror of the Hyrule placeholders SoH strips from its part).
static void KeepTerminaOwn(std::vector<u8>& items, std::vector<s8>& ammo, u32& upgrades) {
    const MsSharedSave* shared = OwnershipShared();
    if (shared == nullptr) {
        return;
    }
    for (const auto& slot : kSharedSlots) {
        const bool owned = MergeShip_Own_MMOwns(slot.item);
        const size_t i = (size_t)SLOT(slot.item);
        if (!owned && (i < items.size()) && (items[i] == slot.item)) {
            items[i] = ITEM_NONE;
            if (i < ammo.size()) {
                ammo[i] = 0;
            }
        }
        if (slot.upgrade < 0) {
            continue;
        }
        u32 level = ((slot.upgrade == UPG_QUIVER) || (slot.upgrade == UPG_BOMB_BAG))
                        ? (u32)MergeShip_Own_MMUpgrade(slot.upgrade)
                        : (u32)MS_BAGS_LEVEL(shared->terminaBags, slot.bagsField);
        if (owned && (level == 0)) {
            level = 1; // an item held has at least its first capacity (MergeShipOoTItems.c MsOwn_Level)
        }
        upgrades = (upgrades & ~gUpgradeMasks[slot.upgrade]) |
                   ((level << gUpgradeShifts[slot.upgrade]) & gUpgradeMasks[slot.upgrade]);
    }
}

// Cycle scene flags as five numbers per scene, with the current scene's live flags.
static std::vector<u32> CycleSceneFlagsArray() {
    std::vector<u32> flags;
    for (s32 i = 0; i < (s32)ARRAY_COUNT(gSaveContext.cycleSceneFlags); i++) {
        CycleSceneFlags f = gSaveContext.cycleSceneFlags[i];
        if (gPlayState != nullptr && Play_GetOriginalSceneId(gPlayState->sceneId) == i) {
            f.chest = gPlayState->actorCtx.sceneFlags.chest;
            f.switch0 = gPlayState->actorCtx.sceneFlags.switches[0];
            f.switch1 = gPlayState->actorCtx.sceneFlags.switches[1];
            f.clearedRoom = gPlayState->actorCtx.sceneFlags.clearedRoom;
            f.collectible = gPlayState->actorCtx.sceneFlags.collectible[0];
        }
        flags.insert(flags.end(), { f.chest, f.switch0, f.switch1, f.clearedRoom, f.collectible });
    }
    return flags;
}

nlohmann::json Anchor::PrepTeamState() {
    const SaveInfo& info = gSaveContext.save.saveInfo;
    nlohmann::json mm;
    mm["cycle"] = MergeShipAnchor_Cycle();
    mm["weekEventReg"] = std::vector<u8>(std::begin(info.weekEventReg), std::end(info.weekEventReg));
    mm["cycleSceneFlags"] = CycleSceneFlagsArray();
    std::vector<u32> permanent;
    for (const PermanentSceneFlags& f : info.permanentSceneFlags) {
        permanent.insert(permanent.end(), { f.chest, f.switch0, f.switch1, f.clearedRoom, f.collectible });
    }
    mm["permanentSceneFlags"] = permanent;
    mm["randoInf"] = std::vector<u16>(std::begin(gSaveContext.save.shipSaveInfo.rando.randoInf),
                                      std::end(gSaveContext.save.shipSaveInfo.rando.randoInf));
    // MergeShip2: Majora's Mask's own items, ammunition and bags only (KeepTerminaOwn).
    std::vector<u8> items(std::begin(info.inventory.items), std::end(info.inventory.items));
    std::vector<s8> ammo(std::begin(info.inventory.ammo), std::end(info.inventory.ammo));
    u32 upgrades = info.inventory.upgrades;
    KeepTerminaOwn(items, ammo, upgrades);
    mm["items"] = items;
    mm["upgrades"] = upgrades;
    mm["questItems"] = info.inventory.questItems;
    mm["dungeonItems"] = std::vector<u8>(std::begin(info.inventory.dungeonItems), std::end(info.inventory.dungeonItems));
    mm["dungeonKeys"] = std::vector<s8>(std::begin(info.inventory.dungeonKeys), std::end(info.inventory.dungeonKeys));
    mm["strayFairies"] = std::vector<s8>(std::begin(info.inventory.strayFairies), std::end(info.inventory.strayFairies));
    mm["healthCapacity"] = info.playerData.healthCapacity;
    mm["isMagicAcquired"] = info.playerData.isMagicAcquired;
    mm["isDoubleMagicAcquired"] = info.playerData.isDoubleMagicAcquired;
    mm["doubleDefense"] = info.playerData.doubleDefense;
    mm["owlActivationFlags"] = info.playerData.owlActivationFlags;
    // MergeShip2: the rest of a player's Termina (ApplyTeamState). The bottles and the trade slots are in "items".
    // (Not unk_EA0 or unk_EA8: neither is a set of flags two players' could be ORed into. unk_EA0 is four 3-bit song
    // ids, one per Gossip Stone (z_en_gs.c), that the Song of Time clears: ORed, a set nobody lit completes and pays
    // out. unk_EA8 is each Giant's freeing order and the count freed (z_door_warp1.c): ORed, the order and count are
    // wrong and the Giants' Chamber picks the wrong cutscene. The remains in questItems already carry the progress.)
    mm["ammo"] = ammo;
    mm["rupees"] = info.playerData.rupees;
    mm["skullTokenCount"] = info.skullTokenCount;
    mm["scenesVisible"] = std::vector<u32>(std::begin(info.scenesVisible), std::end(info.scenesVisible));
    // The dungeon maps' floors and rooms visited (PermanentSceneFlags unk_14 and rooms), two numbers per scene.
    std::vector<u32> maps;
    for (const PermanentSceneFlags& f : info.permanentSceneFlags) {
        maps.insert(maps.end(), { f.unk_14, f.rooms });
    }
    mm["permanentSceneMaps"] = maps;
    if (IS_RANDO) {
        nlohmann::json checks = nlohmann::json::array();
        for (auto& [rc, check] : Rando::StaticData::Checks) {
            const RandoSaveCheck& saved = RANDO_SAVE_CHECKS[rc];
            if (saved.obtained || saved.skipped) {
                checks.push_back({ check.name, saved.obtained, saved.skipped });
            }
        }
        mm["randoChecks"] = checks;
    }
    return mm;
}

// A bottle content Termina keeps one of (MsDelivery.h MsDelivery_BottleOnePerWorld: the Deku Princess, the Seahorse)
// that it has already: in one of its six bottles or Hyrule's four on the Items (Hyrule) page, or -- the Deku Princess --
// back in the Deku King's chamber this cycle (MM EnDnp_Init, WEEKEVENTREG_23_20). A teammate's bottle of it comes empty.
static bool TerminaHasOnePerWorld(u8 item) {
    if (!MsDelivery_BottleOnePerWorld(MergeShipShared_BottleFromItem(item))) {
        return false;
    }
    if (Inventory_HasItemInBottle(item)) {
        return true;
    }
    for (s32 i = 0; i < 4; i++) {
        if (MergeShip_Own_OoTBottle(i) == item) {
            return true;
        }
    }
    return (item == ITEM_DEKU_PRINCESS) && CHECK_WEEKEVENTREG(WEEKEVENTREG_23_20);
}

// The scenes whose PermanentSceneFlags unk_14 and rooms are the dungeon map's floors and rooms visited (z_map_exp.c:
// the four temples and their boss lairs; Stone Tower's inverted temple is SCENE_INISIE_N's). Elsewhere unk_14 holds an
// actor's own state -- the Zora Eggs counted in the Marine Research Lab, the smithy's sword, a Great Fairy's fountain
// -- which no OR of two players' can make right.
static bool IsDungeonMapScene(s32 sceneId) {
    switch (sceneId) {
        case SCENE_MITURIN:
        case SCENE_HAKUGIN:
        case SCENE_SEA:
        case SCENE_INISIE_N:
        case SCENE_MITURIN_BS:
        case SCENE_HAKUGIN_BS:
        case SCENE_SEA_BS:
        case SCENE_INISIE_BS:
            return true;
        default:
            return false;
    }
}

bool Anchor::ApplyTeamState(const nlohmann::json& mm, bool join) {
    SaveInfo& info = gSaveContext.save.saveInfo;
    const bool sameCycle = mm.value("cycle", (u16)0xFFFF) == MergeShipAnchor_Cycle();

    // What the save was, to tell whether the team had anything this player did not ("Save updated from team").
    std::vector<u8> saveBefore(sizeof(gSaveContext.save));
    memcpy(saveBefore.data(), &gSaveContext.save, sizeof(gSaveContext.save));
    std::vector<u8> cycleBefore(sizeof(gSaveContext.cycleSceneFlags));
    memcpy(cycleBefore.data(), gSaveContext.cycleSceneFlags, sizeof(gSaveContext.cycleSceneFlags));
    std::vector<u8> liveBefore;
    if (IsSaveLoaded()) {
        liveBefore.resize(sizeof(gPlayState->actorCtx.sceneFlags));
        memcpy(liveBefore.data(), &gPlayState->actorCtx.sceneFlags, sizeof(gPlayState->actorCtx.sceneFlags));
    }

    // Week events: those a Song of Time keeps, and the rest within the same three days.
    std::vector<u8> weekEventReg = mm.value("weekEventReg", std::vector<u8>{});
    for (size_t i = 0; i < weekEventReg.size() && i < ARRAY_COUNT(info.weekEventReg); i++) {
        u8 keep = 0xFF;
        if (!sameCycle) {
            keep = 0;
            for (s32 bit = 0; bit < 8; bit++) {
                if ((sPersistentCycleWeekEventRegs[i] >> (2 * bit)) & 3) {
                    keep |= (1 << bit);
                }
            }
        }
        info.weekEventReg[i] |= (weekEventReg[i] & keep);
    }

    // Scene flags. (A Song of Time clears the cycle flags it does not keep by its own rules.)
    std::vector<u32> cycleFlags = mm.value("cycleSceneFlags", std::vector<u32>{});
    std::vector<u32> permanentFlags = mm.value("permanentSceneFlags", std::vector<u32>{});
    for (s32 i = 0; i < (s32)ARRAY_COUNT(gSaveContext.cycleSceneFlags); i++) {
        if ((size_t)(i * 5 + 4) < permanentFlags.size() && i < (s32)ARRAY_COUNT(info.permanentSceneFlags)) {
            PermanentSceneFlags& p = info.permanentSceneFlags[i];
            p.chest |= permanentFlags[i * 5];
            p.switch0 |= permanentFlags[i * 5 + 1];
            p.switch1 |= permanentFlags[i * 5 + 2];
            p.clearedRoom |= permanentFlags[i * 5 + 3];
            p.collectible |= permanentFlags[i * 5 + 4];
        }
        if (!sameCycle || (size_t)(i * 5 + 4) >= cycleFlags.size()) {
            continue;
        }
        CycleSceneFlags& c = gSaveContext.cycleSceneFlags[i];
        c.chest |= cycleFlags[i * 5];
        c.switch0 |= cycleFlags[i * 5 + 1];
        c.switch1 |= cycleFlags[i * 5 + 2];
        c.clearedRoom |= cycleFlags[i * 5 + 3];
        c.collectible |= cycleFlags[i * 5 + 4];
        if (IsSaveLoaded() && Play_GetOriginalSceneId(gPlayState->sceneId) == i) {
            gPlayState->actorCtx.sceneFlags.chest |= cycleFlags[i * 5];
            gPlayState->actorCtx.sceneFlags.switches[0] |= cycleFlags[i * 5 + 1];
            gPlayState->actorCtx.sceneFlags.switches[1] |= cycleFlags[i * 5 + 2];
            gPlayState->actorCtx.sceneFlags.clearedRoom |= cycleFlags[i * 5 + 3];
            gPlayState->actorCtx.sceneFlags.collectible[0] |= cycleFlags[i * 5 + 4];
        }
    }

    std::vector<u16> randoInf = mm.value("randoInf", std::vector<u16>{});
    for (size_t i = 0; i < randoInf.size() && i < ARRAY_COUNT(gSaveContext.save.shipSaveInfo.rando.randoInf); i++) {
        gSaveContext.save.shipSaveInfo.rando.randoInf[i] |= randoInf[i];
    }

    // Items and masks: an empty slot takes the team's. A bottle or trade slot the player has keeps its own content (his
    // bottle's catch, his trade item); one he does not have takes the team's -- a bottle of a one-per-world content he
    // has already comes empty (TerminaHasOnePerWorld; checked slot by slot, so two of the team's are one here too).
    // From another cycle: a Song of Time empties the trade slots and every bottle (z_sram_NES.c Sram_SaveEndOfCycle),
    // so a trade item is not the team's to give in this one (its trade is still to do here), and a bottle comes empty.
    // MergeShip2 (mergeship.exe): a teammate's own bombs .. bow make them Majora's Mask's here too (terminaOwn), shown
    // on its Items page (MergeShip_Own_MMPageSlotItem) -- the slot may already hold Hyrule's for use, which is not
    // Termina owning one. (A teammate playing 2ship.exe sends no shared part: this one is all that says so.)
    MsSharedSave* ownShared = OwnershipShared();
    bool ownChanged = false;
    std::vector<u8> items = mm.value("items", std::vector<u8>{});
    bool hadItem[ARRAY_COUNT(info.inventory.items)]; // BUG-0105: what this player held before (new ones arrive full)
    for (size_t i = 0; i < ARRAY_COUNT(info.inventory.items); i++) {
        hadItem[i] = info.inventory.items[i] != ITEM_NONE;
    }
    for (size_t i = 0; i < items.size() && i < ARRAY_COUNT(info.inventory.items); i++) {
        const u8 theirs = items[i];
        if (theirs == ITEM_NONE) {
            continue;
        }
        const s32 sharedSlot = (ownShared != nullptr) ? SharedSlotIndex(i) : -1;
        if (sharedSlot >= 0) {
            // (Bombchus stay locked until the seed's Bombchu Bag, MergeShip_Own_ChusLocked, as every give does.)
            if ((theirs == kSharedSlots[sharedSlot].item) && !MergeShip_Own_MMOwns(theirs) &&
                !((theirs == ITEM_BOMBCHU) && MergeShip_Own_ChusLocked())) {
                info.inventory.items[i] = theirs;
                ownShared->terminaOwn |= (u8)(1u << (MS_OWN_BOMB + sharedSlot));
                ownChanged = true;
            }
            continue;
        }
        if (info.inventory.items[i] != ITEM_NONE) {
            continue;
        }
        const bool bottle = (i >= SLOT_BOTTLE_1) && (i < SLOT_BOTTLE_1 + 6);
        const bool trade = (i == SLOT_TRADE_DEED) || (i == SLOT_TRADE_KEY_MAMA) || (i == SLOT_TRADE_COUPLE);
        if (trade && !sameCycle) {
            continue;
        }
        info.inventory.items[i] = (bottle && (!sameCycle || TerminaHasOnePerWorld(theirs))) ? (u8)ITEM_BOTTLE : theirs;
    }

    // Capacities: the larger of each. MergeShip2 (mergeship.exe): a bag of the shared slots is Majora's Mask's own bag
    // too (terminaBags; the slot's capacity is the better of both games', MergeShip_Own_Sync, which would otherwise
    // lower it again on the next frame).
    const u32 upgrades = mm.value("upgrades", (u32)0);
    for (s32 upg = UPG_QUIVER; upg <= UPG_DEKU_NUTS; upg++) {
        const u32 theirs = (upgrades & gUpgradeMasks[upg]) >> gUpgradeShifts[upg];
        if (ownShared != nullptr) {
            for (const auto& slot : kSharedSlots) {
                if ((slot.upgrade == upg) && (theirs > MS_BAGS_LEVEL(ownShared->terminaBags, slot.bagsField))) {
                    ownShared->terminaBags = MS_BAGS_WITH(ownShared->terminaBags, slot.bagsField, MIN(theirs, 3u));
                    ownChanged = true;
                }
            }
        }
        if (theirs > (u32)CUR_UPG_VALUE(upg)) {
            Inventory_ChangeUpgrade(upg, theirs);
        }
    }

    // Ammunition is each player's own (BUG-0105, 2026-10-12: "no shared ammo period"): no teammate's count is taken. A
    // counted item the team's state just gave arrives full (after the bags above), as a found one does.
    const struct {
        u8 item;
        s32 capacity;
    } kCounted[] = {
        { ITEM_BOW, CUR_CAPACITY(UPG_QUIVER) },
        { ITEM_BOMB, CUR_CAPACITY(UPG_BOMB_BAG) },
        { ITEM_BOMBCHU, MergeShip_Own_ChuCapacity() },
        { ITEM_DEKU_STICK, CUR_CAPACITY(UPG_DEKU_STICKS) },
        { ITEM_DEKU_NUT, CUR_CAPACITY(UPG_DEKU_NUTS) },
        { ITEM_MAGIC_BEANS, 10 },
        { ITEM_POWDER_KEG, 1 },
    };
    for (const auto& counted : kCounted) {
        if ((INV_CONTENT(counted.item) == counted.item) && !hadItem[SLOT(counted.item)]) {
            AMMO(counted.item) = (s8)counted.capacity;
        }
    }

    // A relayed part (Hyrule's engine sending Termina's last snapshot, AnchorNet.h MS_ANCHOR_KEY_RELAYED) still counts
    // what was spent since: no rupees from it, and keys only for a dungeon the joiner has none of.
    const bool relayed = mm.value(MS_ANCHOR_KEY_RELAYED, false);

    // Rupees: spent as they are used, so only a joiner takes the team's (no more than his wallet holds).
    if (join && !relayed) {
        const s32 theirs = MIN((s32)mm.value("rupees", (s16)0), (s32)CUR_CAPACITY(UPG_WALLET));
        if (theirs > info.playerData.rupees) {
            info.playerData.rupees = (s16)theirs;
        }
    }

    // Songs, remains, the notebook... (the pieces of heart are a count: below)
    const u32 questItems = mm.value("questItems", (u32)0);
    info.inventory.questItems |= (questItems & 0x0FFFFFFF);

    // Pieces of heart follow the containers: the count of the larger capacity, or the larger count of the same one.
    const s16 healthCapacity = mm.value("healthCapacity", (s16)0);
    {
        const u32 ours = (info.inventory.questItems & 0xF0000000) >> QUEST_HEART_PIECE_COUNT;
        const u32 theirs = (questItems & 0xF0000000) >> QUEST_HEART_PIECE_COUNT;
        u32 pieces = ours;
        if (healthCapacity > info.playerData.healthCapacity) {
            pieces = theirs;
        } else if (healthCapacity == info.playerData.healthCapacity) {
            pieces = MAX(ours, theirs);
        }
        info.inventory.questItems = (info.inventory.questItems & 0x0FFFFFFF) | (pieces << QUEST_HEART_PIECE_COUNT);
    }

    if (sameCycle) {
        std::vector<u8> dungeonItems = mm.value("dungeonItems", std::vector<u8>{});
        std::vector<s8> dungeonKeys = mm.value("dungeonKeys", std::vector<s8>{});
        std::vector<s8> strayFairies = mm.value("strayFairies", std::vector<s8>{});
        for (size_t i = 0; i < dungeonItems.size() && i < ARRAY_COUNT(info.inventory.dungeonItems); i++) {
            info.inventory.dungeonItems[i] |= dungeonItems[i];
        }
        // Small keys are spent (no merge can tell a key used from one not found): a joiner raises his to the team's; a
        // player in the game keeps his own (MM_UPDATE_DUNGEON_ITEMS keeps them live). Never lowered: the answer may be
        // the server's stored state -- this player's own from before he found more -- or a teammate's old relayed part.
        if (join) {
            for (size_t i = 0; i < dungeonKeys.size() && i < ARRAY_COUNT(info.inventory.dungeonKeys); i++) {
                if (relayed && (info.inventory.dungeonKeys[i] > 0)) {
                    continue;
                }
                info.inventory.dungeonKeys[i] = MAX(info.inventory.dungeonKeys[i], dungeonKeys[i]);
            }
        }
        for (size_t i = 0; i < strayFairies.size() && i < ARRAY_COUNT(info.inventory.strayFairies); i++) {
            info.inventory.strayFairies[i] = MAX(info.inventory.strayFairies[i], strayFairies[i]);
        }
        lastDungeonItems.clear(); // (PollGameState takes these as they are now, without sending them)

        // The Spider Houses' Gold Skulltula tokens, each house's count (the high and low halves) the larger; a Song of
        // Time takes them (Sram_SaveEndOfCycle), so only within the same three days.
        const u32 tokens = mm.value("skullTokenCount", (u32)0);
        info.skullTokenCount = (MAX(info.skullTokenCount >> 16, tokens >> 16) << 16) |
                               MAX(info.skullTokenCount & 0xFFFF, tokens & 0xFFFF);

        // The dungeon maps' floors and rooms visited: a Song of Time clears them too.
        std::vector<u32> maps = mm.value("permanentSceneMaps", std::vector<u32>{});
        for (s32 i = 0; i < (s32)ARRAY_COUNT(info.permanentSceneFlags) && (size_t)(i * 2 + 1) < maps.size(); i++) {
            if (IsDungeonMapScene(i)) {
                info.permanentSceneFlags[i].unk_14 |= maps[i * 2];
                info.permanentSceneFlags[i].rooms |= maps[i * 2 + 1];
            }
        }
    }

    // Tingle's maps: kept through every Song of Time. (unk_EA0 and unk_EA8 are not merged, whatever an older build
    // sends: PrepTeamState says why.)
    std::vector<u32> scenesVisible = mm.value("scenesVisible", std::vector<u32>{});
    for (size_t i = 0; i < scenesVisible.size() && i < ARRAY_COUNT(info.scenesVisible); i++) {
        info.scenesVisible[i] |= scenesVisible[i];
    }

    if (healthCapacity > info.playerData.healthCapacity) {
        info.playerData.health += healthCapacity - info.playerData.healthCapacity;
        info.playerData.healthCapacity = healthCapacity;
    }
    info.playerData.isMagicAcquired |= mm.value("isMagicAcquired", (u8)0);
    info.playerData.isDoubleMagicAcquired |= mm.value("isDoubleMagicAcquired", (u8)0);
    if (mm.value("doubleDefense", (u8)0) && !info.playerData.doubleDefense) {
        info.playerData.doubleDefense = true;
        info.inventory.defenseHearts = 20;
    }
    info.playerData.owlActivationFlags |= mm.value("owlActivationFlags", (u16)0);

    if (IS_RANDO && mm.contains("randoChecks")) {
        for (auto& entry : mm["randoChecks"]) {
            RandoCheckId rc = Rando::StaticData::GetCheckIdFromName(entry.at(0).get<std::string>().c_str());
            if (rc <= RC_UNKNOWN || rc >= RC_MAX) {
                continue;
            }
            RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
            if (entry.at(1).get<bool>()) {
                check.obtained = true;
                check.eligible = false;
                if (sameCycle) {
                    check.cycleObtained = true;
                }
            }
            check.skipped = check.skipped || entry.at(2).get<bool>();
        }
        lastCheckObtained.clear();
    }

    // (Ownership lives in the shared save, not in gSaveContext.)
    bool changed =
        ownChanged || (memcmp(saveBefore.data(), &gSaveContext.save, sizeof(gSaveContext.save)) != 0) ||
        (memcmp(cycleBefore.data(), gSaveContext.cycleSceneFlags, sizeof(gSaveContext.cycleSceneFlags)) != 0);
    if (!liveBefore.empty() && IsSaveLoaded()) {
        changed |= memcmp(liveBefore.data(), &gPlayState->actorCtx.sceneFlags, liveBefore.size()) != 0;
    }
    return changed;
}

// The shared save's part (SharedSaveJson.hpp) of a team state merged into this process's one shared save: Termina's
// live progress is captured first (the merge must see what the player has now, and the apply after it sets counters
// from it), the team's added, and the result applied to Termina at once -- what the door does at every arrival -- when
// the team had anything new. True when it had.
static bool MergeSharedPart(MsSharedSave* shared, const nlohmann::json& state, bool join) {
#ifndef MERGESHIP_HOSTED
    // (2ship.exe has no shared save: MergeShipHost_SharedSave is always NULL there.)
    (void)shared;
    (void)state;
    (void)join;
    return false;
#else
    // The sender's three days are its Termina part's ("cycle"). From another cycle, or one that does not say (a player
    // who has not been to Termina), what a Song of Time takes away -- stray fairies, Termina's trade items, bottle
    // contents -- is not theirs to give in this one: ApplyTeamState's rule, which the apply below would otherwise undo
    // by setting them into Termina's save.
    nlohmann::json part = state[MS_ANCHOR_KEY_SHARED_TEAM_STATE];
    const auto mm = state.find(MS_ANCHOR_KEY_MM_TEAM_STATE);
    const bool sameCycle = (mm != state.end()) && mm->is_object() && mm->contains("cycle") &&
                           (*mm)["cycle"].is_number() &&
                           ((*mm)["cycle"].get<int64_t>() == (int64_t)MergeShipAnchor_Cycle());
    if (!sameCycle) {
        MsSharedJson::DropCycleBound(part);
    }
    MergeShipShared_Capture(shared);
    const bool changed = MsSharedJson::Merge(*shared, part, join);
    if (changed) {
        const u16 equipment = gSaveContext.save.saveInfo.equips.equipment;
        MergeShipShared_Apply(shared, 0);
        // (An arrival's apply runs before the scene draws its buttons and Link; this one is mid-play.)
        if (gPlayState != nullptr) {
            for (u8 button = EQUIP_SLOT_B; button <= EQUIP_SLOT_C_RIGHT; button++) {
                Interface_LoadItemIcon(gPlayState, button);
            }
            Player* player = GET_PLAYER(gPlayState);
            if ((player != nullptr) && (equipment != gSaveContext.save.saveInfo.equips.equipment)) {
                Player_SetEquipmentData(gPlayState, player);
            }
        }
    }
    MergeShipHost_CommitSharedSave();
    return changed;
#endif
}

void Anchor::SendPacket_UpdateTeamState() {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

    // Assume the team queue has been emptied, so clear it
    payload["queue"] = nlohmann::json::array();

    // Hyrule's part as SoH last sent it (or wrote it, in this process), of this playthrough only, and ours.
    nlohmann::json state = MergeShipAnchor_OoTTeamState(this);
    if (!state.is_object()) {
        state = nlohmann::json::object();
    }
    // Hyrule's part is never live here: SoH's last snapshot (AnchorNet.h MS_ANCHOR_KEY_OOT_RELAYED).
    if (state.contains("inventory")) {
        state[MS_ANCHOR_KEY_OOT_RELAYED] = true;
    }
    state[MS_ANCHOR_KEY_MM_TEAM_STATE] = PrepTeamState();
    // MergeShip2: the shared save's progress (AnchorNet.h MS_ANCHOR_KEY_SHARED_TEAM_STATE), for a Termina that belongs
    // to a Hyrule file (MergeShipAnchor_LinkedSharedSave). Termina's live progress goes into it first, as the door's
    // departure does, when Termina takes part in it now (MergeShipHost_SharingAllowed) -- on the game thread only,
    // where the save is not changing under it.
    MsSharedSave* shared = MergeShipAnchor_LinkedSharedSave();
    if (shared != nullptr) {
#ifdef MERGESHIP_HOSTED
        if (MergeShipOnGameThread() && MergeShipHost_SharingAllowed()) {
            MergeShipShared_Capture(shared);
            MergeShipHost_CommitSharedSave();
        }
#endif
        // (Nothing captured into it yet by either game: there is nothing in it to send.)
        if (shared->lastWriter != 0) {
            state[MS_ANCHOR_KEY_SHARED_TEAM_STATE] = MsSharedJson::Write(*shared);
        }
    }
    // The server keeps only "state": the playthrough goes inside it (AnchorNet.h MS_ANCHOR_KEY_SEED), and whose it is
    // (MS_ANCHOR_KEY_OWNER: the Hyrule file this Termina belongs to).
    state[MS_ANCHOR_KEY_SEED] = MergeShipAnchor_LocalSeed();
    state[MS_ANCHOR_KEY_OWNER] = (shared != nullptr) ? shared->hyruleFileId : 0u;
    payload["state"] = state;

    SendJsonToRemote(payload);
}

void Anchor::SendPacket_ClearTeamState(std::string teamId) {
    nlohmann::json payload;
    payload["type"] = UPDATE_TEAM_STATE;
    payload["targetTeamId"] = teamId;
    payload["queue"] = nlohmann::json::array();
    payload["state"] = nlohmann::json::object();
    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_UpdateTeamState(nlohmann::json payload) {
    if (!roomState.syncItemsAndFlags) {
        return;
    }

    isHandlingUpdateTeamState = true;
    struct ClearFlag {
        bool& flag;
        ~ClearFlag() {
            flag = false;
        }
    } clearFlag{ isHandlingUpdateTeamState };

    // MergeShip2: our own state, echoed back, is not merged: it holds this player's counts from before he spent some,
    // and the merges only raise counts. A join answer is the answer to our own request: one that arrived in its window.
    // (ProcessIncomingPacketQueue holds this packet until a Termina file is played, so neither part waits for a save
    // that is not there.)
    const bool own = MergeShipAnchor_IsOwnPacket(payload);
    const bool join = !own && MergeShipIsJoinAnswer(payload);
    // The answer to our own request was another playthrough's (the server's stored state of an earlier seed, or a
    // teammate still on one): ours becomes the team's, so whoever loads this seed next gets this seed's (as SoH does).
    // Once per request; the window stays open for the answers of teammates on this seed.
    bool pushOwn = false;

    if (payload.contains("state") && payload["state"].is_object() && !payload["state"].empty()) {
        const nlohmann::json& state = payload["state"];
        if (!MergeShipAnchor_TeamStateSeedAccepts(payload)) {
            pushOwn = join && !mergeShipSentOwnTeamState.exchange(true);
        } else {
            // Hyrule's part is SoH's: kept to be sent along with ours (MergeShipAnchor_OoTTeamState).
            nlohmann::json oot = state;
            oot.erase(MS_ANCHOR_KEY_MM_TEAM_STATE);
            if (oot.contains("inventory")) {
                ootTeamState = oot;
            }

            bool changed = false;
            // This player's own older state (the server's stored copy, AnchorNet.h MS_ANCHOR_KEY_OWNER) gives no
            // spent-and-refilled counters: its own are newer.
            const MsSharedSave* linked = MergeShipAnchor_LinkedSharedSave();
            const bool ownState = (linked != nullptr) && (linked->hyruleFileId != 0) &&
                                  (state.value(MS_ANCHOR_KEY_OWNER, (uint32_t)0) == linked->hyruleFileId);
            const bool takeCounters = join && !ownState;

            // Termina's part first: what it raises in Termina's own save is in the shared save's capture below.
            if (!own && state.contains(MS_ANCHOR_KEY_MM_TEAM_STATE) && state[MS_ANCHOR_KEY_MM_TEAM_STATE].is_object() &&
                IsSaveLoaded()) {
                changed |= ApplyTeamState(state[MS_ANCHOR_KEY_MM_TEAM_STATE], takeCounters);
            }

            // The shared save's part: merged once per process (both engines get this packet), by the engine the player
            // is in when Termina takes part in the shared save now; otherwise left to Hyrule's engine. Only into the
            // shared save of this playthrough's Hyrule file (MergeShipAnchor_LinkedSharedSave).
            if (!own && state.contains(MS_ANCHOR_KEY_SHARED_TEAM_STATE) &&
                state[MS_ANCHOR_KEY_SHARED_TEAM_STATE].is_object() && IsSaveLoaded() &&
                MergeShipHost_SharingAllowed()) {
                MsSharedSave* shared = MergeShipAnchor_LinkedSharedSave();
                if ((shared != nullptr) && MergeShipAnchor_ClaimShared(payload)) {
                    changed |= MergeSharedPart(shared, state, takeCounters);
                }
            }

            MergeShipHostTest_Log("ANCHOR team state applied: join %d, changed %d", join ? 1 : 0, changed ? 1 : 0);
            if (join && changed) {
                Notification::Emit({
                    .message = "Save updated from team",
                });
            }
        }
    }

    // The queue is replayed whatever the state's playthrough: each queued packet carries its own (checked as it is
    // handled, MergeShipAnchor_SeedAccepts).
    bool replayed = false;
    if (payload.contains("queue") && payload["queue"].is_array() && !payload["queue"].empty()) {
        // Both worlds' engines replay the queue, and the server hands the same queue to every reply until it is cleared
        // (SoH's request, ours, a reconnect's): a queued packet's claim number is its own text's (AnchorReplay.hpp),
        // the same in both engines and in every reply, so it is applied once per process however many replies carry it.
        // Only for a reply that came through the host: packets that did not are always this engine's (AnchorHost.hpp).
        const bool viaHost = MsAnchorHost::Seq(payload) != 0;
        std::unordered_map<std::string, uint32_t> seen; // earlier identical entries: two real finds of the same thing
        std::lock_guard<std::mutex> lock(incomingPacketQueueMutex);
        for (const auto& item : payload["queue"]) {
            if (!item.is_string()) {
                continue;
            }
            const std::string& text = item.get_ref<const std::string&>();
            const uint32_t occurrence = seen[text]++;
            // A malformed entry is skipped, not the rest of the queue (nor thrown on: SoH replays on past it).
            nlohmann::json itemPayload = nlohmann::json::parse(text, nullptr, false);
            if (itemPayload.is_discarded() || !itemPayload.is_object() || !itemPayload.contains("type") ||
                !itemPayload["type"].is_string()) {
                continue;
            }
            if (viaHost) {
                itemPayload[MS_ANCHOR_KEY_SEQ] = MsAnchor_ReplaySeq(text, occurrence);
            }
            incomingPacketQueue.push(std::move(itemPayload));
        }
        replayed = true;
    }

    // Our team state goes out once what was replayed has been handled (ProcessIncomingPacketQueue, the next frame): it
    // clears the server's queue, and is the team's state when ours was asked for.
    if (replayed) {
        mergeShipReplayPushPending = true;
    } else if (pushOwn) {
        SendPacket_UpdateTeamState();
    }
}
