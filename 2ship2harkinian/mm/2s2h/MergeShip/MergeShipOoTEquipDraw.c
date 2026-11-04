// MergeShip2 -- Ocarina of Time's equipment drawn on Majora's Mask's human, and its get-item models. See
// MergeShipOoTEquip.h.
//
// MM's human is Ocarina of Time's child skeleton (same joints, same flex matrix order, and MM's closed fists are the
// child's fists vertex for vertex), so the child's own lists sit on it as modelled: the Deku Shield (in the fist and on
// the back), the Hylian Shield on the back, the Goron's Bracelet. Ocarina of Time has the rest only for the adult: the
// Master Sword, Biggoron's Sword and the Giant's Knife in hand, the Hylian and Mirror Shields in hand, the Mirror Shield on
// the back, the Master Sword's scabbard, the Iron and Hover Boots and the gauntlet plates. Those are fitted to the child
// the way SoH fits the adult's equipment on its child (ScaleAdultEquipmentAsChild: 0.8 in the hands; the scabbard
// translated 218,-100,62 and scaled 0.8), the boots and plates from the vertex extents of the adult's and the child's
// feet and forearms. The adult's lists hold the item in the adult's gloved fist: that fist is dropped
// (MergeShip_OoTEquip_RewriteDL) and MM's bare fist drawn instead. Every fit can be tuned with
// gMergeShip.OoTEquipFit.<HandL|HandR|Sheath|FootL|FootR|Arm>.<X|Y|Z|SX|SY|SZ>.

#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "global.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTEquipAssets.h"
#include "2s2h/MergeShip/MergeShipAdultLink.h" // MergeShip2: the adult draws his own (MergeShipAdultLink.c)
#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip_Own_ButtonBowIsOoT (BUG-0015)
#include "objects/object_link_child/object_link_child.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <stdio.h>
#include <string.h>

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

// ---- Ocarina of Time's lists ------------------------------------------------------------------------------------------
#define BOY(name) MS_OOTEQ_OBJ("object_link_boy", name)
#define CHILD(name) MS_OOTEQ_OBJ("object_link_child", name)

static const ALIGN_ASSET(2) char sAdultHandMasterSwordDL[] = BOY("gLinkAdultLeftHandHoldingMasterSwordNearDL");
static const ALIGN_ASSET(2) char sAdultHandBgsDL[] = BOY("gLinkAdultLeftHandHoldingBgsNearDL");
static const ALIGN_ASSET(2) char sAdultHandBrokenKnifeDL[] = BOY("gLinkAdultHandHoldingBrokenGiantsKnifeDL");
static const ALIGN_ASSET(2) char sAdultHandHylianShieldDL[] = BOY("gLinkAdultRightHandHoldingHylianShieldNearDL");
static const ALIGN_ASSET(2) char sAdultHandMirrorShieldDL[] = BOY("gLinkAdultRightHandHoldingMirrorShieldNearDL");
static const ALIGN_ASSET(2) char sAdultHandBowDL[] = BOY("gLinkAdultRightHandHoldingBowNearDL");
static const ALIGN_ASSET(2) char sAdultHandBowFirstPersonDL[] = BOY("gLinkAdultRightHandHoldingBowFirstPersonDL");
static const ALIGN_ASSET(2) char sAdultMasterSwordAndSheathDL[] = BOY("gLinkAdultMasterSwordAndSheathNearDL");
static const ALIGN_ASSET(2) char sAdultSheathDL[] = BOY("gLinkAdultSheathNearDL");
static const ALIGN_ASSET(2) char sAdultMirrorShieldAndSheathDL[] = BOY("gLinkAdultMirrorShieldAndSheathNearDL");
static const ALIGN_ASSET(2) char sAdultLeftIronBootDL[] = BOY("gLinkAdultLeftIronBootDL");
static const ALIGN_ASSET(2) char sAdultRightIronBootDL[] = BOY("gLinkAdultRightIronBootDL");
static const ALIGN_ASSET(2) char sAdultLeftHoverBootDL[] = BOY("gLinkAdultLeftHoverBootDL");
static const ALIGN_ASSET(2) char sAdultRightHoverBootDL[] = BOY("gLinkAdultRightHoverBootDL");
static const ALIGN_ASSET(2) char sAdultLeftGauntletPlate1DL[] = BOY("gLinkAdultLeftGauntletPlate1DL");
static const ALIGN_ASSET(2) char sAdultLeftGauntletPlate2DL[] = BOY("gLinkAdultLeftGauntletPlate2DL");
static const ALIGN_ASSET(2) char sAdultLeftGauntletPlate3DL[] = BOY("gLinkAdultLeftGauntletPlate3DL");
static const ALIGN_ASSET(2) char sAdultRightGauntletPlate1DL[] = BOY("gLinkAdultRightGauntletPlate1DL");
static const ALIGN_ASSET(2) char sAdultRightGauntletPlate2DL[] = BOY("gLinkAdultRightGauntletPlate2DL");
static const ALIGN_ASSET(2) char sAdultRightGauntletPlate3DL[] = BOY("gLinkAdultRightGauntletPlate3DL");
static const ALIGN_ASSET(2) char sChildFistAndDekuShieldDL[] = CHILD("gLinkChildRightFistAndDekuShieldNearDL");
// Ocarina of Time's Kokiri Sword: the child's own fist holding it, and the child's scabbard with it or empty (MM's human
// is Ocarina of Time's child skeleton: they sit as modelled).
static const ALIGN_ASSET(2) char sChildFistAndKokiriSwordDL[] = CHILD("gLinkChildLeftFistAndKokiriSwordNearDL");
static const ALIGN_ASSET(2) char sChildSwordAndSheathDL[] = CHILD("gLinkChildSwordAndSheathNearDL");
static const ALIGN_ASSET(2) char sChildSheathDL[] = CHILD("gLinkChildSheathNearDL");
static const ALIGN_ASSET(2) char sChildDekuShieldOnBackDL[] = CHILD("gLinkChildDekuShieldWithMatrixDL");
static const ALIGN_ASSET(2) char sChildHylianShieldAndSheathDL[] = CHILD("gLinkChildHylianShieldAndSheathNearDL");
static const ALIGN_ASSET(2) char sChildGoronBraceletDL[] = CHILD("gLinkChildGoronBraceletDL");
static const ALIGN_ASSET(2) char sHoverBootsCircleDL[] = MS_OOTEQ_OBJ("gameplay_keep", "gHoverBootsCircleDL");

