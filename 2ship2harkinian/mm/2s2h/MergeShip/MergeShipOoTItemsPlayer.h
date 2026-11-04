#pragma once

// MergeShip2 -- the player side of Ocarina of Time's items (MergeShipOoTItemsPlayer.inc, included at the end of
// z_player.c). Prototypes for z_player.c's hooks and tables.

#include "2s2h/MergeShip/MergeShipOoTItems.h"

struct AttackAnimInfo;

s32 MergeShip_OoTItems_UseItem(PlayState* play, Player* this, ItemId item, PlayerItemAction itemAction);
s32 MergeShip_OoTItems_ActionHandler13(Player* this, PlayState* play);
void MergeShip_OoTItems_PlayerInit(PlayState* play, Player* this);
void MergeShip_OoTItems_PlayerUpdate(PlayState* play, Player* this);
s32 MergeShip_OoTItems_SlingshotAmmo(PlayState* play, Player* this, ItemId* item, ArrowType* typeParam);
u16 MergeShip_OoTItems_BowSfx(Player* this, u16 bowSfx);
void MergeShip_OoTItems_InitBoomerangIA(PlayState* play, Player* this);
s32 MergeShip_OoTItems_UpperAction_BoomerangWait(Player* this, PlayState* play);
struct AttackAnimInfo* MergeShip_OoTItems_AttackAnimInfo(Player* this, s32 meleeWeaponAnim);
s32 MergeShip_OoTItems_HammerAttack(Player* this);
void MergeShip_OoTItems_HammerDamage(Player* this, s32 meleeWeaponAnim);
s32 MergeShip_OoTItems_HammerHit(PlayState* play, Player* this);
void MergeShip_OoTItems_HammerSwing(PlayState* play, Player* this);
