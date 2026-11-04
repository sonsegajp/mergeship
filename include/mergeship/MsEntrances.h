/*
 * MergeShip2 -- entrance randomizer keys shared by both engines (unified randomizer M8,
 * Shipwright/mergeship2/docs/UNIFIED_RANDO_DESIGN.md "Entrances").
 *
 * SoH's entrance shuffler names every entrance by an int16 index. Hyrule's are Ocarina of Time's own entrance
 * indices (ENTR_*, and the entrance randomizer's grotto indices 0x700 + id / 0x800 + id). Termina's get keys in a
 * range no Ocarina of Time index uses, so one seed's override table can hold both worlds:
 *
 *   0x4000 | (mmEntrance >> 4)   arrive in Termina at Majora's Mask entrance ENTRANCE(scene, spawn) (layer bits 0).
 *                                Dungeons, boss doors, grottos, overworld connections, interior doors and the moon's
 *                                one-way warps (a moon child's trial, the Clock Tower rooftop's Oath to Order) all use
 *                                this form; seasonal scenes use their winter / poisoned entrance (MM swaps it on load).
 *                                MM grotto n is the key of ENTRANCE(GROTTOS, n), with 2Ship's randomizer numbering
 *                                (Rando/Logic/Logic.cpp GetRegionIdFromEntrance: GROTTOS 17-31 stand for the generic
 *                                grottos MM enters through spawns 4 and 10).
 *   0x5000 | n                   leave MM grotto n back out of its hole (MM's grotto exit returns to the respawn point
 *                                the hole set, not to an entrance of its own).
 *   0x5100 | boss                leave an MM boss lair backwards (MM's lairs have no way out but the blue warp; this is
 *                                where an Ocarina of Time boss room behind an MM boss door returns to: the dungeon's
 *                                entrance). boss: 0 Odolwa, 1 Goht, 2 Gyorg, 3 Twinmold.
 *
 * A key is always a positive int16. MsTransfer.entrance carries a key (with MS_TRANSFER_FLAG_RANDO_ENTRANCE): an
 * Ocarina of Time index for SoH, a Termina key for 2Ship.
 */
#ifndef MERGESHIP_MS_ENTRANCES_H
#define MERGESHIP_MS_ENTRANCES_H

#include <stdint.h>

#define MS_ENTR_KEY_TERMINA_FIRST 0x4000
#define MS_ENTR_KEY_TERMINA_END 0x6000 /* exclusive */

#define MS_ENTR_KEY_MM(mmEntrance) ((int16_t)(0x4000 | ((((uint16_t)(mmEntrance)) >> 4) & 0x0FFF)))
#define MS_ENTR_KEY_GROTTO_RETURN(n) ((int16_t)(0x5000 | ((n) & 0xFF)))
#define MS_ENTR_KEY_BOSS_RETURN(boss) ((int16_t)(0x5100 | ((boss) & 0xFF)))

#define MS_ENTR_IS_TERMINA(key) (((int32_t)(key) >= MS_ENTR_KEY_TERMINA_FIRST) && ((int32_t)(key) < MS_ENTR_KEY_TERMINA_END))
#define MS_ENTR_IS_MM_ENTRANCE(key) (((int32_t)(key) & 0xF000) == 0x4000)
#define MS_ENTR_IS_GROTTO_RETURN(key) (((int32_t)(key) & 0xFF00) == 0x5000)
#define MS_ENTR_IS_BOSS_RETURN(key) (((int32_t)(key) & 0xFF00) == 0x5100)
/* The Majora's Mask entrance of a MS_ENTR_KEY_MM key. */
#define MS_ENTR_KEY_TO_MM(key) ((uint16_t)((((uint16_t)(key)) & 0x0FFF) << 4))
#define MS_ENTR_KEY_INDEX(key) ((int32_t)(key) & 0xFF)

#define MS_ENTR_BOSS_ODOLWA 0
#define MS_ENTR_BOSS_GOHT 1
#define MS_ENTR_BOSS_GYORG 2
#define MS_ENTR_BOSS_TWINMOLD 3
#define MS_ENTR_BOSS_COUNT 4

/* Seed file: MERGESHIP_SEED["entrances"] (written by SoH's generator, MergeShipEntrances.cpp):
 *   { "version": MS_ENTR_SEED_VERSION,
 *     "termina": [ [key, override], ... ],   every override of a Termina key: what 2Ship applies
 *     "hyrule":  [ [index, override], ... ]  Ocarina of Time indices whose override is a Termina key (informational;
 *                                             SoH applies its own from the "oot" section's "entrances") }
 * An override is a key: a Termina key stays in Majora's Mask, anything else is an Ocarina of Time index (a trip to
 * Hyrule). */
#define MS_ENTR_SEED_VERSION 1

#endif /* MERGESHIP_MS_ENTRANCES_H */
