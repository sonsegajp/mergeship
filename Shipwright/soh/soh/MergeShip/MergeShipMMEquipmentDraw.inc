// MergeShip2 -- Majora's Mask's equipment drawn on Link (see MergeShipMMEquipment.h).
//
// Included at the end of z_player_lib.c, after MergeShipChildLinkDraw.inc. MM draws an item held in a hand as the
// bare hand followed by the item (gLinkHumanLeftHandHoldingRazorSwordDL = gLinkHumanLeftHandClosedDL +
// gRazorSwordDL), and its sheath and back shield on the sheath limb (MM Player_OverrideLimbDrawGameplayDefault /
// Player_PostLimbDrawGameplay: gPlayerSheathedSwords, gPlayerSwordSheaths, gPlayerShields). The same lists are drawn
// here on each of the three models young and adult Link can be:
//   - MM's human as the child (MergeShipChildLink.h): MM's own lists, as MM draws them;
//   - Ocarina of Time's child: the child's own bare fist and MM's item list. MM's human skeleton is Ocarina of Time's
//     child skeleton (same joints; MM's closed fist and Ocarina of Time's have identical geometry), so MM's items sit
//     in Ocarina of Time's child hands as modelled;
//   - adult Link: the adult's bare fist and MM's item list under a fit (translation and scale in the hand's space).
//     The fits below were measured, not guessed: from oot.o2r / mm.o2r vertex data, the child's fist and grip against
//     the adult's (hand size ratio 1.35-1.52; the Master Sword's blade line against the Kokiri Sword's; the Hylian
//     Shield's centre against the Hero's Shield's; the Master Sword's scabbard mouth against the child's). Each value
//     can be tuned with gMergeShip.MMEquipFit.<Left|Right|Sheath>.<X|Y|Z|Scale>.
// Every list is loaded through ResourceMgr_LoadGfxByName (MergeShipAssets.h).

#include <stdio.h>
#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "mm/objects/object_link_child/object_link_child.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"

// MergeShip2: frame interpolation pairs a frame's matrix operations with the next frame's in order, so a block that is
// drawn on some frames only (a mask on the head, a sword fitted to a hand, a form's extra part) would shift every
// operation after it and stretch a limb for a frame. Each Matrix_Push .. Matrix_Pop here is its own interpolation
// child (SoH's FrameInterpolation_RecordOpenChild), keyed by where it is.
void FrameInterpolation_RecordOpenChild(const void* a, int b);
void FrameInterpolation_RecordCloseChild(void);
#define MS_INTERP_PUSH() (FrameInterpolation_RecordOpenChild(__FILE__, __LINE__), Matrix_Push())
#define MS_INTERP_POP() (Matrix_Pop(), FrameInterpolation_RecordCloseChild())

typedef enum {
    /* -1 */ MSEQ_MODEL_NONE = -1,
    /*  0 */ MSEQ_MODEL_MM_CHILD,
    /*  1 */ MSEQ_MODEL_OOT_CHILD,
    /*  2 */ MSEQ_MODEL_ADULT
} MSEqModel;

typedef enum {
    /* 0 */ MSEQ_PART_LEFT,   // swords
    /* 1 */ MSEQ_PART_RIGHT,  // shields, bow, hookshot
    /* 2 */ MSEQ_PART_SHEATH, // sheaths, sheathed hilts
    /* 3 */ MSEQ_PART_HOOKSHOT, // MM's Hookshot (the right hand, its own fit)
    /* 4 */ MSEQ_PART_BACK_SHIELD, // MM's shields on the back (BUG-0082: the sheath's fit made them huge)
    /* 5 */ MSEQ_PART_MAX
} MSEqPart;

typedef struct {
    f32 x, y, z, scale;
} MSEqFit;

// Adult Link's fits (hand / sheath limb space; see the top of the file).
static const MSEqFit sMSEqAdultFits[MSEQ_PART_MAX] = {
    { 0.0f, -15.0f, -60.0f, 1.45f },  // MSEQ_PART_LEFT
    { 10.0f, 40.0f, 40.0f, 1.3f },    // MSEQ_PART_RIGHT
    { -160.0f, 10.0f, 0.0f, 1.45f },  // MSEQ_PART_SHEATH
    // MM's Hookshot on Ocarina of Time's hookshot, measured from both models' vertices (mm.o2r gLinkHumanHookshotDL,
    // oot.o2r gLinkAdultRightHandHoldingHookshotNearDL): both barrels run along the hand's +Y, so no rotation -- MM's
    // barrel axis moved onto OoT's (x 97, z 34), scaled to OoT's grip-to-muzzle length (1.85), the hook at OoT's hook
    // height (y 1640). Both hookshots fill the same place in first person (the user, 2026-09-30); 2Ship's adult uses
    // the same numbers (MergeShipAdultLink.c MS_ADULT_FIT_HOOKSHOT).
    { 100.7f, 65.6f, -10.5f, 1.85f }, // MSEQ_PART_HOOKSHOT
    // MM's shields on the adult's back the size of Ocarina of Time's Mirror Shield there (BUG-0082, measured from
    // behind: build/tests/v1010_bug0082), where it sits (+X runs down the back).
    { 240.0f, 10.0f, 0.0f, 1.12f }, // MSEQ_PART_BACK_SHIELD
};
static const char* sMSEqFitNames[MSEQ_PART_MAX] = { "Left", "Right", "Sheath", "Hookshot", "BackShield" };

