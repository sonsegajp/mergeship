/*
 * MergeShip2 -- the unified randomizer's canonical items (MsItems.def), for both games.
 *
 * Plain C, and free of either game's enums: it declares only what the table itself defines. Each game maps the
 * table onto its own enums (SoH: soh/MergeShip/MergeShipMsItems.cpp; 2Ship: Rando/Types.h and Rando/MergeShip/).
 * The decisions behind the table are in Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md ("Settled decisions").
 */
#ifndef MERGESHIP_MS_ITEMS_H
#define MERGESHIP_MS_ITEMS_H

/* Bumped whenever a row is added, removed or reordered, or a column's meaning changes. A MERGESHIP_SEED records
 * it (and MSI_MAX) in its "shared" section; an engine built from another table refuses the seed. */
#define MS_ITEMS_VERSION 1

typedef enum MsKind {
    MS_KIND_MAJOR,
    MS_KIND_LESSER,
    MS_KIND_JUNK,
    MS_KIND_HEALTH,
    MS_KIND_MASK,
    MS_KIND_SONG,
    MS_KIND_BOTTLE,
    MS_KIND_SMALL_KEY,
    MS_KIND_BOSS_KEY,
    MS_KIND_DUNGEON_MAP,
    MS_KIND_TOKEN,
    MS_KIND_STRAY_FAIRY,
    MS_KIND_SOUL,
    MS_KIND_TRAP,
    MS_KIND_MAX
} MsKind;

typedef enum MsPersist {
    MS_PERM,  /* shared ownership, survives MM's cycle reset */
    MS_CYCLE, /* consumable: credited to the collecting engine, reset by MM's Song of Time on the MM side */
} MsPersist;

/* The canonical item ids, in table order. */
typedef enum MsItemKey {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name) MSI_##key,
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name) MSI_##key,
#include "MsItems.def"
#undef MS_ITEM
#undef MS_ITEM_OOT
    MSI_MAX
} MsItemKey;

/* 2Ship's RandoItemType of a kind (2Ship has no song, bottle, map, soul or trap types). Only meaningful where
 * 2Ship's RITYPE_* are visible. */
#define MS_KIND_TO_RITYPE(kind) MS_RITYPE_OF_##kind
#define MS_RITYPE_OF_MS_KIND_MAJOR RITYPE_MAJOR
#define MS_RITYPE_OF_MS_KIND_LESSER RITYPE_LESSER
#define MS_RITYPE_OF_MS_KIND_JUNK RITYPE_JUNK
#define MS_RITYPE_OF_MS_KIND_HEALTH RITYPE_HEALTH
#define MS_RITYPE_OF_MS_KIND_MASK RITYPE_MASK
#define MS_RITYPE_OF_MS_KIND_SONG RITYPE_MAJOR
#define MS_RITYPE_OF_MS_KIND_BOTTLE RITYPE_MAJOR
#define MS_RITYPE_OF_MS_KIND_SMALL_KEY RITYPE_SMALL_KEY
#define MS_RITYPE_OF_MS_KIND_BOSS_KEY RITYPE_BOSS_KEY
#define MS_RITYPE_OF_MS_KIND_DUNGEON_MAP RITYPE_LESSER
#define MS_RITYPE_OF_MS_KIND_TOKEN RITYPE_SKULLTULA_TOKEN
#define MS_RITYPE_OF_MS_KIND_STRAY_FAIRY RITYPE_STRAY_FAIRY
#define MS_RITYPE_OF_MS_KIND_SOUL RITYPE_LESSER
#define MS_RITYPE_OF_MS_KIND_TRAP RITYPE_LESSER

/* The MERGESHIP_SEED file: one JSON with a section per engine (UNIFIED_RANDO_DESIGN.md, "Delivery").
 *   { "type": "MERGESHIP_SEED", "version": MS_SEED_VERSION,
 *     "shared": { "msItemsVersion", "msItemCount", "door", "placements": { <check name>: <item key> }, ... },
 *     "oot": <SoH spoiler log>, "mm": <2S2H spoiler body, type 2S2H_RANDO_SPOILER>, "hints", "playthrough" } */
#define MS_SEED_TYPE "MERGESHIP_SEED"
#define MS_SEED_VERSION 1

#endif /* MERGESHIP_MS_ITEMS_H */
