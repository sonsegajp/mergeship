// MergeShip2 -- Majora's Mask FierceDeity form, ported from 2ship2harkinian/mm into Ocarina of Time's player.
// Included at the end of z_player.c. See docs/PORTING_GUIDE.md.
//
// Everything here is Fierce Deity's path through MM's shared player code
// (2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/z_player.c unless stated). Each function names
// its MM original. MM fields that Ocarina of Time's Player already has under another name are used by the
// OoT name:
//   MM unk_ADC (attack/charge timer)  -> OoT unk_844     MM unk_ADD (combo counter) -> OoT unk_845
//   MM unk_B08 (spin charge)          -> OoT unk_858     MM unk_B40 (guard side)    -> OoT unk_870
//   MM unk_B50 (speed cap)            -> OoT unk_880     MM speedXZ                 -> OoT linearVelocity
//   MM sPlayerUseHeldItem             -> sUseHeldItem    MM meleeWeaponEffectIndex[0/1] -> OoT's single blure
// this->meleeWeaponAnimation keeps Ocarina of Time's PlayerMeleeWeaponAnimation numbering (the shared OoT
// code reads it); MM's table below is re-indexed to it. The only difference for the Fierce Deity is that MM
// inserts its Zora jump kick between the jump-slash start and finish, so MM's "+3" to the finish is "+2" here.
//
// ---- Damage mapping (MM -> Ocarina of Time) -------------------------------------------------------------
// MM multiplies each attack's damage value by the enemy's table; Ocarina of Time reads the damage from the
// enemy's table by damage flag and ignores the value. Fierce Deity's sword (MM D_8085D09C
// PLAYER_MELEEWEAPON_SWORD_TWO_HANDED: DMG_SWORD, 4 normal / 8 strong for a transformed player) is four
// times the Kokiri Sword (1 / 2), which is exactly Ocarina of Time's Biggoron class, so:
//   FD slash / stab / combo / spin (MM DMG_SWORD, 4)   -> DMG_SLASH_GIANT (OoT tables: typically 4)
//   FD jump slash (MM DMG_SWORD, 8, "strong")           -> DMG_JUMP_GIANT  (OoT tables: typically 8)
//   FD with nothing in hand (MM row 0: DMG_GORON_PUNCH, 2) -> DMG_HAMMER_SWING (OoT's blunt melee flag)
//   FD sword beam (MM En_M_Thunder, DMG_SWORD_BEAM, 3) -> soh_fd's beam: DMG_SLASH_MASTER, 3 (see ovl_En_M_Thunder)
// MM's damage value is written to the toucher too, for colliders without a damage table (OoT then uses it).

void MergeShipTest_Log(const char* fmt, ...); // MergeShipTest.cpp (no-op outside the harness)

#define FDANIM(name) ((LinkAnimationHeader*)mm_gPlayerAnim_##name)
#define OOTANIM(name) ((LinkAnimationHeader*)gPlayerAnim_##name)

// ---- MM sMeleeAttackAnimInfo (AttackAnimInfo), for the Fierce Deity ------------------------------------
typedef struct MergeShipFDAttackAnimInfo {
    /* 0x0 */ LinkAnimationHeader* unk_0;
    /* 0x4 */ LinkAnimationHeader* unk_4;
    /* 0x8 */ LinkAnimationHeader* unk_8;
    /* 0xC */ u8 unk_C;
    /* 0xD */ u8 unk_D;
} MergeShipFDAttackAnimInfo;

// MM sMeleeAttackAnimInfo with MM's gameplay_keep animations, indexed by Ocarina of Time's
// PlayerMeleeWeaponAnimation. The two hammer rows do not exist in MM and cannot be reached by the
// Fierce Deity (they are chosen only for PLAYER_IA_HAMMER); they repeat MM's two-handed forward slash.
static MergeShipFDAttackAnimInfo sMergeShipFDMeleeAttackAnimInfo[PLAYER_MWA_MAX] = {
    // PLAYER_MWA_FORWARD_SLASH_1H
    { FDANIM(link_fighter_normal_kiru), FDANIM(link_fighter_normal_kiru_end), FDANIM(link_fighter_normal_kiru_endR),
      1, 4 },
    // PLAYER_MWA_FORWARD_SLASH_2H
    { FDANIM(link_fighter_Lnormal_kiru), FDANIM(link_fighter_Lnormal_kiru_end), FDANIM(link_anchor_Lnormal_kiru_endR),
      1, 4 },
    // PLAYER_MWA_FORWARD_COMBO_1H
    { FDANIM(link_fighter_normal_kiru_finsh), FDANIM(link_fighter_normal_kiru_finsh_end),
      FDANIM(link_anchor_normal_kiru_finsh_endR), 0, 5 },
    // PLAYER_MWA_FORWARD_COMBO_2H
    { FDANIM(link_fighter_Lnormal_kiru_finsh), FDANIM(link_fighter_Lnormal_kiru_finsh_end),
      FDANIM(link_anchor_Lnormal_kiru_finsh_endR), 1, 7 },
    // PLAYER_MWA_RIGHT_SLASH_1H
    { FDANIM(link_fighter_Lside_kiru), FDANIM(link_fighter_Lside_kiru_end), FDANIM(link_anchor_Lside_kiru_endR), 1,
      4 },
    // PLAYER_MWA_RIGHT_SLASH_2H
    { FDANIM(link_fighter_LLside_kiru), FDANIM(link_fighter_LLside_kiru_end), FDANIM(link_anchor_LLside_kiru_endL), 0,
      5 },
    // PLAYER_MWA_RIGHT_COMBO_1H
    { FDANIM(link_fighter_Lside_kiru_finsh), FDANIM(link_fighter_Lside_kiru_finsh_end),
      FDANIM(link_anchor_Lside_kiru_finsh_endR), 2, 8 },
    // PLAYER_MWA_RIGHT_COMBO_2H
    { FDANIM(link_fighter_LLside_kiru_finsh), FDANIM(link_fighter_LLside_kiru_finsh_end),
      FDANIM(link_anchor_LLside_kiru_finsh_endR), 3, 8 },
    // PLAYER_MWA_LEFT_SLASH_1H
    { FDANIM(link_fighter_Rside_kiru), FDANIM(link_fighter_Rside_kiru_end), FDANIM(link_anchor_Rside_kiru_endR), 0,
      4 },
    // PLAYER_MWA_LEFT_SLASH_2H
    { FDANIM(link_fighter_LRside_kiru), FDANIM(link_fighter_LRside_kiru_end), FDANIM(link_anchor_LRside_kiru_endR), 0,
      5 },
    // PLAYER_MWA_LEFT_COMBO_1H
    { FDANIM(link_fighter_Rside_kiru_finsh), FDANIM(link_fighter_Rside_kiru_finsh_end),
      FDANIM(link_anchor_Rside_kiru_finsh_endR), 0, 6 },
    // PLAYER_MWA_LEFT_COMBO_2H
    { FDANIM(link_fighter_LRside_kiru_finsh), FDANIM(link_fighter_LRside_kiru_finsh_end),
      FDANIM(link_anchor_LRside_kiru_finsh_endL), 1, 5 },
    // PLAYER_MWA_STAB_1H
    { FDANIM(link_fighter_pierce_kiru), FDANIM(link_fighter_pierce_kiru_end), FDANIM(link_anchor_pierce_kiru_endR), 0,
      3 },
    // PLAYER_MWA_STAB_2H
    { FDANIM(link_fighter_Lpierce_kiru), FDANIM(link_fighter_Lpierce_kiru_end), FDANIM(link_anchor_Lpierce_kiru_endL),
      0, 3 },
    // PLAYER_MWA_STAB_COMBO_1H
    { FDANIM(link_fighter_pierce_kiru_finsh), FDANIM(link_fighter_pierce_kiru_finsh_end),
      FDANIM(link_anchor_pierce_kiru_finsh_endR), 1, 9 },
    // PLAYER_MWA_STAB_COMBO_2H
    { FDANIM(link_fighter_Lpierce_kiru_finsh), FDANIM(link_fighter_Lpierce_kiru_finsh_end),
      FDANIM(link_anchor_Lpierce_kiru_finsh_endR), 1, 8 },
    // PLAYER_MWA_FLIPSLASH_START
    { FDANIM(link_fighter_jump_rollkiru), FDANIM(link_fighter_jump_kiru_finsh), FDANIM(link_fighter_jump_kiru_finsh),
      7, 99 },
    // PLAYER_MWA_JUMPSLASH_START
    { FDANIM(link_fighter_Lpower_jump_kiru), FDANIM(link_fighter_Lpower_jump_kiru_hit),
      FDANIM(link_fighter_Lpower_jump_kiru_hit), 7, 99 },
    // PLAYER_MWA_FLIPSLASH_FINISH
    { FDANIM(link_fighter_jump_kiru_finsh), FDANIM(link_fighter_jump_kiru_finsh_end),
      FDANIM(link_fighter_jump_kiru_finsh_end), 1, 2 },
    // PLAYER_MWA_JUMPSLASH_FINISH
    { FDANIM(link_fighter_Lpower_jump_kiru_hit), FDANIM(link_fighter_Lpower_jump_kiru_end),
      FDANIM(link_fighter_Lpower_jump_kiru_end), 1, 2 },
    // PLAYER_MWA_BACKSLASH_RIGHT
    { FDANIM(link_fighter_turn_kiruR), FDANIM(link_fighter_turn_kiruR_end), FDANIM(link_fighter_turn_kiruR_end), 1,
      5 },
    // PLAYER_MWA_BACKSLASH_LEFT
    { FDANIM(link_fighter_turn_kiruL), FDANIM(link_fighter_turn_kiruL_end), FDANIM(link_fighter_turn_kiruL_end), 1,
      4 },
    // PLAYER_MWA_HAMMER_FORWARD (not in MM; unreachable for the Fierce Deity)
    { FDANIM(link_fighter_Lnormal_kiru), FDANIM(link_fighter_Lnormal_kiru_end), FDANIM(link_anchor_Lnormal_kiru_endR),
      1, 4 },
    // PLAYER_MWA_HAMMER_SIDE (not in MM; unreachable for the Fierce Deity)
    { FDANIM(link_fighter_Lnormal_kiru), FDANIM(link_fighter_Lnormal_kiru_end), FDANIM(link_anchor_Lnormal_kiru_endR),
      1, 4 },
    // PLAYER_MWA_SPIN_ATTACK_1H
    { FDANIM(link_fighter_rolling_kiru), FDANIM(link_fighter_rolling_kiru_end),
      FDANIM(link_anchor_rolling_kiru_endR), 0, 12 },
    // PLAYER_MWA_SPIN_ATTACK_2H
    { FDANIM(link_fighter_Lrolling_kiru), FDANIM(link_fighter_Lrolling_kiru_end),
      FDANIM(link_anchor_Lrolling_kiru_endR), 0, 15 },
    // PLAYER_MWA_BIG_SPIN_1H
    { FDANIM(link_fighter_Wrolling_kiru), FDANIM(link_fighter_Wrolling_kiru_end),
      FDANIM(link_anchor_rolling_kiru_endR), 0, 16 },
    // PLAYER_MWA_BIG_SPIN_2H
    { FDANIM(link_fighter_Wrolling_kiru), FDANIM(link_fighter_Wrolling_kiru_end),
      FDANIM(link_anchor_Lrolling_kiru_endR), 0, 16 },
};

