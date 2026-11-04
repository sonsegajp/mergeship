/*
 * MergeShip2 -- how an item found in one world reaches the other (the unified randomizer, M3).
 * Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md, "Delivery"; SharedSave.h, the ledger.
 *
 * Every check found while a unified seed is played is written to the shared save's ledger (MsLedgerEntry): its world,
 * its check and its item (MsItemKey). The engine of the check's world gives the item natively when it can hold it.
 * The other engine gets it one of two ways, never both (a progressive item given twice would count twice):
 *
 *   state   The M4 shared-save projectors carry it: the finding engine holds it natively, its capture writes it and
 *           the other engine's apply reads it (masks, songs, capacities, the sword chain, bottles, Termina's items...).
 *           Sequential play keeps progressive levels right: each engine applies the other's state before it gives
 *           the next copy natively.
 *   ledger  The entry carries MS_LEDGER_FOR_HYRULE / MS_LEDGER_FOR_TERMINA and the other engine gives the item
 *           natively when it projects the ledger (keys, souls, owls, Ocarina of Time's equipment, quest items...).
 *
 * MsDelivery_StateCarried is the list both engines share. Consumables (MS_CYCLE) never cross: they belong to the
 * engine whose check gave them.
 *
 * Pure C, header only: both engines include it next to MsItems.h.
 */
#ifndef MERGESHIP_MS_DELIVERY_H
#define MERGESHIP_MS_DELIVERY_H

#include <stdint.h>

#include "MsItems.h"
#include "SharedSave.h"

