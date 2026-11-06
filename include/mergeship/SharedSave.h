/*
 * MergeShip2 shared save: the part of a player's progress that belongs to both worlds (M4, HOST_DESIGN.md).
 *
 * One MsSharedSave per Ocarina of Time file slot. The host owns it (MsHostServices.GetSharedSave) and keeps it on
 * disk next to SoH's save (Save/mergeship_shared_<slot+1>.bin). Each engine has a projector between it and its own
 * native save:
 *
 *   capture  engine -> MsSharedSave   when the engine hands over (and SoH also when it saves its file)
 *   apply    MsSharedSave -> engine   when the engine receives the player (and SoH also when it loads the file)
 *
 * Idempotent: values are *set*, never added, so applying the same shared save twice is the same as once. A capture
 * only writes the fields its engine holds: what one game cannot hold (Ocarina of Time's Keaton Mask in Termina, say)
 * stays in the shared save untouched, owned but unusable there, until the player is back in the other world.
 * Counters (life, magic, rupees, ammunition, bottle contents, Termina's comings and goings: the Powder Keg, the
 * trade items, stray fairies) are set both ways. What is only ever gained (masks, songs, equipment, capacities) is
 * merged: an apply adds it, never removes it, and Majora's Mask, which keeps only the *equipped* sword and shield,
 * adds its upgrades on capture instead of setting the lists. Majora's Mask's first cycle (until the first Song of
 * Time) is its own prologue: with the option "Share items during Termina's first cycle" (MS_CVAR_SHARE_FIRST_CYCLE,
 * EngineApi.h; default on) Termina applies and captures from the first arrival, but holds back what would break MM's
 * scripted first cycle (its Ocarina of Time, the Song of Time, the Song of Healing, the transformation masks) until
 * that cycle's own script is done; with the option off it neither captures nor applies before it.
 *
 * Values are canonical (MsItem, MsBottle, MsSong ... below), not either game's item ids. Pure C, plain data: the
 * struct is written to disk as-is (little endian, the size in the header guards against a layout change).
 *
 * Version 2 appends the unified randomizer's ledger (M3, UNIFIED_RANDO_DESIGN.md "Delivery"): every check found in
 * either world while a unified seed is played, with its item (MsItemKey) and what the other engine still has to give
 * (MsLedgerEntry). The ledger is append-only; each engine keeps, in its own native save, how many entries it has
 * already projected, so projecting is idempotent and follows the native save when that is not saved (the gives are
 * redone). A version 1 file is its prefix: the host reads it as version 2 with an empty ledger.
 *
 * Version 3 appends the Termina checks the Anchor team has obtained (teamTerminaChecks): a teammate's find is the
 * team's the moment either engine hears of it, not only once Termina's own file is next saved. A version 2 file is its
 * prefix (no team checks yet).
 *
 * Version 4 appends the C and D-pad buttons the player last left a world with (BUG-0101, MsButtonItem): the other world
 * puts the same items on the same buttons on arrival. A version 3 file is its prefix (no buttons to carry).
 * Version 5 (BUG-0120, BUG-0129): the sword on B is carried too, and Hyrule's magic bean count, which Termina
 * spends when it plants one of Hyrule's beans. A version 4 file is its prefix (none of either).
 */
#ifndef MERGESHIP_SHARED_SAVE_H
#define MERGESHIP_SHARED_SAVE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h> /* memset */

#define MS_SHARED_SAVE_MAGIC 0x5653534Du /* "MSSV" */
#define MS_SHARED_SAVE_VERSION 5u

/* Owned items: bit (1 << MsItem) of MsSharedSave.items. */
typedef enum MsItem {
    /* both games */
    MS_ITEM_OCARINA_FAIRY = 0,  /* OoT Fairy Ocarina (MM has no slot for it: MM gets the Ocarina of Time only) */
    MS_ITEM_OCARINA_OF_TIME,    /* OoT ITEM_OCARINA_OF_TIME, MM ITEM_OCARINA_OF_TIME */
    MS_ITEM_BOW,                /* Hyrule's Fairy Bow (SoH writes it); MM's Hero's Bow is MS_ITEM_MM_BOW. itemsModel
                                 * 2: Termina's bow slot only with itemsCombined (2Ship has no Fairy Bow of its own).
                                 * (itemsModel 1: Termina's bow slot held it; 0: either bow) */
    MS_ITEM_ARROW_FIRE,
    MS_ITEM_ARROW_ICE,
    MS_ITEM_ARROW_LIGHT,
    MS_ITEM_HOOKSHOT,           /* MM's Hookshot only (SoH MMEQUIP_HOOKSHOT, 2Ship ITEM_HOOKSHOT); Hyrule's are
                                 * MS_ITEM_OOT_HOOKSHOT / MS_ITEM_LONGSHOT. (itemsModel 0: either: ignored) */
    MS_ITEM_BOMB_BAG,           /* bombs (level in bombBagLevel) */
    MS_ITEM_BOMBCHU,
    MS_ITEM_DEKU_STICK,
    MS_ITEM_DEKU_NUT,
    MS_ITEM_MAGIC_BEANS,
    MS_ITEM_LENS_OF_TRUTH,
    /* Majora's Mask items SoH also has (MergeShipMMItems / MergeShipMMEquipment) */
    MS_ITEM_POWDER_KEG,
    MS_ITEM_PICTOGRAPH_BOX,
    MS_ITEM_GREAT_FAIRYS_SWORD,
    MS_ITEM_BOMBERS_NOTEBOOK,
    /* Ocarina of Time's own: Termina holds them in its Items (Hyrule) view (2Ship MergeShipOoTItems.h) */
    MS_ITEM_LONGSHOT,
    MS_ITEM_SLINGSHOT,
    MS_ITEM_BOOMERANG,
    MS_ITEM_DINS_FIRE,
    MS_ITEM_FARORES_WIND,
    MS_ITEM_NAYRUS_LOVE,
    MS_ITEM_MEGATON_HAMMER,
    /* itemsModel 1 (MS_SHARED_ITEMS_MODEL_SPLIT): each game's version of an item both games have is its own */
    MS_ITEM_OOT_HOOKSHOT,       /* Hyrule's Hookshot or Longshot (level 1; MS_ITEM_LONGSHOT is level 2): Termina's
                                 * Items (Hyrule) hookshot cell, never MM's Hookshot */
    MS_ITEM_MM_BOW,             /* MM's Hero's Bow (2Ship's bow slot; itemsModel 1: only while Hyrule had no Fairy
                                 * Bow): Hyrule's MM Hero's Bow (SoH MMEQUIP_BOW), never the Fairy Bow */
    /* itemsModel 2 (MS_SHARED_ITEMS_MODEL_SPLIT_ALL): MM's own versions of the rest of the items both games have. The
     * MS_ITEM_ARROW_* / MS_ITEM_LENS_OF_TRUTH / MS_ITEM_BOW bits above are then Hyrule's only (SoH writes them). Neither
     * engine holds the other game's arrows or Lens: those stay here, owned, until Link is back in their world. */
    MS_ITEM_MM_ARROW_FIRE,
    MS_ITEM_MM_ARROW_ICE,
    MS_ITEM_MM_ARROW_LIGHT,
    MS_ITEM_MM_LENS_OF_TRUTH,
    MS_ITEM_MM_OCARINA,         /* MM's Ocarina of Time (2Ship writes it with MS_ITEM_OCARINA_FAIRY): Hyrule's MM item
                                 * menu shows it (2026-09-24), never Hyrule's own ocarina */
    MS_ITEM_COUNT
} MsItem;