// MM D_8085CF50 / D_8085CF58: the spin attack charge start.
static LinkAnimationHeader* sMergeShipFDSpinStartAnims[] = {
    FDANIM(link_fighter_power_kiru_start),
    FDANIM(link_fighter_Lpower_kiru_start),
};
static LinkAnimationHeader* sMergeShipFDSpinStartLAnims[] = {
    FDANIM(link_fighter_power_kiru_startL),
    FDANIM(link_fighter_Lpower_kiru_start),
};

// MM D_8085D294: recoil when the sword hits a wall or bounces.
static LinkAnimationHeader* sMergeShipFDReboundAnims[] = {
    FDANIM(link_fighter_rebound),
    FDANIM(link_fighter_rebound_long),
    FDANIM(link_fighter_reboundR),
    FDANIM(link_fighter_rebound_longR),
};

// Ocarina of Time's sword tables that the Fierce Deity passes through unchanged in MM's logic (spin charge
// D_80854360..78 = MM D_8085CF60..78, guard D_808543A4..C4 = MM D_8085CFBC..DC): the OoT animation and the
// MM animation of the same name, so those shared functions play MM's animation for the Fierce Deity.
static struct {
    LinkAnimationHeader* oot;
    LinkAnimationHeader* mm;
} sMergeShipFDAnimSwaps[] = {
    { OOTANIM(link_fighter_power_kiru_start), FDANIM(link_fighter_power_kiru_start) },
    { OOTANIM(link_fighter_Lpower_kiru_start), FDANIM(link_fighter_Lpower_kiru_start) },
    { OOTANIM(link_fighter_power_kiru_startL), FDANIM(link_fighter_power_kiru_startL) },
    { OOTANIM(link_fighter_power_kiru_wait), FDANIM(link_fighter_power_kiru_wait) },
    { OOTANIM(link_fighter_Lpower_kiru_wait), FDANIM(link_fighter_Lpower_kiru_wait) },
    { OOTANIM(link_fighter_power_kiru_wait_end), FDANIM(link_fighter_power_kiru_wait_end) },
    { OOTANIM(link_fighter_Lpower_kiru_wait_end), FDANIM(link_fighter_Lpower_kiru_wait_end) },
    { OOTANIM(link_fighter_power_kiru_walk), FDANIM(link_fighter_power_kiru_walk) },
    { OOTANIM(link_fighter_Lpower_kiru_walk), FDANIM(link_fighter_Lpower_kiru_walk) },
    { OOTANIM(link_fighter_power_kiru_side_walk), FDANIM(link_fighter_power_kiru_side_walk) },
    { OOTANIM(link_fighter_Lpower_kiru_side_walk), FDANIM(link_fighter_Lpower_kiru_side_walk) },
    { OOTANIM(link_anchor_waitR2defense), FDANIM(link_anchor_waitR2defense) },
    { OOTANIM(link_anchor_waitR2defense_long), FDANIM(link_anchor_waitR2defense_long) },
    { OOTANIM(link_anchor_waitL2defense), FDANIM(link_anchor_waitL2defense) },
    { OOTANIM(link_anchor_waitL2defense_long), FDANIM(link_anchor_waitL2defense_long) },
    { OOTANIM(link_anchor_defense_hit), FDANIM(link_anchor_defense_hit) },
    { OOTANIM(link_anchor_defense_long_hitL), FDANIM(link_anchor_defense_long_hitL) },
    { OOTANIM(link_anchor_defense_long_hitR), FDANIM(link_anchor_defense_long_hitR) },
    { OOTANIM(link_normal_defense_hit), FDANIM(link_normal_defense_hit) },
    { OOTANIM(link_fighter_defense_long_hit), FDANIM(link_fighter_defense_long_hit) },
};

