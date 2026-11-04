// MergeShip2 -- Majora's Mask sound effects inside Ocarina of Time, played from Majora's Mask's own data in
// mm.o2r exactly as Majora's Mask plays them.
//
// DESIGN
//
// An MM sfx id means nothing to Ocarina of Time's tables: the same number names a different sound in each game,
// and what a sound *is* lives in the sequence-0 script (per-bank dispatch tables, envelopes, layers, pitch
// changes) and in the sound fonts it plays from. So MM sounds are played by MM's own pipeline, end to end,
// running beside Ocarina of Time's rather than through it:
//
//   game thread   MergeShip_PlayMMSfx* / MergeShip_MMSfx_PlaySfx          (MergeShipMMSfx.c)
//                 = MM AudioSfx_PlaySfx. MM's request ring, its seven sfx banks, its channel allocation
//                 (gChannelsPerBank/gUsedChannelsPerBank) and MM's per-sfx parameters (importance, distance
//                 range, random pitch, flags; MergeShipMMSfxTables.inc, generated from MM's
//                 tables/sfx/*bank_table.h). Every frame (Audio_Update) MM's AudioSfx_ProcessRequests and
//                 AudioSfx_ProcessActiveSfx run; AudioSfx_SetProperties computes volume, reverb, pan,
//                 frequency, filters from the sound's projected position with MM's formulas and hands them to
//                 the sequence the way MM does: io ports 0/2/4/5 by audio command, the rest through
//                 SfxChannelState, which the sequence reads with its 0xA0-0xA3 commands.
//
//   audio thread  a fifth sequence player running MM's sequence 0       (MergeShipMMSeqPlayer.c)
//                 with MM's script interpreter (sequence, channel and layer commands ported from MM's
//                 seqplayer.c; Ocarina of Time's interpreter cannot run MM's sequence 0, whose commands
//                 0xA0-0xA8, 0xBD, 0xBE, layer 0xF0/0xF1, seq 0xC2/0xC3 are MM's). It owns its 16 channels
//                 and 64 layers; audio commands addressed to player MERGESHIP_MM_SEQ_PLAYER reach it through
//                 Audio_ProcessCmd. It is processed after Ocarina of Time's four players and its notes are
//                 mixed by the same synthesizer, with MM's per-layer start sample position, reverb/gain and
//                 vibrato source applied through small hooks in audio_playback.c / audio_effects.c.
//
//   resources     this file: mm/audio/sequences/Sequence_0 and the sound fonts in its font list
//                 (mm/audio/fonts/Soundfont_0 and _1), appended to Ocarina of Time's fontMap under new font
//                 ids at AudioLoad_Init, so Ocarina of Time's loaders and caches handle them like any font.
//                 MM's font resources name their samples "audio/samples/..." (the MM exporter wrote them
//                 unprefixed); the sound font factory resolves them to "mm/audio/samples/..." for fonts under
//                 mm/, since an unprefixed lookup would find Ocarina of Time's sample of the same name, or none.
//
// Nothing of Ocarina of Time's is replaced: OoT's sfx banks, sequence player 2, its channels, layers, fonts
// and sequences are untouched, and every hook is a no-op for OoT's own players and layers. The MM player only
// runs while MM sounds are in use (it starts on the first MM request and stops after 20 idle seconds), so a
// game that never plays an MM sound -- a human Link -- has exactly Ocarina of Time's audio. While it runs it
// shares the synthesizer's notes with Ocarina of Time's players, as MM's own players share them.
//
// For MM forms, Ocarina of Time's shared player code still requests OoT sfx ids; z_player.c / z_actor.c route
// them here: the OoT id is mapped to the MM id with the same NA_SE_ name (plus the three sounds MM renamed),
// MM's voice/floor/surface offsets are added (the forms' ageProperties hold MM's values) and the sound is played
// the way MM's Player_AnimSfx_Play* plays it. The forms' ocarina is MM's: Deku pipes, Goron drums, Zora guitar
// from MM's sequence 0 and fonts.
//
// Majora's Mask's surround-sound-only effects (surround effect index, Haas delay) are not reproduced: Ocarina of
// Time's synthesizer has no counterpart, and they do nothing in stereo or headphone output.

#include <algorithm>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#define _strdup strdup // MergeShip2 Android
#endif
#include <map>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

#include "soh/ResourceManagerHelpers.h"
#include "soh/MergeShip/MergeShipAssets.h"

extern "C" {
#include "z64.h"
#include "soh/MergeShip/MergeShipMMAudio.h"

extern size_t fontMapSize;
extern char** fontMap;
extern AudioContext gAudioContext;
}

namespace {

constexpr const char* kMMSequence0 = MERGESHIP_MM_NAMESPACE "/audio/sequences/Sequence_0";
constexpr const char* kMMFontsMask = MERGESHIP_MM_NAMESPACE "/audio/fonts*";

bool sRegistered = false;
u8 sSeqFonts[16];
u8 sSeqRawFonts[16]; // MM's own font ids, parallel to sSeqFonts
s32 sSeqNumFonts = 0;
u8* sSeqData = nullptr;
s32 sSeqDataSize = 0;

} // namespace

