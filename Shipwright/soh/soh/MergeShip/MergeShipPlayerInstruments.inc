// MergeShip2 -- Majora's Mask's instruments for the Deku, Goron and Zora: MM's Player_Action_63 (the ocarina
// action) and the per-form players it dispatches to, in place of Ocarina of Time's Player_Action_8084E3C4 for
// those three forms. Included at the end of z_player.c.
//
// Ported from 2ship2harkinian/mm/src/overlays/actors/ovl_player_actor/z_player.c (Player_Action_63,
// func_808525C4, func_8085255C, func_80851C40, func_80851D30, func_80851EAC, func_80851EC8, func_80851F18,
// func_808521E0, func_80852290 and the ocarina part of Player_ActionHandler_13's item use). The instruments are
// drawn by each form's draw code (Deku pipes: MergeShipDekuDraw.c, Goron drums: MergeShipGoronLib.c, Zora guitar:
// MergeShipZoraDraw.inc) from the values set here, as MM's z_player_lib.c draws them.
//
// What the song does in Hyrule (warps, Navi, the actor listening) is Ocarina of Time's, as it is for the human
// forms; MM-only outcomes (Song of Time resets, Elegy, the soaring/warp modes, the Milk Bar and the Zora Hall
// guitar tuner En_Zot) have no Hyrule counterpart and are left out.
//
// Names MM and Ocarina of Time spell differently here:
//   MM msgCtx.ocarinaButtonIndex  = Ocarina of Time msgCtx.lastOcaNoteIdx (set by the message draw on the frame a
//                                   note enters the staff, cleared by Message_Update -- the same frame order as MM)
//   MM OCARINA_MODE_ACTIVE / WARP / EVENT / END = OCARINA_MODE_01 / 02 / 03 / 04
//   MM PLAYER_CSACTION_5 = Ocarina of Time csAction 6 (the cue-started wait, Player_UpdateCommon)
//   MM unk_AA5 == PLAYER_UNKAA5_4 = Ocarina of Time unk_6AD == 3 (a cutscene action is pending)
//   MM unk_AA6_rotFlags = unk_6AE_rotFlags, MM sPlayerControlInput = sControlInput
//   MM Message_DisplayOcarinaStaff(play, OCARINA_ACTION_FREE_PLAY) = func_8010BD58

void MergeShipTest_Log(const char* fmt, ...);

#define MS_INSTR_ANIM(name) ((LinkAnimationHeader*)mm_gPlayerAnim_##name)

// The forms that play their own MM instrument. The humans (and the Fierce Deity, who in MM plays the ocarina
// with the human's animations) keep Ocarina of Time's ocarina.
static s32 MergeShip_Instrument_IsInstrumentForm(Player* this) {
    return (this->mergeShipForm == PLAYER_FORM_DEKU) || (this->mergeShipForm == PLAYER_FORM_GORON) ||
           (this->mergeShipForm == PLAYER_FORM_ZORA);
}

// MM D_8085D17C: taking the instrument out (played backwards, putting it away).
static LinkAnimationHeader* MergeShip_Instrument_StartAnim(Player* this) {
    switch (this->mergeShipForm) {
        case PLAYER_FORM_GORON:
            return MS_INSTR_ANIM(pg_gakkistart);
        case PLAYER_FORM_ZORA:
            return MS_INSTR_ANIM(pz_gakkistart);
        default:
            return MS_INSTR_ANIM(pn_gakkistart);
    }
}

// MM D_8085D190: playing.
static LinkAnimationHeader* MergeShip_Instrument_PlayAnim(Player* this) {
    switch (this->mergeShipForm) {
        case PLAYER_FORM_GORON:
            return MS_INSTR_ANIM(pg_gakkiplay);
        case PLAYER_FORM_ZORA:
            return MS_INSTR_ANIM(pz_gakkiplay);
        default:
            return MS_INSTR_ANIM(pn_gakkiplay);
    }
}

// MM Player_Anim_PlayOnceFreeze.
static void MergeShip_Instrument_AnimPlayOnceFreeze(PlayState* play, Player* this, LinkAnimationHeader* anim) {
    LinkAnimation_Change(play, &this->skelAnime, anim, 1.0f, 0.0f, 0.0f, ANIMMODE_ONCE, 0.0f);
}

