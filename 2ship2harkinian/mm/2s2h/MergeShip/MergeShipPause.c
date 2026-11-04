#include "global.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
#include "2s2h/MergeShip/MergeShipPause.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include <math.h>

// MergeShip2 -- the pause carousel's geometry. See MergeShipPause.h.
//
// Majora's Mask's box, reduced to one rule: the face at turn a is drawn at RotY(a) Translate(0, y, -d) (d = 93, the
// square's half width 3 * 80 * 0.78 / 2 = 93.6, rounded as MM has it) and looked at from the eye e * (sin a, cos a)
// (e = PAUSE_EYE_DIST = 64): the eye is inside the box, 157 units from the face it looks at. For N faces of the same
// width the face distance becomes the N-gon's apothem, d_N = d / tan(pi / N) (tan(pi / 4) = 1 keeps MM's box), and the
// eye keeps its 157-unit distance to the face, e_N = 64 + d - d_N, so every face fills the screen exactly as MM's do,
// and the item-grid cursor, ammo digits and info panel (drawn in screen space) stay where MM has them.
// (MergeShip2: the Items (Hyrule) face no longer has a place of its own -- it is the Item face's Ocarina of Time view,
// switched with C-Up (MergeShip_Pause_SetOoTItemView) -- so the ring is MM's four faces, N = 4, with MM's own numbers;
// the Item face's place holds MM's Select Item or Ocarina of Time's. The rule holds for any N registered later: for
// N = 5, d_5 = 128.0 and e_5 = 29.0, the eye still inside the prism and the faces two steps away wholly behind it,
// which is what MM's draw rule -- skip the face opposite the current one -- relies on.)

#define MM_PAUSE_FACE_DIST 93.0f
#define MERGESHIP_PAUSE_PI 3.14159265358979323846f

typedef struct MergeShipPauseFace {
    u16 page;
    s32 (*isActive)(void); // NULL: always on the ring
} MergeShipPauseFace;

// MergeShip2: the Item face shows Ocarina of Time's Select Item (C-Up on the Item face, z_kaleido_mergeship.c).
static s32 sOoTItemViewShown = false;

s32 MergeShip_Pause_IsOoTItemViewShown(void) {
    return sOoTItemViewShown && MergeShip_OoTItems_Enabled();
}

void MergeShip_Pause_SetOoTItemView(PauseContext* pauseCtx, s32 shown) {
    sOoTItemViewShown = shown;
    if ((pauseCtx != NULL) &&
        ((pauseCtx->pageIndex == PAUSE_ITEM) || (pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM))) {
        pauseCtx->pageIndex = MergeShip_Pause_IsOoTItemViewShown() ? MERGESHIP_PAUSE_OOT_ITEM : PAUSE_ITEM;
    }
}

static s32 MergeShip_Pause_MMItemViewActive(void) {
    return !MergeShip_Pause_IsOoTItemViewShown();
}

// The ring, in R order. Register further faces here (with a page id past MERGESHIP_PAUSE_OOT_ITEM and an "active"
// predicate); MM's four keep their relative order so Masks stays left of Item and Map right of it. The first two share
// the ring's first place: MM's Select Item, or -- switched to with C-Up -- Ocarina of Time's (exactly one of them is
// on the ring).
static const MergeShipPauseFace sRingFaces[] = {
    { PAUSE_ITEM, MergeShip_Pause_MMItemViewActive },
    { MERGESHIP_PAUSE_OOT_ITEM, MergeShip_Pause_IsOoTItemViewShown }, // MergeShip2: z_kaleido_oot_item.c
    { PAUSE_MAP, NULL },
    { PAUSE_QUEST, NULL },
    { PAUSE_MASK, NULL },
};

// MM's turn of each of its faces (KaleidoScope_DrawPages' Matrix_RotateYF constants).
static const f32 sMMFaceTurn[] = {
    0.0f,   // PAUSE_ITEM
    -1.57f, // PAUSE_MAP
    -3.14f, // PAUSE_QUEST
    1.57f,  // PAUSE_MASK
};

// MM's eye point for each of its faces (PAUSE_EYE_DIST * -PAUSE_<page>_X/Z, as z_kaleido_setup.c has them).
static const f32 sMMFaceEyeX[] = {
    PAUSE_EYE_DIST * -PAUSE_ITEM_X,
    PAUSE_EYE_DIST * -PAUSE_MAP_X,
    PAUSE_EYE_DIST * -PAUSE_QUEST_X,
    PAUSE_EYE_DIST * -PAUSE_MASK_X,
};
static const f32 sMMFaceEyeZ[] = {
    PAUSE_EYE_DIST * -PAUSE_ITEM_Z,
    PAUSE_EYE_DIST * -PAUSE_MAP_Z,
    PAUSE_EYE_DIST * -PAUSE_QUEST_Z,
    PAUSE_EYE_DIST * -PAUSE_MASK_Z,
};

