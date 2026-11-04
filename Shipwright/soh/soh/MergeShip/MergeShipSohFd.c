#include "global.h"
#include "soh/MergeShip/MergeShipSohFd.h"
#include "soh/MergeShip/MergeShipSohFdAssets.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipItems.h"
#include "soh/MergeShip/MergeShipMaskItems.h"
#include "soh/MergeShip/MergeShipMMSfx.h"       // MM_NA_SE_PL_TRANSFORM_VOICE
#include "soh/MergeShip/MergeShipTransformFx.h" // MergeShip_PlayMMSfxActor
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);

// MergeShip2 (soh_fd) -- options, form predicates and the per-form item rules carried over from soh_fd.
// See MergeShipSohFd.h.

extern PlayState* gPlayState;

// ---- options -----------------------------------------------------------------------------------------------------

static s32 MergeShip_SohFd_IsHuman(u8 form) {
    return (form == PLAYER_FORM_ADULT) || (form == PLAYER_FORM_CHILD);
}

s32 MergeShip_SohFd_FlipMode(Player* player) {
    // Majora's Mask gives its flips to the human and the Fierce Deity; the Deku hops, the Goron rolls and the Zora
    // dives, so they never flip.
    if ((player == NULL) ||
        !(MergeShip_SohFd_IsHuman(player->mergeShipForm) || (player->mergeShipForm == PLAYER_FORM_FIERCE_DEITY))) {
        return MERGESHIP_FLIPS_OFF;
    }
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMFlips"), MERGESHIP_FLIPS_OFF);
}

s32 MergeShip_SohFd_LedgeMomentumApplies(Player* player) {
    s32 mode = CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMLedgeMomentum"), MERGESHIP_MOMENTUM_FORMS);

    if (player == NULL) {
        return false;
    }
    if (mode == MERGESHIP_MOMENTUM_ALL) {
        return true;
    }
    return (mode == MERGESHIP_MOMENTUM_FORMS) && MERGESHIP_FORM_IS_MM(player->mergeShipForm);
}

s32 MergeShip_SohFd_RollDamageApplies(Player* player) {
    // 0 off, 1 human Link, 2 the transformation forms (soh_fd's default), 3 everyone.
    s32 mode = CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMRollDamage"), 2);

    if (player == NULL) {
        return false;
    }
    switch (mode) {
        case 1:
            return MergeShip_SohFd_IsHuman(player->mergeShipForm);
        case 2:
            return MERGESHIP_FORM_IS_MM(player->mergeShipForm);
        case 3:
            return true;
        default:
            return false;
    }
}

s32 MergeShip_SohFd_BunnyHoodFitApplies(Player* player) {
    // 0 off, 1 Adult Link, 2 the Fierce Deity, 3 both (default).
    s32 mode = CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.BunnyHoodFit"), 3);

    if (player == NULL) {
        return false;
    }
    if (player->mergeShipForm == PLAYER_FORM_ADULT) {
        return (mode == 1) || (mode == 3);
    }
    if (player->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) {
        return (mode == 2) || (mode == 3);
    }
    // TODO(MergeShip2 soh_fd): the Deku, Goron and Zora heads need their own Bunny Hood placement (and their own
    // dropdown entries) before they can wear it well; until then they keep the hood where the head limb puts it.
    return false;
}

s32 MergeShip_SohFd_DogReactionMode(u8 playerForm) {
    switch (playerForm) {
        case PLAYER_FORM_DEKU:
            // soh_fd's default: every dog but Richard, so the lost-dog errand still works with the mask on.
            return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.DogsReactDeku"), MERGESHIP_DOGS_ALL_BUT_RICHARD);
        case PLAYER_FORM_GORON:
            return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.DogsReactGoron"), MERGESHIP_DOGS_ALL);
        case PLAYER_FORM_ZORA:
            return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.DogsReactZora"), MERGESHIP_DOGS_ALL);
        default:
            return MERGESHIP_DOGS_NONE;
    }
}

s32 MergeShip_SohFd_FormsWearTradeMasks(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FormsWearTradeMasks"), 0);
}

