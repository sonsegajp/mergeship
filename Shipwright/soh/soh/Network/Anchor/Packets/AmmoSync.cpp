#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/Network/Anchor/MergeShipAnchor.h"
#include "soh/MergeShip/MergeShipOwnership.h" // MergeShip_Own_ChuCapacity

extern "C" {
#include "macros.h"
#include <mergeship/SharedSave.h>
void MergeShipTest_Log(const char* fmt, ...);
}

/**
 * MS_AMMO_SYNC (include/mergeship/AnchorNet.h)
 *
 * MergeShip2 (task 2): keeps the shared ammo pool (task 1 -- bombs, bombchus, Deku Sticks, Deku Nuts, the bow's
 * arrows) converging across an Anchor team. Beans already sync through stock SoH's UPDATE_BEANS_COUNT; this covers
 * the rest of the counted items (MsOwnBit MS_OWN_BOMB, MS_OWN_BOMBCHU, MS_OWN_STICK, MS_OWN_NUT, MS_OWN_BOW).
 *
 * One count for the whole team (the user's rule: ammo is one pool, shared with Anchor teammates). Sent whenever this
 * player's count changes from what was last sent or received -- picked up or spent -- as the new count; a teammate takes
 * exactly that count (clamped to their own capacity), so spending reaches the team as pickups do. It used to send only
 * rises, applied as max(theirs, this one): spending never left the player, so a teammate's higher count kept refilling
 * the ammo and it never ran out (BUG-0010, "cross-game ammo still infinite... counters mess up"). The count itself, not
 * a change, so a count set from outside (a joiner taking the team's, the pool carried across the Lost Woods door) can
 * only ever send the count everyone should have -- never add a change twice. A value this process just received (or
 * already sent) is not sent straight back -- no echo loop.
 */

namespace {
// MsOwnBit MS_OWN_BOMB .. MS_OWN_BOW, skipping MS_OWN_BEAN (UPDATE_BEANS_COUNT's) and MS_OWN_OCARINA (not ammo).
const s32 kSyncedBits[] = { MS_OWN_BOMB, MS_OWN_BOMBCHU, MS_OWN_STICK, MS_OWN_NUT, MS_OWN_BOW };

u8 SyncedItem(s32 ownBit) {
    switch (ownBit) {
        case MS_OWN_BOMB:
            return ITEM_BOMB;
        case MS_OWN_BOMBCHU:
            return ITEM_BOMBCHU;
        case MS_OWN_STICK:
            return ITEM_STICK;
        case MS_OWN_NUT:
            return ITEM_NUT;
        case MS_OWN_BOW:
            return ITEM_BOW; // MM's Hero's Bow shares this same arrow count (MergeShipOwnership.h)
        default:
            return ITEM_NONE;
    }
}

// The same capacity macro SoH's own Item_Give already clamps each of these to (z_parameter.c); MergeShip's own
// (possibly higher, task 1) capacity clamp runs right after on the next MergeShip_Own_Sync when hosted.
s32 SyncedCapacity(s32 ownBit) {
    switch (ownBit) {
        case MS_OWN_BOMB:
            return CUR_CAPACITY(UPG_BOMB_BAG);
        case MS_OWN_BOMBCHU:
            // Not the bomb bag's: 50, or the Bombchu Bag's tier (the unified seed's pool, or SoH's progressive bag).
            // No chus held: 0 (as a missing bag, quiver or upgrade is for the others), so no count sits in an empty
            // slot.
            return (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) ? MergeShip_Own_ChuCapacity() : 0;
        case MS_OWN_STICK:
            return CUR_CAPACITY(UPG_STICKS);
        case MS_OWN_NUT:
            return CUR_CAPACITY(UPG_NUTS);
        case MS_OWN_BOW:
            return CUR_CAPACITY(UPG_QUIVER);
        default:
            return 0;
    }
}

// The count this process last sent, or last applied from a receive (so applying one does not send it straight back).
// -1: never sent or received yet this process (so the first real count, even 0, is not skipped as "unchanged").
s32 sLastSynced[MS_OWN_AMMO_COUNT] = { -1, -1, -1, -1, -1, -1 };
} // namespace

void Anchor::SendPacket_AmmoSync(s32 ownBit, s32 count) {
    nlohmann::json payload;
    payload["type"] = MS_ANCHOR_AMMO_SYNC;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload[MS_ANCHOR_KEY_AMMO_OWN_BIT] = ownBit;
    payload[MS_ANCHOR_KEY_AMMO_COUNT] = count;

    SendJsonToRemote(payload);
}

void Anchor::MergeShip_CheckAmmoSync() {
    // BUG-0105 (2026-10-12, "no shared ammo period"): ammo is each player's own; nothing is sent or taken.
    if (true) {
        return;
    }
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !CVarGetInteger(MS_CVAR_ANCHOR_AMMO_SYNC, 1)) {
        return;
    }
    for (s32 ownBit : kSyncedBits) {
        s32 count = AMMO(SyncedItem(ownBit));

        if (count != sLastSynced[ownBit]) {
            // The first count this process sees is where it starts, not a change: nothing to send for it.
            if (sLastSynced[ownBit] >= 0) {
                SendPacket_AmmoSync(ownBit, count);
            }
            sLastSynced[ownBit] = count;
        }
    }
}

void Anchor::HandlePacket_AmmoSync(nlohmann::json payload) {
    // BUG-0105 (2026-10-12, "no shared ammo period"): ammo is each player's own; nothing is sent or taken.
    if (true) {
        return;
    }
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !CVarGetInteger(MS_CVAR_ANCHOR_AMMO_SYNC, 1)) {
        return;
    }
    s32 ownBit = payload.value(MS_ANCHOR_KEY_AMMO_OWN_BIT, -1);
    s32 count = payload.value(MS_ANCHOR_KEY_AMMO_COUNT, -1);
    u8 item = ((ownBit >= 0) && (ownBit < MS_OWN_AMMO_COUNT)) ? SyncedItem(ownBit) : ITEM_NONE;

    if ((item == ITEM_NONE) || (count < 0)) {
        return;
    }
    if (count > SyncedCapacity(ownBit)) {
        count = SyncedCapacity(ownBit);
    }
    // Once per process: the pool is one count for both engines, and the parked engine keeps this packet queued
    // (MergeShipAnchor_KeepIncoming) -- replayed later, it would raise a pool the player has since spent. The count is
    // still noted below either way (as 2Ship's copy does), so a replay does not send the team's own count back out.
    const s32 before = AMMO(item);
    const bool claimed = MergeShipAnchor_Claim(payload);
    if (claimed && (INV_CONTENT(item) == item)) {
        AMMO(item) = (s8)count; // SoH's inventory.ammo is s8 (every capacity fits)
    }
    MergeShipTest_Log("ANCHOR AMMO_SYNC in: ownBit %d count %d claimed %d: %d -> %d", ownBit, count, claimed ? 1 : 0,
                      before, (s32)AMMO(item));
    // Either way this process is caught up to what the team already knows: don't send it straight back out.
    sLastSynced[ownBit] = AMMO(item);
}
