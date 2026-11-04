// MergeShip2 -- Ocarina of Time's items on Majora's Mask's human (MergeShipOoTItems.h): the models Ocarina of Time's
// young Link draws for them, attached to MM's human the way OoT attaches them to its Link (same limbs, same offsets).
// Included at the end of z_player_lib.c (it uses that file's statics); the hooks there are one-line calls.
//
//  * Slingshot: OoT's gLinkChildRightHandHoldingSlingshot in the right hand (MM PLAYER_MODELGROUP_BOW = OoT's
//    BOW_SLINGSHOT group), its string (OoT sBowStringData[slingshot]) instead of the bow's, and OoT's stretched first
//    person arm (sPlayerFirstPersonRightHandDLs, child).
//  * Boomerang: OoT's gLinkChildLeftFistAndBoomerang in the left hand (MM PLAYER_MODELGROUP_8 / LH_4 = OoT's BOOMERANG
//    group / LH_BOOMERANG); an open hand while it flies (MM's own LH_4 rule).
//  * Megaton Hammer: OoT's only hammer model is adult Link's (gLinkAdultLeftHandHoldingHammer). Its hand part is cut
//    off (the display list ends where OoT's hand starts, as SoH's EquipmentAlwaysVisible does for young Link) and the
//    hammer is drawn in MM's human's closed left hand.
//  * Longshot: likewise OoT's adult hookshot model (OoT's Longshot uses it) in MM's human's closed right hand.
//  * Skull, Spooky and Gerudo masks: OoT's young-Link mask display lists on the head limb (OoT
//    Player_PostLimbDrawGameplay's mask branch).

#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipAssets.h"

static const ALIGN_ASSET(2) char sMsOoTSlingshotHandDL[] =
    "__OTR__oot/objects/object_link_child/gLinkChildRightHandHoldingSlingshotNearDL";
static const ALIGN_ASSET(2) char sMsOoTSlingshotStringDL[] =
    "__OTR__oot/objects/object_link_child/gLinkChildSlinghotStringDL";
static const ALIGN_ASSET(2) char sMsOoTBowStringDL[] = "__OTR__oot/objects/object_link_boy/gLinkAdultBowStringDL";
static const ALIGN_ASSET(2) char sMsOoTSlingshotFirstPersonDL[] =
    "__OTR__oot/objects/object_link_child/gLinkChildRightArmStretchedSlingshotDL";
static const ALIGN_ASSET(2) char sMsOoTBoomerangFistDL[] =
    "__OTR__oot/objects/object_link_child/gLinkChildLeftFistAndBoomerangNearDL";
static const ALIGN_ASSET(2) char sMsOoTHammerDL[] =
    "__OTR__oot/objects/object_link_boy/gLinkAdultLeftHandHoldingHammerNearDL";
static const ALIGN_ASSET(2) char sMsOoTHookshotDL[] =
    "__OTR__oot/objects/object_link_boy/gLinkAdultRightHandHoldingHookshotNearDL";
// The adult's first-person aiming hand (MergeShipAdultLink.c's sSlots uses the same DL for the adult; young Link
// fits it too, the way his third-person hand already fits gLinkAdultRightHandHoldingHookshotNearDL above).
static const ALIGN_ASSET(2) char sMsOoTHookshotFirstPersonDL[] =
    "__OTR__oot/objects/object_link_boy/gLinkAdultRightHandHoldingHookshotFarDL";
// ArmsHook_Draw's flying hook and chain (z_arms_hook.c), Ocarina of Time's own for both its ages.
static const ALIGN_ASSET(2) char sMsOoTHookshotTipDL[] = "__OTR__oot/objects/object_link_boy/gLinkAdultHookshotTipDL";
static const ALIGN_ASSET(2) char sMsOoTHookshotChainDL[] =
    "__OTR__oot/objects/object_link_boy/gLinkAdultHookshotChainDL";