extern "C" void MergeShip_MMAudio_RegisterFonts(void) {
    // Called by AudioLoad_Init and again when the MM sequence is first started, in case audio initialised
    // before mm.o2r was mounted.
    if (sRegistered || (fontMap == nullptr) || !MergeShip_IsMajorasMaskMounted()) {
        return;
    }
    sRegistered = true;

    SequenceData* seq = ResourceMgr_LoadSeqPtrByName(kMMSequence0);
    if ((seq == nullptr) || (seq->seqData == nullptr) || (seq->seqDataSize <= 0) || (seq->numFonts <= 0)) {
        SPDLOG_ERROR("MergeShip: {} did not load; Majora's Mask sound effects are unavailable", kMMSequence0);
        return;
    }

    // The resource carrying MM font index `index`. MM's exporter names them Soundfont_<index>; loading a font
    // loads all of its samples, so the name is tried first and the whole directory is scanned only if the
    // resource there carries another index.
    std::map<s32, std::string> scanned;
    bool didScan = false;
    auto findFont = [&](s32 index, std::string& outPath) -> bool {
        std::string byName = std::string(MERGESHIP_MM_NAMESPACE "/audio/fonts/Soundfont_") + std::to_string(index);
        SoundFont* sf = ResourceMgr_LoadAudioSoundFontByName(byName.c_str());
        if ((sf != nullptr) && (sf->fntIndex == index)) {
            outPath = byName;
            return true;
        }
        if (!didScan) {
            didScan = true;
            int count = 0;
            char** list = ResourceMgr_ListFiles(kMMFontsMask, &count);
            for (int i = 0; i < count; i++) {
                SoundFont* font = ResourceMgr_LoadAudioSoundFontByName(list[i]);
                if (font != nullptr) {
                    scanned.emplace(font->fntIndex, list[i]);
                }
                free(list[i]);
            }
            free(list);
        }
        auto it = scanned.find(index);
        if (it == scanned.end()) {
            return false;
        }
        outPath = it->second;
        return true;
    };

    // Append sequence 0's fonts to Ocarina of Time's font table. Channel font ids are u8 and 0xFF means none.
    s32 numFonts = std::min<s32>(seq->numFonts, (s32)sizeof(sSeqFonts));
    std::vector<std::string> paths;
    for (s32 i = 0; i < numFonts; i++) {
        std::string path;
        if (!findFont(seq->fonts[i], path)) {
            SPDLOG_ERROR("MergeShip: MM sequence 0 needs sound font {}, which mm.o2r lacks", seq->fonts[i]);
            return;
        }
        paths.push_back(path);
    }
    if (fontMapSize + paths.size() >= 0xFF) {
        SPDLOG_ERROR("MergeShip: no room for MM's sound fonts ({} fonts already registered)", fontMapSize);
        return;
    }

    size_t base = fontMapSize;
    char** newMap = (char**)realloc(fontMap, (base + paths.size()) * sizeof(char*));
    u8* newStatus = (u8*)realloc(gAudioContext.fontLoadStatus, base + paths.size());
    if ((newMap == nullptr) || (newStatus == nullptr)) {
        if (newMap != nullptr) {
            fontMap = newMap;
        }
        if (newStatus != nullptr) {
            gAudioContext.fontLoadStatus = newStatus;
        }
        SPDLOG_ERROR("MergeShip: out of memory registering MM's sound fonts");
        return;
    }
    fontMap = newMap;
    gAudioContext.fontLoadStatus = newStatus;
    for (size_t i = 0; i < paths.size(); i++) {
        fontMap[base + i] = _strdup(paths[i].c_str());
        gAudioContext.fontLoadStatus[base + i] = 0;
        sSeqFonts[i] = (u8)(base + i);
        sSeqRawFonts[i] = (u8)seq->fonts[i];
    }
    fontMapSize = base + paths.size();
    sSeqNumFonts = (s32)paths.size();

    // The sequence writes into itself (MM's 0xC7/0xCF/0xA6 commands), so it runs from a private copy, kept for
    // the whole session as MM keeps sequence 0 cached.
    sSeqData = (u8*)malloc(seq->seqDataSize);
    if (sSeqData == nullptr) {
        sSeqNumFonts = 0;
        return;
    }
    memcpy(sSeqData, seq->seqData, seq->seqDataSize);
    sSeqDataSize = seq->seqDataSize;

    SPDLOG_INFO("MergeShip: MM sequence 0 ({} bytes) and {} sound font(s) registered as font ids {}..{}", sSeqDataSize,
                sSeqNumFonts, (int)base, (int)(base + paths.size() - 1));
}

extern "C" s32 MergeShip_MMAudio_GetSeqFonts(u8* outFonts, s32 maxFonts) {
    s32 n = std::min(sSeqNumFonts, maxFonts);

    for (s32 i = 0; i < n; i++) {
        outFonts[i] = sSeqFonts[i];
    }
    return (sSeqData != nullptr) ? n : 0;
}

// The font id this session registered MM's sound font `mmFontId` as, or 0xFF. MM channels that are never set
// up keep font 0 (their zeroed state), which in MM means MM's font 0, not Ocarina of Time's.
// mm.o2r keeps each sample's ROM medium (MEDIUM_CART for most of MM's banks). The resource manager has already
// loaded the sample into memory, but Ocarina of Time's synthesizer would take a non-RAM medium as a ROM address to
// DMA from (and crash on the pointer), so an MM font's samples are marked as in RAM when it is registered.
static s32 MarkSampleInRam(SoundFontSample* sample) {
    if ((sample == nullptr) || (sample->sampleAddr == nullptr) || (sample->medium == MEDIUM_RAM)) {
        return 0;
    }
    sample->medium = MEDIUM_RAM;
    return 1;
}

