/*
 * MergeShip2 -- one world's checks, as the other engine needs them for hints and trackers (the unified randomizer,
 * M6/M7: UNIFIED_RANDO_DESIGN.md "Hints and trackers").
 *
 * SoH generated the unified seed and holds all of it: Hyrule's checks (its own) and Termina's (RC_MM_*, the same
 * checks as 2Ship's RandoCheckId). 2Ship holds only Termina's. So SoH describes Hyrule to 2Ship: every Hyrule check
 * that holds an item of the seed and every check its check tracker shows (MS_CATALOG_TRACKED), with their names, areas
 * and the item the seed placed there. It hands the catalog over with every trip to Termina (MsTransfer.worldCatalog,
 * EngineApi.h), as it hands over the entrance table. What has been found is not in the catalog: that is the shared
 * save's ledger (SharedSave.h, MsLedgerEntry world + check), read live.
 *
 * The catalog is owned by the sender and stays valid only until its next trip (SoH rebuilds it for every trip, from
 * the check tracker's options of the moment): the receiver copies what it keeps on arrival (2Ship:
 * 2s2h/Rando/MergeShip/UnifiedHyrule.h). Plain C, plain data.
 */
#ifndef MERGESHIP_MS_WORLD_CATALOG_H
#define MERGESHIP_MS_WORLD_CATALOG_H

#include <stdint.h>

#define MS_CATALOG_VERSION 1u

/* MsCatalogCheck.flags */
#define MS_CATALOG_MAJOR 0x01u   /* holds an item worth hinting (not junk, not a consumable) */
#define MS_CATALOG_DUNGEON 0x02u /* in a dungeon of its world */
#define MS_CATALOG_TRACKED 0x04u /* the sender's check tracker shows it (the rest are there for hints) */

typedef struct MsCatalogCheck {
    uint16_t check;   /* the check in its world's numbering (Hyrule: SoH RandomizerCheck), as in MsLedgerEntry */
    uint16_t item;    /* MsItemKey the seed placed there; MSI_MAX when it is not an item of the table */
    uint16_t area;    /* index into MsWorldCatalog.areaNames */
    uint8_t flags;    /* MS_CATALOG_* */
    uint8_t pad;
    const char* name; /* the check's name in its world ("Mido's Top Left Chest"), UTF-8 */
} MsCatalogCheck;

typedef struct MsWorldCatalog {
    uint32_t size;    /* sizeof(MsWorldCatalog) as the sender knew it */
    uint32_t version; /* MS_CATALOG_VERSION */
    uint32_t seed;    /* the unified seed (MsSharedSave.ledgerSeed) the placements belong to */
    int32_t world;    /* MsWorld (SharedSave.h) the checks are in */
    uint32_t areaCount;
    const char* const* areaNames; /* the world's areas as its tracker names them ("Kokiri Forest") */
    uint32_t checkCount;
    const MsCatalogCheck* checks; /* in the sender's tracker order (area by area) */
} MsWorldCatalog;

#endif /* MERGESHIP_MS_WORLD_CATALOG_H */