/* Swords: bit (1 << MsSword) of MsSharedSave.swords. The progressive chain Kokiri -> Razor -> Gilded is shared;
 * Master, Biggoron's and the Giant's Knife are Ocarina of Time's (UNIFIED_RANDO_DESIGN "Swords"). */
typedef enum MsSword {
    MS_SWORD_KOKIRI = 0, /* Ocarina of Time's Kokiri Sword (Hyrule's own; Termina's Equipment (Hyrule) since 2026-09-24) */
    MS_SWORD_RAZOR,
    MS_SWORD_GILDED,
    MS_SWORD_MASTER,
    MS_SWORD_BIGGORON,
    MS_SWORD_GIANTS_KNIFE,
    MS_SWORD_MM_KOKIRI,  /* Majora's Mask's Kokiri Sword (Termina's sword slot; Hyrule's MM equipment): never Ocarina of
                          * Time's, each is its own and looks its own (the user, 2026-09-24) */
    MS_SWORD_COUNT
} MsSword;

/* Shields: bit (1 << MsShield). Each game's shield is its own (the user's rule: Hyrule's Mirror Shield is not MM's), and
 * only its own game has it unless the seed combines them (MsShared_CombineShields). */
typedef enum MsShield {
    MS_SHIELD_DEKU = 0, /* OoT only */
    MS_SHIELD_HYLIAN,   /* OoT's Hylian Shield (itemsModel 0: or MM's Hero's Shield) */
    MS_SHIELD_MIRROR,   /* OoT's Mirror Shield (itemsModel 0: or MM's: ignored then) */
    MS_SHIELD_HERO,     /* MM's Hero's Shield (itemsModel 1) */
    MS_SHIELD_MM_MIRROR, /* MM's Mirror Shield (itemsModel 1) */
    MS_SHIELD_COUNT
} MsShield;

/* MsSharedSave.itemsModel: how `items` and `shields` name the items both games have. 0 (files written before it): the
 * old shared model, where Hyrule's Hookshot / Mirror Shield and MM's were one bit; an apply ignores those ambiguous
 * bits (MS_ITEM_HOOKSHOT, MS_SHIELD_MIRROR) rather than guess, and the next capture of either engine rewrites them. */
#define MS_SHARED_ITEMS_MODEL_SPLIT 1u
/* 2: the arrows, the Lens of Truth and the bow are split too (MS_ITEM_MM_*): each engine writes only its own game's
 * version. Model 1 files named the arrows / Lens (and, while Hyrule had a Fairy Bow, the bow) with the Hyrule bit for
 * both games; each engine's next capture rewrites its own bits (both engines already hold what model 1 gave them). */
#define MS_SHARED_ITEMS_MODEL_SPLIT_ALL 2u

/* Items both games have, as { Hyrule's bit, Termina's bit }. By default each is its own (the user's rule: grabbing one
 * game's Ice Arrows is not the other game's). A unified seed with "Combine Shared Items" (SoH RSK_MS_COMBINE_SHARED_ITEMS,
 * MsSharedSave.itemsCombined) gives both games' versions for either: an apply reads the pairs through
 * MsShared_CombineVersions. Captures always write each game's own bit. */
#define MS_SHARED_ITEM_VERSION_COUNT 5
/* The i-th pair's Hyrule (version 0) or Termina (version 1) bit. (The hookshots, shields and Kokiri Swords have their
 * own bits: MS_SHARED_HOOKSHOTS, MsShared_CombineShields, MsShared_CombineSwords.) */
static inline uint32_t MsShared_VersionBit(int pair, int version) {
    static const uint8_t kPairs[MS_SHARED_ITEM_VERSION_COUNT][2] = {
        { MS_ITEM_BOW, MS_ITEM_MM_BOW },
        { MS_ITEM_ARROW_FIRE, MS_ITEM_MM_ARROW_FIRE },
        { MS_ITEM_ARROW_ICE, MS_ITEM_MM_ARROW_ICE },
        { MS_ITEM_ARROW_LIGHT, MS_ITEM_MM_ARROW_LIGHT },
        { MS_ITEM_LENS_OF_TRUTH, MS_ITEM_MM_LENS_OF_TRUTH },
    };
    return 1u << kPairs[pair][version ? 1 : 0];
}

/* Items both games have with one `items` bit for both, and their counts / levels (quiverLevel, bombBagLevel,
 * stickLevel, nutLevel, arrows .. magicBeans, bottles): each game's own (the user's rule, 2026-09-24: "all items except
 * masks"). An apply takes them from the other game only with the seed's "Combine Shared Items" (itemsCombined). */
static inline int MergeShipShared_BothGamesBit(int bit) {
    switch (bit) {
        case MS_ITEM_OCARINA_FAIRY:
        case MS_ITEM_OCARINA_OF_TIME:
        case MS_ITEM_BOMB_BAG:
        case MS_ITEM_BOMBCHU:
        case MS_ITEM_DEKU_STICK:
        case MS_ITEM_DEKU_NUT:
        case MS_ITEM_MAGIC_BEANS:
            return 1;
        default:
            return 0;
    }
}

/* Termina's own versions (SoH cannot hold MM's arrows or Lens: its capture keeps these bits as they are). */
#define MS_ITEMS_TERMINA_VERSION_BITS \
    ((1u << MS_ITEM_MM_ARROW_FIRE) | (1u << MS_ITEM_MM_ARROW_ICE) | (1u << MS_ITEM_MM_ARROW_LIGHT) | \
     (1u << MS_ITEM_MM_LENS_OF_TRUTH) | (1u << MS_ITEM_MM_OCARINA))

/* The hookshots: Hyrule's (MS_ITEM_OOT_HOOKSHOT, its Hookshot or Longshot) and MM's (MS_ITEM_HOOKSHOT). Combined, either
 * gives the other; MM's gives Hyrule's Hookshot, never the Longshot. */
#define MS_SHARED_HOOKSHOTS ((1u << MS_ITEM_OOT_HOOKSHOT) | (1u << MS_ITEM_HOOKSHOT))