s32 MergeShip_SohFd_StuckSafeguards(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.StuckSafeguards"), 1);
}

s32 MergeShip_SohFd_PreventRestrictedActions(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.PreventRestrictedActions"), 1);
}

s32 MergeShip_SohFd_SceneStartSafeguards(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.SceneStartSafeguards"), 1);
}

s32 MergeShip_SohFd_InstrumentIcons(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FormInstrumentIcons"), 0);
}

s32 MergeShip_SohFd_AdultScream(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.AdultTransformScream"), 1);
}

s32 MergeShip_SohFd_GetMaskFanfare(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GetMaskFanfare"), 1);
}

s32 MergeShip_SohFd_FdCanPlayOcarina(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdCanPlayOcarina"), 0);
}

s32 MergeShip_SohFd_FdCanUseMasks(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdCanUseMasks"), 1);
}

s32 MergeShip_SohFd_FdCanSheathe(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdCanSheathe"), 0);
}

s32 MergeShip_SohFd_FdBeamsBreakRocks(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdBeamsBreakRocks"), 0);
}

s32 MergeShip_SohFd_FdBeamsLightFire(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdBeamsLightFire"), 0);
}

s32 MergeShip_SohFd_FdMagicSpin(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdMagicSpin"), 0);
}

s32 MergeShip_SohFd_FdIncreasedStrength(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdIncreasedStrength"), 0);
}

s32 MergeShip_SohFd_FdUnrestrictItems(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FdUnrestrictItems"), 0);
}

s32 MergeShip_SohFd_FdAnywhere(void) {
    return CVarGetInteger(CVAR_GENERAL("MergeShipFierceDeityAnywhere"), 0) ||
           CVarGetInteger(CVAR_GENERAL("MergeShipDpadMasks"), 0);
}

// ---- forms ---------------------------------------------------------------------------------------------------------

s32 MergeShip_SohFd_PlayerIsTransformed(void) {
    return (gPlayState != NULL) && (GET_PLAYER(gPlayState) != NULL) &&
           MERGESHIP_FORM_IS_MM(GET_PLAYER(gPlayState)->mergeShipForm);
}

s32 MergeShip_SohFd_FierceDeityAllowedInScene(s32 sceneNum) {
    if (MergeShip_SohFd_FdAnywhere()) {
        return true;
    }
    switch (sceneNum) {
        // Majora's Mask: its boss lairs. Hyrule's counterparts are its dungeons' boss rooms and both of Ganon's
        // arenas.
        case SCENE_DEKU_TREE_BOSS:
        case SCENE_DODONGOS_CAVERN_BOSS:
        case SCENE_JABU_JABU_BOSS:
        case SCENE_FOREST_TEMPLE_BOSS:
        case SCENE_FIRE_TEMPLE_BOSS:
        case SCENE_WATER_TEMPLE_BOSS:
        case SCENE_SPIRIT_TEMPLE_BOSS:
        case SCENE_SHADOW_TEMPLE_BOSS:
        case SCENE_GANONDORF_BOSS:
        case SCENE_GANON_BOSS:
        // soh_fd: the fishing hole too, as Majora's Mask 3D lets the Fierce Deity fish in its fishing holes.
        case SCENE_FISHING_POND:
            return true;
        default:
            return false;
    }
}

void MergeShip_SohFd_OnSceneLoad(s32 destScene) {
    u8 human = (u8)gSaveContext.linkAge;

    if ((gMergeShipSave.mmForm != MM_FORM_FIERCE_DEITY) || MergeShip_SohFd_FierceDeityAllowedInScene(destScene)) {
        return;
    }
    // Before Player_Init, so Link's own skeleton is the one loaded (no flash on arrival), and the B button goes back
    // to Link's sword as a transformation back would.
    MergeShip_SwapFormEquips(PLAYER_FORM_FIERCE_DEITY, human);
    MergeShip_SetPlayerForm(human);
}