static const ALIGN_ASSET(2) char sMsOoTSkullMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildSkullMaskDL";
static const ALIGN_ASSET(2) char sMsOoTSpookyMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildSpookyMaskDL";
static const ALIGN_ASSET(2) char sMsOoTGerudoMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildGerudoMaskDL";
static const ALIGN_ASSET(2) char sMsOoTKeatonMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildKeatonMaskDL";
static const ALIGN_ASSET(2) char sMsOoTBunnyMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildBunnyHoodDL";
static const ALIGN_ASSET(2) char sMsOoTGoronMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildGoronMaskDL";
static const ALIGN_ASSET(2) char sMsOoTZoraMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildZoraMaskDL";
static const ALIGN_ASSET(2) char sMsOoTTruthMaskDL[] = "__OTR__oot/objects/object_link_child/gLinkChildMaskOfTruthDL";

void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);

// The adult models' hand parts start at these commands (SoH AgeDependentEquipment.cpp, the same OoT resources).
static void MergeShip_OoTItems_CutAdultHands(void) {
    static s32 sCut = false;
    Gfx end[] = { gsSPEndDisplayList() };

    if (sCut || !MergeShip_IsOcarinaMounted()) {
        return;
    }
    sCut = true;
    ResourceMgr_PatchGfxByName("oot/objects/object_link_boy/gLinkAdultLeftHandHoldingHammerNearDL",
                               "mergeShipHammerOnly", 92, end[0]);
    ResourceMgr_PatchGfxByName("oot/objects/object_link_boy/gLinkAdultRightHandHoldingHookshotNearDL",
                               "mergeShipHookshotOnly", 84, end[0]);
}

static s32 MergeShip_OoTItems_DrawsOn(Player* player) {
    return (player->transformation == PLAYER_FORM_HUMAN) && (player->actor.id == ACTOR_PLAYER) &&
           MergeShip_OoTItems_Enabled();
}

// After MM chose a hand's display list (Player_OverrideLimbDrawGameplayDefault).
static const ALIGN_ASSET(2) char sMsFairyOcarinaHandDL[] = MERGESHIP_OOT_FAIRY_OCARINA_HAND_DL;

void MergeShip_OoTItems_OverrideHandDL(Player* player, s32 limbIndex, Gfx** dList) {
    // BUG-0031: Hyrule's Fairy Ocarina in the young human's hand (the adult's hand: MergeShip_AdultLink_RightHand).
    if ((limbIndex == PLAYER_LIMB_RIGHT_HAND) && (player->transformation == PLAYER_FORM_HUMAN) && (*dList != NULL) &&
        (strncmp((const char*)*dList, "__OTR__", 7) == 0) &&
        (strcmp((const char*)*dList, (const char*)gLinkHumanRightHandHoldingOcarinaDL) == 0) &&
        MergeShip_OoTItems_FairyOcarinaShown()) {
        *dList = (Gfx*)sMsFairyOcarinaHandDL;
    }
    if (!MergeShip_OoTItems_DrawsOn(player)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_RIGHT_HAND) {
        if ((player->heldItemAction == PLAYER_IA_OOT_SLINGSHOT) &&
            (player->rightHandType == PLAYER_MODELTYPE_RH_BOW)) {
            *dList = (Gfx*)sMsOoTSlingshotHandDL;
        } else if ((player->heldItemAction == PLAYER_IA_OOT_LONGSHOT) &&
                   (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT)) {
            *dList = gLinkHumanRightHandClosedDL;
        }
    } else if (limbIndex == PLAYER_LIMB_LEFT_HAND) {
        if ((player->heldItemAction == PLAYER_IA_OOT_BOOMERANG) && (sPlayerLeftHandType == PLAYER_MODELTYPE_LH_4)) {
            // OoT: LH_BOOMERANG shows the boomerang in the fist; while it flies MM's LH_4 rule has already opened it.
            *dList = (Gfx*)sMsOoTBoomerangFistDL;
        }
    }
}