/* `items` as an apply reads it: with `combined`, owning either game's version of a pair owns both. Without it every
 * pair, the bows included, is each game's own (the user's rule, 2026-09-24: a Fairy Bow never gives Termina MM's
 * Hero's Bow, nor Hyrule's Light Arrows MM's). */
static inline uint32_t MsShared_CombineVersions(uint32_t items, int combined) {
    int i;
    if (!combined) {
        return items;
    }
    for (i = 0; i < MS_SHARED_ITEM_VERSION_COUNT; i++) {
        const uint32_t pair = MsShared_VersionBit(i, 0) | MsShared_VersionBit(i, 1);
        if (items & pair) {
            items |= pair;
        }
    }
    if (items & MS_SHARED_HOOKSHOTS) {
        items |= MS_SHARED_HOOKSHOTS;
    }
    return items;
}

/* `shields` as an apply reads it: with `combined`, Hyrule's Hylian Shield and MM's Hero's Shield are one, and so are the
 * two Mirror Shields (the user, 2026-09-24: "hookshots and shields can be included"). Without it each is its own. */
static inline uint8_t MsShared_CombineShields(uint8_t shields, int combined) {
    static const uint8_t kPairs[2] = {
        (1u << MS_SHIELD_HYLIAN) | (1u << MS_SHIELD_HERO),
        (1u << MS_SHIELD_MIRROR) | (1u << MS_SHIELD_MM_MIRROR),
    };
    int i;
    for (i = 0; combined && (i < 2); i++) {
        if (shields & kPairs[i]) {
            shields |= kPairs[i];
        }
    }
    return shields;
}

/* `swords` as an apply reads it: with `combined`, Hyrule's Kokiri Sword and MM's are one. */
static inline uint8_t MsShared_CombineSwords(uint8_t swords, int combined) {
    const uint8_t pair = (uint8_t)((1u << MS_SWORD_KOKIRI) | (1u << MS_SWORD_MM_KOKIRI));
    return (combined && (swords & pair)) ? (uint8_t)(swords | pair) : swords;
}

/* MsSharedSave.hyruleOwn / terminaOwn bits. */
typedef enum MsOwnBit {
    MS_OWN_BOMB = 0,
    MS_OWN_BOMBCHU,
    MS_OWN_STICK,
    MS_OWN_NUT,
    MS_OWN_BEAN,
    MS_OWN_BOW,
    MS_OWN_OCARINA,
    MS_OWN_COUNT
} MsOwnBit;
#define MS_OWN_VALID_HYRULE 1u
#define MS_OWN_VALID_TERMINA 2u
#define MS_OWN_VALID_AMMO_HYRULE 4u  /* ammo[] written (a file from before it takes this engine's counts once) */
#define MS_OWN_VALID_AMMO_TERMINA 8u /* unused since ammo became one pool (2026-09-30); kept so an old file's bit is
                                       * harmless (ammo[] migrates through MS_OWN_VALID_AMMO_HYRULE alone) */
#define MS_OWN_VALID_CHU_BAG 16u     /* chuBagLevel (task 3) was written by this build's code; unset (a file from
                                       * before it), the raw byte is an older mmAmmo[0] and is not trusted -- read as
                                       * 0 (locked) instead, MsShared_ChuBagLevel */
/* MsSharedSave.ammo: ONE count per counted item, MsOwnBit MS_OWN_BOMB .. MS_OWN_BOW (the bow's: its arrows) -- the same
 * in Hyrule and Termina (the user, 2026-09-30: ammo is shared, not each game's own). Capacity is the better bag/quiver
 * either game has found: gUpgradeCapacities[upgrade][MAX(hyruleBags level, terminaBags level)] (both engines' tables
 * agree on every tier); the Bombchu Bag is its own progression (chuBagLevel, MS_CHU_BAG_CAPACITY). */
#define MS_OWN_AMMO_COUNT 6
/* MsSharedSave.hyruleBags / terminaBags: the quiver (bits 0-1), bomb bag (bits 2-3), Deku Stick (bits 4-5) and Deku Nut
 * (bits 6-7) capacity levels, 0 none .. 3 biggest -- each game's own progression (unchanged: only the ammo count and
 * the capacity derived from these became shared, not which bags either game has raised). */
#define MS_BAGS_QUIVER(bags) ((uint8_t)((bags)&3u))
#define MS_BAGS_BOMB_BAG(bags) ((uint8_t)(((bags) >> 2) & 3u))
#define MS_BAGS_MAKE(quiver, bombBag) ((uint8_t)(((quiver)&3u) | (((bombBag)&3u) << 2)))
#define MS_BAGS_FIELD_QUIVER 0
#define MS_BAGS_FIELD_BOMB_BAG 1
#define MS_BAGS_FIELD_STICKS 2
#define MS_BAGS_FIELD_NUTS 3
#define MS_BAGS_LEVEL(bags, field) ((uint8_t)(((bags) >> ((field)*2)) & 3u))
#define MS_BAGS_WITH(bags, field, level) \
    ((uint8_t)(((bags) & ~(3u << ((field)*2))) | ((((unsigned)(level)) & 3u) << ((field)*2))))

/* MsSharedSave.chuBagLevel (0-3) -> the Bombchu Bag's capacity (task 3). Modeled on the Bomb Bag's tiers (20/30/...)
 * but reuses SoH's already-shipped RO_BOMBCHU_BAG_PROGRESSIVE tiers (SeedContext.cpp Context::GetBombchuCapacity) so
 * the number a Hyrule player already knows does not change under MergeShip. */
static inline uint8_t MsShared_ChuBagCapacity(uint8_t level) {
    static const uint8_t kCapacity[4] = { 0, 20, 30, 50 };
    return kCapacity[(level > 3) ? 3 : level];
}

/* MsSharedSave.hyruleChuBag: the seed's RSK_BOMBCHU_BAG mode (0: no bag). Progressive bags raise chuBagLevel one at a
 * time; SoH's RO_BOMBCHU_BAG_SINGLE bag is the whole bag at once (Context::HandleGetBombchuBag merges it as level 3). */
#define MS_CHU_BAG_PROGRESSIVE 1u
#define MS_CHU_BAG_SINGLE 2u

/* MsSharedSave.chuBagLevel, trusted: 0 (locked) unless MS_OWN_VALID_CHU_BAG says this build actually wrote it (a file
 * from before it holds an old mmAmmo[0] byte there instead, not a level). */
static inline uint8_t MsShared_ChuBagLevel(uint8_t rawChuBagLevel, uint8_t ownValid) {
    return (ownValid & MS_OWN_VALID_CHU_BAG) ? rawChuBagLevel : 0;
}

/* BUG-0101: what a C or D-pad button holds, the same in both games (each engine maps its own items to and from
 * these): MsSharedSave.buttonItems. */
