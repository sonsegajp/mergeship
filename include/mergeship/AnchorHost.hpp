// MergeShip2 -- helpers both engines' Anchor use to be a view of the host's one connection (AnchorNet.h).
// Header-only C++ (nlohmann::json, which both games already use). Each engine keeps one MsAnchorHost.
#pragma once

#include <mergeship/AnchorNet.h>
#include <mergeship/EngineApi.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

// A packet type that changes the save (AnchorNet.h MS_ANCHOR_KEY_SEED): applied only from the same playthrough.
// REQUEST_TEAM_STATE too: a reply is the team's state, which a player of another seed must not be sent (the server
// stores it as the team's and hands it to whoever loads next).
inline bool MsAnchor_IsSeededPacket(const std::string& type) {
    static const char* const kTypes[] = {
        "GIVE_ITEM",          "SET_CHECK_STATUS",     "SET_FLAG",           "UNSET_FLAG",
        "UPDATE_BEANS_COUNT", "UPDATE_DUNGEON_ITEMS", "ENTRANCE_DISCOVERED", "GAME_COMPLETE",
        "UPDATE_TEAM_STATE",  "REQUEST_TEAM_STATE",   "CONSUME_ADULT_TRADE_ITEM",
        MS_ANCHOR_MM_GIVE_ITEM, MS_ANCHOR_MM_SET_FLAG, MS_ANCHOR_MM_UNSET_FLAG, MS_ANCHOR_MM_UPDATE_DUNGEON_ITEMS,
        MS_ANCHOR_MM_SET_CHECK_STATUS, MS_ANCHOR_AMMO_SYNC,
    };
    for (const char* known : kTypes) {
        if (type == known) {
            return true;
        }
    }
    return false;
}

// The sender's playthrough (0 when the packet does not say: an older build or stock SoH).
inline uint32_t MsAnchor_PacketSeed(const nlohmann::json& payload) {
    if (payload.is_object() && payload.contains(MS_ANCHOR_KEY_SEED) && payload[MS_ANCHOR_KEY_SEED].is_number()) {
        return payload[MS_ANCHOR_KEY_SEED].get<uint32_t>();
    }
    return 0;
}

// True when `payload` belongs to the playthrough `localSeed` (AnchorNet.h MS_ANCHOR_KEY_SEED).
inline bool MsAnchor_SeedMatches(const nlohmann::json& payload, uint32_t localSeed) {
    return MsAnchor_PacketSeed(payload) == localSeed;
}

struct MsAnchorHost {
    const MsHostNet* net = nullptr;
    void* host = nullptr;
    int32_t engineId = MS_ENGINE_NONE;

    // From MsEngine_Init's services: the host's connection, if this host has one.
    void Set(const MsHostServices* services, int32_t ownEngineId) {
        engineId = ownEngineId;
        if (services != nullptr && MS_HOST_HAS(services, net) && services->net != nullptr &&
            services->net->size >= sizeof(MsHostNet)) {
            net = services->net;
            host = services->host;
        }
    }

    bool Present() const {
        return net != nullptr;
    }

    MsNetStatus Status() const {
        MsNetStatus status = {};
        status.size = sizeof(status);
        if (net != nullptr) {
            net->GetStatus(host, &status);
        }
        return status;
    }

    // The host runs this engine now (true without a host: a standalone game is always active).
    bool IsActive() const {
        return net == nullptr || Status().activeEngine == engineId;
    }

    static uint64_t Seq(const nlohmann::json& payload) {
        if (payload.is_object() && payload.contains(MS_ANCHOR_KEY_SEQ) && payload[MS_ANCHOR_KEY_SEQ].is_number()) {
            return payload[MS_ANCHOR_KEY_SEQ].get<uint64_t>();
        }
        return 0;
    }

    // A packet that must be applied once per process (items, a team state reply, a notification): true for the
    // first engine that claims it. Packets that did not come through the host (standalone, loopback, the team
    // queue replayed from UPDATE_TEAM_STATE) are always this engine's.
    bool Claim(const nlohmann::json& payload) const {
        const uint64_t seq = Seq(payload);
        if (net == nullptr || seq == 0) {
            return true;
        }
        return net->Claim(host, seq, engineId) != 0;
    }

    // Would Claim succeed? (engineId < 0 asks without claiming; AnchorNet.h.)
    bool CanClaim(const nlohmann::json& payload) const {
        const uint64_t seq = Seq(payload);
        if (net == nullptr || seq == 0) {
            return true;
        }
        return net->Claim(host, seq, -1) != 0;
    }

    // A teammate's item is announced ("<player> found ...") once per process: true for the first engine that asks
    // (AnchorNet.h MS_ANCHOR_CLAIM_ANNOUNCED). Independent of Claim: the announcing engine may leave the item itself
    // to the other engine, which then gives it without a second notification.
    bool Announce(const nlohmann::json& payload) const {
        const uint64_t seq = Seq(payload);
        if (net == nullptr || seq == 0) {
            return true;
        }
        return net->Claim(host, seq | MS_ANCHOR_CLAIM_ANNOUNCED, engineId) != 0;
    }

    bool GetBlob(const char* key, std::string& out) const {
        if (net == nullptr) {
            return false;
        }
        std::vector<char> buffer(4096);
        int32_t length = net->GetBlob(host, key, buffer.data(), (uint32_t)buffer.size());
        if (length < 0) {
            buffer.resize((size_t)-length);
            length = net->GetBlob(host, key, buffer.data(), (uint32_t)buffer.size());
        }
        if (length <= 0) {
            return false;
        }
        out.assign(buffer.data(), (size_t)length);
        return true;
    }

    bool GetBlobJson(const char* key, nlohmann::json& out) const {
        std::string text;
        if (!GetBlob(key, text)) {
            return false;
        }
        try {
            out = nlohmann::json::parse(text);
        } catch (...) { return false; }
        return true;
    }

    void SetBlob(const char* key, const std::string& value) const {
        if (net != nullptr) {
            net->SetBlob(host, key, value.data(), (uint32_t)value.size());
        }
    }

    void SetBlobJson(const char* key, const nlohmann::json& value) const {
        SetBlob(key, value.dump());
    }
};