// Player_DrawImpl's tunic environment colour (MM's Mirror Shield lists change the environment colour, which Ocarina
// of Time's torso, drawn after the right hand, takes its tunic colour from).
static Color_RGB8 sMSEqTunicEnv = { 255, 255, 255 };

void MergeShip_MMEquip_SetTunicEnvColor(u8 r, u8 g, u8 b) {
    sMSEqTunicEnv.r = r;
    sMSEqTunicEnv.g = g;
    sMSEqTunicEnv.b = b;
}

static s32 MSEq_ModelOf(Player* player) {
    // Dark Link keeps Ocarina of Time's equipment; an Anchor teammate's dummy wears his own (BUG-0077).
    if ((player->actor.id != ACTOR_PLAYER) && !MergeShip_MMEquip_DrawOverrideActive()) {
        return MSEQ_MODEL_NONE;
    }
    if (player->mergeShipForm == PLAYER_FORM_ADULT) {
        return MSEQ_MODEL_ADULT;
    }
    if (player->mergeShipForm == PLAYER_FORM_CHILD) {
        return gMergeShipMMChildLink ? MSEQ_MODEL_MM_CHILD : MSEQ_MODEL_OOT_CHILD;
    }
    return MSEQ_MODEL_NONE; // an MM form draws its own hands
}

static void MSEq_GetFit(s32 model, s32 part, MSEqFit* out) {
    char name[64];

    if (model != MSEQ_MODEL_ADULT) {
        out->x = out->y = out->z = 0.0f;
        out->scale = 1.0f;
        return;
    }
    snprintf(name, sizeof(name), "gMergeShip.MMEquipFit.%s.X", sMSEqFitNames[part]);
    out->x = CVarGetFloat(name, sMSEqAdultFits[part].x);
    snprintf(name, sizeof(name), "gMergeShip.MMEquipFit.%s.Y", sMSEqFitNames[part]);
    out->y = CVarGetFloat(name, sMSEqAdultFits[part].y);
    snprintf(name, sizeof(name), "gMergeShip.MMEquipFit.%s.Z", sMSEqFitNames[part]);
    out->z = CVarGetFloat(name, sMSEqAdultFits[part].z);
    snprintf(name, sizeof(name), "gMergeShip.MMEquipFit.%s.Scale", sMSEqFitNames[part]);
    out->scale = CVarGetFloat(name, sMSEqAdultFits[part].scale);
}

// A point of MM's model, in the limb's space of `model`.
static void MSEq_FitPoint(s32 model, s32 part, Vec3f* mmPoint, Vec3f* out) {
    MSEqFit fit;

    MSEq_GetFit(model, part, &fit);
    out->x = (mmPoint->x * fit.scale) + fit.x;
    out->y = (mmPoint->y * fit.scale) + fit.y;
    out->z = (mmPoint->z * fit.scale) + fit.z;
}

// BUG-0102: MM's Hero's Bow aimed in first person by the adult, laid on Ocarina of Time's bow: MM's belly (-23, 588)
// on Ocarina of Time's (10, 394), MM's upper tip (1279, -441) on Ocarina of Time's (2010, -309) -- MM's bow is
// shorter and bent deeper, so under the right hand's fit its upper limb ran 20 degrees off Ocarina of Time's. X, Y, Z,
// scale and a turn about Z in degrees; 2Ship's adult uses the same numbers and CVars
// (MergeShipAdultLink.c MergeShip_AdultLink_HeroBowFPValues).
static void MSEq_HeroBowFPValues(f32 v[5]) {
    static const char* sAxes[5] = { "X", "Y", "Z", "S", "R" };
    static const f32 sDefaults[5] = { 281.2f, -307.5f, -10.0f, 1.278f, 18.9f };
    char name[64];
    s32 i;

    for (i = 0; i < 5; i++) {
        snprintf(name, sizeof(name), "gMergeShip.AdultFit.HeroBowFP.%s", sAxes[i]);
        v[i] = CVarGetFloat(name, sDefaults[i]);
    }
}

// ---- composed display lists ---------------------------------------------------------------------------------------

#define MSEQ_DL_MAX 24

typedef struct {
    Gfx* start;
    Gfx* p;
} MSEqDL;

static void MSEq_Begin(PlayState* play, MSEqDL* dl) {
    dl->start = dl->p = Graph_Alloc(play->state.gfxCtx, MSEQ_DL_MAX * sizeof(Gfx));
}

static void MSEq_Call(MSEqDL* dl, const char* path) {
    Gfx* gfx;

    if (path == NULL) {
        return;
    }
    gfx = ResourceMgr_LoadGfxByName(path);
    if (gfx != NULL) {
        gSPDisplayList(dl->p++, gfx);
    }
}

