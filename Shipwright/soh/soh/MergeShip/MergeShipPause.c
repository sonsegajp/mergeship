#include "global.h"
#include "soh/MergeShip/MergeShipPause.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAssets.h"
#include "soh/MergeShip/MergeShipMMItems.h" // MergeShip2: the Items (Termina) face's option
#include <math.h>

// MergeShip2 -- the pause carousel's geometry. See MergeShipPause.h.
//
// Ocarina of Time's box, reduced to one rule: the face at ring position k has angle
// phi = k * 2pi / N; it sits at (d sin phi, y, -d cos phi) turned by -phi about Y, and it is seen
// from the eye at e * (-sin phi, cos phi). OoT's d is WREG(3)/100 (93.55) and its e is 64: the eye is
// inside the box, 157.55 units from the face it looks at. For N faces of the same width the face
// distance becomes the N-gon's apothem, d_N = d / tan(pi / N) (d is the square's half width), and
// the eye keeps its 157.55-unit distance to the face, e_N = 64 + d - d_N, so every face fills the
// screen exactly as Ocarina of Time's do. For N = 5: d_5 = 128.8, e_5 = 28.8, still inside the prism
// (its vertex opposite a face is 159 away), so the faces behind the eye stay back-face culled.
// (MergeShip2: the Items (Termina) face no longer has a place of its own -- it is the Item face's Majora's Mask view,
// switched with C-Down -- so the ring has 5 faces with mm.o2r and 4 without. The rule holds for any N; for N = 6
// d_6 = 162.0, e_6 = -4.5.) A page that is not on the ring (the Item face's other view) has index 0, the Item face's.

#define OOT_PAUSE_EYE_DIST 64.0f

// The first two share the ring's first place: Ocarina of Time's Select Item, or -- switched to with C-Down --
// Majora's Mask's (MergeShip_Pause_SetMMItemView); exactly one of them is on the ring.
static const u16 sRingPages[MERGESHIP_PAUSE_MAX_PAGES] = {
    PAUSE_ITEM,
    MERGESHIP_PAUSE_MM_ITEM, // MergeShip2: Items (Termina), MergeShipMMItems.h
    PAUSE_MAP,
    PAUSE_QUEST,
    PAUSE_EQUIP,
    MERGESHIP_PAUSE_MASK,
};

// MergeShip2: the Item face shows Majora's Mask's Select Item (C-Down on the Item face, z_kaleido_mergeship.c).
static s32 sMMItemViewShown = false;

s32 MergeShip_Pause_IsMMItemViewShown(void) {
    return sMMItemViewShown && MergeShip_MMItems_Enabled();
}

void MergeShip_Pause_SetMMItemView(PauseContext* pauseCtx, s32 shown) {
    sMMItemViewShown = shown;
    if ((pauseCtx != NULL) &&
        ((pauseCtx->pageIndex == PAUSE_ITEM) || (pauseCtx->pageIndex == MERGESHIP_PAUSE_MM_ITEM))) {
        pauseCtx->pageIndex = MergeShip_Pause_IsMMItemViewShown() ? MERGESHIP_PAUSE_MM_ITEM : PAUSE_ITEM;
    }
}

// Whether a face is on the ring now: Ocarina of Time's four always (the Item face as Ocarina of Time's or Majora's
// Mask's Select Item); the Masks face while mm.o2r is mounted.
static s32 MergeShip_Pause_PageActive(u16 page) {
    if (page == MERGESHIP_PAUSE_MASK) {
        return MergeShip_IsMajorasMaskMounted();
    }
    if (page == PAUSE_ITEM) {
        return !MergeShip_Pause_IsMMItemViewShown();
    }
    if (page == MERGESHIP_PAUSE_MM_ITEM) {
        return MergeShip_Pause_IsMMItemViewShown();
    }
    return true;
}

// The faces on the ring, in ring order. Returns how many.
static u16 MergeShip_Pause_ActiveRing(u16* ring) {
    u16 i;
    u16 count = 0;

    for (i = 0; i < MERGESHIP_PAUSE_MAX_PAGES; i++) {
        if (MergeShip_Pause_PageActive(sRingPages[i])) {
            ring[count++] = sRingPages[i];
        }
    }
    return count;
}

u16 MergeShip_Pause_PageCount(void) {
    u16 ring[MERGESHIP_PAUSE_MAX_PAGES];

    return MergeShip_Pause_ActiveRing(ring);
}