// First person (OoT Player_OverrideLimbDrawGameplayFirstPerson: the slingshot arm and the Longshot's hand for young
// Link -- the adult's own first-person hand is MergeShip_AdultLink_OverrideLimbFirstPerson, run after this hook, so
// this only has to cover the young human; when it does run for the adult it sets the same DL the row swap already
// did, harmlessly).
void MergeShip_OoTItems_OverrideFirstPersonHandDL(Player* player, s32 limbIndex, Gfx** dList) {
    if (!MergeShip_OoTItems_DrawsOn(player) || (limbIndex != PLAYER_LIMB_RIGHT_HAND)) {
        return;
    }
    if (MergeShip_OoTEquip_HoldsFairyBow(player) && (gPlayState != NULL)) {
        // BUG-0015: Hyrule's Fairy Bow in first person, not MM's Hero's Bow.
        *dList = MergeShip_OoTEquip_FairyBowFirstPersonDL(gPlayState);
    } else if (player->heldItemAction == PLAYER_IA_OOT_SLINGSHOT) {
        *dList = (Gfx*)sMsOoTSlingshotFirstPersonDL;
    } else if (player->heldItemAction == PLAYER_IA_OOT_LONGSHOT) {
        *dList = (Gfx*)sMsOoTHookshotFirstPersonDL;
    }
}

// The bow string of MM's RH_BOW: OoT's slingshot string while the Slingshot is out (OoT sBowStringData[1] and its
// young-Link rotation). Returns true when it drew the string.
s32 MergeShip_OoTItems_DrawSlingshotString(PlayState* play, Player* player) {
    static Vec3f sSlingshotStringPos = { 606.0f, 236.0f, 0.0f };
    // BUG-0015: Hyrule's Fairy Bow in young Link's fist (MergeShipOoTEquipDraw.c): OoT's adult string (sBowStringData[0],
    // { 0, -360.4, 0 }) under the same hand fit (MSEQ_FIT_HAND_R: scale 0.8).
    static Vec3f sFairyBowStringPos = { 0.0f, -360.4f * 0.8f, 0.0f };
    // BUG-0027: Ocarina of Time's adult holds Ocarina of Time's bow (MergeShipAdultLink.c's rows), so its string is
    // OoT's adult string where OoT puts it (sBowStringData[0]), not MM's young Link's.
    static Vec3f sAdultBowStringPos = { 0.0f, -360.4f, 0.0f };
    // BUG-0059: MM's string, fitted. BUG-0102: in first person the Hero's Bow lies on Ocarina of Time's bow, its upper
    // tip on Ocarina of Time's (MergeShipAdultLink.c MergeShip_AdultLink_HeroBowFPValues): Ocarina of Time's own string
    // meets it there and pulls back to the arrow as in Ocarina of Time (MM's, turned with the bow, went sideways).
    const s32 heroBow = MergeShip_OoTEquip_AdultHoldsHeroBow(player) && (player->unk_AA5 != PLAYER_UNKAA5_3);
    const s32 adultBow = MergeShip_AdultLink_IsActive(player) && (player->heldItemAction != PLAYER_IA_OOT_SLINGSHOT) &&
                         !heroBow;
    const s32 fairyBow = MergeShip_OoTEquip_HoldsFairyBow(player);
    f32 stringScale = adultBow ? 1.0f : 0.8f;

    if (!MergeShip_OoTItems_DrawsOn(player) ||
        ((player->heldItemAction != PLAYER_IA_OOT_SLINGSHOT) && !fairyBow && !adultBow && !heroBow)) {
        return false;
    }
    if (heroBow) {
        static Vec3f sMMBowStringPos = { -35.0f, -395.0f, 0.0f }; // MM D_801C0D98

        stringScale = MergeShip_AdultLink_RightFitPoint(&sMMBowStringPos, &sSlingshotStringPos);
    } else if (adultBow) {
        sSlingshotStringPos = sAdultBowStringPos;
    } else if (fairyBow) {
        sSlingshotStringPos = sFairyBowStringPos;
    } else {
        sSlingshotStringPos.x = 606.0f;
        sSlingshotStringPos.y = 236.0f;
        sSlingshotStringPos.z = 0.0f;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(sSlingshotStringPos.x, sSlingshotStringPos.y, sSlingshotStringPos.z, MTXMODE_APPLY);
    // OoT: while READY_TO_FIRE the string stretches to the hand (MM's bow code does the same with its own fields).
    if ((player->stateFlags3 & PLAYER_STATE3_40) && (player->unk_B28 >= 0) && (player->unk_ACC < 0xB)) {
        Vec3f handPos;
        f32 dist;

        Matrix_MultZero(&handPos);
        dist = Math_Vec3f_DistXYZ(sPlayerCurBodyPartPos, &handPos);
        player->unk_B08 = dist - 3.0f;
        if (dist < 3.0f) {
            player->unk_B08 = 0.0f;
        } else {
            player->unk_B08 *= 1.6f;
            if (player->unk_B08 > 1.0f) {
                player->unk_B08 = 1.0f;
            }
        }
        player->unk_B0C = -0.5f;
    }
    if (heroBow) {
        // MM's string (z_player_lib.c D_801C0D94), at the fit's size.
        Matrix_Scale(stringScale, stringScale * player->unk_B08, stringScale, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)object_link_child_DL_017818);
    } else if (fairyBow || adultBow) {
        // The adult's string (at the hand fit's size on young Link; OoT draws it for the adult with no rotation).
        Matrix_Scale(stringScale, stringScale * player->unk_B08, stringScale, MTXMODE_APPLY);
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMsOoTBowStringDL);
    } else {
        Matrix_Scale(1.0f, player->unk_B08, 1.0f, MTXMODE_APPLY);
        Matrix_RotateZF(player->unk_B08 * -0.2f, MTXMODE_APPLY); // OoT: !LINK_IS_ADULT
        MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)sMsOoTSlingshotStringDL);
    }
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