// MM Player_Anim_PlayOnceAdjustedReverse.
static void MergeShip_Instrument_AnimPlayOnceAdjustedReverse(PlayState* play, Player* this,
                                                             LinkAnimationHeader* anim) {
    LinkAnimation_Change(play, &this->skelAnime, anim, -PLAYER_ANIM_ADJUSTED_SPEED, Animation_GetLastFrame(anim),
                         0.0f, ANIMMODE_ONCE, 0.0f);
}

// MM msgCtx.ocarinaButtonIndex: the note that entered the staff since the player last updated, or
// OCARINA_BTN_INVALID.
static u8 MergeShip_Instrument_ButtonIndex(PlayState* play) {
    u8 buttonIndex = play->msgCtx.lastOcaNoteIdx;

    return (buttonIndex <= OCARINA_BTN_C_UP) ? buttonIndex : OCARINA_BTN_INVALID;
}

// The ocarina part of MM Player_ActionHandler_13's item use (MM z_player.c, `Player_SetAction_PreserveItemAction(
// play, this, Player_Action_63, 0)` onwards): the instrument comes out unless it is already out or coming out.
// Returns false for the forms that play Ocarina of Time's ocarina.
s32 MergeShip_Instrument_Start(PlayState* play, Player* this) {
    LinkAnimationHeader* anim = this->skelAnime.animation;

    if (!MergeShip_Instrument_IsInstrumentForm(this)) {
        return false;
    }

    if ((this->skelAnime.playSpeed < 0.0f) ||
        ((anim != MergeShip_Instrument_StartAnim(this)) && (anim != MergeShip_Instrument_PlayAnim(this)))) {
        Player_AnimPlayOnceAdjusted(play, this, MergeShip_Instrument_StartAnim(this));
    }
    return true;
}

// MM func_80851C40: the instrument plays the song back (the song was recognised, or an event is playing it)
// rather than being played note by note. (MM's Milk Bar Ballad of the Windfish case has no Hyrule counterpart.)
static s32 MergeShip_Instrument_func_80851C40(PlayState* play, Player* this) {
    return (play->msgCtx.msgMode == MSGMODE_SONG_PLAYED) ||
           (play->msgCtx.msgMode == MSGMODE_SETUP_DISPLAY_SONG_PLAYED) ||
           (play->msgCtx.msgMode == MSGMODE_DISPLAY_SONG_PLAYED) ||
           ((play->msgCtx.ocarinaMode != OCARINA_MODE_01) &&
            ((this->csAction == 6) || (play->msgCtx.ocarinaMode == OCARINA_MODE_03) ||
             (play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY_DONE)));
}

// ---- the Deku's pipes ------------------------------------------------------------------------------------

// MM D_801C03A0: the pipes swelling while a song plays back.
static MergeShipScaleKey sMergeShipDekuPipesPlaybackKeys[] = {
    { 0, { 100, 100, 100 } },
    { 2, { 120, 120, 120 } },
    { 6, { 90, 90, 90 } },
    { 7, { 93, 93, 93 } },
};

// MM func_80851D30. MM writes the five pipe scales through &unk_AF0[0].y, which in MM's Player aliases
// arr_AF0[1..5]; Ocarina of Time's Player keeps the two arrays apart, and the Deku's draw reads arr_AF0[1..5].
static void MergeShip_Instrument_func_80851D30(PlayState* play, Player* this) {
    f32* var_s0 = &this->arr_AF0[1];
    Vec3f sp50;

    if (MergeShip_Instrument_func_80851C40(play, this)) {
        s32 i;

        if (this->skelAnime.mode != ANIMMODE_LOOP) {
            Player_AnimPlayLoopAdjusted(play, this, MergeShip_Instrument_PlayAnim(this));
        }
        MergeShip_Goron_func_80124618(sMergeShipDekuPipesPlaybackKeys, this->skelAnime.curFrame, &sp50);

        for (i = 0; i < 5; i++) {
            *var_s0 = sp50.x;
            var_s0++;
        }
    } else if (play->msgCtx.ocarinaMode == OCARINA_MODE_01) {
        u8 buttonIndex = MergeShip_Instrument_ButtonIndex(play);

        if (buttonIndex != OCARINA_BTN_INVALID) {
            var_s0[buttonIndex] = 1.2f;
            Player_AnimPlayOnceAdjusted(play, this, MergeShip_Instrument_PlayAnim(this));
            MergeShipTest_Log("INSTR deku pipe %d", buttonIndex);
        } else {
            s32 i;

            for (i = 0; i < 5; i++) {
                Math_StepToF(var_s0++, 1.0f, 0.04000001f);
            }
        }
    }
}

