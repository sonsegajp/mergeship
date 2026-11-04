// MergeShip2 -- Majora's Mask's Pictograph Box in Hyrule (MergeShipPicto.h). Ported from 2ship2harkinian/mm:
// z_player.c (func_80831814, Player_ActionHandler_13, Player_Action_43, Player_SetAction, Player_UpdateInterface),
// z_parameter.c (Interface_UpdateButtonsPart1's sPictoState branches, Interface_Draw's picto drawing), z_play.c
// (Play_TakePictoPhoto, Play_ConvertRgba16ToIntensityImage, Play_CompressI8ToI5, Play_DecompressI5ToI8, the capture in
// Play_Draw as 2S2H does it) and z_snap.c (Snap_RecordPictographedActors).
//
// Name mapping: MM unk_AA5 = OoT unk_6AD (PLAYER_UNKAA5_2 = MERGESHIP_UNK6AD_PICTO_BOX), ACTORCTX_FLAG_PICTO_BOX_ON =
// sPictoBoxOn, R_PICTO_PHOTO_STATE = sPictoPhotoState, gHiBuffer.pictoPhotoI8 = sPictoPhotoI8, QUEST_PICTOGRAPH =
// gMergeShipPictoHasPhoto, bButtonInterfaceDoActionActive / bButtonInterfaceDoAction = interfaceCtx unk_1FA / unk_1FC,
// MM hudVisibility = OoT hudVisibilityMode (MM HUD_VISIBILITY_NONE = HUD_VISIBILITY_NOTHING, IDLE = NO_CHANGE).

#include "global.h"
#include "soh/framebuffer_effects.h"
#include "soh/MergeShip/MergeShipPicto.h"
#include "soh/MergeShip/MergeShipMMItems.h"
#include "soh/MergeShip/MergeShipMMSfx.h"
#include "soh/MergeShip/MergeShipTransformFx.h"
#include "soh/MergeShip/MergeShipHud.h"
#include "mm/interface/parameter_static/parameter_static.h"

void MergeShipTest_Log(const char* fmt, ...);

// MM PictoBoxState (z_parameter.c).
typedef enum {
    /* 0 */ PICTO_BOX_STATE_OFF,         // Not using the pictograph
    /* 1 */ PICTO_BOX_STATE_LENS,        // Looking through the lens of the pictograph
    /* 2 */ PICTO_BOX_STATE_SETUP_PHOTO, // Looking at the photo currently taken
    /* 3 */ PICTO_BOX_STATE_PHOTO
} MergeShipPictoBoxState;

// MM PictoPhotoState (z64play.h).
typedef enum {
    /* 0 */ PICTO_PHOTO_STATE_OFF,
    /* 1 */ PICTO_PHOTO_STATE_SETUP,
    /* 2 */ PICTO_PHOTO_STATE_PROCESS,
    /* 3 */ PICTO_PHOTO_STATE_READY
} MergeShipPictoPhotoState;

// MM title_setup.c's YREG defaults for the viewfinder (R_PICTO_FOCUS_*).
#define PICTO_FOCUS_BORDER_TOPLEFT_X 80
#define PICTO_FOCUS_BORDER_TOPLEFT_Y 60
#define PICTO_FOCUS_BORDER_TOPRIGHT_X 220
#define PICTO_FOCUS_BORDER_TOPRIGHT_Y 60
#define PICTO_FOCUS_BORDER_BOTTOMLEFT_X 80
#define PICTO_FOCUS_BORDER_BOTTOMLEFT_Y 160
#define PICTO_FOCUS_BORDER_BOTTOMRIGHT_X 220
#define PICTO_FOCUS_BORDER_BOTTOMRIGHT_Y 160
#define PICTO_FOCUS_ICON_X 142
#define PICTO_FOCUS_ICON_Y 108
#define PICTO_FOCUS_TEXT_X 204
#define PICTO_FOCUS_TEXT_Y 177

// MM color.h.
#define PICTO_RGBA16_GET_R(pixel) (((pixel) >> 11) & 0x1F)
#define PICTO_RGBA16_GET_G(pixel) (((pixel) >> 6) & 0x1F)
#define PICTO_RGBA16_GET_B(pixel) (((pixel) >> 1) & 0x1F)

u8 gMergeShipPictoPhotoI5[MERGESHIP_PICTO_PHOTO_COMPRESSED_SIZE + 1];
u8 gMergeShipPictoHasPhoto = false;
u32 gMergeShipPictoFlags[2];

