// MergeShip2 -- Ocarina of Time's adult Link in Majora's Mask (MergeShipAdultLink.h): his animations, the per-frame
// rebuild when his age changes, the menu, and the harness commands.

#include "MergeShipAdultLink.h"
#include "MergeShipAssets.h"
#include "MergeShipOoTEquip.h" // MergeShip2: the Hylian Shield hunch (BUG-0022)

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <filesystem>

#include <dr_wav.h>
#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/O2rArchive.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/MenuTypes.h"
#include "BenJsonConversions.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
int ResourceMgr_OTRSigCheck(char* imgData);
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char* path);
SoundFont* ResourceMgr_LoadAudioSoundFontByName(const char* path);
extern char** gFontMap;  // src/audio/lib/load.c: each sound font's resource path
extern size_t gFontMapSize;
extern char** gSequenceMap; // src/audio/lib/load.c: each sequence's resource path
extern size_t gSequenceMapSize;
SequenceData* ResourceMgr_LoadSeqPtrByName(const char* path); // BenPort.cpp
void MergeShip_Hyrule_SetArrivalAge(u8 ootAge); // MergeShipHyrule.h
}

// ---- animations -----------------------------------------------------------------------------------------------------------
//
// Ocarina of Time's adult plays Ocarina of Time's player animations; MM's human plays MM's, which are (mostly) the same
// animations under the same names, reworked for MM's young Link. For the adult, a frame MM's player code asks for from
// objects/gameplay_keep/gPlayerAnim_<name> is read from Ocarina of Time's oot/objects/gameplay_keep/gPlayerAnim_<name>
// when Ocarina of Time has that animation with the same number of frames (MM's code keeps timing, frame checks and
// lengths from its own header, so a different length would desynchronise them). Animations only MM has (its new moves,
// the masks, the forms') stay MM's.

extern "C" void MergeShip_AdultLink_LogState(const char* tag);