/* The M4 state sync carries the item to the other engine once the finding engine holds it natively. */
static inline int MsDelivery_StateCarried(MsItemKey key) {
    switch (key) {
        /* capacities and levels (MsSharedSave quiverLevel, bombBagLevel, walletLevel, magicLevel, stick/nutLevel;
         * MM's bow, quivers and bags as MM's own bits and levels, SharedSave.h) */
        case MSI_FAIRY_BOW:
        case MSI_PROGRESSIVE_BOW:
        case MSI_BIG_QUIVER:
        case MSI_BIGGEST_QUIVER:
        case MSI_BOMB_BAG:
        case MSI_BIG_BOMB_BAG:
        case MSI_BIGGEST_BOMB_BAG:
        case MSI_PROGRESSIVE_BOMB_BAG:
        case MSI_MM_HEROS_BOW:
        case MSI_MM_PROGRESSIVE_BOW:
        case MSI_MM_BIG_QUIVER:
        case MSI_MM_BIGGEST_QUIVER:
        case MSI_MM_BOMB_BAG:
        case MSI_MM_BIG_BOMB_BAG:
        case MSI_MM_BIGGEST_BOMB_BAG:
        case MSI_MM_PROGRESSIVE_BOMB_BAG:
        case MSI_PROGRESSIVE_WALLET:
        case MSI_ADULT_WALLET:
        case MSI_GIANT_WALLET:
        case MSI_TYCOON_WALLET:
        case MSI_PROGRESSIVE_MAGIC_METER:
        case MSI_MAGIC_SINGLE:
        case MSI_MAGIC_DOUBLE:
        case MSI_PROGRESSIVE_STICK_UPGRADE:
        case MSI_PROGRESSIVE_NUT_UPGRADE:
        case MSI_DEKU_STICK_CAPACITY_20:
        case MSI_DEKU_STICK_CAPACITY_30:
        case MSI_DEKU_NUT_CAPACITY_30:
        case MSI_DEKU_NUT_CAPACITY_40:
        case MSI_DEKU_STICK_BAG:
        case MSI_DEKU_NUT_BAG:
        /* the seed's Bombchu Bag: the giving engine raises MsSharedSave.chuBagLevel (SoH MergeShip_Own_RaiseOoTChuBag,
         * 2Ship MergeShip_Own_UnlockMMChus) and the other reads it back. Both give one more level natively, so a
         * ledger entry as well would count one bag twice. */
        case MSI_PROGRESSIVE_BOMBCHU_BAG:
        /* the progressive ocarina: level 1 is any ocarina (MM's own counts: MS_ITEM_OCARINA_FAIRY), level 2 Hyrule's
         * Ocarina of Time. The fixed-level FAIRY_OCARINA / OCARINA_OF_TIME go through the ledger: Termina's copy past
         * MM's one ocarina arrives as the proxy RI_OOT_OCARINA_OF_TIME. */
        case MSI_PROGRESSIVE_OCARINA:
        case MSI_MM_OCARINA:
        /* life */
        case MSI_HEART_CONTAINER:
        case MSI_PIECE_OF_HEART:
        case MSI_DOUBLE_DEFENSE:
        /* items (MsSharedSave.items) */
        case MSI_FIRE_ARROWS:
        case MSI_ICE_ARROWS:
        case MSI_LIGHT_ARROWS:
        case MSI_LENS_OF_TRUTH:
        case MSI_MM_FIRE_ARROWS:
        case MSI_MM_ICE_ARROWS:
        case MSI_MM_LIGHT_ARROWS:
        case MSI_MM_LENS_OF_TRUTH:
        case MSI_MM_HOOKSHOT:
        case MSI_MM_POWDER_KEG:
        case MSI_MM_PICTOGRAPH_BOX:
        case MSI_MM_GREAT_FAIRYS_SWORD:
        case MSI_MM_BOMBERS_NOTEBOOK:
        /* swords and shields (swords, shields bits) */
        case MSI_PROGRESSIVE_MM_SWORD:
        case MSI_MM_KOKIRI_SWORD:
        case MSI_MM_RAZOR_SWORD:
        case MSI_MM_GILDED_SWORD:
        case MSI_MIRROR_SHIELD:
        case MSI_MM_HEROS_SHIELD:
        case MSI_MM_MIRROR_SHIELD:
        /* Majora's Mask's 24 masks */
        case MSI_MM_DEKU_MASK:
        case MSI_MM_GORON_MASK:
        case MSI_MM_ZORA_MASK:
        case MSI_MM_FIERCE_DEITY_MASK:
        case MSI_MM_MASK_OF_TRUTH:
        case MSI_MM_KAFEIS_MASK:
        case MSI_MM_ALL_NIGHT_MASK:
        case MSI_MM_BUNNY_HOOD:
        case MSI_MM_KEATON_MASK:
        case MSI_MM_GAROS_MASK:
        case MSI_MM_ROMANIS_MASK:
        case MSI_MM_CIRCUS_LEADERS_MASK:
        case MSI_MM_POSTMANS_HAT:
        case MSI_MM_COUPLES_MASK:
        case MSI_MM_GREAT_FAIRYS_MASK:
        case MSI_MM_GIBDO_MASK:
        case MSI_MM_DON_GEROS_MASK:
        case MSI_MM_KAMAROS_MASK:
        case MSI_MM_CAPTAINS_HAT:
        case MSI_MM_STONE_MASK:
        case MSI_MM_BREMEN_MASK:
        case MSI_MM_BLAST_MASK:
        case MSI_MM_MASK_OF_SCENTS:
        case MSI_MM_GIANTS_MASK:
        /* songs both games hold (MsSharedSave.songs) */
        case MSI_SONG_OF_TIME:
        case MSI_EPONAS_SONG:
        case MSI_SONG_OF_STORMS:
        case MSI_SUNS_SONG:
        case MSI_SARIAS_SONG:
        case MSI_MM_SONATA_OF_AWAKENING:
        case MSI_PROGRESSIVE_GORON_LULLABY:
        case MSI_MM_GORONS_LULLABY:
        case MSI_MM_GORON_LULLABY_INTRO:
        case MSI_MM_NEW_WAVE_BOSSA_NOVA:
        case MSI_MM_ELEGY_OF_EMPTINESS:
        case MSI_MM_OATH_TO_ORDER:
        case MSI_MM_SONG_OF_HEALING:
        case MSI_MM_SONG_OF_SOARING:
        /* Termina's quest status, stray fairies and trade slots */
        case MSI_MM_ODOLWAS_REMAINS:
        case MSI_MM_GOHTS_REMAINS:
        case MSI_MM_GYORGS_REMAINS:
        case MSI_MM_TWINMOLDS_REMAINS:
        case MSI_MM_CLOCK_TOWN_STRAY_FAIRY:
        case MSI_MM_WOODFALL_STRAY_FAIRY:
        case MSI_MM_SNOWHEAD_STRAY_FAIRY:
        case MSI_MM_GREAT_BAY_STRAY_FAIRY:
        case MSI_MM_STONE_TOWER_STRAY_FAIRY:
        case MSI_MM_MOONS_TEAR:
        case MSI_MM_LAND_TITLE_DEED:
        case MSI_MM_SWAMP_TITLE_DEED:
        case MSI_MM_MOUNTAIN_TITLE_DEED:
        case MSI_MM_OCEAN_TITLE_DEED:
        case MSI_MM_ROOM_KEY:
        case MSI_MM_LETTER_TO_MAMA:
        case MSI_MM_LETTER_TO_KAFEI:
        case MSI_MM_PENDANT_OF_MEMORIES:
        /* bottles: the bottle and its content (MsSharedSave.bottles; Hyrule shows the first four) */
        case MSI_EMPTY_BOTTLE:
        case MSI_BOTTLE_WITH_MILK:
        case MSI_BOTTLE_WITH_RED_POTION:
        case MSI_MM_EMPTY_BOTTLE:
        case MSI_MM_BOTTLE_WITH_MILK:
        case MSI_MM_BOTTLE_WITH_RED_POTION:
        case MSI_BOTTLE_WITH_GREEN_POTION:
        case MSI_BOTTLE_WITH_BLUE_POTION:
        case MSI_BOTTLE_WITH_FAIRY:
        case MSI_BOTTLE_WITH_FISH:
        case MSI_BOTTLE_WITH_BLUE_FIRE:
        case MSI_BOTTLE_WITH_BUGS:
        case MSI_BOTTLE_WITH_POE:
        case MSI_BOTTLE_WITH_BIG_POE:
        /* (Ruto's Letter and MM's own bottle contents too, with the seed's "Combine Shared Items": MsDelivery_Flags
         * lets every bottle held where it was found cross with the one set of bottles both games hold then. Each
         * game's bottles are its own otherwise, MsDelivery_OwnWorld.) */
            return 1;
        default:
            return 0;
    }
}

