// MergeShip2 -- the game-thread half of Majora's Mask's sound effects inside Ocarina of Time.
//
// MM's sfx system, ported from 2ship2harkinian/mm/src/audio/sfx.c (requests, banks, active sfx) and the
// AudioSfx_* / Audio_PlaySfx_* parts of 2ship2harkinian/mm/src/audio/code_8019AF00.c (per-sfx volume, reverb,
// pan, frequency, filters from the sound's projected position; SfxChannelState). It is a second, independent
// copy of the system: its banks, requests and channel state are its own, it reads MM's per-sfx parameters
// (MergeShipMMSfxTables.inc, generated from MM's tables/sfx/*bank_table.h), and it drives the MM sequence
// player of MergeShipMMSeqPlayer.c (player index MERGESHIP_MM_SEQ_PLAYER). Ocarina of Time's own sfx system,
// its gSoundBanks and its sequence player 2 are untouched.
//
// Scene-level state MM's formulas read (environment/spec reverb, the underwater filters, the sound output
// mode) is taken from Ocarina of Time's audio code, which keeps the same variables for the same purpose.

void MergeShipTest_Log(const char* fmt, ...);
#include "soh/MergeShip/MergeShipGanondorf.h"
#include "global.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipMMSfx.h"
#include "soh/MergeShip/MergeShipChildLink.h" // MergeShip2: MM's human as the child sounds like MM's human

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef ABS_ALT
#define ABS_ALT(x) ((x) < 0 ? -(x) : (x))
#endif

// ---- MM sfx.h -------------------------------------------------------------------------------------------

#define MMSFX_FLAG_MASK 0xC00
#define MMSFX_FLAG 0x800

#define MMSFX_BANK_SHIFT(sfxId) (((sfxId) >> 12) & 0xFF)
#define MMSFX_BANK_MASK(sfxId) ((sfxId)&0xF000)
#define MMSFX_INDEX(sfxId) ((sfxId)&0x3FF)
#define MMSFX_BANK(sfxId) MMSFX_BANK_SHIFT(MMSFX_BANK_MASK(sfxId))

#define MMSFX_PARAM_DIST_RANGE_SHIFT 0
#define MMSFX_PARAM_DIST_RANGE_MASK_UPPER (4 << MMSFX_PARAM_DIST_RANGE_SHIFT)
#define MMSFX_PARAM_DIST_RANGE_MASK (7 << MMSFX_PARAM_DIST_RANGE_SHIFT)
#define MMSFX_FLAG_LOWER_VOLUME_BGM (1 << 3)
#define MMSFX_FLAG_PRIORITY_NO_DIST (1 << 4)
#define MMSFX_FLAG_BLOCK_EQUAL_IMPORTANCE (1 << 5)
#define MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT 6
#define MMSFX_PARAM_RAND_FREQ_RAISE_MASK (3 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT)
#define MMSFX_FLAG_8 (1 << 8)
#define MMSFX_FLAG_SURROUND_LOWPASS_FILTER (1 << 9)
#define MMSFX_FLAG_BEHIND_SCREEN_Z_INDEX_SHIFT 10
#define MMSFX_FLAG_BEHIND_SCREEN_Z_INDEX (1 << MMSFX_FLAG_BEHIND_SCREEN_Z_INDEX_SHIFT)
#define MMSFX_PARAM_RAND_FREQ_SCALE (1 << 11)
#define MMSFX_FLAG_REVERB_NO_DIST (1 << 12)
#define MMSFX_FLAG_VOLUME_NO_DIST (1 << 13)
#define MMSFX_PARAM_RAND_FREQ_LOWER (1 << 14)
#define MMSFX_FLAG_FREQ_NO_DIST (1 << 15)
#define MMSFX_FLAG2_FORCE_RESET (1 << 0)
#define MMSFX_FLAG2_UNUSED2 (1 << 2)
#define MMSFX_FLAG2_UNUSED4 (1 << 4)
#define MMSFX_FLAG2_SURROUND_NO_HIGHPASS_FILTER (1 << 5)
#define MMSFX_FLAG2_UNUSED6 (1 << 6)
#define MMSFX_FLAG2_APPLY_LOWPASS_FILTER (1 << 7)

#define MM_SEQ_IO_VAL_NONE -1
#define MM_SOUNDMODE_SURROUND_EXTERNAL 2 // Ocarina of Time's sSoundOutputMode uses the same value for 3D sound
#define MM_VOL_SCALE_INDEX_SFX 2

// MM SfxChannelIndex
#define MM_SFX_CHANNEL_OCARINA 0xD

// MM OcarinaInstrumentId (z64ocarina.h)
#define MM_OCARINA_INSTRUMENT_OFF 0
#define MM_OCARINA_INSTRUMENT_DEFAULT 1
#define MM_OCARINA_INSTRUMENT_GORON_DRUMS 7
#define MM_OCARINA_INSTRUMENT_ZORA_GUITAR 8
#define MM_OCARINA_INSTRUMENT_DEKU_PIPES 9

typedef enum {
    /* 0 */ MM_BANK_PLAYER,
    /* 1 */ MM_BANK_ITEM,
    /* 2 */ MM_BANK_ENV,
    /* 3 */ MM_BANK_ENEMY,
    /* 4 */ MM_BANK_SYSTEM,
    /* 5 */ MM_BANK_OCARINA,
    /* 6 */ MM_BANK_VOICE
} MMSfxBankType;

typedef enum {
    /* 0 */ MM_SFX_STATE_EMPTY,
    /* 1 */ MM_SFX_STATE_QUEUED,
    /* 2 */ MM_SFX_STATE_READY,
    /* 3 */ MM_SFX_STATE_PLAYING_REFRESH,
    /* 4 */ MM_SFX_STATE_PLAYING,
    /* 5 */ MM_SFX_STATE_PLAYING_ONE_FRAME
} MMSfxState;

typedef struct {
    /* 0x00 */ f32* posX;
    /* 0x04 */ f32* posY;
    /* 0x08 */ f32* posZ;
    /* 0x0C */ f32* freqScale;
    /* 0x10 */ f32* volume;
    /* 0x14 */ s8* reverbAdd;
    /* 0x18 */ f32 dist;
    /* 0x1C */ u32 priority; // lower is more prioritized
    /* 0x20 */ u16 sfxParams;
    /* 0x22 */ u16 sfxId;
    /* 0x25 */ u8 sfxFlags;
    /* 0x24 */ u8 sfxImportance;
    /* 0x26 */ u8 state; // uses MMSfxState enum
    /* 0x27 */ u8 freshness;
    /* 0x28 */ u8 prev;
    /* 0x29 */ u8 next;
    /* 0x2A */ u8 channelIndex;
    /* 0x2B */ u8 randFreq;
    /* 0x2C */ u8 token;
} MMSfxBankEntry;

typedef struct {
    /* 0x0 */ u32 priority; // lower is more prioritized
    /* 0x4 */ u8 entryIndex;
} MMActiveSfx;

typedef struct {
    /* 0x0 */ u8 importance;
    /* 0x1 */ u8 flags;
    /* 0x2 */ u16 params;
} MMSfxParams;

// MM code_8019AF00.c SfxChannelState, read byte-wise by sequence 0's channel commands 0xA0-0xA3: the layout is
// part of the sequence's contract and must stay MM's.
typedef struct {
    /* 0x0 */ f32 volume;
    /* 0x4 */ f32 freqScale;
    /* 0x8 */ s8 reverb;
    /* 0x9 */ s8 panSigned;
    /* 0xA */ s8 stereoBits;
    /* 0xB */ u8 filter;
    /* 0xC */ u8 combFilterGain;
    /* 0xD */ u8 zVolume;
} MMSfxChannelState; // size = 0x10

_Static_assert(offsetof(MMSfxChannelState, reverb) == 0x8, "MM SfxChannelState layout");
_Static_assert(offsetof(MMSfxChannelState, zVolume) == 0xD, "MM SfxChannelState layout");
_Static_assert(sizeof(MMSfxChannelState) == 0x10, "MM SfxChannelState layout");

typedef struct {
    /* 0x00 */ u16 sfxId;
    /* 0x02 */ u8 token;
    /* 0x04 */ s8* reverbAdd;
    /* 0x08 */ Vec3f* pos;
    /* 0x0C */ f32* freqScale;
    /* 0x10 */ f32* volume;
} MMSfxRequest;

typedef struct {
    /* 0x0 */ f32 value;
    /* 0x4 */ f32 target;
    /* 0x8 */ f32 step;
    /* 0xC */ u16 remainingFrames;
} MMSfxBankLerp;

typedef enum {
    /* 0 */ SFX_RM_REQ_BY_BANK,
    /* 1 */ SFX_RM_REQ_BY_POS_AND_BANK,
    /* 2 */ SFX_RM_REQ_BY_POS,
    /* 3 */ SFX_RM_REQ_BY_POS_AND_ID,
    /* 4 */ SFX_RM_REQ_BY_TOKEN_AND_ID,
    /* 5 */ SFX_RM_REQ_BY_ID
} MMSfxRemoveRequest;

#include "soh/MergeShip/MergeShipMMSfxTables.inc"

// MM sfx_params.c gSfxParams
static const MMSfxParams* sMMSfxParams[7] = {
    sMMPlayerBankParams, sMMItemBankParams,    sMMEnvBankParams,   sMMEnemyBankParams,
    sMMSystemBankParams, sMMOcarinaBankParams, sMMVoiceBankParams,
};
static const u16 sMMSfxParamsCount[7] = {
    ARRAY_COUNT(sMMPlayerBankParams), ARRAY_COUNT(sMMItemBankParams),    ARRAY_COUNT(sMMEnvBankParams),
    ARRAY_COUNT(sMMEnemyBankParams),  ARRAY_COUNT(sMMSystemBankParams), ARRAY_COUNT(sMMOcarinaBankParams),
    ARRAY_COUNT(sMMVoiceBankParams),
};

// Scene state kept by Ocarina of Time's code_800EC960.c for the same purposes as MM's.
extern s8 sSpecReverb;
extern s8 sAudioEnvReverb;
extern s8 sAudioCodeReverb;
extern u8 sAudioBaseFilter;
extern u8 sAudioExtraFilter;
extern u8 sAudioBaseFilter2;
extern u8 sAudioExtraFilter2;
extern s8 sSoundOutputMode;

extern PlayState* gPlayState;

// ---- MM sfx.c state -------------------------------------------------------------------------------------

static MMSfxBankEntry sMMSfxPlayerBank[9];
static MMSfxBankEntry sMMSfxItemBank[12];
static MMSfxBankEntry sMMSfxEnvironmentBank[32];
static MMSfxBankEntry sMMSfxEnemyBank[20];
static MMSfxBankEntry sMMSfxSystemBank[8];
static MMSfxBankEntry sMMSfxOcarinaBank[3];
static MMSfxBankEntry sMMSfxVoiceBank[5];
static MMSfxRequest sMMSfxRequests[0x100];
static u8 sMMSfxBankListEnd[7];
static u8 sMMSfxBankFreeListStart[7];
static u8 sMMSfxBankUnused[7];
static MMActiveSfx sMMActiveSfx[7][3];
static u8 sMMCurSfxPlayerChannelIndex;
static u8 sMMSfxBankMuted[7];
static MMSfxBankLerp sMMSfxBankLerp[7];
static u8 sMMSfxRequestWriteIndex = 0;
static u8 sMMSfxRequestReadIndex = 0;