static s16 sPictoState = PICTO_BOX_STATE_OFF;
static s16 sPictoPhotoBeingTaken = false;
static s16 sPictoPhotoState = PICTO_PHOTO_STATE_OFF;
static s32 sPictoBoxOn = false;
// MM gHiBuffer.pictoPhotoI8.
static u8 sPictoPhotoI8[MERGESHIP_PICTO_PHOTO_SIZE];
// 2S2H reads the frame back into pauseBgPreRender.fbufSave (the z-buffer); a buffer of its own here (320x240 RGBA16).
static u16 sPictoFrameRgba16[SCREEN_WIDTH * SCREEN_HEIGHT];
// MM HUD_VISIBILITY_A_B also dims the minimap; Ocarina of Time's nearest mode keeps it (see OnPlayDrawEnd).
static s16 sPictoMinimapMaxAlpha = 255;

s32 MergeShip_Picto_Enabled(void) {
    return MergeShip_MMItems_Enabled() && CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMItems.Pictograph"), 1);
}

s32 MergeShip_Picto_HasPhoto(void) {
    return gMergeShipPictoHasPhoto;
}

s32 MergeShip_Picto_IsBoxOn(void) {
    return sPictoBoxOn;
}

void MergeShip_Picto_Reset(void) {
    sPictoState = PICTO_BOX_STATE_OFF;
    sPictoPhotoBeingTaken = false;
    sPictoPhotoState = PICTO_PHOTO_STATE_OFF;
    sPictoBoxOn = false;
    sPictoMinimapMaxAlpha = 255;
}

void MergeShip_Picto_OnSceneInit(void) {
    // MM Play_Init: R_PICTO_PHOTO_STATE = PICTO_PHOTO_STATE_OFF; Actor_InitContext clears actorCtx.flags. sPictoState is
    // z_parameter.c's static and survives; its branches see the flag down and put the box away.
    sPictoPhotoState = PICTO_PHOTO_STATE_OFF;
    sPictoBoxOn = false;
}

// ---- MM z_play.c ----------------------------------------------------------------------------------------------------

// How much each color component contributes to the intensity image (MM PLAY_INTENSITY_*).
#define PLAY_INTENSITY_RED 2
#define PLAY_INTENSITY_GREEN 4
#define PLAY_INTENSITY_BLUE 1
#define PLAY_INTENSITY_NORM (0x1F * PLAY_INTENSITY_RED + 0x1F * PLAY_INTENSITY_GREEN + 0x1F * PLAY_INTENSITY_BLUE)
#define PLAY_INTENSITY_MIX(r, g, b, m) \
    ((((r)*PLAY_INTENSITY_RED + (g)*PLAY_INTENSITY_GREEN + (b)*PLAY_INTENSITY_BLUE) * (m)) / PLAY_INTENSITY_NORM)

// MM Play_ConvertRgba16ToIntensityImage.
void MergeShip_Picto_ConvertRgba16ToIntensityImage(void* destI, u16* srcRgba16, s32 rgba16Width, s32 pixelLeft,
                                                   s32 pixelTop, s32 pixelRight, s32 pixelBottom, s32 bitDepth) {
    s32 i;
    s32 j;
    u32 pixel;
    u32 r;
    u32 g;
    u32 b;

    switch (bitDepth) {
        case 4: {
            u8* destI4 = destI;
            u32 upper;
            u32 lower;

            for (i = pixelTop; i <= pixelBottom; i++) {
                for (j = pixelLeft; j <= pixelRight; j += 2) {
                    pixel = srcRgba16[i * rgba16Width + j];
                    r = PICTO_RGBA16_GET_R(pixel);
                    g = PICTO_RGBA16_GET_G(pixel);
                    b = PICTO_RGBA16_GET_B(pixel);
                    upper = PLAY_INTENSITY_MIX(r, g, b, 15);

                    pixel = srcRgba16[i * rgba16Width + j + 1];
                    r = PICTO_RGBA16_GET_R(pixel);
                    g = PICTO_RGBA16_GET_G(pixel);
                    b = PICTO_RGBA16_GET_B(pixel);
                    lower = PLAY_INTENSITY_MIX(r, g, b, 15);

                    *(destI4++) = (upper << 4) | lower;
                }
            }
            break;
        }

        case 5: {
            u8* destI5 = destI;

            for (i = pixelTop; i <= pixelBottom; i++) {
                for (j = pixelLeft; j <= pixelRight; j++) {
                    pixel = srcRgba16[i * rgba16Width + j];
                    r = PICTO_RGBA16_GET_R(pixel);
                    g = PICTO_RGBA16_GET_G(pixel);
                    b = PICTO_RGBA16_GET_B(pixel);

                    *(destI5++) = PLAY_INTENSITY_MIX(r, g, b, 255) & 0xF8;
                }
            }
            break;
        }

        case 8: {
            u8* destI8 = destI;

            for (i = pixelTop; i <= pixelBottom; i++) {
                for (j = pixelLeft; j <= pixelRight; j++) {
                    pixel = srcRgba16[i * rgba16Width + j];
                    r = PICTO_RGBA16_GET_R(pixel);
                    g = PICTO_RGBA16_GET_G(pixel);
                    b = PICTO_RGBA16_GET_B(pixel);

                    *(destI8++) = PLAY_INTENSITY_MIX(r, g, b, 255);
                }
            }
            break;
        }

        case 16: {
            u16* destI16 = destI;

            for (i = pixelTop; i <= pixelBottom; i++) {
                for (j = pixelLeft; j <= pixelRight; j++) {
                    *(destI16++) = srcRgba16[i * rgba16Width + j];
                }
            }
            break;
        }
    }
}

