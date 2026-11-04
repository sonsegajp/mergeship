// MergeShip2 -- Majora's Mask's human Link as Ocarina of Time's child: the player half (see MergeShipChildLink.h).
//
// Included at the end of z_player.c, so it can use that file's statics. The drawing half is
// MergeShipChildLinkDraw.inc at the end of z_player_lib.c; the sounds are routed by
// MERGESHIP_PLAYER_SOUNDS_MM at z_player.c's sound helpers and in MergeShipMMSfx.c.
//
// Animations: Ocarina of Time's and Majora's Mask's gameplay_keep share 496 player animations, and 493 of
// them are byte-identical (checked against oot.o2r / mm.o2r; only link_bottle_bug_in, link_bottle_fish_in
// and link_normal_take_out differ). What makes MM's human move differently is which animation MM's code
// picks, so the child reads GET_PLAYER_ANIM from MM's table (sMergeShipMMAnimations), plays MM's copies of
// the three that differ (MergeShip_MMChild_Anim), and runs MM's ledge jump below.

#include "soh/MergeShip/MergeShipChildLink.h"
#include "soh/MergeShip/MergeShipSohFd.h"

void MergeShipTest_Log(const char* fmt, ...);

// MM FloorProperty values Ocarina of Time's enum does not name: MM marks the floors its human flips off.
#define MM_FLOOR_PROPERTY_1 1 // front flip (MM D_8085C2A4[4])
#define MM_FLOOR_PROPERTY_2 2 // side flip (MM D_8085C2A4[5])