static MMSfxBankEntry* sMMSfxBanks[7] = {
    sMMSfxPlayerBank, sMMSfxItemBank,    sMMSfxEnvironmentBank, sMMSfxEnemyBank,
    sMMSfxSystemBank, sMMSfxOcarinaBank, sMMSfxVoiceBank,
};

static u8 sMMSfxBankSizes[ARRAY_COUNT(sMMSfxBanks)] = {
    ARRAY_COUNT(sMMSfxPlayerBank), ARRAY_COUNT(sMMSfxItemBank),   ARRAY_COUNT(sMMSfxEnvironmentBank),
    ARRAY_COUNT(sMMSfxEnemyBank),  ARRAY_COUNT(sMMSfxSystemBank), ARRAY_COUNT(sMMSfxOcarinaBank),
    ARRAY_COUNT(sMMSfxVoiceBank),
};

static u8 sMMSfxChannelLayout = 0;
static u16 sMMSfxChannelLowVolumeFlag = 0;

// MM code_8019AF00.c sfx data
static u8 sMMIsLargeSfxBank[] = {
    true, false, true, true, false, false, true,
};
static u8 sMMSetIOPort5[] = { // MM D_801D6608
    true, true, true, true, true, false, true,
};
static u8 sMMChannelsPerBank[4][7] = {
    { 3, 2, 3, 3, 2, 1, 2 },
    { 3, 2, 2, 2, 2, 2, 2 },
    { 3, 2, 2, 2, 2, 2, 2 },
    { 4, 1, 0, 0, 2, 2, 2 },
};
static u8 sMMUsedChannelsPerBank[4][7] = {
    { 3, 2, 3, 2, 2, 1, 1 },
    { 3, 1, 1, 1, 2, 1, 1 },
    { 3, 1, 1, 1, 2, 1, 1 },
    { 2, 1, 0, 0, 1, 1, 1 },
};

static MMSfxChannelState sMMSfxChannelState[16];

// MM sSfxSyncedVolume / sSfxSyncedVolumeForMetalEffects / sSfxSyncedFreq
static f32 sMMSfxSyncedVolume;
static f32 sMMSfxSyncedVolumeForMetalEffects;
static f32 sMMSfxSyncedFreq;

// Majora's Mask audio is available only with a namespaced mm.o2r; the resource side reports failure.
static s32 sMMAudioUnavailable = false;

// The bank lists are only valid after MM_AudioSfx_Reset has linked them.
static s32 sMMSfxInitialized = false;
static void MM_EnsureInitialized(void);

// ---- commands to the MM sequence player -------------------------------------------------------------------

// MM AUDIOCMD_CHANNEL_SET_IO(SEQ_PLAYER_SFX, channelIndex, ioPort, ioData)
static void MM_SetChannelIO(u8 channelIndex, u8 ioPort, s8 ioData) {
    Audio_QueueCmdS8(0x06000000 | (MERGESHIP_MM_SEQ_PLAYER << 16) | ((channelIndex & 0xFF) << 8) | ioPort, ioData);
}

static s32 MM_SfxPlayerEnabled(void) {
    return gMergeShipMMSeqPlayer.enabled;
}

// ---- MM sfx.c -------------------------------------------------------------------------------------------

// (MM's AudioSfx_MuteBanks has no caller here: its only use, MM's audio spec 0xB, has no Hyrule counterpart.
// Ocarina of Time's bank mutes -- pause menu, ocarina -- are honoured through gSoundBankMuted.)

// MM AudioSfx_LowerBgmVolume
static void MM_AudioSfx_LowerBgmVolume(u8 channelIndex) {
    sMMSfxChannelLowVolumeFlag |= (1 << channelIndex);

    Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, MM_VOL_SCALE_INDEX_SFX, 0x40, 0xF);
    Audio_SetVolScale(SEQ_PLAYER_BGM_SUB, MM_VOL_SCALE_INDEX_SFX, 0x40, 0xF);
}

// MM AudioSfx_RestoreBgmVolume
static void MM_AudioSfx_RestoreBgmVolume(u8 channelIndex) {
    sMMSfxChannelLowVolumeFlag &= ((1 << channelIndex) ^ 0xFFFF);

    if (sMMSfxChannelLowVolumeFlag == 0) {
        Audio_SetVolScale(SEQ_PLAYER_BGM_MAIN, MM_VOL_SCALE_INDEX_SFX, 0x7F, 0xF);
        Audio_SetVolScale(SEQ_PLAYER_BGM_SUB, MM_VOL_SCALE_INDEX_SFX, 0x7F, 0xF);
    }
}

static const MMSfxParams* MM_GetSfxParams(u16 sfxId) {
    static const MMSfxParams sNone = { 0, 0, 0 };
    u8 bank = MMSFX_BANK_SHIFT(sfxId);

    if ((bank >= ARRAY_COUNT(sMMSfxParams)) || (MMSFX_INDEX(sfxId) >= sMMSfxParamsCount[bank])) {
        return &sNone;
    }
    return &sMMSfxParams[bank][MMSFX_INDEX(sfxId)];
}

// MM AudioSfx_PlaySfx. Ocarina of Time's bank mutes (pause menu, ocarina) apply as well.
void MergeShip_MMSfx_PlaySfx(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* volume, s8* reverbAdd) {
    MergeShipTest_Log("MMSFX 0x%04X", sfxId); // MergeShip2: harness evidence
    u8 i;
    u8 bank = MMSFX_BANK_SHIFT(sfxId);
    MMSfxRequest* reqWrite;
    MMSfxRequest* reqRead;

    MergeShipTest_Log("MMSFX req %04X bank=%d unavail=%d muted=%d/%d", sfxId, bank, sMMAudioUnavailable,
                      (bank < ARRAY_COUNT(sMMSfxBanks)) ? sMMSfxBankMuted[bank] : -1,
                      (bank < 7) ? gSoundBankMuted[bank] : -1);
    if ((sfxId == 0) || sMMAudioUnavailable || (bank >= ARRAY_COUNT(sMMSfxBanks))) {
        return;
    }
    MM_EnsureInitialized();

    if (!sMMSfxBankMuted[bank] && !gSoundBankMuted[bank]) {
        reqWrite = &sMMSfxRequests[sMMSfxRequestWriteIndex];

        for (i = sMMSfxRequestReadIndex; sMMSfxRequestWriteIndex != i; i++) {
            reqRead = &sMMSfxRequests[i];
            if ((reqRead->pos == pos) && (reqRead->sfxId == sfxId)) {
                return;
            }
        }

        reqWrite->sfxId = sfxId;
        reqWrite->pos = pos;
        reqWrite->token = token;
        reqWrite->freqScale = freqScale;
        reqWrite->volume = volume;
        reqWrite->reverbAdd = reverbAdd;
        sMMSfxRequestWriteIndex++;
    }
}

// MM AudioSfx_RemoveMatchingRequests
static void MM_AudioSfx_RemoveMatchingRequests(u8 aspect, MMSfxBankEntry* entry) {
    MMSfxRequest* req;
    s32 remove;
    u8 i = sMMSfxRequestReadIndex;

    for (; i != sMMSfxRequestWriteIndex; i++) {
        remove = false;
        req = &sMMSfxRequests[i];

        switch (aspect) {
            case SFX_RM_REQ_BY_BANK:
                if (MMSFX_BANK_MASK(req->sfxId) == MMSFX_BANK_MASK(entry->sfxId)) {
                    remove = true;
                }
                break;

            case SFX_RM_REQ_BY_POS_AND_BANK:
                if ((MMSFX_BANK_MASK(req->sfxId) == MMSFX_BANK_MASK(entry->sfxId)) && (&req->pos->x == entry->posX)) {
                    remove = true;
                }
                break;

            case SFX_RM_REQ_BY_POS:
                if (&req->pos->x == entry->posX) {
                    remove = true;
                }
                break;

            case SFX_RM_REQ_BY_POS_AND_ID:
                if ((&req->pos->x == entry->posX) && (req->sfxId == entry->sfxId)) {
                    remove = true;
                }
                break;

            case SFX_RM_REQ_BY_TOKEN_AND_ID:
                if ((req->token == entry->token) && (req->sfxId == entry->sfxId)) {
                    remove = true;
                }
                break;

            case SFX_RM_REQ_BY_ID:
                if (req->sfxId == entry->sfxId) {
                    remove = true;
                }
                break;

            default:
                break;
        }

        if (remove) {
            req->sfxId = 0; // NA_SE_NONE
        }
    }
}

