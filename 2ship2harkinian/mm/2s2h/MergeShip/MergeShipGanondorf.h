#pragma once

// MergeShip2 -- Ganondorf mode in Termina (2Ship): the player as Ganondorf, as SoH's soh/MergeShip/MergeShipGanondorf.c
// makes him in Hyrule -- the young Ganondorf on MM's young human, the tower Ganondorf on the adult
// (MergeShipAdultLink.h), with the tower's cape (OoT's EnGanonMant: ovl_OoT_En_Ganon_Mant), his colours, what he holds
// and wears. His models are Ocarina of Time's (the archive mounted under oot/, MergeShipAssets.h); his rig is SoH's
// generated one (tools/ganon_rig.py: MergeShipGanondorfRig.inc), shared.
//
// The mode and his colours are SoH's CVars (the password on SoH's file select, its Anchor menu), shared with 2Ship by
// the settings map (include/mergeship/SettingsMap.def).

#include "ultra64.h"

#define MERGESHIP_GANONDORF_CVAR "gMergeShip.GanondorfMode"
#define MERGESHIP_GANONDORF_ARMOR_CVAR "gMergeShip.GanondorfArmor"
#define MERGESHIP_GANONDORF_CAPE_CVAR "gMergeShip.GanondorfCape"

// His forms (SoH's): Ganondorf, Phantom Ganon (the Deku Mask), Beast Ganon (the Fierce Deity's Mask).
#define MS_GANON_FORM_GANONDORF 0
#define MS_GANON_FORM_PHANTOM 1
#define MS_GANON_FORM_BEAST 2

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;
struct Actor;
struct Player;

s32 MergeShip_Ganondorf_Enabled(void);
// Anchor (PlayerUpdate): 0 off, else 1 + his form -- and a remote player's, from theirs (DummyPlayer.cpp).
s32 MergeShip_Ganondorf_AnchorValue(void);
// Anchor (BUG-0111): Beast Ganon's animation (its place in MergeShipGanondorf.c's list, -1: none) and frame, and a
// remote Beast Ganon's (DummyPlayer.cpp).
s32 MergeShip_Ganondorf_AnchorBeast(f32* frame);
void MergeShipAnchor_DummyGanondorfBeast(const struct Actor* actor, s32* index, f32* frame);
s32 MergeShipAnchor_DummyIsGanondorf(const struct Actor* actor);
u32 MergeShipAnchor_DummyGanondorfColor(const struct Actor* actor, s32 cape);
// The player is drawn as Ganondorf (the mode on, the human).
s32 MergeShip_Ganondorf_IsPlayer(struct Player* player);
s32 MergeShip_Ganondorf_Form(void);
void MergeShip_Ganondorf_SetForm(s32 form);
// His colours: 0 his own, else 0x01RRGGBB.
u32 MergeShip_Ganondorf_ArmorColor(void);
u32 MergeShip_Ganondorf_CapeColor(void);
// Player_DrawImpl, in place of the skeleton's draw: Ganondorf on it. True: drawn (Link's own draw is skipped).
s32 MergeShip_Ganondorf_DrawPlayer(struct PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod,
                                   void* overrideLimbDraw, void* postLimbDraw, struct Actor* actor);
// Player_Update, each frame: his cape, Beast Ganon, his organ, his magic's effects.
void MergeShip_Ganondorf_PlayerUpdate(struct PlayState* play, struct Player* player);

// ---- his abilities (MergeShipGanondorfPlayer.inc, compiled into z_player.c) and what they draw (here) ----
// The local player's form after a transformation's flash, -1: none pending.
extern s32 gMsGanondorfPendingForm;
// This frame's boss pose: a tower Ganondorf (or, as Phantom Ganon, a Phantom Ganon) animation at `frame`, blended over
// Link's own pose by `weight`. Retargeted onto Link's skeleton when he is drawn.
void MergeShip_Ganondorf_SetPose(const char* anim, f32 frame, f32 weight);
// Beast Ganon's own skeleton: the animation an ability plays (anim NULL: back to walking / guarding by itself).
void MergeShip_Ganondorf_BeastAnim(const char* anim, f32 speed, s32 loop, f32 morph);
f32 MergeShip_Ganondorf_BeastFrame(void);
s32 MergeShip_Ganondorf_BeastOnFrame(f32 frame);

