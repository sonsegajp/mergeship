/*
 * MergeShip2 -- sprinting (the user, 2026-11-03: "if you hold A out of a roll, you sprint ... make it look more like a
 * sprint"). Young Link and adult Link, in Hyrule and in Termina:
 *   - holding A as a roll ends, with the stick held, starts a sprint; it lasts while A and the stick stay held and Link
 *     keeps running (a few frames of something else -- a bump, a landing -- do not end it);
 *   - while sprinting the run's target speed is MS_SPRINT speed times the normal one (the run animation speeds up with
 *     it, as it does with speed), and a Bunny Hood's 1.5x multiplies on top (any Bunny Hood, the user 2026-11-03);
 *   - a jump keeps the sprint's speed and the sprint goes on after landing, while A and the stick stay held (the user:
 *     "have it remain after jumps"): Hyrule's air control would cut the speed to the run's limit (MsSprint_CarriesJump);
 *   - the pose (applied as each limb is drawn, after the animation is final, during the run animations only): the
 *     upper body leans forward from the hips with the head kept up, the arms and legs swing wider around the run's
 *     average pose, the elbows bend more.
 * Both games play the same run animations (mm.o2r's are byte-identical to oot.o2r's), so the run's average pose is one
 * table for both (kMsSprintMeans, measured from the archives).
 * Pure C, header-only: each engine includes it from its player code.
 */
#ifndef MERGESHIP_SPRINT_H
#define MERGESHIP_SPRINT_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define MS_SPRINT_CVAR "gEnhancements.MergeShip.Sprint"            /* on/off (default on) */
#define MS_SPRINT_SPEED_CVAR "gEnhancements.MergeShip.SprintSpeed" /* the run speed's factor (default 1.5) */
#define MS_SPRINT_DEFAULT_SPEED 1.5f

/* The pose, tunable: gMergeShip.Sprint.<Lean|Head|Arms|Stride|ElbowL|ElbowR|Ramp>. */
typedef struct MsSprintParams {
    float speed;  /* run speed factor */
    float lean;   /* the upper body's forward lean, degrees */
    float head;   /* how much of the lean the head takes back (0..1) */
    float arms;   /* arm swing factor around the run's average */
    float stride; /* leg swing factor around the run's average */
    float elbowL; /* degrees added to the left elbow's bend */
    float elbowR; /* degrees added to the right elbow's bend */
    float ramp;   /* blend per frame into / out of the sprint */
} MsSprintParams;

#define MS_SPRINT_DEFAULT_LEAN 24.0f
#define MS_SPRINT_DEFAULT_HEAD 0.6f
#define MS_SPRINT_DEFAULT_ARMS 1.7f
#define MS_SPRINT_DEFAULT_STRIDE 1.45f
#define MS_SPRINT_DEFAULT_ELBOW_L -50.0f
#define MS_SPRINT_DEFAULT_ELBOW_R -30.0f
#define MS_SPRINT_DEFAULT_RAMP 0.15f

typedef struct MsSprint {
    int8_t active;       /* sprinting */
    int8_t heldFromRoll; /* A held without a break since a roll */
    int8_t launched;     /* left the ground from a sprint: the jump keeps its speed */
    int16_t grace;       /* frames outside the run action, while sprinting */
    int16_t sinceRoll;   /* frames since the last roll ended */
    float blend;         /* 0..1, the pose's and the speed's share */
} MsSprint;

