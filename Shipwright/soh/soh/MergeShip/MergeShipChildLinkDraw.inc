// MergeShip2 -- Majora's Mask's human Link drawn as Ocarina of Time's child (see MergeShipChildLink.h).
//
// Included at the end of z_player_lib.c. The body is MM's (2ship2harkinian/mm/src/code/z_player_lib.c):
// gPlayerSkeletons[PLAYER_FORM_HUMAN] = gLinkHumanSkel, sPlayerEyesTextures/sPlayerMouthTextures[HUMAN], MM's
// bare hands and MM's waist. Everything the child wields or wears as equipment stays Ocarina of Time's, chosen
// by Ocarina of Time's own rules: its swords, shields, sheath and shield on the back, and whatever it holds.
//
// How the equipment is attached. Ocarina of Time bakes each held item into a "hand holding X" display list
// (gLinkChildLeftFistAndKokiriSwordNearDL is a fist and a sword). MM does not: its holding lists are a bare
// hand plus an item list (gLinkHumanLeftHandHoldingKokiriSwordDL = gLinkHumanLeftHandClosedDL + gKokiriSwordDL).
// So the child is drawn MM's way: MM's bare hand for the grip (sMMChildHandGrips), followed by Ocarina of Time's
// list for that hand with its hand removed -- every triangle drawn while Ocarina of Time's hand texture is
// loaded is dropped, everything else (the item's own textures, render state and triangles) is kept. The hand
// textures are read from Ocarina of Time's bare-hand lists, so nothing here names an item: any list the
// child's model-type tables hold, including ones added later, is attached the same way.
//
// Why this lines up. MM's human skeleton is Ocarina of Time's child skeleton: every limb has the same joint
// position (compared limb by limb in oot.o2r / mm.o2r) except the sheath, which MM moved 27 units down its
// parent's y axis. The hands therefore share their limb space with Ocarina of Time's, and the sheath limb is
// put back where Ocarina of Time's is (its joint position read from gLinkChildSkel) so Ocarina of Time's sheath
// and back-shield lists sit where they were modelled.
//
// Every list is loaded with ResourceMgr_LoadGfxByName (some of z_player_lib.c's readers hand table entries to
// the RSP as raw paths, and a raw mm.o2r display-list path faults on its first vertex: MergeShipAssets.h). The
// child's form tables keep Ocarina of Time's lists; only the skeleton and face textures are swapped there.
//
// The adult's gear on the child (SoH's TimelessEquipment / MergeShip.TimelessMMGear / EquipmentAlwaysVisible).
// Ocarina of Time models the Master Sword, Biggoron's Sword, the Giant's Knife, the Mirror Shield, the Hammer, the
// Bow, the Hookshot, the Master Sword's scabbard and the gauntlet plates only for the adult, in the adult's gloved
// hand. On MM's young Link they are worn the way 2Ship's MM human wears them (2s2h/MergeShip/MergeShipOoTEquipDraw.c):
//   - in hand: the adult's list without the adult's gloved hand (every triangle drawn while one of the adult's
//     bare-hand textures is loaded is dropped), held in MM's closed fist under the hand fit;
//   - on the back: the adult's scabbard (with the Master Sword in it while it is the sword put away) under the
//     sheath fit, and the shield on the back as the child carries it;
//   - the Silver and Golden Gauntlets: the adult's plates on MM's forearms and hands under the arm fit, in
//     Ocarina of Time's sGauntletColors, when the child has their strength (Player_GetStrength).
// With CVAR_ENHANCEMENT("MergeShip.MMChildAdultGear") (default on) the child shows the sword it really holds (the
// Master Sword, not Ocarina of Time's child stand-in, the Kokiri Sword), its scabbard and the plates; any adult
// list the child's tables hand over (EquipmentAlwaysVisible) is always worn this way. The fits are 2Ship's, under
// the same CVars: gMergeShip.OoTEquipFit.<HandL|HandR|Sheath|Arm>.<X|Y|Z|SX|SY|SZ>.

#include <stdio.h>
#include "mm/objects/object_link_child/object_link_child.h"
#include "soh/MergeShip/MergeShipAssets.h"

// MergeShip2: frame interpolation pairs a frame's matrix operations with the next frame's in order, so a block that is
// drawn on some frames only (a mask on the head, a sword fitted to a hand, a form's extra part) would shift every
// operation after it and stretch a limb for a frame. Each Matrix_Push .. Matrix_Pop here is its own interpolation
// child (SoH's FrameInterpolation_RecordOpenChild), keyed by where it is.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
#define MS_INTERP_PUSH() (FrameInterpolation_RecordOpenChild(__FILE__, __LINE__), Matrix_Push())
#define MS_INTERP_POP() (Matrix_Pop(), FrameInterpolation_RecordCloseChild())

void MergeShipTest_Log(const char* fmt, ...);

// MM GET_LEFT/RIGHT_HAND_INDEX_FROM_JOINT_TABLE: the appearance entry after the limbs (the one Ocarina of
// Time reads eyes and mouth from) also carries a hand shape in its upper byte.
#define MERGESHIP_MM_LEFT_HAND_INDEX(jointTable) ((jointTable)[PLAYER_LIMB_MAX].x & 0xF000)
#define MERGESHIP_MM_RIGHT_HAND_INDEX(jointTable) ((jointTable)[PLAYER_LIMB_MAX].x & 0x0F00)

u8 gMergeShipMMChildLink = false;

static void* sMergeShipOoTChildEyes[8];
static void* sMergeShipOoTChildMouths[4];
static FlexSkeletonHeader* sMergeShipOoTChildSkel;

// MM sPlayerEyesTextures[PLAYER_FORM_HUMAN] / sPlayerMouthTextures[PLAYER_FORM_HUMAN].
static void* sMergeShipMMHumanEyes[8] = {
    (void*)mm_gLinkHumanEyesOpenTex,  (void*)mm_gLinkHumanEyesHalfTex, (void*)mm_gLinkHumanEyesClosedTex,
    (void*)mm_gLinkHumanEyesRightTex, (void*)mm_gLinkHumanEyesLeftTex, (void*)mm_gLinkHumanEyesUpTex,
    (void*)mm_gLinkHumanEyesDownTex,  (void*)mm_gLinkHumanEyesWincingTex,
};
static void* sMergeShipMMHumanMouths[4] = {
    (void*)mm_gLinkHumanMouthClosedTex,
    (void*)mm_gLinkHumanMouthHalfTex,
    (void*)mm_gLinkHumanMouthOpenTex,
    (void*)mm_gLinkHumanMouthSmileTex,
};

