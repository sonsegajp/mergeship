// MergeShip2 -- the audio-thread half of Majora's Mask's sound effects inside Ocarina of Time.
//
// A fifth sequence player runs MM's sequence 0 (the sfx sequence, mm/audio/sequences/Sequence_0) with MM's
// sequence-script interpreter, ported from 2ship2harkinian/mm/src/audio/lib/seqplayer.c (every function below
// names its MM original). MM's sequence 0 cannot run on Ocarina of Time's interpreter: it uses the commands MM
// added or changed (channel 0xA0-0xA8 read/write the per-channel SfxChannelState, 0xBD is MM's start sample
// position where OoT has a random-range command with a different argument count, 0xBE calls MM's seq-0 custom
// function, layer 0xF0/0xF1, seq 0xC2/0xC3), and MM's sfx code drives it through that SfxChannelState.
//
// The player, its 16 channels and its 64 layers live here, outside gAudioContext, so Ocarina of Time's four
// players, channels and layer pool are untouched. Notes are the one shared resource (the synthesizer mixes
// gAudioContext.notes), exactly as MM's own sequence players share them.
//
// Field names: this is MM code written against Ocarina of Time's structs. MM -> OoT:
//   channel: muted -> stopSomething2, muteFlags -> muteBehavior, targetReverbVol -> reverb, gain -> unk_0C,
//            combFilterSize -> unk_0F, combFilterGain -> unk_20, vibrato.vibratoDepth* -> vibratoExtent*,
//            vibrato.vibratoRate* -> vibratoRate*, seqScriptIO -> soundScriptIO, stereoData -> stereo,
//            adsr.decayIndex -> adsr.releaseRate; MM-only fields (sfxState, startSamplePos, unk_E0,
//            surroundEffectIndex, channelIndex) in sMMChannelExt.
//   layer:   muted -> stopSomething, tunedSample -> sound, bend -> unk_34, stereoData -> stereo; MM-only fields
//            (unk_0A, surroundEffectIndex, targetReverbVol) in sMMLayerExt.
//   player:  muteFlags -> muteBehavior, tempoChange -> unk_0C, storedFadeTimer -> fadeTimerUnkEu,
//            applyBend -> unk_0b1, bend -> unk_34, seqScriptIO -> soundScriptIO, unk_16 -> sMMSeqUnk16;
//            gAudioCtx.maxTempo -> gAudioContext.tempoInternalToExternal, gPitchFrequencies -> gNoteFrequencies.

void MergeShipTest_Log(const char* fmt, ...);
#include "global.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include <string.h>

#define PROCESS_SCRIPT_END -1
#define SEQ_IO_VAL_NONE -1

#define PORTAMENTO_IS_SPECIAL(x) ((x).mode & 0x80)
#define PORTAMENTO_MODE(x) ((x).mode & ~0x80)
#define PORTAMENTO_MODE_OFF 0
#define PORTAMENTO_MODE_1 1
#define PORTAMENTO_MODE_2 2
#define PORTAMENTO_MODE_3 3
#define PORTAMENTO_MODE_4 4
#define PORTAMENTO_MODE_5 5

// MM MUTE_FLAGS_*
#define MUTE_FLAGS_STOP_SAMPLES (1 << 3)
#define MUTE_FLAGS_STOP_LAYER (1 << 4)
#define MUTE_FLAGS_SOFTEN (1 << 5)
#define MUTE_FLAGS_STOP_NOTES (1 << 6)
#define MUTE_FLAGS_STOP_SCRIPT (1 << 7)

// MM SEQPLAYER_STATE_*
#define SEQPLAYER_STATE_0 0
#define SEQPLAYER_STATE_FADE_IN 1
#define SEQPLAYER_STATE_FADE_OUT 2

// MM's layer unk_0A bits (z64audio.h): bit_2 selects the channel's reverb, useVibrato the channel's vibrato,
// bit_9 the channel's gain. Layer command 0xF0 clears them with a mask taken from the sequence, which was
// authored for the N64 build, where a u16 bitfield is allocated from the most significant bit (bit_0 = 0x8000).
// (2S2H's little-endian build allocates from the least significant bit, which changes what those masks clear;
// the N64 meaning is the one MM's sequence 0 was written for.)
#define MM_LAYER_UNK0A_BIT2 (0x8000 >> 2)
#define MM_LAYER_UNK0A_USE_VIBRATO (0x8000 >> 3)
#define MM_LAYER_UNK0A_BIT9 (0x8000 >> 9)

#define MM_NUM_LAYERS 64

void* AudioLoad_SyncLoadFont(u32 fontId);
u8 MergeShip_MMAudio_MapFont(u8 mmFontId);

typedef struct {
    u8* sfxState;
    s32 startSamplePos;
    s32 unk_E0;
    u8 surroundEffectIndex;
    u8 channelIndex;
} MMChannelExt;

typedef struct {
    u16 unk_0A;
    u8 surroundEffectIndex;
    u8 targetReverbVol;
} MMLayerExt;

SequencePlayer gMergeShipMMSeqPlayer;
volatile u32 gMergeShipMMAudioResetCount = 0;
volatile u8 gMergeShipMMAudioUnavailable = false;

static SequenceChannel sMMChannels[16];
static MMChannelExt sMMChannelExt[16];
static SequenceLayer sMMLayers[MM_NUM_LAYERS];
static MMLayerExt sMMLayerExt[MM_NUM_LAYERS];
static AudioListItem sMMLayerFreeList;
static u16 sMMSeqUnk16;
static u8 sMMSeqFonts[16];
static s32 sMMSeqNumFonts;
static s32 sMMInitialized = false;
// The vibrato a layer uses when MM takes it from the layer rather than the channel: MM's
// AudioScript_SeqChannelSetLayer defaults (rate 0x800, depth 0), which no layer command changes.
static SequenceChannel sMMLayerVibrato;
// MM gAudioCtx.customSeqFunctions (AUDIOCMD_GLOBAL_SET_CUSTOM_FUNCTION).
static u32 (*sMMCustomSeqFunctions[4])(s8 value, SequenceChannel* channel);

#define MM_CHAN_EXT(channel) (&sMMChannelExt[(channel) - sMMChannels])
#define MM_LAYER_EXT(layer) (&sMMLayerExt[(layer) - sMMLayers])

static s32 MM_IsMMLayer(SequenceLayer* layer) {
    return (layer != NULL) && (layer != NO_LAYER) && (layer >= sMMLayers) && (layer < &sMMLayers[MM_NUM_LAYERS]);
}

static s32 MM_IsMMChannel(SequenceChannel* channel) {
    return (channel >= sMMChannels) && (channel < &sMMChannels[ARRAY_COUNT(sMMChannels)]);
}

// MM sSeqInstructionArgsTable
#define CMD_ARGS_0() 0
#define CMD_ARGS_1(arg0Type) (((sizeof(arg0Type) - 1) << 7) | 1)
#define CMD_ARGS_2(arg0Type, arg1Type) (((sizeof(arg0Type) - 1) << 7) | ((sizeof(arg1Type) - 1) << 6) | 2)
#define CMD_ARGS_3(arg0Type, arg1Type, arg2Type) \
    (((sizeof(arg0Type) - 1) << 7) | ((sizeof(arg1Type) - 1) << 6) | ((sizeof(arg2Type) - 1) << 5) | 3)

static u8 sMMSeqInstructionArgsTable[] = {
    CMD_ARGS_1(s16),        // 0xA0 (channel: read from SfxChannelState using arg)
    CMD_ARGS_0(),           // 0xA1 (channel: read from SfxChannelState using unk_22)
    CMD_ARGS_1(s16),        // 0xA2 (channel: write to SfxChannelState using arg)
    CMD_ARGS_0(),           // 0xA3 (channel: write to SfxChannelState using unk_22)
    CMD_ARGS_1(u8),         // 0xA4 (channel: surround effect index)
    CMD_ARGS_0(),           // 0xA5 (channel: value += channel index)
    CMD_ARGS_2(u8, s16),    // 0xA6 (channel: write into sequence script at channel index)
    CMD_ARGS_1(u8),         // 0xA7 (channel: shift value)
    CMD_ARGS_2(s16, s16),   // 0xA8 (channel: random range large)
    CMD_ARGS_0(),           // 0xA9 ()
    CMD_ARGS_0(),           // 0xAA ()
    CMD_ARGS_0(),           // 0xAB ()
    CMD_ARGS_0(),           // 0xAC ()
    CMD_ARGS_0(),           // 0xAD ()
    CMD_ARGS_0(),           // 0xAE ()
    CMD_ARGS_0(),           // 0xAF ()
    CMD_ARGS_1(s16),        // 0xB0 (channel: set filter)
    CMD_ARGS_0(),           // 0xB1 (channel: clear filter)
    CMD_ARGS_1(s16),        // 0xB2 (channel: dynread sequence large)
    CMD_ARGS_1(u8),         // 0xB3 (channel: load filter)
    CMD_ARGS_0(),           // 0xB4 (channel: set dyntable large)
    CMD_ARGS_0(),           // 0xB5 (channel: read dyntable large)
    CMD_ARGS_0(),           // 0xB6 (channel: read dyntable)
    CMD_ARGS_1(s16),        // 0xB7 (channel: random large)
    CMD_ARGS_1(u8),         // 0xB8 (channel: random)
    CMD_ARGS_1(u8),         // 0xB9 (channel: set velocity random variance)
    CMD_ARGS_1(u8),         // 0xBA (channel: set gatetime random variance)
    CMD_ARGS_2(u8, s16),    // 0xBB (channel: comb filter)
    CMD_ARGS_1(s16),        // 0xBC (channel: add large)
    CMD_ARGS_1(s16),        // 0xBD (channel: start sample position)
    CMD_ARGS_1(u8),         // 0xBE (channel: custom function)
    CMD_ARGS_0(),           // 0xBF ()
    CMD_ARGS_0(),           // 0xC0 ()
    CMD_ARGS_1(u8),         // 0xC1 (channel: set instrument)
    CMD_ARGS_1(s16),        // 0xC2 (channel: set dyntable)
    CMD_ARGS_0(),           // 0xC3 (channel: large notes off)
    CMD_ARGS_0(),           // 0xC4 (channel: large notes on)
    CMD_ARGS_0(),           // 0xC5 (channel: dyn set dyntable)
    CMD_ARGS_1(u8),         // 0xC6 (channel: set soundFont)
    CMD_ARGS_2(u8, s16),    // 0xC7 (channel: write into sequence script)
    CMD_ARGS_1(u8),         // 0xC8 (channel: subtract -> set value)
    CMD_ARGS_1(u8),         // 0xC9 (channel: `bit and` -> set value)
    CMD_ARGS_1(u8),         // 0xCA (channel: set mute behavior)
    CMD_ARGS_1(s16),        // 0xCB (channel: read sequence -> set value)
    CMD_ARGS_1(u8),         // 0xCC (channel: set value)
    CMD_ARGS_1(u8),         // 0xCD (channel: disable channel)
    CMD_ARGS_1(s16),        // 0xCE (channel: set unk_22)
    CMD_ARGS_1(s16),        // 0xCF (channel: write large into sequence script)
    CMD_ARGS_1(u8),         // 0xD0 (channel: stereo headset effects)
    CMD_ARGS_1(u8),         // 0xD1 (channel: set note allocation policy)
    CMD_ARGS_1(u8),         // 0xD2 (channel: set sustain)
    CMD_ARGS_1(u8),         // 0xD3 (channel: large bend pitch)
    CMD_ARGS_1(u8),         // 0xD4 (channel: set reverb)
    CMD_ARGS_1(u8),         // 0xD5 ()
    CMD_ARGS_1(u8),         // 0xD6 ()
    CMD_ARGS_1(u8),         // 0xD7 (channel: set vibrato rate)
    CMD_ARGS_1(u8),         // 0xD8 (channel: set vibrato depth)
    CMD_ARGS_1(u8),         // 0xD9 (channel: set decay index)
    CMD_ARGS_1(s16),        // 0xDA (channel: set envelope)
    CMD_ARGS_1(u8),         // 0xDB (channel: transpose)
    CMD_ARGS_1(u8),         // 0xDC (channel: set pan mix)
    CMD_ARGS_1(u8),         // 0xDD (channel: set pan)
    CMD_ARGS_1(s16),        // 0xDE (channel: set freqscale)
    CMD_ARGS_1(u8),         // 0xDF (channel: set volume)
    CMD_ARGS_1(u8),         // 0xE0 (channel: set volume scale)
    CMD_ARGS_3(u8, u8, u8), // 0xE1 (channel: set vibratorate linear)
    CMD_ARGS_3(u8, u8, u8), // 0xE2 (channel: set vibrato depth linear)
    CMD_ARGS_1(u8),         // 0xE3 (channel: set vibrato delay)
    CMD_ARGS_0(),           // 0xE4 (channel: dyncall)
    CMD_ARGS_1(u8),         // 0xE5 (channel: set reverb index)
    CMD_ARGS_1(u8),         // 0xE6 (channel: set book offset)
    CMD_ARGS_1(s16),        // 0xE7 (channel: load channel params)
    CMD_ARGS_3(u8, u8, u8), // 0xE8 (channel: channel params)
    CMD_ARGS_1(u8),         // 0xE9 (channel: set note priority)
    CMD_ARGS_0(),           // 0xEA (channel: stop script)
    CMD_ARGS_2(u8, u8),     // 0xEB (channel: set soundFont and instrument)
    CMD_ARGS_0(),           // 0xEC (channel: reset vibrato)
    CMD_ARGS_1(u8),         // 0xED (channel: set hilo gain)
    CMD_ARGS_1(u8),         // 0xEE (channel: small bend pitch)
    CMD_ARGS_2(s16, u8),    // 0xEF ()
    CMD_ARGS_0(),           // 0xF0 (channel: unreserve notes)
    CMD_ARGS_1(u8),         // 0xF1 (channel: reserve notes)
    CMD_ARGS_1(u8),         // 0xF2 (branch relative if less than zero)
    CMD_ARGS_1(u8),         // 0xF3 (branch relative if equal to zero)
    CMD_ARGS_1(u8),         // 0xF4 (jump relative)
    CMD_ARGS_1(s16),        // 0xF5 (branch if greater than or equal to zero)
    CMD_ARGS_0(),           // 0xF6 (break)
    CMD_ARGS_0(),           // 0xF7 (loop end)
    CMD_ARGS_1(u8),         // 0xF8 (loop)
    CMD_ARGS_1(s16),        // 0xF9 (branch if less than zero)
    CMD_ARGS_1(s16),        // 0xFA (branch if equal to zero)
    CMD_ARGS_1(s16),        // 0xFB (jump)
    CMD_ARGS_1(s16),        // 0xFC (call and jump to a function)
    CMD_ARGS_0(),           // 0xFD (delay n frames)
    CMD_ARGS_0(),           // 0xFE (delay 1 frame)
    CMD_ARGS_0(),           // 0xFF (end script)
};