static s32 MarkFontSamplesInRam(SoundFont* sf) {
    s32 changed = 0;

    for (s32 i = 0; i < sf->numInstruments; i++) {
        Instrument* inst = (sf->instruments != nullptr) ? sf->instruments[i] : nullptr;
        if (inst != nullptr) {
            changed += MarkSampleInRam(inst->lowNotesSound.sample);
            changed += MarkSampleInRam(inst->normalNotesSound.sample);
            changed += MarkSampleInRam(inst->highNotesSound.sample);
        }
    }
    for (s32 i = 0; i < sf->numDrums; i++) {
        Drum* drum = (sf->drums != nullptr) ? sf->drums[i] : nullptr;
        if (drum != nullptr) {
            changed += MarkSampleInRam(drum->sound.sample);
        }
    }
    for (s32 i = 0; i < sf->numSfx; i++) {
        if (sf->soundEffects != nullptr) {
            changed += MarkSampleInRam(sf->soundEffects[i].sample);
        }
    }
    return changed;
}

// MM's other sound fonts, registered the first time a sequence needs one (the masks' fanfares use MM fonts that
// sequence 0 does not).
static u8 sExtraFonts[64];
static u8 sExtraRawFonts[64];
static s32 sExtraNumFonts = 0;

extern "C" u8 MergeShip_MMAudio_MapFont(u8 mmFontId) {
    for (s32 i = 0; i < sSeqNumFonts; i++) {
        if (sSeqRawFonts[i] == mmFontId) {
            return sSeqFonts[i];
        }
    }
    for (s32 i = 0; i < sExtraNumFonts; i++) {
        if (sExtraRawFonts[i] == mmFontId) {
            return sExtraFonts[i];
        }
    }
    return 0xFF;
}

// MergeShip_MMAudio_MapFont, registering MM font `mmFontId` (mm.o2r's Soundfont_<id>) after Ocarina of Time's and
// sequence 0's if it is not registered yet.
extern "C" u8 MergeShip_MMAudio_EnsureFont(u8 mmFontId) {
    u8 font = MergeShip_MMAudio_MapFont(mmFontId);
    std::string path = std::string(MERGESHIP_MM_NAMESPACE "/audio/fonts/Soundfont_") + std::to_string(mmFontId);
    SoundFont* sf;
    size_t base;

    if (font != 0xFF) {
        return font;
    }
    if ((sExtraNumFonts >= (s32)sizeof(sExtraFonts)) || (fontMapSize + 1 >= 0xFF)) {
        return 0xFF;
    }
    sf = ResourceMgr_LoadAudioSoundFontByName(path.c_str());
    if ((sf == nullptr) || (sf->fntIndex != mmFontId)) {
        SPDLOG_ERROR("MergeShip: mm.o2r has no sound font {}", (int)mmFontId);
        return 0xFF;
    }
    s32 moved = MarkFontSamplesInRam(sf);
    SPDLOG_INFO("MergeShip: MM sound font {}: {} sample(s) marked in RAM", (int)mmFontId, (int)moved);

    base = fontMapSize;
    char** newMap = (char**)realloc(fontMap, (base + 1) * sizeof(char*));
    if (newMap == nullptr) {
        return 0xFF;
    }
    fontMap = newMap;
    u8* newStatus = (u8*)realloc(gAudioContext.fontLoadStatus, base + 1);
    if (newStatus == nullptr) {
        return 0xFF;
    }
    gAudioContext.fontLoadStatus = newStatus;
    fontMap[base] = _strdup(path.c_str());
    gAudioContext.fontLoadStatus[base] = 0;
    fontMapSize = base + 1;

    sExtraFonts[sExtraNumFonts] = (u8)base;
    sExtraRawFonts[sExtraNumFonts] = mmFontId;
    sExtraNumFonts++;
    SPDLOG_INFO("MergeShip: MM sound font {} registered as font id {}", (int)mmFontId, (int)base);
    return (u8)base;
}

extern "C" u8* MergeShip_MMAudio_GetSeqData(s32* outSize) {
    if (outSize != nullptr) {
        *outSize = sSeqDataSize;
    }
    return sSeqData;
}

// ---- The forms' song fanfares ------------------------------------------------------------------------------
//
// After a song is recognised the message code plays the song back: the ocarina's own notes (muted with sfx bank
// 5; they only drive the staff) and the song's fanfare sequence, which is what is heard. Majora's Mask plays that
// fanfare with the form's instrument (MM z_message.c MSGMODE_SETUP_DISPLAY_SONG_PLAYED:
// Audio_PlayFanfareWithPlayerIOPort7(sOcarinaSongFanfares[song], sOcarinaSongFanfareIoData[form])). MM's
// ocarina fanfares read io port 7 of the fanfare player (0x35 human, 0x5C Goron, 0x5D Zora, 0x5E Deku), store
// it as their channel's instrument -- MM sound font 0's Goron drums, Zora guitar and Deku pipes -- set the
// master volume from it (0x46 human, 0x69 Goron, 0x55 Zora and Deku), and for the Goron and the Zora skip the
// human's volume/vibrato shaping, waiting for the note layer to finish instead.
//
// Majora's Mask has its own fanfares for four of Ocarina of Time's songs (Epona's Song, Sun's Song, Song of Time,
// Song of Storms); those are played from mm.o2r as MM plays them. Saria's Song and Zelda's Lullaby have none, so
// Ocarina of Time's fanfare -- the same single-channel arrangement MM's are built from -- is rewritten the way
// MM's io-port-7 code rewrites its own. The six warp songs have no MM counterpart either; their fanfares are
// orchestral arrangements whose channel 5 plays the ocarina (warp-song font instrument 5, "Ocarina_Looped"), so
// that channel is switched to MM's sound font 0 and the form's instrument, and the orchestra is left as it is.
//
// The sequences are served under virtual resource paths (ResourceManager::CacheExternalResource) with MM's
// sound font 0 (as registered by MergeShip_MMAudio_RegisterFonts) as their font, under sequence ids reserved
// at AudioLoad_Init (MergeShip_MMAudio_ReserveFanfareIds). Sequence commands carry 8-bit ids, so the ids are
// taken below 0xFF.

