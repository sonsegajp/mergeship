// MergeShip2 -- Majora's Mask's Giant's Mask, included at the end of z_player.c (so it can use the player's static
// helpers, as MM's code lives in MM's z_player.c).
//
// Ported from 2ship2harkinian/mm; each function names its MM original:
//   overlays/actors/ovl_player_actor/z_player.c: Player_UseItem's Giant's Mask checks, Player_ActionHandler_13's
//     mask branch, func_808389BC, func_80838A20, Player_Action_89, Player_Action_90, Player_ProcessItemButtons (no
//     magic left), func_80839860 (the hop), func_808339D4 (damage), func_80834600 (knockback),
//     Player_UpdateZTargeting (no lock-on), Player_AnimSfx_PlayVoice, Player_AnimSfx_PlayFloorWalk, Player_Init;
//   code/z_player_lib.c: func_80123140 with PLAYER_BOOTS_GIANT (D_801BFE14), func_80128640 (the mask in the hand);
//   code/z_parameter.c: Magic_Consume(MAGIC_CONSUME_GIANTS_MASK), Magic_Update(MAGIC_STATE_CONSUME_GIANTS_MASK);
//   code/z_actor.c: Player_PlaySfx; audio/code_8019AF00.c: Audio_PlaySfx_GiantsMask;
//   overlays/actors/ovl_Boss_02/z_boss_02.c: Boss02_HandleGiantsMaskCutscene (the cutscene that makes Link a giant).
//
// What cannot be MM's own, and why:
//   MM lets the mask be worn only in Twinmold's lair, and there Boss_02 shrinks the arena and everything in it to a
//   tenth (sGiantModeScaleFactor 0.1) while Link keeps his scale. Hyrule has no Twinmold. By the user's decision the
//   mask is allowed in Hyrule's boss lairs (CVar MergeShip.GiantsMaskInLairs, MergeShip_GiantsMaskAllowedHere) and
//   Link grows instead: to MM's ten times his size, or to the largest size that fits under the ceiling where he puts
//   it on. Twinmold's cutscene (its camera, sounds, flash and timings) runs here, driven by the player rather than
//   Boss_02. Because the world does not shrink, the grown Link is made to behave like MM's giant relative to the
//   world: he moves by MM's giant physics in his own body units, carried out `ratio` times as far, and his collision
//   (cylinder, wall/ceiling/ledge/water heights, legs), fall distance and camera height grow with him.

#include "mm/objects/object_mask_kyojin/object_mask_kyojin.h"

#define MS_GIANT_MODE_SCALE_FACTOR 0.1f                   // MM z_boss_02.c sGiantModeScaleFactor in giant mode
#define MS_GIANT_MAX_RATIO (1.0f / MS_GIANT_MODE_SCALE_FACTOR) // MM's giant is ten times the arena's scale
#define MS_GIANT_MAGIC_TIMER 20        // MM R_MAGIC_CONSUME_TIMER_GIANTS_MASK, which Boss02_Init sets to KREG(14) + 20
#define MS_GIANT_MM_HUMAN_HEIGHT 44.0f // MM Player_GetHeight for the human, the size MM's cutscene camera is set for
#define MS_GIANT_HEADROOM 1.1f         // Link's hair and the mask stand a little above Player_GetHeight

// MM GiantsMaskCsState (z_boss_02.c).
typedef enum {
    /*  0 */ MS_GIANTS_CS_STATE_WAITING_FOR_MASK,
    /*  1 */ MS_GIANTS_CS_STATE_MASK_ON,
    /*  2 */ MS_GIANTS_CS_STATE_MASK_ON_SKIPPED,
    /* 10 */ MS_GIANTS_CS_STATE_MASK_OFF = 10,
    /* 11 */ MS_GIANTS_CS_STATE_MASK_OFF_SKIPPED,
    /* 20 */ MS_GIANTS_CS_STATE_DONE = 20
} MergeShipGiantsMaskCsState;

// MM GiantsMaskCsFlashState (z_boss_02.c).
typedef enum {
    /* 0 */ MS_GIANTS_CS_FLASH_STATE_NOT_STARTED,
    /* 1 */ MS_GIANTS_CS_FLASH_STATE_STARTED,
    /* 2 */ MS_GIANTS_CS_FLASH_STATE_INCREASE_ALPHA,
    /* 3 */ MS_GIANTS_CS_FLASH_STATE_DECREASE_ALPHA
} MergeShipGiantsMaskCsFlashState;

static struct {
    u8 csFlag;           // MM PLAYER_STATE1_100: set by the player's action, cleared when the cutscene is done
    u8 csState;          // MM Boss02::giantsMaskCsState
    u8 flashState;       // MM Boss02::giantsMaskCsFlashState
    u8 isGiant;          // MM sIsInGiantMode
    u8 giantBoots;       // MM Player::currentBoots == PLAYER_BOOTS_GIANT
    u8 draining;         // MM gSaveContext.magicState == MAGIC_STATE_CONSUME_GIANTS_MASK
    u8 canSkipMaskOnCs;  // MM sCanSkipMaskOnCs
    u8 canSkipMaskOffCs; // MM sCanSkipMaskOffCs
    s8 room;             // the room Link grew in
    s16 subCamId;        // MM Boss02::subCamId (CAM_ID_MAIN is MM's SUB_CAM_ID_DONE)
    s16 flashAlpha;      // MM Boss02::giantsMaskCsFlashAlpha
    s16 magicTimer;      // MM interfaceCtx->magicConsumptionTimer
    s16 savedAdjFogNear;
    u32 csTimer; // MM Boss02::giantsMaskCsTimer
    f32 subCamEyeOffsetY;
    f32 subCamDistZFromPlayer;
    f32 subCamAtOffsetY;
    f32 subCamAtVel;
    f32 subCamUpRotZScale;
    f32 playerScale; // MM Boss02::playerScale: 0.01 at Link's size .. 0.1 as MM's giant
    f32 fogNear;     // MM Boss02::fogNear
    f32 targetRatio; // how many times his size Link grows here (MM: 1 / sGiantModeScaleFactor)
    f32 camScale;    // Link's height relative to MM's human, for the cutscene camera
    f32 baseScale;   // Link's own actor scale
    s16 baseRadius;  // Link's own cylinder radius
} sGiantsMask;

