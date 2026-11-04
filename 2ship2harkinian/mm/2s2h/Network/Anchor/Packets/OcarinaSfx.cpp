#include "2s2h/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "audiothread_cmd.h"
#include "sfx.h"
extern PlayState* gPlayState;
extern s8 sOcarinaInstrumentId;     // src/audio/code_8019AF00.c
extern f32 sDefaultOcarinaVolume;   // src/audio/code_8019AF00.c
}

// SFX_CHANNEL_OCARINA (src/audio/code_8019AF00.c)
#define ANCHOR_SFX_CHANNEL_OCARINA 0xD

/**
 * OCARINA_SFX
 *
 * Ocarina effects, only sent to other clients in the same scene as the player. Majora's Mask's forms each play their
 * own instrument (the Deku pipes, the Goron drums, the Zora guitar): the instrument goes along with the note.
 */

void Anchor::SendPacket_OcarinaSfx(uint8_t pitch, float modulator, int8_t bend) {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;

    payload["type"] = OCARINA_SFX;
    payload["note"] = pitch;
    payload["modulator"] = modulator;
    payload["bend"] = bend;
    payload["instrument"] = sOcarinaInstrumentId;
    payload["world"] = MS_ANCHOR_WORLD_MM;
    payload["quiet"] = true;

    for (auto& [clientId, client] : clients) {
        if (IsInOwnScene(client) && client.online && client.isSaveLoaded && !client.self) {
            payload["targetClientId"] = clientId;
            SendJsonToRemote(payload);
        }
    }
}

void Anchor::HandlePacket_OcarinaSfx(nlohmann::json payload) {
    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    // (a Hyrule player's note is Ocarina of Time's; never in this scene anyway)
    if (payload.value("world", std::string(MS_ANCHOR_WORLD_OOT)) != MS_ANCHOR_WORLD_MM) {
        return;
    }
    uint8_t note = payload.at("note").get<uint8_t>();
    float modulator = payload.at("modulator").get<float>();
    int8_t bend = payload.at("bend").get<int8_t>();
    s8 instrument = payload.value("instrument", (s8)1);

    if (!clients.contains(clientId) || !clients[clientId].player) {
        return;
    }

    auto& client = clients[clientId];
    client.ocarinaModulator = modulator;
    client.ocarinaBend = bend;

    if ((note != OCARINA_PITCH_NONE) && (client.ocarinaPitch != note)) {
        // As AudioOcarina_PlayControllerInput: the instrument on io port 7, the pitch on port 5.
        AUDIOCMD_CHANNEL_SET_IO(SEQ_PLAYER_SFX, ANCHOR_SFX_CHANNEL_OCARINA, 7, instrument - 1);
        AUDIOCMD_CHANNEL_SET_IO(SEQ_PLAYER_SFX, ANCHOR_SFX_CHANNEL_OCARINA, 5, note);
        AudioSfx_PlaySfx(NA_SE_OC_OCARINA, &client.player->actor.projectedPos, 4, &client.ocarinaModulator,
                         &sDefaultOcarinaVolume, &gSfxDefaultReverb);
    } else if ((client.ocarinaPitch != OCARINA_PITCH_NONE) && (note == OCARINA_PITCH_NONE)) {
        AudioSfx_StopById(NA_SE_OC_OCARINA);
    }

    client.ocarinaPitch = note;
}
