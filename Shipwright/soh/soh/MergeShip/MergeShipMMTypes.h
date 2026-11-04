#pragma once

// MergeShip2 -- small Majora's Mask types that OoT's Player now carries.
//
// MM's player is compiled rather than transcribed, so OoT's Player gained the fields it was
// written against. A few of those fields have types Ocarina of Time never defined. They are
// plain structs with no behaviour, so they are defined here and included by z64player.h --
// which keeps them available to every translation unit that sees a Player, rather than only to
// the ones that also have MM's headers on their include path.

typedef struct FaceChange {
    /* 0x0 */ s16 face;
    /* 0x2 */ s16 timer;
} FaceChange; // size = 0x4

typedef struct {
    /* 0x00 */ u8 unk_00;
    /* 0x01 */ u8 alpha;
    /* 0x04 */ MtxF mf;
} struct_80122D44_arg1_unk_04; // size = 0x44

typedef struct {
    /* 0x00 */ u8 unk_00;
    /* 0x01 */ s8 unk_01;
    /* 0x02 */ char unk_02[2];
    /* 0x04 */ struct_80122D44_arg1_unk_04 unk_04[4];
} struct_80122D44_arg1; // size >= 0x114

// MM sizes the player's joint buffers from its own animation frame type. OoT's Player carries
// these buffers now, so the constant has to be visible to every translation unit that sees a
// Player -- not only to the ones compiling MM source.
#ifndef PLAYER_LIMB_BUF_SIZE
#define PLAYER_LIMB_BUF_SIZE (ALIGN16(sizeof(Vec3s) * PLAYER_LIMB_MAX + sizeof(s16)) + 0xF)
#endif