static void MM_SeqLayerProcessScript(SequenceLayer* layer);
static void MM_SequencePlayerDisable(SequencePlayer* seqPlayer);

// MM AudioScript_ScriptReadU8 / ReadS16 / ReadCompressedU16
static u8 MM_ScriptReadU8(SeqScriptState* state) {
    return *(state->pc++);
}

static s16 MM_ScriptReadS16(SeqScriptState* state) {
    s16 ret = *(state->pc++) << 8;

    ret = *(state->pc++) | ret;
    return ret;
}

static u16 MM_ScriptReadCompressedU16(SeqScriptState* state) {
    u16 ret = *(state->pc++);

    if (ret & 0x80) {
        ret = (ret << 8) & 0x7F00;
        ret = *(state->pc++) | ret;
    }
    return ret;
}

// MM AudioScript_GetScriptControlFlowArgument
static u16 MM_GetScriptControlFlowArgument(SeqScriptState* state, u8 cmd) {
    u8 highBits = sMMSeqInstructionArgsTable[cmd - 0xA0];
    u8 lowBits = highBits & 3;
    u16 cmdArg = 0;

    if (lowBits == 1) {
        if (!(highBits & 0x80)) {
            cmdArg = MM_ScriptReadU8(state);
        } else {
            cmdArg = MM_ScriptReadS16(state);
        }
    }

    return cmdArg;
}

// MM AudioScript_HandleScriptFlowControl
static s32 MM_HandleScriptFlowControl(SequencePlayer* seqPlayer, SeqScriptState* state, s32 cmd, s32 cmdArg) {
    u32 depth;

    switch (cmd) {
        case 0xFF: // end script
            if (state->depth == 0) {
                return PROCESS_SCRIPT_END;
            }
            state->pc = state->stack[--state->depth];
            break;

        case 0xFD: // delay n frames
            return MM_ScriptReadCompressedU16(state);

        case 0xFE: // delay 1 frame
            return 1;

        case 0xFC: // call and jump to a function
            state->stack[depth = state->depth++] = state->pc;
            state->pc = seqPlayer->seqData + (u16)cmdArg;
            break;

        case 0xF8: // loop
            state->remLoopIters[depth = state->depth] = cmdArg;
            state->stack[state->depth++] = state->pc;
            break;

        case 0xF7: // loop end
            state->remLoopIters[state->depth - 1]--;
            if (state->remLoopIters[state->depth - 1] != 0) {
                state->pc = state->stack[state->depth - 1];
            } else {
                state->depth--;
            }
            break;

        case 0xF6: // break
            state->depth--;
            break;

        case 0xF5: // branch if greater than or equal to zero
        case 0xF9: // branch if less than zero
        case 0xFA: // branch if equal to zero
        case 0xFB: // jump
            if ((cmd == 0xFA) && (state->value != 0)) {
                break;
            }
            if ((cmd == 0xF9) && (state->value >= 0)) {
                break;
            }
            if ((cmd == 0xF5) && (state->value < 0)) {
                break;
            }
            state->pc = seqPlayer->seqData + (u16)cmdArg;
            break;

        case 0xF2: // branch relative if less than zero
        case 0xF3: // branch relative if equal to zero
        case 0xF4: // jump relative
            if ((cmd == 0xF3) && (state->value != 0)) {
                break;
            }
            if ((cmd == 0xF2) && (state->value >= 0)) {
                break;
            }
            state->pc += (s8)(cmdArg & 0xFF);
            break;
    }

    (void)depth;
    return 0;
}

// MM AudioScript_InitSequenceChannel
static void MM_InitSequenceChannel(SequenceChannel* channel) {
    MMChannelExt* ext = MM_CHAN_EXT(channel);
    s32 i;

    channel->enabled = false;
    channel->finished = false;
    channel->stopScript = false;
    channel->stopSomething2 = false; // muted
    channel->hasInstrument = false;
    channel->stereoHeadsetEffects = false;
    channel->transposition = 0;
    channel->largeNotes = false;
    channel->bookOffset = 0;
    channel->stereo.asByte = 0;
    channel->changes.asByte = 0xFF;
    channel->scriptState.depth = 0;
    channel->newPan = 0x40;
    channel->panChannelWeight = 0x80;
    ext->surroundEffectIndex = 0xFF;
    channel->velocityRandomVariance = 0;
    channel->gateTimeRandomVariance = 0;
    channel->noteUnused = NULL;
    channel->reverbIndex = 0;
    channel->reverb = 0; // targetReverbVol
    channel->unk_0C = 0; // gain
    channel->notePriority = 3;
    channel->someOtherPriority = 1;
    channel->delay = 0;
    channel->adsr.envelope = gDefaultEnvelope;
    channel->adsr.releaseRate = 0xF0; // decayIndex
    channel->adsr.sustain = 0;
    channel->vibratoRateTarget = 0x800;
    channel->vibratoRateStart = 0x800;
    channel->vibratoExtentTarget = 0;
    channel->vibratoExtentStart = 0;
    channel->vibratoRateChangeDelay = 0;
    channel->vibratoExtentChangeDelay = 0;
    channel->vibratoDelay = 0;
    channel->filter = NULL;
    channel->unk_20 = 0; // combFilterGain
    channel->unk_0F = 0; // combFilterSize
    channel->volume = 1.0f;
    channel->volumeScale = 1.0f;
    channel->freqScale = 1.0f;

    for (i = 0; i < ARRAY_COUNT(channel->soundScriptIO); i++) {
        channel->soundScriptIO[i] = SEQ_IO_VAL_NONE;
    }

    channel->unused = false;
    Audio_InitNoteLists(&channel->notePool);
    ext->startSamplePos = 0;
    ext->unk_E0 = 0;
    ext->sfxState = NULL;
}

// MM AudioScript_SeqChannelSetLayer
static s32 MM_SeqChannelSetLayer(SequenceChannel* channel, s32 layerIndex) {
    SequenceLayer* layer;
    MMLayerExt* ext;

    if (channel->layers[layerIndex] == NULL) {
        layer = AudioSeq_AudioListPopBack(&sMMLayerFreeList);
        channel->layers[layerIndex] = layer;
        if (layer == NULL) {
            channel->layers[layerIndex] = NULL;
            return -1;
        }
    } else {
        Audio_SeqLayerNoteDecay(channel->layers[layerIndex]);
    }

    layer = channel->layers[layerIndex];
    ext = MM_LAYER_EXT(layer);

    layer->channel = channel;
    layer->adsr = channel->adsr;
    layer->adsr.releaseRate = 0; // decayIndex
    ext->targetReverbVol = channel->reverb;
    layer->enabled = true;
    layer->finished = false;
    layer->stopSomething = false; // muted
    layer->continuousNotes = false;
    layer->bit3 = false;
    layer->ignoreDrumPan = false;
    layer->bit1 = false;
    layer->notePropertiesNeedInit = false;
    layer->gateTime = 0x80;
    ext->surroundEffectIndex = 0x80;
    layer->stereo.asByte = 0;
    layer->portamento.mode = PORTAMENTO_MODE_OFF;
    layer->scriptState.depth = 0;
    layer->pan = 0x40;
    layer->transposition = 0;
    layer->delay = 0;
    layer->gateDelay = 0;
    layer->delay2 = 0;
    layer->note = NULL;
    layer->instrument = NULL;
    layer->instOrWave = 0xFF;
    ext->unk_0A = 0xFFFF;
    // (MM's layer vibrato defaults are sMMLayerVibrato's.)
    layer->freqScale = 1.0f;
    layer->unk_34 = 1.0f; // bend
    layer->velocitySquare2 = 0.0f;

    return 0;
}

// MM AudioScript_SeqLayerDisable
static void MM_SeqLayerDisable(SequenceLayer* layer) {
    if (layer != NULL) {
        if ((layer->channel != &gAudioContext.sequenceChannelNone) && (layer->channel->seqPlayer->finished == true)) {
            Audio_SeqLayerNoteRelease(layer);
        } else {
            Audio_SeqLayerNoteDecay(layer);
        }
        layer->enabled = false;
        layer->finished = true;
    }
}

// MM AudioScript_SeqLayerFree (into this player's own layer pool)
static void MM_SeqLayerFree(SequenceChannel* channel, s32 layerIndex) {
    SequenceLayer* layer = channel->layers[layerIndex];

    if (layer != NULL) {
        AudioSeq_AudioListPushBack(&sMMLayerFreeList, &layer->listItem);
        MM_SeqLayerDisable(layer);
        channel->layers[layerIndex] = NULL;
    }
}

// MM AudioScript_SequenceChannelDisable
static void MM_SequenceChannelDisable(SequenceChannel* channel) {
    s32 i;

    channel->finished = true;

    for (i = 0; i < 4; i++) {
        MM_SeqLayerFree(channel, i);
    }

    Audio_NotePoolClear(&channel->notePool);
    channel->enabled = false;
}

// MM AudioScript_SequencePlayerSetupChannels
static void MM_SequencePlayerSetupChannels(SequencePlayer* seqPlayer, u16 channelBits) {
    SequenceChannel* channel;
    s32 i;

    for (i = 0; i < 16; i++) {
        if (channelBits & 1) {
            channel = seqPlayer->channels[i];
            channel->fontId = seqPlayer->defaultFont;
            MergeShipTest_Log("MMSEQ setup chan=%d %p font=%d", i, (void*)channel, seqPlayer->defaultFont);
            channel->muteBehavior = seqPlayer->muteBehavior;
            channel->noteAllocPolicy = seqPlayer->noteAllocPolicy;
        }
        channelBits = channelBits >> 1;
    }
}

// MM AudioScript_SequencePlayerDisableChannels
static void MM_SequencePlayerDisableChannels(SequencePlayer* seqPlayer) {
    s32 i;

    for (i = 0; i < 16; i++) {
        MM_SequenceChannelDisable(seqPlayer->channels[i]);
    }
}

