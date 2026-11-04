// MergeShip2 -- singleplayer mode in Termina: Navi in Tatl's place (MergeShipSingleplayer.h).

#include "MergeShipSingleplayer.h"

#include <algorithm>
#include <cstring>
#include <string>

#include <libultraship/bridge/resourcebridge.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Rando/Rando.h"
#include "mergeship/EngineApi.h"

extern "C" {
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
extern char** gFontMap; // src/audio/lib/load.c: each sound font's resource path
extern size_t gFontMapSize;
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
void MergeShipTest_Log(const char* fmt, ...);

// z_actor.c: the lock-on colours, whose ACTORCAT_PLAYER entry is the fairy's own colour when she rests by Link.
typedef struct AttentionColor {
    Color_RGBA8 primary;
    Color_RGBA8 secondary;
} AttentionColor;
extern AttentionColor sAttentionColors[ACTORCAT_MAX + 1];
}

namespace {

bool sActive = false;     // the last trip from Hyrule was a singleplayer file's
bool sFdFromMoon = false; // ... and the menu lets the moon's child give the Fierce Deity's Mask

// ---- colours -----------------------------------------------------------------------------------------------------------

// Majora's Mask's Tatl resting by Link, and Ocarina of Time's Navi (z_actor.c sNaviColorList[ACTORCAT_PLAYER]). The
// other categories' colours (enemies yellow, people blue, the rest green) are the same in both games.
const AttentionColor kTatlColor = { { 255, 255, 230, 255 }, { 220, 160, 80, 0 } };
const AttentionColor kNaviColor = { { 255, 255, 255, 255 }, { 0, 0, 255, 0 } };

void ApplyColors(bool navi) {
    sAttentionColors[ACTORCAT_PLAYER] = navi ? kNaviColor : kTatlColor;
}

// ---- voice -------------------------------------------------------------------------------------------------------------
//
// Tatl's voice is Majora's Mask's font 0 sound effects 0x2B/0x2C (NA_SE_VO_NAVY_ENEMY), 0x2F/0x30 (NA_SE_VO_NAVY_HELLO)
// and 0x34 (NA_SE_VO_NAVY_HEAR) -- the entries those sound effects play (build/tests/mm_sp_tatl_probe). Navi's are
// Ocarina of Time's font 0 sound effects with her samples ("Navi - Watch Out!", "Look!", "Hey!", "Hello!",
// "Listen!"), found by sample: they take those entries' place while singleplayer is on.

constexpr const char* kOoTSfxFont = "oot/audio/fonts/00_Sound_Effects_1";
struct VoiceSwap {
    u16 mmSfx;
    const char* naviSample; // the sample's resource name in Ocarina of Time's archive
    u16 ootSfxGuess;        // its sound effect in Ocarina of Time's font 0, when the sample is not found by name
};
const VoiceSwap kVoice[] = {
    { 0x2B, "Navi - Watch Out!", 0x3B }, { 0x2C, "Navi - Look!", 0x3C },  { 0x2F, "Navi - Hey!", 0x3D },
    { 0x30, "Navi - Hello!", 0x85 },     { 0x34, "Navi - Listen!", 0x7D },
};
constexpr size_t kVoiceCount = sizeof(kVoice) / sizeof(kVoice[0]);

SoundFont* sVoiceFont = nullptr; // Majora's Mask's font 0 while Navi's voice is in it
SoundEffect sVoiceSaved[kVoiceCount];

// Ocarina of Time's font 0 sound effect that plays `sample`.
s32 FindOoTSfx(SoundFont* oot, const char* sample, u16 guess) {
    const std::string names[] = { std::string("oot/audio/samples/") + sample + "_META",
                                  std::string("audio/samples/") + sample + "_META" };
    for (const std::string& name : names) {
        void* res = ResourceGetDataByName(name.c_str());
        if (res == nullptr) {
            continue;
        }
        for (s32 i = 0; i < oot->numSfx; i++) {
            if ((void*)oot->soundEffects[i].tunedSample.sample == res) {
                return i;
            }
        }
    }
    return (guess < oot->numSfx) ? guess : -1;
}

void ApplyVoice(bool navi) {
    if (!navi) {
        if (sVoiceFont != nullptr) {
            for (size_t i = 0; i < kVoiceCount; i++) {
                sVoiceFont->soundEffects[kVoice[i].mmSfx] = sVoiceSaved[i];
            }
            sVoiceFont = nullptr;
            MergeShipTest_Log("SPNAVI voice: Tatl's again");
        }
        return;
    }
    if ((sVoiceFont != nullptr) || (gFontMap == nullptr) || (gFontMapSize == 0) || (gFontMap[0] == nullptr)) {
        return;
    }
    SoundFont* mm = ResourceMgr_LoadAudioSoundFontByName(gFontMap[0]);
    SoundFont* oot = ResourceMgr_LoadAudioSoundFontByName(kOoTSfxFont);
    if ((mm == nullptr) || (oot == nullptr) || (mm->soundEffects == nullptr) || (oot->soundEffects == nullptr)) {
        return;
    }
    s32 ootSfx[kVoiceCount];
    for (size_t i = 0; i < kVoiceCount; i++) {
        ootSfx[i] = FindOoTSfx(oot, kVoice[i].naviSample, kVoice[i].ootSfxGuess);
        if ((kVoice[i].mmSfx >= mm->numSfx) || (ootSfx[i] < 0) ||
            (oot->soundEffects[ootSfx[i]].tunedSample.sample == nullptr)) {
            return;
        }
    }
    for (size_t i = 0; i < kVoiceCount; i++) {
        sVoiceSaved[i] = mm->soundEffects[kVoice[i].mmSfx];
        mm->soundEffects[kVoice[i].mmSfx] = oot->soundEffects[ootSfx[i]];
        MergeShipTest_Log("SPNAVI voice: MM font 0 sfx 0x%02X <- OoT font 0 sfx 0x%02X (%s)", kVoice[i].mmSfx, ootSfx[i],
                          kVoice[i].naviSample);
    }
    sVoiceFont = mm;
}

// ---- lines -------------------------------------------------------------------------------------------------------------
//
// Every line Majora's Mask gives Tatl is Navi's (MergeShipNaviLines.inc: her voice, Link's name, her story -- Link's own
// fairy, snatched up by the Skull Kid in the woods). Anything else that names Tatl names Navi (the same length: the box
// keeps its layout).

struct NaviLine {
    u16 textId;
    const char* text;
};
const NaviLine kNaviLines[] = {
#include "MergeShipNaviLines.inc"
};

int Hex(char c) {
    return (c >= '0' && c <= '9') ? (c - '0') : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : -1;
}

// The table's markup to Majora's Mask's message bytes: {hh} a byte, {sfxHHLL} a voice, \n a new line, | a new box.
std::string Encode(const char* text) {
    std::string out;
    for (const char* p = text; *p != '\0'; p++) {
        if ((p[0] == '{') && (strncmp(p + 1, "sfx", 3) == 0) && (p[8] == '}')) {
            out += '\x1E';
            out += (char)((Hex(p[4]) << 4) | Hex(p[5]));
            out += (char)((Hex(p[6]) << 4) | Hex(p[7]));
            p += 8;
        } else if ((p[0] == '{') && (Hex(p[1]) >= 0) && (Hex(p[2]) >= 0) && (p[3] == '}')) {
            out += (char)((Hex(p[1]) << 4) | Hex(p[2]));
            p += 3;
        } else if ((p[0] == '\\') && (p[1] == 'n')) {
            out += '\x11';
            p += 1;
        } else if (p[0] == '|') {
            out += '\x10';
        } else {
            out += *p;
        }
    }
    out += '\xBF';
    return out;
}

void OnOpenText(u16* textId, bool* loadFromMessageTable) {
    if (!MergeShip_Singleplayer_Active() || !*loadFromMessageTable || (gPlayState == nullptr)) {
        return;
    }
    const NaviLine* line = nullptr;
    for (const NaviLine& l : kNaviLines) {
        if (l.textId == *textId) {
            line = &l;
            break;
        }
    }
    CustomMessage::Entry entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
    if (line != nullptr) {
        entry.msg = Encode(line->text);
    } else if (entry.msg.find("Tatl") != std::string::npos) {
        for (size_t at = entry.msg.find("Tatl"); at != std::string::npos; at = entry.msg.find("Tatl", at + 4)) {
            entry.msg.replace(at, 4, "Navi");
        }
    } else {
        return;
    }
    entry.autoFormat = false;
    CustomMessage::LoadCustomMessageIntoFont(entry);
    *loadFromMessageTable = false;
    MergeShipTest_Log("SPNAVI text 0x%04X as Navi's%s", *textId, (line != nullptr) ? " (her line)" : "");
}

// ---- per frame ---------------------------------------------------------------------------------------------------------

void Apply() {
    const bool navi = MergeShip_Singleplayer_Active();
    ApplyColors(navi);
    ApplyVoice(navi);
}

void RegisterSingleplayer() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(OnOpenText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() { Apply(); });
}