// BUG-0066: OoT Player_DrawGameplay's Bunny Hood ears (Matrix_SetTranslateRotateYXZ at 97, -1203, -+240 with the
// ears' rest turn; sBunnyEarKinematics still), built directly as constants (no Matrix_* call, which frame interpolation
// would record mid-skeleton).
static void MergeShip_OoTItems_EarMtx(Mtx* out, f32 z, s16 rotX, s16 rotY, s16 rotZ) {
    MtxF mf;
    f32 sy = Math_SinS(rotY);
    f32 cy = Math_CosS(rotY);
    f32 sx = Math_SinS(rotX);
    f32 cx = Math_CosS(rotX);
    f32 sz = Math_SinS(rotZ);
    f32 cz = Math_CosS(rotZ);
    f32 t1;
    f32 t2;

    // OoT Matrix_SetTranslateRotateYXZ, line for line.
    mf.xx = cy;
    mf.zx = -sy;
    mf.xw = 97.0f;
    mf.yw = -1203.0f;
    mf.zw = z;
    mf.wx = 0.0f;
    mf.wy = 0.0f;
    mf.wz = 0.0f;
    mf.ww = 1.0f;
    mf.zz = cy * cx;
    mf.zy = cy * sx;
    mf.xz = sy * cx;
    mf.xy = sy * sx;
    mf.yz = -sx;
    mf.yy = cx;
    t1 = mf.xx;
    t2 = mf.xy;
    mf.xx = t1 * cz + t2 * sz;
    mf.xy = t2 * cz - t1 * sz;
    t1 = mf.zx;
    t2 = mf.zy;
    mf.zx = t1 * cz + t2 * sz;
    mf.zy = t2 * cz - t1 * sz;
    t2 = mf.yy;
    mf.yx = t2 * sz;
    mf.yy = t2 * cz;
    guMtxF2L(mf.mf, out);
}

static Mtx* MergeShip_OoTItems_BunnyEarMtx(PlayState* play) {
    Mtx* ears = GRAPH_ALLOC(play->state.gfxCtx, 2 * sizeof(Mtx));

    MergeShip_OoTItems_EarMtx(&ears[0], -240.0f, 0x3E2, 0xDBE, -0x348A);  // right ear
    MergeShip_OoTItems_EarMtx(&ears[1], 240.0f, -0x3E2, -0xDBE, -0x348A); // left ear
    return ears;
}