typedef enum MsButtonItem {
    MS_BTN_NONE,
    MS_BTN_STICK,
    MS_BTN_NUT,
    MS_BTN_BOMB,
    MS_BTN_BOW,
    MS_BTN_ARROW_FIRE,
    MS_BTN_ARROW_ICE,
    MS_BTN_ARROW_LIGHT,
    MS_BTN_SLINGSHOT,
    MS_BTN_OCARINA,
    MS_BTN_BOMBCHU,
    MS_BTN_HOOKSHOT,
    MS_BTN_LENS,
    MS_BTN_BEAN,
    MS_BTN_HAMMER,
    MS_BTN_BOOMERANG,
    MS_BTN_DINS_FIRE,
    MS_BTN_FARORES_WIND,
    MS_BTN_NAYRUS_LOVE,
    /* Majora's Mask's 24 masks, in MM's ItemId order (MM ITEM_MASK_DEKU .. ITEM_MASK_GIANT) */
    MS_BTN_MM_MASK_FIRST = 0x40,
    MS_BTN_MM_MASK_LAST = MS_BTN_MM_MASK_FIRST + 23,
} MsButtonItem;
/* MsSharedSave.bSword (BUG-0120): the sword on B, as both worlds name it. Ocarina of Time's swords are Hyrule's
 * (Termina wields them through its Hyrule Equipment view); Majora's Mask's are Termina's (Hyrule wields them through
 * its Termina Equipment view). */
typedef enum MsBSword {
    MS_BSWORD_NONE,
    MS_BSWORD_OOT_KOKIRI,
    MS_BSWORD_OOT_MASTER,
    MS_BSWORD_OOT_BIGGORON, /* Biggoron's Sword or the Giant's Knife, whichever Hyrule holds */
    MS_BSWORD_MM_KOKIRI,
    MS_BSWORD_MM_RAZOR,
    MS_BSWORD_MM_GILDED,
    MS_BSWORD_MM_GREAT_FAIRY,
    MS_BSWORD_COUNT
} MsBSword;

/* MsSharedSave.buttonItems' order: C-left, C-down, C-right, D-pad up, down, left, right. */
#define MS_SHARED_BUTTONS 7

/* Songs: bit (1 << MsSong) of MsSharedSave.songs. */
typedef enum MsSong {
    /* both games */
    MS_SONG_TIME = 0,
    MS_SONG_EPONA,
    MS_SONG_STORMS,
    /* Majora's Mask's (SoH MergeShipSongs) */
    MS_SONG_SONATA,
    MS_SONG_GORON_LULLABY,
    MS_SONG_GORON_LULLABY_INTRO,
    MS_SONG_NEW_WAVE,
    MS_SONG_ELEGY,
    MS_SONG_OATH,
    MS_SONG_HEALING,
    MS_SONG_SOARING,
    MS_SONG_INVERTED_TIME, /* SoH keeps a bit; MM has none (it knows them with the Song of Time) */
    MS_SONG_DOUBLE_TIME,
    /* Ocarina of Time's (Termina: its Hyrule quest status, 2Ship MergeShipHyrule.h; Saria's and Sun's are MM's too) */
    MS_SONG_MINUET,
    MS_SONG_BOLERO,
    MS_SONG_SERENADE,
    MS_SONG_REQUIEM,
    MS_SONG_NOCTURNE,
    MS_SONG_PRELUDE,
    MS_SONG_ZELDAS_LULLABY,
    MS_SONG_SARIA,
    MS_SONG_SUN,
    MS_SONG_COUNT
} MsSong;

/* Bottle contents, MsSharedSave.bottles[]. */
typedef enum MsBottle {
    MS_BOTTLE_NONE = 0, /* no bottle in this slot */
    MS_BOTTLE_EMPTY,
    MS_BOTTLE_RED_POTION,
    MS_BOTTLE_GREEN_POTION,
    MS_BOTTLE_BLUE_POTION,
    MS_BOTTLE_FAIRY,
    MS_BOTTLE_FISH,
    MS_BOTTLE_MILK,
    MS_BOTTLE_MILK_HALF,
    MS_BOTTLE_BUG,
    MS_BOTTLE_BLUE_FIRE,
    MS_BOTTLE_POE,
    MS_BOTTLE_BIG_POE,
    MS_BOTTLE_CHATEAU_ROMANI,
    MS_BOTTLE_DEKU_PRINCESS,
    MS_BOTTLE_ZORA_EGG,
    MS_BOTTLE_GOLD_DUST,
    MS_BOTTLE_MUSHROOM,
    MS_BOTTLE_SEAHORSE,
    MS_BOTTLE_HYLIAN_LOACH,
    MS_BOTTLE_SPRING_WATER,
    MS_BOTTLE_HOT_SPRING_WATER,
    MS_BOTTLE_RUTOS_LETTER, /* OoT only */
    MS_BOTTLE_MYSTERY_MILK, /* MM only (ITEM_OBABA_DRINK) */
    MS_BOTTLE_COUNT
} MsBottle;

/* Ocarina of Time's masks and trade items, bit (1 << MsHyruleItem) of MsSharedSave.hyruleItems. The order is Ocarina
 * of Time's ItemId order (ITEM_WEIRD_EGG 0x21 .. ITEM_MASK_TRUTH 0x2B, ITEM_POCKET_EGG 0x2D .. ITEM_CLAIM_CHECK 0x37)
 * without ITEM_SOLD_OUT. Masks are only gained (merged); trade items come and go (set both ways). Ocarina of Time's
 * Keaton, Bunny, Goron, Zora and Truth masks are not Majora's Mask's (UNIFIED_RANDO_DESIGN "Masks"): Termina has no
 * item for them (nor for the Broken Goron's Sword), so they stay here, owned but unusable in Termina. */
typedef enum MsHyruleItem {
    /* Hyrule's child trade slot */
    MS_HYRULE_WEIRD_EGG = 0,
    MS_HYRULE_CHICKEN,
    MS_HYRULE_ZELDAS_LETTER,
    MS_HYRULE_MASK_KEATON,
    MS_HYRULE_MASK_SKULL,
    MS_HYRULE_MASK_SPOOKY,
    MS_HYRULE_MASK_BUNNY,
    MS_HYRULE_MASK_GORON,
    MS_HYRULE_MASK_ZORA,
    MS_HYRULE_MASK_GERUDO,
    MS_HYRULE_MASK_TRUTH,
    /* Hyrule's adult trade slot */
    MS_HYRULE_POCKET_EGG,
    MS_HYRULE_POCKET_CUCCO,
    MS_HYRULE_COJIRO,
    MS_HYRULE_ODD_MUSHROOM,
    MS_HYRULE_ODD_POTION,
    MS_HYRULE_POACHERS_SAW,
    MS_HYRULE_BROKEN_SWORD,
    MS_HYRULE_PRESCRIPTION,
    MS_HYRULE_EYEBALL_FROG,
    MS_HYRULE_EYEDROPS,
    MS_HYRULE_CLAIM_CHECK,
    MS_HYRULE_ITEM_COUNT
} MsHyruleItem;