namespace {

struct AnimRemap {
    PlayerAnimationHeader* oot = nullptr; // Ocarina of Time's, or null: play MM's
};

std::unordered_map<std::string, AnimRemap> sAnimRemaps;
u32 sRemappedFrames = 0;
u32 sKeptFrames = 0;
bool sTrace = false;

const AnimRemap& LookUp(const char* mmPath) {
    auto found = sAnimRemaps.find(mmPath);
    if (found != sAnimRemaps.end()) {
        return found->second;
    }
    AnimRemap remap;
    std::string rel = mmPath;
    if (rel.rfind("__OTR__", 0) == 0) {
        rel = rel.substr(7);
    }
    if (rel.rfind("mm/", 0) == 0) {
        rel = rel.substr(3);
    }
    if (rel.rfind("alt/", 0) == 0) {
        rel = rel.substr(4);
    }
    std::string ootPath = "oot/" + rel;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    if ((rel.find("gPlayerAnim_") != std::string::npos) && archives->HasFile(ootPath)) {
        auto mm = (PlayerAnimationHeader*)ResourceMgr_LoadAnimByName(mmPath);
        auto oot = (PlayerAnimationHeader*)ResourceMgr_LoadAnimByName(("__OTR__" + ootPath).c_str());
        if ((mm != nullptr) && (oot != nullptr) && (oot->segmentVoid != nullptr) &&
            (mm->common.frameCount == oot->common.frameCount)) {
            remap.oot = oot;
        }
        MergeShipTest_Log("ADULT anim %s: Ocarina of Time's %s (frames mm %d oot %d)", rel.c_str(),
                          remap.oot ? "used" : "NOT used", mm ? mm->common.frameCount : -1,
                          oot ? oot->common.frameCount : -1);
    }
    return sAnimRemaps.emplace(mmPath, remap).first->second;
}

// A frame table inside the player's own struct (the joint, morph, blend and upper-body tables).
bool IsPlayerTable(Player* player, void* table) {
    return ((u8*)table >= (u8*)player) && ((u8*)table < (u8*)player + sizeof(Player));
}

// BUG-0022: young Link under Hyrule's Hylian Shield plays Ocarina of Time's young Link's guard,
// gPlayerAnim_clink_normal_defense_ALL, through MM's own guard animations (MM keeps their timing): its start and its
// hold keep the OoT animation's last frame, its end runs it backward -- as OoT's guard start (Player_ActionHandler_11)
// sets the animation straight to its last frame (LinkAnimation_Change(..., last, last, ANIMMODE_ONCE, 0.0f): no morph),
// holds it, and Player_Action_80843188 plays it back from its last frame when the guard ends. BUG-0137: the start ran
// the OoT animation forward over MM's start, which lasts one frame -- a frame of OoT's first (upright) pose before the
// hunch, "an inbetween frame where he holds it normally". OoT's hunched child has no shield-hit reaction either
// (Player_Action_808435C4 is skipped for Player_IsChildWithHylianShield): MM's hit animations hold the hunch too.
enum class HunchPart { None, Start, Hold, End };

HunchPart HunchPartOf(const char* mmPath) {
    const char* name = strrchr(mmPath, '/');
    name = (name != nullptr) ? name + 1 : mmPath;
    if ((strcmp(name, "gPlayerAnim_link_normal_defense") == 0) ||
        (strcmp(name, "gPlayerAnim_link_normal_defense_free") == 0)) {
        return HunchPart::Start;
    }
    if ((strcmp(name, "gPlayerAnim_link_normal_defense_wait") == 0) ||
        (strcmp(name, "gPlayerAnim_link_normal_defense_wait_free") == 0) ||
        (strcmp(name, "gPlayerAnim_link_normal_defense_hit") == 0) ||
        (strcmp(name, "gPlayerAnim_link_anchor_defense_hit") == 0) ||
        (strcmp(name, "gPlayerAnim_link_anchor_defense_long_hit") == 0) ||
        (strcmp(name, "gPlayerAnim_link_fighter_defense_long_hit") == 0)) {
        return HunchPart::Hold;
    }
    if ((strcmp(name, "gPlayerAnim_link_normal_defense_end") == 0) ||
        (strcmp(name, "gPlayerAnim_link_normal_defense_end_free") == 0)) {
        return HunchPart::End;
    }
    return HunchPart::None;
}

void RegisterHylianHunch() {
    COND_VB_SHOULD(VB_LOAD_PLAYER_ANIMATION_FRAME, true, {
        AnimTask* task = va_arg(args, AnimTask*);
        auto animation = va_arg(args, PlayerAnimationHeader*);
        s32 frame = va_arg(args, s32);
        s32 limbCount = va_arg(args, s32);
        Vec3s* frameTable = va_arg(args, Vec3s*);

        if (!*should || (task == nullptr) || (gPlayState == nullptr) ||
            !CVarGetInteger(MERGESHIP_HYLIAN_HUNCH_CVAR, 1) || (ResourceMgr_OTRSigCheck((char*)animation) == 0)) {
            return;
        }
        Player* player = GET_PLAYER(gPlayState);
        if ((player == nullptr) || (player->transformation != PLAYER_FORM_HUMAN) ||
            MergeShip_AdultLink_IsActive(player) || !IsPlayerTable(player, frameTable) ||
            (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.shield != MS_OOTEQ_SHIELD_HYLIAN) ||
            !MergeShip_OoTEquip_Enabled()) {
            return;
        }
        HunchPart part = HunchPartOf((const char*)animation);
        if (part == HunchPart::None) {
            return;
        }
        static PlayerAnimationHeader* sOoT = nullptr;
        if (sOoT == nullptr) {
            sOoT = (PlayerAnimationHeader*)ResourceMgr_LoadAnimByName(
                "__OTR__oot/objects/gameplay_keep/gPlayerAnim_clink_normal_defense_ALL");
            if ((sOoT == nullptr) || (sOoT->segmentVoid == nullptr)) {
                sOoT = nullptr;
                return;
            }
        }
        auto mm = (PlayerAnimationHeader*)ResourceMgr_LoadAnimByName((const char*)animation);
        s32 mmLast = ((mm != nullptr) && (mm->common.frameCount > 1)) ? (mm->common.frameCount - 1) : 1;
        s32 ootLast = (sOoT->common.frameCount > 1) ? (sOoT->common.frameCount - 1) : 0;
        s32 f = CLAMP(frame, 0, mmLast);
        s32 ootFrame = (part == HunchPart::End) ? (ootLast - (f * ootLast) / mmLast) : ootLast; // (BUG-0137)
        size_t frameSize = sizeof(Vec3s) * limbCount + sizeof(s16);
        memcpy(frameTable, (u8*)sOoT->segmentVoid + frameSize * ootFrame, frameSize);
        MergeShip_OoTEquip_NoteHylianHunch();
        *should = false;
    });
}

void RegisterAnimations() {
    RegisterHylianHunch();
    COND_VB_SHOULD(VB_LOAD_PLAYER_ANIMATION_FRAME, MergeShip_AdultLink_Enabled(), {
        AnimTask* task = va_arg(args, AnimTask*);
        auto animation = va_arg(args, PlayerAnimationHeader*);
        s32 frame = va_arg(args, s32);
        s32 limbCount = va_arg(args, s32);
        Vec3s* frameTable = va_arg(args, Vec3s*);

        if (!*should || (task == nullptr) || (gPlayState == nullptr) || !MergeShip_AdultLink_TablesAdult() ||
            (ResourceMgr_OTRSigCheck((char*)animation) == 0)) {
            return;
        }
        Player* player = GET_PLAYER(gPlayState);
        if ((player == nullptr) || !MergeShip_AdultLink_IsActive(player) || !IsPlayerTable(player, frameTable)) {
            return;
        }
        const AnimRemap& remap = LookUp((const char*)animation);
        if (remap.oot == nullptr) {
            sKeptFrames++;
            return;
        }
        s32 frameCount = remap.oot->common.frameCount;
        if (frame < 0) {
            frame = 0;
        } else if ((frameCount > 0) && (frame >= frameCount)) {
            frame = frameCount - 1;
        }
        size_t frameSize = sizeof(Vec3s) * limbCount + sizeof(s16);
        memcpy(frameTable, (u8*)remap.oot->segmentVoid + frameSize * frame, frameSize);
        sRemappedFrames++;
        *should = false;
    });
}

// ---- the transformation scream ----------------------------------------------------------------------------------------
//
// Hyrule's "Adult Link Transformation Scream" (Shipwright MergeShipSohFdAudio.cpp), in Termina. Majora's Mask's
// transformation voice (NA_SE_PL_TRANSFORM_VOICE) is one layer of three drums of sound font 0 -- young Link's two gasps
// and his scream (drums 4, 5 and 6) at ticks 0, 54 and 124 of the sfx player's 96 a second. Adult Link's recording
// (Hyrule's mergeship2.o2r, AdultTransformScream.wav, read from that archive file: Termina doesn't mount it) is cut at
// the same times, and while the adult transforms those three drums are his parts -- 16-bit samples at the recording's
// rate, with the drums' own envelopes, decay and pan -- until all three notes have started.

constexpr const char* kScreamArchive = "mergeship2.o2r";
constexpr const char* kScreamWav = "mergeship2/audio/AdultTransformScream.wav"; // Shipwright MergeShipSohFdAssets.h
constexpr float kScreamSynthRate = 32000.0f; // a drum tuning of 1.0 plays a 32 kHz sample as it is
constexpr int kScreamFirstDrum = 4;
constexpr int kScreamParts = 3;
constexpr float kScreamTicksPerSecond = 96.0f; // the sfx sequence's tempo, 120 (48 ticks a beat)
constexpr float kScreamPartTicks[kScreamParts] = { 0.0f, 54.0f, 124.0f };
constexpr int kScreamSwapFrames = 60; // past the third note (tick 124, ~1.3 s)

struct ScreamPart {
    std::vector<int16_t> pcm; // mono, the recording's own rate
    Sample sample{};
    AdpcmLoop loop{};
    AdpcmBook book{};
    Drum drum{};
};

ScreamPart sScreamParts[kScreamParts];
bool sScreamLoaded = false;
bool sScreamLoadTried = false;
SoundFont* sScreamFont = nullptr;
Drum* sScreamSavedDrums[kScreamParts];
int sScreamFramesLeft = 0;

bool LoadAdultScream() {
    if (sScreamLoadTried) {
        return sScreamLoaded;
    }
    sScreamLoadTried = true;

    const std::string path = Ship::Context::LocateFileAcrossAppDirs(kScreamArchive, "2ship");
    if (!std::filesystem::exists(path)) {
        SPDLOG_WARN("MergeShip: {} not found; the adult's transformation scream is Majora's Mask's", kScreamArchive);
        return false;
    }
    auto archive = std::make_shared<Ship::O2rArchive>(path);
    archive->Load();
    std::shared_ptr<Ship::File> file = archive->IsLoaded() ? archive->LoadFile(std::string(kScreamWav)) : nullptr;
    if ((file == nullptr) || (file->Buffer == nullptr) || (file->Buffer->size() <= file->BufferOffset)) {
        SPDLOG_ERROR("MergeShip: could not load {} from {}", kScreamWav, path);
        return false;
    }
    drwav wav;
    if (!drwav_init_memory(&wav, file->Buffer->data() + file->BufferOffset, file->Buffer->size() - file->BufferOffset,
                           nullptr)) {
        SPDLOG_ERROR("MergeShip: {} is not a WAV file", kScreamWav);
        return false;
    }
    drwav_uint64 numFrames = 0;
    drwav_get_length_in_pcm_frames(&wav, &numFrames);
    const uint32_t channels = (wav.channels != 0) ? wav.channels : 1;
    const uint32_t rate = (wav.sampleRate != 0) ? wav.sampleRate : (uint32_t)kScreamSynthRate;
    std::vector<int16_t> interleaved((size_t)numFrames * channels);
    drwav_read_pcm_frames_s16(&wav, numFrames, interleaved.data());
    drwav_uninit(&wav);

    for (int part = 0; part < kScreamParts; part++) {
        const size_t start = (size_t)(kScreamPartTicks[part] / kScreamTicksPerSecond * rate);
        const size_t end = (part + 1 < kScreamParts)
                               ? (size_t)(kScreamPartTicks[part + 1] / kScreamTicksPerSecond * rate)
                               : (size_t)numFrames;
        ScreamPart& p = sScreamParts[part];

        if ((start >= end) || (end > (size_t)numFrames)) {
            SPDLOG_ERROR("MergeShip: {} is too short for the transformation voice", kScreamWav);
            return false;
        }
        p.pcm.resize(end - start);
        for (size_t i = start; i < end; i++) {
            int32_t sum = 0;
            for (uint32_t c = 0; c < channels; c++) {
                sum += interleaved[i * channels + c];
            }
            p.pcm[i - start] = (int16_t)(sum / (int32_t)channels);
        }
        p.loop.start = 0;
        p.loop.loopEnd = (u32)p.pcm.size();
        p.loop.count = 0;
        p.loop.sampleEnd = (u32)p.pcm.size();
        p.book.order = 0;
        p.book.numPredictors = 0;
        p.book.codeBook = nullptr;
        p.sample.codec = CODEC_S16;
        p.sample.medium = MEDIUM_RAM;
        p.sample.unk_bit26 = 0;
        p.sample.isRelocated = 1;
        p.sample.size = (u32)(p.pcm.size() * sizeof(int16_t));
        p.sample.fileSize = p.sample.size;
        p.sample.sampleAddr = (u8*)p.pcm.data();
        p.sample.loop = &p.loop;
        p.sample.book = &p.book;
        p.drum.tunedSample.sample = &p.sample;
        p.drum.tunedSample.tuning = (f32)rate / kScreamSynthRate;
    }
    sScreamLoaded = true;
    return true;
}

// Puts Majora's Mask's own voice drums back.
void RestoreScreamDrums() {
    if (sScreamFont != nullptr) {
        for (int part = 0; part < kScreamParts; part++) {
            sScreamFont->drums[kScreamFirstDrum + part] = sScreamSavedDrums[part];
        }
        sScreamFont = nullptr;
    }
    sScreamFramesLeft = 0;
}

bool SwapInAdultScream() {
    if (!LoadAdultScream() || (gFontMap == nullptr) || (gFontMapSize == 0) || (gFontMap[0] == nullptr)) {
        return false;
    }
    // gFontMap[0]: the path AudioPlayback_GetDrum reads font 0's drums from, so it is the very font it plays.
    SoundFont* font = ResourceMgr_LoadAudioSoundFontByName(gFontMap[0]);
    if ((font == nullptr) || (font->drums == nullptr) || (font->numDrums < kScreamFirstDrum + kScreamParts)) {
        return false;
    }
    if (sScreamFont == nullptr) {
        for (int part = 0; part < kScreamParts; part++) {
            Drum* original = font->drums[kScreamFirstDrum + part];

            if (original == nullptr) {
                return false;
            }
            sScreamSavedDrums[part] = original;
            // The drum's own envelope, decay and pan; only the sound is Adult Link's.
            sScreamParts[part].drum.adsrDecayIndex = original->adsrDecayIndex;
            sScreamParts[part].drum.pan = original->pan;
            sScreamParts[part].drum.isRelocated = original->isRelocated;
            sScreamParts[part].drum.envelope = original->envelope;
        }
        // Each pointer is one aligned store: the audio thread reads either drum whole. The parts never move or free,
        // so a note already playing one keeps it after the drums are put back.
        for (int part = 0; part < kScreamParts; part++) {
            font->drums[kScreamFirstDrum + part] = &sScreamParts[part].drum;
        }
        sScreamFont = font;
    }
    sScreamFramesLeft = kScreamSwapFrames;
    MergeShipTest_Log("ADULT transformation voice: Adult Link's scream in font 0 drums %d-%d", kScreamFirstDrum,
                      kScreamFirstDrum + kScreamParts - 1);
    return true;
}

void UpdateAdultScream() {
    if ((sScreamFont != nullptr) && (--sScreamFramesLeft <= 0)) {
        RestoreScreamDrums();
    }
}

// ---- the hookshot's "hyah!" --------------------------------------------------------------------------------------------
//
// BUG-0020: the hookshot pulls Link with NA_SE_VO_LI_LASH (MM Player_UpdateUpperBody, as OoT's). The adult's voice bank 0
// is the Fierce Deity's, whose entry for it (sequence 0's voice table, entry 2) is MM's empty stub: the Fierce Deity never
// hookshots, and MM's font 0 has no sample for it. Ocarina of Time's adult says it with font 0's sound effects 0x15/0x16
// ("Adult Link - Spur Horse 1/2"; its sequence 0 voice table, entry 2). So the adult plays young Link's entry (0x22: the
// voice channel's script picks one of its list 0x0A / 0x0B at random, in young Link's part of font 0, +0x80: sound effects
// 0x8A / 0x8B) while those two are Ocarina of Time's adult samples -- its font 0, which MergeShipHyrule.cpp already loads
// for Hyrule's music -- until the note has started, as the scream does.

constexpr const char* kOoTSfxFont = "oot/audio/fonts/00_Sound_Effects_1";
constexpr u16 kOoTLashSfx[2] = { 0x15, 0x16 };
constexpr u16 kMMLashSfx[2] = { 0x8A, 0x8B };
constexpr int kLashSwapFrames = 45;

SoundFont* sLashFont = nullptr;
SoundEffect sLashSaved[2];
int sLashFramesLeft = 0;

void RestoreLash() {
    if (sLashFont != nullptr) {
        for (int i = 0; i < 2; i++) {
            sLashFont->soundEffects[kMMLashSfx[i]] = sLashSaved[i];
        }
        sLashFont = nullptr;
    }
    sLashFramesLeft = 0;
}

bool SwapInAdultLash() {
    if ((gFontMap == nullptr) || (gFontMapSize == 0) || (gFontMap[0] == nullptr)) {
        return false;
    }
    SoundFont* mm = ResourceMgr_LoadAudioSoundFontByName(gFontMap[0]);
    SoundFont* oot = ResourceMgr_LoadAudioSoundFontByName(kOoTSfxFont);
    if ((mm == nullptr) || (oot == nullptr) || (mm->soundEffects == nullptr) || (oot->soundEffects == nullptr) ||
        (mm->numSfx <= kMMLashSfx[1]) || (oot->numSfx <= kOoTLashSfx[1])) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (oot->soundEffects[kOoTLashSfx[i]].tunedSample.sample == nullptr) {
            return false;
        }
    }
    if (sLashFont == nullptr) {
        for (int i = 0; i < 2; i++) {
            sLashSaved[i] = mm->soundEffects[kMMLashSfx[i]];
            mm->soundEffects[kMMLashSfx[i]] = oot->soundEffects[kOoTLashSfx[i]];
        }
        sLashFont = mm;
    }
    sLashFramesLeft = kLashSwapFrames;
    MergeShipTest_Log("ADULT hookshot voice: Ocarina of Time's font 0 sfx 0x%02X/0x%02X in font 0 sfx 0x%02X/0x%02X",
                      kOoTLashSfx[0], kOoTLashSfx[1], kMMLashSfx[0], kMMLashSfx[1]);
    return true;
}

