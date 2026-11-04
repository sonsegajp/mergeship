/*
 * MergeShip2 Anchor through the host: one network connection per process, shared by both engines.
 *
 * Anchor (Shipwright soh/Network/Anchor, ported to 2Ship as 2s2h/Network/Anchor) is SoH's online co-op: a TCP
 * connection to an Anchor server, a room, NUL-delimited JSON packets. In mergeship.exe the player crosses between
 * Hyrule (soh.dll) and Termina (2ship.dll); if each engine had its own socket the player would leave the room and
 * join again under another client id at every crossing, and the suspended engine's socket would sit unread. So the
 * host owns the socket (host/NetService.cpp) and each engine's Anchor is a view of it:
 *
 *   - Connect/Disconnect/Send go to the one connection; the host reconnects on its own and sends the stored hello
 *     (the HANDSHAKE packet) first on every (re)connect, so the server always knows the room and the client.
 *   - Every received packet is queued for *every* engine (each engine has its own game state to update: the room's
 *     client list, dummies, flags); each engine's Anchor thread drains its queue, suspended or not. The host keeps
 *     the socket, and TCP keep-alive, running while an engine is suspended: switching worlds never disconnects.
 *   - Packets that change the save (items, flags, team state requests) must be applied once per process, not once
 *     per engine: Claim(seq) says which engine does it (the first to claim; normally the active one).
 *   - Blobs: small named values both engines read and write (the room settings the connection was made with, each
 *     world's latest team state for UPDATE_TEAM_STATE).
 *
 * MsHostServices.net (EngineApi.h) points at an MsHostNet, NULL in a host without it. soh.exe and 2ship.exe have no
 * host: their Anchor keeps its own SDL_net connection, exactly as before.
 *
 * The protocol additions both engines speak (all ignored by a stock SoH client, which drops unknown packet types and
 * unknown keys) are the MS_ANCHOR_* names below. Pure C.
 */
#ifndef MERGESHIP_ANCHOR_NET_H
#define MERGESHIP_ANCHOR_NET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MsNetStatus {
    uint32_t size;          /* sizeof(MsNetStatus), set by the caller */
    int32_t enabled;        /* a connection is wanted (Connect was called and Disconnect was not) */
    int32_t connected;      /* the socket is up */
    uint32_t connectionId;  /* 1, 2, ... one per successful connect; 0 before the first. Unchanged across switches. */
    int32_t activeEngine;   /* MsEngineId of the engine the host runs now */
    uint32_t reconnects;    /* connects after the first (the server dropped us, or the network did) */
    uint64_t packetsIn;     /* packets received from the server */
    uint64_t packetsOut;    /* packets sent to it (hello included) */
    char address[128];      /* what Connect was given */
    uint16_t port;
} MsNetStatus;

typedef struct MsHostNet {
    uint32_t size; /* sizeof(MsHostNet) */
    /* Wants the connection to address:port, `hello` (a whole JSON packet, the HANDSHAKE) being sent first on every
     * connect. Returns 1 when the connection is (now) to that address, 0 when another engine already connected
     * elsewhere: that one stays (status.address) and the caller should adopt it. The hello is stored either way. */
    int32_t (*Connect)(void* host, const char* address, uint16_t port, const char* hello, uint32_t helloLength);
    /* Leaves the room: closes the socket, stops reconnecting, drops the queues. For both engines. */
    void (*Disconnect)(void* host);
    /* The hello for later reconnects (not sent now: the server already has this connection's). */
    void (*SetHello)(void* host, const char* hello, uint32_t length);
    /* One packet (JSON text, no NUL; the host adds the delimiter). 0 when not connected (the packet is dropped). */
    int32_t (*Send)(void* host, const char* data, uint32_t length);
    /* The next packet queued for `engineId`: > 0 its length (copied, NUL-terminated), 0 none, < 0 -(the capacity it
     * needs; nothing is taken). `seq` is the packet's process-wide number (for Claim), `connectionId` the connection
     * it came on. */
    int32_t (*Receive)(void* host, int32_t engineId, char* buffer, uint32_t capacity, uint64_t* seq,
                       uint32_t* connectionId);
    void (*GetStatus)(void* host, MsNetStatus* status);
    /* 1 for the first engine that claims packet `seq`, 0 for every later claim. engineId < 0 only asks (1: nobody has
     * claimed it yet) without claiming. Only the active engine's game thread processes packets, so an ask followed
     * by a claim in the same frame cannot race. */
    int32_t (*Claim)(void* host, uint64_t seq, int32_t engineId);
    /* Named values shared by both engines (NUL-terminated keys; data is copied). A zero length erases. */
    void (*SetBlob)(void* host, const char* key, const char* data, uint32_t length);
    /* Like Receive: > 0 length copied (NUL-terminated), 0 no such blob, < 0 -(capacity needed). */
    int32_t (*GetBlob)(void* host, const char* key, char* buffer, uint32_t capacity);
} MsHostNet;

