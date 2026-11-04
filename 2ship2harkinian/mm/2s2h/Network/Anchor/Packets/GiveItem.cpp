#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/BenGui/Notification.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h" // MergeShip2: OriginTag, ProxyHeldInTermina
#include "2s2h/Rando/MergeShip/UnifiedSeed.h"     // MergeShip2: IsProxy
#include <ship/Context.h>                         // MergeShip2: the notification icons (BUG-0090)
#include <ship/window/Window.h>
#include <fast/Fast3dGui.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "macros.h"
extern PlayState* gPlayState;
}

/**
 * MM_GIVE_ITEM (SoH: GIVE_ITEM)
 *
 * An item a teammate found in Termina. Randomizer: the item placed at the check ("ri", its spoiler name), which each
 * teammate converts for himself (progressive items, junk), as SoH sends the RandomizerGet. Vanilla: Majora's Mask's
 * ItemId ("mmItem"). Both carry the canonical item ("msItem"), so a teammate in Hyrule receives it there.
 *
 * GIVE_ITEM (Hyrule's items) is received here only through the shared save, by its canonical item: see below.
 */

// MergeShip2: MM Item_GiveImpl's branch that fills a bottle the player already has (z_parameter.c): ITEM_POTION_RED ..
// ITEM_OBABA_DRINK, the "_2" prizes, ITEM_MILK, ITEM_SEAHORSE_CAUGHT -- all but ITEM_MILK_BOTTLE, ITEM_POE,
// ITEM_GOLD_DUST, ITEM_CHATEAU and ITEM_HYLIAN_LOACH, which that function gives in a new bottle (as ITEM_BOTTLE and
// ITEM_LONGSHOT, MM's "Bottle with Red Potion"). A bottle's content is not a found item (AnchorNet.h, "Bottles"): a
// bought Red Potion went to the team as a whole new "Bottle with Red Potion" (RI_BOTTLE_RED_POTION's itemId is
// ITEM_POTION_RED), and a teammate in Hyrule got one more of Majora's Mask's bottles for it.
static bool IsBottleRefill(u8 item) {
    if ((item == ITEM_MILK_BOTTLE) || (item == ITEM_POE) || (item == ITEM_GOLD_DUST) || (item == ITEM_CHATEAU) ||
        (item == ITEM_HYLIAN_LOACH)) {
        return false;
    }
    return ((item >= ITEM_POTION_RED) && (item <= ITEM_OBABA_DRINK)) || (item == ITEM_CHATEAU_2) ||
           (item == ITEM_MILK) || (item == ITEM_GOLD_DUST_2) || (item == ITEM_HYLIAN_LOACH_2) ||
           (item == ITEM_SEAHORSE_CAUGHT);
}

// MergeShip2 (BUG-0090): the icon of `ri` for a notification (Hyrule's items included), registered for ImGui the
// first time (2Ship registers its own item icons by path, ShipUtils.cpp LoadGuiTextures).
static const char* NotifyIcon(RandoItemId ri) {
    if ((ri <= RI_UNKNOWN) || (ri >= RI_MAX)) {
        return nullptr;
    }
    const char* path = Rando::MergeShip::GetItemIconTexture(ri);
    auto gui = std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
    // (BUG-0107: an item with no icon of its own may name "" -- loading it crashed LoadGuiTexture)
    if ((path == nullptr) || (path[0] == '\0') || (gui == nullptr)) {
        return nullptr;
    }
    if (!gui->HasTextureByName(path)) {
        gui->LoadGuiTexture(path, path, "", ImVec4(1, 1, 1, 1));
    }
    return path;
}

// MergeShip2: `name` with the game its item is from (Rando::MergeShip::OriginTag), unless the name already says so
// (MsItems.def's "Progressive Sword (MM)", "Hookshot (MM)"), as SoH's MergeShipAnchor_NotifyMMFind.
static std::string WithOriginTag(std::string name, RandoItemId ri) {
    const std::string tag = Rando::MergeShip::OriginTag(ri);
    if (!tag.empty() && (name.find(tag) == std::string::npos)) {
        name += tag;
    }
    return name;
}

