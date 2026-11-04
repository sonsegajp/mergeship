/*
 * MergeShip2 -- how Majora's Mask's masks sit on Ocarina of Time's adult Link, in both engines: Hyrule's adult (SoH
 * soh/MergeShip/MergeShipMMMasks.c) and Termina's adult human (2Ship 2s2h/MergeShip/MergeShipAdultLink.c). One table,
 * so a mask fits the same head the same way in both worlds.
 *
 * MM's masks are modelled on MM's young Link's head limb. The adult's head limb is another shape -- measured from the
 * head models' own vertices (MM gLinkHumanHeadDL, OoT gLinkAdultHeadNearDL, the head matrix's part): about 32% deeper
 * front to back (the face reaches further forward), 10% taller and 6% wider, with its crown higher. One offset and
 * one scale for every mask (the old fit: x 140, y -120, scale 1.3) set the face masks too wide and the hats ~300 units
 * above the hood (the user, 2026-10-01: "Masks still dont fit on adult link's head properly"). So each mask has its own
 * placement, in the head limb's space (+X out of the face, -Y up, Z sideways), applied as
 *     translate (x, y, z), then rotate about Z by `pitch` (binary angle units; + tilts the top back), then scale
 *     (sx, sy, sz)
 * -- a point p of the mask goes to (x, y, z) + R(pitch) * (sx, sy, sz) p. MS_MASK_FIT_HEAD maps the young head's
 * box onto the adult's (front, back, crown, chin, sides): the starting fit of every mask. Each mask was then refined out of
 * engine (tools/tune_mask_fit.py, previewed with tools/render_mask_grid.py; the user, 2026-10-01: "make sure no mask is
 * too far forward and ensure theres no clipping"): its stand-off from the face is young Link's scaled to the adult's
 * head (x1.1, the head-box stretch of 1.32 front to back set every mask too far forward), and the adult's head shows
 * through it no more than young Link's does (MM's own hair and hat poke out of some), from the front, three-quarter,
 * side, back and top. The All-Night Mask sits by eye (its eyes over the adult's: the search favoured the chin).
 *
 * Every value can be changed live (tuning, and a player's own taste): the float CVar
 * gMergeShip.MaskFit.Adult.<n>.<X|Y|Z|SX|SY|SZ|Pitch>, n being Majora's Mask's PlayerMask number (1 Mask of Truth ..
 * 20 Giant's Mask, 21 Fierce Deity's .. 24 Deku Mask), or 25 for the Child Mask. Pure C.
 *
 * The Child Mask (include/mergeship/AdultMask.hpp: the Adult Mask as the adult holds it) is the Skull Kid's Link mask,
 * modelled for the Skull Kid's head rather than MM's young Link's, so its fit maps that model straight onto the adult's
 * head: its nose and mouth over his (the two faces' nose-to-mouth heights are the same), stretched front to back so
 * the adult's forward bangs stay behind its forehead, and tipped a little forward to follow his brow; the adult's
 * head does not show through it from the front, three-quarter or side views (tools/render_mask_grid.py's renderer,
 * the user, 2026-10-01).
 */
#ifndef MERGESHIP_MASK_FIT_H
#define MERGESHIP_MASK_FIT_H

#include <stdio.h>

typedef struct MsMaskFit {
    float x, y, z;    /* head-limb units */
    float sx, sy, sz; /* scale along the head limb's axes */
    float pitch;      /* binary angle (0x10000 = 360 degrees); + tilts the mask's top back */
} MsMaskFit;

/* Majora's Mask's PlayerMask numbers, 0 (none) .. 24 (the Deku Mask); then the Child Mask's. */
#define MS_MASK_FIT_CHILD_MASK 25
#define MS_MASK_FIT_COUNT 26

/* The young head's box onto the adult's: X [-200, 873] -> [-262, 1155], Y [-1200, 119] -> [-1377, 68],
 * Z [-685, 686] -> [-728, 728]. */