// ---- the Goron's drums -----------------------------------------------------------------------------------

// MM D_801C0490: the drums bouncing while a song plays back.
static MergeShipScaleKey sMergeShipGoronDrumsPlaybackKeys[] = {
    { 0, { 100, 100, 100 } },  { 1, { 100, 100, 100 } },  { 2, { 90, 100, 105 } },  { 4, { 110, 100, 100 } },
    { 5, { 90, 100, 105 } },   { 6, { 100, 100, 100 } },  { 7, { 90, 100, 105 } },  { 8, { 100, 100, 100 } },
    { 9, { 90, 100, 105 } },   { 10, { 100, 100, 100 } }, { 11, { 90, 100, 105 } }, { 12, { 110, 100, 100 } },
    { 13, { 100, 100, 100 } }, { 14, { 90, 100, 105 } },  { 15, { 90, 100, 105 } }, { 17, { 100, 100, 100 } },
};

// MM struct_8085D714 / D_8085D714: for each note, the arm that strikes (0 left, 1 right) and its strike.
typedef struct {
    s8 unk_0;
    const char* unk_4;
} MergeShipGoronDrumStrike;

static MergeShipGoronDrumStrike D_8085D714[] = {
    { 1, mm_gPlayerAnim_pg_gakkiplayA }, { 1, mm_gPlayerAnim_pg_gakkiplayL }, { 1, mm_gPlayerAnim_pg_gakkiplayD },
    { 0, mm_gPlayerAnim_pg_gakkiplayU }, { 0, mm_gPlayerAnim_pg_gakkiplayR },
};

// MM D_8085BA08 / D_8085BA20: the left arm's and the right arm's limbs.
static u8 D_8085BA08[PLAYER_LIMB_MAX] = {
    false, // PLAYER_LIMB_NONE
    false, // PLAYER_LIMB_ROOT
    false, // PLAYER_LIMB_WAIST
    false, // PLAYER_LIMB_LOWER
    false, // PLAYER_LIMB_R_THIGH
    false, // PLAYER_LIMB_R_SHIN
    false, // PLAYER_LIMB_R_FOOT
    false, // PLAYER_LIMB_L_THIGH
    false, // PLAYER_LIMB_L_SHIN
    false, // PLAYER_LIMB_L_FOOT
    false, // PLAYER_LIMB_UPPER
    false, // PLAYER_LIMB_HEAD
    false, // PLAYER_LIMB_HAT
    false, // PLAYER_LIMB_COLLAR
    true,  // PLAYER_LIMB_L_SHOULDER
    true,  // PLAYER_LIMB_L_FOREARM
    true,  // PLAYER_LIMB_L_HAND
    false, // PLAYER_LIMB_R_SHOULDER
    false, // PLAYER_LIMB_R_FOREARM
    false, // PLAYER_LIMB_R_HAND
    false, // PLAYER_LIMB_SHEATH
    false, // PLAYER_LIMB_TORSO
};
static u8 D_8085BA20[PLAYER_LIMB_MAX] = {
    false, // PLAYER_LIMB_NONE
    false, // PLAYER_LIMB_ROOT
    false, // PLAYER_LIMB_WAIST
    false, // PLAYER_LIMB_LOWER
    false, // PLAYER_LIMB_R_THIGH
    false, // PLAYER_LIMB_R_SHIN
    false, // PLAYER_LIMB_R_FOOT
    false, // PLAYER_LIMB_L_THIGH
    false, // PLAYER_LIMB_L_SHIN
    false, // PLAYER_LIMB_L_FOOT
    false, // PLAYER_LIMB_UPPER
    false, // PLAYER_LIMB_HEAD
    false, // PLAYER_LIMB_HAT
    false, // PLAYER_LIMB_COLLAR
    false, // PLAYER_LIMB_L_SHOULDER
    false, // PLAYER_LIMB_L_FOREARM
    false, // PLAYER_LIMB_L_HAND
    true,  // PLAYER_LIMB_R_SHOULDER
    true,  // PLAYER_LIMB_R_FOREARM
    true,  // PLAYER_LIMB_R_HAND
    false, // PLAYER_LIMB_SHEATH
    false, // PLAYER_LIMB_TORSO
};