#include <iterator>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/File.h>
#include "soh/resource/type/AudioSequence.h"
#include "soh/Enhancements/audio/AudioCollection.h"

extern "C" {
#include "macros.h"
#include "functions.h"
#include "soh/MergeShip/MergeShipForms.h"
extern char** sequenceMap;
extern size_t sequenceMapSize;
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

namespace {

// Ocarina of Time's OCARINA_SONG_* (z_message_PAL.c sOcarinaSongFanfares order).
enum {
    kSongMinuet,
    kSongBolero,
    kSongSerenade,
    kSongRequiem,
    kSongNocturne,
    kSongPrelude,
    kSongSarias,
    kSongEponas,
    kSongLullaby,
    kSongSuns,
    kSongTime,
    kSongStorms,
};

enum FanfareKind {
    kFanfareMM,      // MM's own fanfare, told the form through io port 7
    kFanfareOoTSolo, // Ocarina of Time's single-channel ocarina fanfare, rewritten per form as MM's are
    kFanfareOoTWarp, // Ocarina of Time's warp-song arrangement, its ocarina channel given the form's instrument
};

struct MMFanfare {
    s32 song;
    FanfareKind kind;
    const char* path;
};

const MMFanfare kFanfares[] = {
    { kSongEponas, kFanfareMM, MERGESHIP_MM_NAMESPACE "/audio/sequences/EponasSong(Ocarina)_32" },
    { kSongSuns, kFanfareMM, MERGESHIP_MM_NAMESPACE "/audio/sequences/SunsSong(Ocarina)_33" },
    { kSongTime, kFanfareMM, MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfTime(Ocarina)_34" },
    { kSongStorms, kFanfareMM, MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfStorms(Ocarina)_35" },
    { kSongSarias, kFanfareOoTSolo, "audio/sequences/068_Sarias_Song" },
    { kSongLullaby, kFanfareOoTSolo, "audio/sequences/070_Zeldas_Lullaby" },
    { kSongMinuet, kFanfareOoTWarp, "audio/sequences/052_Minuet_of_Forest" },
    { kSongBolero, kFanfareOoTWarp, "audio/sequences/051_Bolero_of_Fire" },
    { kSongSerenade, kFanfareOoTWarp, "audio/sequences/053_Serenade_of_Water" },
    { kSongRequiem, kFanfareOoTWarp, "audio/sequences/054_Requiem_of_Spirit" },
    { kSongNocturne, kFanfareOoTWarp, "audio/sequences/055_Nocturne_of_Shadow" },
    { kSongPrelude, kFanfareOoTWarp, "audio/sequences/037_Prelude_of_Light" },
};
constexpr s32 kNumForms = 3; // Goron, Zora, Deku
// MM's masks' own fanfares (MM Audio_PlayFanfare(NA_BGM_BREMEN_MARCH / NA_BGM_KAMARO_DANCE)), played as they are,
// and (MergeShip2, soh_fd) MM's "Get a Mask" (NA_BGM_GET_NEW_MASK), for the transformation masks.
const char* const kMaskFanfares[] = {
    // MM NA_BGM_BREMEN_MARCH is sequence 0x53. mm.o2r's file names are one off here: "BremenMarch_52" is sequence
    // 0x52 (NA_BGM_LEARNED_NEW_SONG) and "GetSong_53" is the Bremen March (each file's own sequence number says so).
    MERGESHIP_MM_NAMESPACE "/audio/sequences/GetSong_53",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/KamaroDance_71",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/GetMask_37",
};
constexpr s32 kNumMaskFanfares = 3; // Bremen, Kamaro, Get a Mask (soh_fd, PR #1)
// MergeShip2: MM's own songs' fanfares (MM sOcarinaSongFanfares), in MergeShipSongs.h order (song -
// MERGESHIP_SONG_FIRST). Each reads io port 7 like the ones above: the form's instrument, or 0x35 -- MM's ocarina --
// for a human. mm.o2r's file names are checked against each file's own sequence number: all ten agree here (0x5B
// Sonata, 0x5C Goron Lullaby, 0x5D New Wave Bossa Nova, 0x5E Elegy, 0x5F Oath, 0x48 Healing, 0x47 Soaring, 0x49
// Inverted Song of Time, 0x4A Song of Double Time, 0x61 NA_BGM_OCARINA_LULLABY_INTRO_PTR, the intro MM plays for
// OCARINA_SONG_GORON_LULLABY_INTRO). Sonata and Goron Lullaby also have a 0x4B / 0x4C file: those are the
// cutscene arrangements (NA_BGM_SONATA_OF_AWAKENING / NA_BGM_GORON_LULLABY, sound font 18), not the ocarina ones.
const char* const kMMSongFanfares[] = {
    MERGESHIP_MM_NAMESPACE "/audio/sequences/SonataOfAwakening(Ocarina)_5B",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/GoronLullaby(Ocarina)_5C",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/NewWaveBossaNova(Ocarina)_5D",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/ElegyOfEmptiness(Ocarina)_5E",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/OathToOrder(Ocarina)_5F",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfHealing(Ocarina)_48",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfSoaring(Ocarina)_47",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/InvertedSongOfTime(Ocarina)_49",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfDoubleTime(Ocarina)_4A",
    MERGESHIP_MM_NAMESPACE "/audio/sequences/LullabyIntroPtr(Ocarina)_61",
};
constexpr s32 kNumMMSongFanfares = 10;
// MergeShip2 (singleplayer): the Happy Mask Salesman's theme, MM NA_BGM_HAPPY_MASK_SALESMAN (0x0A), played on the main
// BGM player while the traveling salesman talks (MergeShipSingleplayer.h).
constexpr const char* kHMSTheme = MERGESHIP_MM_NAMESPACE "/audio/sequences/SongOfHealing(HMS)_0A";
constexpr s32 kNumMMBgms = 1;
// One id per MM fanfare (the form arrives through io port 7), one per form for each rewritten one, one per mask
// fanfare, one per MM song, one per MM BGM.
constexpr s32 kNumFanfareIds = 4 + 8 * kNumForms + kNumMaskFanfares + kNumMMSongFanfares + kNumMMBgms;
constexpr s32 kFirstMaskFanfareId = 4 + 8 * kNumForms;
constexpr s32 kFirstMMSongFanfareId = kFirstMaskFanfareId + kNumMaskFanfares;
constexpr s32 kFirstMMBgmId = kFirstMMSongFanfareId + kNumMMSongFanfares;

s32 sFanfareIds[kNumFanfareIds];
bool sFanfareIdsReserved = false;
bool sFanfareBuilt[kNumFanfareIds];

// MM sOcarinaSongFanfareIoData, for Ocarina of Time's PlayerForm (MM's own table is indexed by MM form).
s32 FormIoData(u8 playerForm) {
    switch (playerForm) {
        case PLAYER_FORM_GORON:
            return 0x5C;
        case PLAYER_FORM_ZORA:
            return 0x5D;
        case PLAYER_FORM_DEKU:
            return 0x5E;
        default:
            return -1;
    }
}

s32 FormSlot(u8 playerForm) {
    return (playerForm == PLAYER_FORM_GORON) ? 0 : (playerForm == PLAYER_FORM_ZORA) ? 1 : 2;
}

// What MM's fanfares do with io port 7 (MM SongOfStorms(Ocarina)_35 and its siblings: `ldio 7`, `stseq` into the
// channel's `instr`, a master volume of 0x69 for 0x5C and 0x55 for 0x5D / 0x5E, and for 0x5C / 0x5D a jump to
// `delay 1; testlayer 0; rbeqz; testlayer 1; rbeqz; end`), applied to Ocarina of Time's arrangement.
bool RewriteOoTFanfare(std::vector<u8>& d, s32 io) {
    size_t chan = 0;
    size_t i;

    // Sequence header: `93 hh ll` (channel 3's script) followed by `db vv` (master volume).
    for (i = 0; (i + 4 < d.size()) && (i < 16); i++) {
        if ((d[i] == 0x93) && (d[i + 3] == 0xDB)) {
            chan = (d[i + 1] << 8) | d[i + 2];
            d[i + 4] = (io == 0x5C) ? 0x69 : 0x55;
            break;
        }
    }
    if ((chan == 0) || (chan >= d.size())) {
        return false;
    }
    // Channel 3: `c1 35` sets the ocarina; it becomes the form's instrument.
    for (i = chan; (i + 4 < d.size()) && (i < chan + 32); i++) {
        if ((d[i] == 0xC1) && (d[i + 1] == 0x35)) {
            d[i + 1] = (u8)io;
            if ((io == 0x5C) || (io == 0x5D)) {
                u16 loop = (u16)d.size();
                const u8 wait[] = { 0xFE, 0x80, 0xF3, 0xFC, 0x81, 0xF3, 0xF9, 0xFF };

                d.insert(d.end(), std::begin(wait), std::end(wait));
                d[i + 2] = 0xFB; // jump
                d[i + 3] = (u8)(loop >> 8);
                d[i + 4] = (u8)loop;
            }
            return true;
        }
    }
    return false;
}

// A warp song's ocarina (channel 5, `instr 5` of the warp-song font): the channel first switches to the
// sequence's second font, MM's sound font 0 (`font 1`, then a jump to its own script), and its `instr` becomes
// the form's instrument there. The other channels keep the warp-song font.
bool RewriteOoTWarpFanfare(std::vector<u8>& d, s32 io) {
    size_t i;

    for (i = 0; (i + 2 < d.size()) && (i < 80); i++) {
        if (d[i] == 0x95) {
            size_t chan = (d[i + 1] << 8) | d[i + 2];
            size_t j;

            if ((chan <= i) || (chan >= d.size())) {
                continue;
            }
            for (j = chan; (j + 1 < d.size()) && (j < chan + 64); j++) {
                if ((d[j] == 0xC1) && (d[j + 1] == 0x05)) {
                    u16 stub = (u16)d.size();
                    const u8 code[] = { 0xC6, 0x01, 0xFB, (u8)(chan >> 8), (u8)chan };

                    d[j + 1] = (u8)io;
                    d.insert(d.end(), std::begin(code), std::end(code));
                    d[i + 1] = (u8)(stub >> 8);
                    d[i + 2] = (u8)stub;
                    return true;
                }
            }
        }
    }
    return false;
}

std::string FanfarePath(s32 index) {
    return std::string("mergeship/audio/sequences/form_fanfare_") + std::to_string(index);
}

// Builds (or rebuilds, if the resource cache let it go) the virtual sequence for id slot `index`.
bool EnsureFanfare(s32 index, const MMFanfare& fanfare, s32 io) {
    std::string path = FanfarePath(index);
    auto resMgr = Ship::Context::GetRawInstance()->GetResourceManager();

    if (sFanfareBuilt[index] && (resMgr->GetCachedResource(path) != nullptr)) {
        return true;
    }

    u8 mmFont0 = MergeShip_MMAudio_MapFont(0);
    if (mmFont0 == 0xFF) {
        return false;
    }

    SequenceData* src = ResourceMgr_LoadSeqPtrByName(fanfare.path);
    if ((src == nullptr) || (src->seqData == nullptr) || (src->seqDataSize == 0) || (src->numFonts < 1)) {
        return false;
    }

    std::vector<u8> data((u8*)src->seqData, (u8*)src->seqData + src->seqDataSize);
    if (((fanfare.kind == kFanfareOoTSolo) && !RewriteOoTFanfare(data, io)) ||
        ((fanfare.kind == kFanfareOoTWarp) && !RewriteOoTWarpFanfare(data, io))) {
        SPDLOG_ERROR("MergeShip: could not rewrite {} for a form's instrument", fanfare.path);
        return false;
    }

    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Path = path;
    initData->IsCustom = true;
    auto seq = std::make_shared<SOH::AudioSequence>(initData);
    seq->sequence.seqData = new char[data.size()];
    memcpy(seq->sequence.seqData, data.data(), data.size());
    seq->sequence.seqDataSize = (uint32_t)data.size();
    seq->sequence.seqNumber = (uint16_t)sFanfareIds[index];
    seq->sequence.medium = src->medium;
    seq->sequence.cachePolicy = src->cachePolicy;
    memset(seq->sequence.fonts, 0, sizeof(seq->sequence.fonts));
    seq->sequence.fonts[0] = mmFont0;
    seq->sequence.numFonts = 1;
    if (fanfare.kind == kFanfareOoTWarp) {
        // The last font is the sequence's default: the warp-song font stays the orchestra's.
        seq->sequence.fonts[1] = src->fonts[src->numFonts - 1];
        seq->sequence.numFonts = 2;
    }
    resMgr->CacheExternalResource(path, seq);

    if (sequenceMap[sFanfareIds[index]] == nullptr) {
        sequenceMap[sFanfareIds[index]] = _strdup(path.c_str());
    }
    sFanfareBuilt[index] = true;
    return true;
}

} // namespace

// AudioLoad_Init, after Ocarina of Time's and the custom sequences were numbered: reserve the fanfares' ids.
extern "C" void MergeShip_MMAudio_ReserveFanfareIds(void) {
    s32 found = 0;
    size_t slots = sequenceMapSize + 0xF;

    if (sFanfareIdsReserved || (sequenceMap == nullptr)) {
        return;
    }

    // Free 8-bit ids above Ocarina of Time's own (0x6D is its last), clear of the audio editor's entries and of
    // NA_BGM_DISABLED.
    for (s32 id = 0x6E; (id < 0xFF) && (found < kNumFanfareIds); id++) {
        if (((size_t)id < slots) && (sequenceMap[id] != nullptr)) {
            continue;
        }
        // (129-135 are the audio editor's ocarina instruments, which custom sequences skip too.)
        if (((id >= 129) && (id <= 135)) ||
            ((AudioCollection::Instance != nullptr) && AudioCollection::Instance->HasSequenceNum((uint16_t)id))) {
            continue;
        }
        sFanfareIds[found++] = id;
    }
    if (found < kNumFanfareIds) {
        SPDLOG_ERROR("MergeShip: no free sequence ids for the forms' song fanfares");
        return;
    }

    // The loaders check `id < sequenceMapSize + 0xF` and `id <= sequenceMapSize`.
    size_t newSize = std::max(sequenceMapSize, (size_t)sFanfareIds[kNumFanfareIds - 1] + 1);
    if (newSize != sequenceMapSize) {
        size_t newSlots = newSize + 0xF;
        char** newMap = (char**)realloc(sequenceMap, newSlots * sizeof(char*));
        u8* newStatus = (u8*)realloc(gAudioContext.seqLoadStatus, newSlots);

        if (newMap != nullptr) {
            sequenceMap = newMap;
        }
        if (newStatus != nullptr) {
            gAudioContext.seqLoadStatus = newStatus;
        }
        if ((newMap == nullptr) || (newStatus == nullptr)) {
            SPDLOG_ERROR("MergeShip: out of memory reserving the forms' song fanfares");
            return;
        }
        for (size_t i = slots; i < newSlots; i++) {
            sequenceMap[i] = nullptr;
            gAudioContext.seqLoadStatus[i] = 5;
        }
        sequenceMapSize = newSize;
    }
    sFanfareIdsReserved = true;
    SPDLOG_INFO("MergeShip: the forms' song fanfares use sequence ids {}..{}", sFanfareIds[0],
                sFanfareIds[kNumFanfareIds - 1]);
}

// MM Audio_PlayFanfareWithPlayerIOPort7(sOcarinaSongFanfares[song], sOcarinaSongFanfareIoData[form]) for a
// transformed player. Returns false (Ocarina of Time's fanfare is then played) for the human forms.
extern "C" s32 MergeShip_PlayFormSongFanfare(u8 song) {
    Player* player = (gPlayState != nullptr) ? GET_PLAYER(gPlayState) : nullptr;
    s32 io;
    s32 index = 0;
    s32 i;

    if ((player == nullptr) || !sFanfareIdsReserved || ((io = FormIoData(player->mergeShipForm)) < 0)) {
        return false;
    }

    MergeShip_MMAudio_RegisterFonts();

    for (i = 0; i < (s32)ARRAY_COUNT(kFanfares); i++) {
        if (kFanfares[i].song == song) {
            break;
        }
        index += (kFanfares[i].kind == kFanfareMM) ? 1 : kNumForms;
    }
    if (i == (s32)ARRAY_COUNT(kFanfares)) {
        return false;
    }
    if (kFanfares[i].kind != kFanfareMM) {
        index += FormSlot(player->mergeShipForm);
    }
    if (!EnsureFanfare(index, kFanfares[i], io)) {
        return false;
    }

    Audio_PlayFanfare((u16)sFanfareIds[index]);
    // SEQCMD_SET_SEQPLAYER_IO(SEQ_PLAYER_FANFARE, 7, ioData)
    Audio_QueueSeqCmd(0x70000000 | (SEQ_PLAYER_FANFARE << 24) | (7 << 16) | (u8)io);
    MergeShipTest_Log("FANFARE form=%d song=%d seq=0x%02X %s io7=0x%02X font=%d", player->mergeShipForm, song,
                      sFanfareIds[index], kFanfares[i].path,
                      io, MergeShip_MMAudio_MapFont(0));
    return true;
}

// Test evidence: the instrument each of the fanfare player's channels plays with (font id and the instrument's
// index in that font), logged when it changes. MergeShipTest_Log is a no-op outside the harness.
extern "C" void MergeShip_MMAudio_LogFanfareInstruments(void) {
    static s32 sLast[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    SequencePlayer* seqPlayer = &gAudioContext.seqPlayers[SEQ_PLAYER_FANFARE];
    s32 ch;

    for (ch = 0; ch < 16; ch++) {
        SequenceChannel* channel = seqPlayer->channels[ch];
        s32 value = -1;

        if (seqPlayer->enabled && (channel != nullptr) && channel->enabled && (channel->instrument != nullptr) &&
            (channel->fontId < fontMapSize) && (fontMap[channel->fontId] != nullptr)) {
            SoundFont* sf = ResourceMgr_LoadAudioSoundFontByName(fontMap[channel->fontId]);
            s32 inst;

            for (inst = 0; (sf != nullptr) && (inst < sf->numInstruments); inst++) {
                if (sf->instruments[inst] == channel->instrument) {
                    value = (channel->fontId << 8) | inst;
                    break;
                }
            }
        }
        if (value != sLast[ch]) {
            sLast[ch] = value;
            if (value >= 0) {
                MergeShipTest_Log("FANFARE seq=0x%02X ch%d font=%d (%s) instrument=0x%02X", seqPlayer->seqId, ch,
                                  value >> 8, fontMap[value >> 8], value & 0xFF);
            }
        }
    }
}

// MM Audio_PlayFanfare for the Bremen Mask's march (0), Kamaro's dance (1) and getting a mask (2): MM's sequence
// with MM's sound fonts, under a reserved id of Ocarina of Time's fanfare player.
// MergeShip2: builds (or rebuilds) id slot `index` as MM's sequence `mmPath` as it is, its MM sound fonts mapped to
// where MM's fonts are registered.
static bool EnsureMMSequence(s32 index, const char* mmPath) {
    auto resMgr = Ship::Context::GetRawInstance()->GetResourceManager();
    std::string path = FanfarePath(index);

    if (!sFanfareBuilt[index] || (resMgr->GetCachedResource(path) == nullptr)) {
        SequenceData* src = ResourceMgr_LoadSeqPtrByName(mmPath);
        if ((src == nullptr) || (src->seqData == nullptr) || (src->seqDataSize == 0) || (src->numFonts < 1)) {
            return false;
        }

        auto initData = std::make_shared<Ship::ResourceInitData>();
        initData->Path = path;
        initData->IsCustom = true;
        auto seq = std::make_shared<SOH::AudioSequence>(initData);
        seq->sequence.seqData = new char[src->seqDataSize];
        memcpy(seq->sequence.seqData, src->seqData, src->seqDataSize);
        seq->sequence.seqDataSize = src->seqDataSize;
        seq->sequence.seqNumber = (uint16_t)sFanfareIds[index];
        seq->sequence.medium = src->medium;
        seq->sequence.cachePolicy = src->cachePolicy;
        memset(seq->sequence.fonts, 0, sizeof(seq->sequence.fonts));
        // MM's font ids are MM's; each maps to where MM's fonts are registered.
        for (s32 f = 0; f < src->numFonts; f++) {
            u8 font = MergeShip_MMAudio_EnsureFont(src->fonts[f]);
            if (font == 0xFF) {
                MergeShipTest_Log("MMSEQ %s: MM font %d unavailable", mmPath, (int)src->fonts[f]);
                return false;
            }
            seq->sequence.fonts[f] = font;
        }
        seq->sequence.numFonts = src->numFonts;
        resMgr->CacheExternalResource(path, seq);
        if (sequenceMap[sFanfareIds[index]] == nullptr) {
            sequenceMap[sFanfareIds[index]] = _strdup(path.c_str());
        }
        sFanfareBuilt[index] = true;
    }
    return true;
}

extern "C" s32 MergeShip_MMAudio_PlayMMFanfare(s32 which) {
    s32 index = kFirstMaskFanfareId + which;

    if (!sFanfareIdsReserved || (which < 0) || (which >= kNumMaskFanfares)) {
        return false;
    }
    MergeShip_MMAudio_RegisterFonts();
    if (!EnsureMMSequence(index, kMaskFanfares[which])) {
        return false;
    }

    Audio_PlayFanfare((u16)sFanfareIds[index]);
    MergeShipTest_Log("MASKFANFARE %d seq=0x%02X %s", which, sFanfareIds[index], kMaskFanfares[which]);
    return true;
}

// MergeShip2 (singleplayer): the Happy Mask Salesman's theme on the main BGM player while he talks; the sequence that
// was playing comes back after.
static u16 sHMSPrevBgm = NA_BGM_DISABLED;
static bool sHMSPlaying = false;

extern "C" s32 MergeShip_MMAudio_PlayHMSTheme(void) {
    s32 index = kFirstMMBgmId;

    if (sHMSPlaying) {
        return true;
    }
    if (!sFanfareIdsReserved) {
        return false;
    }
    MergeShip_MMAudio_RegisterFonts();
    if (!EnsureMMSequence(index, kHMSTheme)) {
        return false;
    }
    sHMSPrevBgm = func_800FA0B4(SEQ_PLAYER_BGM_MAIN);
    // SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0, seqId)
    Audio_QueueSeqCmd((SEQ_PLAYER_BGM_MAIN << 24) | (u8)sFanfareIds[index]);
    sHMSPlaying = true;
    MergeShipTest_Log("HMSTHEME play seq=0x%02X (was 0x%04X)", sFanfareIds[index], sHMSPrevBgm);
    return true;
}

extern "C" void MergeShip_MMAudio_StopHMSTheme(void) {
    if (!sHMSPlaying) {
        return;
    }
    sHMSPlaying = false;
    if ((sHMSPrevBgm != NA_BGM_DISABLED) && ((sHMSPrevBgm & 0xFF) != 0xFF)) {
        Audio_QueueSeqCmd((SEQ_PLAYER_BGM_MAIN << 24) | (10 << 16) | (sHMSPrevBgm & 0xFFFF));
    } else {
        // SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 10)
        Audio_QueueSeqCmd(0x10000000 | (SEQ_PLAYER_BGM_MAIN << 24) | (10 << 16));
    }
    MergeShipTest_Log("HMSTHEME stop (back to 0x%04X)", sHMSPrevBgm);
}

// MergeShip2: MM Audio_PlayFanfareWithPlayerIOPort7(sOcarinaSongFanfares[song], sOcarinaSongFanfareIoData[CUR_FORM])
// for one of MM's own songs (`mmSong` = song - MERGESHIP_SONG_FIRST): MM's ocarina fanfare, told the instrument
// through io port 7 -- the Deku's pipes, the Goron's drums, the Zora's guitar, or MM's ocarina (0x35) for a human.
extern "C" s32 MergeShip_MMAudio_PlayMMSongFanfare(s32 mmSong) {
    Player* player = (gPlayState != nullptr) ? GET_PLAYER(gPlayState) : nullptr;
    s32 index = kFirstMMSongFanfareId + mmSong;
    s32 io;

    if (!sFanfareIdsReserved || (mmSong < 0) || (mmSong >= kNumMMSongFanfares)) {
        return false;
    }
    io = (player != nullptr) ? FormIoData(player->mergeShipForm) : -1;
    if (io < 0) {
        io = 0x35; // MM sOcarinaSongFanfareIoData[PLAYER_FORM_HUMAN]
    }
    MergeShip_MMAudio_RegisterFonts();
    if (!EnsureMMSequence(index, kMMSongFanfares[mmSong])) {
        return false;
    }

    Audio_PlayFanfare((u16)sFanfareIds[index]);
    // SEQCMD_SET_SEQPLAYER_IO(SEQ_PLAYER_FANFARE, 7, ioData)
    Audio_QueueSeqCmd(0x70000000 | (SEQ_PLAYER_FANFARE << 24) | (7 << 16) | (u8)io);
    MergeShipTest_Log("MMSONGFANFARE %d seq=0x%02X %s io7=0x%02X", mmSong, sFanfareIds[index], kMMSongFanfares[mmSong],
                      io);
    return true;
}
