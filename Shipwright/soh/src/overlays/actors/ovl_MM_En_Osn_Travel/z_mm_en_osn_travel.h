#ifndef Z_MM_EN_OSN_TRAVEL_H
#define Z_MM_EN_OSN_TRAVEL_H

// MergeShip2 (singleplayer) -- the traveling Happy Mask Salesman: Majora's Mask's En_Osn model and animations
// (mm.o2r object_osn) roaming Hyrule and selling MM's masks (soh/MergeShip/MergeShipSingleplayer.h). Registered at run
// time (MergeShip_SingleplayerActors_Register); params: MM_EN_OSN_TRAVEL_PARAMS_ROAM(radius).

#include <libultraship/libultra.h>
#include "global.h"
#include "soh/MergeShip/mmcompat/objects/object_osn/object_osn.h"

#define MM_EN_OSN_TRAVEL_PARAMS_ROAM(radius) ((s16)((radius) & 0x7FFF))
#define MM_EN_OSN_TRAVEL_GET_RADIUS(thisx) ((f32)((thisx)->params & 0x7FFF))

struct MMEnOsnTravel;

typedef void (*MMEnOsnTravelActionFunc)(struct MMEnOsnTravel*, PlayState*);

typedef struct MMEnOsnTravel {
    /* 0x000 */ Actor actor;
    ColliderCylinder collider;
    SkelAnime skelAnime;
    Vec3s jointTable[HAPPY_MASK_SALESMAN_LIMB_MAX];
    Vec3s morphTable[HAPPY_MASK_SALESMAN_LIMB_MAX];
    MMEnOsnTravelActionFunc actionFunc;
    Vec3s headRot;
    Vec3s torsoRot;
    Vec3f home;       // the roaming centre
    Vec3f target;     // where he is walking
    f32 radius;       // how far from home he roams
    s16 timer;
    s16 talkState;    // MMEnOsnTravelTalk
    s16 lastTextId;
    u8 animIndex;     // MMEnOsnTravelAnim
    u8 walking;
    u8 talked;        // greeted on this visit
    // The schedule (soh/MergeShip/MergeShipSingleplayer.h MsSalesmanPlan): roaming a stay, or walking through.
    s32 planMode;     // MS_SALESMAN_PLAN_STAY / _TRAVEL
    f32 way[24][3];
    s16 wayCount;
    s16 wayIndex;
    s32 stay;
    u8 fadeOut;
    u8 endAtHub;
    u8 leaving;       // fading out, then gone
    u8 alpha;
    // The walk-away clip: its root's travel moves the actor itself (the draw holds the root where it began).
    s16 walkAwayRootZ0;
    s16 walkAwayPrevRootZ;
    // The walk loop: its root last frame (MMEnOsnTravel_Update).
    s16 walkPrevRootZ;
    u8 walkPrevValid;
} MMEnOsnTravel;

#ifdef __cplusplus
extern "C" {
#endif

extern s16 gMergeShipActorId_MM_En_Osn_Travel;

void MMEnOsnTravel_Init(Actor* thisx, PlayState* play);
void MMEnOsnTravel_Destroy(Actor* thisx, PlayState* play);
void MMEnOsnTravel_Update(Actor* thisx, PlayState* play);
void MMEnOsnTravel_Draw(Actor* thisx, PlayState* play);
// Adds the actor to the ActorDB once (gMergeShipActorId_MM_En_Osn_Travel).
void MergeShip_SingleplayerActors_Register(void);

#ifdef __cplusplus
}
#endif

#endif