// MM AudioScript_SequenceChannelEnable
static void MM_SequenceChannelEnable(SequencePlayer* seqPlayer, u8 channelIndex, void* script) {
    SequenceChannel* channel = seqPlayer->channels[channelIndex];
    s32 i;

    channel->enabled = true;
    channel->finished = false;
    channel->scriptState.depth = 0;
    channel->scriptState.pc = script;
    channel->delay = 0;

    for (i = 0; i < ARRAY_COUNT(channel->layers); i++) {
        if (channel->layers[i] != NULL) {
            MM_SeqLayerFree(channel, i);
        }
    }
}

// MM AudioScript_SequencePlayerDisable. The sequence is a private copy and the fonts are permanent, so MM's
// load-status bookkeeping for the sequence and font caches has nothing to release here.
static void MM_SequencePlayerDisable(SequencePlayer* seqPlayer) {
    MM_SequencePlayerDisableChannels(seqPlayer);
    Audio_NotePoolClear(&seqPlayer->notePool);
    if (!seqPlayer->enabled) {
        return;
    }

    seqPlayer->enabled = false;
    seqPlayer->finished = true;
}

// MM AudioScript_ResetSequencePlayer
static void MM_ResetSequencePlayer(SequencePlayer* seqPlayer) {
    s32 i;

    MM_SequencePlayerDisable(seqPlayer);
    seqPlayer->stopScript = false;
    seqPlayer->delay = 0;
    seqPlayer->state = SEQPLAYER_STATE_FADE_IN;
    seqPlayer->fadeTimer = 0;
    seqPlayer->fadeTimerUnkEu = 0; // storedFadeTimer
    seqPlayer->tempoAcc = 0;
    seqPlayer->tempo = 120 * TATUMS_PER_BEAT; // 120 BPM
    seqPlayer->unk_0C = 0;                    // tempoChange
    seqPlayer->transposition = 0;
    seqPlayer->noteAllocPolicy = 0;
    seqPlayer->shortNoteVelocityTable = gDefaultShortNoteVelocityTable;
    seqPlayer->shortNoteGateTimeTable = gDefaultShortNoteGateTimeTable;
    sMMSeqUnk16 = 0;
    seqPlayer->scriptCounter = 0;
    seqPlayer->fadeVolume = 1.0f;
    seqPlayer->fadeVelocity = 0.0f;
    seqPlayer->volume = 0.0f;
    seqPlayer->muteVolumeScale = 0.5f;

    for (i = 0; i < 16; i++) {
        MM_InitSequenceChannel(seqPlayer->channels[i]);
    }
}

// MM AudioScript_InitSequencePlayer + AudioScript_InitSequencePlayerChannels + AudioScript_InitLayerFreelist,
// for this player's own channels and layers.
static void MM_InitSequencePlayer(void) {
    SequencePlayer* seqPlayer = &gMergeShipMMSeqPlayer;
    s32 i;
    s32 j;

    // Layer pool.
    sMMLayerFreeList.prev = &sMMLayerFreeList;
    sMMLayerFreeList.next = &sMMLayerFreeList;
    sMMLayerFreeList.u.count = 0;
    sMMLayerFreeList.pool = NULL;
    for (i = 0; i < MM_NUM_LAYERS; i++) {
        memset(&sMMLayers[i], 0, sizeof(sMMLayers[i]));
        sMMLayers[i].listItem.u.value = &sMMLayers[i];
        sMMLayers[i].listItem.prev = NULL;
        sMMLayers[i].channel = NULL;
        sMMLayers[i].enabled = false;
        AudioSeq_AudioListPushBack(&sMMLayerFreeList, &sMMLayers[i].listItem);
    }

    memset(seqPlayer, 0, sizeof(*seqPlayer));
    for (i = 0; i < 16; i++) {
        SequenceChannel* channel = &sMMChannels[i];

        memset(channel, 0, sizeof(*channel));
        seqPlayer->channels[i] = channel;
        channel->seqPlayer = seqPlayer;
        channel->enabled = false;
        for (j = 0; j < 4; j++) {
            channel->layers[j] = NULL;
        }
        sMMChannelExt[i].channelIndex = i;
        MM_InitSequenceChannel(channel);
    }

    seqPlayer->enabled = false;
    seqPlayer->muted = false;
    seqPlayer->fontDmaInProgress = false;
    seqPlayer->seqDmaInProgress = false;
    seqPlayer->unk_0b1 = false; // applyBend

    for (j = 0; j < 8; j++) {
        seqPlayer->soundScriptIO[j] = SEQ_IO_VAL_NONE;
    }
    seqPlayer->muteBehavior = MUTE_FLAGS_SOFTEN | MUTE_FLAGS_STOP_NOTES;
    seqPlayer->fadeVolumeScale = 1.0f;
    seqPlayer->unk_34 = 1.0f; // bend
    seqPlayer->gameVolume = 1.0f;
    seqPlayer->playerIdx = MERGESHIP_MM_SEQ_PLAYER;
    Audio_InitNoteLists(&seqPlayer->notePool);
    MM_ResetSequencePlayer(seqPlayer);

    // The layer vibrato MM gives a layer in AudioScript_SeqChannelSetLayer.
    memset(&sMMLayerVibrato, 0, sizeof(sMMLayerVibrato));
    sMMLayerVibrato.vibratoRateTarget = 0x800;
    sMMLayerVibrato.vibratoRateStart = 0x800;
    sMMLayerVibrato.vibratoExtentTarget = 0;
    sMMLayerVibrato.vibratoExtentStart = 0;
    sMMLayerVibrato.vibratoRateChangeDelay = 0;
    sMMLayerVibrato.vibratoExtentChangeDelay = 0;
    sMMLayerVibrato.vibratoDelay = 0;
    sMMLayerVibrato.seqPlayer = seqPlayer;

    sMMInitialized = true;
}

// ---- layer script (MM AudioScript_SeqLayerProcessScript and its steps) ----------------------------------

// MM AudioScript_GetInstrument
static u8 MM_GetInstrument(SequenceChannel* channel, u8 instId, Instrument** instOut, AdsrSettings* adsr) {
    Instrument* inst = Audio_GetInstrumentInner(channel->fontId, instId);

    if (inst == NULL) {
        *instOut = NULL;
        return 0;
    }

    adsr->envelope = inst->envelope;
    adsr->releaseRate = inst->releaseRate; // adsrDecayIndex

    *instOut = inst;

    // temporarily offset instrument id by 2 so that instId 0, 1
    // can be reserved by drums and sfxs respectively.
    instId += 2;

    return instId;
}

// MM AudioScript_SetInstrument
static void MM_SetInstrument(SequenceChannel* channel, u8 instId) {
    // Harness evidence: the instrument sequence 0's ocarina channel (MM SFX_CHANNEL_OCARINA, 13) plays with.
    if (channel == gMergeShipMMSeqPlayer.channels[13]) {
        MergeShipTest_Log("MMOCA ch13 set instrument font=%d instrument=0x%02X", channel->fontId, instId);
    }
    if (instId >= 0x80) {
        // Synthetic Waves
        channel->instOrWave = instId;
        channel->instrument = NULL;
    } else if (instId == 0x7F) {
        // Drums
        channel->instOrWave = 0;
        channel->instrument = (Instrument*)1; // invalid pointer, never dereferenced
    } else if (instId == 0x7E) {
        // Sfxs
        channel->instOrWave = 1;
        channel->instrument = (Instrument*)2; // invalid pointer, never dereferenced
    } else {
        // Instruments
        if ((channel->instOrWave = MM_GetInstrument(channel, instId, &channel->instrument, &channel->adsr)) == 0) {
            channel->hasInstrument = false;
            return;
        }
    }

    channel->hasInstrument = true;
}

// MM AudioScript_SequenceChannelSetVolume
static void MM_SequenceChannelSetVolume(SequenceChannel* channel, u8 volume) {
    channel->volume = (s32)volume / 127.0f;
}

// MM AudioScript_SetChannelPriorities
static void MM_SetChannelPriorities(SequenceChannel* channel, u8 priority) {
    if ((priority & 0xF) != 0) {
        channel->notePriority = priority & 0xF;
    }

    priority = priority >> 4;
    if (priority != 0) {
        channel->someOtherPriority = priority;
    }
}

// MM AudioScript_SeqLayerProcessScriptStep1
static void MM_SeqLayerProcessScriptStep1(SequenceLayer* layer) {
    if (!layer->continuousNotes) {
        Audio_SeqLayerNoteDecay(layer);
    } else if ((layer->note != NULL) && (layer->note->playbackState.wantedParentLayer == layer)) {
        Audio_SeqLayerNoteDecay(layer);
    }

    if ((PORTAMENTO_MODE(layer->portamento) == PORTAMENTO_MODE_1) ||
        (PORTAMENTO_MODE(layer->portamento) == PORTAMENTO_MODE_2)) {
        layer->portamento.mode = PORTAMENTO_MODE_OFF;
    }
    layer->notePropertiesNeedInit = true;
}

// MM AudioScript_SeqLayerProcessScriptStep5
static s32 MM_SeqLayerProcessScriptStep5(SequenceLayer* layer, s32 sameTunedSample) {
    Note* note;

    if ((layer->continuousNotes == true) && (layer->bit1 == true)) {
        return 0;
    }

    if ((layer->continuousNotes == true) && (layer->note != NULL) && layer->bit3 && (sameTunedSample == true) &&
        (layer->note->playbackState.parentLayer == layer)) {
        if (layer->sound == NULL) {
            Audio_InitSyntheticWave(layer->note, layer);
        }
    } else {
        if (!sameTunedSample) {
            Audio_SeqLayerNoteDecay(layer);
        }

        layer->note = Audio_AllocNote(layer);

        if (layer->note != NULL) {
            note = layer->note;

            if (note->playbackState.parentLayer == layer) {
                // MM AudioEffects_InitVibrato; the channel-or-layer choice is made in Audio_NoteVibratoInit's hook.
                Audio_NoteVibratoInit(note);
            }
        }
    }

    if ((layer->note != NULL) && (layer->note->playbackState.parentLayer == layer)) {
        note = layer->note;

        Audio_NotePortamentoInit(note);
    }

    return 0;
}