LinkAnimationHeader* MergeShip_FierceDeity_Anim(Player* this, LinkAnimationHeader* anim) {
    s32 i;

    if (this->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) {
        return anim;
    }
    for (i = 0; i < ARRAY_COUNT(sMergeShipFDAnimSwaps); i++) {
        if (sMergeShipFDAnimSwaps[i].oot == anim) {
            return sMergeShipFDAnimSwaps[i].mm;
        }
    }
    return anim;
}

// ---- small MM library helpers (z_lib.c) that Ocarina of Time's does not have ------------------------------
static void MergeShip_FD_Vec3f_SumScaled(Vec3f* a, Vec3f* b, f32 scale, Vec3f* dest) {
    dest->x = b->x * scale + a->x;
    dest->y = b->y * scale + a->y;
    dest->z = b->z * scale + a->z;
}
static void MergeShip_FD_Vec3f_AddRand(Vec3f* orig, f32 scale, Vec3f* dest) {
    dest->x = Rand_CenteredFloat(scale) + orig->x;
    dest->y = Rand_CenteredFloat(scale) + orig->y;
    dest->z = Rand_CenteredFloat(scale) + orig->z;
}
static void MergeShip_FD_Vec3f_DistXYZAndStoreNormDiff(Vec3f* a, Vec3f* b, f32 scale, Vec3f* dest) {
    f32 diff = Math_Vec3f_DistXYZAndStoreDiff(a, b, dest);
    f32 normScale;

    if (diff == 0) {
        return;
    }
    normScale = scale / diff;
    dest->x *= normScale;
    dest->y *= normScale;
    dest->z *= normScale;
}
static void MergeShip_FD_Vec3f_ScaleAndStore(Vec3f* vec, f32 scale, Vec3f* dest) {
    dest->x = vec->x * scale;
    dest->y = vec->y * scale;
    dest->z = vec->z * scale;
}

// MM Magic_Consume(play, amount, MAGIC_CONSUME_DEITY_BEAM) (z_parameter.c): consumed at once, not through a
// request. MergeShip2: MM's Chateau Romani takes nothing while its effect lasts (MergeShipChateau.h).
static s32 MergeShip_FierceDeity_ConsumeDeityBeamMagic(PlayState* play, s16 magicToConsume) {
    if (!gSaveContext.isMagicAcquired) {
        return false;
    }
    if ((gSaveContext.magic - magicToConsume) < 0) {
        if (gSaveContext.magicCapacity != 0) {
            MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
        }
        return false;
    }
    if ((gSaveContext.magicState == MAGIC_STATE_IDLE) || (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS)) {
        if (gSaveContext.magicState == MAGIC_STATE_CONSUME_LENS) {
            play->actorCtx.lensActive = false;
        }
        if (MergeShip_Chateau_IsActive()) { // MergeShip2: MM CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI)
            magicToConsume = 0;
        }
        gSaveContext.magic -= magicToConsume;
        return true;
    }
    MergeShip_PlayMMSfxCentered(MM_NA_SE_SY_ERROR);
    return false;
}

// ---- blure (MM Player_OverrideBlureColors, D_8085CF88) ---------------------------------------------------
typedef struct MergeShipBlureColors {
    u8 p1StartColor[4];
    u8 p2StartColor[4];
    u8 p1EndColor[4];
    u8 p2EndColor[4];
} MergeShipBlureColors;

static MergeShipBlureColors sMergeShipFDBlureColors[] = {
    { { 255, 255, 255, 255 }, { 255, 255, 255, 64 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 } },
    { { 165, 185, 255, 185 }, { 205, 225, 255, 50 }, { 255, 255, 255, 0 }, { 255, 255, 255, 0 } },
};

// MM Player_OverrideBlureColors. MM keeps two sword blures; Ocarina of Time's player has one.
void MergeShip_FierceDeity_OverrideBlureColors(PlayState* play, Player* this, s32 colorType, s32 elemDuration) {
    EffectBlure* blure = Effect_GetByIndex(this->meleeWeaponEffectIndex);
    MergeShipBlureColors* colors = &sMergeShipFDBlureColors[colorType];

    if (blure == NULL) {
        return;
    }
    blure->p1StartColor.r = colors->p1StartColor[0];
    blure->p1StartColor.g = colors->p1StartColor[1];
    blure->p1StartColor.b = colors->p1StartColor[2];
    blure->p1StartColor.a = colors->p1StartColor[3];
    blure->p2StartColor.r = colors->p2StartColor[0];
    blure->p2StartColor.g = colors->p2StartColor[1];
    blure->p2StartColor.b = colors->p2StartColor[2];
    blure->p2StartColor.a = colors->p2StartColor[3];
    blure->p1EndColor.r = colors->p1EndColor[0];
    blure->p1EndColor.g = colors->p1EndColor[1];
    blure->p1EndColor.b = colors->p1EndColor[2];
    blure->p1EndColor.a = colors->p1EndColor[3];
    blure->p2EndColor.r = colors->p2EndColor[0];
    blure->p2EndColor.g = colors->p2EndColor[1];
    blure->p2EndColor.b = colors->p2EndColor[2];
    blure->p2EndColor.a = colors->p2EndColor[3];

    // (MM: a Deku's trail lasts 8; not this form.)
    blure->elemDuration = elemDuration;
}

// ---- sword swing, damage and the attack action ------------------------------------------------------------

// MM func_8082FA5C: swing sounds and the blure colour when the sword becomes active.
void MergeShip_FierceDeity_func_8082FA5C(PlayState* play, Player* this, s32 meleeWeaponState) {
    u16 voiceSfxId;
    u16 itemSfxId;

    if (this->meleeWeaponState == 0) {
        voiceSfxId = MM_NA_SE_VO_LI_SWORD_N;
        itemSfxId = MM_NA_SE_NONE;
        if (this->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H) {
            voiceSfxId = MM_NA_SE_VO_LI_SWORD_L;
        } else {
            itemSfxId = MM_NA_SE_IT_SWORD_SWING_HARD;
            if (this->unk_845 >= 3) {
                voiceSfxId = MM_NA_SE_VO_LI_SWORD_L;
            } else {
                itemSfxId = (this->heldItemAction == PLAYER_IA_MM_SWORD_TWO_HANDED) ? MM_NA_SE_IT_HAMMER_SWING
                                                                                    : MM_NA_SE_IT_SWORD_SWING;
            }
        }

        if (itemSfxId != MM_NA_SE_NONE) {
            MergeShip_PlayMMSfxAndFootstepFlag(this, itemSfxId);
        }

        if (!((this->meleeWeaponAnimation >= PLAYER_MWA_FLIPSLASH_START) &&
              (this->meleeWeaponAnimation <= PLAYER_MWA_JUMPSLASH_FINISH))) {
            MergeShip_PlayMMVoiceSfx(this, voiceSfxId);
        }

        MergeShip_FierceDeity_OverrideBlureColors(play, this, 0, 4);
    }

    this->meleeWeaponState = meleeWeaponState;
}

// MM func_8083FCF0: the sword's active window within an attack animation.
s32 MergeShip_FierceDeity_func_8083FCF0(PlayState* play, Player* this, f32 arg2, f32 arg3, f32 arg4) {
    if (arg4 < this->skelAnime.curFrame) {
        func_80832318(this);
    } else if (arg2 <= this->skelAnime.curFrame) {
        this->stateFlags3 |= PLAYER_STATE3_2000000;
        MergeShip_FierceDeity_func_8082FA5C(play, this, (arg3 <= this->skelAnime.curFrame) ? 1 : -1);
        return true;
    }
    return false;
}

// MM func_80833728
static void MergeShip_FierceDeity_func_80833728(Player* this, s32 index, u32 dmgFlags, s32 damage) {
    this->meleeWeaponQuads[index].info.toucher.dmgFlags = dmgFlags;
    this->meleeWeaponQuads[index].info.toucher.damage = damage;
    this->meleeWeaponQuads[index].info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
}