void Anchor::SendPacket_MMGiveItem(s32 randoItemId, u8 mmItem, s32 resolvedItemId) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    // MergeShip2: a vanilla refill is not an item (IsBottleRefill); MM's new bottle of Red Potion is ITEM_LONGSHOT
    // (GI_POTION_RED_BOTTLE), which no RandoItemId names: it is the "Bottle with Red Potion".
    if ((randoItemId < 0) && IsBottleRefill(mmItem)) {
        return;
    }
    RandoItemId ri = (randoItemId >= 0)          ? (RandoItemId)randoItemId
                     : (mmItem == ITEM_LONGSHOT) ? RI_BOTTLE_RED_POTION
                                                 : Rando::StaticData::GetItemIdFromVanillaItemId(mmItem);
    // MergeShip2: a teammate gets the item the check actually gave, in either world ("ri" and "msItem" alike): a
    // progressive check's placed id (RI_PROGRESSIVE_SWORD) doesn't say which step (Kokiri / Razor / Gilded Sword...)
    // the finder got, and converted against the teammate's own sword it gave another one (the user, 2026-09-30: a
    // teammate's Kokiri Sword arrived as a Razor Sword). The placed item when there is no such item: no resolution
    // recorded for the check (RI_UNKNOWN), or the finder's copy became junk (he had it already) -- the teammate then
    // converts the placed item for himself, as before.
    if ((randoItemId >= 0) && (resolvedItemId > RI_UNKNOWN) && (resolvedItemId < RI_MAX) &&
        (resolvedItemId != RI_JUNK) && (resolvedItemId != RI_NONE) &&
        (Rando::StaticData::Items[(RandoItemId)resolvedItemId].randoItemType != RITYPE_JUNK)) {
        ri = (RandoItemId)resolvedItemId;
    }
    std::string msItem = MergeShipAnchor_MsItemNameOfRI(ri);
    const s32 msKey = MergeShipAnchor_MsItemKeyOfName(msItem);

    if (randoItemId < 0) {
        // Vanilla: what a check gives, not what a pot or the grass drops (rupees, hearts, ammunition refills).
        if (ri == RI_UNKNOWN || msKey < 0 || MergeShipAnchor_MsItemIsJunk(msKey)) {
            return;
        }
    }

    nlohmann::json payload;
    payload["type"] = MM_GIVE_ITEM;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    if (randoItemId >= 0) {
        payload["ri"] = Rando::StaticData::Items[ri].spoilerName;
    } else {
        payload["mmItem"] = mmItem;
    }
    if (!msItem.empty()) {
        payload[MS_ANCHOR_KEY_MS_ITEM] = msItem;
    }
    payload["cycle"] = MergeShipAnchor_Cycle();

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_MMGiveItem(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    uint32_t clientId = payload.value("clientId", (uint32_t)0);
    std::string itemName;
    RandoItemId itemIconRi = RI_UNKNOWN; // BUG-0090

    if (payload.contains("ri") && IS_RANDO) {
        RandoItemId ri = Rando::StaticData::GetItemIdFromName(payload["ri"].get<std::string>().c_str());
        if (ri != RI_UNKNOWN) {
            ri = Rando::ConvertItem(ri);
        }
        // MergeShip2: an item of Hyrule's that Termina cannot hold (a proxy RI_OOT_* with no native Majora's Mask
        // item): Rando::GiveItem gives nothing and records nothing for it, so claiming the packet first lost it for
        // Hyrule. It crosses as a GIVE_ITEM does (HandlePacket_GiveItem below): the shared-save projector records it
        // for Hyrule (a unified seed) and only then is the packet claimed; otherwise Hyrule's engine gives it.
        if (Rando::MergeShip::IsProxy(ri) && !Rando::MergeShip::ProxyHeldInTermina(ri)) {
            if (!MergeShipAnchor_CanClaim(payload)) {
                return;
            }
            if (MergeShipAnchor_Announce(payload)) {
                Notification::Emit({
                    .itemIcon = NotifyIcon(ri),
                    .prefix = clients.contains(clientId) ? clients[clientId].name : std::string("?"),
                    .message = "found",
                    .suffix = WithOriginTag(Rando::StaticData::GetItemName(ri, false), ri),
                });
            }
            const s32 key = MergeShipAnchor_MsItemKeyOfName(MergeShipAnchor_MsItemNameOfRI(ri));
            if (CVarGetInteger(MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS, 1) && (key >= 0) &&
                MergeShipAnchor_ReceiveRemoteItem(key)) {
                MergeShipAnchor_Claim(payload);
            }
            return;
        }
        // Once per process: in mergeship.exe, Hyrule's engine may have given it already through the shared save.
        if (!MergeShipAnchor_Claim(payload) || (ri == RI_UNKNOWN)) {
            return;
        }
        if (ri == RI_JUNK) {
            ri = Rando::CurrentJunkItem();
        }
        Rando::GiveItem(ri);
        // MergeShip2: every item is announced (junk too), with the game it is from.
        itemName = WithOriginTag(Rando::StaticData::GetItemName(ri, false), ri);
        itemIconRi = ri;
    } else {
        // Once per process: in mergeship.exe, Hyrule's engine may have given it already through the shared save.
        if (!MergeShipAnchor_Claim(payload)) {
            return;
        }
        u8 item = ITEM_NONE;
        if (payload.contains("mmItem")) {
            item = payload["mmItem"].get<u8>();
            // MergeShip2: a bottle's content from a build that still sent refills is not an item (IsBottleRefill).
            // Claimed above, so Hyrule's engine does not give it either.
            if (IsBottleRefill(item)) {
                return;
            }
        } else if (payload.contains("ri")) {
            // A randomizer teammate while this file is vanilla: the item's own ItemId, if it has one. MergeShip2: the
            // "Bottle with Red Potion" is MM's ITEM_LONGSHOT (a new bottle), not its itemId ITEM_POTION_RED (a refill).
            RandoItemId ri = Rando::StaticData::GetItemIdFromName(payload["ri"].get<std::string>().c_str());
            if (ri == RI_BOTTLE_RED_POTION) {
                item = ITEM_LONGSHOT;
            } else if (ri != RI_UNKNOWN) {
                item = (u8)Rando::StaticData::Items[ri].itemId;
            }
        }
        if (item == ITEM_NONE) {
            return;
        }
        Item_Give(gPlayState, item);
        RandoItemId ri = (item == ITEM_LONGSHOT) ? RI_BOTTLE_RED_POTION
                                                 : Rando::StaticData::GetItemIdFromVanillaItemId(item);
        if (ri != RI_UNKNOWN) {
            itemName = WithOriginTag(Rando::StaticData::GetItemName(ri, false), ri);
            itemIconRi = ri;
        }
    }

    // A fourth piece of heart (Majora's Mask turns them into a container in its message code, which this skips)
    if (GET_QUEST_HEART_PIECE_COUNT >= 4) {
        RESET_HEART_PIECE_COUNT;
        gSaveContext.save.saveInfo.playerData.healthCapacity += 0x10;
        gSaveContext.save.saveInfo.playerData.health += 0x10;
    }

    // Once per process: Hyrule's engine announced it already if the packet reached it first and it left the item to
    // Termina (AnchorNet.h, MS_ANCHOR_CLAIM_ANNOUNCED; no notification burst on crossing).
    if (!itemName.empty() && MergeShipAnchor_Announce(payload)) {
        Notification::Emit({
            .itemIcon = NotifyIcon(itemIconRi),
            .prefix = clients.contains(clientId) ? clients[clientId].name : std::string("?"),
            .message = "found",
            .suffix = itemName,
        });
    }
}