// MM AudioScript_SeqLayerProcessScriptStep2
static s32 MM_SeqLayerProcessScriptStep2(SequenceLayer* layer) {
    SequenceChannel* channel = layer->channel;
    SeqScriptState* state = &layer->scriptState;
    SequencePlayer* seqPlayer = channel->seqPlayer;
    MMLayerExt* ext = MM_LAYER_EXT(layer);
    u8 cmd;
    u8 cmdArg8;
    u16 cmdArg16;
    u16 velocity;

    while (true) {
        cmd = MM_ScriptReadU8(state);

        // Note Commands
        // To be processed in MM_SeqLayerProcessScriptStep3
        if (cmd <= 0xC0) {
            return cmd;
        }

        // Control Flow Commands
        if (cmd >= 0xF2) {
            cmdArg16 = MM_GetScriptControlFlowArgument(state, cmd);

            if (MM_HandleScriptFlowControl(seqPlayer, state, cmd, cmdArg16) == 0) {
                continue;
            }
            MM_SeqLayerDisable(layer);
            return PROCESS_SCRIPT_END;
        }

        switch (cmd) {
            case 0xC1: // layer: set short note velocity
            case 0xCA: // layer: set pan
                cmdArg8 = *(state->pc++);
                if (cmd == 0xC1) {
                    layer->velocitySquare = SQ(cmdArg8) / SQ(127.0f);
                } else {
                    layer->pan = cmdArg8;
                }
                break;

            case 0xC9: // layer: set short note gatetime
            case 0xC2: // layer: set transposition in semitones
                cmdArg8 = *(state->pc++);
                if (cmd == 0xC9) {
                    layer->gateTime = cmdArg8;
                } else {
                    layer->transposition = cmdArg8;
                }
                break;

            case 0xC4: // layer: continuous notes on
            case 0xC5: // layer: continuous notes off
                if (cmd == 0xC4) {
                    layer->continuousNotes = true;
                } else {
                    layer->continuousNotes = false;
                }
                layer->bit1 = false;
                Audio_SeqLayerNoteDecay(layer);
                break;

            case 0xC3: // layer: set short note default delay
                cmdArg16 = MM_ScriptReadCompressedU16(state);
                layer->shortNoteDefaultDelay = cmdArg16;
                break;

            case 0xC6: // layer: set instrument
                cmd = MM_ScriptReadU8(state);
                if (cmd >= 0x7E) {
                    if (cmd == 0x7E) {
                        // Sfxs
                        layer->instOrWave = 1;
                    } else if (cmd == 0x7F) {
                        // Drums
                        layer->instOrWave = 0;
                    } else {
                        // Synthetic Wave
                        layer->instOrWave = cmd;
                        layer->instrument = NULL;
                    }

                    if (cmd == 0xFF) {
                        layer->adsr.releaseRate = 0;
                    }
                } else {
                    // Instrument
                    if ((layer->instOrWave = MM_GetInstrument(channel, cmd, &layer->instrument, &layer->adsr)) == 0) {
                        layer->instOrWave = 0xFF;
                    }
                }
                break;

            case 0xC7: // layer: enable portamento
                layer->portamento.mode = MM_ScriptReadU8(state);

                cmd = MM_ScriptReadU8(state);
                cmd += channel->transposition;
                cmd += layer->transposition;
                cmd += seqPlayer->transposition;

                if (cmd >= 0x80) {
                    cmd = 0;
                }

                layer->portamentoTargetNote = cmd;

                // If special, the next param is u8 instead of var
                if (PORTAMENTO_IS_SPECIAL(layer->portamento)) {
                    layer->portamentoTime = *(state->pc++);
                    break;
                }

                cmdArg16 = MM_ScriptReadCompressedU16(state);
                layer->portamentoTime = cmdArg16;
                break;

            case 0xC8: // layer: disable portamento
                layer->portamento.mode = PORTAMENTO_MODE_OFF;
                break;

            case 0xCB: // layer: set envelope and decay index
                cmdArg16 = MM_ScriptReadS16(state);
                layer->adsr.envelope = (AdsrEnvelope*)(seqPlayer->seqData + cmdArg16);
                // fallthrough
            case 0xCF: // layer: set decay index
                layer->adsr.releaseRate = MM_ScriptReadU8(state);
                break;

            case 0xCC: // layer: ignore drum pan
                layer->ignoreDrumPan = true;
                break;

            case 0xCD: // layer: stereo effects
                layer->stereo.asByte = MM_ScriptReadU8(state);
                break;

            case 0xCE: // layer: bend pitch
                cmdArg8 = MM_ScriptReadU8(state);
                layer->unk_34 = gBendPitchTwoSemitonesFrequencies[(u8)(cmdArg8 + 0x80)];
                break;

            case 0xF0: // layer: clear unk_0A bits
                cmdArg16 = MM_ScriptReadS16(state);
                ext->unk_0A &= (cmdArg16 ^ 0xFFFF);
                break;

            case 0xF1: // layer: surround effect index
                ext->surroundEffectIndex = MM_ScriptReadU8(state);
                break;

            default:
                switch (cmd & 0xF0) {
                    case 0xD0: // layer: set short note velocity from table
                        velocity = seqPlayer->shortNoteVelocityTable[cmd & 0xF];
                        layer->velocitySquare = SQ(velocity) / SQ(127.0f);
                        break;

                    case 0xE0: // layer: set short note gatetime from table
                        layer->gateTime = seqPlayer->shortNoteGateTimeTable[cmd & 0xF];
                        break;
                }
        }
    }
}

// MM AudioScript_SeqLayerProcessScriptStep4
static s32 MM_SeqLayerProcessScriptStep4(SequenceLayer* layer, s32 cmd) {
    s32 sameTunedSample = true;
    s32 instOrWave;
    s32 speed;
    f32 temp_f14;
    f32 temp_f2;
    Portamento* portamento = NULL;
    f32 freqScale;
    f32 freqScale2;
    SoundFontSound* tunedSample;
    Instrument* instrument;
    Drum* drum;
    SoundFontSound* soundEffect;
    SequenceChannel* channel;
    SequencePlayer* seqPlayer;
    u8 semitone = cmd;
    u16 sfxId;
    s32 semitone2;
    s32 velocity;
    f32 time;
    f32 tuning;
    s32 speed2;

    instOrWave = layer->instOrWave;
    channel = layer->channel;
    seqPlayer = channel->seqPlayer;

    if (instOrWave == 0xFF) {
        if (!channel->hasInstrument) {
            return PROCESS_SCRIPT_END;
        }
        instOrWave = channel->instOrWave;
    }

    switch (instOrWave) {
        case 0:
            // Drums
            semitone += channel->transposition + layer->transposition;
            layer->semitone = semitone;

            drum = Audio_GetDrum(channel->fontId, semitone);
            if (drum == NULL) {
                layer->stopSomething = true;
                layer->delay2 = layer->delay;
                return PROCESS_SCRIPT_END;
            }

            tunedSample = &drum->sound;
            layer->adsr.envelope = drum->envelope;
            layer->adsr.releaseRate = drum->releaseRate;
            if (!layer->ignoreDrumPan) {
                layer->pan = drum->pan;
            }

            layer->sound = tunedSample;
            layer->freqScale = tunedSample->tuning;
            break;

        case 1:
            // Sfxs
            layer->semitone = semitone;
            sfxId = (layer->transposition << 6) + semitone;

            soundEffect = Audio_GetSfx(channel->fontId, sfxId);
            MergeShipTest_Log("MMSEQ sfxnote font=%d id=%d %s", channel->fontId, sfxId,
                              (soundEffect == NULL) ? "MISSING" : ((soundEffect->sample == NULL) ? "NOSAMPLE" : "ok"));
            if (soundEffect == NULL) {
                layer->stopSomething = true;
                layer->delay2 = layer->delay + 1;
                return PROCESS_SCRIPT_END;
            }

            tunedSample = soundEffect;
            layer->sound = tunedSample;
            layer->freqScale = tunedSample->tuning;
            break;

        default:
            semitone += seqPlayer->transposition + channel->transposition + layer->transposition;
            semitone2 = semitone;

            layer->semitone = semitone;
            if (semitone >= 0x80) {
                layer->stopSomething = true;
                return PROCESS_SCRIPT_END;
            }

            if (layer->instOrWave == 0xFF) {
                instrument = channel->instrument;
            } else {
                instrument = layer->instrument;
            }

            if (layer->portamento.mode != PORTAMENTO_MODE_OFF) {
                portamento = &layer->portamento;
                velocity = (semitone > layer->portamentoTargetNote) ? semitone : layer->portamentoTargetNote;

                if (instrument != NULL) {
                    tunedSample = Audio_InstrumentGetSound(instrument, velocity);
                    sameTunedSample = (layer->sound == tunedSample);
                    layer->sound = tunedSample;
                    tuning = tunedSample->tuning;
                } else {
                    layer->sound = NULL;
                    tuning = 1.0f;
                    if (instOrWave >= 0xC0) {
                        layer->sound = &gAudioContext.synthesisReverbs[instOrWave - 0xC0].sound;
                    }
                }

                temp_f2 = gNoteFrequencies[semitone2] * tuning;
                temp_f14 = gNoteFrequencies[layer->portamentoTargetNote] * tuning;

                switch (PORTAMENTO_MODE(*portamento)) {
                    case PORTAMENTO_MODE_1:
                    case PORTAMENTO_MODE_3:
                    case PORTAMENTO_MODE_5:
                        freqScale2 = temp_f2;
                        freqScale = temp_f14;
                        break;

                    case PORTAMENTO_MODE_2:
                    case PORTAMENTO_MODE_4:
                        freqScale = temp_f2;
                        freqScale2 = temp_f14;
                        break;

                    default:
                        freqScale = temp_f2;
                        freqScale2 = temp_f2;
                        break;
                }

                portamento->extent = (freqScale2 / freqScale) - 1.0f;

                if (PORTAMENTO_IS_SPECIAL(*portamento)) {
                    speed = seqPlayer->tempo * 0x8000 / gAudioContext.tempoInternalToExternal;
                    if (layer->delay != 0) {
                        speed = speed * 0x100 / (layer->delay * layer->portamentoTime);
                    }
                } else {
                    speed = 0x20000 / (layer->portamentoTime * gAudioContext.audioBufferParameters.updatesPerFrame);
                }

                if (speed >= 0x7FFF) {
                    speed = 0x7FFF;
                } else if (speed < 1) {
                    speed = 1;
                }

                portamento->speed = speed;
                portamento->cur = 0;
                layer->freqScale = freqScale;
                if (PORTAMENTO_MODE(*portamento) == PORTAMENTO_MODE_5) {
                    layer->portamentoTargetNote = semitone;
                }
                break;
            }

            if (instrument != NULL) {
                tunedSample = Audio_InstrumentGetSound(instrument, semitone);
                sameTunedSample = (tunedSample == layer->sound);
                layer->sound = tunedSample;
                layer->freqScale = gNoteFrequencies[semitone2] * tunedSample->tuning;
            } else {
                layer->sound = NULL;
                layer->freqScale = gNoteFrequencies[semitone2];
                if (instOrWave >= 0xC0) {
                    layer->sound = &gAudioContext.synthesisReverbs[instOrWave - 0xC0].sound;
                }
            }
            break;
    }

    layer->delay2 = layer->delay;
    layer->freqScale *= layer->unk_34; // bend

    if (layer->delay == 0) {
        if (layer->sound != NULL) {
            time = layer->sound->sample->loop->loopEnd;
        } else {
            time = 0.0f;
        }
        time *= seqPlayer->tempo;
        time *= gAudioContext.unk_2870;
        time /= layer->freqScale;
        if (time > 0x7FFE) {
            time = 0x7FFE;
        }

        layer->gateDelay = 0;
        layer->delay = (u16)(s32)time + 1;

        if (layer->portamento.mode != PORTAMENTO_MODE_OFF) {
            // (It's a bit unclear if 'portamento' has actually always been
            // set when this is reached...)
            if ((portamento != NULL) && PORTAMENTO_IS_SPECIAL(*portamento)) {
                speed2 = seqPlayer->tempo * 0x8000 / gAudioContext.tempoInternalToExternal;
                speed2 = speed2 * 0x100 / (layer->delay * layer->portamentoTime);
                if (speed2 >= 0x7FFF) {
                    speed2 = 0x7FFF;
                } else if (speed2 < 1) {
                    speed2 = 1;
                }
                portamento->speed = speed2;
            }
        }
    }
    return sameTunedSample;
}

// MM AudioScript_SeqLayerProcessScriptStep3
static s32 MM_SeqLayerProcessScriptStep3(SequenceLayer* layer, s32 cmd) {
    SeqScriptState* state = &layer->scriptState;
    u16 delay = 0;
    s32 velocity = 0;
    SequenceChannel* channel = layer->channel;
    SequencePlayer* seqPlayer = channel->seqPlayer;
    s32 intDelta;
    f32 floatDelta;

    if (cmd == 0xC0) { // layer: delay
        layer->delay = MM_ScriptReadCompressedU16(state);
        layer->stopSomething = true;
        layer->bit1 = false;
        return PROCESS_SCRIPT_END;
    }

    layer->stopSomething = false;

    if (channel->largeNotes == true) {
        switch (cmd & 0xC0) {
            case 0x00: // layer: large note 0
                delay = MM_ScriptReadCompressedU16(state);
                velocity = *(state->pc++);
                layer->gateTime = *(state->pc++);
                layer->lastDelay = delay;
                break;

            case 0x40: // layer: large note 1
                delay = MM_ScriptReadCompressedU16(state);
                velocity = *(state->pc++);
                layer->gateTime = 0;
                layer->lastDelay = delay;
                break;

            case 0x80: // layer: large note 2
                delay = layer->lastDelay;
                velocity = *(state->pc++);
                layer->gateTime = *(state->pc++);
                break;
        }

        if ((velocity > 0x7F) || (velocity < 0)) {
            velocity = 0x7F;
        }
        layer->velocitySquare = SQ((f32)velocity) / SQ(127.0f);
        cmd -= (cmd & 0xC0);
    } else {
        switch (cmd & 0xC0) {
            case 0x00: // layer: small note 0
                delay = MM_ScriptReadCompressedU16(state);
                layer->lastDelay = delay;
                break;

            case 0x40: // layer: small note 1
                delay = layer->shortNoteDefaultDelay;
                break;

            case 0x80: // layer: small note 2
                delay = layer->lastDelay;
                break;
        }
        cmd -= (cmd & 0xC0);
    }

    if (channel->velocityRandomVariance != 0) {
        floatDelta =
            layer->velocitySquare * (gAudioContext.audioRandom % channel->velocityRandomVariance) / 100.0f;
        if ((gAudioContext.audioRandom & 0x8000) != 0) {
            floatDelta = -floatDelta;
        }

        layer->velocitySquare2 = layer->velocitySquare + floatDelta;

        if (layer->velocitySquare2 < 0.0f) {
            layer->velocitySquare2 = 0.0f;
        } else if (layer->velocitySquare2 > 1.0f) {
            layer->velocitySquare2 = 1.0f;
        }
    } else {
        layer->velocitySquare2 = layer->velocitySquare;
    }

    layer->delay = delay;
    layer->gateDelay = (layer->gateTime * delay) >> 8;

    if (channel->gateTimeRandomVariance != 0) {
        //! @bug should probably be gateTimeRandomVariance (MM's bug, kept; guarded against a zero divisor)
        if (channel->velocityRandomVariance != 0) {
            intDelta = (layer->gateDelay * (gAudioContext.audioRandom % channel->velocityRandomVariance)) / 100;
        } else {
            intDelta = 0;
        }
        if ((gAudioContext.audioRandom & 0x4000) != 0) {
            intDelta = -intDelta;
        }

        layer->gateDelay += intDelta;
        if (layer->gateDelay < 0) {
            layer->gateDelay = 0;
        } else if (layer->gateDelay > layer->delay) {
            layer->gateDelay = layer->delay;
        }
    }

    if ((seqPlayer->muted && (channel->muteBehavior & (MUTE_FLAGS_STOP_NOTES | MUTE_FLAGS_STOP_LAYER))) ||
        channel->stopSomething2) {
        layer->stopSomething = true;
        return PROCESS_SCRIPT_END;
    }

    if (seqPlayer->skipTicks != 0) {
        layer->stopSomething = true;
        return PROCESS_SCRIPT_END;
    }

    return cmd;
}