void UpdateAdultLash() {
    if ((sLashFont != nullptr) && (--sLashFramesLeft <= 0)) {
        RestoreLash();
    }
}

// ---- the Iron Boots' clank ---------------------------------------------------------------------------------------------
//
// BUG-0091: Ocarina of Time's Iron Boots steps, jumps and landings play sound font 0's instrument 0x18, "Iron Boots 2"
// (oot.o2r audio/fonts/00_Sound_Effects_1); Majora's Mask kept the sound ids but made that instrument a dirt step
// (mm.o2r Soundfont_0, SAMPLE_0_127/128). While Link wears the Iron Boots (Hyrule's, MergeShipOoTEquip.h), the
// instrument is Ocarina of Time's.

constexpr u8 kIronBootsInstrument = 0x18;
SoundFont* sIronBootsFont = nullptr;
Instrument* sIronBootsSaved = nullptr;
// The Hover Boots' hum (NA_SE_PL_HOBBERBOOTS_LV, 0x8C9): MM's sequence keeps Ocarina of Time's script for it, but on
// MM's font 0 instrument 0x44. While the boots are on, that instrument is Ocarina of Time's.
constexpr u8 kHoverBootsInstrument = 0x44;
SoundFont* sHoverBootsFont = nullptr;
Instrument* sHoverBootsSaved = nullptr;