/* Link's limbs (PLAYER_LIMB_*, the joint table's index: [0] is the root's translation). */
enum {
    MS_SPRINT_LIMB_ROOT = 1,
    MS_SPRINT_LIMB_WAIST,
    MS_SPRINT_LIMB_LOWER,
    MS_SPRINT_LIMB_R_THIGH,
    MS_SPRINT_LIMB_R_SHIN,
    MS_SPRINT_LIMB_R_FOOT,
    MS_SPRINT_LIMB_L_THIGH,
    MS_SPRINT_LIMB_L_SHIN,
    MS_SPRINT_LIMB_L_FOOT,
    MS_SPRINT_LIMB_UPPER, /* the torso: a child of the root (gLinkAdultSkel), parent of the head and the arms */
    MS_SPRINT_LIMB_HEAD,
    MS_SPRINT_LIMB_HAT,
    MS_SPRINT_LIMB_COLLAR,
    MS_SPRINT_LIMB_L_SHOULDER,
    MS_SPRINT_LIMB_L_FOREARM,
    MS_SPRINT_LIMB_L_HAND,
    MS_SPRINT_LIMB_R_SHOULDER,
    MS_SPRINT_LIMB_R_FOREARM,
    MS_SPRINT_LIMB_R_HAND,
    MS_SPRINT_LIMB_SHEATH,
    MS_SPRINT_LIMB_TORSO,
    MS_SPRINT_LIMB_MAX
};

/* Each run animation's average joint rotation over its cycle (limbs 1..21; circular means of oot.o2r's data). */
typedef struct MsSprintMeans {
    const char* name;
    int16_t rot[21][3];
} MsSprintMeans;

static const MsSprintMeans kMsSprintMeans[] = {
#include "SprintMeans.inc"
};

/* The run animation `anim` (an asset path, "...gameplay_keep/gPlayerAnim_link_normal_run_free") averages, or NULL. */
static inline const MsSprintMeans* MsSprint_MeansFor(const char* anim) {
    size_t i;
    size_t len;

    if (anim == NULL) {
        return NULL;
    }
    len = strlen(anim);
    for (i = 0; i < sizeof(kMsSprintMeans) / sizeof(kMsSprintMeans[0]); i++) {
        size_t n = strlen(kMsSprintMeans[i].name);
        if ((len >= n) && (strcmp(anim + len - n, kMsSprintMeans[i].name) == 0) &&
            ((len == n) || (anim[len - n - 1] == '/'))) {
            return &kMsSprintMeans[i];
        }
    }
    return NULL;
}

/* One frame of the sprint's state. `rolling`: Link's action is the roll; `running`: the free run (not Z-targeted);
 * `onGround`; `aHeld`, `stickHeld`: the controller. Returns whether Link is sprinting. */
static inline int MsSprint_Step(MsSprint* s, const MsSprintParams* p, int enabled, int rolling, int running,
                                int onGround, int aHeld, int stickHeld) {
    if (!enabled) {
        s->active = 0;
    } else if (s->active) {
        if (!aHeld || !stickHeld) {
            s->active = 0;
        } else if (running || rolling || !onGround) {
            s->grace = 0; /* a jump does not end it */
        } else if (++s->grace > 20) {
            s->active = 0; /* the landing, a bump: a moment; anything longer ends it */
        }
    } else if (s->heldFromRoll && !rolling && running && stickHeld && (s->sinceRoll <= 10)) {
        /* The roll hands over to standing still for a frame or two before the run picks up (OoT func_8083A060). */
        s->active = 1;
        s->grace = 0;
    }
    if (rolling) {
        s->sinceRoll = 0;
        s->heldFromRoll = (int8_t)aHeld;
    } else {
        if (s->sinceRoll < 1000) {
            s->sinceRoll++;
        }
        if (!aHeld) {
            s->heldFromRoll = 0;
        }
    }
    if (onGround) {
        s->launched = 0;
    } else if (s->active || (s->blend > 0.0f)) {
        s->launched = 1;
    }
    if (s->active && (running || !onGround)) {
        s->blend += p->ramp;
        if (s->blend > 1.0f) {
            s->blend = 1.0f;
        }
    } else {
        s->blend -= p->ramp;
        if (s->blend < 0.0f) {
            s->blend = 0.0f;
        }
    }
    return s->active;
}

/* In the air: the jump keeps the sprint's speed (Hyrule's air control would cut it to the run's limit). */
static inline int MsSprint_CarriesJump(const MsSprint* s) {
    return s->active || s->launched;
}

