#pragma once

#include "z64save.h" // LINK_AGE_ADULT, LINK_AGE_CHILD

// MergeShip2 — Majora's Mask player forms in Ocarina of Time.
//
// MM's player is OoT's player generalized from *age* to *form*. Where OoT indexes by
// gSaveContext.linkAge, MM indexes the same tables by form:
//
//     OoT: gPlayerSkelHeaders[gSaveContext.linkAge]   MM: gPlayerSkeletons[form]
//     OoT: &sAgeProperties[gSaveContext.linkAge]      MM: &sPlayerAgeProperties[form]
//
// MM numbers its own forms FIERCE_DEITY, GORON, ZORA, DEKU, HUMAN. Adopting that numbering
// here would renumber LINK_AGE_ADULT and LINK_AGE_CHILD, silently breaking every linkAge and
// LINK_IS_ADULT use in the OoT codebase -- almost all of which would keep compiling.
//
// So this index *extends* OoT's instead of replacing it: adult and child keep 0 and 1, and
// MM's forms take new slots. An age is a form, so existing OoT code needs no change, and MM's
// form-indexed data is remapped exactly once -- where the tables are defined.
typedef enum PlayerForm {
    /* 0 */ PLAYER_FORM_ADULT, // == LINK_AGE_ADULT
    /* 1 */ PLAYER_FORM_CHILD, // == LINK_AGE_CHILD
    /* 2 */ PLAYER_FORM_DEKU,
    /* 3 */ PLAYER_FORM_GORON,
    /* 4 */ PLAYER_FORM_ZORA,
    /* 5 */ PLAYER_FORM_FIERCE_DEITY,
    /* 6 */ PLAYER_FORM_MAX
} PlayerForm;

// Majora's Mask's own form numbering. MM code ported here keeps using it wherever MM indexes a
// table by form, so those tables can be copied without reordering; MergeShip_FormToMM and
// MergeShip_FormFromMM convert at the boundary. Both human ages are MM_FORM_HUMAN.
typedef enum MMPlayerForm {
    /* 0 */ MM_FORM_FIERCE_DEITY,
    /* 1 */ MM_FORM_GORON,
    /* 2 */ MM_FORM_ZORA,
    /* 3 */ MM_FORM_DEKU,
    /* 4 */ MM_FORM_HUMAN,
    /* 5 */ MM_FORM_MAX
} MMPlayerForm;

// The identity between an age and a form is load-bearing: every untouched `[linkAge]` index in
// the OoT codebase is really a form index now. Pin it so it cannot rot.
#ifdef __cplusplus
#define MERGESHIP_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define MERGESHIP_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

MERGESHIP_STATIC_ASSERT((int)PLAYER_FORM_ADULT == 0 && (int)LINK_AGE_ADULT == 0,
                        "PLAYER_FORM_ADULT and LINK_AGE_ADULT must both be 0: OoT code indexes form tables by linkAge");
MERGESHIP_STATIC_ASSERT((int)PLAYER_FORM_CHILD == 1 && (int)LINK_AGE_CHILD == 1,
                        "PLAYER_FORM_CHILD and LINK_AGE_CHILD must both be 1: OoT code indexes form tables by linkAge");

// Every table widened from 2 age rows to PLAYER_FORM_MAX form rows must be guarded with this.
// In the previous attempt five arrays were sized by a grown enum and four failed *silently* --
// NULL display lists faulting inside gSPDisplayList, positional action tables read past their
// end, item-id-indexed HUD arrays drawing raw memory as a texture. Only the one carrying a
// static assert failed at build time. The compiler is not a safety net here unless made one.
#define MERGESHIP_ASSERT_FORM_TABLE(arr)                                     \
    MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(arr) == PLAYER_FORM_MAX,             \
                            #arr " must have one row per PlayerForm")