// MM's sound effect sequence sends the Iron Boots' ids (player bank 0x0D walk, 0x1D jump, 0x2D land: NA_CODE_DIRT_DEEP
// and the deep-dirt jump / landing there) to its surface step scripts, which never play instrument 0x18. Ocarina of
// Time's own scripts for them (oot.o2r 000_Sound_Effects, player channel table entries 0x0D / 0x1D / 0x2D) are tiny:
// instrument 0x18 and one or two notes. They are appended to a copy of the sequence the sfx player reads (its
// seqData: every dyncall target resolves against it) and, while the boots are on, the three table entries point at
// them (in the original buffer, which the channels' dynTable points into, and in the copy).
constexpr u8 kBootsSfx[3] = { 0x0D, 0x1D, 0x2D };
// Per script: the channel part (instrument 0x18, layer 0 at +6, end) is filled in with its offset.
const u8 kBootsLayers[3][8] = {
    { 0x5F, 0x28, 0x60, 0xFF },                   // walk: pitch 0x1F
    { 0x5E, 0x0A, 0x60, 0x64, 0x18, 0x60, 0xFF }, // jump: pitch 0x1E, then 0x24
    { 0x66, 0x0A, 0x60, 0x60, 0x18, 0x60, 0xFF }, // land: pitch 0x26, then 0x20
};
const u8 kBootsLayerLength[3] = { 4, 7, 7 };
u8* sBootsSeqOriginal = nullptr; // the sequence buffer the copy was made from
u8* sBootsSeqCopy = nullptr;
u16 sBootsTable = 0;             // the player channel's dyntable, in both
u16 sBootsScript[3];             // the appended scripts' offsets
u16 sBootsSaved[3];              // the table's own entries
bool sBootsTablePatched = false;