// A teammate's item from Hyrule. Hyrule's own engine gives it natively (SoH GIVE_ITEM); here it can only come through
// the shared save, and only once per process (AnchorNet.h, "Cross-world items").
void Anchor::HandlePacket_GiveItem(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags ||
        !CVarGetInteger(MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS, 1)) {
        return;
    }
    const std::string name = payload.value(MS_ANCHOR_KEY_MS_ITEM, std::string());
    const s32 key = MergeShipAnchor_MsItemKeyOfName(name);
    if (key < 0 || !MergeShipAnchor_CanClaim(payload)) {
        return;
    }
    uint32_t clientId = payload.value("clientId", (uint32_t)0);
    // MergeShip2: every item is announced (junk too, and one only Hyrule can hold), tagged with the item's own game,
    // not the packet it arrived on: GIVE_ITEM carries everything Hyrule gives natively, including Majora's Mask's own
    // items Hyrule can also hold (the Pictograph Box, the Powder Keg, the Great Fairy's Sword, Bomber's Notebook ...),
    // so "sent as GIVE_ITEM" is not proof the item is Hyrule's (the user, 2026-09-30; this used to hardcode " (OOT)").
    // Rando::MergeShip::OriginTag (as MM_GIVE_ITEM below) resolves the tag from the item itself: "" for masks (their
    // name already carries it, or the Deku / Fierce Deity exception), else the item's own game from MsItems.def -- not
    // added twice to a display name that already says it ("Progressive Sword (MM)", "Hookshot (MM)").
    // Announced once per process: this engine may leave the item to Hyrule's, which then gives it silently.
    const s32 ri = MergeShipAnchor_MsItemRIOfKey(key);
    if (MergeShipAnchor_Announce(payload)) {
        Notification::Emit({
            .itemIcon = (ri >= 0) ? NotifyIcon((RandoItemId)ri) : nullptr,
            .prefix = clients.contains(clientId) ? clients[clientId].name : std::string("?"),
            .message = "found",
            .suffix = (ri >= 0) ? WithOriginTag(MergeShipAnchor_MsItemDisplayName(key), (RandoItemId)ri)
                                : MergeShipAnchor_MsItemDisplayName(key),
        });
    }
    if (!MergeShipAnchor_ReceiveRemoteItem(key)) {
        // Termina cannot hold it: Hyrule's engine gives it natively when the player is there again.
        return;
    }
    MergeShipAnchor_Claim(payload);
}