#define PLAY_COMPRESS_BITS 5
#define PLAY_DECOMPRESS_BITS 8

// MM Play_CompressI8ToI5.
void MergeShip_Picto_CompressI8ToI5(void* srcI8, void* destI5, size_t size) {
    u32 i;
    u8* src = srcI8;
    s8* dest = destI5;
    s32 bitsLeft = PLAY_DECOMPRESS_BITS; // Bits left in the current dest pixel left to compress into
    u32 destPixel = 0;
    s32 shift;
    u32 srcPixel;

    for (i = 0; i < size; i++) {
        srcPixel = *src++;
        srcPixel = (srcPixel * 0x1F + 0x80) / 0xFF;
        shift = bitsLeft - PLAY_COMPRESS_BITS;
        if (shift > 0) {
            destPixel |= srcPixel << shift;
        } else {
            destPixel |= srcPixel >> -shift;
            *dest++ = destPixel;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel = srcPixel << shift;
        }
        bitsLeft = shift;
    }

    if (bitsLeft < PLAY_DECOMPRESS_BITS) {
        *dest = destPixel;
    }
}

// MM Play_DecompressI5ToI8.
void MergeShip_Picto_DecompressI5ToI8(void* srcI5, void* destI8, size_t size) {
    u32 i;
    u8* src = srcI5;
    s8* dest = destI8;
    s32 bitsLeft = PLAY_DECOMPRESS_BITS; // Bits left in the current src pixel left to decompress
    u32 destPixel;
    s32 shift;
    u32 srcPixel = *src++;

    for (i = 0; i < size; i++) {
        shift = bitsLeft - PLAY_COMPRESS_BITS;
        if (shift > 0) {
            destPixel = 0;
            destPixel |= srcPixel >> shift;
        } else {
            destPixel = 0;
            destPixel |= srcPixel << -shift;
            srcPixel = *src++;
            shift += PLAY_DECOMPRESS_BITS;
            destPixel |= srcPixel >> shift;
        }
        destPixel = (destPixel & 0x1F) * 0xFF / 0x1F;
        *dest++ = destPixel;
        bitsLeft = shift;
    }
}

// MM Play_UpdateMain's first step: `if (R_PICTO_PHOTO_STATE == PICTO_PHOTO_STATE_PROCESS)` -> Play_TakePictoPhoto.
// The frame read back by OnPlayDrawEnd is in sPictoFrameRgba16 once the frame's display list has run (SoH runs it at
// the end of the frame, before the next update).
static void MergeShip_Picto_ProcessPhoto(void) {
    if (sPictoPhotoState == PICTO_PHOTO_STATE_PROCESS) {
        sPictoPhotoState = PICTO_PHOTO_STATE_READY;
        // MM MsgEvent_SendNullTask() waits for the RSP; nothing to wait for here.
        // MM Play_TakePictoPhoto: PreRender_ApplyFilters (a no-op without a coverage buffer, as in 2S2H), then:
        MergeShip_Picto_ConvertRgba16ToIntensityImage(
            sPictoPhotoI8, sPictoFrameRgba16, SCREEN_WIDTH, MERGESHIP_PICTO_PHOTO_TOPLEFT_X,
            MERGESHIP_PICTO_PHOTO_TOPLEFT_Y, (MERGESHIP_PICTO_PHOTO_TOPLEFT_X + MERGESHIP_PICTO_PHOTO_WIDTH) - 1,
            (MERGESHIP_PICTO_PHOTO_TOPLEFT_Y + MERGESHIP_PICTO_PHOTO_HEIGHT) - 1, 8);
        sPictoPhotoState = PICTO_PHOTO_STATE_OFF;
    }
}

