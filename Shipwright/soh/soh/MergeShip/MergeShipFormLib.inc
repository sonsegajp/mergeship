// MergeShip2 -- Majora's Mask drawing details for the four forms that Ocarina of Time's shared player drawing
// (z_player_lib.c) does not have. Found by the 1:1 form audit (2026-09-22). Each block names its MM original in
// 2ship2harkinian/mm/src/code/z_player_lib.c. Included by z_player_lib.c just before Player_DrawImpl, so it
// can use that file's hand tables and statics; everything here acts only for an MM form.

#include <string.h>

static s32 MergeShip_FormLib_AnimIs(LinkAnimationHeader* anim, const char* path) {
    return (anim != NULL) && (strcmp((const char*)anim, path) == 0);
}

// Harness evidence (a no-op outside a MergeShipTest script): logs a hand's display list when it changes.
void MergeShipTest_Log(const char* fmt, ...);
static Gfx** MergeShip_FormLib_TraceHand(Player* this, s32 right, Gfx** dLists) {
    static const char* sLast[2];
    const char* dl = (const char*)dLists[sDListsLodOffset];

    if ((dl != NULL) && (sLast[right] != dl)) {
        sLast[right] = dl;
        MergeShipTest_Log("FORMLIB %s hand form=%d appearance=%04X dl=%s", right ? "right" : "left",
                          this->mergeShipForm, (u16)this->skelAnime.jointTable[PLAYER_LIMB_MAX].x,
                          (strncmp(dl, "__OTR__", 7) == 0) ? dl + 7 : dl);
    }
    return dLists;
}

// ---- MM sPlayerFaces ---------------------------------------------------------------------------------------
// Ocarina of Time's sEyeMouthIndexes matches MM's face table except PLAYER_FACE_SMILE (15), whose mouth is
// MM's PLAYER_MOUTH_SMILE. Returns the mouth index MM uses for `face`.
static s32 MergeShip_FormLib_FaceMouth(s32 face, s32 ootMouth) {
    return (face == 15) ? 3 : ootMouth;
}

// ---- MM sPlayerFootPos ---------------------------------------------------------------------------------------
// Where each form's feet are in its foot limbs (the shadow and Actor_SetFeetPos).
static Vec3f sMergeShipMMFootPos[MM_FORM_MAX] = {
    { 200.0f, 300.0f, 0.0f }, // MM_FORM_FIERCE_DEITY
    { 200.0f, 200.0f, 0.0f }, // MM_FORM_GORON
    { 200.0f, 300.0f, 0.0f }, // MM_FORM_ZORA
    { 200.0f, 150.0f, 0.0f }, // MM_FORM_DEKU
    { 200.0f, 200.0f, 0.0f }, // MM_FORM_HUMAN
};

// MM Player_SetFeetPos for a form; NULL for Ocarina of Time's ages.
static Vec3f* MergeShip_FormLib_FootPos(Player* this) {
    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return NULL;
    }
    return &sMergeShipMMFootPos[MergeShip_FormToMM(this->mergeShipForm)];
}

// ---- MM's hands for a form (Player_OverrideLimbDrawGameplayDefault) ---------------------------------------------
// After the running fists, MM takes the hand shape from the animation itself: bits 0xF000 (left) and 0x0F00
// (right) of the appearance word jointTable[PLAYER_LIMB_MAX].x, 1 closed and 2 open
// (GET_LEFT/RIGHT_HAND_INDEX_FROM_JOINT_TABLE, D_801C095C / D_801C0964).
static Gfx** MergeShip_FormLib_LeftHandDListsImpl(Player* this, Gfx** dLists) {
    s32 handIndex;

    if (this->stateFlags3 & PLAYER_STATE3_2000) {
        return dLists; // MM: the gliding Deku's hands follow its petals (rot->z, done in the common override).
    }
    if ((this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) && (this->actor.speedXZ > 2.0f) &&
        !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
        return &gPlayerLeftHandClosedDLs[this->mergeShipForm];
    }

    handIndex = this->skelAnime.jointTable[PLAYER_LIMB_MAX].x & 0xF000;
    if (handIndex != 0) {
        handIndex = (handIndex >> 12) - 1;
        if (handIndex >= 2) {
            handIndex = 0;
        }
        return (handIndex == 0) ? &gPlayerLeftHandClosedDLs[this->mergeShipForm]
                                : &gPlayerLeftHandOpenDLs[this->mergeShipForm];
    }
    return dLists;
}