// What his magic shows on him (BossGanon_Draw's hand ball, BossGanon_DrawBigMagicCharge's charge).
typedef struct {
    f32 handBallScale;
    f32 handBallSpin;
    f32 bigCircle;
    f32 bigCircleAlpha;
    f32 bigFleck;
    f32 bigFleckAlpha;
    f32 bigCore;
    s16 bigRays;
    f32 bigRayAlpha[10];
    s16 bigSeed;
    u8 barrier;
} MsGanondorfFx;
extern MsGanondorfFx gMsGanondorfFx;
// Where his limbs were last drawn: the right hand's light, his chest, Phantom Ganon's spear tip.
extern Vec3f gMsGanondorfHandPos;
extern Vec3f gMsGanondorfChestPos;
extern Vec3f gMsGanondorfSpearTip;

// His ocarina is the organ of his tower (the organ stands before him, the camera shows him at it).
extern volatile u8 gMergeShipGanonOrgan;

// His masks (the Deku Mask's slot: Phantom Ganon's; the Fierce Deity's: Ganon's face), drawn on his head or in his hand.
s32 MergeShip_Ganondorf_DrawMask(struct PlayState* play, struct Player* player, s32 mask, s32 inHand);
void MergeShip_Ganondorf_CapeBlock(s32 phase);
// Player_UseItem: his masks put on or taken off (true: handled).
s32 MergeShip_Ganondorf_UseItem(struct PlayState* play, struct Player* player, s32 item);
s32 MergeShip_Ganondorf_StartTransform(struct PlayState* play, struct Player* player, s32 form, s32 mask);
// Link's voice (MM's NA_SE_VO_LI_*) as his: true, played (Link's is not).
s32 MergeShip_Ganondorf_PlayVoice(struct Actor* actor, u16 sfxId);
s32 MergeShip_Ganondorf_NoMeleeDamage(struct Player* player);
// The pause screen's figure (MergeShipPauseLink.c, BUG-0133): his scale against Link's frame (1 when he is not drawn).
f32 MergeShip_Ganondorf_PauseScale(struct Player* player);
// His axe (the Iron Knuckle's) is his B: the tower Ganondorf, Ocarina of Time's Biggoron Sword or Giant's Knife on B.
s32 MergeShip_Ganondorf_AxeOut(struct Player* player);
// MergeShipGanondorfDL.cpp: the Iron Knuckle's axe, its colours its own (the silver Iron Knuckle's)
Gfx* MergeShip_Ganondorf_IronKnuckleAxeDL(void);
// The axe's head where it was last drawn (world), its trail's points, and the frame it was drawn on.
extern Vec3f gMsGanondorfAxeHead;
extern Vec3f gMsGanondorfAxeTip;
extern Vec3f gMsGanondorfAxeBase;
extern u32 gMsGanondorfAxeHeadFrame;
// Player_GetHeight for him (MM's camera frames the player by it): his own height, as the Fierce Deity's 124.
f32 MergeShip_Ganondorf_Height(struct Player* player, f32 height);
void MergeShip_Ganondorf_OnMeleeAttack(struct PlayState* play, struct Player* player, s32 meleeWeaponAnimation);

// MergeShipGanondorfDL.cpp
Gfx* MergeShip_Ganondorf_StripHandDL(const char* path);
Gfx* MergeShip_Ganondorf_ColorDL(const char* path, u32 armor, u32 cape);
void MergeShip_Ganondorf_ColorFrameBegin(void);
Gfx* MergeShip_Ganondorf_OrganDL(void);
Gfx* MergeShip_Ganondorf_PhantomMaskDL(void);

#ifdef __cplusplus
}
#endif