#define MS_HYRULE_CHILD_TRADE_BITS 0x00000007u /* Weird Egg, Chicken, Zelda's Letter */
#define MS_HYRULE_MASK_BITS 0x000007F8u        /* Keaton .. Truth */
#define MS_HYRULE_ADULT_TRADE_BITS 0x003FF800u /* Pocket Egg .. Claim Check */

#define MS_SHARED_BOTTLE_SLOTS 6 /* MM has six; OoT shows the first four (UNIFIED_RANDO_DESIGN "Bottles") */
#define MS_SHARED_MAX_HEALTH_CAPACITY 0x140 /* 20 hearts, 0x10 each, in both games */

/* ---- the unified randomizer's ledger (version 2) -------------------------------------------------------------- */

/* The world a check is in. The same numbers as MsEngineId (EngineApi.h): Hyrule is SoH's, Termina 2Ship's. */
typedef enum MsWorld {
    MS_WORLD_NONE = 0,
    MS_WORLD_HYRULE = 1,
    MS_WORLD_TERMINA = 2,
} MsWorld;

/* MsLedgerEntry.flags */
#define MS_LEDGER_GIVEN_AT_SOURCE 0x01u /* the engine of the check's world gave the item natively */
#define MS_LEDGER_FOR_HYRULE 0x02u      /* SoH gives the item when it projects the ledger (found in Termina) */
#define MS_LEDGER_FOR_TERMINA 0x04u     /* 2Ship gives the item when it projects the ledger (found in Hyrule) */
#define MS_LEDGER_REMOTE 0x08u          /* an Anchor teammate's find received in `world` (check 0): not this file's */

/* One check found. Trackers read `world` + `check` to mark it; a projector reads `item` + `flags`. */
typedef struct MsLedgerEntry {
    uint16_t item;  /* MsItemKey (MsItems.def row): the item the seed placed there */
    uint16_t check; /* the check in its world's numbering: Hyrule = SoH RandomizerCheck, Termina = 2Ship RandoCheckId
                     * (SoH's RC_MM_* are the same checks in the same order: RC_MM_ANCIENT_CASTLE_OF_IKANA_BOSS +
                     * (check - 1), tools/port_mm_rando.py); 0 = no check (a harness give) */
    uint8_t world;  /* MsWorld the check is in */
    uint8_t flags;  /* MS_LEDGER_* */
    uint16_t seq;   /* 1-based position, for logs */
} MsLedgerEntry;

/* Enough for every check of both worlds (SoH's ~3.5k with every shuffle, Termina's 2641). */
#define MS_SHARED_LEDGER_MAX 8192
/* MsSharedSave.teamTerminaChecks: one bit per 2Ship RandoCheckId (Termina's 2641 checks). */
#define MS_TEAM_TERMINA_CHECK_BYTES 512