// Link's current size relative to his own (1 while he is not grown).
static f32 sGiantsMaskRatio = 1.0f;

// The grown Link's PlayerAgeProperties: his form's row with every world-unit height and radius scaled.
static PlayerAgeProperties sGiantsMaskAgeProperties;

void MergeShip_GiantsMask_Action_89(Player* this, PlayState* play);
void MergeShip_GiantsMask_Action_90(Player* this, PlayState* play);

s32 MergeShip_GiantsMask_IsWornBy(Player* player) {
    return (player != NULL) && (player->actor.id == ACTOR_PLAYER) && MERGESHIP_WEARS_MM_MASK(player, MM_MASK_GIANT);
}

// MM `player->currentMask == PLAYER_MASK_GIANT` (En_Elf, En_Bom, the interface, the camera).
s32 MergeShip_GiantsMask_IsWorn(PlayState* play) {
    return (play != NULL) && MergeShip_GiantsMask_IsWornBy(GET_PLAYER(play));
}

f32 MergeShip_GiantsMask_SizeRatio(Player* player) {
    return ((player != NULL) && (player->actor.id == ACTOR_PLAYER)) ? sGiantsMaskRatio : 1.0f;
}

// Link's scale, collision cylinder and age properties at `ratio` times his own size.
static void MergeShip_GiantsMask_ApplyRatio(Player* this, f32 ratio) {
    PlayerAgeProperties* base = &sAgeProperties[this->mergeShipForm];
    PlayerAgeProperties* props = &sGiantsMaskAgeProperties;

    if (sGiantsMaskRatio == 1.0f) {
        if (ratio == 1.0f) {
            return;
        }
        sGiantsMask.baseScale = this->actor.scale.x;
        sGiantsMask.baseRadius = this->cylinder.dim.radius;
    }

    sGiantsMaskRatio = ratio;
    Actor_SetScale(&this->actor, sGiantsMask.baseScale * ratio);
    this->cylinder.dim.radius = (s16)(sGiantsMask.baseRadius * ratio);

    if (ratio == 1.0f) {
        this->ageProperties = base;
        return;
    }

    *props = *base;
    props->ceilingCheckHeight *= ratio;
    props->unk_0C *= ratio;
    props->unk_10 *= ratio;
    props->unk_14 *= ratio;
    props->unk_18 *= ratio;
    props->unk_1C *= ratio;
    props->unk_20 *= ratio;
    props->unk_24 *= ratio;
    props->unk_28 *= ratio;
    props->unk_2C *= ratio;
    props->unk_30 *= ratio;
    props->unk_34 *= ratio;
    props->wallCheckRadius *= ratio;
    props->unk_3C *= ratio;
    props->unk_40 *= ratio;
    props->unk_98 *= ratio;
    props->unk_9C *= ratio;
    this->ageProperties = props;
}

// How many times his own size Link can grow where he stands: MM's ten, or less under a lower ceiling.
static f32 MergeShip_GiantsMask_FitRatio(PlayState* play, Player* this) {
    f32 height = (Player_GetHeight(this) / sGiantsMaskRatio) * MS_GIANT_HEADROOM;
    f32 ratio = MS_GIANT_MAX_RATIO;
    Vec3f from = this->actor.world.pos;
    Vec3f to;
    Vec3f hit;
    CollisionPoly* poly;
    s32 bgId;

    from.y += 1.0f;
    to = from;
    to.y = this->actor.world.pos.y + (height * MS_GIANT_MAX_RATIO);

    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &poly, false, false, true, true, &bgId)) {
        ratio = (hit.y - this->actor.world.pos.y) / height;
    }
    return CLAMP(ratio, 1.0f, MS_GIANT_MAX_RATIO);
}

// ---- MM Magic_Consume(MAGIC_CONSUME_GIANTS_MASK) / Magic_Update(MAGIC_STATE_CONSUME_GIANTS_MASK) ------------------
// Ocarina of Time's magic meter has no Giant's Mask state, so the drain MM's Magic_Update runs for it is run here, as
// the Goron's and Zora's drains are: the meter sits in MAGIC_STATE_METER_FLASH_3 (Ocarina of Time only flashes its
// border there, which is what MM's drain state looks like, and it keeps every other magic user out as MM's non-idle
// state does). Magic_Reset ends it, as in MM.

static s32 MergeShip_GiantsMask_MagicConsume(PlayState* play) {
    if (!gSaveContext.isMagicAcquired) {
        return false;
    }
    // (MM: consuming 0 never fails for lack of magic.)
    if (gSaveContext.magicState == MAGIC_STATE_IDLE) {
        if (gSaveContext.magic != 0) {
            sGiantsMask.magicTimer = MS_GIANT_MAGIC_TIMER;
            gSaveContext.magicState = MAGIC_STATE_METER_FLASH_3; // MM MAGIC_STATE_CONSUME_GIANTS_MASK
            sGiantsMask.draining = true;
            return true;
        }
        return false;
    }
    return sGiantsMask.draining && (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_3);
}

