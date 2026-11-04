// MergeShip2 -- Majora's Mask's swords used by the player (see MergeShipMMEquipment.h). Included at the end of
// z_player.c (after the forms' files: the Fierce Deity's port of MM Player_OverrideBlureColors is reused).

#include "soh/MergeShip/MergeShipMMEquipment.h"
#include "overlays/actors/ovl_MM_En_M_Thunder/z_mm_en_m_thunder.h"

// MM func_8083FFEC: each hit wears the Razor Sword; at 0 it is the Kokiri Sword again.
s32 MergeShip_MMEquip_RazorWear(PlayState* play, Player* this) {
    if (this->heldItemAction != PLAYER_IA_MM_SWORD_RAZOR) {
        return false;
    }
    if (gMergeShipSave.mmRazorHealth > 0) {
        // 2S2H VB_LOWER_RAZOR_SWORD_DURABILITY: off with gCheats.UnbreakableRazorSword and in a randomizer seed.
        if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.UnbreakableRazorSword"), 1) && !IS_RANDO) {
            gMergeShipSave.mmRazorHealth--;
        }
        if (gMergeShipSave.mmRazorHealth == 0) {
            // MM: Item_Give(play, ITEM_SWORD_KOKIRI), which takes the Razor Sword's place on B and in the save: MM's
            // own Kokiri Sword (2026-09-24: each game's Kokiri Sword is its own; Ocarina of Time's is not given).
            gMergeShipSave.mmEquipOwned &= ~(1 << MMEQUIP_RAZOR);
            gMergeShipSave.mmEquipSelected &= ~(1 << MMEQUIP_RAZOR);
            MergeShip_MMEquip_Give(play, ITEM_MM_SWORD_KOKIRI);
            // It takes the Razor's place in use too: Give leaves a new sword unselected ("Don't Auto-Equip New Swords
            // and Shields"), and Sanitize would then put Ocarina of Time's Kokiri Sword on B. A replacement, not a find.
            gMergeShipSave.mmEquipSelected |= (1 << MMEQUIP_KOKIRI);
            Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_KOKIRI);
            gSaveContext.equips.buttonItems[0] = ITEM_MM_SWORD_KOKIRI;
            Interface_LoadItemIcon1(play, 0);
            // MM: Player_UseItem(play, this, ITEM_SWORD_KOKIRI); Player_PlaySfx(this, NA_SE_IT_MAJIN_SWORD_BROKEN).
            Player_UseItem(play, this, ITEM_MM_SWORD_KOKIRI);
            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_MAJIN_SWORD_BROKEN);
        }
    }
    return true;
}

// MM func_808332A0 (isSwordBeam false) for the Razor, Gilded and Great Fairy's Swords: MM's wave, of the sword's own
// type (MM params `heldItemAction - PLAYER_IA_SWORD_KOKIRI`). Ocarina of Time's func_80837530 is otherwise the same.
s32 MergeShip_MMEquip_SpawnSpinWave(PlayState* play, Player* this, s32 magicCost) {
    Vec3f* waist = &this->bodyPartsPos[PLAYER_BODYPART_WAIST];
    s32 type;

    switch (this->heldItemAction) {
        case PLAYER_IA_MM_SWORD_RAZOR:
            type = MMENMTHUNDER_TYPE_RAZOR_SWORD;
            break;
        case PLAYER_IA_MM_SWORD_GILDED:
            type = MMENMTHUNDER_TYPE_GILDED_SWORD;
            break;
        case PLAYER_IA_MM_SWORD_GREAT_FAIRY:
            type = MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD;
            break;
        case PLAYER_IA_SWORD_KOKIRI: // MM's own Kokiri Sword: MM's wave too
            if (!MergeShip_MMEquip_HoldsMMKokiriSword(this)) {
                return false;
            }
            type = MMENMTHUNDER_TYPE_KOKIRI_SWORD;
            break;
        default:
            return false;
    }

    if (magicCost != 0) {
        this->unk_858 = 0.0f;
    } else {
        this->unk_858 = 0.5f;
    }
    this->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;

    if (this->actor.category == ACTORCAT_PLAYER) {
        if (gMergeShipActorId_En_M_Thunder >= 0) {
            Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_En_M_Thunder, waist->x, waist->y, waist->z, 0, 0, 0,
                        type | magicCost);
        } else {
            // MM's wave not registered: Ocarina of Time's, of the Master Sword's class (the Biggoron's for the Great
            // Fairy's Sword).
            Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, waist->x, waist->y, waist->z, 0, 0, 0,
                        ((type == MMENMTHUNDER_TYPE_GREAT_FAIRYS_SWORD) ? 3 : 1) | magicCost);
        }
    }
    return true;
}

// MM func_8082FA5C: Player_OverrideBlureColors(play, this, 0, 4) when a swing starts (white; MM keeps one trail
// colour for every sword).
void MergeShip_MMEquip_OnSwing(Player* this) {
    if (MERGESHIP_IA_IS_MM_SWORD(this->heldItemAction) && !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        MergeShip_FierceDeity_OverrideBlureColors(gPlayState, this, 0, 4);
    }
}