// MM func_8083375C with MM D_8085D09C, for a transformed (non-human) player. See the damage mapping above.
void MergeShip_FierceDeity_func_8083375C(Player* this, s32 meleeWeaponAnim) {
    s32 strong = (meleeWeaponAnim >= PLAYER_MWA_FLIPSLASH_START) && (meleeWeaponAnim <= PLAYER_MWA_JUMPSLASH_FINISH);
    u32 dmgFlags;
    s32 damage;

    if (Player_GetMeleeWeaponHeld(this) == 0) {
        // MM D_8085D09C[PLAYER_MELEEWEAPON_NONE] = { DMG_GORON_PUNCH, 2, 2, 0, 0 }
        dmgFlags = DMG_HAMMER_SWING;
        damage = 2;
    } else {
        // MM D_8085D09C[PLAYER_MELEEWEAPON_SWORD_TWO_HANDED] = { DMG_SWORD, 4, 8, 4, 8 }
        dmgFlags = strong ? DMG_JUMP_GIANT : DMG_SLASH_GIANT;
        damage = strong ? 8 : 4;
    }

    MergeShip_FierceDeity_func_80833728(this, 0, dmgFlags, damage);
    MergeShip_FierceDeity_func_80833728(this, 1, dmgFlags, damage);
}

void MergeShip_FierceDeity_Action_84(Player* this, PlayState* play);
void MergeShip_FierceDeity_Action_85(Player* this, PlayState* play);
void MergeShip_FierceDeity_Action_29(Player* this, PlayState* play);

// Ocarina of Time's PlayerMeleeWeaponAnimation -> MM's, for MM's camera (MergeShip_Camera_SetMMMeleeWeaponAnimation).
static s32 MergeShip_FierceDeity_ToMMMeleeWeaponAnimation(s32 meleeWeaponAnim) {
    if (meleeWeaponAnim <= PLAYER_MWA_JUMPSLASH_START) {
        return meleeWeaponAnim; // 0..17 are the same in both games
    } else if (meleeWeaponAnim <= PLAYER_MWA_BACKSLASH_LEFT) {
        return meleeWeaponAnim + 1; // MM PLAYER_MWA_FLIPSLASH_FINISH (19) .. BACKSLASH_LEFT (23)
    } else if (meleeWeaponAnim >= PLAYER_MWA_SPIN_ATTACK_1H) {
        return meleeWeaponAnim + 6; // MM PLAYER_MWA_SPIN_ATTACK_1H (30) .. BIG_SPIN_2H (33)
    }
    return -1; // Ocarina of Time's hammer swings have no MM counterpart
}

// MM func_80833864: start a sword attack. Replaces Ocarina of Time's func_80837948 for the Fierce Deity.
void MergeShip_FierceDeity_func_80833864(PlayState* play, Player* this, s32 meleeWeaponAnim) {
    MergeShip_FierceDeity_func_8083375C(this, meleeWeaponAnim);
    Player_SetupAction(play, this, MergeShip_FierceDeity_Action_84, 0);
    this->av2.actionVar2 = 0;

    if ((meleeWeaponAnim < PLAYER_MWA_FLIPSLASH_FINISH) || (meleeWeaponAnim > PLAYER_MWA_JUMPSLASH_FINISH)) {
        func_80832318(this);
    }

    // Accumulate consecutive slashes to do the "third slash" types
    if ((meleeWeaponAnim != this->meleeWeaponAnimation) || (this->unk_845 >= 3)) {
        this->unk_845 = 0;
    }

    this->unk_845++;
    if (this->unk_845 >= 3) {
        meleeWeaponAnim += 2;
    }

    this->meleeWeaponAnimation = meleeWeaponAnim;
    MergeShip_Camera_SetMMMeleeWeaponAnimation(MergeShip_FierceDeity_ToMMMeleeWeaponAnimation(meleeWeaponAnim));
    Player_AnimPlayOnceAdjusted(play, this, sMergeShipFDMeleeAttackAnimInfo[meleeWeaponAnim].unk_0);
    this->unk_844 = this->skelAnime.animLength + 4.0f;

    if ((meleeWeaponAnim < PLAYER_MWA_FLIPSLASH_START) || (meleeWeaponAnim > PLAYER_MWA_JUMPSLASH_START)) {
        Player_StartAnimMovement(play, this, 0x209); // MM ANIM_FLAG_1 | ANIM_FLAG_ENABLE_MOVEMENT | ANIM_FLAG_NOMOVE
    }
    this->yaw = this->actor.shape.rot.y;
}

// MM func_808400CC: recoil.
void MergeShip_FierceDeity_func_808400CC(PlayState* play, Player* this) {
    if (Player_Action_80843188 != this->actionFunc) { // MM Player_Action_18
        s32 var_v1;

        func_80832440(play, this); // MM func_8082DD2C
        Player_SetupAction(play, this, MergeShip_FierceDeity_Action_85, 0);
        if (Player_CheckHostileLockOn(this)) {
            var_v1 = 2;
        } else {
            var_v1 = 0;
        }
        Player_AnimPlayOnceAdjusted(play, this, sMergeShipFDReboundAnims[Player_HoldsTwoHandedWeapon(this) + var_v1]);
    }

    Player_RequestRumble(this, 180, 20, 100, 0);
    this->linearVelocity = -18.0f;
    // MM func_80840094 (Deku Stick breaking, Razor Sword wear) does nothing for the Fierce Deity's sword.
}

// MM Player_Action_85
void MergeShip_FierceDeity_Action_85(Player* this, PlayState* play) {
    LinkAnimation_Update(play, &this->skelAnime);
    Player_DecelerateToZero(this);

    if (this->skelAnime.curFrame >= 6.0f) {
        func_80839F90(this, play); // MM func_80836988
    }
}