static RegisterShipInitFunc sInit(RegisterSingleplayer, {});

} // namespace

extern "C" void MergeShip_Singleplayer_OnTransfer(const MsTransfer* arrival) {
    const bool was = sActive;
    sActive = (arrival->flags & MS_TRANSFER_FLAG_SINGLEPLAYER) != 0;
    sFdFromMoon = (arrival->flags & MS_TRANSFER_FLAG_SP_FD_FROM_MOON) != 0;
    if (was != sActive) {
        MergeShipTest_Log("SPNAVI singleplayer %s (Fierce Deity from the moon %d)", sActive ? "on" : "off", sFdFromMoon);
    }
}

extern "C" s32 MergeShip_Singleplayer_Active(void) {
    return sActive && !IS_RANDO;
}

extern "C" s32 MergeShip_Singleplayer_MoonKeepsFierceDeity(void) {
    return MergeShip_Singleplayer_Active() && !sFdFromMoon;
}

extern "C" const char* MergeShip_Singleplayer_CUpLabel(void) {
    static const char kNaviLabel[] = "__OTR__oot/textures/parameter_static/gNaviCUpENGTex";
    return MergeShip_Singleplayer_Active() ? kNaviLabel : nullptr;
}

extern "C" void MergeShip_Singleplayer_DebugSet(s32 active, s32 fdFromMoon) {
    sActive = active != 0;
    sFdFromMoon = fdFromMoon != 0;
    MergeShipTest_Log("SPNAVI singleplayer %s (debug; Fierce Deity from the moon %d)", sActive ? "on" : "off",
                      sFdFromMoon);
}