s32 MergeShip_MMChildLink_Sync(void) {
    s32 i;
    u8 want = (u8)(CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMChildLink"), 1) && MergeShip_IsMajorasMaskMounted());

    if (sMergeShipOoTChildSkel == NULL) {
        sMergeShipOoTChildSkel = gPlayerSkelHeaders[PLAYER_FORM_CHILD];
        for (i = 0; i < 8; i++) {
            sMergeShipOoTChildEyes[i] = sEyeTextures[PLAYER_FORM_CHILD][i];
        }
        for (i = 0; i < 4; i++) {
            sMergeShipOoTChildMouths[i] = sMouthTextures[PLAYER_FORM_CHILD][i];
        }
    }
    if (want == gMergeShipMMChildLink) {
        return false;
    }

    gMergeShipMMChildLink = want;
    // MM gPlayerSkeletons[PLAYER_FORM_HUMAN]
    gPlayerSkelHeaders[PLAYER_FORM_CHILD] = want ? (FlexSkeletonHeader*)mm_gLinkHumanSkel : sMergeShipOoTChildSkel;
    for (i = 0; i < 8; i++) {
        sEyeTextures[PLAYER_FORM_CHILD][i] = want ? sMergeShipMMHumanEyes[i] : sMergeShipOoTChildEyes[i];
    }
    for (i = 0; i < 4; i++) {
        sMouthTextures[PLAYER_FORM_CHILD][i] = want ? sMergeShipMMHumanMouths[i] : sMergeShipOoTChildMouths[i];
    }
    MergeShipTest_Log("MMCHILD model=%s skel=%s", want ? "mm-human" : "oot-child",
                      (const char*)gPlayerSkelHeaders[PLAYER_FORM_CHILD] + 7);
    return true;
}

// ---- display-list surgery -------------------------------------------------------------------------------

// Commands the resource loader stores as two Gfx (the second holds a CRC64 of a resource path).
static s32 MergeShip_MMChild_GfxIsWide(u8 op) {
    return (op == G_SETTIMG_OTR_HASH) || (op == G_DL_OTR_HASH) || (op == G_VTX_OTR_HASH) || (op == G_BRANCH_Z_OTR) ||
           (op == G_MARKER) || (op == G_MTX_OTR) || (op == G_MOVEMEM_OTR);
}

static s32 MergeShip_MMChild_GfxIsTriangle(u8 op) {
    return (op == G_TRI1) || (op == G_TRI2) || (op == G_QUAD);
}

#define MERGESHIP_MM_CHILD_SKIN_MAX 24

typedef struct {
    u64 tex[MERGESHIP_MM_CHILD_SKIN_MAX];
    s32 count;
} MMChildSkin;

// The textures a list loads (a G_SETTIMG followed by a G_LOADBLOCK/G_LOADTILE), added to `skin`.
static void MergeShip_MMChild_CollectSkin(MMChildSkin* skin, const char* bareHand) {
    Gfx* cmd = ResourceMgr_LoadGfxByName(bareHand);
    u64 pending = 0;
    s32 i;

    if (cmd == NULL) {
        return;
    }
    for (;; cmd++) {
        u8 op = (u8)(cmd->words.w0 >> 24);

        if (op == G_ENDDL) {
            break;
        }
        if (op == G_SETTIMG_OTR_HASH) {
            pending = ((u64)cmd[1].words.w0 << 32) | (u32)cmd[1].words.w1;
        } else if (((op == G_LOADBLOCK) || (op == G_LOADTILE)) && (pending != 0)) {
            for (i = 0; i < skin->count; i++) {
                if (skin->tex[i] == pending) {
                    break;
                }
            }
            if ((i == skin->count) && (skin->count < MERGESHIP_MM_CHILD_SKIN_MAX)) {
                skin->tex[skin->count++] = pending;
            }
        }
        if (MergeShip_MMChild_GfxIsWide(op)) {
            cmd++;
        }
    }
}

// Ocarina of Time's child hand textures and MM's human hand textures, read from their bare hands; the adult's
// gloved-hand textures, read from his bare hands; and the child's and the adult's scabbard textures, read from
// their empty scabbards.
static MMChildSkin sMergeShipOoTChildSkin;
static MMChildSkin sMergeShipMMHumanSkin;
static MMChildSkin sMergeShipOoTAdultSkin;
static MMChildSkin sMergeShipOoTChildSheathSkin;
static MMChildSkin sMergeShipOoTAdultSheathSkin;

static void MergeShip_MMChild_InitSkins(void) {
    static const char* sOoTBareHands[] = {
        gLinkChildLeftHandNearDL,        gLinkChildLeftHandFarDL,     gLinkChildLeftFistNearDL,
        gLinkChildLeftFistFarDL,         gLinkChildRightHandNearDL,   gLinkChildRightHandFarDL,
        gLinkChildRightHandClosedNearDL, gLinkChildRightHandClosedFarDL, gLinkChildLeftHandUpNearDL,
    };
    static const char* sMMBareHands[] = {
        mm_gLinkHumanLeftHandOpenDL,  mm_gLinkHumanLeftHandClosedDL,  mm_gLinkHumanRightHandOpenDL,
        mm_gLinkHumanRightHandClosedDL, mm_gLinkHumanLeftHandHoldBottleDL,
    };
    static const char* sOoTAdultBareHands[] = {
        gLinkAdultLeftHandNearDL,        gLinkAdultLeftHandFarDL,        gLinkAdultLeftHandClosedNearDL,
        gLinkAdultLeftHandClosedFarDL,   gLinkAdultRightHandNearDL,      gLinkAdultRightHandFarDL,
        gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL,
    };
    s32 i;

    if ((sMergeShipOoTChildSkin.count != 0) && (sMergeShipOoTAdultSkin.count != 0) &&
        (sMergeShipOoTChildSheathSkin.count != 0) && (sMergeShipOoTAdultSheathSkin.count != 0)) {
        return;
    }
    // CollectSkin adds each texture once, so a list that failed to load is simply read again next time.
    for (i = 0; i < ARRAY_COUNT(sOoTBareHands); i++) {
        MergeShip_MMChild_CollectSkin(&sMergeShipOoTChildSkin, sOoTBareHands[i]);
    }
    for (i = 0; i < ARRAY_COUNT(sMMBareHands); i++) {
        MergeShip_MMChild_CollectSkin(&sMergeShipMMHumanSkin, sMMBareHands[i]);
    }
    for (i = 0; i < ARRAY_COUNT(sOoTAdultBareHands); i++) {
        MergeShip_MMChild_CollectSkin(&sMergeShipOoTAdultSkin, sOoTAdultBareHands[i]);
    }
    MergeShip_MMChild_CollectSkin(&sMergeShipOoTChildSheathSkin, gLinkChildSheathNearDL);
    MergeShip_MMChild_CollectSkin(&sMergeShipOoTChildSheathSkin, gLinkChildSheathFarDL);
    MergeShip_MMChild_CollectSkin(&sMergeShipOoTAdultSheathSkin, gLinkAdultSheathNearDL);
    MergeShip_MMChild_CollectSkin(&sMergeShipOoTAdultSheathSkin, gLinkAdultSheathFarDL);
    MergeShipTest_Log("MMCHILD skin textures oot=%d mm=%d adult=%d childSheath=%d adultSheath=%d",
                      sMergeShipOoTChildSkin.count, sMergeShipMMHumanSkin.count, sMergeShipOoTAdultSkin.count,
                      sMergeShipOoTChildSheathSkin.count, sMergeShipOoTAdultSheathSkin.count);
}

// Whether `dl` loads any of `skin`'s textures.
static s32 MergeShip_MMChild_UsesSkin(Gfx* dl, const MMChildSkin* skin) {
    u64 pending = 0;
    s32 i;

    if ((dl == NULL) || (skin->count == 0)) {
        return false;
    }
    for (;; dl++) {
        u8 op = (u8)(dl->words.w0 >> 24);

        if (op == G_ENDDL) {
            break;
        }
        if (op == G_SETTIMG_OTR_HASH) {
            pending = ((u64)dl[1].words.w0 << 32) | (u32)dl[1].words.w1;
        } else if (((op == G_LOADBLOCK) || (op == G_LOADTILE)) && (pending != 0)) {
            for (i = 0; i < skin->count; i++) {
                if (skin->tex[i] == pending) {
                    return true;
                }
            }
        }
        if (MergeShip_MMChild_GfxIsWide(op)) {
            dl++;
        }
    }
    return false;
}

// Copies `src` into `out` without its skin triangles (keepSkin false) or with only them (keepSkin true), and
// without its G_ENDDL. Returns the Gfx written; `*trianglesKept` counts the triangle commands kept.
static s32 MergeShip_MMChild_FilterHand(Gfx* out, Gfx* src, const MMChildSkin* skin, s32 keepSkin,
                                        s32* trianglesKept) {
    s32 n = 0;
    u64 pending = 0;
    s32 onSkin = false;
    s32 i;

    for (;; src++) {
        u8 op = (u8)(src->words.w0 >> 24);

        if (op == G_ENDDL) {
            break;
        }
        if (op == G_SETTIMG_OTR_HASH) {
            pending = ((u64)src[1].words.w0 << 32) | (u32)src[1].words.w1;
        } else if ((op == G_LOADBLOCK) || (op == G_LOADTILE)) {
            onSkin = false;
            for (i = 0; i < skin->count; i++) {
                if (skin->tex[i] == pending) {
                    onSkin = true;
                    break;
                }
            }
        }
        if (MergeShip_MMChild_GfxIsTriangle(op)) {
            if (onSkin != keepSkin) {
                continue;
            }
            (*trianglesKept)++;
        }
        out[n++] = src[0];
        if (MergeShip_MMChild_GfxIsWide(op)) {
            out[n++] = src[1];
            src++;
        }
    }
    return n;
}

static s32 MergeShip_MMChild_GfxLength(Gfx* dl) {
    s32 n = 1;

    for (; (u8)(dl->words.w0 >> 24) != G_ENDDL; dl++, n++) {
        if (MergeShip_MMChild_GfxIsWide((u8)(dl->words.w0 >> 24))) {
            dl++;
            n++;
        }
    }
    return n;
}

// ---- the adult's gear: fits -----------------------------------------------------------------------------

static s32 MergeShip_MMChild_AdultGearOn(void) {
    return CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.MMChildAdultGear"), 1);
}

typedef enum {
    MMCHILD_FIT_HAND_L,
    MMCHILD_FIT_HAND_R,
    MMCHILD_FIT_SHEATH,
    MMCHILD_FIT_ARM,
    MMCHILD_FIT_FOOT,
    MMCHILD_FIT_FOOT_R,
    MMCHILD_FIT_MAX
} MMChildFitId;

typedef struct {
    const char* name;
    f32 v[6]; // x, y, z, sx, sy, sz
} MMChildFit;

// 2Ship's fits for the adult's gear on MM's human (2s2h/MergeShip/MergeShipOoTEquipDraw.c sFits), in the limb's
// space: the hands at SoH's ScaleAdultEquipmentAsChild 0.8, the scabbard translated 218,-100,62 and scaled 0.8, the
// plates at the adult's fist length against the child's (679 / 499 = 0.74).
static const MMChildFit sMergeShipMMChildFits[MMCHILD_FIT_MAX] = {
    { "HandL", { 0.0f, 0.0f, 0.0f, 0.8f, 0.8f, 0.8f } },
    { "HandR", { 0.0f, 0.0f, 0.0f, 0.8f, 0.8f, 0.8f } },
    { "Sheath", { 218.0f, -100.0f, 62.0f, 0.8f, 0.8f, 0.8f } },
    { "Arm", { 0.0f, 0.0f, 0.0f, 0.74f, 0.74f, 0.74f } },
    // BUG-0099: the adult's boots on MM's young Link's feet, in the foot limb's space (MM gLinkHuman*FootDL against OoT
    // gLinkAdult*FootNearDL): length 879 / 1304 (0.674, also up the leg, so the boot keeps its shape), width 598 / 637;
    // the soles meet (the iron boot's at 550 * 0.674 - 56 = MM's 315) and the boot is centered on the foot, which MM
    // set about 70 to the outside on each foot (left z center 59, right -74; the adult's -16 / -6).
    { "FootL", { 19.0f, -56.0f, 74.0f, 0.674f, 0.674f, 0.94f } },
    { "FootR", { 19.0f, -56.0f, -68.0f, 0.674f, 0.674f, 0.94f } },
};

static void MergeShip_MMChild_GetFit(s32 fit, f32* out) {
    static const char* sAxes[6] = { "X", "Y", "Z", "SX", "SY", "SZ" };
    char cvar[64];
    s32 i;

    for (i = 0; i < 6; i++) {
        snprintf(cvar, sizeof(cvar), "gMergeShip.OoTEquipFit.%s.%s", sMergeShipMMChildFits[fit].name, sAxes[i]);
        out[i] = CVarGetFloat(cvar, sMergeShipMMChildFits[fit].v[i]);
    }
}

// The fit on its own, multiplied onto the limb's matrix (G_MTX_MUL).
static Mtx* MergeShip_MMChild_FitMtx(PlayState* play, s32 fit) {
    f32 f[6];
    Mtx* mtx;

    MergeShip_MMChild_GetFit(fit, f);
    MS_INTERP_PUSH();
    Matrix_Translate(f[0], f[1], f[2], MTXMODE_NEW);
    Matrix_Scale(f[3], f[4], f[5], MTXMODE_APPLY);
    mtx = MATRIX_NEWMTX(play->state.gfxCtx);
    MS_INTERP_POP();
    return mtx;
}

// `list` under fit `fit`.
static Gfx* MergeShip_MMChild_Fitted(PlayState* play, Gfx* list, s32 fit) {
    Gfx* out = Graph_Alloc(play->state.gfxCtx, 4 * sizeof(Gfx));

    gSPMatrix(&out[0], MergeShip_MMChild_FitMtx(play, fit), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPDisplayList(&out[1], list);
    gSPPopMatrix(&out[2], G_MTX_MODELVIEW);
    gSPEndDisplayList(&out[3]);
    return out;
}

// ---- the hands ------------------------------------------------------------------------------------------

// MM's grip for each of Ocarina of Time's hand model types (the child's column of z_player_lib.c's tables).
// `fromHolding`: `mmHand` is an MM holding list whose own item is dropped, keeping MM's hand in that pose.
typedef struct {
    const char* mmHand;
    u8 fromHolding;
} MMChildHandGrip;

static const MMChildHandGrip sMMChildHandGrips[PLAYER_MODELTYPE_RH_HOOKSHOT + 1] = {
    /* PLAYER_MODELTYPE_LH_OPEN */ { mm_gLinkHumanLeftHandOpenDL, false },
    /* PLAYER_MODELTYPE_LH_CLOSED */ { mm_gLinkHumanLeftHandClosedDL, false },
    /* PLAYER_MODELTYPE_LH_SWORD */ { mm_gLinkHumanLeftHandClosedDL, false }, // MM holds swords in this fist
    /* PLAYER_MODELTYPE_LH_SWORD_2 */ { mm_gLinkHumanLeftHandClosedDL, false },
    /* PLAYER_MODELTYPE_LH_BGS */ { mm_gLinkHumanLeftHandClosedDL, false }, // the child's Master Sword
    /* PLAYER_MODELTYPE_LH_HAMMER */ { mm_gLinkHumanLeftHandOpenDL, false }, // the child's column: an open hand
    /* PLAYER_MODELTYPE_LH_BOOMERANG */ { mm_gLinkHumanLeftHandClosedDL, false },
    /* PLAYER_MODELTYPE_LH_BOTTLE */ { mm_gLinkHumanLeftHandHoldBottleDL, false }, // the bottle is drawn apart
    /* PLAYER_MODELTYPE_RH_OPEN */ { mm_gLinkHumanRightHandOpenDL, false },
    /* PLAYER_MODELTYPE_RH_CLOSED */ { mm_gLinkHumanRightHandClosedDL, false },
    /* PLAYER_MODELTYPE_RH_SHIELD */ { mm_gLinkHumanRightHandClosedDL, false }, // MM holds shields in this fist
    /* PLAYER_MODELTYPE_RH_BOW_SLINGSHOT */ { mm_gLinkHumanRightHandClosedDL, false }, // MM's bow grip too
    /* PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2 */ { mm_gLinkHumanRightHandClosedDL, false },
    /* PLAYER_MODELTYPE_RH_OCARINA */ { mm_gLinkHumanRightHandHoldingOcarinaDL, true },
    /* PLAYER_MODELTYPE_RH_OOT */ { mm_gLinkHumanRightHandHoldingOcarinaDL, true },
    /* PLAYER_MODELTYPE_RH_HOOKSHOT */ { mm_gLinkHumanRightHandOpenDL, false }, // the child's column: an open hand
};

// MM's hand for `type` and whatever Ocarina of Time's list `ootHand` holds in it. `openClosed` (from the
// animation, MM D_801C095C/D_801C0964) replaces an empty hand with MM's open (1) or closed (0) one. An adult's list
// (the adult's gear on the child) loses the adult's gloved hand instead of the child's; its item is held in MM's
// closed fist under the hand fit (`*adultItem`).
static Gfx* MergeShip_MMChild_ComposeHand(PlayState* play, s32 type, Gfx* ootHand, s32 isLeft, s32 openClosed,
                                          const char** chosen, s32* withItem, s32* adultItem) {
    const MMChildHandGrip* grip;
    Gfx* mmHand;
    Gfx* out;
    Gfx* items;
    s32 itemCount;
    s32 itemTris = 0;
    s32 mmTris = 0;
    s32 n = 0;
    s32 adult;

    *adultItem = false;
    if ((type < 0) || (type >= ARRAY_COUNT(sMMChildHandGrips)) || (ootHand == NULL)) {
        return NULL;
    }
    MergeShip_MMChild_InitSkins();
    grip = &sMMChildHandGrips[type];
    adult = MergeShip_MMChild_UsesSkin(ootHand, &sMergeShipOoTAdultSkin);

    // Ocarina of Time's list without its hand, i.e. the equipment.
    items = Graph_Alloc(play->state.gfxCtx, MergeShip_MMChild_GfxLength(ootHand) * sizeof(Gfx));
    itemCount = MergeShip_MMChild_FilterHand(items, ootHand, adult ? &sMergeShipOoTAdultSkin : &sMergeShipOoTChildSkin,
                                             false, &itemTris);

    *chosen = grip->mmHand;
    if (adult && (itemTris != 0)) {
        // 2Ship MergeShip_OoTEquip_OverrideLimb: the adult's sword, hammer, shield, bow or hookshot in MM's fist.
        *chosen = isLeft ? mm_gLinkHumanLeftHandClosedDL : mm_gLinkHumanRightHandClosedDL;
    } else if ((itemTris == 0) && (openClosed >= 0)) {
        *chosen = isLeft ? (openClosed ? mm_gLinkHumanLeftHandOpenDL : mm_gLinkHumanLeftHandClosedDL)
                         : (openClosed ? mm_gLinkHumanRightHandOpenDL : mm_gLinkHumanRightHandClosedDL);
    }
    mmHand = ResourceMgr_LoadGfxByName(*chosen);
    if (mmHand == NULL) {
        return NULL;
    }

    out = Graph_Alloc(play->state.gfxCtx,
                      (MergeShip_MMChild_GfxLength(mmHand) + itemCount + 4) * sizeof(Gfx));
    if (grip->fromHolding && (*chosen == grip->mmHand)) {
        n = MergeShip_MMChild_FilterHand(out, mmHand, &sMergeShipMMHumanSkin, true, &mmTris);
    } else {
        gSPDisplayList(&out[n], mmHand);
        n++;
    }
    *withItem = (itemTris != 0);
    if (itemTris != 0) {
        s32 i;

        if (adult) {
            gSPMatrix(&out[n], MergeShip_MMChild_FitMtx(play, isLeft ? MMCHILD_FIT_HAND_L : MMCHILD_FIT_HAND_R),
                      G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            n++;
        }
        for (i = 0; i < itemCount; i++) {
            out[n++] = items[i];
        }
        if (adult) {
            gSPPopMatrix(&out[n], G_MTX_MODELVIEW);
            n++;
        }
        *adultItem = adult;
    }
    gSPEndDisplayList(&out[n]);
    return out;
}

static const char* sMergeShipMMChildLoggedDL[PLAYER_LIMB_MAX];
static s8 sMergeShipMMChildLoggedAdult[PLAYER_LIMB_MAX];

static void MergeShip_MMChild_LogDL(s32 limbIndex, const char* path, s32 withItem, s32 adultItem) {
    if ((path != NULL) && ((sMergeShipMMChildLoggedDL[limbIndex] != path) ||
                           (sMergeShipMMChildLoggedAdult[limbIndex] != (s8)adultItem))) {
        sMergeShipMMChildLoggedDL[limbIndex] = path;
        sMergeShipMMChildLoggedAdult[limbIndex] = (s8)adultItem;
        MergeShipTest_Log("MMCHILD limb=%d hand=%s ootItem=%d adultItem=%d", limbIndex, path + 7, withItem, adultItem);
    }
}

// Ocarina of Time's sheath limb joint, which its sheath and back-shield lists are modelled around.
static s32 MergeShip_MMChild_OoTSheathJoint(Vec3f* pos) {
    static s32 sLoaded = false;
    static Vec3s sJoint;

    if (!sLoaded) {
        SkeletonHeader* skel = ResourceMgr_LoadSkeletonByName((const char*)sMergeShipOoTChildSkel, NULL);

        if ((skel == NULL) || (skel->limbCount < PLAYER_LIMB_SHEATH)) {
            return false;
        }
        sJoint = ((LodLimb*)skel->segment[PLAYER_LIMB_SHEATH - 1])->jointPos;
        sLoaded = true;
        MergeShipTest_Log("MMCHILD sheath joint oot=(%d,%d,%d) mm=(%.0f,%.0f,%.0f)", sJoint.x, sJoint.y, sJoint.z,
                          pos->x, pos->y, pos->z);
    }
    pos->x = sJoint.x;
    pos->y = sJoint.y;
    pos->z = sJoint.z;
    return true;
}

// Both hands, the sheath and the waist of MM's human as the child, after Ocarina of Time chose the child's
// lists (`*dList`: a loaded list in gameplay, a table path on the equipment screen).
static void MergeShip_MMChild_OverrideLimbImpl(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos,
                                               s32 handType, Vec3s* jointTable, s32 isPath) {
    Gfx* ootList;
    Gfx* composed;
    const char* chosen = NULL;
    s32 openClosed = -1;
    s32 withItem = false;
    s32 adultItem = false;

    if ((limbIndex == PLAYER_LIMB_L_HAND) || (limbIndex == PLAYER_LIMB_R_HAND)) {
        if (*dList == NULL) {
            return;
        }
        ootList = isPath ? ResourceMgr_LoadGfxByName((const char*)*dList) : *dList;
        if (jointTable != NULL) {
            // MM: the animation can ask for an open or closed empty hand.
            s32 handIndex = (limbIndex == PLAYER_LIMB_L_HAND) ? (MERGESHIP_MM_LEFT_HAND_INDEX(jointTable) >> 12)
                                                              : (MERGESHIP_MM_RIGHT_HAND_INDEX(jointTable) >> 8);

            if (handIndex != 0) {
                openClosed = ((handIndex - 1) >= 2) ? 0 : (handIndex - 1);
            }
        }
        composed = MergeShip_MMChild_ComposeHand(play, handType, ootList, limbIndex == PLAYER_LIMB_L_HAND,
                                                 openClosed, &chosen, &withItem, &adultItem);
        if (composed != NULL) {
            *dList = composed;
            MergeShip_MMChild_LogDL(limbIndex, chosen, withItem, adultItem);
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        // Ocarina of Time's sheath list, where Ocarina of Time's sheath limb is.
        MergeShip_MMChild_OoTSheathJoint(pos);
        if (*dList != NULL) {
            Gfx* list = isPath ? ResourceMgr_LoadGfxByName((const char*)*dList) : *dList;

            // The adult's scabbard (and shield) handed to the child (EquipmentAlwaysVisible): under the sheath fit.
            MergeShip_MMChild_InitSkins();
            if ((list != NULL) && MergeShip_MMChild_UsesSkin(list, &sMergeShipOoTAdultSheathSkin)) {
                *dList = MergeShip_MMChild_Fitted(play, list, MMCHILD_FIT_SHEATH);
            }
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        *dList = ResourceMgr_LoadGfxByName(mm_gLinkHumanWaistDL); // MM gPlayerWaistDLs[HUMAN]
    }
}

// ---- the adult's swords and scabbard (MergeShip.MMChildAdultGear) ----------------------------------------

// The sword the child holds, as a PlayerSword (the cutscene sword is the one on B).
static s32 MergeShip_MMChild_HeldSword(Player* this) {
    switch (this->heldItemAction) {
        case PLAYER_IA_SWORD_MASTER:
            return PLAYER_SWORD_MASTER;
        case PLAYER_IA_SWORD_BIGGORON:
            return PLAYER_SWORD_BIGGORON;
        case PLAYER_IA_SWORD_CS:
            return (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ? PLAYER_SWORD_MASTER : PLAYER_SWORD_NONE;
        default:
            return PLAYER_SWORD_NONE;
    }
}

// The sword on B, as a PlayerSword (the Giant's Knife, whole or broken, is Biggoron's Sword's slot).
static s32 MergeShip_MMChild_BSword(void) {
    switch (gSaveContext.equips.buttonItems[0]) {
        case ITEM_SWORD_MASTER:
            return PLAYER_SWORD_MASTER;
        case ITEM_SWORD_BGS:
        case ITEM_SWORD_KNIFE:
            return PLAYER_SWORD_BIGGORON;
        default:
            return PLAYER_SWORD_NONE;
    }
}

// Ocarina of Time's adult list of the left hand holding `sword` for the hand model type `type` (OoT
// sPlayerLeftHandSwordDLs / gPlayerLeftHandBgsDLs, adult column), in place of the child's stand-in (the Kokiri
// Sword, or the Temple of Time's Master Sword for Biggoron's). NULL: the child's list stays.
static const char* MergeShip_MMChild_AdultSwordHand(s32 type, s32 sword, s32 broken, s32 lod) {
    if (!MergeShip_MMChild_AdultGearOn()) {
        return NULL;
    }
    if (((type == PLAYER_MODELTYPE_LH_SWORD) || (type == PLAYER_MODELTYPE_LH_SWORD_2)) &&
        (sword == PLAYER_SWORD_MASTER)) {
        return lod ? gLinkAdultLeftHandHoldingMasterSwordFarDL : gLinkAdultLeftHandHoldingMasterSwordNearDL;
    }
    if ((type == PLAYER_MODELTYPE_LH_BGS) && (sword == PLAYER_SWORD_BIGGORON)) {
        if (broken) {
            return lod ? gLinkAdultHandHoldingBrokenGiantsKnifeFarDL : gLinkAdultHandHoldingBrokenGiantsKnifeDL;
        }
        return lod ? gLinkAdultLeftHandHoldingBgsFarDL : gLinkAdultLeftHandHoldingBgsNearDL;
    }
    return NULL;
}

// An MM shield (MergeShipMMEquipment.h) is the one on the back: MergeShipMMEquipmentDraw.inc draws that sheath.
static s32 MergeShip_MMChild_MMShieldOnBack(s32 shield) {
    return ((shield == PLAYER_SHIELD_HYLIAN) && MergeShip_MMEquip_IsSelected(MMEQUIP_HERO_SHIELD)) ||
           ((shield == PLAYER_SHIELD_MIRROR) && MergeShip_MMEquip_IsSelected(MMEQUIP_MIRROR_SHIELD));
}

// Ocarina of Time's adult list of the right hand holding the Mirror Shield (sPlayerRightHandShieldDLs, adult column)
// for the shield hand, in place of the child's list, which has no Mirror Shield. NULL: the child's list stays (an MM
// shield is drawn by MergeShipMMEquipmentDraw.inc).
static const char* MergeShip_MMChild_AdultShieldHand(s32 type, s32 shield, s32 lod) {
    if (!MergeShip_MMChild_AdultGearOn() || (type != PLAYER_MODELTYPE_RH_SHIELD) ||
        (shield != PLAYER_SHIELD_MIRROR) || MergeShip_MMChild_MMShieldOnBack(shield)) {
        return NULL;
    }
    return lod ? gLinkAdultRightHandHoldingMirrorShieldFarDL : gLinkAdultRightHandHoldingMirrorShieldNearDL;
}

static s8 sMergeShipMMChildLoggedSheath = -1;

// BUG-0152: Biggoron's Sword / the Giant's Knife put away is worn on the back, alone (no scabbard), in the Master
// Sword's place -- as Termina's young Link wears it (2Ship MergeShipOoTEquip.h MERGESHIP_BGS_ON_BACK_*, the same
// option): Ocarina of Time's adult hand list holding it, moved as its Master Sword moves from the hand into the
// scabbard, under the sheath fit.
#define MERGESHIP_BGS_ON_BACK_X (-714.409f)
#define MERGESHIP_BGS_ON_BACK_Y (-310.307f)
#define MERGESHIP_BGS_ON_BACK_Z (77.999f)

static Mtx* MergeShip_MMChild_BgsOnBackMtx(PlayState* play) {
    Mtx* mtx;

    MS_INTERP_PUSH();
    Matrix_Translate(MERGESHIP_BGS_ON_BACK_X, MERGESHIP_BGS_ON_BACK_Y, MERGESHIP_BGS_ON_BACK_Z, MTXMODE_NEW);
    mtx = MATRIX_NEWMTX(play->state.gfxCtx);
    MS_INTERP_POP();
    return mtx;
}

// The sheath limb while the sword on B is the Master Sword or Biggoron's Sword (2Ship MergeShip_OoTEquip_OverrideLimb
// and MergeShip_OoTEquip_BackShieldDL): the adult's scabbard under the sheath fit, holding the Master Sword while it
// is the sword put away (Biggoron's Sword is never sheathed; the scabbard stays empty), and the shield on the back as
// the child carries it -- the Deku Shield and the child's Hylian Shield (without the Kokiri scabbard its list comes
// with), or the adult's Mirror Shield (without his scabbard) under the sheath fit. NULL: the chosen list stays.
// BUG-0086: with any other sword (the Kokiri Sword) the child's own sheath list (`chosen`) stays, and the Mirror Shield
// on his back is added to it -- Ocarina of Time's child column has no Mirror Shield, so it was not drawn at all.
static Gfx* MergeShip_MMChild_ComposeAdultSheath(PlayState* play, s32 lod, s32 sheathType, s32 shield, s32 sword,
                                                 Gfx* chosen) {
    s32 sheathed = (sheathType == PLAYER_MODELTYPE_SHEATH_16) || (sheathType == PLAYER_MODELTYPE_SHEATH_18);
    s32 shieldOnBack = ((sheathType == PLAYER_MODELTYPE_SHEATH_18) || (sheathType == PLAYER_MODELTYPE_SHEATH_19)) &&
                       (shield > PLAYER_SHIELD_NONE) && (shield < PLAYER_SHIELD_MAX);
    Gfx* scabbard;
    Gfx* shieldList = NULL;
    const MMChildSkin* strip = NULL;
    s32 shieldFitted = false;
    s32 tris = 0;
    s32 n = 0;
    Gfx* out;
    const s32 adultSword = (sword == PLAYER_SWORD_MASTER) || (sword == PLAYER_SWORD_BIGGORON);
    const s32 bgsOnBack = sheathed && (sword == PLAYER_SWORD_BIGGORON) &&
                          CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.BgsOnBack"), 1); // BUG-0152

    if (!MergeShip_MMChild_AdultGearOn() || (shieldOnBack && MergeShip_MMChild_MMShieldOnBack(shield))) {
        return NULL;
    }
    if (!adultSword && !(shieldOnBack && (shield == PLAYER_SHIELD_MIRROR) && (chosen != NULL))) {
        return NULL;
    }
    MergeShip_MMChild_InitSkins();

    // OoT sSwordAndSheathDLs / sSheathDLs, adult column (or the child's own, already chosen: BUG-0086).
    if (!adultSword) {
        scabbard = chosen;
    } else if (sheathed && (sword == PLAYER_SWORD_MASTER)) {
        scabbard = ResourceMgr_LoadGfxByName(lod ? gLinkAdultMasterSwordAndSheathFarDL
                                                 : gLinkAdultMasterSwordAndSheathNearDL);
    } else if (bgsOnBack) {
        // the sword alone (its hand list without the hand), in place of the empty scabbard
        scabbard = ResourceMgr_LoadGfxByName(
            MergeShip_MMChild_AdultSwordHand(PLAYER_MODELTYPE_LH_BGS, PLAYER_SWORD_BIGGORON,
                                             (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KNIFE) &&
                                                 (gSaveContext.swordHealth <= 0.0f),
                                             lod));
    } else {
        scabbard = ResourceMgr_LoadGfxByName(lod ? gLinkAdultSheathFarDL : gLinkAdultSheathNearDL);
    }
    if (shieldOnBack) {
        switch (shield) {
            case PLAYER_SHIELD_DEKU:
                shieldList = ResourceMgr_LoadGfxByName(gLinkChildDekuShieldWithMatrixDL);
                break;
            case PLAYER_SHIELD_HYLIAN:
                shieldList = ResourceMgr_LoadGfxByName(lod ? gLinkChildHylianShieldAndSheathFarDL
                                                           : gLinkChildHylianShieldAndSheathNearDL);
                strip = &sMergeShipOoTChildSheathSkin;
                break;
            default: // PLAYER_SHIELD_MIRROR
                shieldList = ResourceMgr_LoadGfxByName(lod ? gLinkAdultMirrorShieldAndSheathFarDL
                                                           : gLinkAdultMirrorShieldAndSheathNearDL);
                strip = &sMergeShipOoTAdultSheathSkin;
                shieldFitted = true;
                break;
        }
    }
    if (scabbard == NULL) {
        return NULL;
    }

    out = Graph_Alloc(play->state.gfxCtx,
                      (((shieldList != NULL) ? MergeShip_MMChild_GfxLength(shieldList) : 0) +
                       (bgsOnBack ? MergeShip_MMChild_GfxLength(scabbard) : 0) + 10) *
                          sizeof(Gfx));
    if (bgsOnBack) {
        gSPMatrix(&out[n], MergeShip_MMChild_FitMtx(play, MMCHILD_FIT_SHEATH), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        n++;
        gSPMatrix(&out[n], MergeShip_MMChild_BgsOnBackMtx(play), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        n++;
        n += MergeShip_MMChild_FilterHand(&out[n], scabbard, &sMergeShipOoTAdultSkin, false, &tris);
        gSPPopMatrix(&out[n], G_MTX_MODELVIEW);
        n++;
        gSPPopMatrix(&out[n], G_MTX_MODELVIEW);
        n++;
    } else if (adultSword) {
        gSPMatrix(&out[n], MergeShip_MMChild_FitMtx(play, MMCHILD_FIT_SHEATH), G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        n++;
        gSPDisplayList(&out[n], scabbard);
        n++;
        gSPPopMatrix(&out[n], G_MTX_MODELVIEW);
        n++;
    } else {
        gSPDisplayList(&out[n], scabbard); // the child's own sheath, where the limb draws it
        n++;
    }
    if (shieldList != NULL) {
        if (shieldFitted) {
            gSPMatrix(&out[n], MergeShip_MMChild_FitMtx(play, MMCHILD_FIT_SHEATH),
                      G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            n++;
        }
        if (strip != NULL) {
            n += MergeShip_MMChild_FilterHand(&out[n], shieldList, strip, false, &tris);
        } else {
            gSPDisplayList(&out[n], shieldList);
            n++;
        }
        if (shieldFitted) {
            gSPPopMatrix(&out[n], G_MTX_MODELVIEW);
            n++;
        }
    }
    gSPEndDisplayList(&out[n]);

    if (sMergeShipMMChildLoggedSheath != (s8)((sword << 4) | (shieldOnBack ? shield : 0) | (sheathed << 3))) {
        sMergeShipMMChildLoggedSheath = (s8)((sword << 4) | (shieldOnBack ? shield : 0) | (sheathed << 3));
        MergeShipTest_Log("MMCHILD adult sheath sword=%d sheathed=%d shieldOnBack=%d", sword, sheathed,
                          shieldOnBack ? shield : 0);
    }
    return out;
}

// Player_OverrideLimbDrawGameplayDefault
void MergeShip_MMChildLink_OverrideLimb(PlayState* play, Player* this, s32 limbIndex, Gfx** dList, Vec3f* pos) {
    Vec3s* jointTable = this->skelAnime.jointTable;
    s32 lod = (sDListsLodOffset != 0) ? 1 : 0;

    if (!MERGESHIP_MM_CHILD(this)) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        // The sword really held, not the child's stand-in: the adult's list, which the hand below keeps without
        // the adult's hand.
        const char* adultHand = MergeShip_MMChild_AdultSwordHand(sLeftHandType, MergeShip_MMChild_HeldSword(this),
                                                                 Player_HoldsBrokenKnife(this), lod);

        if (adultHand != NULL) {
            Gfx* list = ResourceMgr_LoadGfxByName(adultHand);

            if (list != NULL) {
                *dList = list;
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        // The Mirror Shield in the shield hand: the adult's list, which the hand below keeps without the adult's hand.
        const char* adultHand = MergeShip_MMChild_AdultShieldHand(sRightHandType, this->currentShield, lod);

        if (adultHand != NULL) {
            Gfx* list = ResourceMgr_LoadGfxByName(adultHand);

            if (list != NULL) {
                *dList = list;
            }
        }
    }
    // MM Player_OverrideLimbDrawGameplayDefault asks the animation for the hand shape only after its own
    // choices: not for an open hand closed while running, nor for the shield hand.
    if (((limbIndex == PLAYER_LIMB_L_HAND) && (this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) &&
         (sLeftHandType == PLAYER_MODELTYPE_LH_CLOSED)) ||
        ((limbIndex == PLAYER_LIMB_R_HAND) &&
         ((sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) ||
          ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (sRightHandType == PLAYER_MODELTYPE_RH_CLOSED))))) {
        jointTable = NULL;
    }
    MergeShip_MMChild_OverrideLimbImpl(play, limbIndex, dList, pos,
                                       (limbIndex == PLAYER_LIMB_L_HAND) ? sLeftHandType : sRightHandType,
                                       jointTable, false);
    if (limbIndex == PLAYER_LIMB_SHEATH) {
        Gfx* sheath = MergeShip_MMChild_ComposeAdultSheath(play, lod, this->sheathType, this->currentShield,
                                                           MergeShip_MMChild_BSword(), *dList);

        if (sheath != NULL) {
            *dList = sheath;
        }
    }
}

// Player_OverrideLimbDrawPause (the equipment screen's child). `swordAndShield`: the screen's PlayerSword and
// PlayerShield.
void MergeShip_MMChildLink_OverrideLimbPause(PlayState* play, s32 limbIndex, s32 type, Gfx** dList, Vec3f* pos,
                                             u8* swordAndShield) {
    if (!gMergeShipMMChildLink || (gSaveContext.linkAge != LINK_AGE_CHILD)) {
        return;
    }
    if ((limbIndex == PLAYER_LIMB_L_HAND) && (swordAndShield != NULL)) {
        // Player_OverrideLimbDrawPause: the broken knife while the sword has no health left.
        const char* adultHand =
            MergeShip_MMChild_AdultSwordHand(type, swordAndShield[0], gSaveContext.swordHealth <= 0.0f, 0);

        if (adultHand != NULL) {
            *dList = (Gfx*)adultHand; // a path, as the screen's own table entries are
        }
    } else if ((limbIndex == PLAYER_LIMB_R_HAND) && (swordAndShield != NULL)) {
        const char* adultHand = MergeShip_MMChild_AdultShieldHand(type, swordAndShield[1], 0);

        if (adultHand != NULL) {
            *dList = (Gfx*)adultHand;
        }
    }
    Gfx* const before = *dList;

    MergeShip_MMChild_OverrideLimbImpl(play, limbIndex, dList, pos, type, NULL, true);
    if ((limbIndex == PLAYER_LIMB_SHEATH) && (swordAndShield != NULL)) {
        // The screen's lists are paths until a fit replaces one with its own list.
        Gfx* chosen = ((*dList != NULL) && (*dList == before)) ? ResourceMgr_LoadGfxByName((const char*)*dList)
                                                               : *dList;
        Gfx* sheath = MergeShip_MMChild_ComposeAdultSheath(play, 0, type, swordAndShield[1], swordAndShield[0],
                                                           chosen);

        if (sheath != NULL) {
            *dList = sheath;
        }
    }
}

// Player_PostLimbDrawGameplay, on the right hand's matrix before the string's own placement: the adult's bow string
// (sBowStringData[0]) follows the adult's bow, which MM's young Link holds under the right hand's fit (SoH's
// BowSlingshotAmmoFix / EquipmentAlwaysVisible hand the child the adult's bow).
void MergeShip_MMChildLink_FitAdultBowString(Player* this, s32 stringModel) {
    f32 f[6];

    if (!MERGESHIP_MM_CHILD(this) || (stringModel != 0)) {
        return;
    }
    MergeShip_MMChild_GetFit(MMCHILD_FIT_HAND_R, f);
    Matrix_Translate(f[0], f[1], f[2], MTXMODE_APPLY);
    Matrix_Scale(f[3], f[4], f[5], MTXMODE_APPLY);
}

// ---- the gauntlet plates (MergeShip.MMChildAdultGear) ----------------------------------------------------
//
// Ocarina of Time draws the plates after the skeleton (Player_DrawImpl); each plate list loads its limb's matrix
// from segment 0x0D, where SkelAnime_DrawFlexLod left the adult's limb matrices. On MM's young Link a plate is
// modelled for the adult's bigger forearm and hand, so it is drawn on MM's limb under the arm fit (2Ship
// MergeShip_OoTEquip_PostLimbDraw). The fitted limb matrices are made while the skeleton is drawn (the limb's own
// matrix times the fit), and each plate list is copied with its segment-0x0D matrix loads pointing straight at the
// fitted matrix: no segment is borrowed, so nothing needs restoring, and 0x0D keeps the skeleton's matrices for
// anything after (the Goron's Bracelet list reads it).

enum {
    MMCHILD_ARM_L_FOREARM,
    MMCHILD_ARM_L_HAND,
    MMCHILD_ARM_R_FOREARM,
    MMCHILD_ARM_R_HAND,
    MMCHILD_FOOT_L, // BUG-0099: the Iron and Hover Boots
    MMCHILD_FOOT_R,
    MMCHILD_ARM_MAX
};

static Mtx* sMergeShipMMChildArmMtx[MMCHILD_ARM_MAX];
static u32 sMergeShipMMChildArmFrame[MMCHILD_ARM_MAX];

// Player_OverrideLimbDrawGameplayDefault / Player_OverrideLimbDrawPause, with the limb's final pos and rot (the
// matrix on the stack is its parent's).
void MergeShip_MMChildLink_CaptureArm(PlayState* play, s32 isMMChild, s32 limbIndex, Vec3f* pos, Vec3s* rot) {
    s32 slot;
    f32 f[6];

    switch (limbIndex) {
        case PLAYER_LIMB_L_FOREARM:
            slot = MMCHILD_ARM_L_FOREARM;
            break;
        case PLAYER_LIMB_L_HAND:
            slot = MMCHILD_ARM_L_HAND;
            break;
        case PLAYER_LIMB_R_FOREARM:
            slot = MMCHILD_ARM_R_FOREARM;
            break;
        case PLAYER_LIMB_R_HAND:
            slot = MMCHILD_ARM_R_HAND;
            break;
        case PLAYER_LIMB_L_FOOT:
            slot = MMCHILD_FOOT_L;
            break;
        case PLAYER_LIMB_R_FOOT:
            slot = MMCHILD_FOOT_R;
            break;
        default:
            return;
    }
    if (!isMMChild || !gMergeShipMMChildLink || !MergeShip_MMChild_AdultGearOn()) {
        return;
    }
    if ((slot >= MMCHILD_FOOT_L) ? (CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS) <= EQUIP_VALUE_BOOTS_KOKIRI)
                                 : (Player_GetStrength() < PLAYER_STR_SILVER_G)) {
        return;
    }
    MergeShip_MMChild_GetFit((slot == MMCHILD_FOOT_R)   ? MMCHILD_FIT_FOOT_R
                             : (slot >= MMCHILD_FOOT_L) ? MMCHILD_FIT_FOOT
                                                        : MMCHILD_FIT_ARM,
                             f);
    MS_INTERP_PUSH();
    Matrix_TranslateRotateZYX(pos, rot);
    Matrix_Translate(f[0], f[1], f[2], MTXMODE_APPLY);
    Matrix_Scale(f[3], f[4], f[5], MTXMODE_APPLY);
    sMergeShipMMChildArmMtx[slot] = MATRIX_NEWMTX(play->state.gfxCtx);
    sMergeShipMMChildArmFrame[slot] = play->state.frames;
    MS_INTERP_POP();
}

// A copy of plate list `path` whose segment-0x0D matrix loads load `mtx`.
static Gfx* MergeShip_MMChild_PlateOnLimb(PlayState* play, const char* path, Mtx* mtx, s32 logIndex) {
    static u8 sLogged[6];
    Gfx* src = ResourceMgr_LoadGfxByName(path);
    Gfx* out;
    s32 len;
    s32 i;
    s32 patched = 0;

    if (src == NULL) {
        return NULL;
    }
    len = MergeShip_MMChild_GfxLength(src);
    out = Graph_Alloc(play->state.gfxCtx, len * sizeof(Gfx));
    for (i = 0; i < len; i++) {
        u8 op = (u8)(src[i].words.w0 >> 24);

        out[i] = src[i];
        // F3DEX2 G_MTX from a segmented address (segment in the top byte; LUS marks segmented addresses odd).
        if ((op == G_MTX) && ((((uintptr_t)src[i].words.w1) >> 24) == 0x0D)) {
            out[i].words.w1 = (uintptr_t)mtx;
            patched++;
        }
        if (MergeShip_MMChild_GfxIsWide(op) && (i + 1 < len)) {
            i++;
            out[i] = src[i];
        }
    }
    if ((logIndex >= 0) && (logIndex < ARRAY_COUNT(sLogged)) && !sLogged[logIndex]) {
        sLogged[logIndex] = true;
        MergeShipTest_Log("MMCHILD gauntlet plate %s size=%d matrices=%d", path + 7, len, patched);
    }
    return out;
}

// Player_DrawImpl, after the skeleton: the Silver or Golden Gauntlets on MM's young Link. True when drawn (the
// Goron's Bracelet is then not).
s32 MergeShip_MMChildLink_DrawGauntlets(PlayState* play, s32 form, s32 strength) {
    Mtx* mtx[MMCHILD_ARM_MAX];
    Color_RGB8* color;
    Color_RGB8 custom;
    Gfx* plate;
    s32 ok = (form == PLAYER_FORM_CHILD) && gMergeShipMMChildLink && MergeShip_MMChild_AdultGearOn() &&
             (strength >= PLAYER_STR_SILVER_G) && (strength <= PLAYER_STR_GOLD_G);
    s32 i;

    // Matrices made while this skeleton was drawn, this frame; used once.
    for (i = 0; i < MMCHILD_FOOT_L; i++) {
        mtx[i] = sMergeShipMMChildArmMtx[i];
        if ((mtx[i] == NULL) || (sMergeShipMMChildArmFrame[i] != play->state.frames)) {
            ok = false;
        }
        sMergeShipMMChildArmMtx[i] = NULL;
    }
    if (!ok) {
        return false;
    }

    // Player_DrawImpl: sGauntletColors, with SoH's gauntlet cosmetics.
    color = &sGauntletColors[strength - PLAYER_STR_SILVER_G];
    if ((strength == PLAYER_STR_SILVER_G) && CVarGetInteger(CVAR_COSMETIC("Gloves.SilverGauntlets.Changed"), 0)) {
        custom = CVarGetColor24(CVAR_COSMETIC("Gloves.SilverGauntlets.Value"), *color);
        color = &custom;
    } else if ((strength == PLAYER_STR_GOLD_G) &&
               CVarGetInteger(CVAR_COSMETIC("Gloves.GoldenGauntlets.Changed"), 0)) {
        custom = CVarGetColor24(CVAR_COSMETIC("Gloves.GoldenGauntlets.Value"), *color);
        color = &custom;
    }

    OPEN_DISPS(play->state.gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);
    plate = MergeShip_MMChild_PlateOnLimb(play, gLinkAdultLeftGauntletPlate1DL, mtx[MMCHILD_ARM_L_FOREARM], 0);
    if (plate != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, plate);
    }
    plate = MergeShip_MMChild_PlateOnLimb(play, gLinkAdultRightGauntletPlate1DL, mtx[MMCHILD_ARM_R_FOREARM], 1);
    if (plate != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, plate);
    }
    plate = (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                ? MergeShip_MMChild_PlateOnLimb(play, gLinkAdultLeftGauntletPlate2DL, mtx[MMCHILD_ARM_L_HAND], 2)
                : MergeShip_MMChild_PlateOnLimb(play, gLinkAdultLeftGauntletPlate3DL, mtx[MMCHILD_ARM_L_HAND], 3);
    if (plate != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, plate);
    }
    plate = (sRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                ? MergeShip_MMChild_PlateOnLimb(play, gLinkAdultRightGauntletPlate2DL, mtx[MMCHILD_ARM_R_HAND], 4)
                : MergeShip_MMChild_PlateOnLimb(play, gLinkAdultRightGauntletPlate3DL, mtx[MMCHILD_ARM_R_HAND], 5);
    if (plate != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, plate);
    }

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

// BUG-0099: Player_DrawImpl, after the skeleton: the Iron or Hover Boots (`boots` 1 or 2, as sBootDListGroups) on MM's
// young Link, the adult's boots fitted to his feet (2Ship MergeShip_OoTEquip_PostLimbDraw draws the same in Termina).
void MergeShip_MMChildLink_DrawBoots(PlayState* play, s32 form, s32 boots) {
    static const char* sBoots[2][2] = {
        { gLinkAdultLeftIronBootDL, gLinkAdultRightIronBootDL },
        { gLinkAdultLeftHoverBootDL, gLinkAdultRightHoverBootDL },
    };
    Mtx* left = sMergeShipMMChildArmMtx[MMCHILD_FOOT_L];
    Mtx* right = sMergeShipMMChildArmMtx[MMCHILD_FOOT_R];
    s32 ok = (form == PLAYER_FORM_CHILD) && gMergeShipMMChildLink && MergeShip_MMChild_AdultGearOn() &&
             (boots >= 1) && (boots <= 2) && (left != NULL) && (right != NULL) &&
             (sMergeShipMMChildArmFrame[MMCHILD_FOOT_L] == play->state.frames) &&
             (sMergeShipMMChildArmFrame[MMCHILD_FOOT_R] == play->state.frames);
    Gfx* boot;

    sMergeShipMMChildArmMtx[MMCHILD_FOOT_L] = sMergeShipMMChildArmMtx[MMCHILD_FOOT_R] = NULL;
    if (!ok) {
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    boot = MergeShip_MMChild_PlateOnLimb(play, sBoots[boots - 1][0], left, -1);
    if (boot != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, boot);
    }
    boot = MergeShip_MMChild_PlateOnLimb(play, sBoots[boots - 1][1], right, -1);
    if (boot != NULL) {
        gSPDisplayList(POLY_OPA_DISP++, boot);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

// ---- the tunic ------------------------------------------------------------------------------------------

// MM's human is green through a gDPSetPrimColor(.., 30, 105, 27, 255) inside its body lists (the one 2S2H's
// cosmetic editor patches), where Ocarina of Time's child takes its tunic color from the environment color.
// The child wears Ocarina of Time's tunic, so those primitive colors are set to the color Player_DrawImpl
// chose (the equipped tunic, with SoH's tunic cosmetics applied).
#define MERGESHIP_MM_HUMAN_TUNIC_PRIM 0x1E691BFF

static const char* sMergeShipMMHumanBodyDLs[] = {
    mm_gLinkHumanWaistDL,     mm_gLinkHumanRightThighDL, mm_gLinkHumanRightShinDL,     mm_gLinkHumanRightFootDL,
    mm_gLinkHumanLeftThighDL, mm_gLinkHumanLeftShinDL,   mm_gLinkHumanLeftFootDL,      mm_gLinkHumanHeadDL,
    mm_gLinkHumanHatDL,       mm_gLinkHumanCollarDL,     mm_gLinkHumanLeftShoulderDL,  mm_gLinkHumanLeftForearmDL,
    mm_gLinkHumanRightShoulderDL, mm_gLinkHumanRightForearmDL, mm_gLinkHumanTorsoDL,
};

typedef struct {
    u8 dl;
    u16 index;
} MMChildTunicSite;

static MMChildTunicSite sMergeShipMMChildTunicSites[32];
static s32 sMergeShipMMChildTunicSiteCount = -1;

void MergeShip_MMChildLink_SetTunicColor(Color_RGB8* color) {
    u32 want = ((u32)color->r << 24) | ((u32)color->g << 16) | ((u32)color->b << 8) | 0xFF;
    s32 i;

    if (sMergeShipMMChildTunicSiteCount < 0) {
        sMergeShipMMChildTunicSiteCount = 0;
        for (i = 0; i < ARRAY_COUNT(sMergeShipMMHumanBodyDLs); i++) {
            Gfx* dl = ResourceMgr_LoadGfxByName(sMergeShipMMHumanBodyDLs[i]);
            s32 index;

            for (index = 0; (dl != NULL) && ((u8)(dl[index].words.w0 >> 24) != G_ENDDL); index++) {
                u8 op = (u8)(dl[index].words.w0 >> 24);

                if ((op == G_SETPRIMCOLOR) && ((u32)dl[index].words.w1 == MERGESHIP_MM_HUMAN_TUNIC_PRIM) &&
                    (sMergeShipMMChildTunicSiteCount < ARRAY_COUNT(sMergeShipMMChildTunicSites))) {
                    sMergeShipMMChildTunicSites[sMergeShipMMChildTunicSiteCount].dl = (u8)i;
                    sMergeShipMMChildTunicSites[sMergeShipMMChildTunicSiteCount].index = (u16)index;
                    sMergeShipMMChildTunicSiteCount++;
                } else if (MergeShip_MMChild_GfxIsWide(op)) {
                    index++;
                }
            }
        }
        MergeShipTest_Log("MMCHILD tunic prim sites=%d", sMergeShipMMChildTunicSiteCount);
    }
    for (i = 0; i < sMergeShipMMChildTunicSiteCount; i++) {
        Gfx* dl = ResourceMgr_LoadGfxByName(sMergeShipMMHumanBodyDLs[sMergeShipMMChildTunicSites[i].dl]);

        if ((dl != NULL) && ((u8)(dl[sMergeShipMMChildTunicSites[i].index].words.w0 >> 24) == G_SETPRIMCOLOR)) {
            dl[sMergeShipMMChildTunicSites[i].index].words.w1 = want;
        }
    }
}

// MM sPlayerFirstPersonRightShoulderDLs[PLAYER_FORM_HUMAN]. The child's first-person arm itself is Ocarina of
// Time's slingshot arm (its forearm, hand and slingshot are one list).
void MergeShip_MMChildLink_OverrideLimbFirstPerson(Player* this, s32 limbIndex, Gfx** dList) {
    if (MERGESHIP_MM_CHILD(this) && (limbIndex == PLAYER_LIMB_R_SHOULDER) && (*dList != NULL)) {
        *dList = ResourceMgr_LoadGfxByName(mm_gLinkHumanRightShoulderDL);
    }
}
