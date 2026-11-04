// MergeShip2 -- Link on the pause screen's equipment views. See MergeShipPauseLink.h.

#include "MergeShipGanondorf.h" // MergeShip2: Ganondorf mode (BUG-0133)
#include "global.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/MergeShip/MergeShipPauseLink.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h"
#include "2s2h/MergeShip/MergeShipAnchorColors.h"

void MergeShipTest_Log(const char* fmt, ...);
// z_player.c (the player overlay, linked into the same binary): MM's idle animation for the player's form and model.
PlayerAnimationHeader* Player_GetIdleAnim(Player* this);

// Ocarina of Time's player image on the equipment page (OoT KaleidoScope_InitVertices, equipVtx[80..]): 64 x 112 page
// units from (-64, 50), drawn by a camera at z -400 looking at the origin, fovy 60 (OoT Player_DrawPause).
#define MS_PL_IMAGE_LEFT (-64.0f)
#define MS_PL_IMAGE_TOP 50.0f
#define MS_PL_IMAGE_W 64.0f
#define MS_PL_IMAGE_H 112.0f
#define MS_PL_EYE_Z (-400.0f)
#define MS_PL_TAN_HALF_FOVY 0.57735026f // tan(60 / 2 degrees)
// OoT KaleidoScope_DrawPlayerWork: rot.y 32300 for its camera behind the origin; the page's camera is in front of the
// page, so turned by half a turn: 32300 + 0x8000.
#define MS_PL_ROT_Y ((s16)(32300 + 0x8000))

// XLU parts of the player are not drawn on the page (they would draw after the pause screen's OPA list, with its state).
static Gfx sXluDiscard[0x800];
static Player sSavedPlayer;
static s32 sLastLogKey = -1;

// Ocarina of Time's framing (OoT KaleidoScope_DrawPlayerWork): the child's, the adult's with the Master Sword, and
// the adult's otherwise (the long-sword pose).
typedef struct {
    f32 x, y, z, scale;
} MsPauseLinkFraming;

static const MsPauseLinkFraming sFramingChild = { 2.0f, -130.0f, -150.0f, 0.046f };
static const MsPauseLinkFraming sFramingAdultMaster = { 20.0f, -180.0f, -40.0f, 0.047f };
static const MsPauseLinkFraming sFramingAdultLong = { 25.0f, -228.0f, 60.0f, 0.056f };

static const MsPauseLinkFraming* MsPL_Framing(Player* player) {
    switch (player->transformation) {
        case PLAYER_FORM_DEKU:
            return &sFramingChild; // SoH (soh_fd): the Deku as the child
        case PLAYER_FORM_GORON:
        case PLAYER_FORM_ZORA:
            return &sFramingAdultMaster; // SoH: the Goron and the Zora, who hold no sword, as with the Master Sword
        case PLAYER_FORM_FIERCE_DEITY:
            return &sFramingAdultLong; // SoH: the Fierce Deity as the adult without the Master Sword
        default:
            break;
    }
    if (MergeShip_AdultLink_IsActive(player)) {
        // Ocarina of Time's adult: the long-sword framing only with Biggoron's Sword / the Giant's Knife on B.
        if (MergeShip_OoTEquip_Enabled() &&
            (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.sword == MS_OOTEQ_SWORD_LONG)) {
            return &sFramingAdultLong;
        }
        return &sFramingAdultMaster;
    }
    return &sFramingChild; // MM's young human is Ocarina of Time's child's size
}

static s32 MsPL_Enabled(void) {
    return CVarGetInteger(MERGESHIP_PAUSE_LINK_CVAR, 1) != 0;
}

// The preview's player: standing still, nothing in hand (the sword sheathed, the shield on the back, as Ocarina of Time's
// page shows him), the shield in use now, frame 0 of his idle animation. Changes only the player struct, which the caller
// restores.
static void MsPL_Pose(PlayState* play, Player* player) {
    PlayerAnimationHeader* idle;

    player->stateFlags1 &= ~(PLAYER_STATE1_400000 | PLAYER_STATE1_ZORA_BOOMERANG_THROWN);
    player->stateFlags3 &= ~(PLAYER_STATE3_2000 | PLAYER_STATE3_8000);
    player->actor.speed = 0.0f;
    player->meleeWeaponState = 0;
    player->unk_AAA = 0;
    player->unk_AB8 = 0.0f;
    player->unk_B62 = 0;
    player->headLimbRot.x = player->headLimbRot.y = player->headLimbRot.z = 0;
    player->upperLimbRot.x = player->upperLimbRot.y = player->upperLimbRot.z = 0;
    player->skelAnime.movementFlags = 0;

    if (player->transformation == PLAYER_FORM_HUMAN) {
        // Ocarina of Time's page shows Link without a mask and with nothing in hand.
        player->currentMask = PLAYER_MASK_NONE;
        player->itemAction = PLAYER_IA_NONE;
        player->heldItemAction = PLAYER_IA_NONE;
        // Player_SetEquipmentData's shield (the Termina view's choice, or Ocarina of Time's through MergeShipOoTEquip).
        player->currentShield = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
        MergeShip_OoTEquip_ApplyToPlayer(player);
    } else if (player->transformation != PLAYER_FORM_FIERCE_DEITY) {
        // The Deku, Goron and Zora hold nothing; the Fierce Deity keeps his sword in hand, as MM always shows him.
        player->itemAction = PLAYER_IA_NONE;
        player->heldItemAction = PLAYER_IA_NONE;
    }
    Player_SetModelGroup(player, Player_ActionToModelGroup(player, player->heldItemAction));

    // MM's own frame load (the adult's Ocarina of Time animation through its VB hook, MergeShipAdultLink.cpp); the
    // player's pose now stays if the load can't be made.
    idle = Player_GetIdleAnim(player);
    if (idle != NULL) {
        AnimTaskQueue_AddLoadPlayerFrame(play, idle, 0, player->skelAnime.limbCount, player->skelAnime.jointTable);
    }
}