static Gfx** MergeShip_FormLib_RightHandDListsImpl(Player* this, Gfx** dLists) {
    s32 handIndex;

    if (sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        return dLists; // MM: only the human's shield changes the hand; a form keeps its column.
    }
    if ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (this->actor.speedXZ > 2.0f) &&
        !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
        return &sPlayerRightHandClosedDLs[this->mergeShipForm];
    }

    handIndex = this->skelAnime.jointTable[PLAYER_LIMB_MAX].x & 0x0F00;
    if (handIndex != 0) {
        handIndex = (handIndex >> 8) - 1;
        if (handIndex >= 2) {
            handIndex = 0;
        }
        return (handIndex == 0) ? &sPlayerRightHandClosedDLs[this->mergeShipForm]
                                : &sPlayerRightHandOpenDLs[this->mergeShipForm];
    }
    return dLists;
}

static Gfx** MergeShip_FormLib_LeftHandDLists(Player* this, Gfx** dLists) {
    return MergeShip_FormLib_TraceHand(this, 0, MergeShip_FormLib_LeftHandDListsImpl(this, dLists));
}

static Gfx** MergeShip_FormLib_RightHandDLists(Player* this, Gfx** dLists) {
    return MergeShip_FormLib_TraceHand(this, 1, MergeShip_FormLib_RightHandDListsImpl(this, dLists));
}

// ---- MM's keyframed limb scales for the Goron ------------------------------------------------------------------
// MM D_801C0460 (the torso as the drums come out, pg_gakkistart), D_801C0628 (breathing, pg_wait),
// D_801C07C0 (pg_punchC), D_801C0750 (the left fist, pg_punchA) and D_801C0784 (the right fist, pg_punchB).
static MergeShipScaleKey sMergeShipGoronTorsoDrumsKeys[] = {
    { 0, { 100, 100, 100 } }, { 5, { 100, 100, 100 } }, { 7, { 100, 74, 80 } },
    { 8, { 100, 180, 130 } }, { 10, { 100, 80, 80 } },  { 13, { 100, 100, 100 } },
};
static MergeShipScaleKey sMergeShipGoronTorsoWaitKeys[] = {
    { 0, { 100, 100, 100 } },  { 5, { 100, 100, 100 } },  { 15, { 100, 100, 105 } }, { 25, { 100, 100, 105 } },
    { 34, { 100, 100, 100 } }, { 46, { 100, 100, 100 } }, { 57, { 100, 100, 105 } }, { 67, { 100, 100, 105 } },
    { 76, { 100, 100, 100 } }, { 78, { 100, 100, 100 } },
};
static MergeShipScaleKey sMergeShipGoronTorsoPunchCKeys[] = {
    { 0, { 100, 100, 100 } },  { 10, { 100, 100, 100 } }, { 11, { 100, 120, 120 } },
    { 12, { 100, 120, 120 } }, { 14, { 100, 100, 100 } }, { 19, { 100, 100, 100 } },
};
static MergeShipScaleKey sMergeShipGoronPunchAKeys[] = {
    { 0, { 100, 100, 100 } },  { 5, { 100, 100, 100 } },  { 6, { 200, 200, 200 } },
    { 10, { 200, 200, 200 } }, { 11, { 100, 100, 100 } },
};
static MergeShipScaleKey sMergeShipGoronPunchBKeys[] = {
    { 0, { 100, 100, 100 } },  { 14, { 100, 100, 100 } }, { 15, { 200, 200, 200 } },
    { 16, { 200, 200, 200 } }, { 18, { 100, 100, 100 } },
};

// MM D_801C0958 / func_80125340 / func_8012536C for the Goron's torso: the extra matrix that keeps the torso's
// scale off its children, popped once the torso itself has been drawn.
static s32 sMergeShipGoronTorsoPushed = false;

// MM func_80125CE0.
static void MergeShip_FormLib_func_80125CE0(Player* this, MergeShipScaleKey* keys, Vec3f* pos, Vec3s* rot) {
    Matrix_TranslateRotateZYX(pos, rot);
    pos->x = pos->y = pos->z = 0.0f; // MM func_80125318
    rot->x = rot->y = rot->z = 0;
    MergeShip_Goron_func_80124618(keys, this->skelAnime.curFrame, &this->unk_AF0[0]);
    Matrix_Scale(this->unk_AF0[0].x, this->unk_AF0[0].y, this->unk_AF0[0].z, MTXMODE_APPLY);
}

