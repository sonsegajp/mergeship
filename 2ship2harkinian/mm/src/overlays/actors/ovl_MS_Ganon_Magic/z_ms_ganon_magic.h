#ifndef Z_MS_GANON_MAGIC_H
#define Z_MS_GANON_MAGIC_H

// MergeShip2: Ganondorf mode's magic (soh/MergeShip/MergeShipGanondorf.h) -- the bosses' own projectiles and effects,
// ported from BossGanon (z_boss_ganon.c) and EnFhgFire (z_en_fhg_fire.c), thrown by the player at what he is locked
// on to and hitting enemies instead of Link. Registered at run time (MergeShipGanondorfActors.cpp).

#include "global.h"

typedef enum {
    /* 0 */ MS_GANON_MAGIC_LIGHT_BALL, // BossGanon_LightBall_Update / _Draw (params 0x64): his thrown light ball
    /* 1 */ MS_GANON_MAGIC_BURST,      // BossGanon_LightBall_Update's ring (params 0x190): where a ball breaks
    /* 2 */ MS_GANON_MAGIC_BIG_MAGIC,  // func_808E2544 / func_808E324C (params 0x104..0x108): the big magic's balls
    /* 3 */ MS_GANON_MAGIC_SHOCKWAVE,  // BossGanon_PoundFloor's shockwave (GDF_EFF_SHOCKWAVE): the floor pound
    /* 4 */ MS_GANON_MAGIC_PHANTOM_ORB, // EnFhgFire_EnergyBall: Phantom Ganon's energy ball
    /* 5 */ MS_GANON_MAGIC_HIT // no look: a hitbox on its parent for `timer` frames (stepScale ahead of him), set up by
                               // the spawner (Beast Ganon's swords and roar, Phantom Ganon's charge)
    ,
    /* 6 */ MS_GANON_MAGIC_PHANTOM_BURST, // EnFhgFire_LightningBurst: where Phantom Ganon's ball strikes
    /* 7 */ MS_GANON_MAGIC_BLAST // no look: the floor pound's blast, as a bomb's (breaks what bombs and the hammer break)
} MsGanonMagicType;

struct MsGanonMagic;

typedef struct MsGanonMagic {
    /* 0x0000 */ Actor actor;
    /* 0x014C */ ColliderCylinder collider;
    s16 type;
    s16 state;
    s16 timer;
    s16 frame;
    s16 trailIdx;
    f32 alpha;
    f32 stepScale;
    f32 spin;
    f32 ringScale;
    Vec3f targetPos;
    Vec3f trail[15];
    Vec3f trailRot[15];
    Actor* hit[16]; // the shockwave hits each enemy once
    s16 hitCount;
    u32 baseFlags; // its own damage type (MsGanonMagic_PrepareAT picks the one for what it is about to hit)
    s16 pierce;    // having struck a boss, the frames it has flown on through him
    s16 level;     // the light ball: how long he charged it (0 a quick throw .. 3), how hard it hits
} MsGanonMagic;

void MsGanonMagic_Init(Actor* thisx, PlayState* play);
void MsGanonMagic_Destroy(Actor* thisx, PlayState* play);
void MsGanonMagic_Update(Actor* thisx, PlayState* play);
void MsGanonMagic_Draw(Actor* thisx, PlayState* play);

// The bosses' effects, kept here (BossGanon keeps them in play->specialEffects, a boss room's): advanced and drawn
// every frame by MergeShipGanondorf.c while Ganondorf mode is on.
void MsGanonMagic_UpdateEffects(PlayState* play);
void MsGanonMagic_DrawEffects(PlayState* play);
void MsGanonMagic_SpawnSparkle(Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, s16 fade);
void MsGanonMagic_SpawnLightRay(Vec3f* pos, Vec3f* velocity, Vec3f* accel, f32 scale, f32 length, s16 fade);
void MsGanonMagic_SpawnDustLight(Vec3f* pos, f32 scale, f32 target, s16 slot);
void MsGanonMagic_SpawnShockwave(Vec3f* pos, f32 scale, f32 target);
void MsGanonMagic_SpawnBlackDot(Vec3f* pos, f32 scale);
// EffectSsFhgFlash's light ball (Phantom Ganon's green light), kept here: the effect itself only spawns where his object
// is loaded (his boss room). color: EffectSsFhgFlash's sColors index (0 green).
void MsGanonMagic_SpawnFhgLight(Vec3f* pos, Vec3f* velocity, Vec3f* accel, s16 scale, u8 color);
// One of his thrown orbs (the light ball, Phantom Ganon's ball, the big magic): Dodongos swallow them as bombs.
s32 MsGanonMagic_IsOrb(Actor* actor);
// Any of his magic that hits (EnMThunder_IsFdSwordBeam: the bosses take it as they take the Fierce Deity's beam).
s32 MsGanonMagic_IsMagic(Actor* actor);
// Where the black dots converge (BossGanon's unk_278: the big magic's charge, in his hand).
extern Vec3f gMsGanonMagicChargePos;
// The next light ball's charge (0..3), taken by its Init.
extern s16 gMsGanonMagicChargeLevel;
// The actor's ActorDB id (MergeShipGanondorfActors.cpp); -1 until registered.
extern s16 gMergeShipActorId_MS_Ganon_Magic;


#endif