static u16 MergeShip_Pause_ActiveRing(u16* ring) {
    u16 count = 0;
    u16 i;

    for (i = 0; i < ARRAY_COUNT(sRingFaces); i++) {
        if ((sRingFaces[i].isActive == NULL) || sRingFaces[i].isActive()) {
            ring[count++] = sRingFaces[i].page;
        }
    }
    return count;
}

u16 MergeShip_Pause_PageCount(void) {
    u16 ring[MERGESHIP_PAUSE_MAX_FACES];

    return MergeShip_Pause_ActiveRing(ring);
}

s32 MergeShip_Pause_IsMMRing(void) {
    return MergeShip_Pause_PageCount() == 4;
}

// The page's place on the ring, or -1.
static s32 MergeShip_Pause_RingIndex(u16 page, u16* count) {
    u16 ring[MERGESHIP_PAUSE_MAX_FACES];
    u16 i;

    *count = MergeShip_Pause_ActiveRing(ring);
    for (i = 0; i < *count; i++) {
        if (ring[i] == page) {
            return i;
        }
    }
    return -1;
}

s32 MergeShip_Pause_PageOnRing(u16 page) {
    u16 count;

    return MergeShip_Pause_RingIndex(page, &count) >= 0;
}

u16 MergeShip_Pause_NextPage(u16 page, s32 toRight) {
    u16 ring[MERGESHIP_PAUSE_MAX_FACES];
    u16 count = MergeShip_Pause_ActiveRing(ring);
    s32 index = 0;
    u16 i;

    for (i = 0; i < count; i++) {
        if (ring[i] == page) {
            index = i;
            break;
        }
    }
    index = toRight ? ((index + 1) % count) : ((index + count - 1) % count);
    return ring[index];
}

u16 MergeShip_Pause_ModeTarget(u16 mode) {
    // MM: nextPageMode = pageIndex * 2 + 1 turns left (Z), pageIndex * 2 turns right (R).
    return MergeShip_Pause_NextPage(mode / 2, !(mode & 1));
}

// The face's turn about Y (MM's constants on MM's ring).
static f32 MergeShip_Pause_Turn(u16 page) {
    u16 count;
    s32 index = MergeShip_Pause_RingIndex(page, &count);
    f32 step;

    if ((count == 4) && MERGESHIP_PAUSE_IS_MM_PAGE(page)) {
        return sMMFaceTurn[page];
    }
    if (index < 0) {
        index = 0;
    }
    step = (2.0f * MERGESHIP_PAUSE_PI) / count;
    // Right of Item turns negative (MM: Map -1.57), left of it positive (MM: Masks +1.57).
    return (index <= count / 2) ? (-index * step) : ((count - index) * step);
}

static f32 MergeShip_Pause_FaceDist(void) {
    u16 count = MergeShip_Pause_PageCount();

    if (count == 4) {
        return MM_PAUSE_FACE_DIST;
    }
    return MM_PAUSE_FACE_DIST / tanf(MERGESHIP_PAUSE_PI / count);
}

void MergeShip_Pause_GetEye(u16 page, f32* x, f32* z) {
    f32 turn;
    f32 eyeDist;

    if (MergeShip_Pause_IsMMRing() && MERGESHIP_PAUSE_IS_MM_PAGE(page)) {
        *x = sMMFaceEyeX[page];
        *z = sMMFaceEyeZ[page];
        return;
    }
    turn = MergeShip_Pause_Turn(page);
    eyeDist = PAUSE_EYE_DIST + MM_PAUSE_FACE_DIST - MergeShip_Pause_FaceDist();
    *x = eyeDist * sinf(turn);
    *z = eyeDist * cosf(turn);
    if (fabsf(*x) < 0.001f) {
        *x = 0.0f;
    }
    if (fabsf(*z) < 0.001f) {
        *z = 0.0f;
    }
}

void MergeShip_Pause_StepEye(PauseContext* pauseCtx) {
    f32 fromX;
    f32 fromZ;
    f32 toX;
    f32 toZ;

    // MM: sPageSwitchEyeDx[mode] = -PAUSE_EYE_DIST * (toDirX - fromDirX) / 16, i.e. (toEye - fromEye) / 16, twice per
    // frame over 8 frames. On MM's ring the eye points are integers, so these are MM's exact values.
    MergeShip_Pause_GetEye(pauseCtx->nextPageMode / 2, &fromX, &fromZ);
    MergeShip_Pause_GetEye(MergeShip_Pause_ModeTarget(pauseCtx->nextPageMode), &toX, &toZ);
    pauseCtx->eye.x += ((toX - fromX) / 16) * 2.0f;
    pauseCtx->eye.z += ((toZ - fromZ) / 16) * 2.0f;
}