/* ---- protocol additions ------------------------------------------------------------------------------------------ */

/* clientVersion both MergeShip2 engines report (Anchor ignores most packets from a client of another version, and
 * SoH's and 2Ship's own git hashes differ). Bump when the packets below change meaning. */
#define MS_ANCHOR_CLIENT_VERSION "MergeShip2-Anchor-2" /* 2: every packet carries MS_ANCHOR_KEY_SEED */

/* "world" in client state and PLAYER_UPDATE: which game the player is in. Absent = "oot" (a stock SoH client). */
#define MS_ANCHOR_WORLD_OOT "oot"
#define MS_ANCHOR_WORLD_MM "mm"

/* A Majora's Mask client reports sceneNum = MS_ANCHOR_MM_SCENE_BASE + its scene id, so that a stock SoH client (which
 * compares sceneNum with its own to send PLAYER_UPDATE and draw players) never mistakes Termina for Hyrule. */
#define MS_ANCHOR_MM_SCENE_BASE 0x1000

/* Packet types only MergeShip2 clients send. Stock SoH ignores them (no handler). */
#define MS_ANCHOR_MM_GIVE_ITEM "MM_GIVE_ITEM"                 /* an item found in Termina */
#define MS_ANCHOR_MM_SET_FLAG "MM_SET_FLAG"                   /* Majora's Mask flags (week events, scene flags) */
#define MS_ANCHOR_MM_UNSET_FLAG "MM_UNSET_FLAG"
#define MS_ANCHOR_MM_UPDATE_DUNGEON_ITEMS "MM_UPDATE_DUNGEON_ITEMS"
#define MS_ANCHOR_MM_SET_CHECK_STATUS "MM_SET_CHECK_STATUS"   /* 2Ship randomizer check obtained / skipped */
#define MS_ANCHOR_CHAT "MS_CHAT" /* text chat (AnchorChat.hpp): {message, name, color{r,g,b}} */
/* MergeShip2's shared ammo pool (bombs, bombchus, Deku Sticks, Deku Nuts, the bow's arrows -- MsOwnBit MS_OWN_BOMB,
 * MS_OWN_BOMBCHU, MS_OWN_STICK, MS_OWN_NUT, MS_OWN_BOW; beans already sync through stock SoH's UPDATE_BEANS_COUNT, and
 * the ocarina is not ammo) converges across an Anchor team. Sent by both engines (soh/Network/Anchor,
 * 2s2h/Network/Anchor) whenever this player's own count for one of those rises -- a chest, a drop, a shop, or an
 * apply from crossing the Lost Woods door in mergeship.exe. Keys: "ownBit" (MsOwnBit, an int) and "count" (the
 * sender's new count for it, already clamped to the sender's own capacity). The receiver applies
 * max(receiverCount, count), clamped to ITS OWN capacity (which may differ, e.g. a smaller bomb bag), and only when
 * that actually raises its count: spending your own ammo never drains a teammate's, and echoing an unchanged or lower
 * count back changes nothing, so there is no send/receive loop. A stock SoH client drops the unknown packet type. */
#define MS_ANCHOR_AMMO_SYNC "MS_AMMO_SYNC"
#define MS_ANCHOR_KEY_AMMO_OWN_BIT "ownBit"
#define MS_ANCHOR_KEY_AMMO_COUNT "count"

/* Keys MergeShip2 adds to stock packets:
 *   GIVE_ITEM / MM_GIVE_ITEM "msItem": the canonical item (MsItems.def key name, e.g. "HOOKSHOT"), so the other world
 *     can receive it through the shared save (MergeShipShared_ReceiveRemoteItem).
 *   UPDATE_TEAM_STATE "state"."mergeShipMM": Majora's Mask's part of the team state (the rest is SoH's SaveContext as
 *     before), so one team state on the server carries both worlds.
 *   client state "world", "sceneName" (a display name, so each game can show where a player of the other world is).
 *   MM flag packets "cycle" (the sender's threeDayResetCount) and "persistent" (the flag survives a Song of Time):
 *     a flag that does not survive the cycle is only applied by a receiver in the same cycle. */