// MM func_80851EAC: neither arm is striking.
static void MergeShip_Instrument_func_80851EAC(Player* this) {
    this->unk_B86[0] = -1;
    this->unk_B86[1] = -1;
    this->unk_B10[0] = 0.0f;
}

// MM func_80851EC8: the note's arm strikes its drum (unk_B10[note] is the strike's frame, 3 onwards).
static void MergeShip_Instrument_func_80851EC8(PlayState* play, Player* this, u8 buttonIndex) {
    MergeShipGoronDrumStrike* temp3 = &D_8085D714[buttonIndex];
    f32* temp2 = &this->unk_B10[buttonIndex];
    s16* temp_a3 = &this->unk_B86[temp3->unk_0];

    temp_a3[0] = buttonIndex;
    temp2[0] = 3.0f;
}

// MM func_80851F18: each striking arm takes its strike's frame (MM AnimTaskQueue_AddLoadPlayerFrame /
// AddCopyUsingMap = Ocarina of Time AnimationContext_SetLoadFrame / SetCopyTrue), over the body's pose.
static void MergeShip_Instrument_func_80851F18(PlayState* play, Player* this) {
    MergeShipGoronDrumStrike* temp;
    f32* temp_v0;
    s32 i;

    i = this->unk_B86[0];
    if (i >= 0) {
        temp = &D_8085D714[i];
        i = 0;
        temp_v0 = &this->unk_B10[this->unk_B86[i]];

        AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)temp->unk_4, *temp_v0, this->skelAnime.limbCount,
                                      this->skelAnime.morphTable);
        AnimationContext_SetCopyTrue(play, this->skelAnime.limbCount, this->skelAnime.jointTable,
                                     this->skelAnime.morphTable, D_8085BA08);
    }
    i = this->unk_B86[1];
    if (i >= 0) {
        temp = &D_8085D714[i];
        i = 1;
        temp_v0 = &this->unk_B10[this->unk_B86[i]];

        AnimationContext_SetLoadFrame(play, (LinkAnimationHeader*)temp->unk_4, *temp_v0, this->skelAnime.limbCount,
                                      this->blendTable);
        AnimationContext_SetCopyTrue(play, this->skelAnime.limbCount, this->skelAnime.jointTable, this->blendTable,
                                     D_8085BA20);
    }

    temp_v0 = this->unk_B10;
    for (i = 0; i < 5; i++) {
        *temp_v0 += 1.0f;
        if (*temp_v0 >= 9.0f) {
            *temp_v0 = 8.0f;
            if (this->unk_B86[0] == i) {
                this->unk_B86[0] = -1;
            } else if (this->unk_B86[1] == i) {
                this->unk_B86[1] = -1;
            }
        }
        temp_v0++;
    }
}

