#pragma once

// MergeShip2: Ganondorf mode (hidden). Typing "demonking" on the file select turns it on or off (the file select then
// plays Ganondorf's theme while it is on). Link is drawn as Ocarina of Time's Ganondorf -- the Hyrule-era one
// (object_gndd) as the child, the tower one (object_ganon) as the adult, both the same size and bigger than adult
// Link -- rigged onto Link's skeleton (tools/ganon_rig.py), and speaks with Ganondorf's voice.

#include "z64.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MERGESHIP_GANONDORF_CVAR "gMergeShip.GanondorfMode"

s32 MergeShip_Ganondorf_Enabled(void);
// The player (or a remote player in Anchor) is Ganondorf right now: the mode on for them and not transformed.
s32 MergeShip_Ganondorf_IsPlayer(Player* player);
// Player_DrawImpl: draws the player as Ganondorf (Link's skeleton posed as usual, Link's meshes hidden); false when the
// player is not drawn as Ganondorf (the mode off, a transformation, the equipment screen).
s32 MergeShip_Ganondorf_DrawPlayer(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount,
                                   OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* data,
                                   s32 lod, s32 eyeIndex, s32 firstPerson, s32 pause);
// Player_PlaySfx / Player_PlayVoiceSfx: one of Link's voice clips as Ganondorf's (or silence); true when handled.
s32 MergeShip_Ganondorf_PlayVoice(Actor* actor, u16 sfxId);
// Player_Update: the tower Ganondorf's cape (spawned with him, gone with him).
void MergeShip_Ganondorf_PlayerUpdate(PlayState* play, Player* player);
// func_80837948 (a sword attack starts): his magic instead (BossGanon's light balls).
void MergeShip_Ganondorf_OnMeleeAttack(PlayState* play, Player* player, s32 meleeWeaponAnimation);
// func_80837918: his swings carry no blade.
s32 MergeShip_Ganondorf_NoMeleeDamage(Player* player);
// His equipment (the user, 2026-10-17): the Kokiri Sword's place holds his hand (his magic on B), the Biggoron Sword's
// the Iron Knuckle's axe. True: the axe is his B (the tower Ganondorf, the Biggoron Sword's place equipped).
s32 MergeShip_Ganondorf_AxeOut(Player* player);
// The equipment page's sword slot `column` (0 Kokiri .. 2 Biggoron) drawn as his: its icon, else NULL.
const char* MergeShip_Ganondorf_SwordSlotIcon(s32 column);
// MergeShipGanondorfDL.cpp: the Iron Knuckle's axe, its colours its own (the silver Iron Knuckle's)
Gfx* MergeShip_Ganondorf_IronKnuckleAxeDL(void);
// The axe's head where it was last drawn (world), and the frame it was drawn on.
extern Vec3f gMsGanondorfAxeHead;
extern Vec3f gMsGanondorfAxeTip;
extern Vec3f gMsGanondorfAxeBase;
extern u32 gMsGanondorfAxeHeadFrame;
// EnArrow_Draw: one of his light balls (true: drawn, or gone once it has stopped).
s32 MergeShip_Ganondorf_DrawBall(Actor* actor, PlayState* play, s32 flying);
// Player_UseItem: his masks (the Deku Mask's place: Phantom Ganon; the Fierce Deity's: Beast Ganon). True: handled.
s32 MergeShip_Ganondorf_UseItem(PlayState* play, Player* player, s32 item);
// Anchor: what others see (0: Link; 1 + his form).
s32 MergeShip_Ganondorf_AnchorValue(void);
// Anchor (PlayerUpdate, BUG-0111): Beast Ganon's animation (its place in MergeShipGanondorf.c's list, -1: none) and
// frame; a remote Beast Ganon's, from his updates (DummyPlayer.cpp).
s32 MergeShip_Ganondorf_AnchorBeast(f32* frame);
void MergeShipAnchor_DummyGanondorfBeast(const Actor* actor, s32* index, f32* frame);
// ---- his abilities (MergeShipGanondorfPlayer.inc, compiled into z_player.c) and what they draw (here) ----