// ---- fits -------------------------------------------------------------------------------------------------------------
typedef enum {
    MSEQ_FIT_HAND_L,
    MSEQ_FIT_HAND_R,
    MSEQ_FIT_SHEATH,
    MSEQ_FIT_FOOT,
    MSEQ_FIT_FOOT_R,
    MSEQ_FIT_ARM,
    MSEQ_FIT_NONE,
    MSEQ_FIT_MAX
} MsEqFitId;

typedef struct {
    const char* name;
    f32 v[6]; // x, y, z, sx, sy, sz
} MsEqFit;

static const MsEqFit sFits[MSEQ_FIT_MAX] = {
    // SoH ScaleAdultEquipmentAsChild: the adult's sword / Biggoron's Sword / Mirror Shield in the child's hand at 0.8.
    { "HandL", { 0.0f, 0.0f, 0.0f, 0.8f, 0.8f, 0.8f } },
    { "HandR", { 0.0f, 0.0f, 0.0f, 0.8f, 0.8f, 0.8f } },
    // SoH: the adult's scabbard (and shield on it) on the child: translate 218,-100,62, scale 0.8.
    { "Sheath", { 218.0f, -100.0f, 62.0f, 0.8f, 0.8f, 0.8f } },
    // The adult's boots on MM's young Link's feet, in the foot limb's space (MM gLinkHuman*FootDL against OoT
    // gLinkAdult*FootNearDL): length 879 / 1304 (0.674, also up the leg, so the boot keeps its shape), width 598 / 637;
    // the soles meet (the iron boot's at 550 * 0.674 - 56 = MM's 315) and the boot is centered on the foot, which MM
    // set about 70 to the outside on each foot (left z center 59, right -74; the adult's -16 / -6).
    { "FootL", { 19.0f, -56.0f, 74.0f, 0.674f, 0.674f, 0.94f } },
    { "FootR", { 19.0f, -56.0f, -68.0f, 0.674f, 0.674f, 0.94f } },
    // The adult's fist is 679 long against the child's 499.
    { "Arm", { 0.0f, 0.0f, 0.0f, 0.74f, 0.74f, 0.74f } },
    { "None", { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f } },
};

static void MsEq_GetFit(s32 fit, f32* out) {
    static const char* sAxes[6] = { "X", "Y", "Z", "SX", "SY", "SZ" };
    char cvar[64];
    s32 i;

    for (i = 0; i < 6; i++) {
        snprintf(cvar, sizeof(cvar), "gMergeShip.OoTEquipFit.%s.%s", sFits[fit].name, sAxes[i]);
        out[i] = CVarGetFloat(cvar, sFits[fit].v[i]);
    }
}

// The fit on its own (MTXMODE_NEW), for a list multiplied onto the limb's matrix.
// A constant translate-then-scale matrix (as Matrix_Translate(NEW) + Matrix_Scale(APPLY): the scale on the diagonal,
// the translation in the last row), built directly (guMtxF2L), not through the matrix stack. BUG-0138/0141 (at the
// player's refresh rate, "quick/instant-turning flickers an oot item on player model, eg hylian shield", young Link
// and the adult): these run inside limb callbacks, where every Matrix_* call is recorded for frame interpolation and
// matched to the last frame's by its place -- when the pieces drawn changed between frames (a fast turn, the far
// model) a piece's fit was paired with another's and the item jumped for a frame. A constant has nothing to
// interpolate (MergeShip_AdultLink_FitMtx; Hyrule's BUG-0041).
static Mtx* MsEq_ConstMtx(PlayState* play, f32 tx, f32 ty, f32 tz, f32 sx, f32 sy, f32 sz) {
    float mf[4][4] = {
        { sx, 0.0f, 0.0f, 0.0f },
        { 0.0f, sy, 0.0f, 0.0f },
        { 0.0f, 0.0f, sz, 0.0f },
        { tx, ty, tz, 1.0f },
    };
    Mtx* mtx = GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx));

    guMtxF2L(mf, mtx);
    return mtx;
}

static Mtx* MsEq_FitMtx(PlayState* play, s32 fit) {
    f32 f[6];

    MsEq_GetFit(fit, f);
    return MsEq_ConstMtx(play, f[0], f[1], f[2], f[3], f[4], f[5]);
}