void MergeShip_PauseLink_Draw(PlayState* play, const char* view) {
    static Lights1 sLights = gdSPDefLights1(80, 80, 80, 255, 255, 255, 84, 84, -84); // OoT Player_DrawPauseImpl
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    PauseContext* pauseCtx = &play->pauseCtx;
    Player* player = GET_PLAYER(play);
    const MsPauseLinkFraming* framing;
    Gfx* xluSaved;
    f32 k;
    f32 x;
    f32 y;
    f32 scale;
    s32 key;

    if (!MsPL_Enabled() || (player == NULL) || (player->actor.id != ACTOR_PLAYER) ||
        (player->actor.update == NULL) || (player->skelAnime.skeleton == NULL) ||
        (player->skelAnime.jointTable == NULL) || (player->skelAnime.limbCount == 0) ||
        (player->skelAnime.limbCount > PLAYER_LIMB_MAX) || (player->transformation >= PLAYER_FORM_MAX) ||
        (pauseCtx->alpha == 0)) {
        return;
    }

    framing = MsPL_Framing(player);
    // Ocarina of Time's image, converted to page units: at the framing's depth the image's 112 units span
    // 2 tan(30) (z - eye) of its camera's world; its centre is the image's centre.
    k = MS_PL_IMAGE_H / (2.0f * MS_PL_TAN_HALF_FOVY * (framing->z - MS_PL_EYE_Z));
    // (Its camera looks along +z, so its world x runs to the image's left; the page's camera looks along -z.)
    x = (MS_PL_IMAGE_LEFT + MS_PL_IMAGE_W / 2.0f) - framing->x * k;
    y = (MS_PL_IMAGE_TOP - MS_PL_IMAGE_H / 2.0f) + framing->y * k + pauseCtx->offsetY;
    scale = framing->scale * k;
    // (BUG-0133: Ganondorf is drawn at his own size -- 1.3 adult Links tall at either age, Beast Ganon more -- in a
    // frame made for Link: shrunk to Link's height there)
    scale *= MergeShip_Ganondorf_PauseScale(player);

    key = (player->transformation << 12) | (MergeShip_AdultLink_IsActive(player) ? 0x800 : 0) |
          (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) << 8) | (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) << 6) |
          ((gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.sword & 7) << 3) |
          (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.shield & 7);
    if (key != sLastLogKey) {
        sLastLogKey = key;
        MergeShipTest_Log("PAUSELINK %s form=%d adult=%d sword=%d shield=%d ootSword=%d ootShield=%d x=%.1f y=%.1f "
                          "scale=%.5f",
                          view, player->transformation, MergeShip_AdultLink_IsActive(player),
                          GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD),
                          gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.sword,
                          gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.shield, x, y, scale);
    }

    memcpy(&sSavedPlayer, player, sizeof(Player));
    MsPL_Pose(play, player);

    OPEN_DISPS(gfxCtx);

    xluSaved = POLY_XLU_DISP;
    POLY_XLU_DISP = sXluDiscard;

    gDPPipeSync(POLY_OPA_DISP++);
    Gfx_SetupDL25_Opa(gfxCtx);
    POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP, 0, 0, 0, 0, 997, 1000); // OoT Player_DrawPauseImpl: no fog
    gSPSetLights1(POLY_OPA_DISP++, sLights);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
    // Anchor's colours: the form lists read their recoloured palettes through segment 0x07 while Anchor colours the
    // players (MergeShipAnchorColors.h); the local player's, as his actor draw sets them.
    MergeShip_AnchorColors_SetForPlayer(play, &player->actor);

    // The page's matrix is the current one (the Quest face's, loaded by KaleidoScope_Draw's page draw).
    Matrix_Push();
    Matrix_Translate(x, y, 0.0f, MTXMODE_APPLY);
    Matrix_RotateYS(MS_PL_ROT_Y, MTXMODE_APPLY);
    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);

    Player_DrawImpl(play, player->skelAnime.skeleton, player->skelAnime.jointTable, player->skelAnime.dListCount, 0,
                    player->transformation, 0, player->actor.shape.face, Player_OverrideLimbDrawGameplayDefault,
                    Player_PostLimbDrawGameplay, &player->actor);

    Matrix_Pop();

    // Back to the page: its matrix, the pause screen's segments (KaleidoScope_Draw) and its render state.
    gDPPipeSync(POLY_OPA_DISP++);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, (uintptr_t)pauseCtx->iconItemSegment);
    gSPSegment(POLY_OPA_DISP++, 0x09, (uintptr_t)pauseCtx->iconItem24Segment);
    gSPSegment(POLY_OPA_DISP++, 0x0A, (uintptr_t)pauseCtx->nameSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0B, (uintptr_t)pauseCtx->iconItemVtxSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)pauseCtx->iconItemAltSegment);
    gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)pauseCtx->iconItemLangSegment);
    Gfx_SetupDL42_Opa(gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    POLY_XLU_DISP = xluSaved;

    CLOSE_DISPS(gfxCtx);

    memcpy(player, &sSavedPlayer, sizeof(Player));
}