// MM AudioSfx_ProcessRequest
static void MM_AudioSfx_ProcessRequest(void) {
    u16 sfxId;
    u8 channelCount;
    u8 index;
    MMSfxRequest* req = &sMMSfxRequests[sMMSfxRequestReadIndex];
    MMSfxBankEntry* entry;
    const MMSfxParams* sfxParams;
    s32 bankId;
    u8 evictImportance = 0;
    u8 evictIndex = 0x80;

    if (req->sfxId == 0) {
        return;
    }

    bankId = MMSFX_BANK(req->sfxId);
    if (bankId >= ARRAY_COUNT(sMMSfxBanks)) {
        return;
    }
    channelCount = 0;
    index = sMMSfxBanks[bankId][0].next;

    // Compare the sfx request to all sfx bank entries
    while ((index != 0xFF) && (index != 0)) {

        // If an existing sfx from the same source exists in the bank, then we should either:
        // renew if it is the same sfx, check if any sfx channels are available for that bank,
        // interrupt that sfx and replace it with the new sfx request, or drop the new sfx request.
        if (sMMSfxBanks[bankId][index].posX == &req->pos->x) {

            // If the new sfx has equal importance to the existing sfx,
            // drop the request if the existing sfx has the "SFX_FLAG_BLOCK_EQUAL_IMPORTANCE" flag.
            if ((MM_GetSfxParams(req->sfxId)->params & MMSFX_FLAG_BLOCK_EQUAL_IMPORTANCE) &&
                (MM_GetSfxParams(req->sfxId)->importance == sMMSfxBanks[bankId][index].sfxImportance)) {
                // Drop the new request
                return;
            }

            if (sMMSfxBanks[bankId][index].sfxId == req->sfxId) {
                // The new sfx is the same as the existing sfx
                // Set channelCount to max value to reprocess the sfx
                channelCount = sMMUsedChannelsPerBank[sMMSfxChannelLayout][bankId];
            } else {
                // Determine which sfx to evict based on importance if there are no more channels available
                if (channelCount == 0) {
                    evictIndex = index;
                    sfxId = sMMSfxBanks[bankId][index].sfxId & 0xFFFF;
                    evictImportance = MM_GetSfxParams(sfxId)->importance;
                } else if (sMMSfxBanks[bankId][index].sfxImportance < evictImportance) {
                    evictIndex = index;
                    sfxId = sMMSfxBanks[bankId][index].sfxId & 0xFFFF;
                    evictImportance = MM_GetSfxParams(sfxId)->importance;
                }

                channelCount++;

                // There are no channels available, evict the lowest importance
                if (channelCount == sMMUsedChannelsPerBank[sMMSfxChannelLayout][bankId]) {
                    if (MM_GetSfxParams(req->sfxId)->importance >= evictImportance) {
                        index = evictIndex;
                    } else {
                        index = 0;
                    }
                }
            }

            // Process this sfx by refreshing or replacing and existing sfx
            if (channelCount == sMMUsedChannelsPerBank[sMMSfxChannelLayout][bankId]) {
                sfxParams = MM_GetSfxParams(req->sfxId);

                // Interrupt existing sfx and play the new sfx instead.
                if ((req->sfxId & MMSFX_FLAG_MASK) || (sfxParams->flags & MMSFX_FLAG2_FORCE_RESET) ||
                    (index == evictIndex)) {

                    // Restore bgm if the sfx about to be replaced has the right flag
                    if ((sMMSfxBanks[bankId][index].sfxParams & MMSFX_FLAG_LOWER_VOLUME_BGM) &&
                        (sMMSfxBanks[bankId][index].state != MM_SFX_STATE_QUEUED)) {
                        MM_AudioSfx_RestoreBgmVolume(sMMSfxBanks[bankId][index].channelIndex);
                    }

                    sMMSfxBanks[bankId][index].token = req->token;
                    sMMSfxBanks[bankId][index].sfxId = req->sfxId;
                    sMMSfxBanks[bankId][index].state = MM_SFX_STATE_QUEUED;
                    sMMSfxBanks[bankId][index].freshness = 2;
                    sMMSfxBanks[bankId][index].freqScale = req->freqScale;
                    sMMSfxBanks[bankId][index].volume = req->volume;
                    sMMSfxBanks[bankId][index].reverbAdd = req->reverbAdd;
                    sMMSfxBanks[bankId][index].sfxParams = sfxParams->params;
                    sMMSfxBanks[bankId][index].sfxFlags = sfxParams->flags;
                    sMMSfxBanks[bankId][index].sfxImportance = sfxParams->importance;
                } else if (sMMSfxBanks[bankId][index].state == MM_SFX_STATE_PLAYING_ONE_FRAME) {
                    // The new sfx is "one-frame" and equals the existing sfx: keep playing it.
                    sMMSfxBanks[bankId][index].state = MM_SFX_STATE_PLAYING;
                }
                // Terminate loop, function is finished (nothing more is processed)
                index = 0;
            }
        }

        if (index != 0) {
            index = sMMSfxBanks[bankId][index].next;
        }
    }

    if ((sMMSfxBanks[bankId][sMMSfxBankFreeListStart[bankId]].next != 0xFF) && (index != 0)) {
        // Allocate from free list
        index = sMMSfxBankFreeListStart[bankId];

        entry = &sMMSfxBanks[bankId][index];
        entry->posX = &req->pos->x;
        entry->posY = &req->pos->y;
        entry->posZ = &req->pos->z;
        entry->token = req->token;
        entry->freqScale = req->freqScale;
        entry->volume = req->volume;
        entry->reverbAdd = req->reverbAdd;

        sfxParams = MM_GetSfxParams(req->sfxId);

        entry->sfxParams = sfxParams->params;
        entry->sfxFlags = sfxParams->flags;
        entry->sfxImportance = sfxParams->importance;
        entry->sfxId = req->sfxId;
        entry->state = MM_SFX_STATE_QUEUED;
        entry->freshness = 2;
        entry->prev = sMMSfxBankListEnd[bankId];

        sMMSfxBanks[bankId][sMMSfxBankListEnd[bankId]].next = sMMSfxBankFreeListStart[bankId];
        sMMSfxBankListEnd[bankId] = sMMSfxBankFreeListStart[bankId];
        sMMSfxBankFreeListStart[bankId] = sMMSfxBanks[bankId][sMMSfxBankFreeListStart[bankId]].next;
        sMMSfxBanks[bankId][sMMSfxBankFreeListStart[bankId]].prev = 0xFF;

        entry->next = 0xFF;
    }
}

// MM AudioSfx_RemoveBankEntry
static void MM_AudioSfx_RemoveBankEntry(u8 bankId, u8 entryIndex) {
    MMSfxBankEntry* entry = &sMMSfxBanks[bankId][entryIndex];
    u8 i;

    if (entry->sfxParams & MMSFX_FLAG_LOWER_VOLUME_BGM) {
        MM_AudioSfx_RestoreBgmVolume(entry->channelIndex);
    }

    if (entryIndex == sMMSfxBankListEnd[bankId]) {
        sMMSfxBankListEnd[bankId] = entry->prev;
    } else {
        sMMSfxBanks[bankId][entry->next].prev = entry->prev;
    }

    sMMSfxBanks[bankId][entry->prev].next = entry->next;
    entry->next = sMMSfxBankFreeListStart[bankId];
    entry->prev = 0xFF;
    sMMSfxBanks[bankId][sMMSfxBankFreeListStart[bankId]].prev = entryIndex;
    sMMSfxBankFreeListStart[bankId] = entryIndex;
    entry->state = MM_SFX_STATE_EMPTY;

    for (i = 0; i < sMMChannelsPerBank[sMMSfxChannelLayout][bankId]; i++) {
        if (sMMActiveSfx[bankId][i].entryIndex == entryIndex) {
            sMMActiveSfx[bankId][i].entryIndex = 0xFF;
            i = sMMChannelsPerBank[sMMSfxChannelLayout][bankId];
        }
    }
}

// MM AudioSfx_ChooseActiveSfx
static void MM_AudioSfx_ChooseActiveSfx(u8 bankId) {
    u8 numChosenSfx = 0;
    u8 numChannels;
    u8 entryIndex;
    u8 i;
    u8 j;
    u8 k;
    u8 sfxImportance;
    u8 needNewSfx;
    u8 chosenEntryIndex;
    MMSfxBankEntry* entry;
    MMActiveSfx chosenSfx[MAX_CHANNELS_PER_BANK];
    MMActiveSfx* activeSfx;
    f32 entryPosY;
    f32 entryPosX;

    for (i = 0; i < MAX_CHANNELS_PER_BANK; i++) {
        chosenSfx[i].priority = 0x7FFFFFFF;
        chosenSfx[i].entryIndex = 0xFF;
    }

    entryIndex = sMMSfxBanks[bankId][0].next;
    k = 0;

    // Delete stale sfx and prioritize remaining sfx into the active arrays
    while (entryIndex != 0xFF) {

        // Update the freshness for an "all-frame" sfx if it is still queued
        if ((sMMSfxBanks[bankId][entryIndex].state == MM_SFX_STATE_QUEUED) &&
            (sMMSfxBanks[bankId][entryIndex].sfxId & MMSFX_FLAG_MASK)) {
            sMMSfxBanks[bankId][entryIndex].freshness--;

            // If a "one-frame" sfx is still in "SFX_STATE_PLAYING_ONE_FRAME", then remove the sfx
        } else if (!(sMMSfxBanks[bankId][entryIndex].sfxId & MMSFX_FLAG_MASK) &&
                   (sMMSfxBanks[bankId][entryIndex].state == MM_SFX_STATE_PLAYING_ONE_FRAME)) {
            // ioPort 0, force stop sfx in seq 0
            MM_SetChannelIO(sMMSfxBanks[bankId][entryIndex].channelIndex, 0, 0);
            MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
        }

        // If a "all-frame" sfx goes 2 frames in the "queued" state
        // (because it is too low priority), then remove the sfx
        if (sMMSfxBanks[bankId][entryIndex].freshness == 0) {
            MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
        } else if (sMMSfxBanks[bankId][entryIndex].state != MM_SFX_STATE_EMPTY) {
            // process the entry
            entry = &sMMSfxBanks[bankId][entryIndex];

            // Recompute distSq each frame since the sound's position may have changed
            if (&gSfxDefaultPos.x == entry[0].posX) {
                entry->dist = 0.0f;
            } else {
                entryPosY = *entry->posY * 1;
                entryPosX = *entry->posX * 0.5f;
                entry->dist = (SQ(entryPosX) + SQ(entryPosY) + SQ(*entry->posZ)) / 10.0f;
            }

            // Recompute priority, possibly based on sfx position relative to the camera.
            sfxImportance = entry->sfxImportance;

            if (entry->sfxParams & MMSFX_FLAG_PRIORITY_NO_DIST) {
                // Priority is independent of distance
                entry->priority = SQ(0xFF - sfxImportance) * SQ(76);
            } else {
                // Priority value increases with distance (more likely to eject)
                if (entry->dist > 0x7FFFFFD0) {
                    // max distance
                    entry->dist = 0x70000008;
                }

                entry->priority = (u32)entry->dist + (SQ(0xFF - sfxImportance) * SQ(76));
                if (*entry->posZ < 0.0f) {
                    entry->priority += (s32)(-*entry->posZ * 6.0f);
                }
            }

            // Sfx is too far away, do not consider for chosen sfx
            if (entry->dist > SQ(1e5f)) {

                // If too far away and also playing, stop playing
                if (entry->state == MM_SFX_STATE_PLAYING) {
                    // ioPort 0, force stop sfx in seq 0
                    MM_SetChannelIO(entry->channelIndex, 0, 0);
                    if (entry->sfxId & MMSFX_FLAG_MASK) {
                        MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
                        entryIndex = k;
                    }
                }
            } else {
                // Sort all current sfx entries by priority; this is where the "active" sfx are chosen
                numChannels = sMMChannelsPerBank[sMMSfxChannelLayout][bankId];

                for (i = 0; i < numChannels; i++) {
                    if (chosenSfx[i].priority >= entry->priority) {
                        if (numChosenSfx < sMMChannelsPerBank[sMMSfxChannelLayout][bankId]) {
                            numChosenSfx++;
                        }

                        for (j = numChannels - 1; j > i; j--) {
                            chosenSfx[j].priority = chosenSfx[j - 1].priority;
                            chosenSfx[j].entryIndex = chosenSfx[j - 1].entryIndex;
                        }

                        chosenSfx[i].priority = entry->priority;
                        chosenSfx[i].entryIndex = entryIndex;
                        i = numChannels; // "break"
                    }
                }
            }

            k = entryIndex;
        }

        entryIndex = sMMSfxBanks[bankId][k].next;
    }

    // If a sfx is chosen, update its state
    for (i = 0; i < numChosenSfx; i++) {
        entry = &sMMSfxBanks[bankId][chosenSfx[i].entryIndex];

        if (entry->state == MM_SFX_STATE_QUEUED) {
            entry->state = MM_SFX_STATE_READY;
        } else if (entry->state == MM_SFX_STATE_PLAYING) {
            entry->state = MM_SFX_STATE_PLAYING_REFRESH;
        }
    }

    // Apply the chosenSfx to the activeSfx in each channel
    numChannels = sMMChannelsPerBank[sMMSfxChannelLayout][bankId];
    for (i = 0; i < numChannels; i++) {
        needNewSfx = false;
        activeSfx = &sMMActiveSfx[bankId][i];

        if (activeSfx->entryIndex == 0xFF) {
            // No sfx playing in the channel
            needNewSfx = true;
        } else {
            entry = &sMMSfxBanks[bankId][activeSfx[0].entryIndex];

            if (entry->state == MM_SFX_STATE_PLAYING) {
                if (entry->sfxId & MMSFX_FLAG_MASK) {
                    // For "all-frames" sfx, remove the entry
                    MM_AudioSfx_RemoveBankEntry(bankId, activeSfx->entryIndex);
                } else {
                    // For "one-frame" sfx, reset the state to queued
                    entry->state = MM_SFX_STATE_QUEUED;
                    entry->freshness = 0x80;
                }
                needNewSfx = true;
            } else if (entry->state == MM_SFX_STATE_EMPTY) {
                activeSfx->entryIndex = 0xFF;
                needNewSfx = true;
            } else {
                // Sfx is already playing as it should, nothing to do.
                for (j = 0; j < numChannels; j++) {
                    if (activeSfx->entryIndex == chosenSfx[j].entryIndex) {
                        chosenSfx[j].entryIndex = 0xFF;
                        j = numChannels;
                    }
                }

                numChosenSfx--;
            }
        }

        // enter the new sfx into the activeSfx
        if (needNewSfx == true) {
            for (j = 0; j < numChannels; j++) {
                chosenEntryIndex = chosenSfx[j].entryIndex;
                if ((chosenEntryIndex != 0xFF) &&
                    (sMMSfxBanks[bankId][chosenEntryIndex].state != MM_SFX_STATE_PLAYING_REFRESH)) {
                    for (k = 0; k < numChannels; k++) {
                        if (chosenEntryIndex == sMMActiveSfx[bankId][k].entryIndex) {
                            needNewSfx = false;
                            k = numChannels; // "break"
                        }
                    }

                    if (needNewSfx == true) {
                        activeSfx->entryIndex = chosenEntryIndex;
                        chosenSfx[j].entryIndex = 0xFF;
                        j = numChannels + 1;
                        numChosenSfx--;
                    }
                }
            }
            if (j == numChannels) {
                // nothing found
                activeSfx->entryIndex = 0xFF;
            }
        }
    }
}