// MM func_808401F4: the sword hitting walls and actors, for a non-Goron form.
s32 MergeShip_FierceDeity_func_808401F4(PlayState* play, Player* this) {
    if (this->meleeWeaponState >= 1) {
        s32 temp_v0_3;

        if (this->meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H) {
            if (!(this->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) &&
                !(this->meleeWeaponQuads[1].base.atFlags & AT_BOUNCED)) {
                if (this->skelAnime.curFrame >= 2.0f) {
                    CollisionPoly* poly;
                    s32 bgId;
                    Vec3f spC8;
                    Vec3f pos;
                    Vec3f spB0;
                    Vec3f* var_a1;
                    Vec3f* temp_a0 = &this->meleeWeaponInfo[0].tip;
                    f32 var_fv1;

                    if (this->linearVelocity >= 0.0f) {
                        var_a1 = &this->meleeWeaponInfo[0].base;

                        var_fv1 = Math_Vec3f_DistXYZAndStoreDiff(temp_a0, var_a1, &spB0);
                        if (var_fv1 != 0.0f) {
                            var_fv1 = (var_fv1 + 10.0f) / var_fv1;
                        }

                        spC8.x = temp_a0->x + (spB0.x * var_fv1);
                        spC8.y = temp_a0->y + (spB0.y * var_fv1);
                        spC8.z = temp_a0->z + (spB0.z * var_fv1);
                        if (BgCheck_EntityLineTest2(&play->colCtx, &spC8, temp_a0, &pos, &poly, true, false, false,
                                                    true, &bgId, &this->actor)) {
                            if (!SurfaceType_IsIgnoredByEntities(&play->colCtx, poly, bgId) &&
                                (SurfaceType_GetFloorType(&play->colCtx, poly, bgId) != 6) && // FLOOR_TYPE_6
                                !func_8002F9EC(play, &this->actor, poly, bgId, &pos)) {       // MM func_800B90AC
                                if (this->linearVelocity >= 0.0f) {
                                    // The surface's material is read with Ocarina of Time's collision encoding
                                    // (the walls are Hyrule's): 0xA wood, 0xB soft dirt.
                                    u32 surfaceMaterial = func_80041F10(&play->colCtx, poly, bgId);

                                    if (surfaceMaterial == 0xA) {
                                        CollisionCheck_SpawnShieldParticlesWood(play, &pos, &this->actor.projectedPos);
                                    } else {
                                        pos.x += 8.0f * COLPOLY_GET_NORMAL(poly->normal.x);
                                        pos.y += 8.0f * COLPOLY_GET_NORMAL(poly->normal.y);
                                        //! MM bug kept: z's normal is added to x.
                                        pos.x += 8.0f * COLPOLY_GET_NORMAL(poly->normal.z);
                                        CollisionCheck_SpawnShieldParticles(play, &pos);

                                        if (surfaceMaterial == 0xB) {
                                            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_WALL_HIT_SOFT);
                                        } else {
                                            MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_WALL_HIT_HARD);
                                        }
                                    }

                                    Player_RequestRumble(this, 180, 20, 100, 0);
                                    this->linearVelocity = -14.0f;
                                }
                            }
                        }
                    }
                }
            } else {
                MergeShip_FierceDeity_func_808400CC(play, this);
                func_80832630(play); // MM func_8082DF2C
                return true;
            }
        }

        temp_v0_3 = (this->meleeWeaponQuads[0].base.atFlags & AT_HIT) != 0;
        if (temp_v0_3 || (this->meleeWeaponQuads[1].base.atFlags & AT_HIT)) {
            if (this->meleeWeaponAnimation < PLAYER_MWA_SPIN_ATTACK_1H) {
                Actor* temp_v1 = this->meleeWeaponQuads[temp_v0_3 ? 0 : 1].base.at;

                if ((temp_v1 != NULL) && (temp_v1->id != ACTOR_EN_KANBAN)) {
                    func_80832630(play);
                }
            }

            // (MM func_8083FF30 / func_8083FFEC: Deku Stick and Razor Sword only.)
            if (this->actor.colChkInfo.atHitEffect == 1) {
                this->actor.colChkInfo.damage = 8;
                func_80837C0C(play, this, PLAYER_HIT_RESPONSE_ELECTRIFIED, 0.0f, 0.0f, this->actor.shape.rot.y, 20);
                return true;
            }
        }
    }

    return false;
}

s32 MergeShip_FierceDeity_ActionHandler_7(Player* this, PlayState* play);

// MM Player_Action_84: a sword attack in progress.
void MergeShip_FierceDeity_Action_84(Player* this, PlayState* play) {
    MergeShipFDAttackAnimInfo* attackInfoEntry = &sMergeShipFDMeleeAttackAnimInfo[this->meleeWeaponAnimation];

    if (this->skelAnime.curFrame < (this->skelAnime.endFrame - 6.0f)) {
        this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET; // MM PLAYER_STATE2_20
    }

    if (MergeShip_FierceDeity_func_808401F4(play, this)) {
        return;
    }

    if (this->linearVelocity >= 0.0f) {
        MergeShip_FierceDeity_func_8083FCF0(play, this, 0.0f, attackInfoEntry->unk_C, attackInfoEntry->unk_D);
    }

    //! @bug Lunge Storage (MM): kept.
    if ((this->stateFlags2 & PLAYER_STATE2_SWORD_LUNGE) && LinkAnimation_OnFrame(&this->skelAnime, 0.0f)) {
        this->linearVelocity = 15.0f;
        this->stateFlags2 &= ~PLAYER_STATE2_SWORD_LUNGE;
    }

    if (this->linearVelocity > 12.0f) {
        func_8084269C(play, this); // MM func_8083FBC4 (dust at the feet on Hyrule's ground/sand floors)
    }

    Math_StepToF(&this->linearVelocity, 0.0f, 5.0f);
    func_8083C50C(this); // MM func_8083A548

    if (LinkAnimation_Update(play, &this->skelAnime) ||
        ((this->meleeWeaponAnimation >= PLAYER_MWA_FLIPSLASH_FINISH) &&
         (this->meleeWeaponAnimation <= PLAYER_MWA_JUMPSLASH_FINISH) && (this->skelAnime.curFrame > 2.0f) &&
         Player_CanSpinAttack(this))) {
        sUseHeldItem = this->av2.actionVar2;

        if (!MergeShip_FierceDeity_ActionHandler_7(this, play)) {
            LinkAnimationHeader* anim =
                Player_CheckHostileLockOn(this) ? attackInfoEntry->unk_8 : attackInfoEntry->unk_4;

            func_80832318(this);

            if (anim == NULL) {
                this->skelAnime.movementFlags &= ~0x200; // MM ANIM_FLAG_ENABLE_MOVEMENT
                Player_SetupAction(play, this, Player_Action_Idle, 1); // MM func_8085B384
                Player_AnimChangeOnceMorph(play, this, Player_GetIdleAnim(this));
                this->yaw = this->actor.shape.rot.y;
            } else {
                u8 savedMovementFlags = this->skelAnime.movementFlags;

                if ((anim == FDANIM(link_fighter_Lpower_jump_kiru_end)) &&
                    (this->modelAnimType != PLAYER_ANIMTYPE_3)) {
                    anim = FDANIM(link_fighter_power_jump_kiru_end);
                }

                this->skelAnime.movementFlags = 0;
                Player_SetupAction(play, this, Player_Action_Idle, 1);
                func_8083328C(play, this, anim); // MM Player_Anim_PlayOnceWaterAdjustment
                this->yaw = this->actor.shape.rot.y;
                this->skelAnime.movementFlags = savedMovementFlags;
            }
            this->stateFlags3 |= PLAYER_STATE3_FINISHED_ATTACKING;
        }
    }
    // (MM: the Zora's and Goron's punch buffering; not this form.)
}

// ---- sword beam (MM func_808332A0 with isSwordBeam, Player_ActionHandler_7) -------------------------------

// MM func_808332A0. Replaces Ocarina of Time's func_80837530 for the Fierce Deity. MM spawns its thunder only
// for the sword beam or for the human; the Fierce Deity's spin attacks have no magic wave.
//
// MergeShip2 (soh_fd): the beam is soh_fd's -- Ocarina of Time's En_M_Thunder as the Fierce Deity's flying blade
// (z_en_m_thunder.c, EnMThunder_SwordBeamAttack), aimed at the Z-targeted enemy's height as MM aims it. Its sword
// type is the Biggoron Sword's (Ocarina of Time's two-handed blade), and 0x200 is soh_fd's beam parameter. "FD Magic
// Spin" gives his spin and his spin's charge Ocarina of Time's energy disk.
#define MERGESHIP_FD_THUNDER_SWORD_TYPE 3 // Player_GetMeleeWeaponHeld of the Biggoron Sword

