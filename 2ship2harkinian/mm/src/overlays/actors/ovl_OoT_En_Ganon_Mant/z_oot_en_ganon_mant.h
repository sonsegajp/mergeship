#ifndef Z_OOT_EN_GANON_MANT_H
#define Z_OOT_EN_GANON_MANT_H

// MergeShip2 -- Ocarina of Time's Ganondorf's cape (OoT En_Ganon_Mant), ported to Majora's Mask for Ganondorf mode in
// Termina (2s2h/MergeShip/MergeShipGanondorf.h): the player's cape, as SoH's ovl_En_Ganon_Mant is in Hyrule.

#include "global.h"

#define GANON_MANT_NUM_JOINTS 12
#define GANON_MANT_NUM_STRANDS 12

typedef struct {
    /* 0x000 */ Vec3f root;
    /* 0x00C */ Vec3f joints[GANON_MANT_NUM_JOINTS];
    /* 0x090 */ Vec3f rotations[GANON_MANT_NUM_JOINTS];
    /* 0x120 */ Vec3f velocities[GANON_MANT_NUM_JOINTS];
    /* 0x1B0 */ u8 torn[GANON_MANT_NUM_JOINTS];
} OoTMantStrand;

typedef struct OoTEnGanonMant {
    /* 0x0000 */ Actor actor;
    OoTMantStrand strands[GANON_MANT_NUM_STRANDS];
    f32 minY;                 // minimum Y value possible for joints, for emulating collision with a floor
    f32 backPush;             // larger values push the cloak further outwards, negative is away from the actor
    f32 backSwayMagnitude;    // magnitude of backwards/forwards swaying
    f32 sideSwayMagnitude;    // magnitude of sideways swaying
    f32 attachRightArmTimer;  // the cloak on the right forearm and the left shoulder
    f32 attachLeftArmTimer;   // the cloak on the left forearm and the right shoulder
    f32 attachShouldersTimer; // the cloak on both shoulders
    f32 gravity;              // strand gravity
    f32 baseYaw;
    f32 minDist; // closest distance the cloak can get to the attached actor without being pushed away
    Vec3f rightForearmPos;
    Vec3f leftForearmPos;
    Vec3f rightShoulderPos;
    Vec3f leftShoulderPos;
    u8 tearTimer;
    u8 updateHasRun;
    u8 frameTimer;
    u32 capeColor; // MergeShip2: the cloth's colour (0 its own red, else 0x01RRGGBB: MergeShipGanondorf.h)
} OoTEnGanonMant;

#endif