// ---- MM z_snap.c ------------------------------------------------------------------------------------------------------

// MM Snap_RecordPictographedActors. MM clears saveInfo.pictoFlags0/1, sets PICTO_VALID_IN_SWAMP in SCENE_20SICHITAI and
// runs the validation function of each loaded PictoActor it knows: in the swamp ACTOR_EN_MNK and ACTOR_EN_BIGOKUTA,
// anywhere ACTOR_EN_KAKASI (above ground), ACTOR_EN_ZOV, ACTOR_EN_BAL, ACTOR_EN_DNQ, ACTOR_EN_GE1/GE2/GE3 and
// ACTOR_EN_KAIZOKU. Those are Majora's Mask's actors; none of them is ever loaded in Hyrule (Ocarina of Time's En_Kakasi
// is its own scarecrow, not a PictoActor), and no Hyrule scene is the swamp. So every actor is looked at, none is
// "seen", the flags stay cleared and nothing is recorded -- which is what MM itself does for a photo of anything else.
s32 MergeShip_Picto_RecordPictographedActors(PlayState* play) {
    Actor* actor;
    s32 category;
    s32 seen;
    s32 validCount = 0;

    gMergeShipPictoFlags[0] = 0;
    gMergeShipPictoFlags[1] = 0;

    for (category = 0; category < ACTORCAT_MAX; category++) {
        for (actor = play->actorCtx.actorLists[category].head; actor != NULL; actor = actor->next) {
            // MM's scene and actor switches: no Hyrule scene or actor has a case.
            seen = 0;

            if (!seen) {
                continue;
            }

            validCount++;
        }
    }

    return validCount;
}

// ---- the player -------------------------------------------------------------------------------------------------------

// MM func_80831814(this, play, PLAYER_UNKAA5_2). MM's PLAYER_STATE1_4 is bit 2 (Ocarina of Time's
// PLAYER_STATE1_HOOKSHOT_FALLING), MM's 2000/4000 are HANGING_OFF_LEDGE/CLIMBING_LEDGE; MM depthInWater is
// Ocarina of Time's yDistToWater (func_8083B8F4, the C-up version of this test, reads it the same way).
static s32 MergeShip_Picto_func_80831814(PlayState* play, Player* this, s32 isSwimming) {
    if (!(this->stateFlags1 & (PLAYER_STATE1_HOOKSHOT_FALLING | PLAYER_STATE1_CARRYING_ACTOR |
                               PLAYER_STATE1_HANGING_OFF_LEDGE | PLAYER_STATE1_CLIMBING_LEDGE))) {
        if (Camera_CheckValidMode(Play_GetCamera(play, CAM_ID_MAIN), CAM_MODE_FIRST_PERSON) != 0) {
            if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
                (isSwimming && (this->actor.yDistToWater < this->ageProperties->unk_2C))) {
                this->unk_6AD = MERGESHIP_UNK6AD_PICTO_BOX;
                return true;
            }
        }
    }
    return false;
}

// MM Player_UseItem:
//   * `(itemAction == PLAYER_IA_PICTOGRAPH_BOX) && (this->talkActor != NULL) && (this->exchangeItemAction >
//     PLAYER_IA_NONE)`: the box is shown to the actor as a cutscene item (PLAYER_UNKAA5_5 = Ocarina of Time's 4). The
//     item cutscene for MM's items is MergeShip_MMItems_ActionHandler13's; it takes the box once
//     MergeShip_MMItems_IsCutsceneItemAction does (no Hyrule actor asks for it).
//   * otherwise `if (!func_80831814(this, play, PLAYER_UNKAA5_2)) Audio_PlaySfx(NA_SE_SY_ERROR)`.
void MergeShip_Picto_UseItem(PlayState* play, Player* player, s32 isSwimming) {
    if (!MergeShip_Picto_Enabled()) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return;
    }

    if ((player->talkActor != NULL) && (player->exchangeItemId > EXCH_ITEM_NONE) &&
        MergeShip_MMItems_IsCutsceneItemAction(PLAYER_IA_MM_PICTOGRAPH_BOX)) {
        player->itemAction = PLAYER_IA_MM_PICTOGRAPH_BOX;
        player->unk_6AD = 4;
        return;
    }

    if (!MergeShip_Picto_func_80831814(play, player, isSwimming)) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
    }
}