#define MS_MASK_FIT_HEAD { 2.0f, -62.0f, 0.0f, 1.32f, 1.10f, 1.06f, 0.0f }

static const MsMaskFit kMsAdultMaskFit[MS_MASK_FIT_COUNT] = {
    /*  0 none             */ MS_MASK_FIT_HEAD,
    /*  1 Mask of Truth    */ { 2.0f, -152.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /*  2 Kafei's Mask     */ { -18.0f, -122.0f, 0.0f, 1.354f, 1.188f, 1.145f, 0.0f },
    /*  3 All-Night Mask   */ { 40.0f, -62.0f, 0.0f, 1.15f, 1.1f, 1.06f, 0.0f },
    /*  4 Bunny Hood       */ { 22.0f, -62.0f, 0.0f, 1.32f, 1.1f, 1.06f, 0.0f },
    /*  5 Keaton Mask      */ { 2.0f, -32.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /*  6 Garo's Mask      */ { 22.0f, -62.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /*  7 Romani's Mask    */ { 2.0f, -32.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /*  8 Circus Leader's  */ { 2.0f, -212.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /*  9 Postman's Hat    */ { -18.0f, -62.0f, 0.0f, 1.32f, 1.1f, 1.06f, 0.0f },
    /* 10 Couple's Mask    */ { -38.0f, -32.0f, 0.0f, 1.32f, 1.1f, 1.06f, 0.0f },
    /* 11 Great Fairy's    */ { -18.0f, -2.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /* 12 Gibdo Mask       */ { 22.0f, -32.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /* 13 Don Gero's Mask  */ { -18.0f, 28.0f, 0.0f, 1.122f, 1.1f, 1.06f, 0.0f },
    /* 14 Kamaro's Mask    */ { 22.0f, -152.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /* 15 Captain's Hat    */ { 22.0f, -92.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /* 16 Stone Mask       */ { -38.0f, -92.0f, 0.0f, 1.304f, 1.144f, 1.102f, 0.0f },
    /* 17 Bremen Mask      */ { 102.0f, -122.0f, 0.0f, 1.212f, 1.188f, 1.145f, 0.0f },
    /* 18 Blast Mask       */ { -18.0f, -62.0f, 0.0f, 1.32f, 1.1f, 1.06f, 0.0f },
    /* 19 Mask of Scents   */ { 22.0f, -62.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /* 20 Giant's Mask     */ { -18.0f, -62.0f, 0.0f, 1.254f, 1.1f, 1.06f, 0.0f },
    /* 21 Fierce Deity's   */ { 2.0f, -62.0f, 0.0f, 1.122f, 1.1f, 1.06f, 0.0f },
    /* 22 Goron Mask       */ { 22.0f, -92.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /* 23 Zora Mask        */ { 42.0f, -32.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /* 24 Deku Mask        */ { 22.0f, -62.0f, 0.0f, 1.188f, 1.1f, 1.06f, 0.0f },
    /* 25 Child Mask       */ { 165.0f, -290.0f, 0.0f, 1.2f, 1.1f, 1.08f, -1024.0f },
};

/* The fit of MM mask `mask` (PlayerMask number), each field overridden by its CVar when set: `getFloat` is the engine's
 * CVarGetFloat (name, default). */
static inline MsMaskFit MsMaskFit_Adult(int mask, float (*getFloat)(const char*, float)) {
    static const char* const kFields[7] = { "X", "Y", "Z", "SX", "SY", "SZ", "Pitch" };
    MsMaskFit fit;
    float* values;
    char name[64];
    int i;

    if ((mask < 0) || (mask >= MS_MASK_FIT_COUNT)) {
        mask = 0;
    }
    fit = kMsAdultMaskFit[mask];
    values = &fit.x;
    for (i = 0; i < 7; i++) {
        snprintf(name, sizeof(name), "gMergeShip.MaskFit.Adult.%d.%s", mask, kFields[i]);
        values[i] = getFloat(name, values[i]);
    }
    return fit;
}

#endif /* MERGESHIP_MASK_FIT_H */