/* Never given across: the other world reads the ledger itself. Triforce pieces are counted over both worlds from the
 * ledger by the unified goal (SharedSave.h goalType MS_GOAL_TRIFORCE); giving them natively as well would count them
 * twice and could end either game on its own count. */
static inline int MsDelivery_LedgerOnly(MsItemKey key) {
    return key == MSI_TRIFORCE_PIECE;
}

/* The game whose item `key` is, from MsItems.def's oot / mm columns: MS_WORLD_HYRULE when only Ocarina of Time's engine
 * uses it, MS_WORLD_TERMINA when only Majora's Mask's, MS_WORLD_NONE when both (masks, songs, hearts, magic, wallets,
 * consumables ...). Each game's copy of an item both games have is a row of its own (the bows, arrows, Lens, bags,
 * quivers, ocarinas, hookshots, shields, Kokiri Swords, bottles: MsDelivery_Counterpart), so any check of either world
 * may hold either game's copy, and unless the seed combines them (SoH RSK_MS_COMBINE_SHARED_ITEMS,
 * MsSharedSave.itemsCombined) a copy is its own game's alone wherever it is found (the user, 2026-09-24: "Every item
 * should be available at every check in both games unless turned on or off", "when its off, all oot and all mm items
 * NEED to be separate"): the other world's engine never gives it, and the ledger takes it to its own. */
static inline int MsDelivery_OwnWorld(MsItemKey key) {
    switch (key) {
#define MS_ITEM(k, rg, ri, kind, persist, oot, mm, maxLevel, name) \
    case MSI_##k:                                                  \
        return ((oot) && !(mm)) ? MS_WORLD_HYRULE : (!(oot) && (mm)) ? MS_WORLD_TERMINA : MS_WORLD_NONE;
#define MS_ITEM_OOT(k, rg, kind, persist, oot, mm, maxLevel, name) \
    case MSI_##k:                                                  \
        return ((oot) && !(mm)) ? MS_WORLD_HYRULE : (!(oot) && (mm)) ? MS_WORLD_TERMINA : MS_WORLD_NONE;
#include "MsItems.def"
#undef MS_ITEM
#undef MS_ITEM_OOT
        default:
            return MS_WORLD_NONE;
    }
}

/* The other game's copy of an item both games have (MSI_MAX for none). With the seed's "Combine Shared Items" either is
 * both (SharedSave.h MsShared_CombineVersions / _CombineShields / _CombineSwords): the logic counts the other game's
 * copy with it, the pool's caps count it as the other's, and an engine that finds the other game's copy gives its own. */