static void MergeShip_GiantsMask_UpdateMagic(PlayState* play) {
    if (!sGiantsMask.draining) {
        return;
    }
    if (gSaveContext.magicState != MAGIC_STATE_METER_FLASH_3) {
        sGiantsMask.draining = false; // Magic_Reset (or anything else) ended it
        return;
    }

    if ((play->pauseCtx.state == 0) && (play->pauseCtx.debugState == 0) && (play->msgCtx.msgMode == MSGMODE_NONE) &&
        (play->gameOverCtx.state == GAMEOVER_INACTIVE) && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->transitionMode == TRANS_MODE_OFF)) {
        if (!Play_InCsMode(play)) {
            sGiantsMask.magicTimer--;
            if (sGiantsMask.magicTimer == 0) {
                // MM: `if (!CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI))` -- Chateau Romani's effect.
                if (!MergeShip_Chateau_IsActive()) {
                    gSaveContext.magic--;
                }
                if (gSaveContext.magic <= 0) {
                    gSaveContext.magic = 0;
                }
                sGiantsMask.magicTimer = MS_GIANT_MAGIC_TIMER;
            }
        }
    }
}

// ---- MM Boss02_HandleGiantsMaskCutscene ------------------------------------------------------------------------
// MM's cutscene grows Link's scale from 0.01 to 0.1 and then shrinks the arena. Here the scale it computes (MM's
// playerScale) is mapped onto Link's own growth: 0.01 is his own size and 0.1 is targetRatio times it. The camera's
// offsets are MM's, for MM's human (44 high) growing ten times; they are scaled to Link's height (camScale) and to
// the share of MM's growth he makes (growth). Timings, rates, sounds and the flash are MM's.

// MM Boss02_FillScreen / Boss02_SetFillScreenAlpha / Boss02_StopFillScreen.
static void MergeShip_GiantsMask_FillScreen(u8 red, u8 green, u8 blue, u8 alpha) {
    R_PLAY_FILL_SCREEN_ON = true;
    R_PLAY_FILL_SCREEN_R = red;
    R_PLAY_FILL_SCREEN_G = green;
    R_PLAY_FILL_SCREEN_B = blue;
    R_PLAY_FILL_SCREEN_ALPHA = alpha;
}

// The end of the cutscene's camera and screen effects (MM's `done:` block, and a forced stop).
static void MergeShip_GiantsMask_StopCutsceneCamera(PlayState* play) {
    if (sGiantsMask.subCamId != CAM_ID_MAIN) {
        func_800C08AC(play, sGiantsMask.subCamId, 0); // MM func_80169AFC
        sGiantsMask.subCamId = CAM_ID_MAIN;           // MM SUB_CAM_ID_DONE
        Letterbox_SetSizeTarget(0);
    }
    if (sGiantsMask.csState != MS_GIANTS_CS_STATE_WAITING_FOR_MASK) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL); // MM Cutscene_StopManual
        play->envCtx.adjFogNear = sGiantsMask.savedAdjFogNear;
        MergeShip_MotionBlur_Disable(); // MM Play_DisableMotionBlur
    }
}