// MM func_808521E0: the Goron playing the drums.
static void MergeShip_Instrument_func_808521E0(PlayState* play, Player* this) {
    if (MergeShip_Instrument_func_80851C40(play, this)) {
        if (this->skelAnime.animation != MS_INSTR_ANIM(pg_gakkiplay)) {
            Player_AnimPlayLoopAdjusted(play, this, MS_INSTR_ANIM(pg_gakkiplay));
        }

        MergeShip_Goron_func_80124618(sMergeShipGoronDrumsPlaybackKeys, this->skelAnime.curFrame, &this->unk_AF0[1]);
    } else if (play->msgCtx.ocarinaMode == OCARINA_MODE_01) {
        u8 buttonIndex = MergeShip_Instrument_ButtonIndex(play);

        if (buttonIndex != OCARINA_BTN_INVALID) {
            MergeShip_Instrument_func_80851EC8(play, this, buttonIndex);
            MergeShipTest_Log("INSTR goron drum %d arms L=%d R=%d", buttonIndex, this->unk_B86[0], this->unk_B86[1]);
        }

        MergeShip_Instrument_func_80851F18(play, this);
    }
}

// ---- the Zora's guitar -----------------------------------------------------------------------------------

// MM func_80852290: the Zora playing the guitar. Each note is a strum; the stick leans the upper body (and with
// it the guitar, drawn from unk_B86 in MergeShipZoraDraw.inc) and changes the face.
static void MergeShip_Instrument_func_80852290(PlayState* play, Player* this) {
    if (MergeShip_Instrument_func_80851C40(play, this)) {
        if (this->skelAnime.mode != ANIMMODE_LOOP) {
            Player_AnimPlayLoopAdjusted(play, this, MergeShip_Instrument_PlayAnim(this));
        }

        this->unk_B8A = 8;
    } else {
        f32 sp3C;
        s16 upperLimbRotX;
        s16 sp38;

        if ((play->msgCtx.ocarinaMode == OCARINA_MODE_01) &&
            (MergeShip_Instrument_ButtonIndex(play) != OCARINA_BTN_INVALID)) {
            // (MM's Zora Hall guitar tuner, En_Zot, has no Hyrule counterpart.)
            Player_AnimPlayOnceAdjusted(play, this, MergeShip_Instrument_PlayAnim(this));
            this->unk_B8A = 8;
            MergeShipTest_Log("INSTR zora strum %d", MergeShip_Instrument_ButtonIndex(play));
        }

        sControlInput = play->state.input;
        // MM Lib_GetControlStickData(&sp3C, &sp38, sPlayerControlInput). MM's Math_Atan2S_XY(y, -x) is Ocarina
        // of Time's Math_Atan2S(y, -x).
        {
            f32 x = sControlInput->rel.stick_x;
            f32 y = sControlInput->rel.stick_y;
            f32 magnitude = sqrtf(SQ(x) + SQ(y));

            sp3C = (60.0f < magnitude) ? 60.0f : magnitude;
            if (magnitude > 0.0f) {
                sp38 = Math_Atan2S(sControlInput->cur.stick_y, -(f32)sControlInput->cur.stick_x);
            } else {
                sp38 = 0;
            }
        }

        if ((s16)(sp38 + 0x4000) < 0) {
            sp38 -= 0x8000;
            sp3C = -sp3C;
        }

        if (sp38 < -0x1F40) {
            sp38 = -0x1F40;
        } else if (sp38 > 0x2EE0) {
            sp38 = 0x2EE0;
        }

        upperLimbRotX = (sp3C * -100.0f);
        upperLimbRotX = CLAMP_MAX(upperLimbRotX, 0xFA0);
        Math_SmoothStepToS(&this->upperLimbRot.x, upperLimbRotX, 4, 0x7D0, 0);
        Math_SmoothStepToS(&this->upperLimbRot.y, sp38, 4, 0x7D0, 0);
        this->headLimbRot.x = -this->upperLimbRot.x;
        this->unk_6AE_rotFlags |= UNK6AE_ROT_HEAD_X | UNK6AE_ROT_UPPER_X | UNK6AE_ROT_UPPER_Y;

        // MM's face indices (the forms draw MM's faces): PLAYER_FACE_NEUTRAL 0, OPENING 13, HURT 8.
        upperLimbRotX = ABS(this->upperLimbRot.x);
        if (upperLimbRotX < 0x7D0) {
            this->actor.shape.face = 0;
        } else if (upperLimbRotX < 0xFA0) {
            this->actor.shape.face = 13;
        } else {
            this->actor.shape.face = 8;
        }
    }

    if (DECR(this->unk_B8A) != 0) {
        this->unk_B86[0] += (s16)(s32)(this->upperLimbRot.x * 2.5f);
        this->unk_B86[1] += (s16)(s32)(this->upperLimbRot.y * 3.0f);
    } else {
        this->unk_B86[0] = 0;
        this->unk_B86[1] = 0;
    }
}