// MM AudioScript_SeqLayerProcessScript
static void MM_SeqLayerProcessScript(SequenceLayer* layer) {
    s32 cmd;

    if (!layer->enabled) {
        return;
    }

    if (layer->delay > 1) {
        layer->delay--;
        if (!layer->stopSomething && (layer->delay <= layer->gateDelay)) {
            Audio_SeqLayerNoteDecay(layer);
            layer->stopSomething = true;
        }
        return;
    }

    MM_SeqLayerProcessScriptStep1(layer);

    do {
        cmd = MM_SeqLayerProcessScriptStep2(layer);
        if (cmd == PROCESS_SCRIPT_END) {
            return;
        }

        cmd = MM_SeqLayerProcessScriptStep3(layer, cmd);

    } while ((cmd == -1) && (layer->delay == 0));

    if (cmd != PROCESS_SCRIPT_END) {
        // returns `sameTunedSample` instead of a command
        cmd = MM_SeqLayerProcessScriptStep4(layer, cmd);
    }

    if (cmd != PROCESS_SCRIPT_END) {
        MM_SeqLayerProcessScriptStep5(layer, cmd);
    }

    if (layer->stopSomething == true) {
        if ((layer->note != NULL) || layer->continuousNotes) {
            Audio_SeqLayerNoteDecay(layer);
        }
    }
}

// ---- channel script (MM AudioScript_SequenceChannelProcessScript) --------------------------------------

// MM's 0xC6/0xEB font argument: an index counted from the end of the sequence's font list.
static s32 MM_SeqFontFromArg(u16 arg, u16* outFont) {
    s32 index = sMMSeqNumFonts - (s32)arg - 1;

    if ((index < 0) || (index >= sMMSeqNumFonts)) {
        return false;
    }
    *outFont = sMMSeqFonts[index];
    return true;
}