#define MS_ANCHOR_KEY_MS_ITEM "msItem"
#define MS_ANCHOR_KEY_MM_TEAM_STATE "mergeShipMM"
/* Every packet both engines send: "msSeed", the playthrough the sender's save belongs to -- SoH's randomizer seed
 * (Rando::Context GetSeed(), the same number as a unified seed's 2Ship finalSeed), 0 for a file that is not a
 * randomizer one. Termina's engine sends Hyrule's number (MS_ANCHOR_BLOB_SEED): one player, one playthrough. A receiver
 * applies a packet that changes the save (items, flags, checks, ammo, team state and its queue) only from its own
 * playthrough (MsAnchor_SeedMatches, AnchorHost.hpp): the server keeps a room's last team state and queue for as long as
 * the room lives, and a new seed must never take an old seed's items (the user's new seed starting with an earlier
 * seed's Nayru's Love, 2026-09-30). A packet without it (an older build, stock SoH) counts as seed 0. */
#define MS_ANCHOR_KEY_SEED "msSeed"
/* UPDATE_TEAM_STATE "state" additions (MergeShip2-Anchor-2): the rest of a player's progress, so a joiner gets all of it.
 *   "mergeShipShared": the shared save's progress (SharedSaveJson.hpp; Majora's Mask's items held in Hyrule, Ocarina of
 *     Time's held in Termina, item ownership and the ammo pool, bottles, the team's Termina checks). Merged by the
 *     engine the player is in (claimed once per process, MS_ANCHOR_CLAIM_SHARED), which then applies it to itself; the
 *     other engine applies it as it always does when the player crosses.
 *   "mergeShip": SoH's gMergeShipSave progress (MM masks, gear, songs, remains, owls, stray fairies held in Hyrule) for
 *     a game without a shared save (soh.exe). */
#define MS_ANCHOR_KEY_SHARED_TEAM_STATE "mergeShipShared"
/* "state"."msOwner": the sender's Hyrule file id (SoH MergeShip_Save_SharedFileId, the shared save's hyruleFileId: random
 * per file). A receiver whose file has the same id is reading its own older state (the server's stored copy): it takes
 * no spent-and-refilled counters from it (small keys, rupees, magic) -- its own are newer. */
#define MS_ANCHOR_KEY_OWNER "msOwner"
/* A part that is not live but the other engine's last snapshot: "state"."mergeShipMM"."msRelayed" (SoH sends Termina's
 * part from the TEAM_MM blob or file) and "state"."msOoTRelayed" (2Ship sends Hyrule's part from the TEAM_OOT blob).
 * Small keys spent since it was taken are still in it, so a joiner takes a relayed part's keys only for a dungeon it has
 * none of, and no rupees from it. */
#define MS_ANCHOR_KEY_RELAYED "msRelayed"
#define MS_ANCHOR_KEY_OOT_RELAYED "msOoTRelayed"
#define MS_ANCHOR_KEY_MERGESHIP_TEAM_STATE "mergeShip"
/* Internal (never on the wire): the host's packet number, added by the receiving engine for Claim. */
#define MS_ANCHOR_KEY_SEQ "__msSeq"
/* Claim's second namespace: Claim(seq | MS_ANCHOR_CLAIM_ANNOUNCED) marks packet `seq`'s "<player> found ..."
 * notification as shown (AnchorHost.hpp MsAnchorHost::Announce). An engine announces a teammate's item even when it
 * leaves the packet to the other engine (the item is the other world's: MergeShipShared_ReceiveRemoteItem returned 0);
 * that engine gives it when the player crosses back, and must not announce it again (no notification burst on
 * crossing). The host numbers packets up from 1, so they never reach this bit. */
#define MS_ANCHOR_CLAIM_ANNOUNCED ((uint64_t)1 << 63)
/* Claim's third namespace: Claim(seq | MS_ANCHOR_CLAIM_SHARED) marks an UPDATE_TEAM_STATE's "mergeShipShared" part as
 * merged into this process's one shared save (by the engine that handles the packet first: the one the player is in). */
#define MS_ANCHOR_CLAIM_SHARED ((uint64_t)1 << 62)

/* Blobs (MsHostNet.SetBlob / GetBlob). */
#define MS_ANCHOR_BLOB_SETTINGS "anchor.settings"     /* JSON {host, port, roomId, name, teamId, color{r,g,b}} */
#define MS_ANCHOR_BLOB_OWN_IDS "anchor.ownIds"     /* JSON array: every client id this process was given (reconnects) */
#define MS_ANCHOR_BLOB_CLIENT_ID "anchor.clientId"    /* decimal: the id the server gave this connection */
#define MS_ANCHOR_BLOB_TEAM_OOT "anchor.team.oot"     /* JSON: SoH's latest UPDATE_TEAM_STATE state (no "mm" part) */
#define MS_ANCHOR_BLOB_TEAM_MM "anchor.team.mm"       /* JSON: 2Ship's latest team state part */
#define MS_ANCHOR_BLOB_CHAT "anchor.chat"            /* JSON array: the chat's recent messages (AnchorChat.hpp) */
/* JSON: the room's latest UPDATE_ROOM_STATE "state", from whichever engine received it (BUG-0063/0064/0078: the other
 * world's engine adopts it on resume -- it may never have seen the one packet, sent once on joining). */