// MM Player_OverrideLimbDrawGameplayCommon, PLAYER_LIMB_TORSO, for the Goron (the Zora's torso is
// MergeShip_Zora_OverrideLimb's).
static void MergeShip_FormLib_GoronTorso(Player* this, Vec3f* pos, Vec3s* rot) {
    LinkAnimationHeader* anim = this->skelAnime.animation;
    MergeShipScaleKey* keys;

    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }
    if (MergeShip_FormLib_AnimIs(anim, mm_gPlayerAnim_pg_wait)) {
        keys = sMergeShipGoronTorsoWaitKeys;
    } else if (MergeShip_FormLib_AnimIs(anim, mm_gPlayerAnim_pg_punchC)) {
        keys = sMergeShipGoronTorsoPunchCKeys;
    } else if (MergeShip_FormLib_AnimIs(anim, mm_gPlayerAnim_pg_gakkistart)) {
        keys = sMergeShipGoronTorsoDrumsKeys;
    } else {
        return;
    }

    Matrix_TranslateRotateZYX(pos, rot);
    Matrix_Push();
    sMergeShipGoronTorsoPushed = true;
    pos->x = pos->y = pos->z = 0.0f;
    rot->x = rot->y = rot->z = 0;
    MergeShip_Goron_func_80124618(keys, this->skelAnime.curFrame, &this->unk_AF0[0]);
    Matrix_Scale(this->unk_AF0[0].x, this->unk_AF0[0].y, this->unk_AF0[0].z, MTXMODE_APPLY);
    if ((this->unk_AF0[0].y != 1.0f) || (this->unk_AF0[0].z != 1.0f)) {
        MergeShipTest_Log("FORMLIB goron torso frame=%.1f scale=(%.3f,%.3f,%.3f)", this->skelAnime.curFrame,
                          this->unk_AF0[0].x, this->unk_AF0[0].y, this->unk_AF0[0].z);
    }
}

// MM func_8012536C for the Goron's torso matrix.
static void MergeShip_FormLib_PopGoronTorso(void) {
    if (sMergeShipGoronTorsoPushed) {
        Matrix_Pop();
        sMergeShipGoronTorsoPushed = false;
    }
}

// MM Player_OverrideLimbDrawGameplayDefault's fist swells on pg_punchA (left) / pg_punchB (right).
static void MergeShip_FormLib_GoronFists(Player* this, s32 limbIndex, Vec3f* pos, Vec3s* rot) {
    LinkAnimationHeader* anim = this->skelAnime.animation;

    if (this->mergeShipForm != PLAYER_FORM_GORON) {
        return;
    }
    if ((limbIndex == PLAYER_LIMB_L_HAND) && MergeShip_FormLib_AnimIs(anim, mm_gPlayerAnim_pg_punchA)) {
        MergeShip_FormLib_func_80125CE0(this, sMergeShipGoronPunchAKeys, pos, rot);
    } else if ((limbIndex == PLAYER_LIMB_R_HAND) && MergeShip_FormLib_AnimIs(anim, mm_gPlayerAnim_pg_punchB)) {
        MergeShip_FormLib_func_80125CE0(this, sMergeShipGoronPunchBKeys, pos, rot);
    }
}

// ---- MM's first-person arms (Player_OverrideLimbDrawGameplayFirstPerson) ---------------------------------------
// MM sPlayerFirstPersonLeftForearmDLs / LeftHandDLs / RightShoulderDLs / RightHandDLs, form rows.
static const char* sMergeShipMMFirstPersonDLs[4][4] = {
    // left forearm, left hand, right shoulder, right hand
    { mm_gLinkDekuLeftForearmDL, mm_gLinkDekuLeftHandDL, mm_gLinkDekuRightShoulderDL, mm_gLinkDekuRightHandDL },
    { mm_gLinkGoronLeftForearmDL, mm_gLinkGoronLeftHandOpenDL, mm_gLinkGoronRightShoulderDL,
      mm_gLinkGoronRightHandOpenDL },
    { mm_gLinkZoraLeftForearmDL, mm_gLinkZoraLeftHandClosedDL, mm_gLinkZoraRightShoulderDL,
      mm_gLinkZoraRightHandOpenDL },
    { mm_gLinkFierceDeityLeftForearmDL, mm_gLinkFierceDeityLeftHandDL, mm_gLinkFierceDeityRightShoulderDL,
      mm_gLinkFierceDeityRightHandDL },
};

// Returns true when it chose *dList for an MM form (MM draws no right forearm, and a form's hookshot hand is its
// plain right hand).
static s32 MergeShip_FormLib_FirstPersonLimb(Player* this, s32 limbIndex, Gfx** dList) {
    const char** row;

    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return false;
    }
    row = sMergeShipMMFirstPersonDLs[this->mergeShipForm - PLAYER_FORM_DEKU];
    if (limbIndex == PLAYER_LIMB_L_FOREARM) {
        *dList = (Gfx*)row[0];
    } else if (limbIndex == PLAYER_LIMB_L_HAND) {
        *dList = (Gfx*)row[1];
    } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
        *dList = (Gfx*)row[2];
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        *dList = (Gfx*)row[3];
    } else {
        *dList = NULL;
    }
    return true;
}