// MM D_8085C2A4[4] and [5] ({ jump, landing, landing while Z-targeting }). Indexed by actionVar1 - 4.
static LinkAnimationHeader* sMergeShipMMChildFlipAnims[2][3] = {
    { (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newroll_jump_20f,
      (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newroll_jump_end_20f,
      (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newroll_jump_end_20f },
    { (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newside_jump_20f,
      (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newside_jump_end_20f,
      (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_newside_jump_end_20f },
};

// One of MM's three running ledge jumps at random: 4 the front roll-flip, 5 the side flip, 0 the running jump.
static s32 MergeShip_MMFlips_RandomType(void) {
    static const s32 sChoices[3] = { 4, 5, 0 };
    s32 pick = (s32)(Rand_ZeroOne() * 3.0f);

    return sChoices[CLAMP(pick, 0, 2)];
}

// Which MM ledge jump the floor just left asks for: 4 the front roll-flip, 5 the side flip, 0 MM's plain
// running jump. `hopping` is whether the flip bars a jump attack and a ledge grab, as MM's do (PLAYER_STATE2_80000).
//
// MM flips only off floors its scenes mark FLOOR_PROPERTY_1/2 (ported as is below). Ocarina of Time has no
// equivalent floor property: a scan of every collision header in oot.o2r finds 0 polygons with either value
// (mm.o2r has 317 and 228), so in Hyrule MM's flips would never play. With "MM Jump Flips" on (MergeShip.MMFlips,
// off by default), every jump that MM would make as its plain running jump instead picks one of MM's three running
// ledge jumps at random -- the front flip, the side flip or the running jump -- each with its own MM landing.
// "Majora's Mask" flips as MM's own flips do; "Cosmetic" (soh_fd's) only changes the animation, so a flip still
// allows a jump attack and a ledge grab as the running jump it stands in for does.
static s32 MergeShip_MMChild_LedgeFlipType(Player* this, s32* hopping) {
    s32 mode = MergeShip_SohFd_FlipMode(this);

    *hopping = true;
    if (sPrevFloorProperty == MM_FLOOR_PROPERTY_1) {
        return 4;
    }
    if (sPrevFloorProperty == MM_FLOOR_PROPERTY_2) {
        return 5;
    }
    if (mode != MERGESHIP_FLIPS_OFF) {
        *hopping = (mode == MERGESHIP_FLIPS_MM);
        return MergeShip_MMFlips_RandomType();
    }
    return 0;
}

// "MM Jump Flips" set to Cosmetic, for a player on Ocarina of Time's ledge jump (func_8083A4A8): its running jump is
// drawn as one of MM's three at random, with nothing else changed (soh_fd). MM's landing follows the animation
// (MergeShip_MMChild_LandingAnim), and the front flip's roll sound plays as MM's does (MergeShip_MMChild_Action_25).
LinkAnimationHeader* MergeShip_MMFlips_CosmeticAnim(LinkAnimationHeader* runJumpAnim) {
    s32 flip = MergeShip_MMFlips_RandomType();

    return (flip != 0) ? sMergeShipMMChildFlipAnims[flip - 4][0] : runJumpAnim;
}

// MM func_808373F8 for PLAYER_FORM_HUMAN: the jump off a ledge (the Deku's water hops do not apply).
// MM func_80834D50 is Ocarina of Time's func_80838940. MM's human (the child built as MM's) always jumps this way;
// with "MM Jump Flips" set to Majora's Mask, so do Ocarina of Time's child, Adult Link and the Fierce Deity (MM's
// Fierce Deity shares the human's jump). MM's two plain jumps are byte-identical to Ocarina of Time's.
s32 MergeShip_MMChild_func_808373F8(PlayState* play, Player* this, u16 sfxId) {
    LinkAnimationHeader* anim;
    f32 speed;
    s16 yawDiff = this->yaw - this->actor.shape.rot.y;

    if ((IREG(66) / 100.0f) < this->linearVelocity) {
        speed = IREG(67) / 100.0f;
    } else {
        speed = (IREG(68) / 100.0f + (IREG(69) * this->linearVelocity) / 1000.0f);

        if (speed < 5.0f) {
            speed = 5.0f;
        }
    }

    if ((ABS(yawDiff) >= 0x1000) || (this->linearVelocity <= 4.0f)) {
        anim = (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_jump;
    } else {
        s32 hopping;
        s32 flip = MergeShip_MMChild_LedgeFlipType(this, &hopping);

        if (flip != 0) {
            func_80838940(this, sMergeShipMMChildFlipAnims[flip - 4][0], speed, play,
                          (flip == 4) ? NA_SE_VO_LI_SWORD_N : sfxId);
            this->fallStartHeight = this->actor.world.pos.y; // MM func_80834CD0
            this->av2.actionVar2 = -1;
            // MM PLAYER_STATE2_80000: MM's own flips allow no jump attack or ledge grab. A cosmetic flip stands in for
            // the running jump, which allows both, so it keeps them (its landing is recognised by its animation
            // instead, MergeShip_MMChild_LandingAnim).
            if (hopping) {
                this->stateFlags2 |= PLAYER_STATE2_HOPPING;
            }
            this->av1.actionVar1 = flip;
            MergeShipTest_Log("MMCHILD flip=%d floorProp=%d speed=%.2f vy=%.2f", flip, sPrevFloorProperty, speed,
                              this->actor.velocity.y);
            return true;
        }
        anim = (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_run_jump;
    }

    func_80838940(this, anim, speed, play, sfxId);
    this->fallStartHeight = this->actor.world.pos.y; // MM func_80834CD0
    this->av2.actionVar2 = 1;
    MergeShipTest_Log("MMCHILD ledge jump anim=%s speed=%.2f", (const char*)anim + 7, speed);

    return true;
}

// MM Player_Action_25: the front flip's roll sound on its fourth frame (whoever flips).
void MergeShip_MMChild_Action_25(PlayState* play, Player* this) {
    if ((this->skelAnime.animation == sMergeShipMMChildFlipAnims[0][0]) && LinkAnimation_OnFrame(&this->skelAnime, 4.0f)) {
        Player_PlaySfx(this, NA_SE_PL_ROLL);
    }
}

// MM func_80837134's landing animation for the jumps above: a flip lands on MM D_8085C2A4[n].unk_4
// (unk_8 while Z-targeting), MM's running jump on gPlayerAnim_link_normal_run_jump_end. Returns false when
// Ocarina of Time's landing choice applies.
s32 MergeShip_MMChild_LandingAnim(Player* this, LinkAnimationHeader** anim) {
    s32 i;

    if (!MERGESHIP_MM_CHILD(this) && (MergeShip_SohFd_FlipMode(this) == MERGESHIP_FLIPS_OFF)) {
        return false;
    }
    if ((this->av1.actionVar1 >= 4) && (this->av1.actionVar1 <= 5) &&
        ((this->stateFlags2 & PLAYER_STATE2_HOPPING) ||
         (this->skelAnime.animation == sMergeShipMMChildFlipAnims[this->av1.actionVar1 - 4][0]))) {
        *anim = sMergeShipMMChildFlipAnims[this->av1.actionVar1 - 4][Player_CheckHostileLockOn(this) ? 2 : 1];
        MergeShipTest_Log("MMCHILD flip landing anim=%s", (const char*)*anim + 7);
        return true;
    }
    // A cosmetic flip (Ocarina of Time's jump drawn as an MM flip) lands on its own MM landing.
    for (i = 0; i < 2; i++) {
        if (this->skelAnime.animation == sMergeShipMMChildFlipAnims[i][0]) {
            *anim = sMergeShipMMChildFlipAnims[i][Player_CheckHostileLockOn(this) ? 2 : 1];
            return true;
        }
    }
    if (this->skelAnime.animation == (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_run_jump) {
        *anim = (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_run_jump_end;
        return true;
    }
    return false;
}

// The three player animations whose data differ between the games: the child plays MM's.
LinkAnimationHeader* MergeShip_MMChild_Anim(Player* this, LinkAnimationHeader* anim) {
    if (!MERGESHIP_MM_CHILD(this)) {
        return anim;
    }
    if (anim == &gPlayerAnim_link_bottle_bug_in) {
        return (LinkAnimationHeader*)mm_gPlayerAnim_link_bottle_bug_in;
    }
    if (anim == &gPlayerAnim_link_bottle_fish_in) {
        return (LinkAnimationHeader*)mm_gPlayerAnim_link_bottle_fish_in;
    }
    if (anim == &gPlayerAnim_link_normal_take_out) {
        return (LinkAnimationHeader*)mm_gPlayerAnim_link_normal_take_out;
    }
    return anim;
}

// Once a frame: follow the CVar. When the child's model changes under a child player, rebuild its skeleton
// (as the transformation does) and return it to standing.
void MergeShip_MMChildLink_Update(Player* this, PlayState* play) {
    if (MergeShip_MMChildLink_Sync() && (this->mergeShipForm == PLAYER_FORM_CHILD)) {
        MergeShip_ApplyFormChange(this, play, PLAYER_FORM_CHILD);
        Player_SetEquipmentData(play, this);
        func_80853080(this, play);
        MergeShipTest_Log("MMCHILD rebuilt as %s", gMergeShipMMChildLink ? "MM human" : "OoT child");
    }
}