void MergeShip_Picto_OnEnterFirstPerson(Player* player) {
    if (player->unk_6AD == MERGESHIP_UNK6AD_PICTO_BOX) {
        sPictoBoxOn = true;
    }
}

void MergeShip_Picto_OnSetupAction(void) {
    sPictoBoxOn = false;
}

// MM Player_Action_43: `((this->unk_AA5 == PLAYER_UNKAA5_2) && !(play->actorCtx.flags & ACTORCTX_FLAG_PICTO_BOX_ON))`
// ends the view; nothing else does for PLAYER_UNKAA5_2.
s32 MergeShip_Picto_KeepsView(Player* player) {
    return (player->unk_6AD == MERGESHIP_UNK6AD_PICTO_BOX) && sPictoBoxOn;
}

s32 MergeShip_Picto_DoActionA(s32 doAction, s32 snapDoAction) {
    return sPictoBoxOn ? snapDoAction : doAction;
}

// ---- the interface ------------------------------------------------------------------------------------------------------

// MM Interface_SetBButtonInterfaceDoAction(play, DO_ACTION_STOP): MM's own "Stop" label from do_action_static, in the
// interface's B label (Ocarina of Time's unk_1FA / unk_1FC, which Interface_Draw already puts before the item).
static void MergeShip_Picto_SetBButtonInterfaceStop(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    const char* stopTex = MergeShip_GetMMDoActionTexture(MM_DO_ACTION_STOP);

    if (stopTex == NULL) {
        Interface_LoadActionLabelB(play, DO_ACTION_STOP);
        return;
    }
    interfaceCtx->unk_1FC = DO_ACTION_STOP;
    interfaceCtx->doActionSegment[1] = (char*)stopTex;
    interfaceCtx->unk_1FA = 1;
}

// MM Audio_PlaySfx_MessageDecide / Audio_PlaySfx_MessageCancel. The pass sound they stop is the one Ocarina of Time's
// Message_ShouldAdvance just played.
static void MergeShip_Picto_PlayMessageDecide(void) {
    MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_DECIDE);
    Audio_StopSfxById(NA_SE_SY_MESSAGE_PASS);
}

static void MergeShip_Picto_PlayMessageCancel(void) {
    MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_CANCEL);
    Audio_StopSfxById(NA_SE_SY_MESSAGE_PASS);
}