static void MergeShip_GiantsMask_HandleCutscene(PlayState* play, Player* player) {
    Vec3f subCamEyeOffset;
    Vec3f subCamEye;
    Vec3f subCamAt;
    Vec3f subCamUp;
    Vec3f vec;
    u8 switchArena = false;
    u8 done = false;
    f32 k = sGiantsMask.camScale;
    f32 growth = (sGiantsMask.targetRatio - 1.0f) / (MS_GIANT_MAX_RATIO - 1.0f);
    s16 alpha;

    sGiantsMask.csTimer++;

    switch (sGiantsMask.csState) {
        case MS_GIANTS_CS_STATE_WAITING_FOR_MASK:
            if (sGiantsMask.csFlag) {
                // MM Cutscene_StartManual. Ocarina of Time's player would take a running csCtx as a cutscene for
                // itself (csAction 0x31) and leave the Giant's Mask action, so only its effect on the HUD is kept.
                Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
                sGiantsMask.subCamId = Play_CreateSubCamera(play);
                if (sGiantsMask.subCamId == SUBCAM_NONE) {
                    // (No camera free: the cutscene runs on the main camera, as if MM's sub-camera were done.)
                    sGiantsMask.subCamId = CAM_ID_MAIN;
                } else {
                    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
                    Play_ChangeCameraStatus(play, sGiantsMask.subCamId, CAM_STAT_ACTIVE);
                }
                MergeShip_MotionBlur_Enable(150); // MM Play_EnableMotionBlur
                sGiantsMask.csTimer = 0;
                sGiantsMask.subCamAtVel = 0.0f;
                sGiantsMask.subCamUpRotZScale = 0.0f;
                sGiantsMask.savedAdjFogNear = play->envCtx.adjFogNear;
                sGiantsMask.camScale = k = (Player_GetHeight(player) / sGiantsMaskRatio) / MS_GIANT_MM_HUMAN_HEIGHT;
                if (!sGiantsMask.isGiant) {
                    sGiantsMask.csState = MS_GIANTS_CS_STATE_MASK_ON;
                    sGiantsMask.targetRatio = MergeShip_GiantsMask_FitRatio(play, player);
                    growth = (sGiantsMask.targetRatio - 1.0f) / (MS_GIANT_MAX_RATIO - 1.0f);
                    sGiantsMask.subCamEyeOffsetY = 10.0f * k;
                    sGiantsMask.subCamDistZFromPlayer = 60.0f * k;
                    sGiantsMask.subCamAtOffsetY = 23.0f * k;
                    sGiantsMask.playerScale = 0.01f;
                    sGiantsMask.fogNear = 0.0f;
                    goto maskOn; // MM: so the cutscene cannot be skipped on its first frame
                } else {
                    sGiantsMask.csState = MS_GIANTS_CS_STATE_MASK_OFF;
                    sGiantsMask.subCamEyeOffsetY = 10.0f * k;
                    sGiantsMask.subCamDistZFromPlayer = (60.0f + (140.0f * growth)) * k;
                    sGiantsMask.subCamAtOffsetY = (23.0f + (250.0f * growth)) * k;
                    sGiantsMask.playerScale = 0.1f;
                    sGiantsMask.fogNear = -100.0f;
                    switchArena = true;
                    goto maskOff; // MM: so the cutscene cannot be skipped on its first frame
                }
            }
            break;

        case MS_GIANTS_CS_STATE_MASK_ON:
            if ((sGiantsMask.csTimer < 80) && sGiantsMask.canSkipMaskOnCs &&
                CHECK_BTN_ANY(play->state.input[0].press.button, BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT |
                                                                      BTN_CRIGHT | BTN_DUP | BTN_DDOWN | BTN_DLEFT |
                                                                      BTN_DRIGHT)) {
                sGiantsMask.csState++;
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_STARTED;
                sGiantsMask.csTimer = 0;
                break;
            }

        maskOn:
            if (sGiantsMask.csTimer >= 50) {
                if (sGiantsMask.csTimer == 60) { // MM BREG(43) + 60
                    MergeShip_PlayMMSfxCentered(MM_NA_SE_PL_TRANSFORM_GIANT);
                }

                // MM: the camera quickly moves away from the player and looks up as the player grows; fogNear goes
                // from 0 to -100.
                Math_ApproachF(&sGiantsMask.subCamDistZFromPlayer, (60.0f + (140.0f * growth)) * k, 0.1f,
                               sGiantsMask.subCamAtVel * 640.0f * k * growth);
                Math_ApproachF(&sGiantsMask.subCamAtOffsetY, (23.0f + (250.0f * growth)) * k, 0.1f,
                               sGiantsMask.subCamAtVel * 150.0f * k * growth);
                Math_ApproachF(&sGiantsMask.playerScale, 0.1f, 0.2f, sGiantsMask.subCamAtVel * 0.1f);
                Math_ApproachF(&sGiantsMask.fogNear, -100.0f, 1.0f, sGiantsMask.subCamAtVel * 100.0f);
                Math_ApproachF(&sGiantsMask.subCamAtVel, 1.0f, 1.0f, 0.001f);
            } else {
                // MM: the camera slowly approaches the player.
                Math_ApproachF(&sGiantsMask.subCamDistZFromPlayer, 30.0f * k, 0.1f, 1.0f * k);
            }

            // MM: the camera rolls side to side at the start, and stops rolling after 51 frames.
            if (sGiantsMask.csTimer > 50) {
                Math_ApproachZeroF(&sGiantsMask.subCamUpRotZScale, 1.0f, 0.06f);
            } else {
                Math_ApproachF(&sGiantsMask.subCamUpRotZScale, 0.4f, 1.0f, 0.02f);
            }

            if (sGiantsMask.csTimer == 107) {
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_STARTED;
            }

            if (sGiantsMask.csTimer > 120) {
                switchArena = true;
                sGiantsMask.canSkipMaskOnCs = true;
                goto done;
            }
            break;

        case MS_GIANTS_CS_STATE_MASK_ON_SKIPPED:
            if (sGiantsMask.csTimer >= 8) {
                switchArena = true;
                goto done;
            }
            break;

        case MS_GIANTS_CS_STATE_MASK_OFF:
            if ((sGiantsMask.csTimer < 30) && sGiantsMask.canSkipMaskOffCs &&
                CHECK_BTN_ANY(play->state.input[0].press.button, BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT |
                                                                      BTN_CRIGHT | BTN_DUP | BTN_DDOWN | BTN_DLEFT |
                                                                      BTN_DRIGHT)) {
                sGiantsMask.csState++;
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_STARTED;
                sGiantsMask.csTimer = 0;
                break;
            }

        maskOff:
            if (sGiantsMask.csTimer != 0) {
                if (sGiantsMask.csTimer == 10) { // MM BREG(44) + 10
                    MergeShip_PlayMMSfxCentered(MM_NA_SE_PL_TRANSFORM_NORAML);
                }

                // MM: the camera moves slowly towards the player and looks down as the player shrinks; fogNear
                // goes from -100 back to 0.
                Math_ApproachF(&sGiantsMask.subCamDistZFromPlayer, 60.0f * k, 0.1f,
                               sGiantsMask.subCamAtVel * 640.0f * k * growth);
                Math_ApproachF(&sGiantsMask.subCamAtOffsetY, 23.0f * k, 0.1f,
                               sGiantsMask.subCamAtVel * 150.0f * k * growth);
                Math_ApproachF(&sGiantsMask.playerScale, 0.01f, 0.1f, 0.003f);
                Math_ApproachF(&sGiantsMask.fogNear, 0.0f, 1.0f, sGiantsMask.subCamAtVel * 100.0f);
                Math_ApproachF(&sGiantsMask.subCamAtVel, 2.0f, 1.0f, 0.01f);
            }

            if (sGiantsMask.csTimer == 42) {
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_STARTED;
            }

            if (sGiantsMask.csTimer > 50) {
                sGiantsMask.canSkipMaskOffCs = true;
                goto done;
            }
            break;

        case MS_GIANTS_CS_STATE_MASK_OFF_SKIPPED:
            if (sGiantsMask.csTimer >= 8) {
                goto done;
            }
            break;

        done:
        case MS_GIANTS_CS_STATE_DONE:
            // MM: ends the cutscene and gives the player back the scale 0.01 -- MM's giant keeps Link's own scale,
            // the arena having shrunk. Here the arena stays and the grown Link keeps his size (below).
            MergeShip_GiantsMask_StopCutsceneCamera(play);
            sGiantsMask.csState = MS_GIANTS_CS_STATE_WAITING_FOR_MASK;
            sGiantsMask.csFlag = false; // MM player->stateFlags1 &= ~PLAYER_STATE1_100
            sGiantsMask.playerScale = 0.01f;
            done = true;
            break;
    }

    // MM: switches between the "normal" and the "giant" arena at the right point of the cutscene. The arena is not
    // swapped or scaled here; Link is.
    if (switchArena) {
        sGiantsMask.isGiant = true - sGiantsMask.isGiant;
    }

    // MM Actor_SetScale(&player->actor, this->playerScale), on Link's own growth.
    if (done) {
        MergeShip_GiantsMask_ApplyRatio(player, sGiantsMask.isGiant ? sGiantsMask.targetRatio : 1.0f);
    } else if (sGiantsMask.csState != MS_GIANTS_CS_STATE_WAITING_FOR_MASK) {
        MergeShip_GiantsMask_ApplyRatio(player, 1.0f + ((sGiantsMask.playerScale / 0.01f) - 1.0f) * growth);
        // MM play->envCtx.adjLightSettings.fogNear = this->fogNear
        play->envCtx.adjFogNear = sGiantsMask.savedAdjFogNear + (s16)sGiantsMask.fogNear;
    }

    switch (sGiantsMask.flashState) {
        case MS_GIANTS_CS_FLASH_STATE_NOT_STARTED:
            break;

        case MS_GIANTS_CS_FLASH_STATE_STARTED:
            sGiantsMask.flashAlpha = 0;
            MergeShip_GiantsMask_FillScreen(255, 255, 255, 0);
            sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_INCREASE_ALPHA;
            MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_TRANSFORM_MASK_FLASH);
            // fallthrough
        case MS_GIANTS_CS_FLASH_STATE_INCREASE_ALPHA:
            sGiantsMask.flashAlpha += 40;
            if (sGiantsMask.flashAlpha >= 400) {
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_DECREASE_ALPHA;
            }

            alpha = sGiantsMask.flashAlpha;
            alpha = CLAMP_MAX(alpha, 255);
            R_PLAY_FILL_SCREEN_ALPHA = alpha; // MM Boss02_SetFillScreenAlpha
            break;

        case MS_GIANTS_CS_FLASH_STATE_DECREASE_ALPHA:
            sGiantsMask.flashAlpha -= 40;
            if (sGiantsMask.flashAlpha <= 0) {
                sGiantsMask.flashAlpha = 0;
                sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_NOT_STARTED;
                R_PLAY_FILL_SCREEN_ON = false; // MM Boss02_StopFillScreen
            } else {
                alpha = sGiantsMask.flashAlpha;
                alpha = CLAMP_MAX(alpha, 255);
                R_PLAY_FILL_SCREEN_ALPHA = alpha;
            }
            break;
    }

    if ((sGiantsMask.csState != MS_GIANTS_CS_STATE_WAITING_FOR_MASK) && (sGiantsMask.subCamId != CAM_ID_MAIN)) {
        Matrix_Push();
        Matrix_RotateY(BINANG_TO_RAD(player->actor.shape.rot.y), MTXMODE_NEW); // MM Matrix_RotateYS
        vec.x = 0.0f;
        vec.y = 0.0f;
        vec.z = sGiantsMask.subCamDistZFromPlayer;
        Matrix_MultVec3f(&vec, &subCamEyeOffset); // MM Matrix_MultVecZ

        subCamEye.x = player->actor.world.pos.x + subCamEyeOffset.x;
        subCamEye.y = player->actor.world.pos.y + subCamEyeOffset.y + sGiantsMask.subCamEyeOffsetY;
        subCamEye.z = player->actor.world.pos.z + subCamEyeOffset.z;

        subCamAt.x = player->actor.world.pos.x;
        subCamAt.y = player->actor.world.pos.y + sGiantsMask.subCamAtOffsetY;
        subCamAt.z = player->actor.world.pos.z;

        // MM subCamUpRotZ
        Matrix_RotateZ(Math_SinS((s16)(sGiantsMask.csTimer * 1512)) * sGiantsMask.subCamUpRotZScale, MTXMODE_APPLY);
        vec.x = 0.0f;
        vec.y = 1.0f;
        vec.z = 0.0f;
        Matrix_MultVec3f(&vec, &subCamUp); // MM Matrix_MultVecY
        Matrix_Pop();

        Play_CameraSetAtEyeUp(play, sGiantsMask.subCamId, &subCamAt, &subCamEye, &subCamUp);
        Letterbox_SetSizeTarget(27); // MM ShrinkWindow_Letterbox_SetSizeTarget
    }
}