s32 MergeShip_SohFd_FormItemOverride(u8 form, u8 item) {
    if (!MERGESHIP_FORM_IS_MM(form)) {
        return -1;
    }

    // soh_fd: Roc's Feather (SoH's jump) is every form's -- BUG-0011: the Goron's too (curled up or rolling, the ball
    // jumps: MergeShip_Goron_RocsBallJump).
    if (item == ITEM_ROCS_FEATHER) {
        return 1;
    }

    // soh_fd: the fishing hole's rod, which every form may fish with, as in Majora's Mask 3D's fishing holes.
    if (item == ITEM_FISHING_POLE) {
        return 1;
    }

    // soh_fd: the Fierce Deity puts the other transformation masks on directly ("FD Can Use Transformation Masks";
    // Majora's Mask's table makes him take his own off first). Hyrule's Goron and Zora masks count while they are the
    // transformation masks, so this comes before the trade masks.
    if ((form == PLAYER_FORM_FIERCE_DEITY) && MergeShip_SohFd_FdCanUseMasks() &&
        MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(MergeShip_ResolveMaskItem(item)) &&
        (MergeShip_ResolveMaskItem(item) != ITEM_MM_MASK_FIERCE_DEITY)) {
        return 1;
    }

    // soh_fd "Forms Wear Trade Masks": Hyrule's own masks may be worn over a form.
    if ((item >= ITEM_MASK_KEATON) && (item <= ITEM_MASK_TRUTH)) {
        return MergeShip_SohFd_FormsWearTradeMasks() ? 1 : -1;
    }

    if (form == PLAYER_FORM_FIERCE_DEITY) {
        s32 unrestrict = MergeShip_SohFd_FdUnrestrictItems();

        // "Unrestrict Items for FD": 2 = everything but swords and shields, like Timeless Equipment for the deity.
        // Hyrule's items only: Majora's Mask's own (its masks, equipment and items) keep Majora's Mask's rules.
        if ((unrestrict >= 2) && (item < ITEM_MM_MASK_FIERCE_DEITY)) {
            if (((item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) ||
                ((item >= ITEM_SHIELD_DEKU) && (item <= ITEM_SHIELD_MIRROR))) {
                return 0;
            }
            return 1;
        }
        // 1 = Deku Nuts, bombs, Bombchus, the three spells and the Lens of Truth.
        if ((unrestrict == 1) &&
            ((item == ITEM_NUT) || (item == ITEM_BOMB) || (item == ITEM_BOMBCHU) || (item == ITEM_DINS_FIRE) ||
             (item == ITEM_FARORES_WIND) || (item == ITEM_NAYRUS_LOVE) || (item == ITEM_LENS))) {
            return 1;
        }
        // "FD Can Play Ocarina".
        if (((item == ITEM_OCARINA_FAIRY) || (item == ITEM_OCARINA_TIME)) && MergeShip_SohFd_FdCanPlayOcarina()) {
            return 1;
        }
    }
    return -1;
}

// ---- Hyrule's masks over a form, and the Bunny Hood Fit ---------------------------------------------------------

// The Hyrule mask worn over a form. A form's currentMask is its own mask (MergeShip), so this one is kept aside.
static u8 sFormTradeMask = PLAYER_MASK_NONE;

u8 MergeShip_SohFd_FormTradeMask(void) {
    return MergeShip_SohFd_FormsWearTradeMasks() ? sFormTradeMask : PLAYER_MASK_NONE;
}

void MergeShip_SohFd_SetFormTradeMask(u8 mask) {
    sFormTradeMask = mask;
}

u8 MergeShip_SohFd_WornHyruleMask(PlayState* play, Player* player) {
    if (player->currentMask < PLAYER_MASK_MAX) {
        return player->currentMask;
    }
    if (MERGESHIP_FORM_IS_MM(player->mergeShipForm) && (player == GET_PLAYER(play))) {
        return MergeShip_SohFd_FormTradeMask();
    }
    return PLAYER_MASK_NONE;
}

// soh_fd's Bunny Hood Fit, in head-local units (-Y is up). The hood is made for Young Link's head and sinks into the
// taller Adult and Fierce Deity heads: it is lifted, tilted back so its eyes tuck in, and (at 1.0) not resized.
#define MASKFIT_BUNNY_ADULT_RAISE 220.0f
#define MASKFIT_BUNNY_DEITY_RAISE 100.0f
#define MASKFIT_BUNNY_ADULT_PITCH 0x0900 // ~12.7 degrees
#define MASKFIT_BUNNY_DEITY_PITCH 0x0900
#define MASKFIT_BUNNY_ADULT_SCALE 1.0f
#define MASKFIT_BUNNY_DEITY_SCALE 1.0f

static s32 MergeShip_SohFd_BunnyHoodFit(Player* player, f32* raiseY, s16* pitch, f32* scale) {
    if (!MergeShip_SohFd_BunnyHoodFitApplies(player)) {
        return false;
    }
    if (player->mergeShipForm == PLAYER_FORM_ADULT) {
        *raiseY = -MASKFIT_BUNNY_ADULT_RAISE;
        *pitch = MASKFIT_BUNNY_ADULT_PITCH;
        *scale = MASKFIT_BUNNY_ADULT_SCALE;
    } else {
        *raiseY = -MASKFIT_BUNNY_DEITY_RAISE;
        *pitch = MASKFIT_BUNNY_DEITY_PITCH;
        *scale = MASKFIT_BUNNY_DEITY_SCALE;
    }
    return true;
}

static Mtx* sSohFdMaskMtxList = NULL;

void MergeShip_SohFd_DrawOnHead(PlayState* play, Player* player, Gfx** dList) {
    u8 mask = MergeShip_SohFd_WornHyruleMask(play, player);
    f32 raiseY = 0.0f;
    s16 pitch = 0;
    f32 scale = 1.0f;
    s32 fit;
    s32 i;

    sSohFdMaskMtxList = NULL;
    if ((mask == PLAYER_MASK_NONE) || (*dList == NULL)) {
        return;
    }
    fit = (mask == PLAYER_MASK_BUNNY) && MergeShip_SohFd_BunnyHoodFit(player, &raiseY, &pitch, &scale);
    // A human's mask is drawn from its skeleton's list as Ocarina of Time draws it. A form's skeleton need not have its
    // head at the entry the masks load (7), so a form always gets the head's own matrices.
    // TODO(MergeShip2 soh_fd): the Deku, Goron and Zora heads still need each mask placed for them (a fit per form);
    // until then a mask sits where the head limb puts it.
    if (!fit && !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return;
    }
    // Built here, from the matrix stack while the head's is on it, so SoH's frame interpolation replays them with the
    // skeleton's (a saved matrix would trail the head at high frame rates).
    sSohFdMaskMtxList = Graph_Alloc(play->state.gfxCtx, 8 * sizeof(Mtx));
    // MergeShip2: its own frame-interpolation child (drawn on some frames only; see MergeShipMMMasks.c).
    FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    Matrix_Push();
    if (fit) {
        Matrix_Translate(0.0f, raiseY, 0.0f, MTXMODE_APPLY);
        if (pitch != 0) {
            // A backward tilt is a pitch about world X, which is the head's local Z.
            Matrix_RotateZ(BINANG_TO_RAD(pitch), MTXMODE_APPLY);
        }
        if (scale != 1.0f) {
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
        }
    }
    for (i = 0; i < 8; i++) {
        MATRIX_TOMTX(&sSohFdMaskMtxList[i]);
    }
    Matrix_Pop();
    FrameInterpolation_RecordCloseChild();
}

Mtx* MergeShip_SohFd_TakeMaskMtxList(void) {
    Mtx* list = sSohFdMaskMtxList;

    sSohFdMaskMtxList = NULL;
    return list;
}

// ---- the Fierce Deity's ocarina ------------------------------------------------------------------------------------

// soh_fd: the Fierce Deity's ocarina hand is his bare hand (MM gives him none), so the ocarina -- the Fairy Ocarina
// or the Ocarina of Time, whichever Link owns, cut from Hyrule's own models -- is drawn on it, at the hand's matrix.
void MergeShip_SohFd_DrawFdOcarina(PlayState* play, Player* player) {
    Gfx* ocarina;

    if ((player->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) || !MergeShip_SohFd_AssetsMounted()) {
        return;
    }
    if (player->rightHandType == PLAYER_MODELTYPE_RH_OCARINA) {
        ocarina = (INV_CONTENT(ITEM_OCARINA_FAIRY) == ITEM_OCARINA_FAIRY) ? (Gfx*)gMergeShip2FdFairyOcarinaDL
                                                                          : (Gfx*)gMergeShip2FdOotOcarinaDL;
    } else if (player->rightHandType == PLAYER_MODELTYPE_RH_OOT) {
        ocarina = (Gfx*)gMergeShip2FdOotOcarinaDL;
    } else {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, ocarina);
    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- "Adult Link Transformation Scream" ---------------------------------------------------------------------------

s32 MergeShip_SohFd_PlayAdultScream(Player* player) {
    if (!MergeShip_SohFd_AdultScream() || (player->mergeShipForm != PLAYER_FORM_ADULT) ||
        (player->actor.id != ACTOR_PLAYER) || !MergeShip_SohFd_AssetsMounted()) {
        return false;
    }
    // Majora's Mask's own transformation voice, with Adult Link's recording in its drums (MergeShipSohFdAudio.cpp):
    // it plays through MM's sfx player like any other sound.
    if (!MergeShip_SohFd_SwapInAdultVoice()) {
        return false;
    }
    MergeShip_PlayMMSfxActor(&player->actor, MM_NA_SE_PL_TRANSFORM_VOICE);
    return true;
}

// ---- "Get-a-Mask Fanfare" ------------------------------------------------------------------------------------------

s32 MergeShip_MMAudio_PlayMMFanfare(s32 which);     // MergeShipAudio.cpp
int MergeShip_OoTMasksAreTransformationMasks(void); // MergeShipMaskItems.cpp
#define MERGESHIP_MM_FANFARE_GET_MASK 2             // MergeShipAudio.cpp kMaskFanfares, MM NA_BGM_GET_NEW_MASK

s32 MergeShip_SohFd_PlayGetMaskFanfare(s32 isTransformationMask, s16 ootItemId) {
    if (!MergeShip_SohFd_GetMaskFanfare()) {
        return false;
    }
    if (!isTransformationMask && !(((ootItemId == ITEM_MASK_GORON) || (ootItemId == ITEM_MASK_ZORA)) &&
                                   MergeShip_OoTMasksAreTransformationMasks())) {
        return false;
    }
    return MergeShip_MMAudio_PlayMMFanfare(MERGESHIP_MM_FANFARE_GET_MASK);
}

// ---- the equipment screen's Link ------------------------------------------------------------------------------------

u8 MergeShip_SohFd_PauseForm(void) {
    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.FormPauseModel"), 1)) {
        return (u8)gSaveContext.linkAge;
    }
    return MergeShip_GetPlayerForm();
}

s32 MergeShip_SohFd_PauseIsChildSized(void) {
    u8 form = MergeShip_SohFd_PauseForm();

    return (form == PLAYER_FORM_CHILD) || (form == PLAYER_FORM_DEKU);
}

// soh_fd's heights, set against the rendered pause screen: the Deku's root sits well above the child's, so it is
// lowered by 100 (72 when animated, whose idle roots lower); the Fierce Deity is right when still and sinks once
// animated, so he is raised by 45 then. The Goron and the Zora stand in their own idle, whose roots sit as the adult's.
f32 MergeShip_SohFd_PauseDropY(u8 form, s32 animated) {
    switch (form) {
        case PLAYER_FORM_DEKU:
            return animated ? 72.0f : 100.0f;
        case PLAYER_FORM_FIERCE_DEITY:
            return animated ? -45.0f : 0.0f;
        default:
            return 0.0f;
    }
}

s32 MergeShip_SohFd_PauseShiftsLeft(u8 form, s32 animated) {
    return animated && ((form == PLAYER_FORM_ADULT) || (form == PLAYER_FORM_FIERCE_DEITY) ||
                        (form == PLAYER_FORM_GORON) || (form == PLAYER_FORM_ZORA));
}

// ---- the fishing hole ------------------------------------------------------------------------------------------

// Each form's closed left hand, measured from its display list (mm.o2r): its fist spans y 2-799 (Fierce Deity),
// -79-294 (Deku), -68-1062 (Goron, centred off the hand's axis at x 238, z -154) and -52-485 (Zora). The Fierce
// Deity's and the Deku's grips are soh_fd's, set in game; the Goron's and the Zora's are their fists' centres.
s32 MergeShip_SohFd_RodGrip(u8 form, Vec3f* grip) {
    switch (form) {
        case PLAYER_FORM_FIERCE_DEITY:
            *grip = (Vec3f){ 0.0f, 340.0f, 0.0f };
            return true;
        case PLAYER_FORM_DEKU:
            *grip = (Vec3f){ 0.0f, 210.0f, 0.0f };
            return true;
        case PLAYER_FORM_GORON:
            *grip = (Vec3f){ 238.0f, 497.0f, -154.0f };
            return true;
        case PLAYER_FORM_ZORA:
            *grip = (Vec3f){ 10.0f, 216.0f, -40.0f };
            return true;
        default:
            return false;
    }
}

// ---- the fishing pond's buttons -------------------------------------------------------------------------------------

// The pond disables every button while the rod is not in hand (z_parameter.c). soh_fd re-enabled the Fierce Deity's
// Mask there afterwards -- the pond is one of his places -- so a form could come and go; every transformation mask
// the player may use does the same here, and the rest stay gray as in Ocarina of Time.
void MergeShip_SohFd_PondMaskButtons(PlayState* play) {
    Player* player = GET_PLAYER(play);
    s32 i;

    if (player == NULL) {
        return;
    }
    for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        u8 item = MergeShip_ResolveMaskItem(gSaveContext.equips.buttonItems[i]);

        if (MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(item) && MergeShip_FormCanUseItem(player->mergeShipForm, item) &&
            ((item != ITEM_MM_MASK_FIERCE_DEITY) || MergeShip_FierceDeityMaskAllowedHere(play))) {
            gSaveContext.buttonStatus[(i >= 4) ? (i + 1) : i] = BTN_ENABLED; // C buttons 1-3, D-pad 5-8
        }
    }
}