static void MM_SequenceChannelProcessScript(SequenceChannel* channel) {
    MMChannelExt* ext = MM_CHAN_EXT(channel);
    s32 i;
    u8* data;
    u32 rand;
    SequencePlayer* seqPlayer;

    if (channel->stopScript) {
        goto exit_loop;
    }

    seqPlayer = channel->seqPlayer;
    if (seqPlayer->muted && (channel->muteBehavior & MUTE_FLAGS_STOP_SCRIPT)) {
        return;
    }

    if (channel->delay >= 2) {
        channel->delay--;
        goto exit_loop;
    }

    while (true) {
        SeqScriptState* scriptState = &channel->scriptState;
        s32 param;
        s16 temp1;
        u16 cmdArgU16;
        u32 cmdArgs[3] = { 0, 0, 0 };
        s8 cmdArgS8;
        u8 cmd = MM_ScriptReadU8(scriptState);
        u8 lowBits;
        u8 highBits;
        s32 delay;
        s32 temp2;
        u8 phi_v0_3;
        u8 new_var;
        u8 depth;
        u8* seqData = seqPlayer->seqData;
        u32 new_var2;

        // Commands 0xA0 - 0xFF
        if (cmd >= 0xA0) {
            highBits = sMMSeqInstructionArgsTable[cmd - 0xA0];
            lowBits = highBits & 3;

            // read in arguments for the instruction
            for (i = 0; i < lowBits; i++, highBits <<= 1) {
                if (!(highBits & 0x80)) {
                    cmdArgs[i] = MM_ScriptReadU8(scriptState);
                } else {
                    cmdArgs[i] = MM_ScriptReadS16(scriptState);
                }
            }

            // Control Flow Commands
            if (cmd >= 0xF2) {
                delay = MM_HandleScriptFlowControl(seqPlayer, scriptState, cmd, cmdArgs[0]);

                if (delay != 0) {
                    if (delay == PROCESS_SCRIPT_END) {
                        MM_SequenceChannelDisable(channel);
                    } else {
                        channel->delay = delay;
                    }
                    break;
                }
                continue;
            }

            switch (cmd) {
                case 0xEA: // channel: stop script
                    channel->stopScript = true;
                    goto exit_loop;

                case 0xF1: // channel: reserve notes
                    Audio_NotePoolClear(&channel->notePool);
                    cmd = (u8)cmdArgs[0];
                    Audio_NotePoolFill(&channel->notePool, cmd);
                    break;

                case 0xF0: // channel: unreserve notes
                    Audio_NotePoolClear(&channel->notePool);
                    break;

                case 0xC2: // channel: set dyntable
                    cmdArgU16 = (u16)cmdArgs[0];
                    channel->dynTable = (void*)&seqPlayer->seqData[cmdArgU16];
                    break;

                case 0xC5: // channel: dyn set dyntable
                    if (scriptState->value != -1) {
                        data = (*channel->dynTable)[scriptState->value];
                        cmdArgU16 = (u16)((data[0] << 8) + data[1]);
                        scriptState->pc = (void*)&seqPlayer->seqData[cmdArgU16];
                    }
                    break;

                case 0xEB: { // channel: set soundFont and instrument
                    u16 result;

                    if (MM_SeqFontFromArg((u16)cmdArgs[0], &result) &&
                        AudioHeap_SearchCaches(FONT_TABLE, CACHE_EITHER, result)) {
                        channel->fontId = result;
                    }

                    cmdArgs[0] = cmdArgs[1];
                }
                    // fallthrough
                case 0xC1: // channel: set instrument
                    cmd = (u8)cmdArgs[0];
                    MM_SetInstrument(channel, cmd);
                    break;

                case 0xC3: // channel: large notes off
                    channel->largeNotes = false;
                    break;

                case 0xC4: // channel: large notes on
                    channel->largeNotes = true;
                    break;

                case 0xDF: // channel: set volume
                    cmd = (u8)cmdArgs[0];
                    MM_SequenceChannelSetVolume(channel, cmd);
                    channel->changes.s.volume = true;
                    break;

                case 0xE0: // channel: set volume scale
                    cmd = (u8)cmdArgs[0];
                    channel->volumeScale = (f32)(s32)cmd / 128.0f;
                    channel->changes.s.volume = true;
                    break;

                case 0xDE: // channel: set freqscale
                    cmdArgU16 = (u16)cmdArgs[0];
                    channel->freqScale = (f32)(s32)cmdArgU16 / 0x8000;
                    channel->changes.s.freqScale = true;
                    break;

                case 0xD3: // channel: large bend pitch
                    cmd = (u8)cmdArgs[0];
                    cmd += 0x80;
                    channel->freqScale = gBendPitchOneOctaveFrequencies[cmd];
                    channel->changes.s.freqScale = true;
                    break;

                case 0xEE: // channel: small bend pitch
                    cmd = (u8)cmdArgs[0];
                    cmd += 0x80;
                    channel->freqScale = gBendPitchTwoSemitonesFrequencies[cmd];
                    channel->changes.s.freqScale = true;
                    break;

                case 0xDD: // channel: set pan
                    cmd = (u8)cmdArgs[0];
                    channel->newPan = cmd;
                    channel->changes.s.pan = true;
                    break;

                case 0xDC: // channel: set pan mix
                    cmd = (u8)cmdArgs[0];
                    channel->panChannelWeight = cmd;
                    channel->changes.s.pan = true;
                    break;

                case 0xDB: // channel: transpose
                    cmdArgS8 = (s8)cmdArgs[0];
                    channel->transposition = cmdArgS8;
                    break;

                case 0xDA: // channel: set envelope
                    cmdArgU16 = (u16)cmdArgs[0];
                    channel->adsr.envelope = (AdsrEnvelope*)&seqPlayer->seqData[cmdArgU16];
                    break;

                case 0xD9: // channel: set decay index
                    cmd = (u8)cmdArgs[0];
                    channel->adsr.releaseRate = cmd;
                    break;

                case 0xD8: // channel: set vibrato depth
                    cmd = (u8)cmdArgs[0];
                    channel->vibratoExtentTarget = cmd * 8;
                    channel->vibratoExtentStart = 0;
                    channel->vibratoExtentChangeDelay = 0;
                    break;

                case 0xD7: // channel: set vibrato rate
                    cmd = (u8)cmdArgs[0];
                    channel->vibratoRateChangeDelay = 0;
                    channel->vibratoRateTarget = cmd * 32;
                    channel->vibratoRateStart = cmd * 32;
                    break;

                case 0xE2: // channel: set vibrato depth linear
                    cmd = (u8)cmdArgs[0];
                    channel->vibratoExtentStart = cmd * 8;
                    cmd = (u8)cmdArgs[1];
                    channel->vibratoExtentTarget = cmd * 8;
                    cmd = (u8)cmdArgs[2];
                    channel->vibratoExtentChangeDelay = cmd * 16;
                    break;

                case 0xE1: // channel: set vibratorate linear
                    cmd = (u8)cmdArgs[0];
                    channel->vibratoRateStart = cmd * 32;
                    cmd = (u8)cmdArgs[1];
                    channel->vibratoRateTarget = cmd * 32;
                    cmd = (u8)cmdArgs[2];
                    channel->vibratoRateChangeDelay = cmd * 16;
                    break;

                case 0xE3: // channel: set vibrato delay
                    cmd = (u8)cmdArgs[0];
                    channel->vibratoDelay = cmd * 16;
                    break;

                case 0xD4: // channel: set reverb volume
                    cmd = (u8)cmdArgs[0];
                    channel->reverb = cmd;
                    break;

                case 0xC6: { // channel: set soundFont
                    u16 result;

                    {
                        s32 ok = MM_SeqFontFromArg((u16)cmdArgs[0], &result);
                        MergeShipTest_Log("MMSEQ C6 chan=%p arg=%d ok=%d font=%d cached=%d numFonts=%d",
                                          (void*)channel, cmdArgs[0], ok, ok ? result : -1,
                                          ok ? (AudioHeap_SearchCaches(FONT_TABLE, CACHE_EITHER, result) != NULL) : -1,
                                          sMMSeqNumFonts);
                    }
                    if (!MM_SeqFontFromArg((u16)cmdArgs[0], &result)) {
                        break;
                    }
                    if (AudioHeap_SearchCaches(FONT_TABLE, CACHE_EITHER, result)) {
                        channel->fontId = result;
                    }
                    break;
                }

                case 0xC7: // channel: write into sequence script
                    cmd = (u8)cmdArgs[0];
                    cmdArgU16 = (u16)cmdArgs[1];
                    seqData = &seqPlayer->seqData[cmdArgU16];
                    seqData[0] = (u8)scriptState->value + cmd;
                    break;

                case 0xC8: // channel: subtract -> set value
                case 0xCC: // channel: set value
                case 0xC9: // channel: `bit and` -> set value
                    cmdArgS8 = (s8)cmdArgs[0];

                    if (cmd == 0xC8) {
                        scriptState->value -= cmdArgS8;
                    } else if (cmd == 0xCC) {
                        scriptState->value = cmdArgS8;
                    } else {
                        scriptState->value &= cmdArgS8;
                    }
                    break;

                case 0xCD: // channel: disable channel
                    cmd = (u8)cmdArgs[0];
                    MM_SequenceChannelDisable(seqPlayer->channels[cmd & 0xF]);
                    break;

                case 0xCA: // channel: set mute behavior
                    cmd = (u8)cmdArgs[0];
                    channel->muteBehavior = cmd;
                    channel->changes.s.volume = true;
                    break;

                case 0xCB: // channel: read sequence -> set value
                    cmdArgU16 = (u16)cmdArgs[0];
                    scriptState->value = *(seqPlayer->seqData + (u32)(cmdArgU16 + scriptState->value));
                    break;

                case 0xCE: // channel: set unk_22
                    cmdArgU16 = (u16)cmdArgs[0];
                    channel->unk_22 = cmdArgU16;
                    break;

                case 0xCF: // channel: write large into sequence script
                    cmdArgU16 = (u16)cmdArgs[0];
                    seqData = &seqPlayer->seqData[cmdArgU16];
                    seqData[0] = (channel->unk_22 >> 8) & 0xFF;
                    seqData[1] = channel->unk_22 & 0xFF;
                    break;

                case 0xD0: // channel: stereo headset effects
                    cmd = (u8)cmdArgs[0];
                    if (cmd & 0x80) {
                        channel->stereoHeadsetEffects = true;
                    } else {
                        channel->stereoHeadsetEffects = false;
                    }
                    channel->stereo.asByte = cmd & 0x7F;
                    break;

                case 0xD1: // channel: set note allocation policy
                    cmd = (u8)cmdArgs[0];
                    channel->noteAllocPolicy = cmd;
                    break;

                case 0xD2: // channel: set sustain
                    cmd = (u8)cmdArgs[0];
                    channel->adsr.sustain = cmd;
                    break;

                case 0xE5: // channel: set reverb index
                    cmd = (u8)cmdArgs[0];
                    channel->reverbIndex = cmd;
                    break;

                case 0xE4: // channel: dyncall
                    if (scriptState->value != -1) {
                        data = (*channel->dynTable)[scriptState->value];
                        depth = scriptState->depth;
                        //! @bug: Missing a stack depth check here
                        scriptState->stack[depth] = scriptState->pc;
                        scriptState->depth++;
                        cmdArgU16 = (u16)((data[0] << 8) + data[1]);
                        scriptState->pc = seqPlayer->seqData + cmdArgU16;
                    }
                    break;

                case 0xE6: // channel: set book offset
                    cmd = (u8)cmdArgs[0];
                    channel->bookOffset = cmd;
                    break;

                case 0xE7: // channel: load channel params
                    cmdArgU16 = (u16)cmdArgs[0];
                    data = &seqPlayer->seqData[cmdArgU16];
                    channel->muteBehavior = *data++;
                    channel->noteAllocPolicy = *data++;
                    MM_SetChannelPriorities(channel, *data++);
                    channel->transposition = (s8)*data++;
                    channel->newPan = *data++;
                    channel->panChannelWeight = *data++;
                    channel->reverb = *data++;
                    channel->reverbIndex = *data++;
                    //! @bug: Not marking reverb state as changed
                    channel->changes.s.pan = true;
                    break;

                case 0xE8: // channel: channel params
                    channel->muteBehavior = cmdArgs[0];
                    channel->noteAllocPolicy = cmdArgs[1];
                    cmd = (u8)cmdArgs[2];
                    MM_SetChannelPriorities(channel, cmd);
                    channel->transposition = (s8)MM_ScriptReadU8(scriptState);
                    channel->newPan = MM_ScriptReadU8(scriptState);
                    channel->panChannelWeight = MM_ScriptReadU8(scriptState);
                    channel->reverb = MM_ScriptReadU8(scriptState);
                    channel->reverbIndex = MM_ScriptReadU8(scriptState);
                    //! @bug: Not marking reverb state as changed
                    channel->changes.s.pan = true;
                    break;

                case 0xEC: // channel: reset vibrato
                    channel->vibratoExtentTarget = 0;
                    channel->vibratoExtentStart = 0;
                    channel->vibratoExtentChangeDelay = 0;
                    channel->vibratoRateTarget = 0;
                    channel->vibratoRateStart = 0;
                    channel->vibratoRateChangeDelay = 0;
                    channel->filter = NULL;
                    channel->unk_0C = 0; // gain
                    channel->adsr.sustain = 0;
                    channel->velocityRandomVariance = 0;
                    channel->gateTimeRandomVariance = 0;
                    channel->unk_0F = 0; // combFilterSize
                    channel->unk_20 = 0; // combFilterGain
                    channel->bookOffset = 0;
                    ext->startSamplePos = 0;
                    ext->unk_E0 = 0;
                    channel->freqScale = 1.0f;
                    break;

                case 0xE9: // channel: set note priority
                    MM_SetChannelPriorities(channel, (u8)cmdArgs[0]);
                    break;

                case 0xED: // channel: set hilo gain
                    cmd = (u8)cmdArgs[0];
                    channel->unk_0C = cmd;
                    break;

                case 0xB0: // channel: set filter
                    cmdArgU16 = (u16)cmdArgs[0];
                    data = seqPlayer->seqData + cmdArgU16;
                    channel->filter = (s16*)data;
                    break;

                case 0xB1: // channel: clear filter
                    channel->filter = NULL;
                    break;

                case 0xB3: // channel: load filter
                    cmd = cmdArgs[0];

                    if (channel->filter != NULL) {
                        lowBits = (cmd >> 4) & 0xF; // LowPassCutoff
                        cmd &= 0xF;                 // HighPassCutoff
                        AudioHeap_LoadFilter(channel->filter, lowBits, cmd);
                    }
                    break;

                case 0xB2: // channel: dynread sequence large
                    cmdArgU16 = (u16)cmdArgs[0];
                    channel->unk_22 =
                        BE16SWAP(*(u16*)(seqPlayer->seqData + (u32)(cmdArgU16 + scriptState->value * 2)));
                    break;

                case 0xB4: // channel: set dyntable large
                    channel->dynTable = (void*)&seqPlayer->seqData[channel->unk_22];
                    break;

                case 0xB5: // channel: read dyntable large
                    channel->unk_22 = BE16SWAP(((u16*)(channel->dynTable))[scriptState->value]);
                    break;

                case 0xB6: // channel: read dyntable
                    scriptState->value = (*channel->dynTable)[0][scriptState->value];
                    break;

                case 0xB7: // channel: random large
                    channel->unk_22 = (cmdArgs[0] == 0) ? (gAudioContext.audioRandom & 0xFFFF)
                                                        : (gAudioContext.audioRandom % cmdArgs[0]);
                    break;

                case 0xB8: // channel: random value
                    scriptState->value = (cmdArgs[0] == 0) ? (gAudioContext.audioRandom & 0xFFFF)
                                                           : (gAudioContext.audioRandom % cmdArgs[0]);
                    break;

                case 0xA8: // channel: random range large (MM's move of Ocarina of Time's 0xBD)
                    rand = Audio_NextRandom();
                    channel->unk_22 = (cmdArgs[0] == 0) ? (rand & 0xFFFF) : (rand % cmdArgs[0]);
                    channel->unk_22 += cmdArgs[1];
                    temp2 = (channel->unk_22 / 0x100) + 0x80;
                    param = channel->unk_22 % 0x100;
                    channel->unk_22 = (temp2 << 8) | param;
                    break;

                case 0xB9: // channel: set velocity random variance
                    channel->velocityRandomVariance = cmdArgs[0];
                    break;

                case 0xBA: // channel: set gatetime random variance
                    channel->gateTimeRandomVariance = cmdArgs[0];
                    break;

                case 0xBB: // channel: comb filter
                    channel->unk_0F = cmdArgs[0]; // combFilterSize
                    channel->unk_20 = cmdArgs[1]; // combFilterGain
                    break;

                case 0xBC: // channel: add large
                    channel->unk_22 += cmdArgs[0];
                    break;

                case 0xBD: // channel: start sample position
                    ext->startSamplePos = cmdArgs[0];
                    break;

                case 0xBE: // channel: custom function
                    if (cmdArgs[0] < 5) {
                        if ((cmdArgs[0] < ARRAY_COUNT(sMMCustomSeqFunctions)) &&
                            (sMMCustomSeqFunctions[cmdArgs[0]] != NULL)) {
                            scriptState->value = sMMCustomSeqFunctions[cmdArgs[0]](scriptState->value, channel);
                        }
                    }
                    break;

                case 0xA0: // channel: read from SfxChannelState using arg
                case 0xA1: // channel: read from SfxChannelState using unk_22
                case 0xA2: // channel: write to SfxChannelState using arg
                case 0xA3: // channel: write to SfxChannelState using unk_22
                    if ((cmd == 0xA0) || (cmd == 0xA2)) {
                        cmdArgU16 = (u16)cmdArgs[0];
                    } else {
                        cmdArgU16 = channel->unk_22;
                    }

                    if (ext->sfxState != NULL) {
                        if ((cmd == 0xA0) || (cmd == 0xA1)) {
                            scriptState->value = ext->sfxState[cmdArgU16];
                        } else {
                            ext->sfxState[cmdArgU16] = scriptState->value;
                        }
                    }
                    break;

                case 0xA4: // channel: surround effect index
                    ext->surroundEffectIndex = cmdArgs[0];
                    break;

                case 0xA5: // channel: value += channel index
                    scriptState->value += ext->channelIndex;
                    break;

                case 0xA6: // channel: write into sequence script at channel index
                    cmd = (u8)cmdArgs[0];
                    cmdArgU16 = (u16)cmdArgs[1];
                    seqData = seqPlayer->seqData + (u32)(cmdArgU16 + ext->channelIndex);
                    seqData[0] = (u8)scriptState->value + cmd;
                    break;

                case 0xA7: // channel: shift value
                    new_var2 = (cmdArgs[0] & 0x80);
                    new_var = (scriptState->value & 0x80);

                    if (!new_var2) {
                        phi_v0_3 = scriptState->value << (cmdArgs[0] & 0xF);
                    } else {
                        phi_v0_3 = scriptState->value >> (cmdArgs[0] & 0xF);
                    }

                    if (cmdArgs[0] & 0x40) {
                        phi_v0_3 &= (u8)~0x80;
                        phi_v0_3 |= new_var;
                    }

                    scriptState->value = phi_v0_3;
                    break;
            }
            continue;
        }

        // Commands 0x70 - 0x9F
        if (cmd >= 0x70) {
            lowBits = cmd & 0x7;

            if (((cmd & 0xF8) != 0x70) && (lowBits >= 4)) {
                lowBits = 0;
            }

            switch (cmd & 0xF8) {
                case 0x80: // channel: test layer is finished
                    if (channel->layers[lowBits] != NULL) {
                        scriptState->value = channel->layers[lowBits]->finished;
                    } else {
                        scriptState->value = -1;
                    }
                    break;

                case 0x88: // channel: set layer
                    cmdArgU16 = MM_ScriptReadS16(scriptState);
                    if (!MM_SeqChannelSetLayer(channel, lowBits)) {
                        channel->layers[lowBits]->scriptState.pc = &seqPlayer->seqData[cmdArgU16];
                    }
                    break;

                case 0x90: // channel: free layer
                    MM_SeqLayerFree(channel, lowBits);
                    break;

                case 0x98: // channel: dynset layer
                    if ((scriptState->value != -1) && (MM_SeqChannelSetLayer(channel, lowBits) != -1)) {
                        data = (*channel->dynTable)[scriptState->value];
                        cmdArgU16 = (data[0] << 8) + data[1];
                        channel->layers[lowBits]->scriptState.pc = &seqPlayer->seqData[cmdArgU16];
                    }
                    break;

                case 0x70: // channel: io write value
                    channel->soundScriptIO[lowBits] = scriptState->value;
                    break;

                case 0x78: // channel: set layer relative
                    temp1 = MM_ScriptReadS16(scriptState);
                    if (!MM_SeqChannelSetLayer(channel, lowBits)) {
                        channel->layers[lowBits]->scriptState.pc = &scriptState->pc[temp1];
                    }
                    break;
            }
            continue;
        }

        // Commands 0x00 - 0x6F
        lowBits = cmd & 0xF;

        switch (cmd & 0xF0) {
            case 0x00: // channel: delay short
                channel->delay = lowBits;
                if (lowBits == 0) {
                    break;
                }
                goto exit_loop;

            case 0x10: // channel: load sample
                if (lowBits < 8) {
                    channel->soundScriptIO[lowBits] = SEQ_IO_VAL_NONE;
                    if (AudioLoad_SlowLoadSample(channel->fontId, scriptState->value,
                                                 &channel->soundScriptIO[lowBits]) == -1) {}
                } else {
                    lowBits -= 8;
                    channel->soundScriptIO[lowBits] = SEQ_IO_VAL_NONE;
                    if (AudioLoad_SlowLoadSample(channel->fontId, channel->unk_22 + 0x100,
                                                 &channel->soundScriptIO[lowBits]) == -1) {}
                }
                break;

            case 0x60: // channel: io read value
                scriptState->value = channel->soundScriptIO[lowBits];
                if (lowBits < 2) {
                    channel->soundScriptIO[lowBits] = SEQ_IO_VAL_NONE;
                }
                break;

            case 0x50: // channel: io read value subtract
                scriptState->value -= channel->soundScriptIO[lowBits];
                break;

            case 0x20: // channel: start channel
                cmdArgU16 = MM_ScriptReadS16(scriptState);
                MM_SequenceChannelEnable(seqPlayer, lowBits, &seqPlayer->seqData[cmdArgU16]);
                break;

            case 0x30: // channel: io write value 2
                cmd = MM_ScriptReadU8(scriptState);
                seqPlayer->channels[lowBits]->soundScriptIO[cmd & 7] = scriptState->value;
                break;

            case 0x40: // channel: io read value 2
                cmd = MM_ScriptReadU8(scriptState);
                scriptState->value = seqPlayer->channels[lowBits]->soundScriptIO[cmd & 7];
                break;
        }
    }
exit_loop:

    for (i = 0; i < ARRAY_COUNT(channel->layers); i++) {
        if (channel->layers[i] != NULL) {
            MM_SeqLayerProcessScript(channel->layers[i]);
        }
    }
}