static void MM_AudioSfx_SetProperties(u8 bankId, u8 entryIndex, u8 channelIndex);

// MM AudioSfx_PlayActiveSfx
static void MM_AudioSfx_PlayActiveSfx(u8 bankId) {
    u8 entryIndex;
    SequenceChannel* channel;
    MMSfxBankEntry* entry;
    u8 i;
    u8 ioPort5Data;

    for (i = 0; i < sMMChannelsPerBank[sMMSfxChannelLayout][bankId]; i++) {
        entryIndex = sMMActiveSfx[bankId][i].entryIndex;
        // If entry is not empty
        if (entryIndex != 0xFF) {
            entry = &sMMSfxBanks[bankId][entryIndex];
            channel = gMergeShipMMSeqPlayer.channels[sMMCurSfxPlayerChannelIndex];

            if (entry->state == MM_SFX_STATE_READY) {
                // Initialize a sfx (new sfx request)
                entry->channelIndex = sMMCurSfxPlayerChannelIndex;
                if (entry->sfxParams & MMSFX_FLAG_LOWER_VOLUME_BGM) {
                    MM_AudioSfx_LowerBgmVolume(sMMCurSfxPlayerChannelIndex);
                }

                // Add noise that will offset the frequency of the sfx
                if ((entry->sfxParams & MMSFX_PARAM_RAND_FREQ_RAISE_MASK) !=
                    (0 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT)) {
                    switch (entry->sfxParams & MMSFX_PARAM_RAND_FREQ_RAISE_MASK) {
                        case (1 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT):
                            entry->randFreq = Audio_NextRandom() & 0xF;
                            break;

                        case (2 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT):
                            entry->randFreq = Audio_NextRandom() & 0x1F;
                            break;

                        case (3 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT):
                            entry->randFreq = Audio_NextRandom() & 0x3F;
                            break;

                        default:
                            entry->randFreq = 0;
                            break;
                    }
                }

                // Calculate all the properties of sfx
                MM_AudioSfx_SetProperties(bankId, entryIndex, sMMCurSfxPlayerChannelIndex);

                MergeShipTest_Log("MMSFX start %04X bank=%d chan=%d font=%d", entry->sfxId, bankId,
                                  sMMCurSfxPlayerChannelIndex, (channel != NULL) ? channel->fontId : -1);
                // ioPort 0, enable the sfx to play in sequence 0
                MM_SetChannelIO(sMMCurSfxPlayerChannelIndex, 0, 1);

                // ioPort 4, write the lower bits sfx index for sequence 0 to find the right code to execute
                MM_SetChannelIO(sMMCurSfxPlayerChannelIndex, 4, entry->sfxId & 0xFF);

                // If the sfx bank has more than 255 entries, store the upper bits too
                if (sMMIsLargeSfxBank[bankId]) {
                    ioPort5Data = ((u8)((entry->sfxId & 0x300) >> 7) + (u8)((entry->sfxId & 0xFF) >> 7));
                } else {
                    ioPort5Data = 0;
                }

                if ((entry->sfxParams & MMSFX_FLAG_8) && (entry->freshness == 0x80)) {
                    ioPort5Data += 0x80;
                }

                if (sMMSetIOPort5[bankId]) {
                    // ioPort 5, upper bits of the sfx index and a flag
                    MM_SetChannelIO(sMMCurSfxPlayerChannelIndex, 5, ioPort5Data);
                }

                // Update playing state
                if (entry->sfxId & MMSFX_FLAG_MASK) {
                    entry->state = MM_SFX_STATE_PLAYING;
                } else {
                    entry->state = MM_SFX_STATE_PLAYING_ONE_FRAME;
                }
            } else if ((u8)channel->soundScriptIO[1] == (u8)MM_SEQ_IO_VAL_NONE) {
                // Signal from seq 0 that the sfx is finished playing. Remove entry
                MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
            } else if (entry->state == MM_SFX_STATE_PLAYING_REFRESH) {
                // Sfx is playing but a refresh is requested
                MM_AudioSfx_SetProperties(bankId, entryIndex, sMMCurSfxPlayerChannelIndex);

                if (entry->sfxId & MMSFX_FLAG_MASK) {
                    entry->state = MM_SFX_STATE_PLAYING;
                } else {
                    entry->state = MM_SFX_STATE_PLAYING_ONE_FRAME;
                }
            }
        }

        sMMCurSfxPlayerChannelIndex++;
    }
}

// MM AudioSfx_StopByPosAndBankImpl
static void MM_AudioSfx_StopByPosAndBankImpl(u8 bankId, Vec3f* pos) {
    MMSfxBankEntry* entry;
    u8 entryIndex = sMMSfxBanks[bankId][0].next;
    u8 prevEntryIndex = 0;

    while (entryIndex != 0xFF) {
        entry = &sMMSfxBanks[bankId][entryIndex];
        if (entry->posX == &pos->x) {
            if (entry->state >= MM_SFX_STATE_PLAYING_REFRESH) {
                MM_SetChannelIO(entry->channelIndex, 0, 0);
            }

            if (entry->state != MM_SFX_STATE_EMPTY) {
                MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
            }
        } else {
            prevEntryIndex = entryIndex;
        }

        entryIndex = sMMSfxBanks[bankId][prevEntryIndex].next;
    }
}

// MM AudioSfx_StopByPos
void MergeShip_MMSfx_StopByPos(Vec3f* pos) {
    u8 bankId;
    MMSfxBankEntry entryToRemove;

    MM_EnsureInitialized();
    for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
        MM_AudioSfx_StopByPosAndBankImpl(bankId, pos);
    }

    entryToRemove.posX = &pos->x;
    MM_AudioSfx_RemoveMatchingRequests(SFX_RM_REQ_BY_POS, &entryToRemove);
}

// MM AudioSfx_StopById
static void MM_AudioSfx_StopById(u32 sfxId) {
    MMSfxBankEntry* entry;
    u8 bankId = MMSFX_BANK(sfxId);
    u8 entryIndex;
    u8 prevEntryIndex = 0;
    MMSfxBankEntry entryToRemove;

    if (bankId >= ARRAY_COUNT(sMMSfxBanks)) {
        return;
    }
    MM_EnsureInitialized();
    entryIndex = sMMSfxBanks[bankId][0].next;

    while (entryIndex != 0xFF) {
        entry = &sMMSfxBanks[bankId][entryIndex];
        if (entry->sfxId == sfxId) {
            if (entry->state >= MM_SFX_STATE_PLAYING_REFRESH) {
                MM_SetChannelIO(entry->channelIndex, 0, 0);
            }
            if (entry->state != MM_SFX_STATE_EMPTY) {
                MM_AudioSfx_RemoveBankEntry(bankId, entryIndex);
            }
        } else {
            prevEntryIndex = entryIndex;
        }

        entryIndex = sMMSfxBanks[bankId][prevEntryIndex].next;
    }

    entryToRemove.sfxId = sfxId;
    MM_AudioSfx_RemoveMatchingRequests(SFX_RM_REQ_BY_ID, &entryToRemove);
}

// MM AudioSfx_ProcessRequests
static void MM_AudioSfx_ProcessRequests(void) {
    if (MM_SfxPlayerEnabled()) {
        while (sMMSfxRequestWriteIndex != sMMSfxRequestReadIndex) {
            MM_AudioSfx_ProcessRequest();
            sMMSfxRequestReadIndex++;
        }
    }
}

// MM AudioSfx_StepBankLerp
static void MM_AudioSfx_StepBankLerp(u8 bankId) {
    if (sMMSfxBankLerp[bankId].remainingFrames != 0) {
        sMMSfxBankLerp[bankId].remainingFrames--;

        if (sMMSfxBankLerp[bankId].remainingFrames != 0) {
            sMMSfxBankLerp[bankId].value -= sMMSfxBankLerp[bankId].step;
        } else {
            sMMSfxBankLerp[bankId].value = sMMSfxBankLerp[bankId].target;
        }
    }
}

// MM AudioSfx_ProcessActiveSfx
static void MM_AudioSfx_ProcessActiveSfx(void) {
    u8 bankId;

    if (MM_SfxPlayerEnabled()) {
        sMMCurSfxPlayerChannelIndex = 0;

        for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
            MM_AudioSfx_ChooseActiveSfx(bankId);
            MM_AudioSfx_PlayActiveSfx(bankId);
            MM_AudioSfx_StepBankLerp(bankId);
        }
    }
}

// MM AudioSfx_IsPlaying
u8 MergeShip_MMSfx_IsPlaying(u16 sfxId) {
    MMSfxBankEntry* entry;
    u8 bankId = MMSFX_BANK(sfxId);
    u8 entryIndex;

    if (bankId >= ARRAY_COUNT(sMMSfxBanks)) {
        return false;
    }
    MM_EnsureInitialized();
    entryIndex = sMMSfxBanks[bankId][0].next;

    while (entryIndex != 0xFF) {
        entry = &sMMSfxBanks[bankId][entryIndex];
        if (entry->sfxId == sfxId) {
            return true;
        }
        entryIndex = entry->next;
    }

    return false;
}

