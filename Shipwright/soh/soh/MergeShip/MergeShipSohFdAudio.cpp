// MergeShip2 (soh_fd) -- mergeship2.o2r, and Adult Link's transformation scream played through Majora's Mask's voice.
//
// Majora's Mask's transformation voice (MM NA_SE_PL_TRANSFORM_VOICE, 0x9AA) is script 0x156C of its sfx sequence
// (player bank, sub-table 0x042E entry 0x2A): one layer plays three drums of MM's sound font 0 -- young Link's two
// gasps (drums 4 and 5, "ChildLinkGasp1" / "ChildLinkGasp2") and his scream (drum 6, SAMPLE_0_213) -- at ticks 0, 54
// and 124 of the sfx player's 96 a second. Adult Link's recording (mergeship2.o2r) has the same three parts at the
// same times, so it is cut there, and while Adult Link transforms those three drums are his parts: each a 16-bit
// sample at the recording's own rate, with the drum's own envelope, release and pan. MM's sfx player then plays his
// voice as it plays every other sound -- its volume and sound-effect settings, pan, reverb and 32 kHz synthesis --
// and MM's own drums are put back once the voice has started all three notes.

#include "soh/MergeShip/MergeShipSohFd.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <dr_wav.h>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/File.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipSohFdAssets.h"
#include "soh/ResourceManagerHelpers.h"

extern "C" void MergeShipTest_Log(const char* fmt, ...); // MergeShipTest.cpp: a no-op unless a test script runs