typedef struct MsSharedSave {
    uint32_t magic;      /* MS_SHARED_SAVE_MAGIC */
    uint32_t version;    /* MS_SHARED_SAVE_VERSION */
    uint32_t size;       /* sizeof(MsSharedSave) */
    uint32_t generation; /* bumped by every capture */
    int32_t lastWriter;  /* MsEngineId of the last capture; 0 = nothing captured yet (the rest is meaningless) */
    int32_t saveSlot;    /* the Ocarina of Time file slot, -1 = the unsaved debug file */

    /* life, magic, money */
    uint16_t healthCapacity; /* 0x10 per heart */
    int16_t health;
    uint8_t heartPieces;     /* 0-3 */
    uint8_t doubleDefense;   /* 0 / 1 */
    uint8_t magicLevel;      /* 0 none, 1 normal meter, 2 double */
    uint8_t magicAmount;     /* current amount */
    uint16_t rupees;
    uint8_t walletLevel;     /* 0 99, 1 200, 2 500, 3 999 (SoH's Tycoon's Wallet; MM holds at most 2) */
    uint8_t itemsModel;      /* MS_SHARED_ITEMS_MODEL_*: written by every capture (was padding: old files read 0) */

    /* Majora's Mask's 24 masks */
    uint8_t transformationMasks; /* bit (1 << MM form): 0 Fierce Deity, 1 Goron, 2 Zora, 3 Deku */
    uint8_t chateauMagic;        /* BUG-0153 (was pad1): MS_CHATEAU_VALID | MS_CHATEAU_ACTIVE -- Chateau Romani's
                                  * endless magic (MM WEEKEVENTREG_DRANK_CHATEAU_ROMANI, SoH MergeShipChateau.c), one
                                  * status for both games. Older files read 0: each game keeps its own. */
    uint16_t mmTradeOwned;       /* MM's trade items owned, bit (item - Moon's Tear): Moon's Tear, the Land / Swamp /
                                  * Mountain / Ocean Title Deeds, Room Key, Letter to Mama, Letter to Kafei, Pendant of
                                  * Memories (the slots cycle through them, 2026-09-24). Set by each capture; carved
                                  * from padding (0 in older files: then the mmTrade* items below say what is owned). */
    uint32_t mmMasks;            /* bit (1 << MM PlayerMask): 1 Truth .. 20 Giant's */

    /* items */
    uint32_t items;              /* bit (1 << MsItem) */
    uint8_t quiverLevel;         /* 0 none, 1 30, 2 40, 3 50 (both games) */
    uint8_t bombBagLevel;        /* 0 none, 1 20, 2 30, 3 40 */
    uint8_t stickLevel;          /* 0 none, 1 10, 2 20, 3 30 */
    uint8_t nutLevel;            /* 0 none, 1 20, 2 30, 3 40 */
    uint8_t arrows;
    uint8_t bombs;
    uint8_t bombchus;
    uint8_t sticks;
    uint8_t nuts;
    uint8_t magicBeans;
    uint8_t powderKegs;
    uint8_t bottles[MS_SHARED_BOTTLE_SLOTS]; /* MsBottle */
    uint8_t swords;              /* bit (1 << MsSword) */
    uint8_t shields;             /* bit (1 << MsShield) */
    uint8_t razorSwordHealth;    /* hits left (MM swordHealth) */
    uint8_t pad2;

    /* songs and Termina's quest status */
    uint32_t songs;              /* bit (1 << MsSong) */
    uint8_t remains;             /* bit 0 Odolwa, 1 Goht, 2 Gyorg, 3 Twinmold */
    uint8_t pad3;
    uint16_t owlActivationFlags; /* MM OWL_WARP_* bits */
    uint8_t strayFairies[5];     /* Clock Town (0/1), Woodfall, Snowhead, Great Bay, Stone Tower (0-15) */
    /* Majora's Mask's trade slots, as MM ItemId values (0xFF none): deed, key/letter to Mama, Kafei/pendant */
    uint8_t mmTradeDeed;
    uint8_t mmTradeKeyMama;
    uint8_t mmTradeCouple;

    /* Hyrule's equipped gear (SoH gSaveContext.equips / inventory), carried for whoever plays Link in Termina (adult
     * Link in 2Ship, M5). SoH writes it on every capture; it applies it back only when ootGearWriter says Termina
     * changed it (a 2Ship player that uses Hyrule's gear sets ootGearWriter = MS_ENGINE_2SHIP). */
    uint8_t ootAge;              /* LINK_AGE_* at the last capture from SoH */
    uint8_t ootGearWriter;       /* MsEngineId that last wrote this block */
    uint8_t ootButtonItems[8];   /* equips.buttonItems: B, C-left, C-down, C-right, D-pad up, down, left, right */
    uint8_t ootCButtonSlots[7];  /* equips.cButtonSlots */
    uint8_t pad4;
    uint16_t ootEquipment;       /* equips.equipment: sword, shield, tunic, boots (EQUIP_VALUE_*, 4 bits each) */
    uint16_t ootOwnedEquipment;  /* inventory.equipment: owned swords, shields, tunics, boots */
    uint32_t ootUpgrades;        /* inventory.upgrades: strength, scale, quiver, bomb bag, wallet ... */

    /* The unified randomizer's goal (SoH soh/MergeShip/MergeShipGoal.h, 2Ship 2s2h/Rando/MergeShip/UnifiedGoal.h),
     * carved from `reserved` (version 1 files read it as zeros: no goal). Not projected: each engine writes its own
     * part directly when it changes and at every capture, and reads the other's live. Hyrule (SoH) owns goalType,
     * majoraFormMasks, triforceRequired, goalSeed and MS_GOAL_FLAG_HYRULE; Termina (2Ship) owns MS_GOAL_FLAG_MAJORA. Triforce
     * pieces are counted from the ledger (every piece found in either world). goalType 0: not a unified seed, and both
     * games behave as they always do. */
    uint8_t goalType;            /* MS_GOAL_* */
    uint8_t goalFlags;           /* MS_GOAL_FLAG_* */
    uint8_t majoraFormMasks;     /* transformation masks Majora requires besides 2Ship's own counts (0-4) */
    uint8_t itemsCombined;       /* 1: the seed's "Combine Shared Items" (SoH RSK_MS_COMBINE_SHARED_ITEMS; was goalPad,
                                  * 0 in older files): either game's version of an item both games have gives both
                                  * (MsShared_VersionBit). 0: each game's version is its own. Written by SoH with the
                                  * goal (0 outside a unified seed); 2Ship also writes it from the seed it applies. */
    uint16_t triforceRequired;   /* MS_GOAL_TRIFORCE: pieces needed, found in either world */
    uint8_t gossipHints;         /* MS_GOSSIP_HINTS_*: Hyrule's "Gossip Stone Hints" (SoH RSK_GOSSIP_STONE_HINTS) + 1,
                                  * which Termina's gossip stones follow (BUG-0026). 0: not a unified seed (was goalPad2,
                                  * 0 in older files): Termina's own options. Written by SoH with the goal. */
    uint8_t hyruleExtras;        /* MS_HYRULE_EXTRA_*: Hyrule's items that Termina also uses (BUG-0014, was goalPad2):
                                  * Roc's Feather, Deku Beans, Gold Deku Beans. Written by SoH at every capture. */
    uint32_t goalSeed;           /* the unified seed (SoH ctx->GetSeed() == 2Ship rando.finalSeed) the goal belongs to;
                                  * SoH clears goalFlags when it writes another seed here */

    /* Hyrule's quest status and pocket items (the "Hyrule block"), carved from `reserved` like the goal: the size and
     * the version do not change, and a file written before it reads it as zeros. hyruleBlock says which engine wrote
     * it; while it is 0 an apply leaves these untouched (a zero here is "not captured yet", not "none"). Termina holds
     * them in its Hyrule views (2Ship MergeShipHyrule.h, MergeShipOoTItems.h, MergeShipOoTEquip.h). */
    uint32_t hyruleQuest;        /* Ocarina of Time QuestItem bits (MS_HYRULE_QUEST_BITS: medallions, Spiritual
                                  * Stones, Stone of Agony, Gerudo Card; the songs are in `songs`). Merged. */
    uint32_t hyruleItems;        /* bit (1 << MsHyruleItem): Ocarina of Time's masks (merged) and trade items (set) */
    int16_t gsTokens;            /* Gold Skulltula tokens (Hyrule's count, set by SoH; Termina does not show it) */
    uint8_t hyruleSeeds;         /* Deku Seeds (Ocarina of Time AMMO(ITEM_SLINGSHOT)); set both ways */
    uint8_t hyruleTradeChild;    /* MsHyruleItem in Hyrule's child trade slot, MS_HYRULE_SLOT_* otherwise */
    uint8_t hyruleTradeAdult;    /* MsHyruleItem in Hyrule's adult trade slot, MS_HYRULE_SLOT_* otherwise */
    uint8_t hyruleBlock;         /* MS_HYRULE_BLOCK_*: who has written the Hyrule block (0: nobody yet) */
    uint8_t hyruleActiveMM;      /* unused since ammo became one pool (2026-09-30; was: MsOwnBit, Hyrule (SoH) has
                                  * Majora's Mask's copy of the item in its slot now). Left as-is (was pad). */
    uint8_t hyruleChuBag;        /* nonzero: the unified seed has the Bombchu Bag (SoH RSK_BOMBCHU_BAG), its mode
                                  * MS_CHU_BAG_PROGRESSIVE / MS_CHU_BAG_SINGLE: no bombchu ammo is obtainable in
                                  * either game (chests, drops, shops) before chuBagLevel > 0
                                  * (MergeShip_Own_ChusLocked). Written by SoH with the goal; was pad. */

    /* Termina's own swords and shields (2Ship 2s2h/MergeShip/MergeShipTerminaEquip.h, the pause screen's Termina
     * Equipment view), carved from `reserved` like the goal (a file written before it reads zeros: terminaGearValid 0,
     * and an apply leaves Termina's equipment as the rest of the apply sets it). Termina writes it on every capture and
     * reads it on every apply; SoH leaves it alone. Majora's Mask's swords and shields, not Ocarina of Time's. */
    uint8_t terminaSwords;       /* MM's swords Termina holds: bit 0 Kokiri, 1 Razor, 2 Gilded (as MsSword) */
    uint8_t terminaShields;      /* MM's shields Termina holds: bit 0 Hero's, 1 Mirror */
    uint8_t terminaSword;        /* in use: MM EQUIP_VALUE_SWORD_* (0 none), | 0x80 while the Great Fairy's Sword is
                                  * on B (MS_TEQ_SHARED_GREAT_FAIRY_ON_B) */
    uint8_t terminaShield;       /* in use: MM EQUIP_VALUE_SHIELD_* (0 none) */
    uint8_t terminaSeenSwords;   /* `swords` as that capture left it: what Hyrule adds after it is new to Termina */
    uint8_t terminaSeenShields;  /* `shields` likewise */
    uint8_t terminaGearValid;    /* 1 once Termina has written this block */
    uint8_t terminaActiveOoT;    /* unused since ammo became one pool (2026-09-30; was: MsOwnBit, Termina (2Ship) has
                                  * Ocarina of Time's copy of the item in its slot now). Left as-is (was pad). */

    /* The Hyrule file this shared save belongs to (SoH's MergeShip save "sharedFileId", random, made with the file).
     * A new file or a new seed in the slot starts it anew (SoH MergeShipHost_ClaimSharedSave): nothing of an earlier
     * playthrough reaches the new one. 0 = a file from before it, which keeps its shared save. */
    uint32_t hyruleFileId;
    /* 1: the slot was started anew; Termina's file of the slot is the earlier playthrough's and is made again on the
     * next trip (2Ship MergeShipHost.cpp), which clears this. */
    uint8_t terminaFileStale;
    /* Which game owns the items both games have that are one inventory slot in each engine -- bombs, bombchus, sticks,
     * nuts, beans, the bow, the ocarina (MsOwnBit) -- and each game's quiver and bomb bag (MS_BAGS_*). Each engine holds
     * either game's in its own slot and ammo, usable, and shows each only on its own game's menu (the user, 2026-09-24:
     * "you can use ANY mm item and OOT item when you get them"; "you should ONLY get the MM version on ITS OWN menu").
     * Both engines write them live, as an item is given (SoH MergeShipOwnership.h, 2Ship MergeShipOwnership.h). */
    uint8_t hyruleOwn;   /* MsOwnBit: Ocarina of Time's */
    uint8_t terminaOwn;  /* MsOwnBit: Majora's Mask's */
    uint8_t hyruleBags;  /* Ocarina of Time's quiver, bomb bag, stick and nut levels (MS_BAGS_*) */
    uint8_t terminaBags; /* Majora's Mask's */
    uint8_t ownValid;    /* MS_OWN_VALID_*: the game's bits are written (a file from before them derives them once) */
    uint8_t hyruleBottleMM;  /* older builds: bit i, Hyrule's bottle slot i held a bottle of Majora's Mask's (SoH moves
                              * it to `bottles`, then clears the bit) */
    uint8_t terminaBottleOoT; /* unused */
    /* ONE count of bombs .. arrows (MS_OWN_AMMO_COUNT, MsOwnBit order), the same value in Hyrule and Termina (the
     * user, 2026-09-30: ammo is shared, not each game's own -- was two per-game counts here, ootAmmo / mmAmmo, swapped
     * whenever the "active" copy changed; that machinery is gone, so whichever engine last ran writes this array every
     * frame and the other reads it back on its next Bind, MergeShip_Own_Sync). Carved from `reserved` (zeros in an
     * older file: the first engine to bind takes its own live counts once, MS_OWN_VALID_AMMO_HYRULE). */
    uint8_t ammo[MS_OWN_AMMO_COUNT];
    /* The unified randomizer's Bombchu Bag progression (task 3, 2026-09-30), reusing the six bytes the old per-game
     * mmAmmo[] freed: 0 none (no bombchu ammo obtainable anywhere, MergeShip_Own_ChusLocked), 1..3 rising capacity
     * (MS_CHU_BAG_CAPACITY -- 20/30/50, SoH's existing Context::GetBombchuCapacity tiers). The engine that gives a bag
     * raises it (SoH MergeShip_Own_RaiseOoTChuBag, 2Ship MergeShip_Own_UnlockMMChus) and the other reads it back: it
     * is the state that carries the bag across (MsDelivery_StateCarried), never the ledger as well. */
    uint8_t chuBagLevel;
    /* The Clock Town bank's balance (BUG-0030, 2026-10-07), carved from the pad below: Termina's file keeps it
     * (HIGH_SCORE(HS_BANK_RUPEES)) only at MM's own saves, while the rupees paid in are the shared `rupees`, written
     * with every capture -- so a deposit lasted only until the next load of Termina's file, and the money was gone.
     * 2Ship captures and applies it together with `rupees`. bankValid 0 (older files): Termina's file's own. */
    uint8_t bankValid;
    uint8_t bankRupees[2];   /* little-endian */
    /* Deku Beans and Gold Deku Beans (BUG-0012/0014, was mmAmmo[4..5]): their counts, one pool for both games while
     * hyruleExtras has MS_HYRULE_EXTRA_BEAN_AMMO (SoH's "Deku Beans Are Ammo"). SoH writes them at its captures and
     * reads them back at its applies; Termina plants from them directly. */
    uint8_t dekuBeanAmmo;
    uint8_t goldBeanAmmo;
    /* Ocarina of Time's four bottles (MsBottle) while "Combine Shared Items" is off: Hyrule writes its own bottles here,
     * Termina holds them on its Items (Hyrule) page; `bottles` is then Majora's Mask's six, which Hyrule holds on its
     * MM item menu. Combined, `bottles` is the one set both games hold, as before. */
    uint8_t ootBottles[4];

    /* ---- version 2: the unified randomizer's ledger (M3). Captures never touch it; it is only appended to. ---- */
    uint32_t ledgerSeed;  /* the unified seed (SoH ctx->GetSeed() == 2Ship rando.finalSeed) the ledger belongs to;
                           * 0 = none. SoH resets the ledger when a file of another seed is loaded into the slot. */
    uint32_t ledgerCount; /* entries used */
    MsLedgerEntry ledger[MS_SHARED_LEDGER_MAX];

    /* ---- version 3: the Termina checks the Anchor team has obtained (2026-09-30). A teammate's find reaches both
     * engines as MM_SET_CHECK_STATUS; 2Ship marks it in its file, but that file is written only at MM's own saves, and
     * a parked engine holds the packet in memory -- so after a restart the check was open again while its item stayed
     * given (the shared save, the ledger), and finding it again gave a second copy to everyone (the user's second Deku
     * Princess, 2026-09-30). Whichever engine hears of it sets the bit here at once (MsShared_NoteTeamTerminaCheck);
     * 2Ship marks those checks obtained whenever it loads or resumes its file. Only for the unified seed they belong to
     * (teamChecksSeed == ledgerSeed; another seed in the slot starts them anew). */
    uint32_t teamChecksSeed;                                   /* the ledgerSeed the bits below belong to; 0 = none */
    uint8_t teamTerminaChecks[MS_TEAM_TERMINA_CHECK_BYTES];    /* bit per 2Ship RandoCheckId */

    /* ---- version 4 (BUG-0101): the C and D-pad buttons the player last left a world with (MsButtonItem, in
     * MS_SHARED_BUTTONS order). The other world puts each item it also has on the same button on arrival, once:
     * applying clears buttonsWriter. ---- */
    uint8_t buttonsWriter;                         /* MsEngineId of the world that wrote them; 0 = nothing to carry */
    uint8_t buttonItems[MS_SHARED_BUTTONS];

    /* ---- version 5 ---- */
    /* BUG-0120: the sword on B the player last left a world with (MsBSword). The other world puts it on B on arrival
     * when it holds that sword, once: applying clears bWriter. */
    uint8_t bWriter;                               /* MsEngineId of the world that wrote it; 0 = nothing to carry */
    uint8_t bSword;
    /* BUG-0129: Hyrule's magic beans (Ocarina of Time AMMO(ITEM_BEAN)). Termina's bean slot, while it holds only
     * Hyrule's beans (Termina has found none of its own), counts these: planting one there spends one of Hyrule's
     * (hyruleBeansWriter = MS_ENGINE_2SHIP), and Hyrule takes the count back on arrival. SoH writes it on every capture
     * (MS_ENGINE_SOH); 0: a file from before it (Termina's slot keeps its own count then). */
    uint8_t hyruleBeans;
    uint8_t hyruleBeansWriter;
} MsSharedSave;

