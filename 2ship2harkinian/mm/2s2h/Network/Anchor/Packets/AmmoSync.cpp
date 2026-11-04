#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip_Own_ChuCapacity

extern "C" {
#include "macros.h"
#include <mergeship/SharedSave.h>
}

/**
 * MS_AMMO_SYNC (include/mergeship/AnchorNet.h)
 *
 * MergeShip2 (task 2): keeps the shared ammo pool (task 1 -- bombs, bombchus, Deku Sticks, Deku Nuts, the bow's
 * arrows) converging across an Anchor team. Mirrors SoH's soh/Network/Anchor/Packets/AmmoSync.cpp (MsOwnBit
 * MS_OWN_BOMB, MS_OWN_BOMBCHU, MS_OWN_STICK, MS_OWN_NUT, MS_OWN_BOW; MS_OWN_BEAN and MS_OWN_OCARINA are not sent).
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
const s32 kSyncedBits[] = { MS_OWN_BOMB, MS_OWN_BOMBCHU, MS_OWN_STICK, MS_OWN_NUT, MS_OWN_BOW };

u8 SyncedItem(s32 ownBit) {
    switch (ownBit) {
        case MS_OWN_BOMB:
            return ITEM_BOMB;
        case MS_OWN_BOMBCHU:
            return ITEM_BOMBCHU;
        case MS_OWN_STICK:
            return ITEM_DEKU_STICK;
        case MS_OWN_NUT:
            return ITEM_DEKU_NUT;
        case MS_OWN_BOW:
            return ITEM_BOW; // Hyrule's Fairy Bow shares this same arrow count (MergeShipOwnership.h)
        default:
            return ITEM_NONE;
    }
}

// The same capacity this engine's own Item_GiveImpl already clamps each of these to (z_parameter.c);
// MergeShip's own (possibly higher, task 1) capacity clamp runs right after on the next MergeShip_Own_Sync.
s32 SyncedCapacity(s32 ownBit) {
    switch (ownBit) {
        case MS_OWN_BOMB:
            return CUR_CAPACITY(UPG_BOMB_BAG);
        case MS_OWN_BOMBCHU:
            // The pool's (50, or the seed's Bombchu Bag), not the bomb bag's; no chus held: 0, as SoH's copy -- a
            // count must not sit in an empty slot for the first pickup to add to.
            return (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) ? MergeShip_Own_ChuCapacity() : 0;
        case MS_OWN_STICK:
            return CUR_CAPACITY(UPG_DEKU_STICKS);
        case MS_OWN_NUT:
            return CUR_CAPACITY(UPG_DEKU_NUTS);
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
    // The pool is one per process (MergeShipOwnership.h) and both engines queue this packet (the parked one keeps it,
    // MergeShipAnchor_KeepIncoming): only the engine that claims it applies it, or the other would replay it later
    // against a pool the player has spent since.
    if (MergeShipAnchor_Claim(payload) && (INV_CONTENT(item) == item)) {
        AMMO(item) = (s16)count;
    }
    // Either way this process is caught up to what the team already knows: don't send it straight back out.
    sLastSynced[ownBit] = AMMO(item);
}
