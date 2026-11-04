#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Notification/Notification.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/MergeShip/MergeShipDekuBeans.h" // MergeShip2: Progressive Deku Beans' Magic Beans

extern "C" {
#include "functions.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...); // MergeShip2: the test harness's trace
u8 Return_Item_Entry(GetItemEntry itemEntry, u8 returnItem); // z_parameter.c: a give's item-receive hooks
}

// MergeShip2 (MergeShipAnchorHost.cpp): true when this engine shows a teammate item's notification, once per process
// (AnchorNet.h, MS_ANCHOR_CLAIM_ANNOUNCED).
bool MergeShipAnchor_Announce(const nlohmann::json& payload);

// MergeShip2: MergeShip's two-step progressive items. Ocarina of Time's own progressive items are resolved by the
// finder before the give (Item::GetGIEntry) and travel as the step found (GI_LONGSHOT), so a teammate gets exactly the
// finder's. These three travel as the progressive item, and Randomizer_Item_Give steps them by the found flags it
// reads. The finder's own Randomizer_Item_Give sets the step's flag first, which goes out as SET_FLAG ahead of this
// packet (HookHandlers.cpp OnFlagSet); a receiver that stepped by its own flags then went one past the finder -- a
// Razor Sword found arrived as the Gilded Sword, a Goron Lullaby Intro as the full Lullaby, Deku Beans as gold ones.
// So the packet carries the finder's step (kMsStepKey) and the receiver gives exactly that step.
namespace {

// 1, 2 or 3: how many copies of a stepped progressive item the finder holds, this one included.
const char* const kMsStepKey = "msStep";

struct TwoStepProgressive {
    RandomizerGet rg;
    RandomizerInf first;     // the first copy's found flag (randomizer.cpp Randomizer_Item_Give)
    RandomizerInf second;    // the second copy's
    RandomizerGet firstItem; // each step as an item of its own, as msItem names it (MergeShipAnchor_MsItemName)
    RandomizerGet secondItem;
    RandomizerInf third = RAND_INF_MAX; // a third copy's, when it has three (Progressive Deku Beans)
};

const TwoStepProgressive kTwoStepProgressives[] = {
    { RG_PROGRESSIVE_MM_SWORD, RAND_INF_OBTAINED_MM_RAZOR_SWORD, RAND_INF_OBTAINED_MM_GILDED_SWORD, RG_MM_RAZOR_SWORD,
      RG_MM_GILDED_SWORD },
    { RG_PROGRESSIVE_GORON_LULLABY, RAND_INF_OBTAINED_MM_GORON_LULLABY_INTRO, RAND_INF_OBTAINED_MM_GORON_LULLABY,
      RG_MM_GORON_LULLABY_INTRO, RG_MM_GORONS_LULLABY },
    // msItem names the beans' progressive item itself, not a step. Two steps: Deku, Gold Deku Beans.
    { RG_PROGRESSIVE_DEKU_BEANS, RAND_INF_OBTAINED_DEKU_BEANS, RAND_INF_OBTAINED_GOLD_DEKU_BEANS, RG_NONE, RG_NONE },
};

const TwoStepProgressive* FindTwoStepProgressive(u16 modId, s32 getItemId) {
    if (modId != MOD_RANDOMIZER) {
        return nullptr;
    }
    for (const TwoStepProgressive& progressive : kTwoStepProgressives) {
        if (progressive.rg == getItemId) {
            return &progressive;
        }
    }
    return nullptr;
}

// The finder's step of `progressive` in a GIVE_ITEM: its kMsStepKey or, from a finder without it (a build before it),
// the step its msItem names. 0 when the packet does not say.
int FinderStep(const nlohmann::json& payload, const TwoStepProgressive& progressive) {
    if (payload.contains(kMsStepKey) && payload.at(kMsStepKey).is_number_integer()) {
        const int step = payload.at(kMsStepKey).get<int>();
        if ((step == 1) || (step == 2) || ((step == 3) && (progressive.third != RAND_INF_MAX))) {
            return step;
        }
    }
    const std::string msItem = payload.value(MS_ANCHOR_KEY_MS_ITEM, std::string());
    if (msItem.empty() || (progressive.firstItem == RG_NONE)) {
        return 0;
    }
    if (msItem == MergeShipAnchor_MsItemName(MOD_RANDOMIZER, progressive.secondItem)) {
        return 2;
    }
    return (msItem == MergeShipAnchor_MsItemName(MOD_RANDOMIZER, progressive.firstItem)) ? 1 : 0;
}

// A teammate's copy of `progressive`, given as the finder's `step`: the flags the finder's Randomizer_Item_Give left
// (the second copy's comes with the first's), then the item-receive hooks, which give what those flags hold
// (MergeShipRandoItems.cpp GiveMMSwords / GiveLullabies; the beans are read from their flags, MergeShipDekuBeans.cpp).
// A receiver already at that step -- the finder's SET_FLAG of it arrived first, or it was ahead -- steps no further.
// (The flags set here are not sent on: SendPacket_SetFlag skips them while a packet is applied.)
void GiveProgressiveStep(GetItemEntry& entry, const TwoStepProgressive& progressive, int step) {
    const RandomizerInf target = (step == 3) ? progressive.third : (step == 2) ? progressive.second : progressive.first;
    // The steps below the finder's come with it.
    const RandomizerInf below[2] = { progressive.first, progressive.second };
    for (int i = 0; i < step - 1; i++) {
        if (!Flags_GetRandomizerInf(below[i])) {
            Flags_SetRandomizerInf(below[i]);
        }
    }
    const bool held = Flags_GetRandomizerInf(target);
    MergeShipTest_Log("ANCHOR GIVE_ITEM rg %d: the finder's step %d, %s", (int)progressive.rg, step,
                      held ? "held already (not stepped)" : "stepped onto it");
    if (held) {
        Return_Item_Entry(entry, RG_NONE); // Randomizer_Item_Give's own exit, without its step
    } else {
        Randomizer_Item_Give(gPlayState, entry); // exactly one step, onto `target`
    }
}

} // namespace