/* A teammate obtained Termina check `check` (2Ship RandoCheckId): 1 when the bit was newly set. */
static inline int MsShared_NoteTeamTerminaCheck(MsSharedSave* shared, uint32_t check) {
    uint8_t bit;

    if ((shared == NULL) || (shared->ledgerSeed == 0) || (check == 0) || (check >= MS_TEAM_TERMINA_CHECK_BYTES * 8u)) {
        return 0;
    }
    if (shared->teamChecksSeed != shared->ledgerSeed) {
        memset(shared->teamTerminaChecks, 0, sizeof(shared->teamTerminaChecks));
        shared->teamChecksSeed = shared->ledgerSeed;
    }
    bit = (uint8_t)(1u << (check & 7u));
    if (shared->teamTerminaChecks[check >> 3] & bit) {
        return 0;
    }
    shared->teamTerminaChecks[check >> 3] |= bit;
    return 1;
}

static inline int MsShared_TeamTerminaCheck(const MsSharedSave* shared, uint32_t check) {
    if ((shared == NULL) || (shared->ledgerSeed == 0) || (shared->teamChecksSeed != shared->ledgerSeed) ||
        (check >= MS_TEAM_TERMINA_CHECK_BYTES * 8u)) {
        return 0;
    }
    return (shared->teamTerminaChecks[check >> 3] >> (check & 7u)) & 1;
}