// ---- MM Player_Action_63 ---------------------------------------------------------------------------------

// MM func_8085255C.
static void MergeShip_Instrument_func_8085255C(PlayState* play, Player* this) {
    if (this->mergeShipForm == PLAYER_FORM_DEKU) {
        MergeShip_Instrument_func_80851D30(play, this);
    } else if (this->mergeShipForm == PLAYER_FORM_GORON) {
        MergeShip_Instrument_func_808521E0(play, this);
    } else if (this->mergeShipForm == PLAYER_FORM_ZORA) {
        MergeShip_Instrument_func_80852290(play, this);
    }
}

// MM func_808525C4: once the instrument is out, the Deku and the Zora hold their playing pose (frame 0 of
// gakkiplay) and the Goron waits over its drums.
static void MergeShip_Instrument_func_808525C4(PlayState* play, Player* this) {
    if (this->av2.actionVar2++ >= 3) {
        if ((this->mergeShipForm == PLAYER_FORM_ZORA) || (this->mergeShipForm == PLAYER_FORM_DEKU)) {
            MergeShip_Instrument_AnimPlayOnceFreeze(play, this, MergeShip_Instrument_PlayAnim(this));
        } else if (this->mergeShipForm == PLAYER_FORM_GORON) {
            MergeShip_Instrument_func_80851EAC(this);
            Player_AnimPlayLoopAdjusted(play, this, MS_INSTR_ANIM(pg_gakkiwait));
        }

        this->unk_B48 = 1.0f;
    }
}