#define MERGESHIP_ASSERT_MM_FORM_TABLE(arr) \
    MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(arr) == MM_FORM_MAX, #arr " must have one row per MM form")

// Majora's Mask groups the player's voice clips into per-form banks of this size, and each form
// row carries its bank index as SFX_VOICE_BANK_SIZE * n.
#define SFX_VOICE_BANK_SIZE 0x20

// Majora's Mask's transformation masks, as values of Player::currentMask. They sit *past*
// PLAYER_MASK_MAX rather than inside OoT's PlayerMask enum: OoT sizes its NPC mask-reaction table
// and the Deku scrub mask contest by PLAYER_MASK_MAX, and neither knows these masks. Order and
// relative values are MM's (PLAYER_MASK_FIERCE_DEITY..PLAYER_MASK_DEKU), so `mask - FIERCE_DEITY`
// is the MM form, as in MM.
#define PLAYER_MASK_MM_FIERCE_DEITY (PLAYER_MASK_MAX + MM_FORM_FIERCE_DEITY)
#define PLAYER_MASK_MM_GORON (PLAYER_MASK_MAX + MM_FORM_GORON)
#define PLAYER_MASK_MM_ZORA (PLAYER_MASK_MAX + MM_FORM_ZORA)
#define PLAYER_MASK_MM_DEKU (PLAYER_MASK_MAX + MM_FORM_DEKU)
#define PLAYER_MASK_IS_MM_TRANSFORMATION(mask) \
    ((mask) >= PLAYER_MASK_MM_FIERCE_DEITY && (mask) <= PLAYER_MASK_MM_DEKU)

// Item action -> MM form, as MM's `itemAction - PLAYER_IA_MASK_FIERCE_DEITY`.
#define MERGESHIP_IA_IS_TRANSFORMATION_MASK(ia) \
    ((ia) >= PLAYER_IA_MM_MASK_FIERCE_DEITY && (ia) <= PLAYER_IA_MM_MASK_DEKU)
#define MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(item) \
    ((item) >= ITEM_MM_MASK_FIERCE_DEITY && (item) <= ITEM_MM_MASK_DEKU)

// Majora's Mask's other twenty masks, numbered as MM's PlayerMask (PLAYER_MASK_TRUTH 1 .. PLAYER_MASK_GIANT
// 0x14). As values of Player::currentMask they follow the transformation masks, past OoT's PlayerMask for
// the same reason: `currentMask - PLAYER_MASK_MM_REGULAR_BASE` is the MM mask.
typedef enum MMMask {
    /* 0x00 */ MM_MASK_NONE,
    /* 0x01 */ MM_MASK_TRUTH,
    /* 0x02 */ MM_MASK_KAFEIS_MASK,
    /* 0x03 */ MM_MASK_ALL_NIGHT,
    /* 0x04 */ MM_MASK_BUNNY,
    /* 0x05 */ MM_MASK_KEATON,
    /* 0x06 */ MM_MASK_GARO,
    /* 0x07 */ MM_MASK_ROMANI,
    /* 0x08 */ MM_MASK_CIRCUS_LEADER,
    /* 0x09 */ MM_MASK_POSTMAN,
    /* 0x0A */ MM_MASK_COUPLE,
    /* 0x0B */ MM_MASK_GREAT_FAIRY,
    /* 0x0C */ MM_MASK_GIBDO,
    /* 0x0D */ MM_MASK_DON_GERO,
    /* 0x0E */ MM_MASK_KAMARO,
    /* 0x0F */ MM_MASK_CAPTAIN,
    /* 0x10 */ MM_MASK_STONE,
    /* 0x11 */ MM_MASK_BREMEN,
    /* 0x12 */ MM_MASK_BLAST,
    /* 0x13 */ MM_MASK_SCENTS,
    /* 0x14 */ MM_MASK_GIANT,
    /* 0x15 */ MM_MASK_MAX
} MMMask;