#define MS_GANON_FORM_GANONDORF 0
#define MS_GANON_FORM_PHANTOM 1
#define MS_GANON_FORM_BEAST 2
s32 MergeShip_Ganondorf_Form(void);
void MergeShip_Ganondorf_SetForm(s32 form);
// The local player's form after a transformation's flash (MergeShip_Action_PutOnMask), -1: none pending.
extern s32 gMsGanondorfPendingForm;
// This frame's boss pose: a tower Ganondorf (or, as Phantom Ganon, a Phantom Ganon) animation at `frame`, blended over
// Link's own pose by `weight`. Retargeted onto Link's skeleton when he is drawn (after the frame's animation loads).
void MergeShip_Ganondorf_SetPose(const char* anim, f32 frame, f32 weight);
// Beast Ganon's own skeleton: the animation an ability plays (anim NULL: back to walking / guarding by itself).
void MergeShip_Ganondorf_BeastAnim(const char* anim, f32 speed, s32 loop, f32 morph);
f32 MergeShip_Ganondorf_BeastFrame(void);
s32 MergeShip_Ganondorf_BeastOnFrame(f32 frame);

// What his magic shows on him (BossGanon_Draw's hand ball, BossGanon_DrawBigMagicCharge's charge; set by the abilities,
// drawn with him).
typedef struct {
    f32 handBallScale; // BossGanon handLightBallScale
    f32 handBallSpin;  // unk_258
    f32 bigCircle;     // unk_284
    f32 bigCircleAlpha; // unk_288
    f32 bigFleck;      // unk_28C
    f32 bigFleckAlpha; // unk_290
    f32 bigCore;       // unk_2D0
    s16 bigRays;       // unk_1AC
    f32 bigRayAlpha[10]; // unk_294
    s16 bigSeed;       // unk_1AA
    u8 barrier;        // blocking: his barrier
} MsGanondorfFx;
extern MsGanondorfFx gMsGanondorfFx;
// Where his limbs were last drawn: the right hand's light (BossGanon unk_260), his chest (unk_1FC), Phantom Ganon's
// spear tip (BossGanondrof spearTip).
extern Vec3f gMsGanondorfHandPos;
extern Vec3f gMsGanondorfChestPos;
extern Vec3f gMsGanondorfSpearTip;

// His colours (the Anchor menu, shown while Ganondorf mode is on): his armour's brown leather and his cape's red.
// 0: as they are; else 0x01RRGGBB. The player's from the menu, a remote player's from Anchor.
#define MERGESHIP_GANONDORF_ARMOR_CVAR "gMergeShip.GanondorfArmor"
#define MERGESHIP_GANONDORF_CAPE_CVAR "gMergeShip.GanondorfCape"
u32 MergeShip_Ganondorf_ArmorColor(void);
u32 MergeShip_Ganondorf_CapeColor(void);
// MergeShipGanondorfDL.cpp: `path`'s list recoloured (null: the list as it is), and the copies' upkeep.
Gfx* MergeShip_Ganondorf_ColorDL(const char* path, u32 armor, u32 cape);
void MergeShip_Ganondorf_ColorFrameBegin(void);

// His ocarina is the organ of his tower: while he plays it, the organ stands before him (MergeShipGanondorfDL.cpp's
// copy of EnGanonOrgan's), he plays it as BossGanon does (gGanondorfPlayOrganAnim), the camera shows him at it, and the
// ocarina's notes sound on the organ (audio_seqplayer.c: font 30's organ on the ocarina channel while this is set).
extern volatile u8 gMergeShipGanonOrgan;
Gfx* MergeShip_Ganondorf_OrganDL(void);

// His masks (the Deku Mask's slot: Phantom Ganon's; the Fierce Deity's: Ganon's face), drawn as Link's transformation
// masks are -- in his hand, then on his face, then pulled off (z_player_lib.c MergeShip_DrawMaskOnHead / InHand). The
// matrix is Link's head limb's at the size his head is drawn (MS_GANONDORF_MASK_SCALE). True: drawn (he is Ganondorf).
s32 MergeShip_Ganondorf_DrawMask(PlayState* play, Player* player, s32 mask, s32 inHand);
Gfx* MergeShip_Ganondorf_PhantomMaskDL(void); // MergeShipGanondorfDL.cpp: his head limb's mask parts
void MergeShip_Ganondorf_CapeBlock(s32 phase); // his cape in his guard (BossGanon_Block's)

// The file select: watches the keyboard for the password (FileChoose_Main), and which music it plays.
void MergeShip_Ganondorf_FileSelectUpdate(void);
u16 MergeShip_Ganondorf_FileSelectBgm(void);

#ifdef __cplusplus
}
#endif