/**
 * GIVE_ITEM
 */

uint8_t incomingIceTrapsFromAnchor = 0;

void Anchor::SendPacket_GiveItem(u16 modId, s16 getItemId) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    if (modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && incomingIceTrapsFromAnchor > 0) {
        incomingIceTrapsFromAnchor = MAX(incomingIceTrapsFromAnchor - 1, 0);
        return;
    }

    // Ignore sending master sword in final Ganon fight
    if (modId == MOD_RANDOMIZER && getItemId == RG_MASTER_SWORD && gPlayState->sceneNum == SCENE_GANON_BOSS) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = GIVE_ITEM;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["modId"] = modId;
    payload["getItemId"] = getItemId;
    // MergeShip2: the canonical item, so a teammate in Termina receives it there (MergeShipAnchorHost.cpp).
    std::string msItem = MergeShipAnchor_MsItemName(modId, getItemId);
    if (!msItem.empty()) {
        payload[MS_ANCHOR_KEY_MS_ITEM] = msItem;
    }
    // MergeShip2: the finder's step of a two-step progressive item (kTwoStepProgressives above): its found flags, which
    // Randomizer_Item_Give has set by now (this runs from the item-receive hooks), read as MergeShipAnchor_MsItemName
    // reads them for msItem.
    if (const TwoStepProgressive* progressive = FindTwoStepProgressive(modId, getItemId)) {
        payload[kMsStepKey] = ((progressive->third != RAND_INF_MAX) && Flags_GetRandomizerInf(progressive->third)) ? 3
                              : Flags_GetRandomizerInf(progressive->second)                                         ? 2
                                                                                                                    : 1;
    }

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_GiveItem(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }
    // MergeShip2: once per process. In mergeship.exe the engine the player is in applies it first (Termina's gives
    // it through the shared save); this one, parked meanwhile, then skips it.
    if (!MergeShipAnchor_Claim(payload)) {
        MergeShipTest_Log("ANCHOR GIVE_ITEM %s: already given in Termina", payload.value(MS_ANCHOR_KEY_MS_ITEM, "").c_str());
        return;
    }
    MergeShipTest_Log("ANCHOR GIVE_ITEM mod %d item %d (%s): given in Hyrule", payload.value("modId", 0),
                      payload.value("getItemId", 0), payload.value(MS_ANCHOR_KEY_MS_ITEM, "").c_str());

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    AnchorClient& client = clients[clientId];
    u16 modId = payload.at("modId").get<u16>();
    u16 getItemId = payload.at("getItemId").get<u16>();

    GetItemEntry getItemEntry;
    if (modId == MOD_NONE) {
        getItemEntry = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, getItemId);
    } else {
        getItemEntry = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(getItemId)).GetGIEntry_Copy();
    }

    if (getItemEntry.modIndex == MOD_NONE) {
        if (getItemEntry.getItemId == GI_SWORD_BGS) {
            gSaveContext.bgsFlag = true;
        }
        Item_Give(gPlayState, static_cast<u8>(getItemEntry.itemId));
    } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
        if (getItemEntry.getItemId == RG_ICE_TRAP) {
            gSaveContext.ship.pendingIceTrapCount++;
            incomingIceTrapsFromAnchor++;
        } else {
            // MergeShip2: a two-step progressive item ends on the finder's step, never past it (kTwoStepProgressives).
            const TwoStepProgressive* progressive =
                FindTwoStepProgressive(getItemEntry.modIndex, getItemEntry.getItemId);
            const int step = (progressive != nullptr) ? FinderStep(payload, *progressive) : 0;
            if (step != 0) {
                GiveProgressiveStep(getItemEntry, *progressive, step);
            } else {
                Randomizer_Item_Give(gPlayState, getItemEntry);
            }
        }
    }

    // Full heal if getting a heart container or piece
    if (getItemEntry.gid == GID_HEART_CONTAINER || getItemEntry.gid == GID_HEART_PIECE) {
        gSaveContext.healthAccumulator = 0x140;
    }

    // Handle if the player gets a 4th heart piece (usually handled in z_message)
    s32 heartPieces = (s32)(gSaveContext.inventory.questItems & 0xF0000000) >> (QUEST_HEART_PIECE + 4);
    if (heartPieces >= 4) {
        gSaveContext.inventory.questItems &= ~0xF0000000;
        gSaveContext.inventory.questItems += (heartPieces % 4) << (QUEST_HEART_PIECE + 4);
        gSaveContext.healthCapacity += 0x10 * (heartPieces / 4);
        gSaveContext.health += 0x10 * (heartPieces / 4);
    }

    // MergeShip2: every item a teammate finds is announced (the user's rule), junk included -- once per process:
    // Termina's engine announces it when the packet arrives there and leaves an item only Hyrule holds to this one,
    // which then gives it silently (AnchorNet.h, MS_ANCHOR_CLAIM_ANNOUNCED; no notification burst on crossing).
    if (MergeShipAnchor_Announce(payload)) {
        if (getItemEntry.modIndex == MOD_NONE) {
            Notification::Emit({
                .itemIcon = MergeShip_NotificationIcon(getItemEntry.itemId), // MergeShip2 (BUG-0090): MM's too
                .prefix = client.name,
                .message = "found",
                .suffix = SohUtils::GetItemName(getItemEntry.itemId),
            });
        } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
            Rando::Item& rItem = Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId);
            Notification::Emit({
                // MergeShip2 (BUG-0090): the item's own icon when it has no custom one (MM's items too)
                .itemIcon = rItem.HasCustomIcon() ? rItem.GetCustomIcon()
                                                  : MergeShip_NotificationIcon(rItem.GetGIEntry_Copy().itemId),
                .prefix = client.name,
                .message = "found",
                .suffix = rItem.GetName().english,
            });
        }
    }
}