// MM Interface_UpdateButtonsPart1, the two pictograph branches (`sPictoState != PICTO_BOX_STATE_OFF` and
// VB_PICTO_ACTIVATE, i.e. the picto flag raised with the box put away) and its restoreHudVisibility tail.
// Called at the start of Ocarina of Time's Interface_Update. MM's MM-only branches before these (Honey & Darling,
// the swamp boat, the beaver and Goron races) have no Hyrule counterpart; riding Epona does, and comes first as in MM.
// MM HUD_VISIBILITY_A_B has no Ocarina of Time mode: HUD_VISIBILITY_A_B_MINIMAP, with the minimap dimmed in
// MergeShip_Picto_OnPlayDrawEnd as MM dims it.
void MergeShip_Picto_InterfaceUpdate(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    s32 restoreHudVisibility = false;

    MergeShip_Picto_ProcessPhoto();

    // MM Interface_Update: `if (!IS_PAUSED(&play->pauseCtx) && (play->gameOverCtx.state == GAMEOVER_INACTIVE))`.
    if ((play->pauseCtx.state != 0) || (play->pauseCtx.debugState != 0) ||
        (play->gameOverCtx.state != GAMEOVER_INACTIVE) || (player == NULL)) {
        return;
    }
    if (gSaveContext.cutsceneIndex >= 0xFFF0) {
        return;
    }
    if (player->stateFlags1 & PLAYER_STATE1_ON_HORSE) {
        return;
    }

    if (sPictoState != PICTO_BOX_STATE_OFF) {
        // Related to pictograph
        if (sPictoState == PICTO_BOX_STATE_LENS) {
            if (!sPictoBoxOn) {
                MergeShip_Picto_CompressI8ToI5(sPictoPhotoI8, gMergeShipPictoPhotoI5, MERGESHIP_PICTO_PHOTO_SIZE);
                interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;
                restoreHudVisibility = true;
                sPictoState = PICTO_BOX_STATE_OFF;
            } else if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_B)) {
                sPictoBoxOn = false;
                interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;
                restoreHudVisibility = true;
                sPictoState = PICTO_BOX_STATE_OFF;
            } else if (CHECK_BTN_ALL(play->state.input[0].press.button, BTN_A)) {
                // MM also takes the photo when the N64 Voice Recognition Unit hears "cheese"; there is no such unit
                // here. MM's EVENTINF_41 cutscene test is the swamp boat's.
                MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_CAMERA_SHUTTER);
                sPictoPhotoState = PICTO_PHOTO_STATE_SETUP;
                play->haltAllActors = true;
                sPictoState = PICTO_BOX_STATE_SETUP_PHOTO;
                sPictoPhotoBeingTaken = true;
            }
        }
        // The "Keep this picture?" answer: MergeShip_Picto_OnChoice, from Message_Update.
    } else if (sPictoBoxOn) {
        // Related to pictograph
        if (!gMergeShipPictoHasPhoto) {
            MergeShip_Picto_SetBButtonInterfaceStop(play);
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_A_B_MINIMAP);
            sPictoState = PICTO_BOX_STATE_LENS;
        } else {
            MergeShip_Picto_DecompressI5ToI8(gMergeShipPictoPhotoI5, sPictoPhotoI8, MERGESHIP_PICTO_PHOTO_SIZE);
            play->haltAllActors = true;
            sPictoState = PICTO_BOX_STATE_SETUP_PHOTO;
        }
    }

    if (restoreHudVisibility) {
        gSaveContext.hudVisibilityMode = HUD_VISIBILITY_NO_CHANGE;
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
        }
    } else if (sPictoState == PICTO_BOX_STATE_LENS) {
        // MM runs none of its other button logic while the box is out; Ocarina of Time's func_80083108 still runs after
        // this and restores the whole HUD when a button's status changes. Keep MM's view (a no-op when unchanged).
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_A_B_MINIMAP);
    }
}

// MM Interface_UpdateButtonsPart1's `(sPictoState >= PICTO_BOX_STATE_SETUP_PHOTO) && (Message_GetState ==
// TEXT_STATE_CHOICE) && Message_ShouldAdvance` branch. MM runs it before Message_Update; Ocarina of Time's
// Message_Update closes any two-choice box on A unless an actor answered it earlier in the frame, and the interface
// update comes after it, so the answer is taken here, from Message_Update's MSGMODE_TEXT_DONE.
s32 MergeShip_Picto_OnChoice(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;

    if ((sPictoState < PICTO_BOX_STATE_SETUP_PHOTO) || (play->msgCtx.textId != MERGESHIP_TEXT_MM_PICTO_KEEP)) {
        return false;
    }
    if (!Message_ShouldAdvance(play)) {
        return true;
    }
    play->haltAllActors = false;
    // (MM player->stateFlags1 &= ~PLAYER_STATE1_200: see MergeShip_Picto_Draw.)
    Message_CloseTextbox(play);
    if (play->msgCtx.choiceIndex != 0) {
        MergeShip_Picto_PlayMessageCancel();
        MergeShip_Picto_SetBButtonInterfaceStop(play);
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_A_B_MINIMAP);
        sPictoState = PICTO_BOX_STATE_LENS;
        gMergeShipPictoHasPhoto = false;
        MergeShipTest_Log("PICTO keep: no -> lens");
    } else {
        MergeShip_Picto_PlayMessageDecide();
        interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;
        sPictoState = PICTO_BOX_STATE_OFF;
        if (sPictoPhotoBeingTaken) {
            MergeShip_Picto_CompressI8ToI5(sPictoPhotoI8, gMergeShipPictoPhotoI5, MERGESHIP_PICTO_PHOTO_SIZE);
            MergeShip_Picto_RecordPictographedActors(play);
        }
        sPictoBoxOn = false;
        gMergeShipPictoHasPhoto = true;
        sPictoPhotoBeingTaken = false;
        // The interface's restoreHudVisibility tail.
        gSaveContext.hudVisibilityMode = HUD_VISIBILITY_NO_CHANGE;
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
        }
        MergeShipTest_Log("PICTO keep: yes -> photo kept");
    }
    return true;
}