namespace {

bool sAssetsMounted = false;

// ---- Adult Link's transformation scream --------------------------------------------------------------------------

constexpr float kSynthRate = 32000.0f; // the synthesis's rate: a drum tuning of 1.0 plays a 32 kHz sample as it is
constexpr int kVoiceFirstDrum = 4;     // MM sound font 0's drums 4, 5 and 6
constexpr int kVoiceParts = 3;
constexpr float kVoiceTicksPerSecond = 96.0f; // the sfx sequence's tempo, 120 (48 ticks a beat)
constexpr float kVoicePartTicks[kVoiceParts] = { 0.0f, 54.0f, 124.0f }; // script 0x156C's layer: its drum notes
constexpr int kVoiceSwapFrames = 60; // game frames the adult drums stay in: past the third note (tick 124, ~1.3 s)

struct ScreamPart {
    std::vector<int16_t> pcm; // mono, the recording's own rate
    SoundFontSample sample{};
    AdpcmLoop loop{};
    AdpcmBook book{};
    Drum drum{};
};

ScreamPart sScreamParts[kVoiceParts];
bool sScreamLoaded = false;
bool sScreamLoadTried = false;
SoundFont* sSwappedFont = nullptr;
Drum* sSavedDrums[kVoiceParts];
int sSwapFramesLeft = 0;

// Cuts the recording into the voice's three parts, once. Returns false if it could not be loaded.
bool LoadAdultScream() {
    if (sScreamLoadTried) {
        return sScreamLoaded;
    }
    sScreamLoadTried = true;

    auto file = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(
        MERGESHIP2_ADULT_SCREAM_WAV);
    if ((file == nullptr) || (file->Buffer == nullptr) || (file->Buffer->size() <= file->BufferOffset)) {
        SPDLOG_ERROR("MergeShip: could not load {}", MERGESHIP2_ADULT_SCREAM_WAV);
        return false;
    }
    drwav wav;
    if (!drwav_init_memory(&wav, file->Buffer->data() + file->BufferOffset, file->Buffer->size() - file->BufferOffset,
                           nullptr)) {
        SPDLOG_ERROR("MergeShip: {} is not a WAV file", MERGESHIP2_ADULT_SCREAM_WAV);
        return false;
    }
    drwav_uint64 numFrames = 0;
    drwav_get_length_in_pcm_frames(&wav, &numFrames);
    const uint32_t channels = (wav.channels != 0) ? wav.channels : 1;
    const uint32_t rate = (wav.sampleRate != 0) ? wav.sampleRate : (uint32_t)kSynthRate;
    std::vector<int16_t> interleaved((size_t)numFrames * channels);
    drwav_read_pcm_frames_s16(&wav, numFrames, interleaved.data());
    drwav_uninit(&wav);

    for (int part = 0; part < kVoiceParts; part++) {
        const size_t start = (size_t)(kVoicePartTicks[part] / kVoiceTicksPerSecond * rate);
        const size_t end = (part + 1 < kVoiceParts) ? (size_t)(kVoicePartTicks[part + 1] / kVoiceTicksPerSecond * rate)
                                                    : (size_t)numFrames;
        ScreamPart& p = sScreamParts[part];

        if ((start >= end) || (end > (size_t)numFrames)) {
            SPDLOG_ERROR("MergeShip: {} is too short for the transformation voice", MERGESHIP2_ADULT_SCREAM_WAV);
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
        p.book.npredictors = 0;
        p.book.book = nullptr;
        p.sample.codec = CODEC_S16;
        p.sample.medium = MEDIUM_RAM;
        p.sample.unk_bit26 = 0;
        p.sample.isRelocated = 1;
        p.sample.size = (u32)(p.pcm.size() * sizeof(int16_t));
        p.sample.fileSize = p.sample.size;
        p.sample.sampleAddr = (u8*)p.pcm.data();
        p.sample.loop = &p.loop;
        p.sample.book = &p.book;
        p.drum.sound.sample = &p.sample;
        p.drum.sound.tuning = (f32)rate / kSynthRate;
    }
    sScreamLoaded = true;
    return true;
}

// Puts MM's own voice drums back.
void RestoreVoiceDrums() {
    if (sSwappedFont != nullptr) {
        for (int part = 0; part < kVoiceParts; part++) {
            sSwappedFont->drums[kVoiceFirstDrum + part] = sSavedDrums[part];
        }
        sSwappedFont = nullptr;
    }
    sSwapFramesLeft = 0;
}

} // namespace

void MergeShip_SohFd_MountAssets(const std::shared_ptr<Ship::ArchiveManager>& archiveManager) {
    std::string path = Ship::Context::LocateFileAcrossAppDirs("mergeship2.o2r");

    sAssetsMounted = false;
    if (!std::filesystem::exists(path)) {
        SPDLOG_WARN("MergeShip: mergeship2.o2r not found; the fork's own sounds and icons are unavailable");
        return;
    }
    if (archiveManager->AddArchive(path) == nullptr) {
        SPDLOG_ERROR("MergeShip: could not mount {}", path);
        return;
    }
    sAssetsMounted = true;
    SPDLOG_INFO("MergeShip: mounted {}", path);
}

extern "C" s32 MergeShip_SohFd_AssetsMounted(void) {
    return sAssetsMounted ? 1 : 0;
}

extern "C" s32 MergeShip_SohFd_SwapInAdultVoice(void) {
    SoundFont* font;

    if (!sAssetsMounted || !LoadAdultScream()) {
        return false;
    }
    font = ResourceMgr_LoadAudioSoundFontByName(MERGESHIP_MM_NAMESPACE "/audio/fonts/Soundfont_0");
    if ((font == nullptr) || (font->drums == nullptr) || (font->numDrums < kVoiceFirstDrum + kVoiceParts)) {
        return false;
    }
    if (sSwappedFont == nullptr) {
        for (int part = 0; part < kVoiceParts; part++) {
            Drum* original = font->drums[kVoiceFirstDrum + part];

            if (original == nullptr) {
                return false;
            }
            sSavedDrums[part] = original;
            // The drum's own envelope, release and pan; only the sound is Adult Link's.
            sScreamParts[part].drum.releaseRate = original->releaseRate;
            sScreamParts[part].drum.pan = original->pan;
            sScreamParts[part].drum.loaded = original->loaded;
            sScreamParts[part].drum.envelope = original->envelope;
        }
        // Each pointer is one aligned store: the audio thread reads either drum whole. The parts never move or free,
        // so a note already playing one keeps it after the drums are put back.
        for (int part = 0; part < kVoiceParts; part++) {
            font->drums[kVoiceFirstDrum + part] = &sScreamParts[part].drum;
        }
        sSwappedFont = font;
    }
    sSwapFramesLeft = kVoiceSwapFrames;
    MergeShipTest_Log("SOHFD adult voice in MM font 0 drums %d-%d", kVoiceFirstDrum, kVoiceFirstDrum + kVoiceParts - 1);
    return true;
}

extern "C" void MergeShip_SohFd_UpdateAdultVoice(void) {
    if ((sSwappedFont != nullptr) && (--sSwapFramesLeft <= 0)) {
        RestoreVoiceDrums();
    }
}