/* The Bunny Hood's share on top of the sprint, for a hood whose own speed the game does not give (1.5x, as MM's). */
static inline float MsSprint_BunnyFactor(const MsSprint* s) {
    return 1.0f + (0.5f * s->blend);
}

/* The run's target speed factor this frame. */
static inline float MsSprint_SpeedFactor(const MsSprint* s, const MsSprintParams* p) {
    return 1.0f + ((p->speed - 1.0f) * s->blend);
}

/* ---- rotation helpers: a joint's rotation is Rz(z) * Ry(y) * Rx(x) (Matrix_TranslateRotateZYX), binary angles ---- */

static inline void MsSprint_EulerToMtx(const int16_t r[3], float m[3][3]) {
    const float k = 3.14159265f / 32768.0f;
    float cx = cosf(r[0] * k), sx = sinf(r[0] * k);
    float cy = cosf(r[1] * k), sy = sinf(r[1] * k);
    float cz = cosf(r[2] * k), sz = sinf(r[2] * k);

    m[0][0] = cz * cy;
    m[0][1] = (cz * sy * sx) - (sz * cx);
    m[0][2] = (cz * sy * cx) + (sz * sx);
    m[1][0] = sz * cy;
    m[1][1] = (sz * sy * sx) + (cz * cx);
    m[1][2] = (sz * sy * cx) - (cz * sx);
    m[2][0] = -sy;
    m[2][1] = cy * sx;
    m[2][2] = cy * cx;
}

static inline int16_t MsSprint_Angle(float radians) {
    return (int16_t)(int32_t)lrintf(radians * (32768.0f / 3.14159265f));
}

static inline void MsSprint_MtxToEuler(const float m[3][3], int16_t r[3]) {
    float sy = -m[2][0];

    if (sy > 1.0f) {
        sy = 1.0f;
    } else if (sy < -1.0f) {
        sy = -1.0f;
    }
    r[1] = MsSprint_Angle(asinf(sy));
    if (fabsf(sy) < 0.9999f) {
        r[0] = MsSprint_Angle(atan2f(m[2][1], m[2][2]));
        r[2] = MsSprint_Angle(atan2f(m[1][0], m[0][0]));
    } else {
        r[0] = MsSprint_Angle(atan2f(-m[1][2], m[1][1]));
        r[2] = 0;
    }
}

static inline void MsSprint_Mul(const float a[3][3], const float b[3][3], float out[3][3]) {
    int i, j;

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            out[i][j] = (a[i][0] * b[0][j]) + (a[i][1] * b[1][j]) + (a[i][2] * b[2][j]);
        }
    }
}

/* Rotation by `deg` about unit axis `a`. */
static inline void MsSprint_AxisAngle(const float a[3], float deg, float m[3][3]) {
    float t = deg * (3.14159265f / 180.0f);
    float c = cosf(t), s = sinf(t), v = 1.0f - c;

    m[0][0] = (a[0] * a[0] * v) + c;
    m[0][1] = (a[0] * a[1] * v) - (a[2] * s);
    m[0][2] = (a[0] * a[2] * v) + (a[1] * s);
    m[1][0] = (a[1] * a[0] * v) + (a[2] * s);
    m[1][1] = (a[1] * a[1] * v) + c;
    m[1][2] = (a[1] * a[2] * v) - (a[0] * s);
    m[2][0] = (a[2] * a[0] * v) - (a[1] * s);
    m[2][1] = (a[2] * a[1] * v) + (a[0] * s);
    m[2][2] = (a[2] * a[2] * v) + c;
}

/* `rot` (a joint's rotation, relative to `parent` -- the parent's orientation in Link's model space) turned by `deg`
 * about Link's model-space side axis (+X; +Z is forward): positive leans forward. */
