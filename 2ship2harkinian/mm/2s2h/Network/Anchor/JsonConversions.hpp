#pragma once

// MergeShip2 -- Anchor's JSON conversions for 2 Ship 2 Harkinian (SoH's JsonConversions.hpp; Vec3f's and Vec3s's are
// 2Ship's own, in BenJsonConversions.hpp).

#ifdef __cplusplus

#include <nlohmann/json.hpp>
#include "2s2h/BenJsonConversions.hpp"
#include "2s2h/Network/Anchor/Anchor.h"

extern "C" {
#include "z64.h"
}

inline void from_json(const json& j, Color_RGB8& color) {
    j.at("r").get_to(color.r);
    j.at("g").get_to(color.g);
    j.at("b").get_to(color.b);
}

inline void to_json(json& j, const Color_RGB8& color) {
    j = json{ { "r", color.r }, { "g", color.g }, { "b", color.b } };
}

inline void to_json(json& j, const PosRot& posRot) {
    j = json{ { "pos", posRot.pos }, { "rot", posRot.rot } };
}

inline void from_json(const json& j, PosRot& posRot) {
    j.at("pos").get_to(posRot.pos);
    j.at("rot").get_to(posRot.rot);
}

inline void from_json(const json& j, AnchorClient& client) {
    client.clientId = j.value("clientId", (u32)0);
    client.name = j.value("name", "???");
    client.color = j.value("color", Color_RGB8{ 255, 255, 255 });
    client.clientVersion = j.value("clientVersion", "???");
    client.teamId = j.value("teamId", "default");
    client.online = j.value("online", false);
    client.seed = j.value("seed", (u32)0);
    client.isSaveLoaded = j.value("isSaveLoaded", false);
    client.isGameComplete = j.value("isGameComplete", false);
    client.sceneNum = j.value("sceneNum", (s16)-1);
    client.curRoomNum = j.value("curRoomNum", (s8)-1);
    client.entranceIndex = j.value("entranceIndex", (s32)0);
    client.self = j.value("self", false);
    client.world = j.value("world", std::string(MS_ANCHOR_WORLD_OOT));
    client.sceneName = j.value("sceneName", std::string());
    client.cycle = j.value("cycle", (u16)0);
    client.form = j.value("form", (u8)PLAYER_FORM_HUMAN);
}

#endif // __cplusplus