void MergeShip_FierceDeity_func_808332A0(PlayState* play, Player* this, s32 magicCost, s32 isSwordBeam) {
    if (magicCost != 0) {
        this->unk_858 = 0.0f;
    } else {
        this->unk_858 = 0.5f;
    }

    this->stateFlags1 |= PLAYER_STATE1_CHARGING_SPIN_ATTACK;
    if (this->actor.id != ACTOR_PLAYER) {
        return;
    }
    if (isSwordBeam) {
        s16 pitch = 0;
        Actor* thunder;

        if (this->focusActor != NULL) {
            pitch = Math_Vec3f_Pitch(&this->bodyPartsPos[PLAYER_BODYPART_WAIST], &this->focusActor->focus.pos);
        }
        if (gSaveContext.magic == 0) {
            MergeShipTest_Log("FDBEAM skipped: no magic");
            return;
        }

        thunder = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, this->bodyPartsPos[PLAYER_BODYPART_WAIST].x,
                              this->bodyPartsPos[PLAYER_BODYPART_WAIST].y, this->bodyPartsPos[PLAYER_BODYPART_WAIST].z,
                              pitch, 0, 0, MERGESHIP_FD_THUNDER_SWORD_TYPE | 0x200);

        if (thunder != NULL) {
            MergeShip_FierceDeity_ConsumeDeityBeamMagic(play, 1);
            this->unk_D57 = 4;
        }
        MergeShipTest_Log("FDBEAM spawned=%d pitch=%d target=%s mp=%d", thunder != NULL, pitch,
                          (this->focusActor != NULL) ? "locked" : "none", gSaveContext.magic);
    } else if (MergeShip_SohFd_FdMagicSpin()) {
        Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, this->bodyPartsPos[PLAYER_BODYPART_WAIST].x,
                    this->bodyPartsPos[PLAYER_BODYPART_WAIST].y, this->bodyPartsPos[PLAYER_BODYPART_WAIST].z, 0, 0, 0,
                    MERGESHIP_FD_THUNDER_SWORD_TYPE | magicCost);
    }
}

// MM Player_ActionHandler_7, Fierce Deity path: a Z-targeted slash also fires a sword beam.
s32 MergeShip_FierceDeity_ActionHandler_7(Player* this, PlayState* play) {
    if (!func_8083C6B8(play, this)) {  // MM func_8083A6C0
        if (func_8083BB20(this)) {     // MM func_808396B8
            s32 meleeWeaponAnim = func_80837818(this); // MM func_808335F4 (same for a sword)

            MergeShip_FierceDeity_func_80833864(play, this, meleeWeaponAnim);
            if ((meleeWeaponAnim >= PLAYER_MWA_SPIN_ATTACK_1H) || Player_IsZTargeting(this)) {
                this->stateFlags2 |= PLAYER_STATE2_SPIN_ATTACKING; // MM PLAYER_STATE2_20000
                MergeShip_FierceDeity_func_808332A0(play, this, 0, meleeWeaponAnim < PLAYER_MWA_SPIN_ATTACK_1H);
            }
        } else {
            return false;
        }
    }
    return true;
}

// MM func_808334D4: the spin attack charge begins. Replaces Ocarina of Time's func_80837704.
void MergeShip_FierceDeity_func_808334D4(PlayState* play, Player* this) {
    LinkAnimationHeader* anim;

    if ((this->meleeWeaponAnimation >= PLAYER_MWA_RIGHT_SLASH_1H) &&
        (this->meleeWeaponAnimation <= PLAYER_MWA_RIGHT_COMBO_2H)) {
        anim = sMergeShipFDSpinStartLAnims[Player_HoldsTwoHandedWeapon(this)];
    } else {
        anim = sMergeShipFDSpinStartAnims[Player_HoldsTwoHandedWeapon(this)];
    }

    func_80832318(this);
    LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 8.0f, Animation_GetLastFrame(anim), ANIMMODE_ONCE, -9.0f);
    MergeShip_FierceDeity_func_808332A0(play, this, 2 << 8, false);
}

// ---- jump attack (MM func_808395F0, Player_Action_29, func_8083CBC4) --------------------------------------

// MM func_8083CBC4
s32 MergeShip_FierceDeity_func_8083CBC4(Player* this, f32 arg1, s16 arg2, f32 arg3, f32 arg4, f32 arg5, s16 arg6) {
    s16 temp_v0 = this->yaw - arg2;

    if ((this->unk_880 * 1.5f) < fabsf(this->linearVelocity)) {
        arg5 *= 4.0f;
        arg3 *= 4.0f;
    }

    if (ABS(temp_v0) > 0x6000) {
        if (!Math_StepToF(&this->linearVelocity, 0.0f, arg3)) {
            return false;
        }
        this->yaw = arg2;
    } else {
        Math_AsymStepToF(&this->linearVelocity, arg1, arg4, arg5);
        Math_ScaledStepToS(&this->yaw, arg2, arg6);
    }

    return true;
}

// MM Player_Action_29: the jump attack in the air.
void MergeShip_FierceDeity_Action_29(Player* this, PlayState* play) {
    MergeShipFDAttackAnimInfo* attackInfoEntry = &sMergeShipFDMeleeAttackAnimInfo[this->meleeWeaponAnimation];
    f32 speedTarget;
    s16 yawTarget;

    this->stateFlags2 |= PLAYER_STATE2_DISABLE_ROTATION_Z_TARGET;
    this->actor.gravity = -1.2f;

    LinkAnimation_Update(play, &this->skelAnime);

    if (!MergeShip_FierceDeity_func_808401F4(play, this)) {
        MergeShip_FierceDeity_func_8083FCF0(play, this, 6.0f, attackInfoEntry->unk_C, attackInfoEntry->unk_D);
        if (!(this->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            Player_GetMovementSpeedAndYaw(this, &speedTarget, &yawTarget, SPEED_MODE_LINEAR, play);
            MergeShip_FierceDeity_func_8083CBC4(this, speedTarget, this->yaw, 1.0f, 0.05f, 0.1f, 200);
        } else if (func_80843E64(play, this) >= 0) { // MM func_80836F10: the player did not die from the fall
            this->meleeWeaponAnimation += 2;          // MM += 3: MM's Zora jump kick sits between
            MergeShip_FierceDeity_func_80833864(play, this, this->meleeWeaponAnimation);
            this->unk_845 = 3;
            this->meleeWeaponState = 0;
            MergeShip_PlayMMFloorLandSfx(this);
        }
    }
}

// MM func_808395F0: the jump attack starts. Replaces Ocarina of Time's func_8083BA90 for the Fierce Deity.
void MergeShip_FierceDeity_func_808395F0(PlayState* play, Player* this, s32 meleeWeaponAnim, f32 linearVelocity,
                                         f32 yVelocity) {
    MergeShip_FierceDeity_func_80833864(play, this, meleeWeaponAnim);
    Player_SetupAction(play, this, MergeShip_FierceDeity_Action_29, 0);
    this->stateFlags3 |= PLAYER_STATE3_MIDAIR; // MM PLAYER_STATE3_2
    this->linearVelocity = linearVelocity;
    this->yaw = this->actor.shape.rot.y;
    this->actor.velocity.y = yVelocity;
    this->actor.bgCheckFlags &= ~BGCHECKFLAG_GROUND;
    MergeShip_PlayMMFloorJumpSfx(this);
    MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_L);
}

// ---- guard (MM func_80830B88 Fierce Deity branch, func_80830AE8, func_80830B38) ---------------------------

// MM func_80830B88 for the Fierce Deity: holding R raises the sword to guard, without a shield and without
// Z-targeting. Replaces Ocarina of Time's func_80834758 (which needs a shield and Z-targeting).
s32 MergeShip_FierceDeity_func_80830B88(PlayState* play, Player* this) {
    if (CHECK_BTN_ALL(sControlInput->cur.button, BTN_R)) {
        if (!(this->stateFlags1 & (PLAYER_STATE1_SHIELDING | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_CUTSCENE))) {
            if (!(this->stateFlags1 & PLAYER_STATE1_IN_WATER) ||
                ((gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) &&
                 (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))) {
                // MM play->bButtonAmmoPlusOne: the B button is a minigame's ammo button (Hyrule's shooting gallery).
                if ((play->shootingGalleryStatus == 0) && (this->heldItemAction == this->itemAction)) {
                    LinkAnimationHeader* anim = MergeShip_FierceDeity_Anim(this, func_808346C4(play, this));
                    f32 endFrame = Animation_GetLastFrame(anim);

                    LinkAnimation_Change(play, &this->upperSkelAnime, anim, 1.0f, endFrame, endFrame, ANIMMODE_ONCE,
                                         0.0f);
                    MergeShip_PlayMMSfxActor(&this->actor, MM_NA_SE_IT_SHIELD_SWING); // MM func_80830AE8
                    return true;
                }
            }
        }
    }
    return false;
}