static inline MsItemKey MsDelivery_Counterpart(MsItemKey key) {
    switch (key) {
        case MSI_FAIRY_BOW: return MSI_MM_HEROS_BOW;
        case MSI_MM_HEROS_BOW: return MSI_FAIRY_BOW;
        case MSI_PROGRESSIVE_BOW: return MSI_MM_PROGRESSIVE_BOW;
        case MSI_MM_PROGRESSIVE_BOW: return MSI_PROGRESSIVE_BOW;
        case MSI_FIRE_ARROWS: return MSI_MM_FIRE_ARROWS;
        case MSI_MM_FIRE_ARROWS: return MSI_FIRE_ARROWS;
        case MSI_ICE_ARROWS: return MSI_MM_ICE_ARROWS;
        case MSI_MM_ICE_ARROWS: return MSI_ICE_ARROWS;
        case MSI_LIGHT_ARROWS: return MSI_MM_LIGHT_ARROWS;
        case MSI_MM_LIGHT_ARROWS: return MSI_LIGHT_ARROWS;
        case MSI_LENS_OF_TRUTH: return MSI_MM_LENS_OF_TRUTH;
        case MSI_MM_LENS_OF_TRUTH: return MSI_LENS_OF_TRUTH;
        case MSI_BIG_QUIVER: return MSI_MM_BIG_QUIVER;
        case MSI_MM_BIG_QUIVER: return MSI_BIG_QUIVER;
        case MSI_BIGGEST_QUIVER: return MSI_MM_BIGGEST_QUIVER;
        case MSI_MM_BIGGEST_QUIVER: return MSI_BIGGEST_QUIVER;
        case MSI_BOMB_BAG: return MSI_MM_BOMB_BAG;
        case MSI_MM_BOMB_BAG: return MSI_BOMB_BAG;
        case MSI_BIG_BOMB_BAG: return MSI_MM_BIG_BOMB_BAG;
        case MSI_MM_BIG_BOMB_BAG: return MSI_BIG_BOMB_BAG;
        case MSI_BIGGEST_BOMB_BAG: return MSI_MM_BIGGEST_BOMB_BAG;
        case MSI_MM_BIGGEST_BOMB_BAG: return MSI_BIGGEST_BOMB_BAG;
        case MSI_PROGRESSIVE_BOMB_BAG: return MSI_MM_PROGRESSIVE_BOMB_BAG;
        case MSI_MM_PROGRESSIVE_BOMB_BAG: return MSI_PROGRESSIVE_BOMB_BAG;
        /* MM's one ocarina is level 1 of Hyrule's progressive ocarina */
        case MSI_PROGRESSIVE_OCARINA:
        case MSI_FAIRY_OCARINA:
        case MSI_OCARINA_OF_TIME: return MSI_MM_OCARINA;
        case MSI_MM_OCARINA: return MSI_PROGRESSIVE_OCARINA;
        /* MM's Hookshot is level 1 of Hyrule's (SharedSave.h MS_SHARED_HOOKSHOTS) */
        case MSI_PROGRESSIVE_HOOKSHOT:
        case MSI_HOOKSHOT:
        case MSI_LONGSHOT: return MSI_MM_HOOKSHOT;
        case MSI_MM_HOOKSHOT: return MSI_HOOKSHOT;
        case MSI_HYLIAN_SHIELD: return MSI_MM_HEROS_SHIELD;
        case MSI_MM_HEROS_SHIELD: return MSI_HYLIAN_SHIELD;
        case MSI_MIRROR_SHIELD: return MSI_MM_MIRROR_SHIELD;
        case MSI_MM_MIRROR_SHIELD: return MSI_MIRROR_SHIELD;
        case MSI_KOKIRI_SWORD: return MSI_MM_KOKIRI_SWORD;
        case MSI_MM_KOKIRI_SWORD: return MSI_KOKIRI_SWORD;
        case MSI_EMPTY_BOTTLE: return MSI_MM_EMPTY_BOTTLE;
        case MSI_MM_EMPTY_BOTTLE: return MSI_EMPTY_BOTTLE;
        case MSI_BOTTLE_WITH_MILK: return MSI_MM_BOTTLE_WITH_MILK;
        case MSI_MM_BOTTLE_WITH_MILK: return MSI_BOTTLE_WITH_MILK;
        case MSI_BOTTLE_WITH_RED_POTION: return MSI_MM_BOTTLE_WITH_RED_POTION;
        case MSI_MM_BOTTLE_WITH_RED_POTION: return MSI_BOTTLE_WITH_RED_POTION;
        default: return MSI_MAX;
    }
}

/* The MS_LEDGER_* flags of a check found in `world` holding `key`: `heldHere` is whether the finding engine gave it
 * natively. Consumables stay where they were found; state-carried items cross with the state once held. `combined`:
 * the seed's "Combine Shared Items"; without it an item of one game's own (MsDelivery_OwnWorld) reaches only that game:
 * found in its own world it stays there, found in the other the ledger takes it home. */
static inline int MsDelivery_IsBottleKey(MsItemKey key) {
    switch (key) {
#define MS_ITEM(k, rg, ri, kind, persist, oot, mm, maxLevel, name) \
    case MSI_##k:                                                  \
        return (kind) == MS_KIND_BOTTLE;
#define MS_ITEM_OOT(k, rg, kind, persist, oot, mm, maxLevel, name) \
    case MSI_##k:                                                  \
        return (kind) == MS_KIND_BOTTLE;
#include "MsItems.def"
#undef MS_ITEM
#undef MS_ITEM_OOT
        default:
            return 0;
    }
}