// Play_Draw after the world and before the HUD (GameInteractor OnPlayDrawEnd):
//   * MM Play_Draw's `R_PICTO_PHOTO_STATE == PICTO_PHOTO_STATE_SETUP` branch as 2S2H ports it:
//     FB_WriteFramebufferSliceToCPU(&gfx, fbufSave, false) -- the 4:3 middle of the frame at 320x240, RGBA16.
//   * MM HUD_VISIBILITY_A_B dims the minimap (Interface_UpdateHudAlphas, 32 a frame); Ocarina of Time's
//     HUD_VISIBILITY_A_B_MINIMAP raises it, so it is dimmed here, after the alphas were updated and before they are drawn.
void MergeShip_Picto_OnPlayDrawEnd(PlayState* play) {
    if (sPictoPhotoState == PICTO_PHOTO_STATE_SETUP) {
        Gfx* gfx;

        OPEN_DISPS(play->state.gfxCtx);

        sPictoPhotoState = PICTO_PHOTO_STATE_PROCESS;
        gfx = OVERLAY_DISP;
        FB_WriteFramebufferSliceToCPU(&gfx, sPictoFrameRgba16, false);
        OVERLAY_DISP = gfx;

        CLOSE_DISPS(play->state.gfxCtx);
    }

    if (sPictoState == PICTO_BOX_STATE_LENS) {
        sPictoMinimapMaxAlpha -= 32;
        if (sPictoMinimapMaxAlpha < 0) {
            sPictoMinimapMaxAlpha = 0;
        }
        if (play->interfaceCtx.minimapAlpha > sPictoMinimapMaxAlpha) {
            play->interfaceCtx.minimapAlpha = sPictoMinimapMaxAlpha;
        }
    } else {
        sPictoMinimapMaxAlpha = 255;
    }
}