// ---- the pause menu's items for a form ---------------------------------------------------------------------------

// MM KaleidoScope_Update (PAUSE_STATE_OPENING_0): every item icon the current form's gPlayerFormItemRestrictions
// row refuses is grayed (KaleidoScope_GrayOutTextureRGBA32); soh_fd grayed them too. The rule is the one the C
// buttons use (MergeShip_FormCanUseItem).
s32 MergeShip_SohFd_PauseItemRefused(u8 item) {
    Player* player = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;

    return (player != NULL) && MERGESHIP_FORM_IS_MM(player->mergeShipForm) && (item != ITEM_NONE) &&
           !MergeShip_FormCanUseItem(player->mergeShipForm, item);
}

// ---- the ocarina item icon of a form's instrument ----------------------------------------------------------------

void* MergeShip_SohFd_InstrumentIconSwap(void* baseTex, u8 item) {
    Player* player;

    if (((item != ITEM_OCARINA_FAIRY) && (item != ITEM_OCARINA_TIME)) || !MergeShip_SohFd_InstrumentIcons() ||
        !MergeShip_SohFd_AssetsMounted() || (gPlayState == NULL) || ((player = GET_PLAYER(gPlayState)) == NULL)) {
        return baseTex;
    }
    switch (player->mergeShipForm) {
        case PLAYER_FORM_DEKU:
            return (void*)gMergeShip2DekuPipesIconTex;
        case PLAYER_FORM_GORON:
            return (void*)gMergeShip2GoronDrumsIconTex;
        case PLAYER_FORM_ZORA:
            return (void*)gMergeShip2ZoraGuitarIconTex;
        default:
            return baseTex;
    }
}