// MM AudioSfx_Reset
static void MM_AudioSfx_Reset(void) {
    u8 bankId;
    u8 i;

    sMMSfxRequestWriteIndex = 0;
    sMMSfxRequestReadIndex = 0;
    sMMSfxChannelLowVolumeFlag = 0;

    for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
        sMMSfxBankListEnd[bankId] = 0;
        sMMSfxBankFreeListStart[bankId] = 1;
        sMMSfxBankUnused[bankId] = 0;
        sMMSfxBankMuted[bankId] = false;
        sMMSfxBankLerp[bankId].value = 1.0f;
        sMMSfxBankLerp[bankId].remainingFrames = 0;
    }

    for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
        for (i = 0; i < MAX_CHANNELS_PER_BANK; i++) {
            sMMActiveSfx[bankId][i].entryIndex = 0xFF;
        }
    }

    for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
        sMMSfxBanks[bankId][0].prev = 0xFF;
        sMMSfxBanks[bankId][0].next = 0xFF;

        for (i = 1; i < sMMSfxBankSizes[bankId] - 1; i++) {
            sMMSfxBanks[bankId][i].prev = i - 1;
            sMMSfxBanks[bankId][i].next = i + 1;
        }

        sMMSfxBanks[bankId][i].prev = i - 1;
        sMMSfxBanks[bankId][i].next = 0xFF;
    }
}

// MM AudioSfx_ResetSfxChannelState
static void MM_AudioSfx_ResetSfxChannelState(void) {
    MMSfxChannelState* state;
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sMMSfxChannelState); i++) {
        state = &sMMSfxChannelState[i];
        state->volume = 1.0f;
        state->freqScale = 1.0f;
        state->reverb = 0;
        state->panSigned = 0x40;
        state->stereoBits = 0;
        state->filter = 0xFF;
        state->combFilterGain = 0xFF;
        state->zVolume = 0xFF;
    }

    sMMSfxChannelState[MM_SFX_CHANNEL_OCARINA].combFilterGain = 0;
}

// ---- MM code_8019AF00.c: the properties of an active sfx ------------------------------------------------

// MM AudioSfx_ComputeVolume
static f32 MM_AudioSfx_ComputeVolume(u8 bankId, u8 entryIndex) {
    MMSfxBankEntry* bankEntry = &sMMSfxBanks[bankId][entryIndex];
    f32 minDist;
    f32 phi_f14;
    f32 baseDist = 500.0f;
    f32 phi_f12;
    f32 volume;

    if (bankEntry->sfxParams & MMSFX_FLAG_VOLUME_NO_DIST) {
        return 1.0f;
    }

    if (bankEntry->dist > 10000.0f) {
        volume = 0.0f;
    } else {
        if (bankEntry->sfxParams & MMSFX_PARAM_DIST_RANGE_MASK_UPPER) {
            phi_f12 = 0.35f;
            phi_f14 = 0.65f;
        } else {
            phi_f12 = 0.81f;
            phi_f14 = 0.19f;
        }

        switch (bankEntry->sfxParams & MMSFX_PARAM_DIST_RANGE_MASK) {
            case 0:
                baseDist = 500.0f;
                break;

            case 1:
                baseDist = 666.6667f;
                break;

            case 2:
                baseDist = 952.381f;
                break;

            case 3:
                baseDist = 3846.154f;
                break;

            case 4:
                baseDist = 1000.0f;
                break;

            case 5:
                baseDist = 666.6667f;
                break;

            case 6:
                baseDist = 500.0f;
                break;

            case 7:
                baseDist = 400.0f;
                break;
        }

        minDist = baseDist / 5.0f;

        if (bankEntry->dist < minDist) {
            volume = 1.0f;
        } else if (bankEntry->dist < baseDist) {
            volume =
                ((((baseDist - minDist) - (bankEntry->dist - minDist)) / (baseDist - minDist)) * phi_f14) + phi_f12;
        } else {
            volume = (1.0f - ((bankEntry->dist - baseDist) / (10000.0f - baseDist))) * phi_f12;
        }

        volume = SQ(volume);
    }

    return volume;
}

// MM AudioSfx_ComputeReverb
static s8 MM_AudioSfx_ComputeReverb(u8 bankId, u8 entryIndex, u8 channelIndex) {
    s8 distAdd = 0;
    s32 scriptAdd = 0;
    MMSfxBankEntry* entry = &sMMSfxBanks[bankId][entryIndex];
    SequenceChannel* channel = gMergeShipMMSeqPlayer.channels[channelIndex];
    s32 reverb;

    if (!(entry->sfxParams & MMSFX_FLAG_REVERB_NO_DIST)) {
        if (entry->dist < 2500.0f) {
            distAdd = *entry->posZ > 0.0f ? (entry->dist / 2500.0f) * 70.0f : (entry->dist / 2500.0f) * 91.0f;
        } else {
            distAdd = 70;
        }
    }

    if (channel != NULL) {
        scriptAdd = channel->soundScriptIO[1];
        if (channel->soundScriptIO[1] <= MM_SEQ_IO_VAL_NONE) {
            scriptAdd = 0;
        }
    }

    reverb = (*entry->reverbAdd & 0x7F) + distAdd;

    if (entry->state != MM_SFX_STATE_READY) {
        reverb += scriptAdd;
    }

    if ((bankId != MM_BANK_OCARINA) || (MMSFX_INDEX(entry->sfxId) >= 2)) {
        reverb += sAudioEnvReverb + (sAudioCodeReverb & 0x3F) + sSpecReverb;
    }

    if (reverb > 0x7F) {
        reverb = 0x7F;
    }

    return reverb;
}

// MM AudioSfx_ComputePanSigned
static s8 MM_AudioSfx_ComputePanSigned(f32 x, f32 z, u8 token) {
    f32 absX = ABS_ALT(x);
    f32 absZ = ABS_ALT(z);
    f32 pan;

    if (absX > 8000.0f) {
        absX = 8000.0f;
    }

    if (absZ > 8000.0f) {
        absZ = 8000.0f;
    }

    if ((x == 0.0f) && (z == 0.0f)) {
        pan = 0.5f;
    } else if (absZ <= absX) {
        pan = (16000.0f - absX) / (4.5f * (16000.0f - absZ));
        if (x >= 0.0f) {
            pan = 1.0f - pan;
        }
    } else {
        pan = (x / (3.6f * absZ)) + 0.5f;
    }

    if ((absZ < 50.0f) && (absX < 50.0f)) {
        pan = ((pan - 0.5f) * SQ(absX / 50.0f)) + 0.5f;
    }

    return (pan * 127.0f) + 0.5f;
}

// MM AudioSfx_ComputeFreqScale
static f32 MM_AudioSfx_ComputeFreqScale(u8 bankId, u8 entryIndex) {
    s32 applyRandScaling = false;
    MMSfxBankEntry* entry = &sMMSfxBanks[bankId][entryIndex];
    f32 dist;
    f32 freq = 1.0f;

    if (entry->sfxParams & MMSFX_PARAM_RAND_FREQ_LOWER) {
        freq = 1.0f - ((gAudioContext.audioRandom & 0xF) / 192.0f);
    }

    switch (bankId) {
        case MM_BANK_VOICE:
            if (((entry->sfxId & 0xFF) < 0x40) && (sAudioBaseFilter2 != 0)) {
                applyRandScaling = true;
            } else if (((entry->sfxId & 0xFF) >= 0x40) && (sAudioExtraFilter2 != 0)) {
                applyRandScaling = true;
            }
            break;

        case MM_BANK_PLAYER:
        case MM_BANK_ITEM:
            if (sAudioBaseFilter2 != 0) {
                applyRandScaling = true;
            }
            break;

        case MM_BANK_ENV:
        case MM_BANK_ENEMY:
            if (((*entry->reverbAdd & 0x80) != 0) | (sAudioExtraFilter2 != 0)) {
                applyRandScaling = true;
            }
            break;

        case MM_BANK_SYSTEM:
        case MM_BANK_OCARINA:
            break;

        default:
            break;
    }

    if ((applyRandScaling == true) && !(entry->sfxParams & MMSFX_PARAM_RAND_FREQ_SCALE)) {
        freq *= 1.0293 - ((gAudioContext.audioRandom & 0xF) / 144.0f);
    }

    dist = entry->dist;
    if (!(entry->sfxParams & MMSFX_FLAG_VOLUME_NO_DIST) && !(entry->sfxParams & MMSFX_FLAG_FREQ_NO_DIST)) {
        if (dist >= 10000.0f) {
            freq += 0.2f;
        } else {
            freq += (0.2f * (dist / 10000.0f));
        }
    }

    if ((entry->sfxParams & MMSFX_PARAM_RAND_FREQ_RAISE_MASK) != (0 << MMSFX_PARAM_RAND_FREQ_RAISE_SHIFT)) {
        freq += (entry->randFreq / 192.0f);
    }

    return freq;
}

// MM AudioSfx_ComputeSurroundSoundFilter
static u8 MM_AudioSfx_ComputeSurroundSoundFilter(f32 behindScreenZ, MMSfxBankEntry* entry, s8 panSigned) {
    u8 lowPassCutoff;
    u8 highPassCutoff;
    u16* sfxParams;
    f32 distParam = 15.0f;
    f32 dist;

    // Remnant of OoT
    if (*entry->posZ < behindScreenZ) {
        lowPassCutoff = 0;
    } else {
        lowPassCutoff = 0;
    }

    sfxParams = &entry->sfxParams;

    if (*sfxParams & MMSFX_FLAG_SURROUND_LOWPASS_FILTER) {
        lowPassCutoff = 0xF;
    }

    switch ((*sfxParams & MMSFX_PARAM_DIST_RANGE_MASK)) {
        case 0:
            distParam = 15.0f;
            break;

        case 1:
            distParam = 12.0f;
            break;

        case 2:
            distParam = 9.0f;
            break;

        case 3:
            distParam = 6.0f;
            break;

        case 4:
            distParam = 18.0f;
            break;

        case 5:
            distParam = 21.0f;
            break;

        case 6:
            distParam = 24.0f;
            break;

        case 7:
            distParam = 27.0f;
            break;

        default:
            break;
    }

    if (!(entry->sfxFlags & MMSFX_FLAG2_SURROUND_NO_HIGHPASS_FILTER)) {
        if (entry->dist > 1923.077f) {
            dist = 1923.077f;
        } else {
            dist = entry->dist;
        }

        highPassCutoff = (distParam * dist) / (2 * 1923.077f);
    } else {
        highPassCutoff = 0;
    }

    (void)panSigned;
    return (lowPassCutoff << 4) + highPassCutoff;
}

// MM AudioSfx_ComputeCombFilter
static s8 MM_AudioSfx_ComputeCombFilter(f32 posY, u16 sfxParams) {
    s8 combFilterGain;

    if (posY < 0.0f) {
        if (posY < -625.0f) {
            combFilterGain = -32;
        } else {
            combFilterGain = (s8)(((625.0f + posY) / 625.0f) * 31.0f) + 0xE0;
        }
    } else if (posY > 1250.0f) {
        combFilterGain = 127;
    } else {
        combFilterGain = (posY / 1250.0f) * 126.0f;
    }

    (void)sfxParams;
    return combFilterGain | 1;
}