// ---- the mask going on and coming off (MM z_player.c) ------------------------------------------------------------

// MM func_808389BC.
static void MergeShip_GiantsMask_func_808389BC(PlayState* play, Player* this) {
    Player_SetupActionPreserveItemAction(play, this, MergeShip_GiantsMask_Action_89, 0);
    Player_AnimChangeOnceMorphAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_cl_setmask);
    // MM PLAYER_STATE1_100 | PLAYER_STATE1_20000000. Ocarina of Time gives bit 8 to
    // PLAYER_STATE1_START_CHANGING_HELD_ITEM, so the player's half of the handshake with the cutscene is csFlag.
    this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    sGiantsMask.csFlag = true;
    sGiantsMask.room = play->roomCtx.curRoom.num;
    func_80832224(this); // MM func_8082DAD4
}

// MM func_80838A20.
static void MergeShip_GiantsMask_func_80838A20(PlayState* play, Player* this) {
    Player_SetupActionPreserveItemAction(play, this, MergeShip_GiantsMask_Action_90, 0);
    Player_AnimPlayOnceAdjusted(play, this, (LinkAnimationHeader*)mm_gPlayerAnim_cl_maskoff);
    this->currentMask = PLAYER_MASK_NONE;
    this->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE; // MM PLAYER_STATE1_100 | PLAYER_STATE1_20000000
    sGiantsMask.csFlag = true;
    func_80832224(this); // MM func_8082DAD4
    sGiantsMask.draining = false;
    Magic_Reset(play);
}