// The player channel's dyntable for sound ids 0x00-0x7F (channel 0: the header's ldchan 0x90). MM's channel picks one
// of four tables by io port 5 (AudioSfx: the id's bits 7-9) -- "and 0x7F; ldseqtoptr <table of tables>" -- and writes it
// into its own dyntable command; the first of them is the one for these ids. 0 if this is not that sequence.
u16 FindPlayerDynTable(const u8* seq, s32 length) {
    for (s32 i = 0; (i + 2 < length) && (i < 0x200); i++) {
        if (seq[i] == 0x90) {
            const s32 channel = (seq[i + 1] << 8) | seq[i + 2];
            for (s32 j = channel; (j + 4 < length) && (j < channel + 0x80); j++) {
                if ((seq[j] == 0xC9) && (seq[j + 1] == 0x7F) && (seq[j + 2] == 0xB2)) {
                    const s32 tables = (seq[j + 3] << 8) | seq[j + 4];
                    if (tables + 1 >= length) {
                        return 0;
                    }
                    return (u16)((seq[tables] << 8) | seq[tables + 1]);
                }
            }
            return 0;
        }
    }
    return 0;
}

void WriteTableEntry(u8* seq, s32 index, u16 value) {
    seq[sBootsTable + index * 2] = (u8)(value >> 8);
    seq[sBootsTable + index * 2 + 1] = (u8)value;
}