// ---- sequence script (MM AudioScript_SequencePlayerProcessSequence) ------------------------------------

static void MM_SequencePlayerProcessSequence(SequencePlayer* seqPlayer) {
    u8 cmd;
    u8 cmdLowBits;
    SeqScriptState* seqScript = &seqPlayer->scriptState;
    s16 tempS;
    u16 temp;
    s32 i;
    s32 value;
    u8* data1;
    u8* data2;
    u8* data3;
    u8* data4;
    s32 tempoChange;
    s32 j;
    SequenceChannel* channel;
    u16* new_var;
    s32 delay;

    if (!seqPlayer->enabled) {
        return;
    }

    // (MM checks the sequence's and default font's load status here; this player's sequence is a private copy
    // and its fonts are permanent.)
    if (!AudioLoad_IsFontLoadComplete(seqPlayer->defaultFont)) {
        MM_SequencePlayerDisable(seqPlayer);
        return;
    }

    if (seqPlayer->muted && (seqPlayer->muteBehavior & MUTE_FLAGS_STOP_SCRIPT)) {
        return;
    }

    seqPlayer->scriptCounter++;

    tempoChange = seqPlayer->tempo + (s16)seqPlayer->unk_0C;
    if (tempoChange > gAudioContext.tempoInternalToExternal) {
        tempoChange = gAudioContext.tempoInternalToExternal;
    }

    seqPlayer->tempoAcc += tempoChange;

    if (seqPlayer->tempoAcc < gAudioContext.tempoInternalToExternal) {
        return;
    }

    seqPlayer->tempoAcc -= (u16)gAudioContext.tempoInternalToExternal;
    sMMSeqUnk16++;

    if (seqPlayer->stopScript == true) {
        return;
    }

    if (seqPlayer->delay > 1) {
        seqPlayer->delay--;
    } else {
        seqPlayer->recalculateVolume = true;

        while (true) {
            cmd = MM_ScriptReadU8(seqScript);

            // 0xF2 and above are "flow control" commands, including termination.
            if (cmd >= 0xF2) {
                delay = MM_HandleScriptFlowControl(seqPlayer, seqScript, cmd,
                                                   MM_GetScriptControlFlowArgument(&seqPlayer->scriptState, cmd));

                if (delay != 0) {
                    if (delay == -1) {
                        MM_SequencePlayerDisable(seqPlayer);
                    } else {
                        seqPlayer->delay = delay;
                    }
                    break;
                }
                continue;
            }

            // Commands 0xC0 - 0xF1
            if (cmd >= 0xC0) {
                switch (cmd) {
                    case 0xF1: // seqPlayer: reserve notes
                        Audio_NotePoolClear(&seqPlayer->notePool);
                        cmd = MM_ScriptReadU8(seqScript);
                        Audio_NotePoolFill(&seqPlayer->notePool, cmd);
                        break;

                    case 0xF0: // seqPlayer: unreserve notes
                        Audio_NotePoolClear(&seqPlayer->notePool);
                        break;

                    case 0xDF: // seqPlayer: transpose
                        seqPlayer->transposition = 0;
                        // fallthrough
                    case 0xDE: // seqPlayer: transpose relative
                        seqPlayer->transposition += (s8)MM_ScriptReadU8(seqScript);
                        break;

                    case 0xDD: // seqPlayer: set tempo
                        seqPlayer->tempo = MM_ScriptReadU8(seqScript) * TATUMS_PER_BEAT;
                        if (seqPlayer->tempo > gAudioContext.tempoInternalToExternal) {
                            seqPlayer->tempo = gAudioContext.tempoInternalToExternal;
                        }

                        if ((s16)seqPlayer->tempo <= 0) {
                            seqPlayer->tempo = 1;
                        }
                        break;

                    case 0xDC: // seqPlayer: add tempo
                        seqPlayer->unk_0C = (s8)MM_ScriptReadU8(seqScript) * TATUMS_PER_BEAT;
                        break;

                    case 0xDA: // seqPlayer: change volume
                        cmd = MM_ScriptReadU8(seqScript);
                        temp = MM_ScriptReadS16(seqScript);
                        switch (cmd) {
                            case SEQPLAYER_STATE_0:
                            case SEQPLAYER_STATE_FADE_IN:
                                if (seqPlayer->state != SEQPLAYER_STATE_FADE_OUT) {
                                    seqPlayer->fadeTimerUnkEu = temp;
                                    seqPlayer->state = cmd;
                                }
                                break;

                            case SEQPLAYER_STATE_FADE_OUT:
                                seqPlayer->fadeTimer = temp;
                                seqPlayer->state = cmd;
                                seqPlayer->fadeVelocity = (0.0f - seqPlayer->fadeVolume) / (s32)seqPlayer->fadeTimer;
                                break;
                        }
                        break;

                    case 0xDB: // seqPlayer: set volume
                        value = MM_ScriptReadU8(seqScript);
                        switch (seqPlayer->state) {
                            case SEQPLAYER_STATE_FADE_IN:
                                seqPlayer->state = SEQPLAYER_STATE_0;
                                seqPlayer->fadeVolume = 0.0f;
                                // fallthrough
                            case SEQPLAYER_STATE_0:
                                seqPlayer->fadeTimer = seqPlayer->fadeTimerUnkEu;
                                if (seqPlayer->fadeTimerUnkEu != 0) {
                                    seqPlayer->fadeVelocity =
                                        ((value / 127.0f) - seqPlayer->fadeVolume) / (s32)seqPlayer->fadeTimer;
                                } else {
                                    seqPlayer->fadeVolume = value / 127.0f;
                                }
                                break;

                            case SEQPLAYER_STATE_FADE_OUT:
                                break;
                        }
                        break;

                    case 0xD9: // seqPlayer: set volume scale
                        seqPlayer->fadeVolumeScale = (s8)MM_ScriptReadU8(seqScript) / 127.0f;
                        break;

                    case 0xD7: // seqPlayer: initialize channels
                        temp = MM_ScriptReadS16(seqScript);
                        MM_SequencePlayerSetupChannels(seqPlayer, temp);
                        break;

                    case 0xD6: // seqPlayer: disable channels
                        MM_ScriptReadS16(seqScript);
                        break;

                    case 0xD5: // seqPlayer: set mute scale
                        seqPlayer->muteVolumeScale = (s8)MM_ScriptReadU8(seqScript) / 127.0f;
                        break;

                    case 0xD4: // seqPlayer: mute
                        seqPlayer->muted = true;
                        break;

                    case 0xD3: // seqPlayer: set mute behavior
                        seqPlayer->muteBehavior = MM_ScriptReadU8(seqScript);
                        break;

                    case 0xD1: // seqPlayer: set short note gatetime table
                    case 0xD2: // seqPlayer: set short note velocity table
                        temp = MM_ScriptReadS16(seqScript);
                        data3 = &seqPlayer->seqData[temp];
                        if (cmd == 0xD2) {
                            seqPlayer->shortNoteVelocityTable = data3;
                        } else {
                            seqPlayer->shortNoteGateTimeTable = data3;
                        }
                        break;

                    case 0xD0: // seqPlayer: set note allocation policy
                        seqPlayer->noteAllocPolicy = MM_ScriptReadU8(seqScript);
                        break;

                    case 0xCE: // seqPlayer: random value
                        cmd = MM_ScriptReadU8(seqScript);
                        if (cmd == 0) {
                            seqScript->value = (gAudioContext.audioRandom >> 2) & 0xFF;
                        } else {
                            seqScript->value = (gAudioContext.audioRandom >> 2) % cmd;
                        }
                        break;

                    case 0xCD: // seqPlayer: dyncall
                        temp = MM_ScriptReadS16(seqScript);
                        if ((seqScript->value != -1) && (seqScript->depth != 3)) {
                            data1 = seqPlayer->seqData + (u32)(temp + (seqScript->value << 1));
                            seqScript->stack[seqScript->depth] = seqScript->pc;
                            seqScript->depth++;
                            temp = (data1[0] << 8) + data1[1];
                            seqScript->pc = &seqPlayer->seqData[temp];
                        }
                        break;

                    case 0xCC: // seqPlayer: set value
                        seqScript->value = MM_ScriptReadU8(seqScript);
                        break;

                    case 0xC9: // seqPlayer: `bit and` -> set value
                        seqScript->value &= MM_ScriptReadU8(seqScript);
                        break;

                    case 0xC8: // seqPlayer: subtract -> set value
                        seqScript->value -= MM_ScriptReadU8(seqScript);
                        break;

                    case 0xC7: // seqPlayer: write into sequence script
                        cmd = MM_ScriptReadU8(seqScript);
                        temp = MM_ScriptReadS16(seqScript);
                        data2 = &seqPlayer->seqData[temp];
                        *data2 = (u8)seqScript->value + cmd;
                        break;

                    case 0xC2: // seqPlayer: dynamic jump
                        temp = MM_ScriptReadS16(seqScript);
                        if (seqScript->value != -1) {
                            data4 = seqPlayer->seqData + (u32)(temp + (seqScript->value << 1));

                            temp = (data4[0] << 8) + data4[1];
                            seqScript->pc = &seqPlayer->seqData[temp];
                        }
                        break;

                    case 0xC6: // seqPlayer: stop script
                        seqPlayer->stopScript = true;
                        return;

                    case 0xC5: // seqPlayer: set unk_16
                        sMMSeqUnk16 = MM_ScriptReadS16(seqScript);
                        break;

                    case 0xEF: // seqPlayer:
                        MM_ScriptReadS16(seqScript);
                        MM_ScriptReadU8(seqScript);
                        break;

                    case 0xC4: // seqPlayer: start sequence
                        // Starts a sequence of MM's table on a player of MM's numbering; sequence 0 never uses it
                        // and neither exists here, so the arguments are consumed and ignored.
                        MM_ScriptReadU8(seqScript);
                        MM_ScriptReadU8(seqScript);
                        break;

                    case 0xC3: // seqPlayer: mute channels
                        temp = MM_ScriptReadS16(seqScript);
                        if (seqScript->value != -1) {
                            new_var = (u16*)(seqPlayer->seqData + (u32)(temp + seqScript->value * 2));
                            // (MM reads the u16 natively from big-endian sequence data.)
                            temp = BE16SWAP(*new_var);

                            for (i = 0; i < ARRAY_COUNT(seqPlayer->channels); i++) {
                                seqPlayer->channels[i]->stopSomething2 = temp & 1;
                                temp = temp >> 1;
                            }
                        }
                        break;
                }
                continue;
            }

            // Commands 0x00 - 0xBF
            cmdLowBits = cmd & 0x0F;

            switch (cmd & 0xF0) {
                case 0x00: // seqPlayer: test channel disabled
                    seqScript->value = seqPlayer->channels[cmdLowBits]->enabled ^ 1;
                    break;

                case 0x50: // seqPlayer: io read value subtract
                    seqScript->value -= seqPlayer->soundScriptIO[cmdLowBits & 7];
                    break;

                case 0x70: // seqPlayer: io write value
                    seqPlayer->soundScriptIO[cmdLowBits & 7] = seqScript->value;
                    break;

                case 0x80: // seqPlayer: io read value
                    seqScript->value = seqPlayer->soundScriptIO[cmdLowBits & 7];
                    if (cmdLowBits < 2) {
                        seqPlayer->soundScriptIO[cmdLowBits] = SEQ_IO_VAL_NONE;
                    }
                    break;

                case 0x40: // seqPlayer: disable channel
                    MM_SequenceChannelDisable(seqPlayer->channels[cmdLowBits]);
                    break;

                case 0x90: // seqPlayer: start channel
                    temp = MM_ScriptReadS16(seqScript);
                    MM_SequenceChannelEnable(seqPlayer, cmdLowBits, (void*)&seqPlayer->seqData[temp]);
                    break;

                case 0xA0: // seqPlayer: start channel relative
                    tempS = MM_ScriptReadS16(seqScript);
                    MM_SequenceChannelEnable(seqPlayer, cmdLowBits, (void*)&seqScript->pc[tempS]);
                    break;

                case 0xB0: // seqPlayer: load sequence
                    // A slow load of a sequence of MM's table; not used by sequence 0. Arguments consumed.
                    MM_ScriptReadU8(seqScript);
                    MM_ScriptReadS16(seqScript);
                    break;

                case 0x60: // seqPlayer: async load
                    // An async load from MM's tables; not used by sequence 0. Arguments consumed.
                    MM_ScriptReadU8(seqScript);
                    MM_ScriptReadU8(seqScript);
                    break;
            }
        }
    }

    for (j = 0; j < 16; j++) {
        channel = seqPlayer->channels[j];
        if (channel->enabled) {
            MM_SequenceChannelProcessScript(channel);
        }
    }
}