// MM Player_Action_89: the Giant's Mask going on, while the cutscene makes Link a giant.
void MergeShip_GiantsMask_Action_89(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS; // MM PLAYER_STATE2_40

    MergeShip_UpdateMaskAnim(play, this); // MM func_80855218(play, this, NULL)
    this->av1.actionVar1++;

    if (!sGiantsMask.csFlag) { // MM !(this->stateFlags1 & PLAYER_STATE1_100)
        this->prevMask = this->currentMask;
        // (MM also sets gSaveContext.save.equippedMask; MergeShip_MMMasks_Update keeps gMergeShipSave's.)
        this->currentMask = PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT);
        MergeShip_GiantsMask_MagicConsume(play); // MM Magic_Consume(play, 0, MAGIC_CONSUME_GIANTS_MASK)
        sGiantsMask.giantBoots = true;           // MM currentBoots = prevBoots = PLAYER_BOOTS_GIANT
        Player_SetBootData(play, this);          // MM func_80123140
        func_80853080(this, play);               // MM func_8085B384
    }
}

// MM Player_Action_90: the Giant's Mask coming off, while the cutscene makes Link his own size again.
void MergeShip_GiantsMask_Action_90(Player* this, PlayState* play) {
    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_ALWAYS; // MM PLAYER_STATE2_40

    LinkAnimation_Update(play, &this->skelAnime);

    if (!sGiantsMask.csFlag) { // MM !(this->stateFlags1 & PLAYER_STATE1_100)
        this->prevMask = this->currentMask;
        this->currentMask = PLAYER_MASK_NONE;
        sGiantsMask.giantBoots = false; // MM currentBoots = prevBoots = PLAYER_BOOTS_HYLIAN
        Player_SetBootData(play, this); // MM func_80123140
        func_80853080(this, play);      // MM func_8085B384
    }
}

// MM Player_UseItem, the `var_v1` block for PLAYER_IA_MASK_GIANT: refused (NA_SE_SY_ERROR) while the magic meter is
// busy or empty, or under a ceiling too low for the human; otherwise deferred to Player_ActionHandler_13
// (MM unk_AA5 = PLAYER_UNKAA5_5, Ocarina of Time's unk_6AD = 4).
void MergeShip_GiantsMask_Use(PlayState* play, Player* this) {
    CollisionPoly* poly;
    s32 bgId;
    f32 ceilingY;

    // (MM's Player_ProcessItemButtons does not run during the cutscene; Ocarina of Time's "mask left the C buttons"
    // check does, and must not queue the mask again then.)
    if (sGiantsMask.csFlag) {
        return;
    }

    // MergeShip2: the Giant's Mask goes on only where MergeShip.GiantsMaskInLairs allows it (its button is disabled
    // elsewhere, as MM disables it outside Twinmold's lair); it can always come off.
    if (!MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT) && !MergeShip_GiantsMaskAllowedHere(play)) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return;
    }

    if ((!MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT) &&
         ((gSaveContext.magicState != MAGIC_STATE_IDLE) || (gSaveContext.magic == 0))) ||
        (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) &&
         BgCheck_EntityCheckCeiling(&play->colCtx, &ceilingY, &this->actor.world.pos,
                                    sAgeProperties[this->mergeShipForm].ceilingCheckHeight, &poly, &bgId,
                                    &this->actor))) {
        MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        return;
    }

    this->itemAction = PLAYER_IA_MM_MASK_GIANT;
    this->unk_6AD = 4; // MM unk_AA5 = PLAYER_UNKAA5_5
}

// MM Player_ActionHandler_13's mask branch for PLAYER_IA_MASK_GIANT and a human.
s32 MergeShip_GiantsMask_ActionHandler13(PlayState* play, Player* this) {
    u8 maskId = PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT);

    this->prevMask = this->currentMask;
    if (maskId == this->currentMask) {
        this->currentMask = PLAYER_MASK_NONE;
        MergeShip_GiantsMask_func_80838A20(play, this);
    } else {
        this->currentMask = maskId;
        MergeShip_GiantsMask_func_808389BC(play, this);
    }
    return true;
}

// MM Player_ProcessItemButtons: `if ((currentMask == PLAYER_MASK_GIANT) && (magic == 0)) func_80838A20(play, this)`,
// behind its early returns.
void MergeShip_GiantsMask_CheckMagic(PlayState* play, Player* this) {
    if ((this->actor.id != ACTOR_PLAYER) ||
        (this->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_IN_CUTSCENE)) ||
        MergeShip_MMMasks_BlocksItems(this) || func_8008F128(this) || MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return;
    }
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT) && (gSaveContext.magic == 0)) {
        MergeShip_GiantsMask_func_80838A20(play, this);
    }
}