// MM AudioSfx_SetProperties
static void MM_AudioSfx_SetProperties(u8 bankId, u8 entryIndex, u8 channelIndex) {
    static s8 sZVolumes[] = {
        0,  2,  4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,  25,  26,  27,  28,  29,  30, 31, 32, 33,
        34, 35, 36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52, 53, 54, 55,
        56, 56, 57,  57,  58,  58,  59,  59,  60,  60,  60,  61,  61,  61,  62,  62,  62,  63,  63, 63, 64, 64,
        64, 65, 65,  65,  66,  66,  66,  67,  67,  67,  68,  68,  69,  69,  70,  70,  71,  71,  72, 73, 74, 75,
        76, 77, 78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94, 95, 96, 97,
        98, 99, 100, 101, 102, 103, 105, 107, 109, 111, 113, 115, 117, 119, 121, 123, 125, 127,
    };
    f32 volume = 1.0f;
    s8 volumeS8;
    s8 reverb = 0;
    f32 freqScale = 1.0f;
    s8 panSigned = 0x40;
    u8 filter = 0;
    s8 combFilterGain = 0;
    s8 zVolume = -1;
    f32 behindScreenZ = 0.0f;
    u8 baseFilter = 0;
    MMSfxBankEntry* entry = &sMMSfxBanks[bankId][entryIndex];
    s32 surround = (sSoundOutputMode == MM_SOUNDMODE_SURROUND_EXTERNAL);

    switch (bankId) {
        case MM_BANK_PLAYER:
        case MM_BANK_ITEM:
        case MM_BANK_ENV:
        case MM_BANK_ENEMY:
        case MM_BANK_VOICE:
            if (surround) {
                combFilterGain = MM_AudioSfx_ComputeCombFilter(*entry->posY, entry->sfxParams);
            }
            // fallthrough
        case MM_BANK_OCARINA:
            entry->dist = sqrtf(entry->dist * 10.0f);
            volume = MM_AudioSfx_ComputeVolume(bankId, entryIndex) * *entry->volume;
            reverb = MM_AudioSfx_ComputeReverb(bankId, entryIndex, channelIndex);
            panSigned = MM_AudioSfx_ComputePanSigned(*entry->posX, *entry->posZ, entry->token);
            freqScale = MM_AudioSfx_ComputeFreqScale(bankId, entryIndex) * *entry->freqScale;

            if (surround) {
                if (*entry->posZ >= 0.0f) {
                    if (*entry->posZ < 200.0f) {
                        zVolume = 32.0f - ((*entry->posZ / 300.0f) * 32.0f);
                    } else {
                        zVolume = 0;
                    }
                } else if (*entry->posZ < 0.0f) {
                    if (*entry->posZ > -300.0f) {
                        zVolume = 32.0f - ((*entry->posZ / 300.0f) * 95.0f);
                    } else {
                        zVolume = 127;
                    }
                }
                zVolume = sZVolumes[(u8)zVolume & 0x7F];
            }

            if (sAudioBaseFilter != 0) {
                if ((bankId == MM_BANK_PLAYER) || (bankId == MM_BANK_ITEM) ||
                    ((bankId == MM_BANK_VOICE) && (((entry->sfxId & 0xFF) < 0x40) || ((entry->sfxId & 0xFF) >= 0x80)))) {
                    baseFilter = sAudioBaseFilter;
                    if (sAudioExtraFilter) {
                        reverb >>= 2;
                    }
                }
            }

            if (((*entry->reverbAdd & 0x80) | baseFilter | sAudioExtraFilter |
                 (entry->sfxFlags & MMSFX_FLAG2_APPLY_LOWPASS_FILTER)) != 0) {
                filter = ((((*entry->reverbAdd & 0x80) >> 2) | baseFilter | sAudioExtraFilter |
                           ((entry->sfxFlags & MMSFX_FLAG2_APPLY_LOWPASS_FILTER) >> 2)) *
                          2);
                filter &= 0xFF;
            } else if (surround && !(entry->sfxParams & MMSFX_FLAG_VOLUME_NO_DIST)) {
                filter = MM_AudioSfx_ComputeSurroundSoundFilter(behindScreenZ, entry, panSigned);
            }
            break;

        case MM_BANK_SYSTEM:
            break;
    }

    if (sMMSfxChannelState[channelIndex].volume != volume) {
        volumeS8 = (u8)(volume * 127.0f);
        sMMSfxChannelState[channelIndex].volume = volume;
    } else {
        volumeS8 = -1;
    }

    // ioPort 2, sets volume
    MM_SetChannelIO(channelIndex, 2, volumeS8);

    if (sMMSfxChannelState[channelIndex].reverb != reverb) {
        sMMSfxChannelState[channelIndex].reverb = reverb;
    }
    if (sMMSfxChannelState[channelIndex].freqScale != freqScale) {
        sMMSfxChannelState[channelIndex].freqScale = freqScale;
    }
    if (sMMSfxChannelState[channelIndex].stereoBits != 0) {
        sMMSfxChannelState[channelIndex].stereoBits = 0;
    }
    if (sMMSfxChannelState[channelIndex].filter != filter) {
        sMMSfxChannelState[channelIndex].filter = filter;
    }
    if (sMMSfxChannelState[channelIndex].combFilterGain != (u8)combFilterGain) {
        sMMSfxChannelState[channelIndex].combFilterGain = combFilterGain;
    }
    if (sMMSfxChannelState[channelIndex].zVolume != (u8)zVolume) {
        sMMSfxChannelState[channelIndex].zVolume = zVolume;
    }
    if (sMMSfxChannelState[channelIndex].panSigned != panSigned) {
        sMMSfxChannelState[channelIndex].panSigned = panSigned;
    }
}

// MM AudioSfx_SetFreqAndStereoBits (sequence 0's custom function 0, called on the audio thread).
u32 MergeShip_MMSfx_SetFreqAndStereoBits(s8 seqScriptValIn, SequenceChannel* channel) {
    u8 index = (u8)seqScriptValIn & 0xF;

    channel->stereo.asByte = sMMSfxChannelState[index].stereoBits;
    channel->freqScale = sMMSfxChannelState[index].freqScale;
    channel->changes.s.freqScale = true;

    return (u8)seqScriptValIn;
}

u8* MergeShip_MMSfx_GetChannelState(s32 channelIndex) {
    return (u8*)&sMMSfxChannelState[channelIndex & 0xF];
}

// ---- MM's Audio_PlaySfx_* wrappers ----------------------------------------------------------------------

// MM Audio_SetSyncedSfxFreqAndVolume
static f32 MM_Audio_SetSyncedSfxFreqAndVolume(f32 freqVolParam) {
    f32 ret = 1.0f;

    if (freqVolParam > 6.0f) {
        sMMSfxSyncedVolume = 1.0f;
        sMMSfxSyncedFreq = 1.1f;
    } else {
        ret = freqVolParam / 6.0f;
        sMMSfxSyncedVolume = ret * (1.0f - 0.775f) + 0.775f;
        sMMSfxSyncedFreq = (ret * 0.2f) + 0.9f;
    }

    return ret;
}

// MM Audio_PlaySfx_AtPosForMetalEffectsWithSyncedFreqAndVolume
void MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume(Vec3f* pos, u16 sfxId, f32 freqVolParam) {
    f32 sp2C;
    f32 phi_f0;
    u8 phi_v0;
    u16 metalSfxId = 0;

    sp2C = MM_Audio_SetSyncedSfxFreqAndVolume(freqVolParam);
    MergeShip_MMSfx_PlaySfx(sfxId, pos, 4, &sMMSfxSyncedFreq, &sMMSfxSyncedVolume, &gSfxDefaultReverb);

    if ((sfxId & 0xF0) == 0xB0) {
        // Crawlspaces (unused remnant of OoT)
        phi_f0 = 0.3f;
        phi_v0 = true;
        sp2C = 1.0f;
    } else {
        phi_f0 = 1.1f;
        phi_v0 = gAudioContext.audioRandom & 1;
    }

    if (phi_f0 < freqVolParam) {
        if (phi_v0) {
            if ((sfxId & 0x1FF) < 0x80) {
                metalSfxId = MM_NA_SE_PL_METALEFFECT_KID;
            } else if ((sfxId & 0x1FF) < 0xF0) {
                metalSfxId = MM_NA_SE_PL_METALEFFECT_ADULT;
            }

            if (metalSfxId != 0) {
                sMMSfxSyncedVolumeForMetalEffects = (sp2C * 0.7) + 0.3;
                MergeShip_MMSfx_PlaySfx(metalSfxId, pos, 4, &sMMSfxSyncedFreq, &sMMSfxSyncedVolumeForMetalEffects,
                                        &gSfxDefaultReverb);
            }
        }
    }
}

// MM Audio_PlaySfx_AtPosWithSyncedFreqAndVolume
void MergeShip_MMSfx_PlayAtPosWithSyncedFreqAndVolume(Vec3f* pos, u16 sfxId, f32 freqVolParam) {
    MM_Audio_SetSyncedSfxFreqAndVolume(freqVolParam);
    MergeShip_MMSfx_PlaySfx(sfxId, pos, 4, &sMMSfxSyncedFreq, &sMMSfxSyncedVolume, &gSfxDefaultReverb);
}

// MergeShip2: MM Audio_PlaySfx_AtPosWithAllChannelsIO -- the Bremen Mask's chick join chime (MergeShipBremen.c).
void MergeShip_MMSfx_PlayAtPosWithAllChannelsIO(Vec3f* pos, u16 sfxId, u8 ioData) {
    u8 channelIndex = 0;
    u8 i;
    u8 bankId = MMSFX_BANK_SHIFT(sfxId);

    for (i = 0; i < bankId; i++) {
        channelIndex += sMMChannelsPerBank[sMMSfxChannelLayout][i];
    }

    for (i = 0; i < sMMChannelsPerBank[sMMSfxChannelLayout][bankId]; i++) {
        MM_SetChannelIO(channelIndex++, 6, ioData);
    }

    MergeShip_MMSfx_PlaySfx(sfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                            &gSfxDefaultReverb);
}

// ---- the entry points game code calls (MergeShipTransformFx.h) ------------------------------------------