#define MS_ANCHOR_BLOB_ROOM_STATE "anchor.roomState"
#define MS_ANCHOR_BLOB_SEED "anchor.seed"            /* decimal: SoH's MS_ANCHOR_KEY_SEED for the file it has loaded */

/* Cross-world items. When a teammate's item is received in the world it does not belong to (an Ocarina of Time item
 * while the player is in Termina, or the other way round), Anchor hands it to the engine's shared-save projector:
 *
 *     int32_t MergeShipShared_ReceiveRemoteItem(int32_t msItemKey);   (extern "C", one in each engine module)
 *
 * msItemKey is an MsItemKey (MsItems.h). It gives the item to the player now, as the unified randomizer delivers an
 * item found in this world (MsDelivery.h): natively where this engine can hold it, and to the other engine through
 * the shared save (its state on the next crossing, or a ledger entry flagged MS_LEDGER_REMOTE | MS_LEDGER_FOR_*). It
 * returns 1 when it took the item, 0 when it could not: outside a unified seed an item this engine cannot represent,
 * or no game in progress; then Anchor leaves the packet unclaimed and the item's own engine applies it natively when
 * it runs again. Defined by the item delivery: SoH soh/MergeShip/MergeShipUnified.cpp, 2Ship
 * 2s2h/Rando/MergeShip/UnifiedDelivery.cpp.
 * Anchor claims the packet (MsHostNet.Claim) only after a 1, so every item is applied exactly once per process, and
 * announces it once per process too (MS_ANCHOR_CLAIM_ANNOUNCED), whichever engine gives it. */

/* Bottles. A bottle ITEM is a found item and reaches every teammate: a new bottle, empty or holding something (a
 * randomizer "Bottle with Milk", "Bottle with Chateau Romani", "Bottle with Deku Princess"; of vanilla Majora's Mask's
 * new-bottle gives, those with a canonical item: ITEM_BOTTLE, ITEM_LONGSHOT -- MM's "Bottle with Red Potion" --
 * ITEM_MILK_BOTTLE, ITEM_GOLD_DUST, ITEM_CHATEAU). A bottle's CONTENT alone never is (the user, 2026-09-30: an extra
 * Deku Princess): a catch (MM Player_UpdateBottleHeld never reaches Item_Give, so OnItemGive never sees it) or a refill
 * (MM Item_Give's branch filling a bottle the player has: ITEM_POTION_RED .. ITEM_OBABA_DRINK, ITEM_CHATEAU_2,
 * ITEM_MILK, ITEM_GOLD_DUST_2, ITEM_HYLIAN_LOACH_2, ITEM_SEAHORSE_CAUGHT) is not sent as MM_GIVE_ITEM, and one received
 * from another build is dropped (2s2h/Network/Anchor/Packets/GiveItem.cpp). A randomizer check's own item (a refill
 * placed at a check) is a found item like any other. A received bottle item whose content is one of a kind in the
 * receiver's world (MsDelivery.h MsDelivery_BottleOnePerWorld: the Deku Princess, the Seahorse) comes empty when that
 * world has it already -- each player's world keeps its own one. */

/* Options, the same CVar names in both games' configs (each game's Mergeship menu, and 2Ship's Network > Anchor). */
/* In mergeship.exe, both worlds share the host's one Anchor connection (default on). Off: each engine connects on
 * its own, as the standalone games do (two clients in the room; not recommended). */
#define MS_CVAR_ANCHOR_SHARED_CONNECTION "gEnhancements.MergeShip.AnchorSharedConnection"
/* Items teammates find in the other world are received in this one (default on). */
#define MS_CVAR_ANCHOR_CROSS_WORLD_ITEMS "gEnhancements.MergeShip.AnchorCrossWorldItems"
/* Text chat: T opens the chat box, messages show above players' heads (default on). */
#define MS_CVAR_ANCHOR_CHAT "gEnhancements.MergeShip.AnchorChat"
/* The shared ammo pool (bombs, bombchus, Deku Sticks, Deku Nuts, arrows) converges across the team (default on). */
#define MS_CVAR_ANCHOR_AMMO_SYNC "gEnhancements.MergeShip.AnchorAmmoSync"

#ifdef __cplusplus
}
#endif

#endif /* MERGESHIP_ANCHOR_NET_H */
