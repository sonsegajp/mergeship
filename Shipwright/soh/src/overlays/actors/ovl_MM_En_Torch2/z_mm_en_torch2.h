#ifndef Z_MM_EN_TORCH2_H
#define Z_MM_EN_TORCH2_H

// MergeShip2 (BUG-0144) -- Majora's Mask's Elegy of Emptiness in Hyrule: its shell (MM ovl_En_Torch2, "Elegy of
// Emptiness Shell") and the beam of light that makes it (MM ovl_Eff_Change), ported from
// 2ship2harkinian/mm/src/overlays/actors. Registered at run time (soh/MergeShip/MergeShipActorMMEnTorch2.cpp) and
// spawned when the Elegy is played where Hyrule allows it (MergeShipSongs.c, MM Player_Action_88 / func_80848640).

#include <libultraship/libultra.h>
#include "global.h"
#include "soh/MergeShip/MergeShipKeyframe.h"

// The shell's params: the form it is the shell of, as MM numbers them (PlayerTransformation: MM spawns it with
// player->transformation). The Fierce Deity's is the human's shell, as in MM.
typedef enum {
    /* 0 */ MM_TORCH2_PARAM_FIERCE_DEITY,
    /* 1 */ MM_TORCH2_PARAM_GORON,
    /* 2 */ MM_TORCH2_PARAM_ZORA,
    /* 3 */ MM_TORCH2_PARAM_DEKU,
    /* 4 */ MM_TORCH2_PARAM_HUMAN,
    /* 5 */ MM_TORCH2_PARAM_MAX
} MMEnTorch2Param;

typedef enum {
    /* 0 */ MM_TORCH2_STATE_INITIALIZED,
    /* 1 */ MM_TORCH2_STATE_FADING_IN,
    /* 2 */ MM_TORCH2_STATE_SOLID,
    /* 3 */ MM_TORCH2_STATE_IDLE,
    /* 4 */ MM_TORCH2_STATE_DYING
} MMEnTorch2State;

typedef struct MMEnTorch2 {
    Actor actor;
    ColliderCylinder collider;
    u8 state;
    u8 framesUntilNextState;
    s16 alpha;
} MMEnTorch2;

struct MMEffChange;
typedef void (*MMEffChangeActionFunc)(struct MMEffChange*, PlayState*);

// MM EffChange: the beam's keyframe skeleton (6 limbs) and its colours (params & 7).
typedef struct MMEffChange {
    Actor actor;
    KFSkelAnimeFlex kfSkelAnime;
    Vec3s jointTable[6];
    Vec3s morphTable[6];
    s16 step;
    u8 primColors[4];
    u8 envColors[4];
    MMEffChangeActionFunc actionFunc;
} MMEffChange;

#define MM_EFFCHANGE_GET_COLORS(thisx) ((thisx)->params & 7)

extern s16 gMergeShipActorId_En_Torch2;
extern s16 gMergeShipActorId_Eff_Change;
// MM ActorContext.elegyShells: the shell of each form in the scene (NULL: none).
extern MMEnTorch2* gMergeShipElegyShells[MM_TORCH2_PARAM_MAX];

void MMEnTorch2_Init(Actor* thisx, PlayState* play);
void MMEnTorch2_Destroy(Actor* thisx, PlayState* play);
void MMEnTorch2_Update(Actor* thisx, PlayState* play);
void MMEnTorch2_Draw(Actor* thisx, PlayState* play);

void MMEffChange_Init(Actor* thisx, PlayState* play);
void MMEffChange_Destroy(Actor* thisx, PlayState* play);
void MMEffChange_Update(Actor* thisx, PlayState* play);
void MMEffChange_Draw(Actor* thisx, PlayState* play);

#endif // Z_MM_EN_TORCH2_H