// MM Player_Action_63 for the Deku, Goron and Zora, with Ocarina of Time's handling of what the song does.
// Returns false for the other forms (Ocarina of Time's Player_Action_8084E3C4 then runs as it is).
s32 MergeShip_Instrument_Action_63(Player* this, PlayState* play) {
    MergeShip_MMAudio_LogFanfareInstruments(); // harness evidence only (MergeShipTest_Log is a no-op otherwise)

    if (!MergeShip_Instrument_IsInstrumentForm(this)) {
        return false;
    }

    if ((this->unk_6AD != 3) &&
        ((LinkAnimation_Update(play, &this->skelAnime) &&
          (this->skelAnime.animation == MergeShip_Instrument_StartAnim(this))) ||
         ((this->skelAnime.mode == ANIMMODE_LOOP) && (this->av2.actionVar2 == 0)))) {
        MergeShip_Instrument_func_808525C4(play, this);
        // MM opens the staff on each of the frames the finished start animation holds; 2S2H's
        // NoDropOcarinaInput opens it once, so the first notes are not dropped. Ocarina of Time's own ocarina
        // action opens it once, and Hyrule's ocarina actors wait for PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR.
        if (this->av2.actionVar2 == 1) {
            if (this->stateFlags2 & (PLAYER_STATE2_NEAR_OCARINA_ACTOR | PLAYER_STATE2_PLAY_FOR_ACTOR)) {
                this->stateFlags2 |= PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR;
            } else {
                func_8010BD58(play, OCARINA_ACTION_FREE_PLAY);
            }
        }
    } else if (this->av2.actionVar2 != 0) {
        if (play->msgCtx.ocarinaMode == OCARINA_MODE_04) {
            // MM OCARINA_MODE_END, with Ocarina of Time's camera and talk handling.
            func_8005B1A4(Play_GetCamera(play, 0));

            if ((this->talkActor != NULL) && (this->talkActor == this->unk_6A8)) {
                Player_StartTalking(play, this->talkActor);
            } else if (this->naviTextId < 0) {
                this->talkActor = this->naviActor;
                this->naviActor->textId = -this->naviTextId;
                Player_StartTalking(play, this->talkActor);
            } else if (!Player_ActionHandler_13(this, play)) {
                // MM func_80836A5C (Ocarina of Time func_8083A060), then D_8085D17C played backwards.
                func_8083A060(this, play);
                MergeShip_Instrument_AnimPlayOnceAdjustedReverse(play, this, MergeShip_Instrument_StartAnim(this));
            }

            this->stateFlags2 &= ~(PLAYER_STATE2_NEAR_OCARINA_ACTOR | PLAYER_STATE2_ATTEMPT_PLAY_FOR_ACTOR |
                                   PLAYER_STATE2_PLAY_FOR_ACTOR);
            this->unk_6A8 = NULL;
            MergeShipTest_Log("INSTR end form=%d", this->mergeShipForm);
        } else if (play->msgCtx.ocarinaMode == OCARINA_MODE_02) {
            // Ocarina of Time's warp songs (MM's warp modes are Termina's).
            gSaveContext.respawn[RESPAWN_MODE_RETURN].entranceIndex = sWarpSongEntrances[play->msgCtx.lastPlayedSong];
            gSaveContext.respawn[RESPAWN_MODE_RETURN].playerParams = 0x5FF;
            gSaveContext.respawn[RESPAWN_MODE_RETURN].data = play->msgCtx.lastPlayedSong;

            this->csAction = 0;
            this->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;

            Player_TryCsAction(play, NULL, 8);
            play->mainCamera.unk_14C &= ~8;

            this->stateFlags1 |= PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;
            this->stateFlags2 |= PLAYER_STATE2_OCARINA_PLAYING;

            if (Actor_Spawn(&play->actorCtx, play, ACTOR_DEMO_KANKYO, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0xF) == NULL) {
                Environment_WarpSongLeave(play);
            }

            gSaveContext.seqId = (u8)NA_BGM_DISABLED;
            gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
        } else if (this->unk_6AD == 3) {
            // MM: while a cutscene action is pending the root's motion is pulled back toward the player.
            f32 temp_fa0 = this->skelAnime.jointTable[0].x;
            f32 temp_fa1 = this->skelAnime.jointTable[0].z;
            f32 var_fv1;

            var_fv1 = sqrtf(SQ(temp_fa0) + SQ(temp_fa1));
            if (var_fv1 != 0.0f) {
                var_fv1 = (var_fv1 - 100.0f) / var_fv1;
                var_fv1 = CLAMP_MIN(var_fv1, 0.0f);
            }

            this->skelAnime.jointTable[0].x = temp_fa0 * var_fv1;
            this->skelAnime.jointTable[0].z = temp_fa1 * var_fv1;
        } else {
            MergeShip_Instrument_func_8085255C(play, this);
        }
    }

    return true;
}

// MM Player_CsAction_29 (take the instrument out), Player_CsAction_30 (play it) and Player_CsAction_31 (put it
// away), for the cutscene actions Ocarina of Time uses for the ocarina (csAction 26's start and update, 27's
// start). Returns false for the other forms and cutscene actions.
s32 MergeShip_Instrument_CsAction(PlayState* play, Player* this, s32 mmCsAction) {
    if ((mmCsAction == 0) || !MergeShip_Instrument_IsInstrumentForm(this)) {
        return false;
    }

    switch (mmCsAction) {
        case 29:
            Player_AnimPlayOnceAdjusted(play, this, MergeShip_Instrument_StartAnim(this));
            func_8084B498(this); // MM: this->itemAction = PLAYER_IA_OCARINA
            Player_SetModels(this, Player_ActionToModelGroup(this, this->itemAction));
            break;

        case 30:
            if (LinkAnimation_Update(play, &this->skelAnime) &&
                (this->skelAnime.animation == MergeShip_Instrument_StartAnim(this))) {
                MergeShip_Instrument_func_808525C4(play, this);
                break;
            }
            if (this->av2.actionVar2 != 0) {
                MergeShip_Instrument_func_8085255C(play, this);
            }
            break;

        case 31:
            MergeShip_Instrument_AnimPlayOnceAdjustedReverse(play, this, MergeShip_Instrument_StartAnim(this));
            break;
    }
    return true;
}

#undef MS_INSTR_ANIM