// ---- MM's held bottle (Player_PostLimbDrawGameplay's func_80128640 bottle branch) ------------------------------
// MM D_801C0CE8: where the bottle sits in each form's hand. MM draws gBottleContentsDL (unless empty) and
// gBottleGlassDL there; Ocarina of Time's bottle models are shaped for Link's own hands.
static Vec3f sMergeShipMMBottlePos[MM_FORM_MAX] = {
    { 0.0f, 0.0f, 0.0f },        // MM_FORM_FIERCE_DEITY
    { 300.0f, 300.0f, -230.0f }, // MM_FORM_GORON
    { 0.0f, 90.0f, -50.0f },     // MM_FORM_ZORA
    { 0.0f, 20.0f, -60.0f },     // MM_FORM_DEKU
    { 0.0f, 0.0f, 0.0f },        // MM_FORM_HUMAN
};

// Returns true when it drew the bottle for an MM form. `color` is the contents' colour, `empty` an empty bottle.
static s32 MergeShip_FormLib_DrawBottle(PlayState* play, Player* this, Color_RGB8* color, s32 empty) {
    Vec3f* pos;

    if (!MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        return false;
    }
    pos = &sMergeShipMMBottlePos[MergeShip_FormToMM(this->mergeShipForm)];
    MergeShipTest_Log("FORMLIB bottle form=%d empty=%d at (%.0f,%.0f,%.0f)", this->mergeShipForm, empty, pos->x,
                      pos->y, pos->z);

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(pos->x, pos->y, pos->z, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (!empty) {
        gDPSetEnvColor(POLY_XLU_DISP++, color->r, color->g, color->b, 0);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gBottleContentsDL);
    }
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gBottleGlassDL);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

// ---- MM's glow on the face while a mask takes hold (Player_PostLimbDrawGameplay, PLAYER_LIMB_HEAD) --------------
// MM draws it while PLAYER_STATE1_2 (the transformation; OoT's field mergeShipTransforming) is set and
// actionVar2 (stepped to 255 by func_80855218) is non-zero: gameplay_keep_DL_054C90 in blue, its textures
// scrolled by gameplay_keep_Matanimheader_054F18.
static Vec3f sMergeShipMMTransformGlowPos[MM_FORM_MAX] = {
    { 0.0f, 0.0f, 0.0f },        // MM_FORM_FIERCE_DEITY
    { -578.3f, -1100.9f, 0.0f }, // MM_FORM_GORON
    { -189.5f, -594.87f, 0.0f }, // MM_FORM_ZORA
    { -570.0f, -812.0f, 0.0f },  // MM_FORM_DEKU
    { -230.0f, -520.0f, 0.0f },  // MM_FORM_HUMAN
};

static void MergeShip_FormLib_DrawTransformGlow(PlayState* play, Player* this) {
    u8 mmForm;
    Vec3f* pos;
    u32 step;

    if (!this->mergeShipTransforming || (this->av2.actionVar2 == 0)) {
        return;
    }
    mmForm = MergeShip_FormToMM(this->mergeShipForm);
    pos = &sMergeShipMMTransformGlowPos[mmForm];
    step = play->gameplayFrames;

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    // MM AnimatedMat_DrawXlu(gameplay_keep_Matanimheader_054F18), read from mm.o2r: one entry, segment 0x0B,
    // type 1 (two-layer scroll), layers { -1, 0, 16x16 } and { 1, -2, 16x16 }, sMatAnimStep = gameplayFrames.
    gSPSegment(POLY_XLU_DISP++, 0x0B,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, -1 * step, -(s32)(0 * step), 16, 16, 1, 1 * step,
                                             -(s32)(-2 * (s32)step), 16, 16, -1, 0, 1, 2));
    Matrix_Translate(pos->x, pos->y, 0.0f, MTXMODE_APPLY);
    if (mmForm == MM_FORM_ZORA) {
        Matrix_Scale(0.7f, 0.7f, 0.7f, MTXMODE_APPLY);
    }
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 255, (u8)this->av2.actionVar2);
    MergeShipTest_Log("FORMLIB glow form=%d alpha=%d", this->mergeShipForm, (u8)this->av2.actionVar2);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)mm_gameplay_keep_DL_054C90);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}