// The limb's own matrix times the fit, for a list that loads a limb matrix itself (segment 0x0D, remapped to 0x0E).
static Mtx* MsEq_LimbFitMtx(PlayState* play, s32 fit) {
    f32 f[6];
    Mtx* mtx;

    MsEq_GetFit(fit, f);
    Matrix_Push();
    Matrix_Translate(f[0], f[1], f[2], MTXMODE_APPLY);
    Matrix_Scale(f[3], f[4], f[5], MTXMODE_APPLY);
    mtx = Matrix_Finalize(play->state.gfxCtx);
    Matrix_Pop();
    return mtx;
}

// MM's `base` (a bare fist, or nothing) followed by Ocarina of Time's `item` under a fit.
static Gfx* MsEq_Compose(PlayState* play, const char* base, const char* item, u32 flags, s32 fit) {
    Gfx* start = GRAPH_ALLOC(play->state.gfxCtx, 8 * sizeof(Gfx));
    Gfx* p = start;

    if (base != NULL) {
        gSPDisplayList(p++, base);
    }
    gSPMatrix(p++, MsEq_FitMtx(play, fit), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPDisplayList(p++, MergeShip_OoTEquip_RewriteDL(item, flags));
    gSPPopMatrix(p++, G_MTX_MODELVIEW);
    gSPEndDisplayList(p++);
    return start;
}

static s32 MsEq_IsPath(Gfx* dList, const char* part1, const char* part2) {
    const char* path = (const char*)dList;

    if ((dList == NULL) || (strncmp(path, "__OTR__", 7) != 0)) {
        return false;
    }
    return (strstr(path, part1) != NULL) && ((part2 == NULL) || (strstr(path, part2) != NULL));
}

static s32 MsEq_IsHuman(Player* player) {
    return MergeShip_OoTEquip_Enabled() && (player->actor.id == ACTOR_PLAYER) &&
           (player->transformation == PLAYER_FORM_HUMAN);
}

// ---- in hand and on the back --------------------------------------------------------------------------------------------

static u32 sHylianHunchFrame = 0xFFFFFFF0u;

void MergeShip_OoTEquip_NoteHylianHunch(void) {
    if (gPlayState != NULL) {
        sHylianHunchFrame = gPlayState->gameplayFrames;
    }
}

s32 MergeShip_OoTEquip_HylianHunch(Player* player) {
    return (gPlayState != NULL) && MsEq_IsHuman(player) && !MergeShip_AdultLink_IsActive(player) &&
           (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.shield == MS_OOTEQ_SHIELD_HYLIAN) &&
           ((u32)(gPlayState->gameplayFrames - sHylianHunchFrame) <= 1);
}

Gfx* MergeShip_OoTEquip_FairyBowFirstPersonDL(PlayState* play) {
    return MsEq_Compose(play, gLinkHumanRightHandClosedDL, sAdultHandBowFirstPersonDL, MS_OOTEQ_DL_STRIP_HAND,
                        MSEQ_FIT_HAND_R);
}

// The held bow's button is Hyrule's Fairy Bow's (the Items (Hyrule) bow cell), not MM's Hero's Bow.
static s32 MsEq_HeldBowIsOoT(Player* player) {
    u8 btn = player->heldItemButton;
    u8 slot;

    if (IS_HELD_DPAD(btn)) {
        slot = DPAD_GET_CUR_FORM_BTN_SLOT(HELD_ITEM_TO_DPAD(btn));
    } else if ((btn >= EQUIP_SLOT_C_LEFT) && (btn <= EQUIP_SLOT_C_RIGHT)) {
        slot = GET_CUR_FORM_BTN_SLOT(btn);
    } else {
        return false;
    }
    return MergeShip_Own_ButtonBowIsOoT(slot);
}

s32 MergeShip_OoTEquip_SwordInHand(Player* player) {
    return (player->leftHandType == PLAYER_MODELTYPE_LH_ONE_HAND_SWORD) ||
           (player->leftHandType == PLAYER_MODELTYPE_LH_TWO_HAND_SWORD);
}

s32 MergeShip_OoTEquip_HoldsFairyBow(Player* player) {
    if (!MsEq_IsHuman(player) || MergeShip_AdultLink_IsActive(player)) {
        return false;
    }
    return MsEq_HeldBowIsOoT(player);
}

// BUG-0059: the adult holding MM's Hero's Bow (his own rows hold Ocarina of Time's bow, the Fairy Bow).
s32 MergeShip_OoTEquip_AdultHoldsHeroBow(Player* player) {
    return MergeShip_AdultLink_IsActive(player) && (player->heldItemAction >= PLAYER_IA_BOW) &&
           (player->heldItemAction <= PLAYER_IA_BOW_LIGHT) && !MsEq_HeldBowIsOoT(player);
}

void MergeShip_OoTEquip_OverrideLimb(PlayState* play, Player* player, s32 limbIndex, Gfx** dList) {
    if (!MsEq_IsHuman(player)) {
        return;
    }

    if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        u8 held = player->heldItemId;

        // Only where MM chose a hand holding its sword (not an open / closed hand while running or climbing).
        if (!MsEq_IsPath(*dList, "LeftHandHolding", "Sword")) {
            return;
        }
        // While the sword is drawn or put away the held item is briefly not the sword, and MM's hand for a two-handed
        // sword is the Great Fairy's: the B button's Ocarina of Time sword is the one in the hand then (BUG-0023, the
        // Biggoron's Sword flashing to the Great Fairy's Sword).
        // (The Great Fairy's Sword really held, from a C button, stays itself.)
        if (!ITEM_IS_OOT_EQUIP_SWORD(held) && (held != ITEM_SWORD_GREAT_FAIRY)) {
            held = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
        }
        if (!ITEM_IS_OOT_EQUIP_SWORD(held)) {
            return;
        }
        if (held == ITEM_OOT_SWORD_MASTER) {
            *dList = MsEq_Compose(play, gLinkHumanLeftHandClosedDL, sAdultHandMasterSwordDL, MS_OOTEQ_DL_STRIP_HAND,
                                  MSEQ_FIT_HAND_L);
        } else if (held == ITEM_OOT_SWORD_KOKIRI) {
            *dList = (Gfx*)sChildFistAndKokiriSwordDL; // the child's own fist and sword
        } else if (MergeShip_OoTEquip_HoldsBrokenKnife(player) || (held == ITEM_OOT_SWORD_BROKEN)) {
            *dList = MsEq_Compose(play, gLinkHumanLeftHandClosedDL, sAdultHandBrokenKnifeDL, MS_OOTEQ_DL_STRIP_HAND,
                                  MSEQ_FIT_HAND_L);
        } else {
            *dList = MsEq_Compose(play, gLinkHumanLeftHandClosedDL, sAdultHandBgsDL, MS_OOTEQ_DL_STRIP_HAND,
                                  MSEQ_FIT_HAND_L);
        }
    } else if (limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        // BUG-0015: Hyrule's Fairy Bow, not MM's Hero's Bow (its string: MergeShip_OoTItems_DrawSlingshotString).
        if (MsEq_IsPath(*dList, "RightHandHolding", "Bow") && MergeShip_OoTEquip_HoldsFairyBow(player)) {
            *dList = MsEq_Compose(play, gLinkHumanRightHandClosedDL, sAdultHandBowDL, MS_OOTEQ_DL_STRIP_HAND,
                                  MSEQ_FIT_HAND_R);
            return;
        }
        if (!MsEq_IsPath(*dList, "RightHandHolding", "Shield")) {
            return;
        }
        // BUG-0022: hunched under the Hylian Shield on his back, the hand is empty.
        if (MergeShip_OoTEquip_HylianHunch(player)) {
            *dList = gLinkHumanRightHandClosedDL;
            return;
        }
        switch (OOT_EQUIP.shield) {
            case MS_OOTEQ_SHIELD_DEKU:
                *dList = (Gfx*)sChildFistAndDekuShieldDL; // the child's own fist and shield
                break;
            case MS_OOTEQ_SHIELD_HYLIAN:
                *dList = MsEq_Compose(play, gLinkHumanRightHandClosedDL, sAdultHandHylianShieldDL,
                                      MS_OOTEQ_DL_STRIP_HAND, MSEQ_FIT_HAND_R);
                break;
            case MS_OOTEQ_SHIELD_MIRROR:
                *dList = MsEq_Compose(play, gLinkHumanRightHandClosedDL, sAdultHandMirrorShieldDL,
                                      MS_OOTEQ_DL_STRIP_HAND, MSEQ_FIT_HAND_R);
                break;
            default:
                break;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        u8 bItem = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);

        if (OOT_EQUIP.sword == MS_OOTEQ_SWORD_MASTER && (bItem == ITEM_OOT_SWORD_MASTER)) {
            // OoT sSheathWithSwordDLs / sSheathWithoutSwordDLs: the sword in its scabbard, or the empty scabbard.
            s32 sheathed = (player->sheathType == PLAYER_MODELTYPE_SHEATH_14) ||
                           (player->sheathType == PLAYER_MODELTYPE_SHEATH_12);

            *dList = MsEq_Compose(play, NULL, sheathed ? sAdultMasterSwordAndSheathDL : sAdultSheathDL, 0,
                                  MSEQ_FIT_SHEATH);
        } else if ((OOT_EQUIP.sword == MS_OOTEQ_SWORD_KOKIRI) && (bItem == ITEM_OOT_SWORD_KOKIRI)) {
            // OoT's child scabbard, the Kokiri Sword in it or not.
            s32 sheathed = (player->sheathType == PLAYER_MODELTYPE_SHEATH_14) ||
                           (player->sheathType == PLAYER_MODELTYPE_SHEATH_12);

            *dList = (Gfx*)(sheathed ? sChildSwordAndSheathDL : sChildSheathDL);
        } else if ((OOT_EQUIP.sword == MS_OOTEQ_SWORD_LONG) && ITEM_IS_OOT_EQUIP_SWORD(bItem)) {
            // OoT: Biggoron's Sword has no scabbard of its own; the Master Sword's stays on Link's back, empty.
            // BUG-0021: put away, the sword is worn in it (MERGESHIP_BGS_ON_BACK_*), under the same fit.
            s32 sheathed = (player->sheathType == PLAYER_MODELTYPE_SHEATH_14) ||
                           (player->sheathType == PLAYER_MODELTYPE_SHEATH_12);

            // BUG-0062: not while it is in the hand (MM keeps the sword "sheathed" holding a two-handed one).
            if (sheathed && !MergeShip_OoTEquip_SwordInHand(player) && CVarGetInteger(MERGESHIP_BGS_ON_BACK_CVAR, 1)) {
                Gfx* start = GRAPH_ALLOC(play->state.gfxCtx, 8 * sizeof(Gfx));
                Gfx* p = start;
                Mtx* back;

                back = MsEq_ConstMtx(play, MERGESHIP_BGS_ON_BACK_X, MERGESHIP_BGS_ON_BACK_Y, MERGESHIP_BGS_ON_BACK_Z,
                                     1.0f, 1.0f, 1.0f); // (BUG-0138: a constant, off the recorded stack)
                // The sword alone, without the Master Sword's scabbard (the user, 2026-10-07).
                gSPMatrix(p++, MsEq_FitMtx(play, MSEQ_FIT_SHEATH), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
                gSPMatrix(p++, back, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
                gSPDisplayList(p++, MergeShip_OoTEquip_RewriteDL(sAdultHandBgsDL, MS_OOTEQ_DL_STRIP_HAND));
                gSPPopMatrix(p++, G_MTX_MODELVIEW);
                gSPPopMatrix(p++, G_MTX_MODELVIEW);
                gSPEndDisplayList(p++);
                *dList = start;
            } else {
                *dList = MsEq_Compose(play, NULL, sAdultSheathDL, 0, MSEQ_FIT_SHEATH);
            }
        }
    }
}

Gfx* MergeShip_OoTEquip_BackShieldDL(PlayState* play, Player* player, Gfx* dList) {
    if (!MsEq_IsHuman(player)) {
        return dList;
    }
    switch (OOT_EQUIP.shield) {
        case MS_OOTEQ_SHIELD_DEKU:
            return (Gfx*)sChildDekuShieldOnBackDL;
        case MS_OOTEQ_SHIELD_HYLIAN:
            // The child's own Hylian Shield on the back, without its Kokiri scabbard (MM draws its own).
            return MsEq_Compose(play, NULL, sChildHylianShieldAndSheathDL, MS_OOTEQ_DL_STRIP_SHEATH, MSEQ_FIT_NONE);
        case MS_OOTEQ_SHIELD_MIRROR:
            return MsEq_Compose(play, NULL, sAdultMirrorShieldAndSheathDL, MS_OOTEQ_DL_STRIP_SHEATH, MSEQ_FIT_SHEATH);
        default:
            return dList;
    }
}

// ---- boots, gauntlets and bracelet ------------------------------------------------------------------------------------

// A list that loads its limb's matrix from segment 0x0D (OoT Player_DrawImpl draws these after the skeleton): drawn on
// this limb with the fit, through segment 0x0E.
static void MsEq_DrawOnLimb(PlayState* play, const char* dl, s32 fit, const Color_RGB8* env) {
    Mtx* fitMtx;

    // BUG-0051/0054: the fit's matrix operations are their own interpolation child, keyed by the list: drawn on some
    // frames only (a plate that changes with the hand, the hookshot's aim), they shifted every recorded operation
    // after them, and the plate was paired with another's (the hookshot's chain) -- it grew and flew to the target.
    FrameInterpolation_RecordOpenChild(dl, fit);
    fitMtx = MsEq_LimbFitMtx(play, fit);
    FrameInterpolation_RecordCloseChild();

    OPEN_DISPS(play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x0E, fitMtx);
    if (env != NULL) {
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetEnvColor(POLY_OPA_DISP++, env->r, env->g, env->b, 0);
    }
    gSPDisplayList(POLY_OPA_DISP++, MergeShip_OoTEquip_RewriteDL(dl, MS_OOTEQ_DL_REMAP_0D));
    // Segment 0x0E is MM's master display list (graph.c); later draws in this frame call into it.
    gSPSegment(POLY_OPA_DISP++, 0x0E, gGfxMasterDL);

    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_OoTEquip_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex) {
    // OoT sGauntletColors (silver, golden).
    static const Color_RGB8 sGauntletColors[] = { { 255, 255, 255 }, { 254, 207, 15 } };
    const Color_RGB8* gauntletColor;

    // Ocarina of Time's adult wears the boots and plates unfitted (MergeShip_AdultLink_PostLimbDraw).
    if (!MsEq_IsHuman(player) || (player->actor.scale.y < 0.0f) || MergeShip_AdultLink_IsActive(player)) {
        return;
    }
    // BUG-0054: in first person (the hookshot's or bow's aim) the arms are not drawn: nor are their plates (they sat in
    // front of the camera).
    if ((player->stateFlags1 & PLAYER_STATE1_100000) &&
        ((limbIndex == PLAYER_LIMB_LEFT_FOREARM) || (limbIndex == PLAYER_LIMB_LEFT_HAND) ||
         (limbIndex == PLAYER_LIMB_RIGHT_FOREARM) || (limbIndex == PLAYER_LIMB_RIGHT_HAND))) {
        return;
    }
    gauntletColor = (OOT_EQUIP.strength >= 3) ? &sGauntletColors[1] : &sGauntletColors[0];

    switch (limbIndex) {
        case PLAYER_LIMB_LEFT_FOOT:
            if ((player->currentBoots == PLAYER_BOOTS_IRON) || (player->currentBoots == PLAYER_BOOTS_HOVER)) {
                MsEq_DrawOnLimb(play,
                                (player->currentBoots == PLAYER_BOOTS_IRON) ? sAdultLeftIronBootDL
                                                                            : sAdultLeftHoverBootDL,
                                MSEQ_FIT_FOOT, NULL);
            }
            break;
        case PLAYER_LIMB_RIGHT_FOOT:
            if ((player->currentBoots == PLAYER_BOOTS_IRON) || (player->currentBoots == PLAYER_BOOTS_HOVER)) {
                MsEq_DrawOnLimb(play,
                                (player->currentBoots == PLAYER_BOOTS_IRON) ? sAdultRightIronBootDL
                                                                            : sAdultRightHoverBootDL,
                                MSEQ_FIT_FOOT_R, NULL);
            }
            break;
        case PLAYER_LIMB_LEFT_FOREARM:
            if (OOT_EQUIP.strength == 1) {
                // OoT: the child wears the Goron's Bracelet on the left forearm.
                MsEq_DrawOnLimb(play, sChildGoronBraceletDL, MSEQ_FIT_NONE, NULL);
            } else if (OOT_EQUIP.strength >= 2) {
                MsEq_DrawOnLimb(play, sAdultLeftGauntletPlate1DL, MSEQ_FIT_ARM, gauntletColor);
            }
            break;
        case PLAYER_LIMB_LEFT_HAND:
            if (OOT_EQUIP.strength >= 2) {
                MsEq_DrawOnLimb(play,
                                (player->leftHandType == PLAYER_MODELTYPE_LH_OPEN) ? sAdultLeftGauntletPlate2DL
                                                                                   : sAdultLeftGauntletPlate3DL,
                                MSEQ_FIT_ARM, gauntletColor);
            }
            break;
        case PLAYER_LIMB_RIGHT_FOREARM:
            if (OOT_EQUIP.strength >= 2) {
                MsEq_DrawOnLimb(play, sAdultRightGauntletPlate1DL, MSEQ_FIT_ARM, gauntletColor);
            }
            break;
        case PLAYER_LIMB_RIGHT_HAND:
            if (OOT_EQUIP.strength >= 2) {
                MsEq_DrawOnLimb(play,
                                (player->rightHandType == PLAYER_MODELTYPE_RH_OPEN) ? sAdultRightGauntletPlate2DL
                                                                                    : sAdultRightGauntletPlate3DL,
                                MSEQ_FIT_ARM, gauntletColor);
            }
            break;
        default:
            break;
    }
}

void MergeShip_OoTEquip_DrawHoverEffect(PlayState* play, Player* player) {
    // OoT Player_Draw: the Hover Boots' ring under Link while they hold him up.
    static Vec3s sZeroRot = { 0, 0, 0 };
    s32 timer = MergeShip_OoTEquip_HoverTimer();
    f32 alpha = 0.0f;
    s32 t;

    if (!MsEq_IsHuman(player) || (player->currentBoots != PLAYER_BOOTS_HOVER) ||
        (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (timer == 0) || (timer >= 19)) {
        return;
    }
    if (timer >= 15) {
        alpha = (19 - timer) * 51.0f;
    } else {
        t = MIN(timer, 9);
        alpha = (-t * 4) + 36;
        alpha = alpha * alpha;
        alpha = (s32)((Math_CosS(alpha) * 100.0f) + 100.0f) + 55.0f;
        alpha = alpha * (t * (1.0f / 9.0f));
    }

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Push();
    Matrix_SetTranslateRotateYXZ(player->actor.world.pos.x, player->actor.world.pos.y + 2.0f,
                                 player->actor.world.pos.z, &sZeroRot);
    Matrix_Scale(4.0f, 4.0f, 4.0f, MTXMODE_APPLY);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 32, 1, 0, (play->gameplayFrames * -15) % 128, 16, 32));
    gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 255, (u8)alpha);
    gDPSetEnvColor(POLY_XLU_DISP++, 120, 90, 30, 128);
    gSPDisplayList(POLY_XLU_DISP++, sHoverBootsCircleDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- get-item models (OoT z_draw.c sDrawItemTable and its GetItem_Draw* functions) -----------------------------------------
#define GI(obj, name) MS_OOTEQ_OBJ(obj, name)
static const ALIGN_ASSET(2) char sGiMasterSwordDL[] = GI("object_toki_objects", "object_toki_objects_DL_001BD0");
static const ALIGN_ASSET(2) char sGiKokiriSwordDL[] = GI("object_gi_sword_1", "gGiKokiriSwordDL");
static const ALIGN_ASSET(2) char sGiBiggoronSwordDL[] = GI("object_gi_longsword", "gGiBiggoronSwordDL");
static const ALIGN_ASSET(2) char sGiDekuShieldDL[] = GI("object_gi_shield_1", "gGiDekuShieldDL");
static const ALIGN_ASSET(2) char sGiHylianShieldDL[] = GI("object_gi_shield_2", "gGiHylianShieldDL");
static const ALIGN_ASSET(2) char sGiMirrorShieldDL[] = GI("object_gi_shield_3", "gGiMirrorShieldDL");
static const ALIGN_ASSET(2) char sGiMirrorShieldSymbolDL[] = GI("object_gi_shield_3", "gGiMirrorShieldSymbolDL");
static const ALIGN_ASSET(2) char sGiTunicCollarDL[] = GI("object_gi_clothes", "gGiTunicCollarDL");
static const ALIGN_ASSET(2) char sGiGoronCollarColorDL[] = GI("object_gi_clothes", "gGiGoronCollarColorDL");
static const ALIGN_ASSET(2) char sGiGoronTunicColorDL[] = GI("object_gi_clothes", "gGiGoronTunicColorDL");
static const ALIGN_ASSET(2) char sGiZoraCollarColorDL[] = GI("object_gi_clothes", "gGiZoraCollarColorDL");
static const ALIGN_ASSET(2) char sGiZoraTunicColorDL[] = GI("object_gi_clothes", "gGiZoraTunicColorDL");
static const ALIGN_ASSET(2) char sGiTunicDL[] = GI("object_gi_clothes", "gGiTunicDL");
static const ALIGN_ASSET(2) char sGiIronBootsDL[] = GI("object_gi_boots_2", "gGiIronBootsDL");
static const ALIGN_ASSET(2) char sGiIronBootsRivetsDL[] = GI("object_gi_boots_2", "gGiIronBootsRivetsDL");
static const ALIGN_ASSET(2) char sGiHoverBootsDL[] = GI("object_gi_hoverboots", "gGiHoverBootsDL");
static const ALIGN_ASSET(2) char sGiGoronBraceletDL[] = GI("object_gi_bracelet", "gGiGoronBraceletDL");
static const ALIGN_ASSET(2) char sGiGauntletsDL[] = GI("object_gi_gloves", "gGiGauntletsDL");
static const ALIGN_ASSET(2) char sGiGauntletsPlateDL[] = GI("object_gi_gloves", "gGiGauntletsPlateDL");
static const ALIGN_ASSET(2) char sGiSilverGauntletsColorDL[] = GI("object_gi_gloves", "gGiSilverGauntletsColorDL");
static const ALIGN_ASSET(2) char sGiSilverGauntletsPlateColorDL[] = GI("object_gi_gloves", "gGiSilverGauntletsPlateColorDL");
static const ALIGN_ASSET(2) char sGiGoldenGauntletsColorDL[] = GI("object_gi_gloves", "gGiGoldenGauntletsColorDL");
static const ALIGN_ASSET(2) char sGiGoldenGauntletsPlateColorDL[] = GI("object_gi_gloves", "gGiGoldenGauntletsPlateColorDL");
static const ALIGN_ASSET(2) char sGiScaleWaterDL[] = GI("object_gi_scale", "gGiScaleWaterDL");
static const ALIGN_ASSET(2) char sGiScaleDL[] = GI("object_gi_scale", "gGiScaleDL");
static const ALIGN_ASSET(2) char sGiSilverScaleWaterColorDL[] = GI("object_gi_scale", "gGiSilverScaleWaterColorDL");
static const ALIGN_ASSET(2) char sGiSilverScaleColorDL[] = GI("object_gi_scale", "gGiSilverScaleColorDL");
static const ALIGN_ASSET(2) char sGiGoldenScaleWaterColorDL[] = GI("object_gi_scale", "gGiGoldenScaleWaterColorDL");
static const ALIGN_ASSET(2) char sGiGoldenScaleColorDL[] = GI("object_gi_scale", "gGiGoldenScaleColorDL");
static const ALIGN_ASSET(2) char sGiBulletBagDL[] = GI("object_gi_dekupouch", "gGiBulletBagDL");
static const ALIGN_ASSET(2) char sGiBulletBagColorDL[] = GI("object_gi_dekupouch", "gGiBulletBagColorDL");
static const ALIGN_ASSET(2) char sGiBulletBagStringDL[] = GI("object_gi_dekupouch", "gGiBulletBagStringDL");
static const ALIGN_ASSET(2) char sGiBulletBagStringColorDL[] = GI("object_gi_dekupouch", "gGiBulletBagStringColorDL");
static const ALIGN_ASSET(2) char sGiBulletBag50ColorDL[] = GI("object_gi_dekupouch", "gGiBulletBag50ColorDL");
static const ALIGN_ASSET(2) char sGiBulletBag50StringColorDL[] = GI("object_gi_dekupouch", "gGiBulletBag50StringColorDL");
static const ALIGN_ASSET(2) char sGiBulletBagWritingDL[] = GI("object_gi_dekupouch", "gGiBulletBagWritingDL");

static void MsEq_GiOpa(PlayState* play, const char* a, const char* b, const char* c, const char* d) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    if (a != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, a);
    }
    if (b != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, b);
    }
    if (c != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, c);
    }
    if (d != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, d);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void MsEq_GiXlu(PlayState* play, const char* a, const char* b, const char* c) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
    if (a != NULL) {
        gSPDisplayList(POLY_XLU_DISP++, a);
    }
    if (b != NULL) {
        gSPDisplayList(POLY_XLU_DISP++, b);
    }
    if (c != NULL) {
        gSPDisplayList(POLY_XLU_DISP++, c);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_OoTEquip_DrawGetItem(PlayState* play, u8 item) {
    u32 frames = play->state.frames;

    switch (item) {
        case ITEM_OOT_SWORD_MASTER:
            // SoH Randomizer_DrawMasterSword: the Temple of Time's Master Sword (Ocarina of Time has no get-item model).
            OPEN_DISPS(play->state.gfxCtx);
            Gfx_SetupDL25_Opa(play->state.gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08, Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames, 0, 32, 32, 1, 0, 0, 32, 32));
            Matrix_Push();
            Matrix_Scale(0.05f, 0.05f, 0.05f, MTXMODE_APPLY);
            Matrix_RotateZF(2.1f, MTXMODE_APPLY);
            MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
            gSPDisplayList(POLY_OPA_DISP++, sGiMasterSwordDL);
            Matrix_Pop();
            CLOSE_DISPS(play->state.gfxCtx);
            break;
        case ITEM_OOT_SWORD_BIGGORON:
        case ITEM_OOT_SWORD_GIANTS_KNIFE:
        case ITEM_OOT_SWORD_BROKEN:
            // GetItem_DrawGoronSword
            OPEN_DISPS(play->state.gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08, Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames, 0, 32, 32, 1, 0, 0, 32, 32));
            CLOSE_DISPS(play->state.gfxCtx);
            MsEq_GiOpa(play, sGiBiggoronSwordDL, NULL, NULL, NULL);
            break;
        case ITEM_OOT_SHIELD_DEKU:
            MsEq_GiOpa(play, sGiDekuShieldDL, NULL, NULL, NULL);
            break;
        case ITEM_OOT_SWORD_KOKIRI:
            MsEq_GiOpa(play, sGiKokiriSwordDL, NULL, NULL, NULL); // OoT GetItem_DrawOpa0 (GID_SWORD_KOKIRI)
            break;
        case ITEM_OOT_SHIELD_HYLIAN:
            MsEq_GiOpa(play, sGiHylianShieldDL, NULL, NULL, NULL);
            break;
        case ITEM_OOT_SHIELD_MIRROR:
            // GetItem_DrawMirrorShield
            OPEN_DISPS(play->state.gfxCtx);
            gSPSegment(POLY_OPA_DISP++, 0x08,
                       Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, (frames * 2) % 256, 64, 64, 1, 0, frames % 128, 32,
                                        32));
            CLOSE_DISPS(play->state.gfxCtx);
            MsEq_GiOpa(play, sGiMirrorShieldDL, NULL, NULL, NULL);
            MsEq_GiXlu(play, sGiMirrorShieldSymbolDL, NULL, NULL);
            break;
        case ITEM_OOT_TUNIC_KOKIRI:
        case ITEM_OOT_TUNIC_GORON:
            // GetItem_DrawOpa1023 (Ocarina of Time has no Kokiri Tunic model; the tunic model in the Goron's colours)
            MsEq_GiOpa(play, sGiGoronCollarColorDL, sGiTunicCollarDL, sGiGoronTunicColorDL, sGiTunicDL);
            break;
        case ITEM_OOT_TUNIC_ZORA:
            MsEq_GiOpa(play, sGiZoraCollarColorDL, sGiTunicCollarDL, sGiZoraTunicColorDL, sGiTunicDL);
            break;
        case ITEM_OOT_BOOTS_KOKIRI:
        case ITEM_OOT_BOOTS_IRON:
            // GetItem_DrawOpa0Xlu1
            MsEq_GiOpa(play, sGiIronBootsDL, NULL, NULL, NULL);
            MsEq_GiXlu(play, sGiIronBootsRivetsDL, NULL, NULL);
            break;
        case ITEM_OOT_BOOTS_HOVER:
            MsEq_GiOpa(play, sGiHoverBootsDL, NULL, NULL, NULL);
            break;
        case ITEM_OOT_BRACELET:
            MsEq_GiOpa(play, sGiGoronBraceletDL, NULL, NULL, NULL);
            break;
        case ITEM_OOT_GAUNTLETS_SILVER:
            // GetItem_DrawOpa10Xlu32
            MsEq_GiOpa(play, sGiSilverGauntletsColorDL, sGiGauntletsDL, NULL, NULL);
            MsEq_GiXlu(play, sGiSilverGauntletsPlateColorDL, sGiGauntletsPlateDL, NULL);
            break;
        case ITEM_OOT_GAUNTLETS_GOLD:
            MsEq_GiOpa(play, sGiGoldenGauntletsColorDL, sGiGauntletsDL, NULL, NULL);
            MsEq_GiXlu(play, sGiGoldenGauntletsPlateColorDL, sGiGauntletsPlateDL, NULL);
            break;
        case ITEM_OOT_SCALE_SILVER:
        case ITEM_OOT_SCALE_GOLDEN:
            // GetItem_DrawScale
            OPEN_DISPS(play->state.gfxCtx);
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScroll(play->state.gfxCtx, 0, frames * 2, -(frames * 2), 64, 64, 1, frames * 4,
                                        -(frames * 4), 32, 32));
            CLOSE_DISPS(play->state.gfxCtx);
            if (item == ITEM_OOT_SCALE_SILVER) {
                MsEq_GiXlu(play, sGiSilverScaleColorDL, sGiScaleDL, NULL);
                MsEq_GiXlu(play, sGiSilverScaleWaterColorDL, sGiScaleWaterDL, NULL);
            } else {
                MsEq_GiXlu(play, sGiGoldenScaleColorDL, sGiScaleDL, NULL);
                MsEq_GiXlu(play, sGiGoldenScaleWaterColorDL, sGiScaleWaterDL, NULL);
            }
            break;
        case ITEM_OOT_BULLET_BAG:
            // GetItem_DrawBulletBag (the 50-seed bag in its own colours)
            if (gSaveContext.save.shipSaveInfo.mergeShipOoTItems.bulletBag >= 3) {
                MsEq_GiOpa(play, sGiBulletBag50ColorDL, sGiBulletBagDL, NULL, NULL);
                MsEq_GiXlu(play, sGiBulletBagStringDL, sGiBulletBag50StringColorDL, sGiBulletBagWritingDL);
            } else {
                MsEq_GiOpa(play, sGiBulletBagColorDL, sGiBulletBagDL, NULL, NULL);
                MsEq_GiXlu(play, sGiBulletBagStringDL, sGiBulletBagStringColorDL, sGiBulletBagWritingDL);
            }
            break;
        default:
            break;
    }
}