// Pushes `model`'s fit for `part`, then a translation (MM's own placement of a part, e.g. a sheathed hilt).
static void MSEq_PushFit(PlayState* play, MSEqDL* dl, s32 model, s32 part, f32 tx, f32 ty, f32 tz) {
    MSEqFit fit;
    Mtx* mtx;

    MSEq_GetFit(model, part, &fit);
    // BUG-0041: built directly, not through the matrix stack (as 2Ship's MergeShip_AdultLink_FitMtx): the fit is a
    // constant multiplied onto the limb, which is interpolated already. Recorded, every fit of a frame shared one
    // interpolation key (this line), so when the parts drawn changed between frames (Link turning fast, the LOD
    // switching) they were paired with the wrong ones and the gear jumped. Translate, scale, translate:
    // translation = fit + scale * t, the scale on the diagonal.
    {
        MtxF mf = { {
            { fit.scale, 0.0f, 0.0f, 0.0f },
            { 0.0f, fit.scale, 0.0f, 0.0f },
            { 0.0f, 0.0f, fit.scale, 0.0f },
            { fit.x + fit.scale * tx, fit.y + fit.scale * ty, fit.z + fit.scale * tz, 1.0f },
        } };

        mtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
        guMtxF2L(mf.mf, mtx);
    }
    gSPMatrix(dl->p++, mtx, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
}

static void MSEq_Pop(MSEqDL* dl) {
    gSPPopMatrix(dl->p++, G_MTX_MODELVIEW);
}

static Gfx* MSEq_End(MSEqDL* dl) {
    gDPPipeSync(dl->p++);
    gDPSetEnvColor(dl->p++, sMSEqTunicEnv.r, sMSEqTunicEnv.g, sMSEqTunicEnv.b, 0);
    gSPEndDisplayList(dl->p++);
    return dl->start;
}

// MM's list for an item in hand (MM gLinkHuman*Holding*DL without its hand).
static void MSEq_ItemLists(s32 id, const char** a, const char** b) {
    *a = *b = NULL;
    switch (id) {
        case MMEQUIP_RAZOR:
            *a = mm_gRazorSwordDL;
            break;
        case MMEQUIP_KOKIRI:
            *a = mm_gKokiriSwordDL; // MM's own Kokiri Sword (2026-09-24)
            break;
        case MMEQUIP_GILDED:
            *a = mm_gLinkHumanGildedSwordHandleDL;
            *b = mm_gLinkHumanGildedSwordBladeDL;
            break;
        case MMEQUIP_GREAT_FAIRY:
            *a = mm_gLinkHumanGreatFairysSwordDL;
            break;
        case MMEQUIP_HERO_SHIELD:
            *a = mm_gLinkHumanHerosShieldDL;
            break;
        case MMEQUIP_MIRROR_SHIELD:
            *a = mm_gLinkHumanMirrorShieldDL;
            break;
        case MMEQUIP_HOOKSHOT:
            *a = mm_gLinkHumanHookshotDL;
            break;
        case MMEQUIP_BOW:
            *a = mm_gLinkHumanBowDL;
            break;
        default:
            break;
    }
}

// A hand holding MM item `id`, for `model` (lod 0 near, 1 far).
static Gfx* MSEq_ComposeHand(PlayState* play, s32 model, s32 lod, s32 isLeft, s32 id) {
    static const char* sLeftFists[3][2] = {
        { mm_gLinkHumanLeftHandClosedDL, mm_gLinkHumanLeftHandClosedDL },
        { gLinkChildLeftFistNearDL, gLinkChildLeftFistFarDL },
        { gLinkAdultLeftHandClosedNearDL, gLinkAdultLeftHandClosedFarDL },
    };
    static const char* sRightFists[3][2] = {
        { mm_gLinkHumanRightHandClosedDL, mm_gLinkHumanRightHandClosedDL },
        { gLinkChildRightHandClosedNearDL, gLinkChildRightHandClosedFarDL },
        { gLinkAdultRightHandClosedNearDL, gLinkAdultRightHandClosedFarDL },
    };
    const char* itemA;
    const char* itemB;
    MSEqDL dl;

    MSEq_ItemLists(id, &itemA, &itemB);
    MSEq_Begin(play, &dl);
    MSEq_Call(&dl, isLeft ? sLeftFists[model][lod] : sRightFists[model][lod]);
    if (model == MSEQ_MODEL_ADULT) {
        // Each shield's centre on the centre of the Ocarina of Time shield it pairs with, held in the adult's hand
        // (oot.o2r gLinkAdultRightHandHolding<Hylian|Mirror>ShieldNearDL less the hand, against mm.o2r's lists under
        // the right-hand fit): the MM Mirror Shield sat 184 units higher than Ocarina of Time's, the Hero's Shield a
        // little off the Hylian Shield (BUG-0002). Height and width only; its depth stays in the grip. MM units.
        f32 tx = 0.0f, ty = 0.0f;
        if (id == MMEQUIP_MIRROR_SHIELD) {
            tx = -141.5f, ty = 12.3f;
        } else if (id == MMEQUIP_HERO_SHIELD) {
            tx = 7.7f, ty = 20.8f;
        }
        MSEq_PushFit(play, &dl, model,
                     isLeft ? MSEQ_PART_LEFT : ((id == MMEQUIP_HOOKSHOT) ? MSEQ_PART_HOOKSHOT : MSEQ_PART_RIGHT), tx, ty,
                     0.0f);
    }
    MSEq_Call(&dl, itemA);
    MSEq_Call(&dl, itemB);
    if (model == MSEQ_MODEL_ADULT) {
        MSEq_Pop(&dl);
    }
    return MSEq_End(&dl);
}

// MM D_801C018C / gPlayerSheathedSwords / gPlayerSwordSheaths / gPlayerShields, and where MM puts a sheathed hilt
// (gLinkHumanSheathedRazorSwordMtx, gLinkHumanSheathedKokiriSwordMtx for the Gilded Sword; read from mm.o2r).
static const char* MSEq_SheathedSword(s32 id) {
    return (id == MMEQUIP_RAZOR)    ? mm_gLinkHumanSheathedRazorSwordDL
           : (id == MMEQUIP_KOKIRI) ? mm_gLinkHumanSheathedKokiriSwordDL
                                    : mm_gLinkHumanSheathedGildedSwordDL;
}
static const char* MSEq_EmptySheath(s32 id) {
    return (id == MMEQUIP_RAZOR)    ? mm_gLinkHumanRazorSwordSheathDL
           : (id == MMEQUIP_KOKIRI) ? mm_gLinkHumanKokiriSwordSheathDL
                                    : mm_gLinkHumanGildedSwordSheathDL;
}
static const char* MSEq_BackShield(s32 id) {
    return (id == MMEQUIP_HERO_SHIELD) ? mm_gLinkHumanHerosShieldWithMtxDL : mm_gLinkHumanMirrorShieldWithMtxDL;
}
static void MSEq_SheathedHilt(s32 id, const char** hilt, Vec3f* pos) {
    if (id == MMEQUIP_RAZOR) {
        *hilt = mm_gRazorSwordHandleDL;
        pos->x = -480.0f;
        pos->y = -240.0f;
        pos->z = -20.0f;
    } else {
        // MM's Kokiri Sword and the Gilded Sword share gLinkHumanSheathedKokiriSwordMtx's place.
        *hilt = (id == MMEQUIP_KOKIRI) ? mm_gKokiriSwordHandleDL : mm_gLinkHumanGildedSwordHandleDL;
        pos->x = -578.0f;
        pos->y = -221.0f;
        pos->z = -32.0f;
    }
}

// Ocarina of Time's shield-on-the-back list without Ocarina of Time's scabbard in it (its scabbard's texture is the
// empty scabbard's, MergeShipChildLinkDraw.inc's sheath skins), for an MM sword's own scabbard beside it (BUG-0092).
static Gfx* MSEq_ShieldWithoutScabbard(PlayState* play, const char* path, s32 adult) {
    Gfx* src = (path != NULL) ? ResourceMgr_LoadGfxByName(path) : NULL;
    const MMChildSkin* strip;
    Gfx* out;
    s32 tris = 0;
    s32 n;

    if (src == NULL) {
        return NULL;
    }
    MergeShip_MMChild_InitSkins();
    strip = adult ? &sMergeShipOoTAdultSheathSkin : &sMergeShipOoTChildSheathSkin;
    if (!MergeShip_MMChild_UsesSkin(src, strip)) {
        return src;
    }
    out = Graph_Alloc(play->state.gfxCtx, (MergeShip_MMChild_GfxLength(src) + 2) * sizeof(Gfx));
    n = MergeShip_MMChild_FilterHand(out, src, strip, false, &tris);
    gSPEndDisplayList(&out[n]);
    return out;
}

// The sheath limb: `swordId` an MM sword (-1: Ocarina of Time's), `shieldId` an MM shield on the back (-1: Ocarina of
// Time's or none), with Ocarina of Time's sheath model type and shield. `form`/`lod` index Ocarina of Time's tables.
// `*allMM` tells whether every part drawn is MM's (MM's human then keeps MM's own sheath joint).
static Gfx* MSEq_ComposeSheath(PlayState* play, s32 model, s32 lod, s32 form, s32 sheathType, s32 shield, s32 swordId,
                               s32 shieldId, s32* allMM) {
    s32 swordInSheath = (sheathType == PLAYER_MODELTYPE_SHEATH_16) || (sheathType == PLAYER_MODELTYPE_SHEATH_18);
    s32 shieldOnBack = ((sheathType == PLAYER_MODELTYPE_SHEATH_18) || (sheathType == PLAYER_MODELTYPE_SHEATH_19)) &&
                       (shield != PLAYER_SHIELD_NONE);
    s32 row = lod * PLAYER_FORM_MAX + form;
    s32 fitted = (model == MSEQ_MODEL_ADULT);
    MSEqDL dl;

    *allMM = (swordId >= 0) && (!shieldOnBack || (shieldId >= 0));
    if ((swordId < 0) && !(shieldOnBack && (shieldId >= 0))) {
        return NULL; // nothing of MM's on the back: Ocarina of Time's list as chosen
    }
    MSEq_Begin(play, &dl);

    if ((swordId >= 0) && shieldOnBack && (shieldId < 0) && fitted) {
        // BUG-0092: the adult carries the MM sword in its own scabbard (as when the shield is in his hand), and
        // Ocarina of Time's shield on his back without Ocarina of Time's scabbard.
        // The Deku Shield is only the child's (BUG-0093: his list on the adult, as EquipmentAlwaysVisible has it).
        const s32 deku = (shield == PLAYER_SHIELD_DEKU);
        const s32 shieldRow = deku ? (lod * PLAYER_FORM_MAX + PLAYER_FORM_CHILD) : row;
        Gfx* shieldOnly = MSEq_ShieldWithoutScabbard(
            play, (const char*)sSheathWithoutSwordDLs[shield * 2 * PLAYER_FORM_MAX + shieldRow], !deku);

        MSEq_PushFit(play, &dl, model, MSEQ_PART_SHEATH, 0.0f, 0.0f, 0.0f);
        MSEq_Call(&dl, swordInSheath ? MSEq_SheathedSword(swordId) : MSEq_EmptySheath(swordId));
        MSEq_Pop(&dl);
        if (shieldOnly != NULL) {
            gSPDisplayList(dl.p++, shieldOnly);
        }
        *allMM = false;
        return MSEq_End(&dl);
    }
    if ((swordId >= 0) && shieldOnBack && (shieldId < 0)) {
        // Ocarina of Time's shield on the back comes with Ocarina of Time's scabbard in one list: that list, empty,
        // and MM's hilt in it where MM sheathes it.
        MSEq_Call(&dl, (const char*)sSheathWithoutSwordDLs[shield * 2 * PLAYER_FORM_MAX + row]);
        if (swordInSheath) {
            const char* hilt;
            Vec3f pos;

            MSEq_SheathedHilt(swordId, &hilt, &pos);
            MSEq_PushFit(play, &dl, model, MSEQ_PART_SHEATH, pos.x, pos.y, pos.z);
            MSEq_Call(&dl, hilt);
            MSEq_Pop(&dl);
        }
        return MSEq_End(&dl);
    }

    // The sword part.
    if (swordId >= 0) {
        if (fitted) {
            MSEq_PushFit(play, &dl, model, MSEQ_PART_SHEATH, 0.0f, 0.0f, 0.0f);
        }
        MSEq_Call(&dl, swordInSheath ? MSEq_SheathedSword(swordId) : MSEq_EmptySheath(swordId));
        if (fitted) {
            MSEq_Pop(&dl);
        }
    } else {
        // Ocarina of Time's sword and scabbard without a shield (sSwordAndSheathDLs / sSheathDLs).
        MSEq_Call(&dl, (const char*)(swordInSheath ? sSwordAndSheathDLs[row] : sSheathDLs[row]));
    }

    // MM's shield on the back (MM Player_PostLimbDrawGameplay, PLAYER_LIMB_SHEATH); its list multiplies in MM's own
    // matrix, so it is drawn inside a push.
    if (shieldOnBack && (shieldId >= 0)) {
        MSEq_PushFit(play, &dl, model, MSEQ_PART_BACK_SHIELD, 0.0f, 0.0f, 0.0f);
        MSEq_Call(&dl, MSEq_BackShield(shieldId));
        MSEq_Pop(&dl);
    }
    return MSEq_End(&dl);
}

// ---- what is in use -----------------------------------------------------------------------------------------------

s32 MergeShip_MMEquip_HeroBowInUse(PlayState* play, Player* player) {
    // Young Link's shooting gallery hands him the bow action with the slingshot (Ocarina of Time's own quirk).
    if ((play != NULL) && (play->shootingGalleryStatus != 0) && !LINK_IS_ADULT) {
        return false;
    }
    return Player_HoldsBow(player) && MergeShip_MMEquip_IsSelected(MMEQUIP_BOW);
}

static s32 MSEq_HookshotInUse(Player* player) {
    return Player_HoldsHookshot(player) && MergeShip_MMEquip_IsSelected(MMEQUIP_HOOKSHOT);
}

// The MM sword the B button holds (-1: none), for the sheath.
static s32 MSEq_EquippedSword(Player* player) {
    u8 b = player->currentSwordItemId;

    if (((b == ITEM_MM_SWORD_RAZOR) || (b == ITEM_MM_SWORD_GILDED) || (b == ITEM_MM_SWORD_KOKIRI)) &&
        MergeShip_MMEquip_IsSelected(MERGESHIP_MMEQUIP_ID(b))) {
        return MERGESHIP_MMEQUIP_ID(b);
    }
    return -1;
}

// The MM shield that is the equipped shield (-1: Ocarina of Time's or none).
static s32 MSEq_ShieldOf(s32 shield) {
    if ((shield == PLAYER_SHIELD_HYLIAN) && MergeShip_MMEquip_IsSelected(MMEQUIP_HERO_SHIELD)) {
        return MMEQUIP_HERO_SHIELD;
    }
    if ((shield == PLAYER_SHIELD_MIRROR) && MergeShip_MMEquip_IsSelected(MMEQUIP_MIRROR_SHIELD)) {
        return MMEQUIP_MIRROR_SHIELD;
    }
    return -1;
}

static s32 MSEq_LeftHandItem(Player* player, s32 type) {
    if ((type == PLAYER_MODELTYPE_LH_SWORD) || (type == PLAYER_MODELTYPE_LH_SWORD_2)) {
        s32 action = player->heldItemAction;

        if ((action == PLAYER_IA_MM_SWORD_RAZOR) || (action == PLAYER_IA_MM_SWORD_GILDED)) {
            s32 id = (action == PLAYER_IA_MM_SWORD_RAZOR) ? MMEQUIP_RAZOR : MMEQUIP_GILDED;

            return MergeShip_MMEquip_IsSelected(id) ? id : -1;
        }
        // MM's Kokiri Sword swings as Ocarina of Time's (PLAYER_IA_SWORD_KOKIRI); the slot's selection says whose.
        if ((action == PLAYER_IA_SWORD_KOKIRI) && MergeShip_MMEquip_IsSelected(MMEQUIP_KOKIRI)) {
            return MMEQUIP_KOKIRI;
        }
        if (action == PLAYER_IA_SWORD_CS) {
            return MSEq_EquippedSword(player); // a cutscene's sword in hand is the equipped one
        }
    } else if ((type == PLAYER_MODELTYPE_LH_BGS) && (player->heldItemAction == PLAYER_IA_MM_SWORD_GREAT_FAIRY) &&
               MergeShip_MMEquip_IsSelected(MMEQUIP_GREAT_FAIRY)) {
        return MMEQUIP_GREAT_FAIRY;
    }
    return -1;
}

static s32 MSEq_RightHandItem(PlayState* play, Player* player, s32 type) {
    if (type == PLAYER_MODELTYPE_RH_SHIELD) {
        return MSEq_ShieldOf(player->currentShield);
    }
    if (((type == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT) || (type == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2)) &&
        MergeShip_MMEquip_HeroBowInUse(play, player)) {
        return MMEQUIP_BOW;
    }
    if ((type == PLAYER_MODELTYPE_RH_HOOKSHOT) && MSEq_HookshotInUse(player)) {
        return MMEQUIP_HOOKSHOT;
    }
    return -1;
}

// ---- the limb overrides -----------------------------------------------------------------------------------------

s32 MergeShip_MMEquip_OverrideLimb(PlayState* play, Player* this, s32 limbIndex, Gfx** dList, Vec3f* pos) {
    s32 model = MSEq_ModelOf(this);
    s32 lod = (sDListsLodOffset != 0) ? 1 : 0;
    s32 id;

    // BUG-0089: the sheath limb is composed even when Ocarina of Time chose no list for it (young Link guarding with the
    // Deku Shield: the MM sword stays on his back).
    if ((model == MSEQ_MODEL_NONE) || !MergeShip_MMEquip_IsEnabled() ||
        ((*dList == NULL) && (limbIndex != PLAYER_LIMB_SHEATH))) {
        return false;
    }
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        id = MSEq_LeftHandItem(this, sLeftHandType);
        if (id >= 0) {
            *dList = MSEq_ComposeHand(play, model, lod, true, id);
            return true;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        id = MSEq_RightHandItem(play, this, sRightHandType);
        if (id >= 0) {
            *dList = MSEq_ComposeHand(play, model, lod, false, id);
            return true;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        s32 swordId = MSEq_EquippedSword(this);
        s32 shieldId = MSEq_ShieldOf(this->currentShield);

        if ((swordId >= 0) || (shieldId >= 0)) {
            s32 allMM;
            Gfx* sheath = MSEq_ComposeSheath(play, model, lod, this->mergeShipForm, this->sheathType,
                                             this->currentShield, swordId, shieldId, &allMM);

            if (sheath != NULL) {
                *dList = sheath;
                // MM's human wearing only MM's parts keeps MM's own sheath joint (the MM-child override would move it
                // to Ocarina of Time's, 27 units up, for Ocarina of Time's lists).
                return (model == MSEQ_MODEL_MM_CHILD) && allMM;
            }
        }
    }
    return false;
}

void MergeShip_MMEquip_OverrideLimbFirstPerson(PlayState* play, Player* this, s32 limbIndex, Gfx** dList) {
    s32 model = MSEq_ModelOf(this);
    s32 bow;
    s32 hookshot;

    if ((model == MSEQ_MODEL_NONE) || !MergeShip_MMEquip_IsEnabled() || (this->unk_6AD != 2)) {
        return;
    }
    bow = MergeShip_MMEquip_HeroBowInUse(play, this);
    hookshot = MSEq_HookshotInUse(this);
    if (!bow && !hookshot) {
        return;
    }
    if (limbIndex == PLAYER_LIMB_R_HAND) {
        if ((model == MSEQ_MODEL_ADULT) && !bow) {
            // MM's own first-person model (object_link_child_DL_017B40, MM sPlayerFirstPersonRightHandHookshotDLs):
            // its rounder first-person body, crown and handle, cut before young Link's hand (its texture
            // object_link_child_Tex_019240 on), in the adult's own fist under the Hookshot fit -- MM's Hookshot looks in
            // first person as in MM (the user, 2026-09-30); 2Ship's adult draws the same (MergeShipAdultLink.c).
            Gfx* firstPerson =
                MergeShip_MMEquip_CutDLBeforeTexture(mm_object_link_child_DL_017B40, "object_link_child_Tex_019240");

            if (firstPerson != NULL) {
                MSEqDL dl;

                MSEq_Begin(play, &dl);
                MSEq_Call(&dl, gLinkAdultRightHandClosedNearDL);
                MSEq_PushFit(play, &dl, model, MSEQ_PART_HOOKSHOT, 0.0f, 0.0f, 0.0f);
                gSPDisplayList(dl.p++, firstPerson);
                MSEq_Pop(&dl);
                *dList = MSEq_End(&dl);
            } else {
                *dList = MSEq_ComposeHand(play, model, 0, false, MMEQUIP_HOOKSHOT);
            }
        } else if (model == MSEQ_MODEL_ADULT) {
            // BUG-0102: the Hero's Bow on Ocarina of Time's bow (MSEq_HeroBowFPValues): scale, turn, translate --
            // built directly, as MSEq_PushFit.
            MSEqDL dl;
            f32 v[5];
            f32 c;
            f32 s;
            Mtx* mtx;

            MSEq_HeroBowFPValues(v);
            c = cosf(v[4] * (M_PI / 180.0f)) * v[3];
            s = sinf(v[4] * (M_PI / 180.0f)) * v[3];
            {
                MtxF mf = { {
                    { c, s, 0.0f, 0.0f },
                    { -s, c, 0.0f, 0.0f },
                    { 0.0f, 0.0f, v[3], 0.0f },
                    { v[0], v[1], v[2], 1.0f },
                } };

                mtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
                guMtxF2L(mf.mf, mtx);
            }
            MSEq_Begin(play, &dl);
            MSEq_Call(&dl, gLinkAdultRightHandClosedNearDL);
            gSPMatrix(dl.p++, mtx, G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
            MSEq_Call(&dl, mm_gLinkHumanBowDL);
            MSEq_Pop(&dl);
            *dList = MSEq_End(&dl);
        } else {
            // MM sPlayerFirstPersonRightHandDLs / sPlayerFirstPersonRightHandHookshotDLs [PLAYER_FORM_HUMAN].
            *dList = ResourceMgr_LoadGfxByName(bow ? mm_object_link_child_DL_018490 : mm_object_link_child_DL_017B40);
        }
    } else if ((model != MSEQ_MODEL_ADULT) && (limbIndex == PLAYER_LIMB_L_FOREARM)) {
        // MM sPlayerFirstPersonLeftForearmDLs / LeftHandDLs [PLAYER_FORM_HUMAN]: MM's human shows its left arm.
        *dList = ResourceMgr_LoadGfxByName((model == MSEQ_MODEL_MM_CHILD) ? mm_gLinkHumanLeftForearmDL
                                                                          : gLinkChildLeftForearmNearDL);
    } else if ((model != MSEQ_MODEL_ADULT) && (limbIndex == PLAYER_LIMB_L_HAND)) {
        *dList = ResourceMgr_LoadGfxByName((model == MSEQ_MODEL_MM_CHILD) ? mm_gLinkHumanLeftHandClosedDL
                                                                          : gLinkChildLeftFistNearDL);
    }
}

s32 MergeShip_MMEquip_OverrideLimbPause(PlayState* play, s32 limbIndex, s32 type, Gfx** dList, u8* swordAndShield) {
    s32 model = LINK_IS_ADULT ? MSEQ_MODEL_ADULT : (gMergeShipMMChildLink ? MSEQ_MODEL_MM_CHILD : MSEQ_MODEL_OOT_CHILD);
    s32 form = LINK_IS_ADULT ? PLAYER_FORM_ADULT : PLAYER_FORM_CHILD;
    s32 swordId = -1;
    s32 id = -1;

    if (!MergeShip_MMEquip_IsEnabled()) {
        return false;
    }
    if (swordAndShield[0] == PLAYER_SWORD_KOKIRI) {
        if (MergeShip_MMEquip_IsSelected(MMEQUIP_RAZOR)) {
            swordId = MMEQUIP_RAZOR;
        } else if (MergeShip_MMEquip_IsSelected(MMEQUIP_GILDED)) {
            swordId = MMEQUIP_GILDED;
        } else if (MergeShip_MMEquip_IsSelected(MMEQUIP_KOKIRI)) {
            swordId = MMEQUIP_KOKIRI;
        }
    }
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        if ((type == PLAYER_MODELTYPE_LH_SWORD) || (type == PLAYER_MODELTYPE_LH_SWORD_2)) {
            id = swordId;
        } else if ((type == PLAYER_MODELTYPE_LH_BGS) && (swordAndShield[0] == PLAYER_SWORD_BIGGORON) &&
                   MergeShip_MMEquip_IsSelected(MMEQUIP_GREAT_FAIRY)) {
            id = MMEQUIP_GREAT_FAIRY;
        }
        if (id >= 0) {
            *dList = MSEq_ComposeHand(play, model, 0, true, id);
            return true;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        if (type == PLAYER_MODELTYPE_RH_SHIELD) {
            id = MSEq_ShieldOf(swordAndShield[1]);
        }
        if (id >= 0) {
            *dList = MSEq_ComposeHand(play, model, 0, false, id);
            return true;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        s32 shieldId = MSEq_ShieldOf(swordAndShield[1]);

        if ((swordId >= 0) || (shieldId >= 0)) {
            s32 allMM;
            Gfx* sheath = MSEq_ComposeSheath(play, model, 0, form, type, swordAndShield[1], swordId, shieldId, &allMM);

            if (sheath != NULL) {
                *dList = sheath;
                return (model == MSEQ_MODEL_MM_CHILD) && allMM;
            }
        }
    }
    return false;
}

// ---- post-limb pieces -------------------------------------------------------------------------------------------

f32 MergeShip_MMEquip_MeleeLengthScale(Player* player) {
    MSEqFit fit;

    if ((MSEq_ModelOf(player) != MSEQ_MODEL_ADULT) || !MERGESHIP_IA_IS_MM_SWORD(player->heldItemAction) ||
        !MergeShip_MMEquip_IsEnabled()) {
        return 1.0f;
    }
    MSEq_GetFit(MSEQ_MODEL_ADULT, MSEQ_PART_LEFT, &fit);
    return fit.scale;
}

// MM Player_PostLimbDrawGameplay, PLAYER_LIMB_RIGHT_HAND with PLAYER_MODELTYPE_RH_BOW: the string
// (object_link_child_DL_017818 at D_801C0D98), pulled back while an arrow is nocked (MM unk_B08 = OoT unk_858).
s32 MergeShip_MMEquip_DrawBowString(PlayState* play, Player* this, Vec3f* prevBodyPartPos) {
    static Vec3f sMMBowStringPos = { -35.0f, -395.0f, 0.0f }; // MM D_801C0D98
    s32 model = MSEq_ModelOf(this);
    Vec3f pos;
    MSEqFit fit;

    if ((model == MSEQ_MODEL_NONE) || !MergeShip_MMEquip_IsEnabled() || !MergeShip_MMEquip_HeroBowInUse(play, this)) {
        return false;
    }
    if ((model == MSEQ_MODEL_ADULT) && (this->unk_6AD == 2)) {
        // BUG-0102: in first person the bow lies on Ocarina of Time's (MSEq_HeroBowFPValues), its upper tip on Ocarina
        // of Time's: Ocarina of Time's own string (sBowStringData[0]) meets it there and pulls back to the arrow as in
        // Ocarina of Time. MM's string, turned with the bow, was pulled sideways off the arrow.
        return false;
    }

    MSEq_FitPoint(model, MSEQ_PART_RIGHT, &sMMBowStringPos, &pos);
    MSEq_GetFit(model, MSEQ_PART_RIGHT, &fit);

    OPEN_DISPS(play->state.gfxCtx);

    MS_INTERP_PUSH();
    Matrix_Translate(pos.x, pos.y, pos.z, MTXMODE_APPLY);
    if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->unk_860 >= 0) && (this->unk_834 <= 10)) {
        Vec3f stringPos;
        f32 dist;

        Matrix_MultVec3f(&sZeroVec, &stringPos);
        dist = Math_Vec3f_DistXYZ(prevBodyPartPos, &stringPos);
        this->unk_858 = dist - 3.0f;
        if (dist < 3.0f) {
            this->unk_858 = 0.0f;
        } else {
            this->unk_858 *= 1.6f;
            if (this->unk_858 > 1.0f) {
                this->unk_858 = 1.0f;
            }
        }
        this->unk_85C = -0.5f;
    }
    Matrix_Scale(fit.scale, this->unk_858 * fit.scale, fit.scale, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, ResourceMgr_LoadGfxByName(mm_object_link_child_DL_017818));
    MS_INTERP_POP();

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

Vec3f* MergeShip_MMEquip_ArrowOffset(PlayState* play, Player* player, Vec3f* ootOffset) {
    static Vec3f sMMHumanArrowPos = { 420.0f, 1210.0f, 380.0f }; // MM D_801C0D6C (PLAYER_FORM_HUMAN)
    s32 model = MSEq_ModelOf(player);

    if (((model == MSEQ_MODEL_MM_CHILD) || (model == MSEQ_MODEL_OOT_CHILD)) && MergeShip_MMEquip_IsEnabled() &&
        MergeShip_MMEquip_HeroBowInUse(play, player)) {
        return &sMMHumanArrowPos;
    }
    return ootOffset;
}

// MM D_801C0DD8 (the hand's hookshot point, MM rightHandWorld = OoT unk_3C8) and D_801C0DE4 (the hook).
Vec3f* MergeShip_MMEquip_HookshotOffset(Player* player, Vec3f* ootOffset, s32 tip) {
    static Vec3f sMMHandPoint = { 50.0f, 800.0f, 0.0f };
    static Vec3f sMMTipPoint = { 50.0f, 850.0f, 0.0f };
    static Vec3f sFitted;
    s32 model = MSEq_ModelOf(player);

    if ((model == MSEQ_MODEL_NONE) || !MergeShip_MMEquip_IsEnabled() || !MSEq_HookshotInUse(player)) {
        return ootOffset;
    }
    MSEq_FitPoint(model, MSEQ_PART_HOOKSHOT, tip ? &sMMTipPoint : &sMMHandPoint, &sFitted);
    return &sFitted;
}

// MM Player_DrawHookshotReticle(play, player, 77600.0f): MM's hookshot reaches as far as the Longshot.
f32 MergeShip_MMEquip_HookshotReticleRange(Player* player, f32 ootRange) {
    if (MergeShip_MMEquip_IsEnabled() && MSEq_HookshotInUse(player)) {
        return 77600.0f * CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f);
    }
    return ootRange;
}

s32 MergeShip_MMEquip_HookshotIsMM(Player* player) {
    return (player != NULL) && (player->actor.id == ACTOR_PLAYER) && MergeShip_MMEquip_IsEnabled() &&
           MSEq_HookshotInUse(player);
}

// MM ArmsHook_Init: this->timer = 26.
s32 MergeShip_MMEquip_HookshotTimer(Player* player, s32 ootTimer) {
    if (MergeShip_MMEquip_HookshotIsMM(player)) {
        return (s32)(26 * CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f));
    }
    return ootTimer;
}

// MM ArmsHook_Draw: the hook (object_link_child_DL_01D960) and the chain (gameplay_keep gHookshotChainDL).
const char* MergeShip_MMEquip_HookshotTipDL(Player* player) {
    return MergeShip_MMEquip_HookshotIsMM(player) ? mm_object_link_child_DL_01D960 : NULL;
}

const char* MergeShip_MMEquip_HookshotChainDL(Player* player) {
    return MergeShip_MMEquip_HookshotIsMM(player) ? mm_gHookshotChainDL : NULL;
}

f32 MergeShip_MMEquip_HookshotTipScale(Player* player) {
    MSEqFit fit;

    if (!MergeShip_MMEquip_HookshotIsMM(player)) {
        return 1.0f;
    }
    MSEq_GetFit(MSEq_ModelOf(player), MSEQ_PART_HOOKSHOT, &fit);
    return fit.scale;
}