// Link leaves the room he grew in (or the mask's options are turned off) while grown: he is his own size again at
// once, as MM's Player_Init takes the mask off when a scene loads.
static void MergeShip_GiantsMask_ForceOff(PlayState* play, Player* this) {
    MergeShip_GiantsMask_StopCutsceneCamera(play);
    if (sGiantsMask.flashState != MS_GIANTS_CS_FLASH_STATE_NOT_STARTED) {
        sGiantsMask.flashState = MS_GIANTS_CS_FLASH_STATE_NOT_STARTED;
        R_PLAY_FILL_SCREEN_ON = false;
    }
    sGiantsMask.csState = MS_GIANTS_CS_STATE_WAITING_FOR_MASK;
    sGiantsMask.csFlag = false;
    sGiantsMask.isGiant = false;
    if (sGiantsMask.draining) {
        sGiantsMask.draining = false;
        Magic_Reset(play);
    }
    if (MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT)) {
        this->prevMask = this->currentMask;
        this->currentMask = PLAYER_MASK_NONE;
    }
    if ((this->actionFunc == MergeShip_GiantsMask_Action_89) || (this->actionFunc == MergeShip_GiantsMask_Action_90)) {
        func_80853080(this, play); // MM func_8085B384
    }
    if (sGiantsMask.giantBoots) {
        sGiantsMask.giantBoots = false;
        Player_SetBootData(play, this);
    }
    MergeShip_GiantsMask_ApplyRatio(this, 1.0f);
}

// Every frame the player updates (Player_UpdateCommon): the magic drain and Twinmold's cutscene, which MM runs from
// Magic_Update and Boss_02's update.
void MergeShip_GiantsMask_Update(Player* this, PlayState* play) {
    if (this->actor.id != ACTOR_PLAYER) {
        return;
    }

    if ((sGiantsMask.isGiant || (sGiantsMaskRatio != 1.0f) ||
         (sGiantsMask.csState != MS_GIANTS_CS_STATE_WAITING_FOR_MASK)) &&
        ((play->roomCtx.curRoom.num != sGiantsMask.room) ||
         !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMMasks"), 1) ||
         !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GiantsMaskInLairs"), 0))) {
        MergeShip_GiantsMask_ForceOff(play, this);
    }

    MergeShip_GiantsMask_UpdateMagic(play);
    MergeShip_GiantsMask_HandleCutscene(play, this);
}

// MM Player_Init: `if (gSaveContext.save.equippedMask == PLAYER_MASK_GIANT) equippedMask = PLAYER_MASK_NONE` -- a
// scene never starts with the Giant's Mask on (MergeShip_MMMasks_RestoreOnSceneLoad skips it), and Link starts at his
// own size. Called before the rest of Player_Init.
void MergeShip_GiantsMask_OnSceneLoad(PlayState* play) {
    if (gMergeShipSave.equippedMMMask == MM_MASK_GIANT) {
        gMergeShipSave.equippedMMMask = MM_MASK_NONE;
    }
    if (sGiantsMask.flashState != MS_GIANTS_CS_FLASH_STATE_NOT_STARTED) {
        R_PLAY_FILL_SCREEN_ON = false;
    }
    if (sGiantsMask.csState != MS_GIANTS_CS_STATE_WAITING_FOR_MASK) {
        MergeShip_MotionBlur_Disable();
    }
    if (sGiantsMask.draining && (gSaveContext.magicState == MAGIC_STATE_METER_FLASH_3)) {
        Magic_Reset(play);
    }
    memset(&sGiantsMask, 0, sizeof(sGiantsMask)); // (MM Boss02_Init: sCanSkipMaskOnCs = sCanSkipMaskOffCs = false)
    sGiantsMask.subCamId = CAM_ID_MAIN;
    sGiantsMask.targetRatio = 1.0f;
    sGiantsMask.camScale = 1.0f;
    sGiantsMaskRatio = 1.0f;
}

// ---- while the mask is worn ---------------------------------------------------------------------------------------

// MM D_801BFE14[PLAYER_BOOTS_GIANT] (z_player_lib.c), in the order func_80123140 writes it.
static const s16 sMMGiantBootData[18] = {
    100, 1000, 300, 800, 250, 200, 800, 200, 90, 800, 350, -80, 600, 540, 750, 60, 200, 200,
};

// MM func_80123140 with PLAYER_BOOTS_GIANT, for Player_SetBootData. Returns true when it set the registers.
s32 MergeShip_GiantsMask_SetBootData(PlayState* play, Player* this) {
    const s16* bootRegs = sMMGiantBootData;

    if ((this->actor.id != ACTOR_PLAYER) || !sGiantsMask.giantBoots || MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return false;
    }

    REG(27) = 2000;
    REG(48) = 170;

    REG(19) = bootRegs[0];
    REG(30) = bootRegs[1];
    REG(32) = bootRegs[2];
    REG(34) = bootRegs[3];
    REG(35) = bootRegs[4];
    REG(36) = bootRegs[5];
    REG(37) = bootRegs[6];
    REG(38) = bootRegs[7];
    REG(39) = bootRegs[8];
    REG(43) = bootRegs[9];  // MM R_DECELERATE_RATE
    REG(45) = bootRegs[10]; // MM R_RUN_SPEED_LIMIT
    REG(68) = bootRegs[11]; // gravity
    REG(69) = bootRegs[12];
    IREG(66) = bootRegs[13];
    IREG(67) = bootRegs[14];
    IREG(68) = bootRegs[15];
    IREG(69) = bootRegs[16];
    MREG(95) = bootRegs[17];

    // MM: `if (play->roomCtx.curRoom.type == ROOM_TYPE_INDOORS) R_RUN_SPEED_LIMIT = 500`.
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }

    // (MM ends with Actor_SetScale(0.01f): its giant keeps Link's own scale because the arena shrank. The grown Link's
    // scale is MergeShip_GiantsMask_ApplyRatio's.)
    return true;
}