// Post limb draw: the hammer and the Longshot heads, the worn Ocarina of Time mask.
void MergeShip_OoTItems_PostLimbDraw(PlayState* play, Player* player, s32 limbIndex) {
    const char* dl = NULL;

    if (!MergeShip_OoTItems_DrawsOn(player) || (player->actor.scale.y < 0.0f)) {
        return;
    }

    if ((limbIndex == PLAYER_LIMB_LEFT_HAND) && (player->heldItemAction == PLAYER_IA_OOT_HAMMER) &&
        (player->leftHandType == PLAYER_MODELTYPE_LH_CLOSED)) {
        dl = sMsOoTHammerDL;
    } else if ((limbIndex == PLAYER_LIMB_RIGHT_HAND) && (player->heldItemAction == PLAYER_IA_OOT_LONGSHOT) &&
               (player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT)) {
        dl = sMsOoTHookshotDL;
    } else if ((limbIndex == PLAYER_LIMB_HEAD) && (player->currentMask == PLAYER_MASK_NONE)) {
        switch (MergeShip_OoTItems_WornMask()) {
            case ITEM_OOT_MASK_SKULL:
                dl = sMsOoTSkullMaskDL;
                break;
            case ITEM_OOT_MASK_SPOOKY:
                dl = sMsOoTSpookyMaskDL;
                break;
            case ITEM_OOT_MASK_GERUDO:
                dl = sMsOoTGerudoMaskDL;
                break;
            case ITEM_OOT_MASK_KEATON:
                dl = sMsOoTKeatonMaskDL;
                break;
            case ITEM_OOT_MASK_BUNNY:
                dl = sMsOoTBunnyMaskDL;
                break;
            case ITEM_OOT_MASK_GORON:
                dl = sMsOoTGoronMaskDL;
                break;
            case ITEM_OOT_MASK_ZORA:
                dl = sMsOoTZoraMaskDL;
                break;
            case ITEM_OOT_MASK_TRUTH:
                dl = sMsOoTTruthMaskDL;
                break;
            default:
                break;
        }
    }
    if (dl == NULL) {
        return;
    }

    MergeShip_OoTItems_CutAdultHands();

    OPEN_DISPS(play->state.gfxCtx);
    if (dl == sMsOoTBunnyMaskDL) {
        // BUG-0066: the hood's ears are two matrices on segment 0x0B (OoT Player_DrawGameplay), at rest.
        gSPSegment(POLY_OPA_DISP++, 0x0B, MergeShip_OoTItems_BunnyEarMtx(play));
    }
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

// Player_DrawGetItemImpl: the Ocarina of Time item Link holds up (a shown trade item, Ruto's Letter, a get item).
// Returns true when it drew one.
s32 MergeShip_OoTItems_DrawHeldGetItem(PlayState* play, Player* player) {
    u8 item = MergeShip_OoTItems_ShownItem(player);

    if (item == ITEM_NONE) {
        return false;
    }
    MergeShip_OoTItems_DrawGetItem(play, item);
    return true;
}

// ArmsHook_Draw drew Majora's Mask's own tip/chain unconditionally, even while the flying hook is Ocarina of Time's
// ported Longshot (heldItemAction stays PLAYER_IA_OOT_LONGSHOT for as long as it is out, same as the hand DL checks
// above) -- Ocarina of Time's Hookshot/Longshot must always be Ocarina of Time's model, in Hyrule and in Termina.
const char* MergeShip_OoTItems_HookshotTipDL(Player* player) {
    return (player->heldItemAction == PLAYER_IA_OOT_LONGSHOT) ? sMsOoTHookshotTipDL : NULL;
}

const char* MergeShip_OoTItems_HookshotChainDL(Player* player) {
    return (player->heldItemAction == PLAYER_IA_OOT_LONGSHOT) ? sMsOoTHookshotChainDL : NULL;
}