// ---- Z-target sparkles (MM func_80844D80) and per-frame state ---------------------------------------------

static Color_RGBA8 sMergeShipFDSparklePrim = { 100, 255, 255, 0 }; // MM D_8085D3F4
static Color_RGBA8 sMergeShipFDSparkleEnv = { 0, 100, 100, 0 };    // MM D_8085D3F8

// MM func_80844D80: sparkles along the sword while the Fierce Deity is Z-targeting.
void MergeShip_FierceDeity_func_80844D80(PlayState* play, Player* this) {
    Vec3f pos;
    Vec3f spA0;
    Vec3f velocity;
    Vec3f accel;
    Vec3f sp7C;
    s32 i;

    Math_Vec3f_Diff(&this->meleeWeaponInfo[0].tip, &this->meleeWeaponInfo[0].base, &sp7C);
    MergeShip_FD_Vec3f_SumScaled(&this->meleeWeaponInfo[0].base, &sp7C, 0.3f, &spA0);

    for (i = 0; i < 2; i++) {
        MergeShip_FD_Vec3f_SumScaled(&this->meleeWeaponInfo[0].base, &sp7C, Rand_ZeroOne(), &pos);
        MergeShip_FD_Vec3f_AddRand(&pos, 15.0f, &pos);
        MergeShip_FD_Vec3f_DistXYZAndStoreNormDiff(&spA0, &pos, 1.7f, &velocity);
        MergeShip_FD_Vec3f_ScaleAndStore(&velocity, 0.01f, &accel);
        EffectSsKiraKira_SpawnDispersed(play, &pos, &velocity, &accel, &sMergeShipFDSparklePrim,
                                        &sMergeShipFDSparkleEnv, Rand_S16Offset(-20, -120), 15);
    }
}

// MM Player_UpdateCommon, the Fierce Deity's part: MM's unk_D57 (the sword-beam timer MM's enemies read)
// counts down, and the sword sparkles while Z-targeting.
void MergeShip_FierceDeity_UpdateCommon(Player* this, PlayState* play) {
    if (this->unk_D57 != 0) {
        this->unk_D57--;
    }
    if (Player_IsZTargeting(this)) {
        MergeShip_FierceDeity_func_80844D80(play, this);
    }
}

// ---- idle (MM Player_ChooseNextIdleAnim, Player_CheckForIdleAnim, sFidgetAnimations) ----------------------

// MM sFidgetAnimations (FidgetType rows; MM's extra FIDGET_SNIFF row is the Mask of Scents', which the
// Fierce Deity cannot wear).
static LinkAnimationHeader* sMergeShipFDFidgetAnimations[][2] = {
    { FDANIM(link_normal_wait_typeA_20f), FDANIM(link_normal_waitF_typeA_20f) }, // FIDGET_LOOK_AROUND
    { FDANIM(link_normal_wait_typeC_20f), FDANIM(link_normal_waitF_typeC_20f) }, // FIDGET_COLD
    { FDANIM(link_normal_wait_typeB_20f), FDANIM(link_normal_waitF_typeB_20f) }, // FIDGET_WARM
    { FDANIM(link_normal_wait_typeB_20f), FDANIM(link_normal_waitF_typeB_20f) }, // FIDGET_HOT
    { FDANIM(link_wait_typeD_20f), FDANIM(link_waitF_typeD_20f) },               // FIDGET_STRETCH_1
    { FDANIM(link_wait_typeD_20f), FDANIM(link_waitF_typeD_20f) },               // FIDGET_STRETCH_2
    { FDANIM(link_wait_typeD_20f), FDANIM(link_waitF_typeD_20f) },               // FIDGET_STRETCH_3
    { FDANIM(link_wait_heat1_20f), FDANIM(link_waitF_heat1_20f) },               // FIDGET_CRIT_HEALTH_START
    { FDANIM(link_wait_heat2_20f), FDANIM(link_waitF_heat2_20f) },               // FIDGET_CRIT_HEALTH_LOOP
    { FDANIM(link_wait_itemD1_20f), FDANIM(link_wait_itemD1_20f) },              // FIDGET_SWORD_SWING
    { FDANIM(link_wait_itemA_20f), FDANIM(link_waitF_itemA_20f) },               // FIDGET_ADJUST_TUNIC
    { FDANIM(link_wait_itemB_20f), FDANIM(link_waitF_itemB_20f) },               // FIDGET_TAP_FEET
    { FDANIM(link_wait_itemC_20f), FDANIM(link_wait_itemC_20f) },                // FIDGET_ADJUST_SHIELD
    { FDANIM(link_wait_itemD2_20f), FDANIM(link_wait_itemD2_20f) },              // FIDGET_SWORD_SWING_TWO_HAND
};

// MM sFidgetAnimSfx* lists, with MM's sound ids.
static AnimSfxEntry sMergeShipFDFidgetSfxSneeze[] = {
    { MM_NA_SE_VO_LI_SNEEZE, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 8) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxSweat[] = {
    { MM_NA_SE_VO_LI_SWEAT, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 18) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxCritHealthStart[] = {
    { MM_NA_SE_VO_LI_BREATH_REST, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 13) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxCritHealthLoop[] = {
    { MM_NA_SE_VO_LI_BREATH_REST, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 10) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxTunic[] = {
    { MM_NA_SE_PL_CALM_HIT, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 44) },
    { MM_NA_SE_PL_CALM_HIT, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 48) },
    { MM_NA_SE_PL_CALM_HIT, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 52) },
    { MM_NA_SE_PL_CALM_HIT, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 56) },
    { MM_NA_SE_PL_CALM_HIT, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 60) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxTapFeet[] = {
    { 0, ANIMSFX_DATA(8, 25) }, { 0, ANIMSFX_DATA(8, 30) }, { 0, ANIMSFX_DATA(8, 44) },
    { 0, ANIMSFX_DATA(8, 48) }, { 0, ANIMSFX_DATA(8, 52) }, { 0, -ANIMSFX_DATA(8, 56) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxShield[] = {
    { MM_NA_SE_IT_SHIELD_SWING, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 16) },
    { MM_NA_SE_IT_SHIELD_SWING, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 20) },
    { MM_NA_SE_IT_SHIELD_SWING, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 70) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxSword[] = {
    { MM_NA_SE_IT_HAMMER_SWING, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 10) },
    { MM_NA_SE_VO_LI_AUTO_JUMP, ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 10) },
    { MM_NA_SE_IT_SWORD_SWING, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 22) },
    { MM_NA_SE_VO_LI_SWORD_N, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 22) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxSwordTwoHand[] = {
    { MM_NA_SE_IT_SWORD_SWING, ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 39) },
    { MM_NA_SE_VO_LI_SWORD_N, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 39) },
};
static AnimSfxEntry sMergeShipFDFidgetSfxStretch[] = {
    { MM_NA_SE_VO_LI_RELAX, -ANIMSFX_DATA(ANIMSFX_TYPE_VOICE, 20) },
};