/* MsSharedSave.goalType: the unified randomizer's win condition (SoH's setting RSK_MS_GOAL + 1). */
#define MS_GOAL_NONE 0     /* not a unified seed */
#define MS_GOAL_GANON 1    /* Hyrule's win condition (Ganon, or SoH's own trigger); Majora is optional */
#define MS_GOAL_MAJORA 2   /* defeat Majora; Ganon is optional */
#define MS_GOAL_BOTH 3     /* Hyrule's win condition and Majora, in either order */
#define MS_GOAL_TRIFORCE 4 /* a Triforce hunt over both worlds */
/* MsSharedSave.goalFlags */
#define MS_GOAL_FLAG_HYRULE 0x01u   /* Hyrule's win condition is met (SoH) */
#define MS_GOAL_FLAG_MAJORA 0x02u   /* Majora was defeated (2Ship) */
#define MS_GOAL_FLAG_COMPLETE 0x80u /* the whole goal was met and an ending played */

/* MsSharedSave.hyruleBlock */
#define MS_CHATEAU_VALID 0x80u  /* chateauMagic was written by a capture */
#define MS_CHATEAU_ACTIVE 0x01u /* Chateau Romani's endless magic is on */

#define MS_HYRULE_BLOCK_SOH 0x01u   /* SoH has captured the Hyrule block */
#define MS_HYRULE_BLOCK_2SHIP 0x02u /* 2Ship has captured it (what Termina holds of it) */
/* MsSharedSave.hyruleQuest: the Ocarina of Time QuestItem bits that are not songs (0-5 the medallions, 18-20 the
 * Spiritual Stones, 21 the Stone of Agony, 22 the Gerudo Card), in Ocarina of Time's numbering. */
/* MsSharedSave.hyruleExtras. */
#define MS_HYRULE_EXTRA_ROCS_FEATHER 0x01u
#define MS_HYRULE_EXTRA_DEKU_BEANS 0x02u
#define MS_HYRULE_EXTRA_GOLD_BEANS 0x04u
#define MS_HYRULE_EXTRA_BEAN_AMMO 0x08u /* the bean counts are ammo (else unlimited) */
#define MS_HYRULE_EXTRA_VALID 0x80u     /* written by SoH (0: a file from before it) */

/* MsSharedSave.gossipHints. */
#define MS_GOSSIP_HINTS_UNSET 0
#define MS_GOSSIP_HINTS_NONE 1
#define MS_GOSSIP_HINTS_NEED_NOTHING 2
#define MS_GOSSIP_HINTS_NEED_TRUTH 3
#define MS_GOSSIP_HINTS_NEED_STONE 4

#define MS_HYRULE_QUEST_BITS 0x007C003Fu
/* MsSharedSave.hyruleTradeChild / hyruleTradeAdult besides an MsHyruleItem */
#define MS_HYRULE_SLOT_EMPTY 0xFFu
#define MS_HYRULE_SLOT_SOLD_OUT 0xFEu /* the child slot's SOLD OUT (the Keaton Mask was sold, OoT ITEM_SOLD_OUT) */

/* A version 1 file on disk is this struct up to (not including) ledgerSeed. */
#define MS_SHARED_SAVE_V1_SIZE ((uint32_t)offsetof(MsSharedSave, ledgerSeed))
/* A version 2 file: up to (not including) teamChecksSeed. */
#define MS_SHARED_SAVE_V2_SIZE ((uint32_t)offsetof(MsSharedSave, teamChecksSeed))
#define MS_SHARED_SAVE_V3_SIZE ((uint32_t)offsetof(MsSharedSave, buttonsWriter))
/* A version 4 file: up to (not including) bWriter. */
#define MS_SHARED_SAVE_V4_SIZE ((uint32_t)offsetof(MsSharedSave, bWriter))

#endif /* MERGESHIP_SHARED_SAVE_H */