// The copy with Ocarina of Time's scripts, made once per sequence buffer; installed as the sfx player's seqData.
bool InstallBootsSequence() {
    SequencePlayer* seqPlayer = &gAudioCtx.seqPlayers[SEQ_PLAYER_SFX];
    u8* current = seqPlayer->seqData;

    if (current == nullptr) {
        return false;
    }
    if ((current == sBootsSeqCopy) && (sBootsSeqCopy != nullptr)) {
        return true;
    }
    if ((gSequenceMapSize == 0) || (gSequenceMap[0] == nullptr)) {
        return false;
    }
    if (seqPlayer->seqId != 0) {
        return false; // another sequence on the sfx player: leave it
    }
    // The player reads the audio heap's copy of the resource (AudioLoad_SyncLoad), of the resource's size.
    SequenceData* data = ResourceMgr_LoadSeqPtrByName(gSequenceMap[0]);
    if (data == nullptr) {
        return false;
    }
    const s32 length = data->seqDataSize;
    if (current != sBootsSeqOriginal) {
        const u16 table = FindPlayerDynTable(current, length);
        if ((table == 0) || (table + 0x80 * 2 > length)) {
            return false;
        }
        u8* copy = (u8*)malloc(length + 64);
        if (copy == nullptr) {
            return false;
        }
        memcpy(copy, current, length);
        s32 at = length;
        for (s32 i = 0; i < 3; i++) {
            const u16 script = (u16)at;
            const u16 layer = (u16)(at + 6);
            copy[at++] = 0xC1; // set instrument
            copy[at++] = kIronBootsInstrument;
            copy[at++] = 0x88; // ldlayer 0
            copy[at++] = (u8)(layer >> 8);
            copy[at++] = (u8)layer;
            copy[at++] = 0xFF; // end (return from the dyncall)
            memcpy(&copy[at], kBootsLayers[i], kBootsLayerLength[i]);
            at += kBootsLayerLength[i];
            sBootsScript[i] = script;
        }
        // The previous copy (another load of the sequence) is kept: a channel may still be running in it.
        sBootsSeqOriginal = current;
        sBootsSeqCopy = copy;
        sBootsTable = table;
        sBootsTablePatched = false;
        for (s32 i = 0; i < 3; i++) {
            sBootsSaved[i] = (u16)((current[table + kBootsSfx[i] * 2] << 8) | current[table + kBootsSfx[i] * 2 + 1]);
        }
        MergeShipTest_Log("IRONBOOTS sequence: player table @%04X, Ocarina of Time's scripts @%04X/%04X/%04X", table,
                          sBootsScript[0], sBootsScript[1], sBootsScript[2]);
    }
    seqPlayer->seqData = sBootsSeqCopy;
    return true;
}

void SetBootsTable(bool boots) {
    if ((sBootsSeqOriginal == nullptr) || (sBootsTablePatched == boots)) {
        return;
    }
    for (s32 i = 0; i < 3; i++) {
        const u16 value = boots ? sBootsScript[i] : sBootsSaved[i];
        WriteTableEntry(sBootsSeqOriginal, kBootsSfx[i], value);
        WriteTableEntry(sBootsSeqCopy, kBootsSfx[i], value);
    }
    sBootsTablePatched = boots;
}

void UpdateIronBootsSample() {
    Player* player = ((gPlayState != nullptr) && (gGameState == (GameState*)gPlayState)) ? GET_PLAYER(gPlayState)
                                                                                           : nullptr;
    const bool want = (player != nullptr) && (player->currentBoots == PLAYER_BOOTS_IRON);

    if (want && (sIronBootsFont == nullptr) && (gFontMap != nullptr) && (gFontMapSize > 0) &&
        (gFontMap[0] != nullptr)) {
        SoundFont* mm = ResourceMgr_LoadAudioSoundFontByName(gFontMap[0]);
        SoundFont* oot = ResourceMgr_LoadAudioSoundFontByName(kOoTSfxFont);

        if ((mm != nullptr) && (oot != nullptr) && (mm->instruments != nullptr) && (oot->instruments != nullptr) &&
            (mm->numInstruments > kIronBootsInstrument) && (oot->numInstruments > kIronBootsInstrument) &&
            (oot->instruments[kIronBootsInstrument] != nullptr)) {
            sIronBootsSaved = mm->instruments[kIronBootsInstrument];
            mm->instruments[kIronBootsInstrument] = oot->instruments[kIronBootsInstrument];
            sIronBootsFont = mm;
            MergeShipTest_Log("IRONBOOTS Ocarina of Time's instrument 0x%02X in font 0", kIronBootsInstrument);
        }
    } else if (!want && (sIronBootsFont != nullptr)) {
        sIronBootsFont->instruments[kIronBootsInstrument] = sIronBootsSaved;
        sIronBootsFont = nullptr;
        sIronBootsSaved = nullptr;
    }
    const bool hover = (player != nullptr) && (player->currentBoots == PLAYER_BOOTS_HOVER);
    if (hover && (sHoverBootsFont == nullptr) && (gFontMap != nullptr) && (gFontMapSize > 0) &&
        (gFontMap[0] != nullptr)) {
        SoundFont* mm = ResourceMgr_LoadAudioSoundFontByName(gFontMap[0]);
        SoundFont* oot = ResourceMgr_LoadAudioSoundFontByName(kOoTSfxFont);

        if ((mm != nullptr) && (oot != nullptr) && (mm->instruments != nullptr) && (oot->instruments != nullptr) &&
            (mm->numInstruments > kHoverBootsInstrument) && (oot->numInstruments > kHoverBootsInstrument) &&
            (oot->instruments[kHoverBootsInstrument] != nullptr)) {
            sHoverBootsSaved = mm->instruments[kHoverBootsInstrument];
            mm->instruments[kHoverBootsInstrument] = oot->instruments[kHoverBootsInstrument];
            sHoverBootsFont = mm;
            MergeShipTest_Log("HOVERBOOTS Ocarina of Time's instrument 0x%02X in font 0", kHoverBootsInstrument);
        }
    } else if (!hover && (sHoverBootsFont != nullptr)) {
        sHoverBootsFont->instruments[kHoverBootsInstrument] = sHoverBootsSaved;
        sHoverBootsFont = nullptr;
        sHoverBootsSaved = nullptr;
    }
    // Ocarina of Time's scripts for the boots' sounds, in the sequence the sfx player reads.
    if (want) {
        if (InstallBootsSequence()) {
            SetBootsTable(true);
        }
    } else {
        SetBootsTable(false);
    }
}