void MergeShip_Pause_FinishSwitch(PauseContext* pauseCtx) {
    pauseCtx->pageIndex = MergeShip_Pause_ModeTarget(pauseCtx->nextPageMode);
    MergeShip_Pause_GetEye(pauseCtx->pageIndex, &pauseCtx->eye.x, &pauseCtx->eye.z);
}

void MergeShip_Pause_SetupOpening(PauseContext* pauseCtx) {
    u16 rightPage;

    // Each pause screen opens with the Item face on MM's own Select Item (the Ocarina of Time view shown when it last
    // closed is gone; the page it was on opens as PAUSE_ITEM).
    MergeShip_Pause_SetOoTItemView(pauseCtx, false);
    if (!MergeShip_Pause_PageOnRing(pauseCtx->pageIndex)) {
        pauseCtx->pageIndex = PAUSE_ITEM;
    }
    rightPage = MergeShip_Pause_NextPage(pauseCtx->pageIndex, true);
    MergeShip_Pause_GetEye(rightPage, &pauseCtx->eye.x, &pauseCtx->eye.z);
    pauseCtx->pageIndex = rightPage;
}

const u8* MergeShip_Pause_ButtonStatus(u16 page) {
    return gPageSwitchNextButtonStatus[(MERGESHIP_PAUSE_HAS_ARRAYS(page) ? page : PAUSE_ITEM) + 1];
}

void MergeShip_Pause_LoadPageMatrix(u16 page, f32 y, f32 roll) {
    Matrix_RotateYF(MergeShip_Pause_Turn(page), MTXMODE_NEW);
    Matrix_Translate(0.0f, y, -MergeShip_Pause_FaceDist(), MTXMODE_APPLY);
    Matrix_Scale(0.78f, 0.78f, 0.78f, MTXMODE_APPLY);
    Matrix_RotateXFApply(-roll / 100.0f);
}

void MergeShip_Pause_ToRing(u16 page) {
    MtxF mmMatrix;

    if (!MERGESHIP_PAUSE_IS_MM_PAGE(page) || MergeShip_Pause_IsMMRing()) {
        return;
    }
    // MM built RotY(mmTurn) Translate(0, y, -93) ... (or, for the world map, RotY(yaw) Translate(0, y, depth) ...).
    // Pre-multiplying by RotY(turn) Translate(0, 0, -(dist - 93)) RotY(-mmTurn) turns the face to its place on the
    // ring and pushes it out to the N-gon's apothem, keeping everything MM put on it where it is on the face.
    Matrix_Get(&mmMatrix);
    Matrix_RotateYF(MergeShip_Pause_Turn(page), MTXMODE_NEW);
    Matrix_Translate(0.0f, 0.0f, -(MergeShip_Pause_FaceDist() - MM_PAUSE_FACE_DIST), MTXMODE_APPLY);
    Matrix_RotateYF(-sMMFaceTurn[page], MTXMODE_APPLY);
    Matrix_Mult(&mmMatrix, MTXMODE_APPLY);
}

void MergeShip_Pause_PromptMatrix(PauseContext* pauseCtx, f32 y) {
    if (MERGESHIP_PAUSE_IS_MM_PAGE(pauseCtx->pageIndex)) {
        MergeShip_Pause_ToRing(pauseCtx->pageIndex);
    } else {
        // MM's prompt switch: the prompt page stands where the current face was, turned by -roll.
        MergeShip_Pause_LoadPageMatrix(pauseCtx->pageIndex, y, pauseCtx->roll);
    }
}

f32 MergeShip_Pause_FaceRoll(PauseContext* pauseCtx, u16 page) {
    // MM's prompt block sets the current face's roll to roll + 314 every frame the save or game-over prompt is up,
    // except that PAUSE_STATE_GAMEOVER_5 sets every face's roll to roll first.
    if ((page == pauseCtx->pageIndex) &&
        ((pauseCtx->state == PAUSE_STATE_SAVEPROMPT) || IS_PAUSE_STATE_GAMEOVER(pauseCtx)) &&
        (pauseCtx->state != PAUSE_STATE_GAMEOVER_5)) {
        return pauseCtx->roll + 314.0f;
    }
    // Every other face shares one roll (MM sets them together); read it from an MM face that is not the current one.
    return (pauseCtx->pageIndex != PAUSE_ITEM) ? pauseCtx->itemPageRoll : pauseCtx->mapPageRoll;
}