// Player_UpdateCommon, after Actor_UpdatePos: MM's giant moves at its own (slow) speeds through an arena shrunk to a
// tenth; the grown Link, in a world that keeps its size, covers `ratio` times the distance, so relative to the world
// he moves as MM's giant does.
void MergeShip_GiantsMask_ScaleMovement(Player* this) {
    f32 extra = MergeShip_GiantsMask_SizeRatio(this) - 1.0f;

    if (extra != 0.0f) {
        f32 speedRate = R_UPDATE_RATE * 0.5f * extra; // Actor_UpdatePos

        this->actor.world.pos.x += this->actor.velocity.x * speedRate;
        this->actor.world.pos.y += this->actor.velocity.y * speedRate;
        this->actor.world.pos.z += this->actor.velocity.z * speedRate;
    }
}

// The fall is measured in Link's own body lengths, as MM's giant's is in its shrunk arena.
void MergeShip_GiantsMask_ScaleFallDistance(Player* this) {
    f32 ratio = MergeShip_GiantsMask_SizeRatio(this);

    if (ratio != 1.0f) {
        this->fallDistance = (s16)(this->fallDistance / ratio);
    }
}

// MM func_80839860: `if (this->currentBoots == PLAYER_BOOTS_GIANT) speed /= 2.0f`.
f32 MergeShip_GiantsMask_HopSpeed(Player* this, f32 speed) {
    if ((this->actor.id == ACTOR_PLAYER) && sGiantsMask.giantBoots) {
        speed /= 2.0f;
    }
    return speed;
}

// z_player_lib.c func_8008F87C (the legs reaching for the floor): its leg lengths and foot height are world units for
// Link at his own scale.
void MergeShip_GiantsMask_ScaleFootIK(Player* this, f32* legLengthSq, f32* legLengthSqDiff, f32* footHeight,
                                      Vec3f* footprintPos) {
    f32 ratio = MergeShip_GiantsMask_SizeRatio(this);

    if (ratio != 1.0f) {
        *legLengthSq *= SQ(ratio);
        *legLengthSqDiff *= SQ(ratio);
        *footHeight *= ratio;
        footprintPos->y += 15.0f * (ratio - 1.0f);
    }
}

// MM Player_AnimSfx_PlayVoice: `if (this->currentMask == PLAYER_MASK_GIANT) Audio_PlaySfx_GiantsMask(...)` -- the voice
// of MM's voice bank 1 ((sfxId & 0x681F) + 0x20), lowered and with reverb. Returns true when handled.
s32 MergeShip_GiantsMask_PlayVoiceSfx(Player* this, u16 sfxId) {
    static f32 sGiantsMaskFreq = 0.89167805f; // MM sGiantsMaskFreq ("around 2 semitones down in pitch")
    static s8 sGiantsMaskReverbAdd = 0x40;    // MM sGiantsMaskReverbAdd
    u16 mmSfxId;

    if (!MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT)) {
        return false;
    }
    mmSfxId = MergeShip_OoTToMMSfxId(sfxId);
    if (mmSfxId != 0) {
        MergeShip_MMSfx_PlaySfx((mmSfxId & 0x681F) + 0x20, &this->actor.projectedPos, 4, &sGiantsMaskFreq,
                                &gSfxDefaultFreqAndVolScale, &sGiantsMaskReverbAdd);
    }
    return true;
}

// MM Player_AnimSfx_PlayFloorWalk: `if (this->currentMask == PLAYER_MASK_GIANT) sfxId = NA_SE_PL_GIANT_WALK`.
s32 MergeShip_GiantsMask_PlayWalkSfx(Player* this, f32 freqVolumeLerp) {
    if (!MERGESHIP_WEARS_MM_MASK(this, MM_MASK_GIANT)) {
        return false;
    }
    MergeShip_MMSfx_PlayAtPosForMetalEffectsWithSyncedFreqAndVolume(&this->actor.projectedPos, MM_NA_SE_PL_GIANT_WALK,
                                                                     freqVolumeLerp);
    return true;
}

// MM Player_PlaySfx (z_actor.c): with the Giant's Mask on, every player sound goes through
// Audio_PlaySfx_AtPosWithPresetLowFreqAndHighReverb. This is the path of Link's Ocarina of Time sounds (MM's own
// sounds, for MM's human as the child, take the same preset in MergeShipMMSfx.c). Returns true when handled.
s32 MergeShip_GiantsMask_PlayPlayerSfx(Actor* actor, u16 sfxId) {
    static f32 sTwoSemitonesLoweredFreq = 0.9f; // MM sTwoSemitonesLoweredFreq (Audio_ResetSfx)
    static s8 sSfxIncreasedReverb = 20;         // MM sSfxIncreasedReverb

    if ((actor == NULL) || !MergeShip_GiantsMask_IsWornBy((Player*)actor)) {
        return false;
    }
    Audio_PlaySfxGeneral(sfxId, &actor->projectedPos, 4, &sTwoSemitonesLoweredFreq, &gSfxDefaultFreqAndVolScale,
                         &sSfxIncreasedReverb);
    GameInteractor_ExecuteOnPlayerSfx(sfxId);
    return true;
}

// MM func_80128640's mask branch for the Giant's Mask, from z_player_lib.c's MergeShip_DrawMaskInHand (the Giant's Mask
// is the one regular mask put on with gPlayerAnim_cl_setmask and taken off with gPlayerAnim_cl_maskoff).
void MergeShip_GiantsMask_DrawInHand(PlayState* play, Player* player, s32 mask) {
    if (mask != PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT)) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(-323.67f, 412.15f, -969.96f, MTXMODE_APPLY);
    Matrix_RotateZYX(-0x32BE, -0x50DE, -0x7717, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)mm_object_mask_kyojin_DL_000380); // MM D_801C0B20[PLAYER_MASK_GIANT - 1]
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}