// MM sFidgetAnimSfxTypes, resolved to the lists (two entries per FidgetType row, as MM).
static AnimSfxEntry* sMergeShipFDFidgetSfxLists[ARRAY_COUNT(sMergeShipFDFidgetAnimations) * 2] = {
    NULL,
    NULL,
    sMergeShipFDFidgetSfxSneeze,
    sMergeShipFDFidgetSfxSneeze,
    sMergeShipFDFidgetSfxSweat,
    sMergeShipFDFidgetSfxSweat,
    sMergeShipFDFidgetSfxSweat,
    sMergeShipFDFidgetSfxSweat,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxStretch,
    sMergeShipFDFidgetSfxCritHealthStart,
    sMergeShipFDFidgetSfxCritHealthStart,
    sMergeShipFDFidgetSfxCritHealthLoop,
    sMergeShipFDFidgetSfxCritHealthLoop,
    sMergeShipFDFidgetSfxSword,
    sMergeShipFDFidgetSfxSword,
    sMergeShipFDFidgetSfxTunic,
    sMergeShipFDFidgetSfxTunic,
    sMergeShipFDFidgetSfxTapFeet,
    sMergeShipFDFidgetSfxTapFeet,
    sMergeShipFDFidgetSfxShield,
    sMergeShipFDFidgetSfxShield,
    sMergeShipFDFidgetSfxSwordTwoHand,
    sMergeShipFDFidgetSfxSwordTwoHand,
};

// MM Player_CheckForIdleAnim with MM's fidget animations.
s32 MergeShip_FierceDeity_CheckForIdleAnim(Player* this) {
    if (Player_GetIdleAnim(this) != this->skelAnime.animation) {
        LinkAnimationHeader** fidgetAnim;
        s32 i;

        for (i = 0, fidgetAnim = &sMergeShipFDFidgetAnimations[0][0];
             i < ARRAY_COUNT(sMergeShipFDFidgetAnimations) * 2; i++, fidgetAnim++) {
            if (this->skelAnime.animation == *fidgetAnim) {
                return i + 1;
            }
        }
        return IDLE_ANIM_NONE;
    }
    return IDLE_ANIM_DEFAULT;
}

// MM Player_Action_Idle's fidget sound lists.
void MergeShip_FierceDeity_ProcessFidgetAnimSfxList(Player* this, s32 fidgetAnimIndex) {
    if ((fidgetAnimIndex >= 0) && (fidgetAnimIndex < ARRAY_COUNT(sMergeShipFDFidgetSfxLists)) &&
        (sMergeShipFDFidgetSfxLists[fidgetAnimIndex] != NULL)) {
        MergeShip_PlayMMAnimSfxList(this, sMergeShipFDFidgetSfxLists[fidgetAnimIndex]);
    }
}

// MM Player_ChooseNextIdleAnim, Fierce Deity path (fidgets as the human does; the other forms never fidget).
void MergeShip_FierceDeity_ChooseNextIdleAnim(PlayState* play, Player* this) {
    LinkAnimationHeader* anim;
    u32 healthIsCritical;
    LinkAnimationHeader** fidgetAnimPtr;
    s32 fidgetType;
    s32 commonType;
    f32 morphFrames;
    s16 endFrame;

    if ((this->focusActor != NULL) ||
        (!(healthIsCritical = HealthMeter_IsCritical()) && (this->idleType = ((this->idleType + 1) & 1)))) {
        this->stateFlags2 &= ~PLAYER_STATE2_IDLE_FIDGET;
        anim = Player_GetIdleAnim(this);
    } else {
        this->stateFlags2 |= PLAYER_STATE2_IDLE_FIDGET;

        if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
            anim = Player_GetIdleAnim(this);
        } else {
            // MM curRoom.environmentType is Ocarina of Time's curRoom.behaviorType2 (the room's fidget type).
            fidgetType = play->roomCtx.curRoom.behaviorType2;

            if (healthIsCritical) {
                if (this->idleType >= PLAYER_IDLE_DEFAULT) {
                    fidgetType = FIDGET_CRIT_HEALTH_START;
                    this->idleType = PLAYER_IDLE_CRIT_HEALTH;
                } else {
                    fidgetType = FIDGET_CRIT_HEALTH_LOOP;
                }
            } else {
                commonType = Rand_ZeroOne() * 5;

                if (commonType < 4) {
                    if ((((commonType + FIDGET_SWORD_SWING) != FIDGET_SWORD_SWING) &&
                         ((commonType + FIDGET_SWORD_SWING) != FIDGET_ADJUST_SHIELD)) ||
                        ((this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) &&
                         (((commonType + FIDGET_SWORD_SWING) == FIDGET_ADJUST_SHIELD) ||
                          (Player_GetMeleeWeaponHeld(this) != 0)))) {
                        if (((commonType + FIDGET_SWORD_SWING) == FIDGET_SWORD_SWING) &&
                            Player_HoldsTwoHandedWeapon(this)) {
                            //! @bug (MM and OoT) unreachable.
                            commonType = FIDGET_SWORD_SWING_TWO_HAND - FIDGET_SWORD_SWING;
                        }
                        fidgetType = FIDGET_SWORD_SWING + commonType;
                    }
                }
            }

            if ((fidgetType < 0) || (fidgetType >= ARRAY_COUNT(sMergeShipFDFidgetAnimations))) {
                fidgetType = FIDGET_LOOK_AROUND;
            }
            fidgetAnimPtr = &sMergeShipFDFidgetAnimations[fidgetType][0];
            if (this->modelAnimType != PLAYER_ANIMTYPE_1) {
                fidgetAnimPtr = &sMergeShipFDFidgetAnimations[fidgetType][1];
            }
            anim = *fidgetAnimPtr;
        }
    }

    endFrame = Animation_GetLastFrame(anim);
    if (this->skelAnime.animation == anim) {
        morphFrames = 0.0f;
    } else {
        morphFrames = -6.0f;
    }

    LinkAnimation_Change(play, &this->skelAnime, anim, (2.0f / 3.0f) * sWaterSpeedFactor, 0.0f, endFrame,
                         ANIMMODE_ONCE, morphFrames);
}

// ---- time travel (MM Player_StartMode_TimeTravel / Player_Action_TimeTravelEnd, FD branches) --------------

// MM Player_StartMode_TimeTravel: the Fierce Deity comes back from the pedestal holding its sword. (MM's
// Player_PutSwordInHand indexes a two-entry table by form and gives form 0 the Razor Sword, which Hyrule
// does not have; the Fierce Deity's own sword is put in hand, and Player_ProcessItemButtons would force it
// the next frame anyway.)
void MergeShip_FierceDeity_PutSwordInHand(PlayState* play, Player* this) {
    s32 itemAction = PLAYER_IA_MM_SWORD_TWO_HANDED;

    Player_DestroyHookshot(this);
    Player_DetachHeldActor(play, this);
    this->heldItemId = ITEM_MM_SWORD_FIERCE_DEITY;
    this->nextModelGroup = Player_ActionToModelGroup(this, itemAction);
    Player_InitItemAction(play, this, itemAction);
    func_80834644(play, this);
}

// MM Player_Action_TimeTravelEnd, the part that differs for the Fierce Deity: the voice at frame 158, then
// MM's func_808484CC (the sword swing sound list).
static AnimSfxEntry sMergeShipFDTimeTravelEndSfx[] = {
    { MM_NA_SE_IT_MASTER_SWORD_SWING, -ANIMSFX_DATA(ANIMSFX_TYPE_GENERAL, 60) }, // MM D_8085D5E0
};
void MergeShip_FierceDeity_TimeTravelEndSfx(Player* this) {
    if (LinkAnimation_OnFrame(&this->skelAnime, 158.0f)) {
        MergeShip_PlayMMVoiceSfx(this, MM_NA_SE_VO_LI_SWORD_N);
    } else {
        MergeShip_PlayMMAnimSfxList(this, sMergeShipFDTimeTravelEndSfx);
    }
}