// ---- per frame ----------------------------------------------------------------------------------------------------------

void RegisterAdultLink() {
    RegisterAnimations();
    COND_HOOK(OnPlayerPostLimbDraw, MergeShip_AdultLink_Enabled(), [](Player* player, s32 limbIndex) {
        if (gPlayState != nullptr) {
            MergeShip_AdultLink_PostLimbDraw(gPlayState, player, limbIndex);
        }
    });
    // The age changed under a standing player (the debug option, a harness command): rebuild him as MM rebuilds the
    // player after a transformation. Also the option itself turning off rebuilds the adult as MM's young human.
    COND_HOOK(OnGameStateUpdate, true, []() {
        UpdateAdultScream();
        UpdateAdultLash();
        UpdateIronBootsSample(); // BUG-0091
        if ((gPlayState != nullptr) && (gGameState == (GameState*)gPlayState)) {
            MergeShip_AdultLink_UpdateRebuild(gPlayState);
            if (sTrace) {
                MergeShip_AdultLink_LogState("AT");
            }
        }
    });
}

static RegisterShipInitFunc sAdultLinkInit(RegisterAdultLink, { MERGESHIP_ADULT_LINK_CVAR });

} // namespace

// Hyrule's option (Shipwright MergeShipSohFd.c MergeShip_SohFd_AdultScream), shared through SettingsMap.def.
#define MERGESHIP_ADULT_SCREAM_CVAR "gEnhancements.MergeShip.AdultTransformScream"

extern "C" void MergeShip_AdultLink_TransformVoice(Player* player, u16 sfxId) {
    if ((sfxId != NA_SE_PL_TRANSFORM_VOICE) || !CVarGetInteger(MERGESHIP_ADULT_SCREAM_CVAR, 1) ||
        !MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    SwapInAdultScream();
}

// The hookshot pulling the adult (MM Player_UpdateUpperBody): Ocarina of Time's adult "hyah!" (above). False: MM's own.
extern "C" s32 MergeShip_AdultLink_HookshotVoice(Player* player) {
    if (!MergeShip_AdultLink_IsActive(player) || (player->currentMask == PLAYER_MASK_GIANT) ||
        (player->currentMask == PLAYER_MASK_SCENTS) || !SwapInAdultLash()) {
        return false;
    }
    Player_PlaySfx(player, NA_SE_VO_LI_LASH + SFX_VOICE_BANK_SIZE);
    return true;
}

// ---- menu ("Enhancements" > "Mergeship", BenMenu.cpp) ---------------------------------------------------------------------

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
using namespace UIWidgets;

static void RegisterAdultLinkMenu() {
    WidgetPath path = { "Enhancements", "Mergeship", SECTION_COLUMN_1 };
    mBenMenu->AddWidget(path, "Adult Link", WIDGET_CVAR_CHECKBOX)
        .CVar(MERGESHIP_ADULT_LINK_CVAR)
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Ocarina of Time's adult Link in Termina: arriving from Hyrule as the adult (or with the "
                              "debug option below), the human is Ocarina of Time's adult Link -- his model, height, "
                              "reach, run speed, animations, voice and Ocarina of Time equipment -- under Majora's "
                              "Mask's rules: the human's items and B sword, and the transformation masks (taking one off "
                              "returns to the adult). The age is kept in the save and through the Song of Time. Needs "
                              "oot-assets.o2r."));
    mBenMenu->AddWidget(path, "Biggoron's Sword On The Back", WIDGET_CVAR_CHECKBOX)
        .CVar(MERGESHIP_BGS_ON_BACK_CVAR)
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Biggoron's Sword and the Giant's Knife, put away, are worn on Link's back in the Master "
                              "Sword's scabbard (Ocarina of Time shows none)."));
    mBenMenu->AddWidget(path, "Hylian Shield Hunch", WIDGET_CVAR_CHECKBOX)
        .CVar(MERGESHIP_HYLIAN_HUNCH_CVAR)
        .Options(CheckboxOptions()
                     .DefaultValue(true)
                     .Tooltip("Young Link guarding with Ocarina of Time's Hylian Shield hunches forward under it, the "
                              "shield on his back, as in Ocarina of Time, instead of crouching behind it in his hand."));
    static const std::unordered_map<int32_t, const char*> sAges = {
        { 0, "From the save" },
        { 1, "Adult (Ocarina of Time)" },
        { 2, "Young (Majora's Mask)" },
    };
    mBenMenu->AddWidget(path, "Human's age (debug)", WIDGET_CVAR_COMBOBOX)
        .CVar(MERGESHIP_ADULT_LINK_DEBUG_CVAR)
        .Options(ComboboxOptions()
                     .ComboMap(&sAges)
                     .DefaultIndex(0)
                     .Tooltip("Testing: makes the human Ocarina of Time's adult Link or Majora's Mask's young Link now "
                              "(the player is rebuilt where he stands) and writes that age into the save."));
}

static RegisterMenuInitFunc sAdultLinkMenuInit(RegisterAdultLinkMenu);

} // namespace BenGui

// ---- harness (MergeShipTest.cpp) --------------------------------------------------------------------------------------------

static long ParseArg(const std::vector<std::string>& args, size_t i, long def = 0) {
    return (i < args.size()) ? strtol(args[i].c_str(), nullptr, 0) : def;
}