static inline void MsSprint_TurnAboutSide(const float parent[3][3], int16_t rot[3], float deg) {
    float axis[3] = { parent[0][0], parent[0][1], parent[0][2] }; /* parent^T * (1, 0, 0) */
    float turn[3][3];
    float local[3][3];
    float out[3][3];

    MsSprint_AxisAngle(axis, deg, turn);
    MsSprint_EulerToMtx(rot, local);
    MsSprint_Mul(turn, local, out);
    MsSprint_MtxToEuler(out, rot);
}

static inline void MsSprint_Amplify(int16_t rot[3], const int16_t mean[3], float k) {
    int c;

    for (c = 0; c < 3; c++) {
        int16_t d = (int16_t)(rot[c] - mean[c]);
        rot[c] = (int16_t)(int32_t)(mean[c] + (int32_t)lrintf(d * k));
    }
}

/* As limb `limb` is drawn: its rotation `rot` (from `joints`, the final joint table) made the sprint's. `anim` is the
 * playing animation's asset path. */
static inline void MsSprint_LimbRot(const MsSprint* s, const MsSprintParams* p, const char* anim,
                                    const int16_t* joints, int limb, int16_t rot[3]) {
    const MsSprintMeans* means;
    float b = s->blend;

    if ((b <= 0.0f) || (limb < MS_SPRINT_LIMB_ROOT) || (limb >= MS_SPRINT_LIMB_MAX)) {
        return;
    }
    means = MsSprint_MeansFor(anim);
    if (means == NULL) {
        return; /* a jump, a landing: their own animations, as they are */
    }
    {
        const int16_t* mean = means->rot[limb - 1];

        if ((limb >= MS_SPRINT_LIMB_R_THIGH) && (limb <= MS_SPRINT_LIMB_L_FOOT)) {
            MsSprint_Amplify(rot, mean, 1.0f + ((p->stride - 1.0f) * b));
        } else if ((limb >= MS_SPRINT_LIMB_L_SHOULDER) && (limb <= MS_SPRINT_LIMB_R_HAND)) {
            MsSprint_Amplify(rot, mean, 1.0f + ((p->arms - 1.0f) * b));
        } else if (limb == MS_SPRINT_LIMB_UPPER) {
            MsSprint_Amplify(rot, mean, 1.0f + ((p->arms - 1.0f) * 0.5f * b));
        }
    }
    if (limb == MS_SPRINT_LIMB_L_FOREARM) {
        rot[2] = (int16_t)(rot[2] + (int32_t)lrintf(p->elbowL * b * (32768.0f / 180.0f)));
    } else if (limb == MS_SPRINT_LIMB_R_FOREARM) {
        rot[2] = (int16_t)(rot[2] + (int32_t)lrintf(p->elbowR * b * (32768.0f / 180.0f)));
    } else if ((limb == MS_SPRINT_LIMB_UPPER) || (limb == MS_SPRINT_LIMB_HEAD)) {
        float root[3][3];

        MsSprint_EulerToMtx(&joints[MS_SPRINT_LIMB_ROOT * 3], root);
        if (limb == MS_SPRINT_LIMB_UPPER) {
            MsSprint_TurnAboutSide(root, rot, p->lean * b);
        } else {
            /* The head's parent is the torso as drawn: the joint table's, with this lean. */
            int16_t upper[3] = { joints[MS_SPRINT_LIMB_UPPER * 3], joints[MS_SPRINT_LIMB_UPPER * 3 + 1],
                                 joints[MS_SPRINT_LIMB_UPPER * 3 + 2] };
            float upperM[3][3];
            float parent[3][3];

            MsSprint_LimbRot(s, p, anim, joints, MS_SPRINT_LIMB_UPPER, upper);
            MsSprint_EulerToMtx(upper, upperM);
            MsSprint_Mul(root, upperM, parent);
            MsSprint_TurnAboutSide(parent, rot, -p->lean * p->head * b);
        }
    }
}

#endif /* MERGESHIP_SPRINT_H */