// MM Interface_Draw: "Draw pictograph focus icons" and "Draw pictograph photo".
// MM sets PLAYER_STATE1_200 while the photo is shown, which its z_actor.c freezes some actors on and its clock and
// En_Holl read; play->haltAllActors (set with it) already stops every actor, and Ocarina of Time's bit 9 is
// PLAYER_STATE1_READY_TO_FIRE, so it is not set here.
void MergeShip_Picto_Draw(PlayState* play) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s16 sp2CC;

    if (sPictoState == PICTO_BOX_STATE_OFF) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    // Draw pictograph focus icons
    if (sPictoState == PICTO_BOX_STATE_LENS) {
        Gfx_SetupDL_39Overlay(play->state.gfxCtx);

        gDPSetAlphaCompare(OVERLAY_DISP++, G_AC_THRESHOLD);
        gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 155, 255);
        gDPLoadTextureBlock_4b(OVERLAY_DISP++, mm_gPictoBoxFocusBorderTex, G_IM_FMT_IA, 16, 16, 0,
                               G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);

        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_BORDER_TOPLEFT_X << 2, PICTO_FOCUS_BORDER_TOPLEFT_Y << 2,
                            (PICTO_FOCUS_BORDER_TOPLEFT_X << 2) + (16 << 2),
                            (PICTO_FOCUS_BORDER_TOPLEFT_Y << 2) + (16 << 2), G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_BORDER_TOPRIGHT_X << 2, PICTO_FOCUS_BORDER_TOPRIGHT_Y << 2,
                            (PICTO_FOCUS_BORDER_TOPRIGHT_X << 2) + (16 << 2),
                            (PICTO_FOCUS_BORDER_TOPRIGHT_Y << 2) + (16 << 2), G_TX_RENDERTILE, 512, 0, 1 << 10,
                            1 << 10);
        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_BORDER_BOTTOMLEFT_X << 2,
                            PICTO_FOCUS_BORDER_BOTTOMLEFT_Y << 2, (PICTO_FOCUS_BORDER_BOTTOMLEFT_X << 2) + (16 << 2),
                            (PICTO_FOCUS_BORDER_BOTTOMLEFT_Y << 2) + (16 << 2), G_TX_RENDERTILE, 0, 512, 1 << 10,
                            1 << 10);
        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_BORDER_BOTTOMRIGHT_X << 2,
                            PICTO_FOCUS_BORDER_BOTTOMRIGHT_Y << 2, (PICTO_FOCUS_BORDER_BOTTOMRIGHT_X << 2) + (16 << 2),
                            (PICTO_FOCUS_BORDER_BOTTOMRIGHT_Y << 2) + (16 << 2), G_TX_RENDERTILE, 512, 512, 1 << 10,
                            1 << 10);

        gDPLoadTextureBlock_4b(OVERLAY_DISP++, mm_gPictoBoxFocusIconTex, G_IM_FMT_I, 32, 16, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);

        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_ICON_X << 2, PICTO_FOCUS_ICON_Y << 2,
                            (PICTO_FOCUS_ICON_X << 2) + (32 << 2), (PICTO_FOCUS_ICON_Y << 2) + (16 << 2),
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);

        gDPLoadTextureBlock_4b(OVERLAY_DISP++, mm_gPictoBoxFocusTextTex, G_IM_FMT_I, 32, 8, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);

        gSPTextureRectangle(OVERLAY_DISP++, PICTO_FOCUS_TEXT_X << 2, PICTO_FOCUS_TEXT_Y << 2,
                            (PICTO_FOCUS_TEXT_X << 2) + (32 << 2), (PICTO_FOCUS_TEXT_Y << 2) + (8 << 2),
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }

    // Draw pictograph photo. MM skips Interface_Draw in the frame the photo is captured (Play_Draw's goto past the
    // post-world draw) and converts it at the start of the next update; Ocarina of Time's HUD is drawn in that frame,
    // so the photo waits until it has been converted.
    if ((sPictoState >= PICTO_BOX_STATE_SETUP_PHOTO) && (sPictoPhotoState == PICTO_PHOTO_STATE_OFF)) {
        if (!sPictoBoxOn) {
            MergeShip_Picto_CompressI8ToI5(sPictoPhotoI8, gMergeShipPictoPhotoI5, MERGESHIP_PICTO_PHOTO_SIZE);

            interfaceCtx->unk_1FA = interfaceCtx->unk_1FC = 0;

            sPictoState = PICTO_BOX_STATE_OFF;
            gSaveContext.hudVisibilityMode = HUD_VISIBILITY_NO_CHANGE;
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
        } else {
            s16 pictoRectTop;
            s16 pictoRectLeft;

            if (sPictoState == PICTO_BOX_STATE_SETUP_PHOTO) {
                sPictoState = PICTO_BOX_STATE_PHOTO;
                Message_StartTextbox(play, MERGESHIP_TEXT_MM_PICTO_KEEP, NULL);
                Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
            }

            gDPPipeSync(OVERLAY_DISP++);
            gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 200, 200, 250);
            gDPFillRectangle(OVERLAY_DISP++, 70, 22, 251, 151);

            Gfx_SetupDL_39Overlay(play->state.gfxCtx);

            gDPSetRenderMode(OVERLAY_DISP++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 250, 160, 160, 255);

            // Picture is offset up by 33 pixels to give room for the message box at the bottom
            pictoRectTop = MERGESHIP_PICTO_PHOTO_TOPLEFT_Y - 33;
            for (sp2CC = 0; sp2CC < (MERGESHIP_PICTO_PHOTO_HEIGHT / 8); sp2CC++, pictoRectTop += 8) {
                pictoRectLeft = MERGESHIP_PICTO_PHOTO_TOPLEFT_X;

                // 2S2H [Port] Invalidate each section: the buffer's contents change between photos.
                gSPInvalidateTexCache(OVERLAY_DISP++, (uintptr_t)(sPictoPhotoI8 + (0x500 * sp2CC)));
                gDPLoadTextureBlock(OVERLAY_DISP++, sPictoPhotoI8 + (0x500 * sp2CC), G_IM_FMT_I, G_IM_SIZ_8b,
                                    MERGESHIP_PICTO_PHOTO_WIDTH, 8, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                gSPTextureRectangle(OVERLAY_DISP++, pictoRectLeft << 2, pictoRectTop << 2,
                                    (pictoRectLeft + MERGESHIP_PICTO_PHOTO_WIDTH) << 2, (pictoRectTop << 2) + (8 << 2),
                                    G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
            }
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}