extern "C" void MergeShip_AdultLink_LogState(const char* tag) {
    if (gPlayState == nullptr) {
        MergeShipTest_Log("%s no play", tag);
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr) {
        MergeShipTest_Log("%s no player", tag);
        return;
    }
    const char* anim = (player->skelAnime.animation != nullptr) ? (const char*)player->skelAnime.animation : "-";
    MergeShipTest_Log("%s form=%d saveAge=%d tables=%s active=%d skel=%s ceiling=%.1f unk08=%.3f voice=0x%X "
                      "surface=0x%X height=%.1f runLimit=%d speed=%.2f pos=(%.1f,%.1f,%.1f) anim=%s remapped=%u kept=%u "
                      "B=0x%02X sword=%d shield=%d heldItem=0x%02X ia=%d lh=%d rh=%d sheath=%d mask=%d s1=%08X",
                      tag, player->transformation, MergeShip_AdultLink_GetSaveAge(),
                      MergeShip_AdultLink_TablesAdult() ? "adult" : "young", MergeShip_AdultLink_IsActive(player),
                      (const char*)gPlayerSkeletons[PLAYER_FORM_HUMAN], player->ageProperties->ceilingCheckHeight,
                      player->ageProperties->unk_08, player->ageProperties->voiceSfxIdOffset,
                      player->ageProperties->surfaceSfxIdOffset, Player_GetHeight(player), R_RUN_SPEED_LIMIT,
                      player->speedXZ, player->actor.world.pos.x, player->actor.world.pos.y,
                      player->actor.world.pos.z, anim, sRemappedFrames, sKeptFrames,
                      BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), player->currentShield,
                      player->heldItemId, player->heldItemAction, player->leftHandType, player->rightHandType,
                      player->sheathType, player->currentMask, player->stateFlags1);
}

bool MergeShipTest_AdultLinkCommand(const std::string& op, const std::vector<std::string>& args) {
    if (op == "adulthookvoice") {
        // adulthookvoice: the adult's hookshot "hyah!" as the hookshot pulling him plays it (BUG-0020).
        s32 played = (gPlayState != nullptr) && MergeShip_AdultLink_HookshotVoice(GET_PLAYER(gPlayState));
        MergeShipTest_Log("ADULT hookshot voice played=%d", played);
        return true;
    }
    if (op == "adult") {
        // adult <0|1>: the save's age (the player is rebuilt where he stands).
        MergeShip_AdultLink_SetSaveAge((u8)ParseArg(args, 0, 1));
        MergeShipTest_Log("ADULT save age %d", MergeShip_AdultLink_GetSaveAge());
        return true;
    }
    if (op == "adultarrive") {
        // adultarrive <ootAge> [entrance]: what the host does for a player coming through the Lost Woods door: the
        // arrival age, then the scene loads (the player spawns).
        MergeShip_Hyrule_SetArrivalAge((u8)ParseArg(args, 0, 0));
        if (gPlayState != nullptr) {
            gPlayState->nextEntrance = (u16)ParseArg(args, 1, gSaveContext.save.entrance);
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        }
        MergeShipTest_Log("ADULT arrive ootAge=%ld entrance=0x%lX", ParseArg(args, 0, 0),
                          ParseArg(args, 1, gSaveContext.save.entrance));
        return true;
    }
    if (op == "adultreload") {
        // The scene loads again (the player spawns anew) with the save's age as it is.
        if (gPlayState != nullptr) {
            gPlayState->nextEntrance = (u16)ParseArg(args, 0, gSaveContext.save.entrance);
            gPlayState->transitionTrigger = TRANS_TRIGGER_START;
            gPlayState->transitionType = TRANS_TYPE_FADE_BLACK;
        }
        MergeShipTest_Log("ADULT reload, save age %d", MergeShip_AdultLink_GetSaveAge());
        return true;
    }
    if (op == "adultstate") {
        MergeShip_AdultLink_LogState(args.empty() ? "ADULTSTATE" : args[0].c_str());
        return true;
    }
    if (op == "adulttrace") {
        sTrace = !args.empty() && (args[0] == "on");
        return true;
    }
    if (op == "adultsavejson") {
        // The age through the save file's JSON (BenJsonConversions.hpp), and an older save without the key.
        nlohmann::json j = gSaveContext.save.shipSaveInfo;
        ShipSaveInfo copy = {};
        j.get_to(copy);
        nlohmann::json old = j;
        old.erase("mergeShipHumanAge");
        ShipSaveInfo fromOld = {};
        fromOld.mergeShipHumanAge = 0xEE;
        old.get_to(fromOld);
        MergeShipTest_Log("ADULT savejson key=%d json=%s roundtrip=%d old-save=%d",
                          (int)j.contains("mergeShipHumanAge"),
                          j.contains("mergeShipHumanAge") ? j["mergeShipHumanAge"].dump().c_str() : "-",
                          copy.mergeShipHumanAge, fromOld.mergeShipHumanAge);
        return true;
    }
    if (op == "adultsot") {
        // The Song of Time's new cycle, as the save sees it: MM's end-of-cycle save, then the save reloaded.
        u8 before = MergeShip_AdultLink_GetSaveAge();
        if (gPlayState != nullptr) {
            Sram_SaveEndOfCycle(gPlayState);
        }
        MergeShipTest_Log("ADULT end-of-cycle save: age before %d after %d", before, MergeShip_AdultLink_GetSaveAge());
        return true;
    }
    return false;
}