static inline uint8_t MsDelivery_Flags(MsItemKey key, int world, int heldHere, int consumable, int combined) {
    uint8_t flags = heldHere ? MS_LEDGER_GIVEN_AT_SOURCE : 0;
    if (consumable || MsDelivery_LedgerOnly(key)) {
        return flags;
    }
    if (!combined && (MsDelivery_OwnWorld(key) != MS_WORLD_NONE)) {
        /* A bottle of the other game's found here goes straight into that game's bottles in the shared save
         * (MsSharedSave.bottles / ootBottles, which the finding engine fills and the other applies): no ledger entry,
         * or it would be given a second time. */
        if ((MsDelivery_OwnWorld(key) != world) && !MsDelivery_IsBottleKey(key)) {
            flags |= (MsDelivery_OwnWorld(key) == MS_WORLD_HYRULE) ? MS_LEDGER_FOR_HYRULE : MS_LEDGER_FOR_TERMINA;
        }
        return flags;
    }
    /* Combined, MsSharedSave.bottles is the one set of bottles both games hold: a bottle held where it was found
     * reaches the other game with it, whatever it holds -- Ruto's Letter and Majora's Mask's own contents (the Deku
     * Princess, a Zora Egg ...) as much as the bottles MsDelivery_StateCarried lists. A ledger entry as well made the
     * other engine give a second bottle of the same content (the user, 2026-09-30: "I was given an extra deku
     * princess"). Not held (the finder's bottles were full): the ledger gives the other game its own bottle. */
    if (!heldHere || !(MsDelivery_StateCarried(key) || MsDelivery_IsBottleKey(key))) {
        flags |= (world == MS_WORLD_HYRULE) ? MS_LEDGER_FOR_TERMINA : MS_LEDGER_FOR_HYRULE;
    }
    return flags;
}

/* A bottle item (the bottle itself comes with it). */
static inline int MsDelivery_IsBottle(MsKind kind) {
    return kind == MS_KIND_BOTTLE;
}

/* A bottle content (SharedSave.h MsBottle) that is one of a kind in its world. Majora's Mask never has two, so no
 * delivery makes a second one: not a randomizer item (SoH's RSK_SHUFFLE_MM_ITEMS "Bottle with Deku Princess"), not a
 * teammate's find (Anchor), not the ledger, not the shared save (the user, 2026-09-30: "I was given an extra deku
 * princess ... Stuff like deku princess absolutely should not do that"). A bottle item holding one while its world has
 * it already comes as an empty bottle: the bottle is the item, the content is the world's. Each engine checks what it
 * can see: SoH Majora's Mask's six in the shared save (MergeShip_Own_GiveMMBottle); 2Ship Termina's six and its week
 * events (UnifiedDelivery.cpp GiveNative, MergeShipShared_Apply).
 *   DEKU_PRINCESS  MM EnDnp_Init: Woodfall Temple's princess is there only while none is in a bottle and she is not
 *                  back in the Deku King's chamber this cycle (WEEKEVENTREG_23_20).
 *   SEAHORSE       MM EnTsn / EnOt: the fisherman's one seahorse, the one Pinnacle Rock's waits for.
 * The other contents may be held more than once in MM too, so they are not capped:
 *   ZORA_EGG                  seven in the world, each its own; several are carried at once.
 *   GOLD_DUST, HYLIAN_LOACH   prizes that are won again (MM ITEM_GOLD_DUST_2, ITEM_HYLIAN_LOACH_2).
 *   MUSHROOM, SPRING_WATER, HOT_SPRING_WATER, BUG, FISH, FAIRY, POE, BIG_POE, BLUE_FIRE   caught again.
 *   RED/GREEN/BLUE_POTION, MILK, MILK_HALF, CHATEAU_ROMANI, MYSTERY_MILK                   bought again.
 *   RUTOS_LETTER              Ocarina of Time's one; SoH's own rules keep it (its randomizer item, Anchor's team
 *                             state).
 * Unique or not, a content alone is never a found item: a catch (MM Player_UpdateBottleHeld, which never reaches
 * Item_Give) or a refill (MM Item_Give filling a bottle the player has) never goes to a teammate; a bottle item (a
 * "Bottle with X", a new bottle) does (AnchorNet.h, "Bottles"). */
static inline int MsDelivery_BottleOnePerWorld(int bottle) {
    return (bottle == MS_BOTTLE_DEKU_PRINCESS) || (bottle == MS_BOTTLE_SEAHORSE);
}

#endif /* MERGESHIP_MS_DELIVERY_H */