// MM AudioScript_SequencePlayerProcessSound. Channels go through Ocarina of Time's
// Audio_SequenceChannelProcessSound, which is MM's AudioScript_SequenceChannelProcessSound with SoH's per-player
// volume (gameVolume) in place of 2S2H's portVolumeScale.
static void MM_SequencePlayerProcessSound(SequencePlayer* seqPlayer) {
    s32 i;

    if ((seqPlayer->fadeTimer != 0) && (seqPlayer->skipTicks == 0)) {
        seqPlayer->fadeVolume += seqPlayer->fadeVelocity;
        seqPlayer->recalculateVolume = true;

        if (seqPlayer->fadeVolume > 1.0f) {
            seqPlayer->fadeVolume = 1.0f;
        }
        if (seqPlayer->fadeVolume < 0.0f) {
            seqPlayer->fadeVolume = 0.0f;
        }

        seqPlayer->fadeTimer--;
        if ((seqPlayer->fadeTimer == 0) && (seqPlayer->state == SEQPLAYER_STATE_FADE_OUT)) {
            MM_SequencePlayerDisable(seqPlayer);
            return;
        }
    }

    if (seqPlayer->recalculateVolume) {
        seqPlayer->appliedFadeVolume = seqPlayer->fadeVolume * seqPlayer->fadeVolumeScale;
    }

    for (i = 0; i < 16; i++) {
        if (seqPlayer->channels[i]->enabled == true) {
            Audio_SequenceChannelProcessSound(seqPlayer->channels[i], seqPlayer->recalculateVolume,
                                              seqPlayer->unk_0b1);
        }
    }

    seqPlayer->recalculateVolume = false;
}

// ---- engine entry points --------------------------------------------------------------------------------

// MM AudioLoad_SyncInitSeqPlayerInternal + AudioThread_SetFadeInTimer + AudioSfx_Init's channel setup
// (AUDIOCMD_CHANNEL_SET_SFX_STATE for every channel, AUDIOCMD_GLOBAL_SET_CUSTOM_FUNCTION(SEQ_0)).
static void MM_StartSequence(s32 fadeTimer) {
    SequencePlayer* seqPlayer = &gMergeShipMMSeqPlayer;
    s32 seqSize = 0;
    u8* seqData;
    s32 i;

    if (!sMMInitialized) {
        MM_InitSequencePlayer();
    }

    MM_SequencePlayerDisable(seqPlayer);

    MergeShip_MMAudio_RegisterFonts();
    seqData = MergeShip_MMAudio_GetSeqData(&seqSize);
    sMMSeqNumFonts = MergeShip_MMAudio_GetSeqFonts(sMMSeqFonts, ARRAY_COUNT(sMMSeqFonts));
    if ((seqData == NULL) || (sMMSeqNumFonts <= 0)) {
        gMergeShipMMAudioUnavailable = true;
        return;
    }

    for (i = 0; i < sMMSeqNumFonts; i++) {
        AudioLoad_SyncLoadFont(sMMSeqFonts[i]);
    }

    MM_ResetSequencePlayer(seqPlayer);
    seqPlayer->seqId = 0;
    seqPlayer->defaultFont = sMMSeqFonts[sMMSeqNumFonts - 1];
    {
        // MM's channels start zeroed, so a channel sequence 0 never sets up (the voice channel, 14) plays
        // from MM font 0. Here font 0 is Ocarina of Time's, so start every channel on MM font 0's slot.
        u8 mmFont0 = MergeShip_MMAudio_MapFont(0);

        MergeShipTest_Log("MMSEQ start: MM font 0 -> %d, default %d", mmFont0, seqPlayer->defaultFont);
        for (i = 0; i < 16; i++) {
            seqPlayer->channels[i]->fontId = (mmFont0 != 0xFF) ? mmFont0 : seqPlayer->defaultFont;
        }
    }
    seqPlayer->seqData = seqData;
    seqPlayer->enabled = true;
    seqPlayer->scriptState.pc = seqData;
    seqPlayer->scriptState.depth = 0;
    seqPlayer->delay = 0;
    seqPlayer->finished = false;
    seqPlayer->playerIdx = MERGESHIP_MM_SEQ_PLAYER;
    seqPlayer->skipTicks = 0;

    // AudioThread_SetFadeInTimer
    if (fadeTimer != 0) {
        seqPlayer->state = SEQPLAYER_STATE_FADE_IN;
        seqPlayer->fadeTimerUnkEu = fadeTimer;
        seqPlayer->fadeTimer = fadeTimer;
        seqPlayer->fadeVolume = 0.0f;
        seqPlayer->fadeVelocity = 0.0f;
    }

    for (i = 0; i < 16; i++) {
        sMMChannelExt[i].sfxState = MergeShip_MMSfx_GetChannelState(i);
    }
    sMMCustomSeqFunctions[0] = MergeShip_MMSfx_SetFreqAndStereoBits;
}

void MergeShip_MMAudio_OnHeapInit(void) {
    // The notes and every channel's note pool were reallocated; start over from a disabled player.
    MM_InitSequencePlayer();
    gMergeShipMMAudioResetCount++;
}

void MergeShip_MMAudio_OnResetBegin(void) {
    if (sMMInitialized) {
        gMergeShipMMSeqPlayer.finished = true;
        MM_SequencePlayerDisable(&gMergeShipMMSeqPlayer);
    }
}

void MergeShip_MMAudio_ProcessSequence(void) {
    SequencePlayer* seqPlayer = &gMergeShipMMSeqPlayer;
    SequencePlayer* ootSfxPlayer = &gAudioContext.seqPlayers[SEQ_PLAYER_SFX];

    if (!sMMInitialized || !seqPlayer->enabled) {
        return;
    }

    // Ocarina of Time's volume controls for sound effects (the menu's sfx volume, the sfx player's volume
    // scale) apply to Majora's Mask's sound effects too.
    if ((seqPlayer->gameVolume != ootSfxPlayer->gameVolume) ||
        (seqPlayer->fadeVolumeScale != ootSfxPlayer->fadeVolumeScale)) {
        seqPlayer->gameVolume = ootSfxPlayer->gameVolume;
        seqPlayer->fadeVolumeScale = ootSfxPlayer->fadeVolumeScale;
        seqPlayer->recalculateVolume = true;
    }

    MM_SequencePlayerProcessSequence(seqPlayer);
    if (seqPlayer->enabled) {
        MM_SequencePlayerProcessSound(seqPlayer);
    }
}

void MergeShip_MMAudio_SetMuted(s32 muted) {
    if (sMMInitialized) {
        gMergeShipMMSeqPlayer.muted = muted ? true : false;
        gMergeShipMMSeqPlayer.recalculateVolume = true;
    }
}

// MM AudioThread_ProcessChannelCmd / ProcessSeqPlayerCmd for the commands MM's sfx code sends, plus this
// player's start (0x70) and stop (0x71).
void MergeShip_MMAudio_ProcessCmd(AudioCmd* cmd) {
    SequencePlayer* seqPlayer = &gMergeShipMMSeqPlayer;
    SequenceChannel* channel;

    if (cmd->op == 0x70) {
        MM_StartSequence(cmd->asInt);
        return;
    }
    if (!sMMInitialized) {
        return;
    }
    if (cmd->op == 0x71) {
        seqPlayer->finished = true;
        MM_SequencePlayerDisable(seqPlayer);
        return;
    }

    if (cmd->op == 0x46) {
        // seqPlayer io port
        seqPlayer->soundScriptIO[cmd->arg2 & 7] = cmd->asSbyte;
        return;
    }
    if (cmd->op == 0x41) {
        if (seqPlayer->fadeVolumeScale != cmd->asFloat) {
            seqPlayer->fadeVolumeScale = cmd->asFloat;
            seqPlayer->recalculateVolume = true;
        }
        return;
    }

    if (cmd->arg1 >= 16) {
        return;
    }
    channel = seqPlayer->channels[cmd->arg1];

    switch (cmd->op) {
        case 0x01: // volume scale
            if (channel->volumeScale != cmd->asFloat) {
                channel->volumeScale = cmd->asFloat;
                channel->changes.s.volume = true;
            }
            break;

        case 0x06: // io port
            if (cmd->arg2 < 8) {
                channel->soundScriptIO[cmd->arg2] = cmd->asSbyte;
            }
            break;

        case 0x0A: // vibrato depth
            channel->vibratoExtentTarget = cmd->asUbyte * 8;
            channel->vibratoExtentChangeDelay = 1;
            break;

        case 0x0B: // vibrato rate
            channel->vibratoRateTarget = cmd->asUbyte * 32;
            channel->vibratoRateChangeDelay = 1;
            break;

        default:
            break;
    }
}

// ---- playback hooks -------------------------------------------------------------------------------------

// MM AudioPlayback_NoteInitForLayer's start sample position.
void MergeShip_MMAudio_NoteInitForLayer(Note* note, SequenceLayer* layer) {
    SequenceChannel* channel;
    MMChannelExt* ext;
    s32 instId;

    note->unk_BC = 0;
    if (!MM_IsMMLayer(layer) || !MM_IsMMChannel(layer->channel)) {
        return;
    }

    channel = layer->channel;
    ext = MM_CHAN_EXT(channel);
    instId = layer->instOrWave;
    if (instId == 0xFF) {
        instId = channel->instOrWave;
    }

    if ((instId >= 0x80) && (instId < 0xC0)) {
        // synthetic wave: no start position
    } else if ((layer->sound == NULL) || (layer->sound->sample == NULL) || (layer->sound->sample->loop == NULL)) {
        // no sample to start into
    } else if (ext->startSamplePos == 1) {
        note->unk_BC = layer->sound->sample->loop->start;
    } else {
        note->unk_BC = ext->startSamplePos;
        if (note->unk_BC >= layer->sound->sample->loop->loopEnd) {
            note->unk_BC = 0;
        }
    }
}

// MM AudioPlayback_ProcessNotes / AudioPlayback_SeqLayerDecayRelease: targetReverbVol and gain.
void MergeShip_MMAudio_LayerAttrs(SequenceLayer* layer, u8* reverb, u8* gain) {
    MMLayerExt* ext;

    if (!MM_IsMMLayer(layer)) {
        return;
    }

    ext = MM_LAYER_EXT(layer);
    if (!(ext->unk_0A & MM_LAYER_UNK0A_BIT2)) {
        *reverb = ext->targetReverbVol;
    }
    if (!(ext->unk_0A & MM_LAYER_UNK0A_BIT9)) {
        *gain = 0;
    }
}

// MM AudioEffects_InitVibrato: the channel's vibrato when the layer's useVibrato bit is set, else the layer's.
SequenceChannel* MergeShip_MMAudio_VibratoChannel(SequenceLayer* layer, SequenceChannel* channel) {
    if (MM_IsMMLayer(layer) && !(MM_LAYER_EXT(layer)->unk_0A & MM_LAYER_UNK0A_USE_VIBRATO)) {
        return &sMMLayerVibrato;
    }
    return channel;
}