#define PLAYER_MASK_MM_REGULAR_BASE PLAYER_MASK_MM_DEKU
#define PLAYER_MASK_MM_REGULAR(mmMask) (PLAYER_MASK_MM_REGULAR_BASE + (mmMask))
#define PLAYER_MASK_IS_MM_REGULAR(mask) \
    ((mask) >= PLAYER_MASK_MM_REGULAR(MM_MASK_TRUTH) && (mask) <= PLAYER_MASK_MM_REGULAR(MM_MASK_GIANT))
// The MM mask of a currentMask value, or MM_MASK_NONE.
#define PLAYER_MASK_TO_MM_MASK(mask) \
    (PLAYER_MASK_IS_MM_REGULAR(mask) ? ((mask) - PLAYER_MASK_MM_REGULAR_BASE) : MM_MASK_NONE)
// True while the player wears this MM mask, e.g. MERGESHIP_WEARS_MM_MASK(player, MM_MASK_STONE): the
// check MM's actors make as `Player_GetMask(play) == PLAYER_MASK_STONE`.
#define MERGESHIP_WEARS_MM_MASK(player, mmMask) ((player)->currentMask == PLAYER_MASK_MM_REGULAR(mmMask))

#define MERGESHIP_IA_IS_MM_REGULAR_MASK(ia) ((ia) >= PLAYER_IA_MM_MASK_TRUTH && (ia) <= PLAYER_IA_MM_MASK_GIANT)
#define MERGESHIP_ITEM_IS_MM_REGULAR_MASK(item) ((item) >= ITEM_MM_MASK_TRUTH && (item) <= ITEM_MM_MASK_GIANT)
// Any of Majora's Mask's 24 masks, and the Adult Mask.
#define MERGESHIP_ITEM_IS_MM_MASK(item) \
    (MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(item) || MERGESHIP_ITEM_IS_MM_REGULAR_MASK(item) || \
     ((item) == ITEM_MM_MASK_ADULT)) // MergeShip2: the Adult Mask too (MergeShipAdultMask.h), a Masks page mask
// The last item id MergeShip2 adds; tables indexed by item id run to here.
#define ITEM_MM_LAST ITEM_MM_MASK_ADULT // MergeShip2: MM Chateau Romani (MergeShipChateau.h) follows the masks; the Adult Mask ends it

// Saved per file. In MM every form shares the human's C buttons (GET_CUR_FORM_BTN_ITEM reads C from
// set 0) and only the B button belongs to the form (BUTTON_ITEM_EQUIP(CUR_FORM, EQUIP_SLOT_B)). The
// B item of the form the player is *in* lives in gSaveContext.equips.buttonItems[0] like any OoT
// equip; the other forms' B items wait here, so OoT's HUD and item code read the right one.
typedef struct MergeShipSaveData {
    u8 mmForm;        // MMPlayerForm the player is in; MM_FORM_HUMAN means the OoT age
    u8 ownedMasks;    // bit (1 << MMPlayerForm) per transformation mask owned
    u8 transformSeen; // MM WEEKEVENTREG_30_10..80: a form's first transformation cannot be skipped
    u8 bButtonItem[MM_FORM_MAX];
    u32 ownedMMMasks;  // bit (1 << MMMask) per other MM mask owned
    u8 equippedMMMask; // MM gSaveContext.save.equippedMask for these masks: worn again when the scene loads
    // MergeShip2: MM's equipment (MergeShipMMEquipment.h). MM keeps only the equipped sword and shield; these hold the
    // owned ones and which variant of each shared slot is in use.
    u8 mmEquipOwned;      // bit (1 << MMEquipId) per MM sword, shield, hookshot and bow owned
    u8 mmEquipSelected;   // bit (1 << MMEquipId): the MM item is the variant its Ocarina of Time slot is switched to
    u8 mmEquipSlotOnlyMM; // bit (1 << MMEquipSlot): the slot's Ocarina of Time item is not owned, only its MM variants
    u8 mmRazorHealth;     // MM saveInfo.playerData.swordHealth: the Razor Sword's hits left (100 when obtained)
    // MergeShip2: MM's songs and quest items (MergeShipSongs.h).
    u16 mmSongs;            // bit (song - MERGESHIP_SONG_FIRST) per MM song learned (MM QUEST_SONG_*)
    u8 mmQuestItems;        // MERGESHIP_QUEST_* : the four remains and the Bomber's Notebook
    u16 owlActivationFlags; // MM saveInfo.playerData.owlActivationFlags: bit per OWL_WARP_* statue activated
    s8 timeSpeedOffset;     // MM save.timeSpeedOffset: 0, or -2 after the Inverted Song of Time
    // MergeShip2: MM inventory.strayFairies, by MMStrayFairyArea (MergeShipStrayFairy.h): [0] is Clock Town's one
    // fairy (MM WEEKEVENTREG_08_80), [1..4] Woodfall, Snowhead, Great Bay and Stone Tower (0..15 each).
    u8 strayFairies[5];
    // MergeShip2: the Adult Mask (MergeShipAdultMask.h): the age whose timeline the world keeps while the mask has
    // changed Link's age -- LINK_AGE_ADULT / LINK_AGE_CHILD, or 0xFF when Link is his real age (no mask age).
    u8 adultMaskTimelineAge;
    // MergeShip2 (BUG-0127): Kakariko's gate guard has paid for a look at MM's Keaton Mask (once; Link keeps it).
    u8 guardSawMMKeaton;
} MergeShipSaveData;