// MM Actor_PlaySfx / Player_PlaySfx (Audio_PlaySfx_AtPos on the actor's projected position).
void MergeShip_PlayMMSfxActor(Actor* actor, u16 mmSfxId) {
    MergeShip_MMSfx_PlaySfx(mmSfxId, &actor->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                            &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

// MM Audio_PlaySfx_AtPos
void MergeShip_PlayMMSfxAtPos(Vec3f* pos, u16 mmSfxId) {
    MergeShip_MMSfx_PlaySfx(mmSfxId, pos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                            &gSfxDefaultReverb);
}

// MM Audio_PlaySfx / Audio_PlaySfx_2 / Lib_PlaySfx_2 (the screen's centre: no distance, no pan).
void MergeShip_PlayMMSfxCentered(u16 mmSfxId) {
    MergeShip_MMSfx_PlaySfx(mmSfxId, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                            &gSfxDefaultReverb);
}

// MM AudioSfx_StopById
void MergeShip_StopMMSfxById(u16 mmSfxId) {
    MM_AudioSfx_StopById(mmSfxId);
}

// MM Player_AnimSfx_PlayFloorWalk (the Giant's Mask branch has no Hyrule counterpart).
void MergeShip_PlayMMFloorWalkSfx(Player* player, f32 freqVolumeLerp) {
    s32 sfxId = MM_NA_SE_PL_WALK_GROUND + player->floorSfxOffset + player->ageProperties->surfaceSfxIdOffset;

    // BUG-0091: the Iron Boots are Ocarina of Time's: their clank is Ocarina of Time's own (MM has none).
    if (player->currentBoots == PLAYER_BOOTS_IRON) {
        func_800F4010(&player->actor.projectedPos, NA_SE_PL_WALK_HEAVYBOOTS, freqVolumeLerp);
        return;
    }

    MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume(&player->actor.projectedPos, sfxId,
                                                                     freqVolumeLerp);
}

// ---- Ocarina of Time -> Majora's Mask sound ids, for the forms ------------------------------------------

u16 MergeShip_OoTToMMSfxId(u16 ootSfxId) {
    u16 key = ootSfxId | 0x800; // the generated table holds the SFX_FLAG forms of the ids

    // Where MM plays a different sound for the same action than the one sharing Ocarina of Time's name, use MM's.
    // Climbing a vine: Ocarina of Time plays NA_SE_PL_WALK_WALL (0x80C); MM plays its 0x80C too (named
    // NA_SE_PL_WALK_METAL1 there, MM z_player.c's climb), not MM's NA_SE_PL_WALK_WALL (0x9CE, a different sound).
    if (key == 0x80C) {
        return ootSfxId;
    }
    s32 lo = 0;
    s32 hi = ARRAY_COUNT(sOoTToMMSfxIds) - 1;

    while (lo <= hi) {
        s32 mid = (lo + hi) / 2;

        if (sOoTToMMSfxIds[mid][0] == key) {
            u16 mmSfxId = sOoTToMMSfxIds[mid][1];

            // A one-frame request (`- SFX_FLAG`) stays one-frame.
            if (!(ootSfxId & 0x800)) {
                mmSfxId &= ~0x800;
            }
            return mmSfxId;
        } else if (sOoTToMMSfxIds[mid][0] < key) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return 0;
}

static void MM_PlayerPlaySfx(Player* player, u16 mmSfxId) {
    // MergeShip2: Ganondorf mode -- his voice for MM's young Link's (MergeShipGanondorf.h)
    if (MergeShip_Ganondorf_PlayVoice(&player->actor, mmSfxId)) {
        return;
    }
    // MM Player_PlaySfx (z_actor.c).
    // MergeShip2: MM Player_PlaySfx's Giant's Mask branch, Audio_PlaySfx_AtPosWithPresetLowFreqAndHighReverb.
    if (player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT)) {
        static f32 sTwoSemitonesLoweredFreq = 0.9f; // MM sTwoSemitonesLoweredFreq (Audio_ResetSfx)
        static s8 sSfxIncreasedReverb = 20;         // MM sSfxIncreasedReverb

        MergeShip_MMSfx_PlaySfx(mmSfxId, &player->actor.projectedPos, 4, &sTwoSemitonesLoweredFreq,
                                &gSfxDefaultFreqAndVolScale, &sSfxIncreasedReverb);
        return;
    }
    MergeShip_MMSfx_PlaySfx(mmSfxId, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                            &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
}

s32 MergeShip_TryPlayMMPlayerSfx(Actor* actor, u16 ootSfxId) {
    Player* player = (Player*)actor;
    u16 mmSfxId;

    if ((actor == NULL) || (actor->id != ACTOR_PLAYER) || !MERGESHIP_PLAYER_SOUNDS_MM(player)) {
        return false;
    }
    // BUG-0091: the Iron and Hover Boots are Ocarina of Time's, and so are their sounds (MM has none of its own).
    switch (ootSfxId | 0x800) {
        case NA_SE_PL_WALK_HEAVYBOOTS:
        case NA_SE_PL_JUMP_HEAVYBOOTS:
        case NA_SE_PL_LAND_HEAVYBOOTS:
        case NA_SE_PL_HOBBERBOOTS_LV:
            return false;
        default:
            break;
    }

    // A Majora's Mask form sounds like Majora's Mask; a sound MM has no counterpart for stays silent.
    mmSfxId = MergeShip_OoTToMMSfxId(ootSfxId);
    if (MERGESHIP_MM_CHILD(player)) {
        // MergeShip2: MM's human as the child. Ocarina of Time's child voice bank (a voice id already offset by
        // its voiceSfxIdOffset, 0x20) is MM's human bank at the same offset (MM sPlayerAgeProperties[HUMAN]).
        u16 flagged = ootSfxId | 0x800;

        if ((mmSfxId == 0) && (flagged >= (NA_SE_VO_LI_SWORD_N | 0x800) + SFX_VOICE_BANK_SIZE) &&
            (flagged < (NA_SE_VO_LI_SWORD_N | 0x800) + 2 * SFX_VOICE_BANK_SIZE)) {
            mmSfxId = MergeShip_OoTToMMSfxId(ootSfxId - SFX_VOICE_BANK_SIZE);
            if (mmSfxId != 0) {
                mmSfxId += SFX_VOICE_BANK_SIZE;
            }
        }
        // The child keeps Ocarina of Time's sound for what MM's human never makes (its slingshot, boomerang...).
        if (mmSfxId == 0) {
            return false;
        }
    }
    if (mmSfxId != 0) {
        MM_PlayerPlaySfx(player, mmSfxId);
    }
    return true;
}

// MM Player_AnimSfx_PlayVoice (the Giant's Mask / Mask of Scents branches have no Hyrule counterpart).
void MergeShip_PlayOoTVoiceSfxAsMM(Player* player, u16 ootSfxId) {
    // MergeShip2: Ganondorf mode -- his voice, not MM's young Link's (MergeShipGanondorf.h)
    if (MergeShip_Ganondorf_PlayVoice(&player->actor, ootSfxId)) {
        return;
    }
    u16 mmSfxId = MergeShip_OoTToMMSfxId(ootSfxId);

    if ((mmSfxId != 0) && (player->actor.id == ACTOR_PLAYER)) {
        // MM: the Mask of Scents speaks with its own voice bank (SFX_VOICE_BANK_SIZE * 7).
        u16 offset = (player->currentMask == PLAYER_MASK_MM_REGULAR(MM_MASK_SCENTS)) ? (SFX_VOICE_BANK_SIZE * 7)
                                                                                     : player->ageProperties->voiceSfxIdOffset;
        MM_PlayerPlaySfx(player, mmSfxId + offset);
    } else if ((mmSfxId == 0) && MERGESHIP_MM_CHILD(player)) {
        // MergeShip2: a voice clip MM's human does not have: the child keeps Ocarina of Time's.
        Audio_PlaySfxGeneral(ootSfxId + player->ageProperties->voiceSfxIdOffset, &player->actor.projectedPos, 4,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

// MM func_8082E00C's AudioSfx_StopById for one voice sound.
void MergeShip_StopOoTVoiceSfxAsMM(Player* player, u16 ootSfxId) {
    u16 mmSfxId = MergeShip_OoTToMMSfxId(ootSfxId);

    if (mmSfxId != 0) {
        MM_AudioSfx_StopById((u16)(mmSfxId + player->ageProperties->voiceSfxIdOffset));
    }
}

// MM Player_AnimSfx_PlayFloor (byAge false) / Player_AnimSfx_PlayFloorByAge (byAge true).
void MergeShip_PlayOoTFloorSfxAsMM(Player* player, u16 ootSfxId, s32 byAge) {
    u16 mmSfxId = MergeShip_OoTToMMSfxId(ootSfxId);

    if (mmSfxId != 0) {
        mmSfxId += player->floorSfxOffset;
        if (byAge) {
            mmSfxId += player->ageProperties->surfaceSfxIdOffset;
        }
        MM_PlayerPlaySfx(player, mmSfxId);
    }
}

// MM Player_AnimSfx_PlayFloorJump
void MergeShip_PlayMMJumpingSfx(Player* player) {
    if (player->currentBoots == PLAYER_BOOTS_IRON) { // BUG-0091: Ocarina of Time's Iron Boots
        Audio_PlaySfxGeneral(NA_SE_PL_JUMP_HEAVYBOOTS, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    MM_PlayerPlaySfx(player,
                     MM_NA_SE_PL_JUMP_GROUND + player->floorSfxOffset + player->ageProperties->surfaceSfxIdOffset);
}

// MM Player_AnimSfx_PlayFloorLand
void MergeShip_PlayMMLandingSfx(Player* player) {
    if (player->currentBoots == PLAYER_BOOTS_IRON) { // BUG-0091: Ocarina of Time's Iron Boots
        Audio_PlaySfxGeneral(NA_SE_PL_LAND_HEAVYBOOTS, &player->actor.projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        return;
    }
    MM_PlayerPlaySfx(player,
                     MM_NA_SE_PL_LAND_GROUND + player->floorSfxOffset + player->ageProperties->surfaceSfxIdOffset);
}

// z_player.c's AnimSfxEntry and ANIMSFX_* (its layout is fixed by the player's animation-sfx tables).
typedef struct {
    /* 0x00 */ u16 sfxId;
    /* 0x02 */ s16 data;
} MMOoTAnimSfxEntry;

#define ANIMSFX_SHIFT_TYPE(type) ((type) << 11)
#define ANIMSFX_GET_TYPE(data) ((data)&0x7800)
#define ANIMSFX_GET_FRAME(data) ((data)&0x7FF)

// MM Player_PlayAnimSfx for an Ocarina of Time list (types have the same numbers in both games).
void MergeShip_PlayOoTAnimSfxListAsMM(Player* player, const void* entries) {
    const MMOoTAnimSfxEntry* entry = (const MMOoTAnimSfxEntry*)entries;
    s32 cont;

    do {
        s32 data = ABS(entry->data);
        s32 type = ANIMSFX_GET_TYPE(data);

        if (LinkAnimation_OnFrame(&player->skelAnime, fabsf(ANIMSFX_GET_FRAME(data)))) {
            if (type == ANIMSFX_SHIFT_TYPE(1)) { // GENERAL (MM Player_PlaySfx)
                u16 mmSfxId = MergeShip_OoTToMMSfxId(entry->sfxId);

                if (mmSfxId != 0) {
                    MM_PlayerPlaySfx(player, mmSfxId);
                } else if (MERGESHIP_MM_CHILD(player)) {
                    Player_PlaySfx(&player->actor, entry->sfxId); // MergeShip2: none in MM: the child keeps OoT's
                }
            } else if (type == ANIMSFX_SHIFT_TYPE(2)) { // FLOOR
                MergeShip_PlayOoTFloorSfxAsMM(player, entry->sfxId, false);
            } else if (type == ANIMSFX_SHIFT_TYPE(3)) { // FLOOR_BY_AGE
                MergeShip_PlayOoTFloorSfxAsMM(player, entry->sfxId, true);
            } else if (type == ANIMSFX_SHIFT_TYPE(4)) { // VOICE
                MergeShip_PlayOoTVoiceSfxAsMM(player, entry->sfxId);
            } else if (type == ANIMSFX_SHIFT_TYPE(5)) { // FLOOR_LAND
                MergeShip_PlayMMLandingSfx(player);
            } else if (type == ANIMSFX_SHIFT_TYPE(6)) { // MM ANIMSFX_TYPE_6
                MergeShip_PlayMMFloorWalkSfx(player, 6.0f);
            } else if (type == ANIMSFX_SHIFT_TYPE(7)) { // FLOOR_JUMP
                MergeShip_PlayMMJumpingSfx(player);
            } else if (type == ANIMSFX_SHIFT_TYPE(8)) { // MM ANIMSFX_TYPE_8
                MergeShip_PlayMMFloorWalkSfx(player, 0.0f);
            } else if (type == ANIMSFX_SHIFT_TYPE(9)) { // MM ANIMSFX_TYPE_9
                MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume(
                    &player->actor.projectedPos, player->ageProperties->surfaceSfxIdOffset + MM_NA_SE_PL_WALK_LADDER,
                    0.0f);
            }
        }

        cont = (entry->data >= 0);
        entry++;
    } while (cont);
}

// ---- MM's ocarina instruments for the forms --------------------------------------------------------------

// MM z_message.c sPlayerFormOcarinaInstruments, by Ocarina of Time's PlayerForm. The human forms (and the
// Fierce Deity, MM's form 0) keep the ocarina.
static u8 MM_FormOcarinaInstrument(u8 playerForm) {
    switch (playerForm) {
        case PLAYER_FORM_DEKU:
            return MM_OCARINA_INSTRUMENT_DEKU_PIPES;
        case PLAYER_FORM_GORON:
            return MM_OCARINA_INSTRUMENT_GORON_DRUMS;
        case PLAYER_FORM_ZORA:
            return MM_OCARINA_INSTRUMENT_ZORA_GUITAR;
        default:
            return MM_OCARINA_INSTRUMENT_OFF;
    }
}

static u8 sMMFormOcarinaInstrument = MM_OCARINA_INSTRUMENT_OFF; // the form's instrument
static u8 sMMOcarinaInstrument = MM_OCARINA_INSTRUMENT_OFF;     // the instrument sequence 0 plays now
static u8 sOoTOcarinaInstrument = 0;                            // what Ocarina of Time last selected

static void MM_ApplyOcarinaInstrument(void) {
    u8 instrument = MM_OCARINA_INSTRUMENT_OFF;

    // Ocarina of Time selects its default instrument where Majora's Mask's message code selects the form's.
    if ((sOoTOcarinaInstrument == 1 /* OCARINA_INSTRUMENT_DEFAULT */) && !sMMAudioUnavailable) {
        instrument = sMMFormOcarinaInstrument;
    }

    if (instrument == sMMOcarinaInstrument) {
        return;
    }

    if (sMMOcarinaInstrument != MM_OCARINA_INSTRUMENT_OFF) {
        MM_AudioSfx_StopById(MM_NA_SE_OC_OCARINA);
    }
    sMMOcarinaInstrument = instrument;
    // MM AudioOcarina_SetInstrument: SEQCMD_SET_CHANNEL_IO(SEQ_PLAYER_SFX, SFX_CHANNEL_OCARINA, 1, instrument)
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 1, instrument);
}

void MergeShip_SetFormOcarinaInstrument(u8 playerForm) {
    sMMFormOcarinaInstrument = MM_FormOcarinaInstrument(playerForm);
    MM_ApplyOcarinaInstrument();
}

void MergeShip_MMOcarina_OnSetInstrument(u8 ootInstrumentId) {
    Player* player;

    sOoTOcarinaInstrument = ootInstrumentId;
    // Whoever starts the ocarina (the player's ocarina action, a song prompt, a cutscene), the instrument is
    // the one of the form playing it.
    if ((gPlayState != NULL) && ((player = GET_PLAYER(gPlayState)) != NULL)) {
        sMMFormOcarinaInstrument = MM_FormOcarinaInstrument(player->mergeShipForm);
    }
    MM_ApplyOcarinaInstrument();
}

s32 MergeShip_MMOcarina_IsActive(void) {
    return sMMOcarinaInstrument != MM_OCARINA_INSTRUMENT_OFF;
}

// MM AudioOcarina_PlayControllerInput / AudioOcarina_PlaybackSong note start.
void MergeShip_MMOcarina_PlayNote(u8 pitch, f32* freqScale, f32* volume) {
    // Sets ocarina instrument Id to io port 7, which is used as an index in seq 0 to get the true instrument Id
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 7, sMMOcarinaInstrument - 1);
    // Sets pitch to io port 5
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 5, pitch);
    MergeShip_MMSfx_PlaySfx(MM_NA_SE_OC_OCARINA, &gSfxDefaultPos, 4, freqScale, volume, &gSfxDefaultReverb);
    MergeShipTest_Log("MMOCA note pitch=%d MM instrument=%d (io7=%d) banks muted=%d", pitch, sMMOcarinaInstrument,
                      sMMOcarinaInstrument - 1, gSoundBankMuted[5]);
}

// Another player's note (Anchor): played on MM's ocarina channel with that player's form's instrument, at that
// player's position; the local player's own instrument is put back when the note ends.
static u8 sMMRemoteInstrument = MM_OCARINA_INSTRUMENT_OFF;

s32 MergeShip_MMOcarina_FormHasInstrument(u8 playerForm) {
    return (MM_FormOcarinaInstrument(playerForm) != MM_OCARINA_INSTRUMENT_OFF) && MergeShip_IsMajorasMaskMounted() &&
           !sMMAudioUnavailable && !gMergeShipMMAudioUnavailable;
}

void MergeShip_MMOcarina_PlayRemoteNote(u8 playerForm, u8 pitch, Vec3f* pos, f32* freqScale, f32* volume) {
    u8 instrument = MM_FormOcarinaInstrument(playerForm);

    if (instrument == MM_OCARINA_INSTRUMENT_OFF) {
        return;
    }
    MM_EnsureInitialized();
    if (sMMRemoteInstrument != MM_OCARINA_INSTRUMENT_OFF) {
        MM_AudioSfx_StopById(MM_NA_SE_OC_OCARINA);
    }
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 1, instrument);
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 7, instrument - 1);
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 5, pitch);
    MergeShip_MMSfx_PlaySfx(MM_NA_SE_OC_OCARINA, pos, 4, freqScale, volume, &gSfxDefaultReverb);
    sMMRemoteInstrument = instrument;
}

void MergeShip_MMOcarina_StopRemoteNote(void) {
    if (sMMRemoteInstrument == MM_OCARINA_INSTRUMENT_OFF) {
        return;
    }
    MM_AudioSfx_StopById(MM_NA_SE_OC_OCARINA);
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 1, sMMOcarinaInstrument);
    sMMRemoteInstrument = MM_OCARINA_INSTRUMENT_OFF;
}

void MergeShip_MMOcarina_SetVibrato(u8 vibrato) {
    // Sets vibrato to io port 6
    MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 6, vibrato);
}

void MergeShip_MMOcarina_Stop(void) {
    if (sMMOcarinaInstrument != MM_OCARINA_INSTRUMENT_OFF) {
        MM_AudioSfx_StopById(MM_NA_SE_OC_OCARINA);
    }
}

// ---- per frame -----------------------------------------------------------------------------------------

static u32 sMMLastAudioResetCount = 0;
static s32 sMMStartCooldown = 0;
static s32 sMMIdleFrames = 0;

// Frames without any MM sound before the MM sequence player is stopped again, so that Ocarina of Time's
// audio is exactly as before whenever Majora's Mask's sounds are not in use.
#define MM_IDLE_STOP_FRAMES (20 * 60)

static s32 MM_HasPendingOrActiveSfx(void) {
    if (sMMRemoteInstrument != MM_OCARINA_INSTRUMENT_OFF) {
        return true;
    }
    u8 bankId;

    if (sMMSfxRequestWriteIndex != sMMSfxRequestReadIndex) {
        return true;
    }
    for (bankId = 0; bankId < ARRAY_COUNT(sMMSfxBanks); bankId++) {
        if (sMMSfxBanks[bankId][0].next != 0xFF) {
            return true;
        }
    }
    return sMMOcarinaInstrument != MM_OCARINA_INSTRUMENT_OFF;
}

void MergeShip_MMSfx_Reset(void) {
    MM_AudioSfx_Reset();
    MM_AudioSfx_ResetSfxChannelState();
    MM_Audio_SetSyncedSfxFreqAndVolume(0.0f);
    sMMOcarinaInstrument = MM_OCARINA_INSTRUMENT_OFF;
    sMMSfxInitialized = true;
}

static void MM_EnsureInitialized(void) {
    if (!sMMSfxInitialized) {
        MergeShip_MMSfx_Reset();
    }
}

void MergeShip_MMSfx_Update(void) {
    u32 resetCount = gMergeShipMMAudioResetCount;

    MM_EnsureInitialized();

    if (!MergeShip_IsMajorasMaskMounted() || gMergeShipMMAudioUnavailable) {
        if (!sMMAudioUnavailable) {
            sMMAudioUnavailable = true;
            MM_AudioSfx_Reset();
        }
        return;
    }

    // An audio reset rebuilt the notes and restarted every player: MM's banks start over, as MM's
    // Audio_ResetForAudioHeapStep1 resets them.
    if (resetCount != sMMLastAudioResetCount) {
        sMMLastAudioResetCount = resetCount;
        MM_AudioSfx_Reset();
        MM_AudioSfx_ResetSfxChannelState();
        sMMOcarinaInstrument = MM_OCARINA_INSTRUMENT_OFF;
        sMMStartCooldown = 0;
    }

    if (MM_HasPendingOrActiveSfx()) {
        sMMIdleFrames = 0;
    } else if (sMMIdleFrames < MM_IDLE_STOP_FRAMES) {
        sMMIdleFrames++;
    }

    // The commands are asynchronous: a cooldown keeps one start (or stop) in flight at a time.
    if (sMMStartCooldown > 0) {
        sMMStartCooldown--;
    } else if (!MM_SfxPlayerEnabled()) {
        // MM AudioSfx_Init: start sequence 0 (fade-in 1) and hand the channels their SfxChannelState (the audio
        // thread does the latter as it starts the sequence).
        if (sMMIdleFrames == 0) {
            MM_AudioSfx_ResetSfxChannelState();
            Audio_QueueCmdS32(0x70000000 | (MERGESHIP_MM_SEQ_PLAYER << 16), 1);
            // Starting the sequence resets the channels' io ports: restate the ocarina instrument.
            if (sMMOcarinaInstrument != MM_OCARINA_INSTRUMENT_OFF) {
                MM_SetChannelIO(MM_SFX_CHANNEL_OCARINA, 1, sMMOcarinaInstrument);
            }
            sMMStartCooldown = 30;
        }
    } else if (sMMIdleFrames >= MM_IDLE_STOP_FRAMES) {
        Audio_QueueCmdS32(0x71000000 | (MERGESHIP_MM_SEQ_PLAYER << 16), 0);
        sMMStartCooldown = 30;
    }

    // MM Audio_Update order: AudioSfx_ProcessRequests, ..., AudioSfx_ProcessActiveSfx.
    MM_AudioSfx_ProcessRequests();
    MM_AudioSfx_ProcessActiveSfx();
}