static u16 MergeShip_Pause_RingIndex(u16 page) {
    u16 ring[MERGESHIP_PAUSE_MAX_PAGES];
    u16 count = MergeShip_Pause_ActiveRing(ring);
    u16 i;

    for (i = 0; i < count; i++) {
        if (ring[i] == page) {
            return i;
        }
    }
    return 0;
}

u16 MergeShip_Pause_NextPage(u16 page, s32 toRight) {
    u16 ring[MERGESHIP_PAUSE_MAX_PAGES];
    u16 count = MergeShip_Pause_ActiveRing(ring);
    u16 index = MergeShip_Pause_RingIndex(page);

    index = toRight ? ((index + 1) % count) : ((index + count - 1) % count);
    return ring[index];
}

u16 MergeShip_Pause_ModeTarget(u16 mode) {
    // OoT: mode = pageIndex * 2 + 1 turns left (Z), pageIndex * 2 turns right (R).
    return MergeShip_Pause_NextPage(mode / 2, !(mode & 1));
}

static f32 MergeShip_Pause_Angle(u16 page) {
    return MergeShip_Pause_RingIndex(page) * (2.0f * M_PI) / MergeShip_Pause_PageCount();
}

static f32 MergeShip_Pause_Apothem(f32 boxRadius) {
    // The square's apothem is its half width; tan(pi / 4) = 1 keeps OoT's box unchanged.
    return boxRadius / tanf(M_PI / MergeShip_Pause_PageCount());
}

void MergeShip_Pause_GetEye(u16 page, f32* x, f32* z) {
    f32 boxRadius = WREG(3) / 100.0f;
    f32 eyeDist = OOT_PAUSE_EYE_DIST + boxRadius - MergeShip_Pause_Apothem(boxRadius);
    f32 phi = MergeShip_Pause_Angle(page);

    *x = -eyeDist * sinf(phi);
    *z = eyeDist * cosf(phi);
    // Keep OoT's exact eye values where the float math only approximates 0.
    if (fabsf(*x) < 0.001f) {
        *x = 0.0f;
    }
    if (fabsf(*z) < 0.001f) {
        *z = 0.0f;
    }
}

// Ocarina of Time's camera looks at the centre, which is past the eye on the way to the face. With six faces e_6 is
// below zero: the eye stands past the centre, on the face's side, and looking at the centre would turn the camera
// around to the face opposite. It looks the same way instead: from the centre through the eye to the face.
void MergeShip_Pause_GetLookAt(f32 eyeX, f32 eyeZ, f32* x, f32* z) {
    f32 boxRadius = WREG(3) / 100.0f;

    *x = 0.0f;
    *z = 0.0f;
    if (OOT_PAUSE_EYE_DIST + boxRadius - MergeShip_Pause_Apothem(boxRadius) < 0.0f) {
        *x = 2.0f * eyeX;
        *z = 2.0f * eyeZ;
    }
}

void MergeShip_Pause_StepEye(PauseContext* pauseCtx, f32 fraction) {
    f32 fromX;
    f32 fromZ;
    f32 toX;
    f32 toZ;

    MergeShip_Pause_GetEye(pauseCtx->mode / 2, &fromX, &fromZ);
    MergeShip_Pause_GetEye(MergeShip_Pause_ModeTarget(pauseCtx->mode), &toX, &toZ);
    pauseCtx->eye.x += (toX - fromX) * fraction;
    pauseCtx->eye.z += (toZ - fromZ) * fraction;
}

void MergeShip_Pause_LoadPageMatrix(u16 page, f32 boxRadius, f32 y, f32 flip) {
    f32 dist = MergeShip_Pause_Apothem(boxRadius);
    f32 phi = MergeShip_Pause_Angle(page);

    // OoT per face: Item RotX(-f); Map RotZ(-f) RotY(-1.57); Quest RotX(f) RotY(3.14); Equipment RotZ(f)
    // RotY(1.57). Each is RotY(-phi) followed by RotX(-f) about the face's own axis.
    Matrix_Translate(dist * sinf(phi), y, -dist * cosf(phi), MTXMODE_NEW);
    Matrix_Scale(0.78f, 0.78f, 0.78f, MTXMODE_APPLY);
    Matrix_RotateY(-phi, MTXMODE_APPLY);
    Matrix_RotateX(-flip / 100.0f, MTXMODE_APPLY);
}