#ifdef __cplusplus
extern "C" {
#endif

extern MergeShipSaveData gMergeShipSave;

// The form the player is in right now. MM forms fall back to the OoT age when mm.o2r is not
// mounted, because their skeletons would not exist.
u8 MergeShip_GetPlayerForm(void);
// Records the form in the save, as MM's `gSaveContext.save.playerForm = form`. It does not
// rebuild the player; the transformation (or MergeShip_DebugSetForm) does that.
void MergeShip_SetPlayerForm(u8 form);

u8 MergeShip_FormToMM(u8 form);
u8 MergeShip_FormFromMM(u8 mmForm);

s32 MergeShip_HasTransformationMask(u8 mmForm);
void MergeShip_GiveTransformationMask(u8 mmForm);
s32 MergeShip_HasMMMask(u8 mmMask);
void MergeShip_GiveMMMask(u8 mmMask);
// Owned, by item id: any of the 24 MM mask items.
s32 MergeShip_HasMMMaskItem(u8 item);

// Swap the B button in gSaveContext.equips for the one belonging to `newForm`.
void MergeShip_SwapFormEquips(u8 oldForm, u8 newForm);

// Test/debug only: become `form` at once, without the transformation cutscene.
void MergeShip_DebugSetForm(u8 form);
// Consumed by the player: returns and clears a pending debug form, or 0xFF.
u8 MergeShip_TakeDebugForm(void);

#ifdef __cplusplus
}
#endif

// Every limb display-list table is built from blocks of (2 LODs x one entry per form). Widening
// them from ages to forms means every block grows, and a table left at the old width is read off
// its end by the shared LOD stride -- silently, with a garbage Gfx pointer. Tie each one to the
// enum so that cannot happen quietly.
#define MERGESHIP_ASSERT_LIMB_TABLE(arr, blocks) \
    MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(arr) == (blocks) * 2 * PLAYER_FORM_MAX, \
                            #arr " must hold (blocks) x 2 x PLAYER_FORM_MAX entries")

// True for the four forms that come from Majora's Mask, i.e. the ones whose assets are read
// from mm.o2r and whose behaviour comes from MM's player code.
#define MERGESHIP_FORM_IS_MM(form) ((form) >= PLAYER_FORM_DEKU && (form) < PLAYER_FORM_MAX)
