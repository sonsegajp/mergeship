// MergeShip2 -- claim ids for packets replayed from the Anchor server's queue (UPDATE_TEAM_STATE "queue").
// Header-only, shared by SoH's and 2Ship's Anchor so both engines number the same queued packet the same way.
#pragma once

#include <atomic>
#include <cstdint>
#include <random>
#include <string>

// Claim-id namespaces (AnchorNet.h claim ids are 64-bit):
//   bit 63  MS_ANCHOR_CLAIM_ANNOUNCED  a live packet's own claim
//   bit 62  MS_ANCHOR_CLAIM_SHARED     a shared-pool claim
//   bit 61  replayed queue entry       (this header; bits 62 and 63 always clear)
// A replayed entry's id is derived from its raw JSON text, not from where it sits in a reply: the server hands the
// same queue to every request (SoH's, 2Ship's, after a reconnect), so the same queued packet must map to the same id
// and be applied once per process however many replies carry it. `occurrence` counts earlier entries in the same queue
// with the identical text, so two real pickups that serialised the same (two rupees of one kind) both apply.
//   id = (1 << 61) | ((FNV-1a64(text) & (2^45 - 1)) << 16) | (occurrence & 0xFFFF)
inline uint64_t MsAnchor_ReplaySeq(const std::string& text, uint32_t occurrence) {
    uint64_t hash = 14695981039346656037ull; // FNV-1a 64-bit offset basis
    for (unsigned char byte : text) {
        hash ^= byte;
        hash *= 1099511628211ull; // FNV-1a 64-bit prime
    }
    return (1ull << 61) | ((hash & ((1ull << 45) - 1)) << 16) | (static_cast<uint64_t>(occurrence) & 0xFFFFull);
}

// `seq` (a payload's MS_ANCHOR_KEY_SEQ) numbers a replayed queue entry (MsAnchor_ReplaySeq): bit 61 set, 62 and 63 clear.
inline bool MsAnchor_IsReplaySeq(uint64_t seq) {
    return (seq >> 61) == 1;
}

// A number no other queued packet has: a per-module random 64-bit base plus a counter. Both engines' SendJsonToRemote
// put it on every packet the server may queue ("addToQueue") as "msId", so two real finds of the same item (two small
// keys of one dungeon, two Pieces of Heart) never serialise to the same text and never share a replay id, while every
// reply that carries the same queued packet still gives it the same id (MsAnchor_ReplaySeq).
inline uint64_t MsAnchor_NewPacketId() {
    static const uint64_t sBase = [] {
        std::random_device device;
        return (static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device());
    }();
    static std::atomic<uint64_t> sCounter{ 0 };
    return sBase + sCounter.fetch_add(1);
}
