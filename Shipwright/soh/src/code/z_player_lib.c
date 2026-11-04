#include "soh/MergeShip/MergeShipGanondorf.h"
#include "soh/MergeShip/MergeShipAdultMask.h" // MergeShip2: the Adult Mask
#include "global.h"
#include "soh/MergeShip/MergeShip3dsSkin.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "objects/gameplay_field_keep/gameplay_field_keep.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_link_child/object_link_child.h"
// MergeShip2: the four Majora's Mask forms. These skeletons are read at run time from mm.o2r;
// the mm_ prefix exists because OoT and MM spell 2195 asset identifiers identically.
#include "mm/objects/object_link_nuts/object_link_nuts.h"
#include "mm/objects/object_link_goron/object_link_goron.h"
#include "mm/objects/object_link_zora/object_link_zora.h"
#include "mm/objects/object_link_boy/object_link_boy.h"
#include "mm/objects/gameplay_keep/gameplay_keep.h"
#include "mm/objects/object_mask_boy/object_mask_boy.h"
#include "mm/objects/object_mask_goron/object_mask_goron.h"
#include "mm/objects/object_mask_zora/object_mask_zora.h"
#include "mm/objects/object_mask_nuts/object_mask_nuts.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"
#include "soh/MergeShip/MergeShipMMMasks.h" // MergeShip2: MM's other twenty masks
#include "soh/MergeShip/MergeShipChateau.h" // MergeShip2: MM's Chateau Romani (PLAYER_BOTTLE_MM_CHATEAU)
#include "soh/MergeShip/MergeShipMMEquipment.h" // MergeShip2: MM's swords, shields, bow, hookshot (...Draw.inc, at the end)
#include "soh/MergeShip/MergeShipMMItems.h" // MergeShip2: MM's keg and bottle contents (PLAYER_BOTTLE_MM_*)
#include "soh/MergeShip/MergeShipSohFd.h"   // MergeShip2 (soh_fd): soh_fd's form options
#include "soh/MergeShip/MergeShipChildLink.h" // MergeShip2: MM's human as the child (MergeShipChildLinkDraw.inc, at the end)
#include "soh/MergeShip/forms/MergeShipDekuDraw.h" // MergeShip2: the Deku's drawing (MM z_player_lib.c)
#include "soh/MergeShip/forms/MergeShipGoron.h" // MergeShip2: the Goron's drawing (forms/MergeShipGoronLib.c)
#include "soh/MergeShip/forms/MergeShipZora.h"  // MergeShip2: the Zora's drawing (forms/MergeShipZoraDraw.inc, at the end)
#include "overlays/actors/ovl_Demo_Effect/z_demo_effect.h"

#include <libultraship/bridge/resourcebridge.h>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/Enhancements/randomizer/draw.h"
#include "soh/ResourceManagerHelpers.h"

#include <stdlib.h>
#include <libultraship/bridge/consolevariablebridge.h>

typedef struct {
    /* 0x00 */ u8 flag;
    /* 0x02 */ u16 textId;
} TextTriggerEntry; // size = 0x04

typedef struct {
    /* 0x00 */ void* dList;
    /* 0x04 */ Vec3f pos;
} BowStringData; // size = 0x10

// MergeShip2: indexed by PlayerForm, which extends OoT's linkAge rather than replacing it, so
// every existing gPlayerSkelHeaders[gSaveContext.linkAge] use keeps working unchanged.
FlexSkeletonHeader* gPlayerSkelHeaders[] = {
    &gLinkAdultSkel,          // PLAYER_FORM_ADULT
    &gLinkChildSkel,          // PLAYER_FORM_CHILD
    &mm_gLinkDekuSkel,        // PLAYER_FORM_DEKU         (object_link_nuts)
    &mm_gLinkGoronSkel,       // PLAYER_FORM_GORON
    &mm_gLinkZoraSkel,        // PLAYER_FORM_ZORA
    &mm_gLinkFierceDeitySkel, // PLAYER_FORM_FIERCE_DEITY (object_link_boy)
};
MERGESHIP_ASSERT_FORM_TABLE(gPlayerSkelHeaders);

s16 sBootData[PLAYER_BOOTS_MAX][17] = {
    { 200, 1000, 300, 700, 550, 270, 600, 350, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 1000, 0, 800, 300, -160, 600, 590, 750, 125, 200, 130 },
    { 200, 1000, 300, 700, 550, 270, 600, 600, 800, 550, -100, 600, 540, 270, 25, 0, 130 },
    { 200, 1000, 300, 700, 380, 400, 0, 300, 800, 500, -100, 600, 590, 750, 125, 200, 130 },
    { 80, 800, 150, 700, 480, 270, 600, 50, 800, 550, -40, 400, 540, 270, 25, 0, 80 },
    { 200, 1000, 300, 800, 500, 400, 800, 400, 800, 550, -100, 600, 540, 750, 125, 400, 200 },
};

// Used to map action params to model groups
u8 sActionModelGroups[] = {
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NONE
    PLAYER_MODELGROUP_SWORD,            // PLAYER_IA_SWORD_CS
    PLAYER_MODELGROUP_10,               // PLAYER_IA_FISHING_POLE
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_MASTER
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_SWORD_KOKIRI
    PLAYER_MODELGROUP_BGS,              // PLAYER_IA_SWORD_BIGGORON
    PLAYER_MODELGROUP_10,               // PLAYER_IA_DEKU_STICK
    PLAYER_MODELGROUP_HAMMER,           // PLAYER_IA_HAMMER
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_FIRE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_ICE
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_LIGHT
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0C
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0D
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_BOW_0E
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_SLINGSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_HOOKSHOT
    PLAYER_MODELGROUP_HOOKSHOT,         // PLAYER_IA_LONGSHOT
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMB
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_BOMBCHU
    PLAYER_MODELGROUP_BOOMERANG,        // PLAYER_IA_BOOMERANG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_15
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_SPELL_16
    PLAYER_MODELGROUP_BOW_SLINGSHOT,    // PLAYER_IA_MAGIC_SPELL_17
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FARORES_WIND
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_NAYRUS_LOVE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DINS_FIRE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_DEKU_NUT
    PLAYER_MODELGROUP_OCARINA,          // PLAYER_IA_OCARINA_FAIRY
    PLAYER_MODELGROUP_OOT,              // PLAYER_IA_OCARINA_OF_TIME
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FISH
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FIRE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BUG
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_BIG_POE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_RUTOS_LETTER
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_RED
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_BLUE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_POTION_GREEN
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_FULL
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_MILK_HALF
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_BOTTLE_FAIRY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ZELDAS_LETTER
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_WEIRD_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CHICKEN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MAGIC_BEAN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_EGG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POCKET_CUCCO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_COJIRO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_MUSHROOM
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_ODD_POTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_POACHERS_SAW
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_BROKEN_GORONS_SWORD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_PRESCRIPTION
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_FROG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_EYEDROPS
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_CLAIM_CHECK
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_KEATON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SKULL
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_SPOOKY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_BUNNY_HOOD
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GORON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_ZORA
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_GERUDO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MASK_TRUTH
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_LENS_OF_TRUTH
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_FIERCE_DEITY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_GORON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_ZORA
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_DEKU
    PLAYER_MODELGROUP_10,               // PLAYER_IA_MM_ZORA_BOOMERANG (MM PLAYER_MODELGROUP_ZORA_BOOMERANG: closed hands; its
                                        // PLAYER_ANIMTYPE_DEFAULT is set in Player_SetModelGroup)
    PLAYER_MODELGROUP_BGS,              // PLAYER_IA_MM_SWORD_TWO_HANDED (MM PLAYER_MODELGROUP_TWO_HANDED_SWORD)
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_TRUTH
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_KAFEIS_MASK
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_ALL_NIGHT
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_BUNNY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_KEATON
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_GARO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_ROMANI
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_CIRCUS_LEADER
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_POSTMAN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_COUPLE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_GREAT_FAIRY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_GIBDO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_DON_GERO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_KAMARO
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_CAPTAIN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_STONE
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_BREMEN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_BLAST
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_SCENTS
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MASK_GIANT
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_CHATEAU // MergeShip2: MM sPlayerItemModelGroups
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_MM_SWORD_RAZOR // MergeShip2: MM sPlayerItemModelGroups
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_IA_MM_SWORD_GILDED
    PLAYER_MODELGROUP_BGS,              // PLAYER_IA_MM_SWORD_GREAT_FAIRY (MM PLAYER_MODELGROUP_TWO_HANDED_SWORD)
    // MergeShip2: MM sPlayerItemModelGroups for the keg, the Pictograph Box, the bottle contents and trade items.
    PLAYER_MODELGROUP_EXPLOSIVES,       // PLAYER_IA_MM_POWDER_KEG
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_PICTOGRAPH_BOX
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_DEKU_PRINCESS
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_ZORA_EGG
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_GOLD_DUST
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_MUSHROOM
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_SEAHORSE
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_HYLIAN_LOACH
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_SPRING_WATER
    PLAYER_MODELGROUP_BOTTLE,           // PLAYER_IA_MM_BOTTLE_HOT_SPRING_WATER
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_MOONS_TEAR
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_DEED_LAND
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_DEED_SWAMP
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_DEED_MOUNTAIN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_DEED_OCEAN
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_ROOM_KEY
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_LETTER_TO_MAMA
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_LETTER_TO_KAFEI
    PLAYER_MODELGROUP_DEFAULT,          // PLAYER_IA_MM_PENDANT_OF_MEMORIES
};
MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(sActionModelGroups) == PLAYER_IA_MAX, "sActionModelGroups needs one entry per PlayerItemAction");

TextTriggerEntry sTextTriggers[] = {
    { 1, 0x3040 },
    { 2, 0x401D },
    { 0, 0x0000 },
    { 2, 0x401D },
};

// Used to map model groups to model types for [animation, left hand, right hand, sheath, waist]
u8 gPlayerModelTypes[PLAYER_MODELGROUP_MAX][PLAYER_MODELGROUPENTRY_MAX] = {
    /* PLAYER_MODELGROUP_0 */
    { PLAYER_ANIMTYPE_2, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_16,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD_AND_SHIELD */
    { PLAYER_ANIMTYPE_1, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_SHIELD, PLAYER_MODELTYPE_SHEATH_17,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_DEFAULT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_4 */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BGS */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_BGS, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOW_SLINGSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_BOW_SLINGSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_EXPLOSIVES */
    { PLAYER_ANIMTYPE_5, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOOMERANG */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOOMERANG, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HOOKSHOT */
    { PLAYER_ANIMTYPE_4, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_HOOKSHOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_10 */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_CLOSED, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_HAMMER */
    { PLAYER_ANIMTYPE_3, PLAYER_MODELTYPE_LH_HAMMER, PLAYER_MODELTYPE_RH_CLOSED, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OCARINA */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OCARINA, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_OOT */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_OPEN, PLAYER_MODELTYPE_RH_OOT, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_BOTTLE */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_BOTTLE, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_18,
      PLAYER_MODELTYPE_WAIST },
    /* PLAYER_MODELGROUP_SWORD */
    { PLAYER_ANIMTYPE_0, PLAYER_MODELTYPE_LH_SWORD, PLAYER_MODELTYPE_RH_OPEN, PLAYER_MODELTYPE_SHEATH_19,
      PLAYER_MODELTYPE_WAIST },
};

Gfx* sPlayerRightHandShieldDLs[PLAYER_SHIELD_MAX * 2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightFistAndDekuShieldNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightFistAndDekuShieldFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingHylianShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingHylianShieldFarDL,
    gLinkChildRightHandClosedFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingMirrorShieldNearDL,
    gLinkChildRightHandClosedNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingMirrorShieldFarDL,
    gLinkChildRightHandClosedFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandShieldDLs, 4);

Gfx* sSheathWithSwordDLs[(PLAYER_SHIELD_MAX + 2) * 2 * PLAYER_FORM_MAX] = {
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildDekuShieldSwordAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildDekuShieldSwordAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultHylianShieldSwordAndSheathNearDL,
    gLinkChildHylianShieldSwordAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultHylianShieldSwordAndSheathFarDL,
    gLinkChildHylianShieldSwordAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMirrorShieldSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMirrorShieldSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    gLinkChildDekuShieldWithMatrixDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sSheathWithSwordDLs, 6);

Gfx* sSheathWithoutSwordDLs[(PLAYER_SHIELD_MAX + 2) * 2 * PLAYER_FORM_MAX] = {
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathFarDL,
    gLinkChildDekuShieldAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultHylianShieldAndSheathNearDL,
    gLinkChildHylianShieldAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultHylianShieldAndSheathFarDL,
    gLinkChildHylianShieldAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMirrorShieldAndSheathNearDL,
    gLinkChildSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMirrorShieldAndSheathFarDL,
    gLinkChildSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathNearDL,
    gLinkChildDekuShieldWithMatrixDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sSheathWithoutSwordDLs, 6);

Gfx* gPlayerLeftHandBgsDLs[4 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandHoldingBgsNearDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandHoldingSwordDL,
    gLinkAdultLeftHandHoldingBgsFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandHoldingSwordDL,
    gLinkAdultHandHoldingBrokenGiantsKnifeDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandHoldingSwordDL,
    gLinkAdultHandHoldingBrokenGiantsKnifeFarDL,
    gLinkChildLeftHandHoldingMasterSwordDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandHoldingSwordDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(gPlayerLeftHandBgsDLs, 2);

Gfx* gPlayerLeftHandOpenDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftHandNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftHandFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(gPlayerLeftHandOpenDLs, 1);

Gfx* gPlayerLeftHandClosedDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandClosedNearDL,
    gLinkChildLeftFistNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandClosedDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandClosedFarDL,
    gLinkChildLeftFistFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandClosedDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(gPlayerLeftHandClosedDLs, 1);

Gfx* sPlayerLeftHandSwordDLs2[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerLeftHandSwordDLs2, 1);

Gfx* sPlayerLeftHandSwordDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandHoldingMasterSwordNearDL,
    gLinkChildLeftFistAndKokiriSwordNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandHoldingMasterSwordFarDL,
    gLinkChildLeftFistAndKokiriSwordFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandClosedDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerLeftHandSwordDLs, 1);

Gfx* sPlayerRightHandOpenDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandNearDL,
    gLinkChildRightHandNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandFarDL,
    gLinkChildRightHandFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandOpenDLs, 1);

Gfx* sPlayerRightHandClosedDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandClosedNearDL,
    gLinkChildRightHandClosedNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandClosedDL,
    mm_gLinkZoraRightHandClosedDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandClosedFarDL,
    gLinkChildRightHandClosedFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandClosedDL,
    mm_gLinkZoraRightHandClosedDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandClosedDLs, 1);

Gfx* sPlayerRightHandBowSlingshotDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandBowSlingshotDLs, 1);

Gfx* sSwordAndSheathDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultMasterSwordAndSheathNearDL,
    gLinkChildSwordAndSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultMasterSwordAndSheathFarDL,
    gLinkChildSwordAndSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sSwordAndSheathDLs, 1);

Gfx* sSheathDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultSheathNearDL,
    gLinkChildSheathNearDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
    gLinkAdultSheathFarDL,
    gLinkChildSheathFarDL,
    mm_gLinkDekuSheathDL,
    NULL,
    NULL,
    NULL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sSheathDLs, 1);

Gfx* sPlayerWaistDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultWaistNearDL,
    gLinkChildWaistNearDL,
    mm_gLinkDekuWaistDL,
    mm_gLinkGoronWaistDL,
    mm_gLinkZoraWaistDL,
    mm_gLinkFierceDeityWaistDL,
    gLinkAdultWaistFarDL,
    gLinkChildWaistFarDL,
    mm_gLinkDekuWaistDL,
    mm_gLinkGoronWaistDL,
    mm_gLinkZoraWaistDL,
    mm_gLinkFierceDeityWaistDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerWaistDLs, 1);

Gfx* sPlayerRightHandBowSlingshotDLs2[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandHoldingBowNearDL,
    gLinkChildRightHandHoldingSlingshotNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingBowFarDL,
    gLinkChildRightHandHoldingSlingshotFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandBowSlingshotDLs2, 1);

Gfx* sPlayerRightHandOcarinaDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandHoldingFairyOcarinaNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingFairyOcarinaFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandOcarinaDLs, 1);

Gfx* sPlayerRightHandOotDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandHoldingOotNearDL,
    gLinkChildRightHandAndOotNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingOotFarDL,
    gLinkChildRightHandHoldingOOTFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandOotDLs, 1);

Gfx* sPlayerRightHandHookshotDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultRightHandHoldingHookshotNearDL,
    gLinkChildRightHandNearDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
    gLinkAdultRightHandHoldingHookshotNearDL,
    gLinkChildRightHandFarDL,
    mm_gLinkDekuRightHandDL,
    mm_gLinkGoronRightHandOpenDL,
    mm_gLinkZoraRightHandOpenDL,
    mm_gLinkFierceDeityRightHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerRightHandHookshotDLs, 1);

Gfx* sPlayerLeftHandHammerDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandHoldingHammerNearDL,
    gLinkChildLeftHandNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandHoldingHammerFarDL,
    gLinkChildLeftHandFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerLeftHandHammerDLs, 1);

Gfx* gPlayerLeftHandBoomerangDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandNearDL,
    gLinkChildLeftFistAndBoomerangNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandFarDL,
    gLinkChildLeftFistAndBoomerangFarDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandOpenDL,
    mm_gLinkZoraLeftHandOpenDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(gPlayerLeftHandBoomerangDLs, 1);

Gfx* sPlayerLeftHandBottleDLs[2 * PLAYER_FORM_MAX] = {
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandHoldBottleDL,
    mm_gLinkZoraLeftHandHoldBottleDL,
    mm_gLinkFierceDeityLeftHandDL,
    gLinkAdultLeftHandOutNearDL,
    gLinkChildLeftHandUpNearDL,
    mm_gLinkDekuLeftHandDL,
    mm_gLinkGoronLeftHandHoldBottleDL,
    mm_gLinkZoraLeftHandHoldBottleDL,
    mm_gLinkFierceDeityLeftHandDL,
};
MERGESHIP_ASSERT_LIMB_TABLE(sPlayerLeftHandBottleDLs, 1);

Gfx* sFirstPersonLeftForearmDLs[] = {
    gLinkAdultRightArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonLeftHandDLs[] = {
    gLinkAdultRightHandOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightShoulderDLs[] = {
    gLinkAdultRightShoulderNearDL,
    gLinkChildRightShoulderNearDL,
};

Gfx* sFirstPersonForearmDLs[] = {
    gLinkAdultLeftArmOutNearDL,
    NULL,
};

Gfx* sFirstPersonRightHandHoldingWeaponDLs[] = {
    gLinkAdultRightHandHoldingBowFirstPersonDL,
    gLinkChildRightArmStretchedSlingshotDL,
};

// Indexed by model types (left hand, right hand, sheath or waist)
Gfx** sPlayerDListGroups[PLAYER_MODELTYPE_MAX] = {
    gPlayerLeftHandOpenDLs,           // PLAYER_MODELTYPE_LH_OPEN
    gPlayerLeftHandClosedDLs,         // PLAYER_MODELTYPE_LH_CLOSED
    sPlayerLeftHandSwordDLs,          // PLAYER_MODELTYPE_LH_SWORD
    sPlayerLeftHandSwordDLs2,         // PLAYER_MODELTYPE_LH_SWORD_2
    gPlayerLeftHandBgsDLs,            // PLAYER_MODELTYPE_LH_BGS
    sPlayerLeftHandHammerDLs,         // PLAYER_MODELTYPE_LH_HAMMER
    gPlayerLeftHandBoomerangDLs,      // PLAYER_MODELTYPE_LH_BOOMERANG
    sPlayerLeftHandBottleDLs,         // PLAYER_MODELTYPE_LH_BOTTLE
    sPlayerRightHandOpenDLs,          // PLAYER_MODELTYPE_RH_OPEN
    sPlayerRightHandClosedDLs,        // PLAYER_MODELTYPE_RH_CLOSED
    sPlayerRightHandShieldDLs,        // PLAYER_MODELTYPE_RH_SHIELD
    sPlayerRightHandBowSlingshotDLs,  // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT
    sPlayerRightHandBowSlingshotDLs2, // PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2
    sPlayerRightHandOcarinaDLs,       // PLAYER_MODELTYPE_RH_OCARINA
    sPlayerRightHandOotDLs,           // PLAYER_MODELTYPE_RH_OOT
    sPlayerRightHandHookshotDLs,      // PLAYER_MODELTYPE_RH_HOOKSHOT
    sSwordAndSheathDLs,               // PLAYER_MODELTYPE_SHEATH_16
    sSheathDLs,                       // PLAYER_MODELTYPE_SHEATH_17
    sSheathWithSwordDLs,              // PLAYER_MODELTYPE_SHEATH_18
    sSheathWithoutSwordDLs,           // PLAYER_MODELTYPE_SHEATH_19
    sPlayerWaistDLs,                  // PLAYER_MODELTYPE_WAIST
};

Gfx gCullBackDList[] = {
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};

Gfx gCullFrontDList[] = {
    gsSPSetGeometryMode(G_CULL_FRONT),
    gsSPEndDisplayList(),
};

Vec3f* D_80160000;
s32 sDListsLodOffset;
Vec3f sGetItemRefPos;
s32 sLeftHandType;
s32 sRightHandType;

// MergeShip2 -- MM D_801BFE14: each form's movement registers, MM's own values and layout (18 per row:
// MM adds REG(39) after REG(38), which MM's walking code reads).
static s16 sMMBootData[MM_BOOTS_MAX][18] = {
    { 200, 666, 200, 700, 366, 200, 600, 175, 60, 800, 1000, -100, 600, 590, 800, 125, 300, 65 },   // FIERCE_DEITY
    { 200, 1000, 300, 800, 500, 400, 800, 400, 120, 800, 550, -100, 600, 540, 750, 125, 400, 200 }, // HYLIAN
    { 100, 1000, 300, 800, 250, 200, 800, 200, 90, 800, 350, -80, 600, 540, 750, 60, 200, 200 },    // GIANT
    { 200, 1000, 300, 700, 550, 270, 600, 1000, 120, 800, 600, -100, 600, 590, 750, 125, 200, 130 }, // DEKU
    { 200, 1000, 300, 700, 550, 270, 700, 300, 120, 800, 600, -100, 600, 590, 750, 125, 200, 130 }, // ZORA_LAND
    { 200, 1000, 300, 700, 550, 270, 700, 300, 120, 800, 600, -100, 600, 590, 750, 125, 200, 130 }, // ZORA_UNDERWATER
    { 200, 1000, 300, 700, 550, 270, 700, 200, 120, 800, 600, -140, 600, 590, 750, 125, 200, 130 }, // GORON
    { 80, 800, 150, 700, 480, 270, 600, 50, 120, 800, 300, -40, 400, 540, 270, 25, 0, 80 },         // 7
};

// MM D_801BFF90: the boots each form wears.
static u8 sMMFormBoots[MM_FORM_MAX] = {
    MM_BOOTS_FIERCE_DEITY, // MM_FORM_FIERCE_DEITY
    MM_BOOTS_GORON,        // MM_FORM_GORON
    MM_BOOTS_ZORA_LAND,    // MM_FORM_ZORA
    MM_BOOTS_DEKU,         // MM_FORM_DEKU
    MM_BOOTS_HYLIAN,       // MM_FORM_HUMAN
};

MergeShipPlayerExt gMergeShipPlayerExt;

// MM func_80123140 ("OoT's Player_SetBootData").
void MergeShip_SetFormBootData(PlayState* play, Player* player) {
    s16* bootRegs;
    s32 currentBoots;
    f32 scale;

    if ((player->actor.id == ACTOR_PLAYER) && (player->mergeShipForm == PLAYER_FORM_FIERCE_DEITY)) {
        REG(27) = 1200;
    } else {
        REG(27) = 2000;
    }

    REG(48) = 370;

    currentBoots = gMergeShipPlayerExt.mmBoots;
    if (currentBoots >= MM_BOOTS_ZORA_UNDERWATER) {
        if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
            currentBoots++;
        }
        if (player->mergeShipForm == PLAYER_FORM_GORON) {
            REG(48) = 200;
        }
    } else if (currentBoots == MM_BOOTS_GIANT) {
        REG(48) = 170;
    }

    bootRegs = sMMBootData[currentBoots];
    REG(19) = bootRegs[0];
    REG(30) = bootRegs[1];
    REG(32) = bootRegs[2];
    REG(34) = bootRegs[3];
    REG(35) = bootRegs[4];
    REG(36) = bootRegs[5];
    REG(37) = bootRegs[6];
    REG(38) = bootRegs[7];
    REG(39) = bootRegs[8];
    REG(43) = bootRegs[9];  // MM R_DECELERATE_RATE
    REG(45) = bootRegs[10]; // MM R_RUN_SPEED_LIMIT
    REG(68) = bootRegs[11]; // gravity
    REG(69) = bootRegs[12];
    IREG(66) = bootRegs[13];
    IREG(67) = bootRegs[14];
    IREG(68) = bootRegs[15];
    IREG(69) = bootRegs[16];
    MREG(95) = bootRegs[17];

    // MM: `if (play->roomCtx.curRoom.type == ROOM_TYPE_INDOORS)`; OoT's indoor rooms are
    // ROOM_BEHAVIOR_TYPE1_2, which OoT's own Player_SetBootData tests for the same cap.
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }

    if ((player->actor.id == ACTOR_PLAYER) && (player->mergeShipForm == PLAYER_FORM_FIERCE_DEITY)) {
        scale = 0.015f;
    } else {
        scale = 0.01f;
    }

    Actor_SetScale(&player->actor, scale);
}

// MergeShip2: MM func_80123140 with PLAYER_BOOTS_GIANT, defined in MergeShipGiantsMask.inc (z_player.c).
s32 MergeShip_GiantsMask_SetBootData(PlayState* play, Player* this);

void Player_SetBootData(PlayState* play, Player* this) {
    s32 currentBoots;
    s16* bootRegs;

    // MergeShip2: MM func_80123140 -- the Giant's Mask moves Link by MM's PLAYER_BOOTS_GIANT row.
    if (MergeShip_GiantsMask_SetBootData(play, this)) {
        return;
    }

    // MergeShip2: MM's forms move by MM's table.
    if (MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        MergeShip_SetFormBootData(play, this);
        return;
    }

    REG(27) = 2000;
    REG(48) = 370;

    currentBoots = this->currentBoots;
    if (currentBoots == PLAYER_BOOTS_KOKIRI) {
        if (!LINK_IS_ADULT) {
            currentBoots = PLAYER_BOOTS_KOKIRI_CHILD;
        }
    } else if (currentBoots == PLAYER_BOOTS_IRON) {
        if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) {
            currentBoots = PLAYER_BOOTS_IRON_UNDERWATER;
        }
        REG(27) = 500;
        REG(48) = 100;
    }

    bootRegs = sBootData[currentBoots];
    REG(19) = bootRegs[0];
    REG(30) = bootRegs[1];
    REG(32) = bootRegs[2];
    REG(34) = bootRegs[3];
    REG(35) = bootRegs[4];
    REG(36) = bootRegs[5];
    REG(37) = bootRegs[6];
    REG(38) = bootRegs[7];
    REG(43) = bootRegs[8];
    REG(45) = bootRegs[9];
    REG(68) = bootRegs[10];
    REG(69) = bootRegs[11];
    IREG(66) = bootRegs[12];
    IREG(67) = bootRegs[13];
    IREG(68) = bootRegs[14];
    IREG(69) = bootRegs[15];
    MREG(95) = bootRegs[16];

    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) {
        REG(45) = 500;
    }
}

// SoH: Custom method used to determine if we're using a custom model for link
uint8_t Player_IsCustomLinkModel() {
    return (LINK_IS_ADULT && ResourceGetIsCustomByName(gLinkAdultSkel)) ||
           (LINK_IS_CHILD && ResourceGetIsCustomByName(gLinkChildSkel));
}

s32 Player_InBlockingCsMode(PlayState* play, Player* this) {
    return (this->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_IN_CUTSCENE)) || (this->csAction != 0) ||
           (play->transitionTrigger == TRANS_TRIGGER_START) || (this->stateFlags1 & PLAYER_STATE1_LOADING) ||
           (this->stateFlags3 & PLAYER_STATE3_FLYING_WITH_HOOKSHOT) ||
           ((gSaveContext.magicState != MAGIC_STATE_IDLE) && (Player_ActionToMagicSpell(this, this->itemAction) >= 0));
}

s32 Player_InCsMode(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return Player_InBlockingCsMode(play, this) || (this->unk_6AD == 4);
}

/**
 * Checks if Player is currently locked onto a hostile actor.
 * `PLAYER_STATE1_HOSTILE_LOCK_ON` controls Player's "battle" response to hostile actors.
 *
 * Note that within Player, `Player_UpdateHostileLockOn` exists, which updates the flag and also returns the check.
 * Player can use this function instead if the flag should be checked, but not updated.
 */
s32 Player_CheckHostileLockOn(Player* this) {
    return (this->stateFlags1 & PLAYER_STATE1_HOSTILE_LOCK_ON);
}

s32 Player_IsChildWithHylianShield(Player* this) {
    // MergeShip2: MM's human holds the Hero's Shield in front (the Hylian slot's MM variant, MergeShipMMEquipment.h).
    return gSaveContext.linkAge != 0 && (this->currentShield == PLAYER_SHIELD_HYLIAN) &&
           !MergeShip_MMEquip_ChildHoldsHeroShield();
}

s32 Player_ActionToModelGroup(Player* this, s32 itemAction) {
    s32 modelGroup = sActionModelGroups[itemAction];

    // MergeShip2: MM Player_ActionToModelGroup -- a Goron "holding" its B item holds its fists
    // (MM PLAYER_MODELGROUP_1, Ocarina of Time's CHILD_HYLIAN_SHIELD row; the Goron rows draw no sword).
    // (MM Player_IsGoronOrDeku: the Deku too.)
    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) &&
        ((this->mergeShipForm == PLAYER_FORM_GORON) || (this->mergeShipForm == PLAYER_FORM_DEKU))) {
        return PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    }

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && Player_IsChildWithHylianShield(this)) {
        // child, using kokiri sword with hylian shield equipped
        return PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    } else {
        return modelGroup;
    }
}

// MergeShip2 (BUG-0093/0094): adult Link with the Deku Shield (MM's timeless gear) is dressed as SoH's
// EquipmentAlwaysVisible dresses him -- the child's Deku Shield on his back and in his fist -- with that option off too.
#define MERGESHIP_EQUIPMENT_VISIBLE(player)                           \
    (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) || \
     (LINK_IS_ADULT && ((player)->currentShield == PLAYER_SHIELD_DEKU)))

void Player_SetModelsForHoldingShield(Player* this) {
    // MergeShip2: MM Player_SetModelsForHoldingShield -- the Goron and the Deku guard without a shield model
    // (MM Player_IsGoronOrDeku).
    if ((this->stateFlags1 & PLAYER_STATE1_SHIELDING) && (this->mergeShipForm != PLAYER_FORM_GORON) &&
        (this->mergeShipForm != PLAYER_FORM_DEKU) &&
        ((this->itemAction < 0) || (this->itemAction == this->heldItemAction))) {
        if ((CVarGetInteger(CVAR_CHEAT("ShieldTwoHanded"), 0) && (this->heldItemAction != PLAYER_IA_DEKU_STICK) ||
             !Player_HoldsTwoHandedWeapon(this)) &&
            !Player_IsChildWithHylianShield(this)) {
            this->rightHandType = PLAYER_MODELTYPE_RH_SHIELD;
            if (LINK_IS_CHILD && (MERGESHIP_EQUIPMENT_VISIBLE(this)) &&
                (this->currentShield == PLAYER_SHIELD_MIRROR)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][0];
            } else if (LINK_IS_ADULT && (MERGESHIP_EQUIPMENT_VISIBLE(this)) &&
                       (this->currentShield == PLAYER_SHIELD_DEKU)) {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][1];
            } else {
                this->rightHandDLists = &sPlayerDListGroups[PLAYER_MODELTYPE_RH_SHIELD][this->mergeShipForm];
            }
            if (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_16;
            } else if (this->sheathType == PLAYER_MODELTYPE_SHEATH_19) {
                this->sheathType = PLAYER_MODELTYPE_SHEATH_17;
            }
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][this->mergeShipForm];
            if (MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
                // MergeShip2: EquipmentAlwaysVisible dresses Link; a form keeps its own column.
            } else if ((MERGESHIP_EQUIPMENT_VISIBLE(this)) && LINK_IS_CHILD &&
                       gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
            } else if ((MERGESHIP_EQUIPMENT_VISIBLE(this)) && LINK_IS_ADULT &&
                       gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI) {
                this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
            }
            this->modelAnimType = PLAYER_ANIMTYPE_2;
            this->itemAction = -1;
        }
    }
}

void Player_SetModels(Player* this, s32 modelGroup) {
    // Left hand
    this->leftHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
    this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][this->mergeShipForm];

    // MergeShip2: EquipmentAlwaysVisible swaps in adult/child Link's models; an MM form keeps its own column.
    if (MERGESHIP_EQUIPMENT_VISIBLE(this) && !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        if (LINK_IS_CHILD &&
            (this->leftHandType == PLAYER_MODELTYPE_LH_HAMMER ||
             ((this->leftHandType == PLAYER_MODELTYPE_LH_SWORD || this->leftHandType == PLAYER_MODELTYPE_LH_BGS) &&
              (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][0];
        }

        if (LINK_IS_ADULT && (this->leftHandType == PLAYER_MODELTYPE_LH_BOOMERANG ||
                              (this->leftHandType == PLAYER_MODELTYPE_LH_SWORD &&
                               gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI))) {
            this->leftHandDLists = &sPlayerDListGroups[this->leftHandType][1];
        }
    }

    // Right hand
    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][this->mergeShipForm];

    this->rightHandType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
    this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][this->mergeShipForm];

    if (MERGESHIP_EQUIPMENT_VISIBLE(this) && !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        if (LINK_IS_CHILD &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
             (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_MIRROR))) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][0];
        }
        if (LINK_IS_ADULT &&
            (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][1];
        }
    }
    if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
         MERGESHIP_EQUIPMENT_VISIBLE(this)) &&
        this->rightHandType == 11 && !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) { // If holding Bow/Slingshot
        this->rightHandDLists = &sPlayerDListGroups[this->rightHandType][Player_HoldsSlingshot(this)];
    }

    // Sheath
    this->sheathType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
    this->sheathDLists = &sPlayerDListGroups[this->sheathType][this->mergeShipForm];

    if (MERGESHIP_EQUIPMENT_VISIBLE(this) && !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
        if (LINK_IS_CHILD && (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                                  ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS)) ||
                              (this->currentShield == PLAYER_SHIELD_MIRROR) &&
                                  (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_CHILD && this->currentShield == PLAYER_SHIELD_MIRROR &&
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_KOKIRI &&
                   this->sheathType == PLAYER_MODELTYPE_SHEATH_18) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        } else if (LINK_IS_ADULT && (this->currentShield == PLAYER_SHIELD_DEKU &&
                                     gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER) ||
                   (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER &&
                    this->sheathType == PLAYER_MODELTYPE_SHEATH_18 && this->currentShield == PLAYER_SHIELD_DEKU)) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][1];
        } else if (LINK_IS_CHILD && this->sheathType == PLAYER_MODELTYPE_SHEATH_17 &&
                   ((gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER) ||
                    (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) {
            this->sheathDLists = &sPlayerDListGroups[this->sheathType][0];
        }
    }

    // Waist
    this->waistDLists = &sPlayerDListGroups[gPlayerModelTypes[modelGroup][4]][this->mergeShipForm];

    Player_SetModelsForHoldingShield(this);
    GameInteractor_ExecuteOnPlayerSetModels(this, modelGroup);
}

void Player_SetModelGroup(Player* this, s32 modelGroup) {
    this->modelGroup = modelGroup;

    if (modelGroup == PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    } else {
        this->modelAnimType = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_ANIM];
    }

    // MergeShip2: MM PLAYER_MODELGROUP_ZORA_BOOMERANG (the Zora's fins, group 10 here) animates as DEFAULT.
    if ((this->mergeShipForm == PLAYER_FORM_ZORA) && (modelGroup == PLAYER_MODELGROUP_10)) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    if ((this->modelAnimType < PLAYER_ANIMTYPE_3) && (this->currentShield == PLAYER_SHIELD_NONE)) {
        this->modelAnimType = PLAYER_ANIMTYPE_0;
    }

    Player_SetModels(this, modelGroup);
}

void func_8008EC70(Player* this) {
    this->itemAction = this->heldItemAction;
    Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
    this->unk_6AD = 0;
}

void Player_SetEquipmentData(PlayState* play, Player* this) {
    // MergeShip2: MM Player_SetEquipmentData for a form. The form wears no Hylian gear and counts as wearing
    // the Kokiri Tunic (the Goron and Zora Tunics are greyed out while transformed, MergeShipItems.c). The
    // Zora's breathing underwater is its own, not a tunic's (z_parameter.c's hazard check).
    if (MERGESHIP_FORM_IS_MM(this->mergeShipForm) && (this->csAction != 0x56)) {
        u8 mmForm = MergeShip_FormToMM(this->mergeShipForm);

        this->currentShield = PLAYER_SHIELD_NONE;
        this->currentTunic = PLAYER_TUNIC_KOKIRI;
        this->currentBoots = PLAYER_BOOTS_KOKIRI;
        if ((this->mergeShipForm != PLAYER_FORM_ZORA) || ((gMergeShipPlayerExt.mmBoots != MM_BOOTS_ZORA_LAND) &&
                                                           (gMergeShipPlayerExt.mmBoots != MM_BOOTS_ZORA_UNDERWATER))) {
            gMergeShipPlayerExt.mmBoots = sMMFormBoots[mmForm];
        }
        this->currentSwordItemId = B_BTN_ITEM;
        Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
        Player_SetBootData(play, this);
        return;
    }
    if (this->csAction != 0x56) {
        this->currentShield = SHIELD_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        this->currentTunic = TUNIC_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC));
        this->currentBoots = BOOTS_EQUIP_TO_PLAYER(CUR_EQUIP_VALUE(EQUIP_TYPE_BOOTS));
        this->currentSwordItemId = B_BTN_ITEM;
        Player_SetModelGroup(this, Player_ActionToModelGroup(this, this->heldItemAction));
        Player_SetBootData(play, this);
    }
}

void Player_UpdateBottleHeld(PlayState* play, Player* this, s32 item, s32 itemAction) {
    Inventory_UpdateBottleItem(play, item, this->heldItemButton);

    if (item != ITEM_BOTTLE) {
        this->heldItemId = item;
        this->heldItemAction = itemAction;
    }

    if (GameInteractor_Should(VB_PLAYER_UPDATE_BOTTLE_HELD, true, this)) {
        this->itemAction = itemAction;
    }
}

void Player_ReleaseLockOn(Player* this) {
    this->focusActor = NULL;
    this->stateFlags2 &= ~PLAYER_STATE2_LOCK_ON_WITH_SWITCH;
}

/**
 * This function aims to clear Z-Target related state when it isn't in use.
 * It also handles setting a specific free fall related state that is interntwined with Z-Targeting.
 * TODO: Learn more about this and give a name to PLAYER_STATE1_19
 */
void Player_ClearZTargeting(Player* this) {
    if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        (this->stateFlags1 & (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_IN_WATER)) ||
        (!(this->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL)) &&
         ((this->actor.world.pos.y - this->actor.floorHeight) < 100.0f))) {
        this->stateFlags1 &=
            ~(PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_PARALLEL |
              PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    } else if (!(this->stateFlags1 &
                 (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_CLIMBING_LADDER))) {
        this->stateFlags1 |= PLAYER_STATE1_FREEFALL;
    } else if ((this->stateFlags1 & PLAYER_STATE1_JUMPING) && (this->mergeShipForm == PLAYER_FORM_DEKU)) {
        // MergeShip2: MM Player_ClearZTargeting -- a jumping Deku drops its Z-target.
        this->stateFlags1 &= ~(PLAYER_STATE1_Z_TARGETING | PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS |
                               PLAYER_STATE1_PARALLEL | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
    }

    Player_ReleaseLockOn(this);
}

/**
 * Sets the "auto lock-on actor" to lock onto an actor without Player's input.
 * This function will first release any existing lock-on or (try to) release parallel.
 *
 * When using Switch Targeting, it is not possible to carry an auto lock-on actor into a normal
 * lock-on when the auto lock-on is finished.
 * This is because the `PLAYER_STATE2_LOCK_ON_WITH_SWITCH` flag is never set with an auto lock-on.
 * With Hold Targeting it is possible to keep the auto lock-on going by keeping the Z button held down.
 *
 * The auto lock-on is considered "friendly" even if the actor is actually hostile. If the auto lock-on is hostile,
 * Player's battle response will not occur (if he is actionable) and the camera behaves differently.
 * When transitioning from auto lock-on to normal lock-on (with Hold Targeting) there will be a noticeable change
 * when it switches from "friendly" mode to "hostile" mode.
 */
void Player_SetAutoLockOnActor(PlayState* play, Actor* actor) {
    Player* this = GET_PLAYER(play);

    Player_ClearZTargeting(this);
    this->focusActor = actor;
    this->autoLockOnActor = actor;
    this->stateFlags1 |= PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS;
    Camera_SetParam(Play_GetCamera(play, CAM_ID_MAIN), 8, actor);
    Camera_RequestMode(Play_GetCamera(play, CAM_ID_MAIN), 2);
}

s32 func_8008EF30(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->stateFlags1 & PLAYER_STATE1_ON_HORSE);
}

s32 func_8008EF44(PlayState* play, s32 ammo) {
    play->shootingGalleryStatus = ammo + 1;
    return 1;
}

s32 Player_IsBurningStickInRange(PlayState* play, Vec3f* pos, f32 xzRange, f32 yRange) {
    Player* this = GET_PLAYER(play);
    Vec3f diff;
    s32 pad;

    if ((this->heldItemAction == PLAYER_IA_DEKU_STICK) && (this->unk_860 != 0)) {
        Math_Vec3f_Diff(&this->meleeWeaponInfo[0].tip, pos, &diff);
        return ((SQ(diff.x) + SQ(diff.z)) <= SQ(xzRange)) && (0.0f <= diff.y) && (diff.y <= yRange);
    } else {
        return false;
    }
}

s32 Player_GetStrength(void) {
    s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

    // MergeShip2: MM sPlayerStrengths -- the Goron's strength is its own.
    if (MergeShip_GetPlayerForm() == PLAYER_FORM_GORON) {
        return MergeShip_Goron_GetStrength();
    }
    // MergeShip2 (soh_fd): "FD Increased Strength" -- the Fierce Deity lifts what the Gold Gauntlets lift.
    if ((MergeShip_GetPlayerForm() == PLAYER_FORM_FIERCE_DEITY) && MergeShip_SohFd_FdIncreasedStrength()) {
        return PLAYER_STR_GOLD_G;
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("ToggleStrength"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("StrengthDisabled"), 0)) {
        return PLAYER_STR_NONE;
    }

    if (GameInteractor_Should(VB_PLAYER_MEETS_AGE_REQ, LINK_IS_ADULT, LINK_AGE_ADULT)) {
        return strengthUpgrade;
    } else if (strengthUpgrade != 0) {
        return PLAYER_STR_BRACELET;
    } else {
        return PLAYER_STR_NONE;
    }
}

u8 Player_GetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    // MergeShip2: a transformed player's currentMask is the form's MM mask. Ocarina of Time's actors
    // size their mask tables by PLAYER_MASK_MAX and have no reactions for these, so to them the player
    // wears no mask -- MergeShip2 (soh_fd): unless a Hyrule mask is worn over the form ("Forms Wear Trade Masks").
    if (this->currentMask >= PLAYER_MASK_MAX) {
        return MERGESHIP_FORM_IS_MM(this->mergeShipForm) ? MergeShip_SohFd_FormTradeMask() : PLAYER_MASK_NONE;
    }
    return this->currentMask;
}

Player* Player_UnsetMask(PlayState* play) {
    Player* this = GET_PLAYER(play);

    this->currentMask = PLAYER_MASK_NONE;

    return this;
}

s32 Player_HasMirrorShieldEquipped(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->currentShield == PLAYER_SHIELD_MIRROR);
}

s32 Player_HasMirrorShieldSetToDraw(PlayState* play) {
    Player* this = GET_PLAYER(play);

    return (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) && (this->currentShield == PLAYER_SHIELD_MIRROR);
}

s32 Player_ActionToMagicSpell(Player* this, s32 itemAction) {
    s32 magicSpell = itemAction - PLAYER_IA_MAGIC_SPELL_15;

    if ((magicSpell >= 0) && (magicSpell < 6)) {
        return magicSpell;
    } else {
        return -1;
    }
}

s32 Player_HoldsHookshot(Player* this) {
    return (this->heldItemAction == PLAYER_IA_HOOKSHOT) || (this->heldItemAction == PLAYER_IA_LONGSHOT);
}

// SoH
s32 Player_HoldsBow(Player* this) {
    switch (this->heldItemAction) {
        case PLAYER_IA_BOW:
        case PLAYER_IA_BOW_FIRE:
        case PLAYER_IA_BOW_ICE:
        case PLAYER_IA_BOW_LIGHT:
            return true;
        default:
            return false;
    }
}

// SoH
s32 Player_HoldsSlingshot(Player* this) {
    return this->heldItemAction == PLAYER_IA_SLINGSHOT;
}

s32 func_8008F128(Player* this) {
    return Player_HoldsHookshot(this) && (this->heldActor == NULL);
}

s32 Player_ActionToMeleeWeapon(s32 itemAction) {
    s32 sword = itemAction - PLAYER_IA_FISHING_POLE;

    // MergeShip2: MM Player_MeleeWeaponFromIA -- Fierce Deity's sword is MM's
    // PLAYER_MELEEWEAPON_SWORD_TWO_HANDED, its own entry in the tables indexed by this.
    if (itemAction == PLAYER_IA_MM_SWORD_TWO_HANDED) {
        return MERGESHIP_MELEEWEAPON_MM_TWO_HANDED;
    }
    // MergeShip2: MM's Razor, Gilded and Great Fairy's (two-handed) Swords (MergeShipMMEquipment.h).
    if (itemAction == PLAYER_IA_MM_SWORD_RAZOR) {
        return MERGESHIP_MELEEWEAPON_MM_RAZOR;
    }
    if (itemAction == PLAYER_IA_MM_SWORD_GILDED) {
        return MERGESHIP_MELEEWEAPON_MM_GILDED;
    }
    if (itemAction == PLAYER_IA_MM_SWORD_GREAT_FAIRY) {
        return MERGESHIP_MELEEWEAPON_MM_TWO_HANDED;
    }

    if ((sword > 0) && (sword < 6)) {
        return sword;
    } else {
        return 0;
    }
}

s32 Player_GetMeleeWeaponHeld(Player* this) {
    return Player_ActionToMeleeWeapon(this->heldItemAction);
}

s32 Player_HoldsTwoHandedWeapon(Player* this) {
    if (((this->heldItemAction >= PLAYER_IA_SWORD_BIGGORON) && (this->heldItemAction <= PLAYER_IA_HAMMER)) ||
        (this->heldItemAction == PLAYER_IA_MM_SWORD_TWO_HANDED) || // MergeShip2: MM Player_IsHoldingTwoHandedWeapon
        (this->heldItemAction == PLAYER_IA_MM_SWORD_GREAT_FAIRY)) { // (the Great Fairy's Sword is MM's too)
        return true;
    } else {
        return false;
    }
}

s32 Player_HoldsBrokenKnife(Player* this) {
    return (this->heldItemAction == PLAYER_IA_SWORD_BIGGORON) && (gSaveContext.swordHealth <= 0.0f);
}

s32 Player_ActionToBottle(Player* this, s32 itemAction) {
    s32 bottle = itemAction - PLAYER_IA_BOTTLE;

    if ((bottle >= 0) && (bottle < 13)) {
        return bottle;
    } else if (itemAction == PLAYER_IA_MM_BOTTLE_CHATEAU) {
        // MergeShip2: MM GET_BOTTLE_FROM_IA(PLAYER_IA_BOTTLE_CHATEAU) -- past Ocarina of Time's thirteen bottles.
        return PLAYER_BOTTLE_MM_CHATEAU;
    } else if (MergeShip_MMItems_ActionToBottle(itemAction) >= 0) {
        // MergeShip2: MM's other bottle contents (PLAYER_BOTTLE_MM_FIRST.., MergeShipMMItems.h).
        return MergeShip_MMItems_ActionToBottle(itemAction);
    } else {
        return -1;
    }
}

s32 Player_GetBottleHeld(Player* this) {
    return Player_ActionToBottle(this, this->heldItemAction);
}

s32 Player_ActionToExplosive(Player* this, s32 itemAction) {
    s32 explosive = itemAction - PLAYER_IA_BOMB;

    if ((explosive >= 0) && (explosive < 2)) {
        return explosive;
    } else if (itemAction == PLAYER_IA_MM_POWDER_KEG) {
        // MergeShip2: MM Player_ExplosiveFromIA -- the Powder Keg (PLAYER_EXPLOSIVE_POWDER_KEG).
        return MERGESHIP_PLAYER_EXPLOSIVE_POWDER_KEG;
    } else {
        return -1;
    }
}

s32 Player_GetExplosiveHeld(Player* this) {
    return Player_ActionToExplosive(this, this->heldItemAction);
}

s32 func_8008F2BC(Player* this, s32 itemAction) {
    s32 sword = 0;

    // MergeShip2: MM's swords are swords too (drawn and put away with the sword sounds). So is the Fierce Deity's
    // (MM's GET_SWORD_FROM_IA covers PLAYER_IA_SWORD_TWO_HANDED), which "FD Can Sheathe Sword" puts away and draws.
    if (MERGESHIP_IA_IS_MM_SWORD(itemAction) || (itemAction == PLAYER_IA_MM_SWORD_TWO_HANDED)) {
        return 0;
    }
    if (itemAction != PLAYER_IA_SWORD_CS) {
        sword = itemAction - PLAYER_IA_SWORD_MASTER;
        if ((sword < 0) || (sword >= 3)) {
            goto return_neg;
        }
    }

    return sword;

return_neg:
    return -1;
}

s32 Player_GetEnvironmentalHazard(PlayState* play) {
    Player* this = GET_PLAYER(play);
    TextTriggerEntry* triggerEntry;
    s32 envHazard;

    if (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3) { // Room is hot
        envHazard = PLAYER_ENV_HAZARD_HOTROOM - 1;
    } else if ((this->underwaterTimer > 80) &&
               ((this->currentBoots == PLAYER_BOOTS_IRON) || (this->underwaterTimer >= 300))) { // Deep underwater
        envHazard = ((this->currentBoots == PLAYER_BOOTS_IRON) && (this->actor.bgCheckFlags & BGCHECKFLAG_GROUND))
                        ? (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)
                        : (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1);
    } else if (this->stateFlags1 & PLAYER_STATE1_IN_WATER) { // Swimming
        envHazard = PLAYER_ENV_HAZARD_SWIMMING - 1;
    } else {
        return PLAYER_ENV_HAZARD_NONE;
    }

    // Trigger general textboxes under certain conditions, like "It's so hot in here!"
    if (!Player_InCsMode(play)) {
        triggerEntry = &sTextTriggers[envHazard];

        if ((triggerEntry->flag != 0) && !(gSaveContext.textTriggerFlags & triggerEntry->flag) &&
            (((envHazard == (PLAYER_ENV_HAZARD_HOTROOM - 1)) &&
              (this->currentTunic != PLAYER_TUNIC_GORON && (this->mergeShipForm != PLAYER_FORM_GORON) &&
               CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0 &&
               CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)) ||
             (((envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FLOOR - 1)) ||
               (envHazard == (PLAYER_ENV_HAZARD_UNDERWATER_FREE - 1))) &&
              (this->currentBoots == PLAYER_BOOTS_IRON) &&
              (this->currentTunic != PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_CHEAT("SuperTunic"), 0) == 0 &&
               CVarGetInteger(CVAR_ENHANCEMENT("DisableTunicWarningText"), 0) == 0)))) {
            Message_StartTextbox(play, triggerEntry->textId, NULL);
            gSaveContext.textTriggerFlags |= triggerEntry->flag;
        }
    }

    return envHazard + 1;
}

u8 sEyeMouthIndexes[][2] = {
    { 0, 0 }, { 1, 0 }, { 2, 0 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { 4, 0 }, { 5, 1 },
    { 7, 2 }, { 0, 2 }, { 3, 0 }, { 4, 0 }, { 2, 2 }, { 1, 1 }, { 0, 2 }, { 0, 0 },
};

/**
 * Link's eye and mouth textures are placed at the exact same place in adult and child Link's respective object files.
 * This allows the array to only contain the symbols for one file and have it apply to both. This is a problem for
 * shiftability, and changes will need to be made in the code to account for this in a modding scenario. The symbols
 * from adult Link's object are used here.
 */

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
// TODO: Formatting
void* sEyeTextures[][8] = {
    { gLinkAdultEyesOpenTex, gLinkAdultEyesHalfTex, gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
      gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex, gLinkAdultEyesUnk2Tex },
    { gLinkChildEyesOpenTex, gLinkChildEyesHalfTex, gLinkChildEyesClosedfTex, gLinkChildEyesRollLeftTex,
      gLinkChildEyesRollRightTex, gLinkChildEyesShockTex, gLinkChildEyesUnk1Tex, gLinkChildEyesUnk2Tex },
    { NULL }, // Deku has fixed eyes in its model.
    { mm_gLinkGoronEyesOpenTex, mm_gLinkGoronEyesHalfTex, mm_gLinkGoronEyesClosedTex,
      mm_gLinkGoronEyesSurprisedTex, NULL, NULL, NULL, NULL },
    { mm_gLinkZoraEyesOpenTex, mm_gLinkZoraEyesHalfTex, mm_gLinkZoraEyesClosedTex,
      mm_gLinkZoraEyesRightTex, mm_gLinkZoraEyesLeftTex, mm_gLinkZoraEyesUpTex,
      mm_gLinkZoraEyesDownTex, mm_gLinkZoraEyesWincingTex },
    { NULL }, // Fierce Deity has fixed eyes in its model.
};
MERGESHIP_ASSERT_FORM_TABLE(sEyeTextures);

#else
void* sEyeTextures[] = {
    gLinkAdultEyesOpenTex,      gLinkAdultEyesHalfTex,  gLinkAdultEyesClosedfTex, gLinkAdultEyesRollLeftTex,
    gLinkAdultEyesRollRightTex, gLinkAdultEyesShockTex, gLinkAdultEyesUnk1Tex,    gLinkAdultEyesUnk2Tex,
};
#endif

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
void* sMouthTextures[][4] = {
    {
        gLinkAdultMouth1Tex,
        gLinkAdultMouth2Tex,
        gLinkAdultMouth3Tex,
        gLinkAdultMouth4Tex,
    },
    {
        gLinkChildMouth1Tex,
        gLinkChildMouth2Tex,
        gLinkChildMouth3Tex,
        gLinkChildMouth4Tex,
    },
    { NULL }, // Deku
    { NULL }, // Goron
    { mm_gLinkZoraMouthClosedTex, mm_gLinkZoraMouthHalfTex, mm_gLinkZoraMouthOpenTex,
      mm_gLinkZoraMouthSmileTex },
    { NULL }, // Fierce Deity
};
MERGESHIP_ASSERT_FORM_TABLE(sMouthTextures);
#else
void* sMouthTextures[] = {
    gLinkAdultMouth1Tex,
    gLinkAdultMouth2Tex,
    gLinkAdultMouth3Tex,
    gLinkAdultMouth4Tex,
};
#endif

Color_RGB8 sTunicColors[] = {
    { 30, 105, 27 },
    { 100, 20, 0 },
    { 0, 60, 100 },
};

Color_RGB8 sGauntletColors[] = {
    { 255, 255, 255 },
    { 254, 207, 15 },
    // #region SOH [RBA] values matching OOB reads on N64
    { 0, 0, 6 },
    { 2, 89, 24 },
    { 6, 2, 90 },
    { 96, 6, 2 },
};

Gfx* sBootDListGroups[][2] = {
    { gLinkAdultLeftIronBootDL, gLinkAdultRightIronBootDL },   // PLAYER_BOOTS_IRON
    { gLinkAdultLeftHoverBootDL, gLinkAdultRightHoverBootDL }, // PLAYER_BOOTS_HOVER
};

#include "soh/MergeShip/MergeShipFormLib.inc" // MergeShip2: MM form drawing details (hands, fists, feet, glow)

// The equipment preview uses a packed equipment array as callback data.
s32 Player_OverrideLimbDrawPause(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg);

void Player_DrawImpl(PlayState* play, void** skeleton, Vec3s* jointTable, s32 dListCount, s32 lod, s32 tunic, s32 boots,
                     s32 face, OverrideLimbDrawOpa overrideLimbDraw, PostLimbDrawOpa postLimbDraw, void* data) {
    Color_RGB8* color;
    // Equipment-screen previews pass a packed equipment value, not a Player.
    // In gameplay, use the actor's applied form, not a pending CVar request.
    u8 form = (data != NULL && overrideLimbDraw != Player_OverrideLimbDrawPause)
                  ? ((Player*)data)->mergeShipForm
                  : MergeShip_SohFd_PauseForm(); // MergeShip2 (soh_fd): the equipment screen shows the form
    if (form >= PLAYER_FORM_MAX) {
        form = gSaveContext.linkAge;
    }
    s32 eyeIndex = (jointTable[22].x & 0xF) - 1;
    s32 mouthIndex = (jointTable[22].x >> 4) - 1;

    OPEN_DISPS(play->state.gfxCtx);

    if (eyeIndex < 0) {
        eyeIndex = sEyeMouthIndexes[face][0];
    }

    if (eyeIndex > 7)
        eyeIndex = 7;

    // Native MM: directional eyes use open; wincing selects Goron's surprised eyes.
    if (form == PLAYER_FORM_GORON) {
        if (eyeIndex >= 3 && eyeIndex <= 6) {
            eyeIndex = 0;
        } else if (eyeIndex == 7) {
            eyeIndex = 3;
        }
    }

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[form][eyeIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sEyeTextures[eyeIndex]));
#endif
    if (mouthIndex < 0) {
        mouthIndex = sEyeMouthIndexes[face][1];
        if (MERGESHIP_FORM_IS_MM(form) || ((form == PLAYER_FORM_CHILD) && gMergeShipMMChildLink)) { // MergeShip2: MM sPlayerFaces (PLAYER_FACE_SMILE smiles)
            mouthIndex = MergeShip_FormLib_FaceMouth(face, mouthIndex);
        }
    }

    if (mouthIndex > 3)
        mouthIndex = 3;

#if defined(MODDING) || defined(_MSC_VER) || defined(__GNUC__)
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[form][mouthIndex]));
#else
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sMouthTextures[eyeIndex]));
#endif

    Color_RGB8 sTemp;
    color = &sTunicColors[tunic];
    if (tunic == PLAYER_TUNIC_KOKIRI && CVarGetInteger(CVAR_COSMETIC("Link.KokiriTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.KokiriTunic.Value"), sTunicColors[PLAYER_TUNIC_KOKIRI]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_GORON && CVarGetInteger(CVAR_COSMETIC("Link.GoronTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.GoronTunic.Value"), sTunicColors[PLAYER_TUNIC_GORON]);
        color = &sTemp;
    } else if (tunic == PLAYER_TUNIC_ZORA && CVarGetInteger(CVAR_COSMETIC("Link.ZoraTunic.Changed"), 0)) {
        sTemp = CVarGetColor24(CVAR_COSMETIC("Link.ZoraTunic.Value"), sTunicColors[PLAYER_TUNIC_ZORA]);
        color = &sTemp;
    }

    // MergeShip2: MM's human (the child) colors its tunic with a primitive color inside its display lists; it wears
    // Ocarina of Time's tunic by having those recolored (MergeShipChildLinkDraw.inc).
    if ((form == PLAYER_FORM_CHILD) && gMergeShipMMChildLink) {
        MergeShip_MMChildLink_SetTunicColor(color);
    }
    if (MERGESHIP_FORM_IS_MM(form) || ((form == PLAYER_FORM_CHILD) && gMergeShipMMChildLink)) {
        sTemp = (Color_RGB8){ 255, 255, 255 };
        color = &sTemp;
        // MergeShip2: MM's models keep a white environment colour; their tunic colours (Anchor's player colour
        // among them) come through MergeShipFormColors.cpp, not VB_APPLY_TUNIC_COLOR's environment colour.
        gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);
    } else if (GameInteractor_Should(VB_APPLY_TUNIC_COLOR, true, data, color)) {
        gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);
    }
    MergeShip_MMEquip_SetTunicEnvColor(color->r, color->g, color->b); // MergeShip2: restored after MM's shields

    // If we have a custom link model, always use the most detailed LOD
    // MergeShip2: a custom model replaces Link's skeleton, never an MM form's (2S2H asks the form's own skeleton).
    if (!MERGESHIP_FORM_IS_MM(form) && Player_IsCustomLinkModel()) {
        lod = 0;
    }

    // MergeShip2: the far LOD sits one whole form-row further in, not two ages further in.
    sDListsLodOffset = lod * PLAYER_FORM_MAX;

#if MERGESHIP_OOT3D
    // MergeShip2: Link's OoT3D body wears Grezzo's painted tunic textures (green, red, blue) in place of the tint.
    gMergeShip3dsTunic = (tunic >= 0 && tunic < 3) ? tunic : -1;
#endif
    // MergeShip2: Ganondorf mode -- the player drawn as Ganondorf on Link's skeleton (MergeShipGanondorf.h); none of
    // Link's own gear below.
    s32 msGanondorf =
        MergeShip_Ganondorf_DrawPlayer(play, skeleton, jointTable, dListCount, overrideLimbDraw, postLimbDraw, data, lod,
                                       eyeIndex, overrideLimbDraw == Player_OverrideLimbDrawGameplayFirstPerson,
                                       overrideLimbDraw == Player_OverrideLimbDrawPause);
    if (!msGanondorf) {
        SkelAnime_DrawFlexLod(play, skeleton, jointTable, dListCount, overrideLimbDraw, postLimbDraw, data, lod);
    }
#if MERGESHIP_OOT3D
    gMergeShip3dsTunic = -1;
#endif

    if (!msGanondorf && !MERGESHIP_FORM_IS_MM(form) && !GameInteractor_InvisibleLinkActive() &&
        ((CVarGetInteger(CVAR_ENHANCEMENT("FirstPersonGauntlets"), 0) && LINK_IS_ADULT) ||
         (overrideLimbDraw != Player_OverrideLimbDrawGameplayFirstPerson)) &&
        (overrideLimbDraw != Player_OverrideLimbDrawGameplayCrawling) &&
        (gSaveContext.gameMode != GAMEMODE_END_CREDITS)) {
        if (LINK_IS_ADULT) {
            s32 strengthUpgrade = CUR_UPG_VALUE(UPG_STRENGTH);

            // MergeShip2 (BUG-0004): the plates come off with the transformation into an MM form; MM's Link
            // transforms bare-armed, and the form he becomes never wears them.
            if ((data != NULL) && (overrideLimbDraw != Player_OverrideLimbDrawPause) &&
                ((Player*)data)->mergeShipTransforming && MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
                strengthUpgrade = 0;
            }

            if (strengthUpgrade >= 2) { // silver or gold gauntlets
                gDPPipeSync(POLY_OPA_DISP++);

                color = &sGauntletColors[strengthUpgrade - 2];
                if (strengthUpgrade == PLAYER_STR_SILVER_G &&
                    CVarGetInteger(CVAR_COSMETIC("Gloves.SilverGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.SilverGauntlets.Value"), *color);
                    color = &sTemp;
                } else if (strengthUpgrade == PLAYER_STR_GOLD_G &&
                           CVarGetInteger(CVAR_COSMETIC("Gloves.GoldenGauntlets.Changed"), 0)) {
                    sTemp = CVarGetColor24(CVAR_COSMETIC("Gloves.GoldenGauntlets.Value"), *color);
                    color = &sTemp;
                }
                gDPSetEnvColor(POLY_OPA_DISP++, color->r, color->g, color->b, 0);

                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultLeftGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, gLinkAdultRightGauntletPlate1DL);
                gSPDisplayList(POLY_OPA_DISP++, (sLeftHandType == PLAYER_MODELTYPE_LH_OPEN)
                                                    ? gLinkAdultLeftGauntletPlate2DL
                                                    : gLinkAdultLeftGauntletPlate3DL);
                gSPDisplayList(POLY_OPA_DISP++, (sRightHandType == PLAYER_MODELTYPE_RH_OPEN)
                                                    ? gLinkAdultRightGauntletPlate2DL
                                                    : gLinkAdultRightGauntletPlate3DL);
            }

            if (boots != 0) {
                Gfx** bootDLists = sBootDListGroups[boots - 1];

                gSPDisplayList(POLY_OPA_DISP++, bootDLists[0]);
                gSPDisplayList(POLY_OPA_DISP++, bootDLists[1]);
            }
        } else {
            s32 strength = Player_GetStrength();

            // MergeShip2 (BUG-0004): the young Link's plates (or bracelet) come off with the transformation too.
            if ((data != NULL) && (overrideLimbDraw != Player_OverrideLimbDrawPause) &&
                ((Player*)data)->mergeShipTransforming && MERGESHIP_FORM_IS_MM(MergeShip_GetPlayerForm())) {
                strength = PLAYER_STR_NONE;
            }

            // MergeShip2: MM's young Link with the Silver or Golden Gauntlets' strength (SoH's TimelessEquipment)
            // wears their plates, fitted to his forearms (MergeShipChildLinkDraw.inc), in place of the bracelet.
            if (!MergeShip_MMChildLink_DrawGauntlets(play, form, strength) && (strength > PLAYER_STR_NONE)) {
                gSPDisplayList(POLY_OPA_DISP++, gLinkChildGoronBraceletDL);
            }
            MergeShip_MMChildLink_DrawBoots(play, form, boots); // BUG-0099
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

Vec3f sZeroVec = { 0.0f, 0.0f, 0.0f };

// Native MM Player_AdjustSingleLeg geometry, remapped to the six-form enum.
// The first two rows retain OoT's adult/child values.
Vec3f D_80126038[] = {
    { 1304.0f, 0.0f, 0.0f }, { 695.0f, 0.0f, 0.0f },
    { 408.0f, 0.0f, 0.0f }, { 1156.0f, 0.0f, 0.0f },
    { 1406.0f, 0.0f, 0.0f }, { 1304.0f, 0.0f, 0.0f },
};
f32 D_80126050[] = { 1265.0f, 826.0f, 359.0f, 1056.0f, 1506.0f, 1265.0f };
f32 D_80126058[] = { SQ(13.04f), SQ(6.95f), 16.646399f, 133.63359f, 197.68358f, 170.0416f };
f32 D_80126060[] = { 10.019104f, -19.925102f, 3.7582989f, 22.120003f, -29.12001f, 10.019104f };
f32 D_80126068[] = { 5.0f, 3.0f, 1.0f, 4.0f, 1.0f, 5.0f };
MERGESHIP_ASSERT_FORM_TABLE(D_80126038);
MERGESHIP_ASSERT_FORM_TABLE(D_80126050);
MERGESHIP_ASSERT_FORM_TABLE(D_80126058);
MERGESHIP_ASSERT_FORM_TABLE(D_80126060);
MERGESHIP_ASSERT_FORM_TABLE(D_80126068);

Vec3f D_80126070 = { 0.0f, -300.0f, 0.0f };

// MergeShip2: MM Giant's Mask, defined in MergeShipGiantsMask.inc (z_player.c).
void MergeShip_GiantsMask_ScaleFootIK(Player* this, f32* legLengthSq, f32* legLengthSqDiff, f32* footHeight,
                                      Vec3f* footprintPos);

void func_8008F87C(PlayState* play, Player* this, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, s32 thighLimbIndex,
                   s32 shinLimbIndex, s32 footLimbIndex) {
    Vec3f spA4;
    Vec3f sp98;
    Vec3f footprintPos;
    CollisionPoly* sp88;
    s32 sp84;
    f32 sp80;
    f32 sp7C;
    f32 sp78;
    f32 sp74;
    f32 sp70;
    f32 sp6C;
    f32 sp68;
    f32 sp64;
    f32 sp60;
    f32 sp5C;
    f32 sp58;
    f32 sp54;
    f32 sp50;
    s16 temp1;
    s16 temp2;
    s32 temp3;

    if ((this->actor.scale.y >= 0.0f) && !(this->stateFlags1 & PLAYER_STATE1_DEAD) &&
        (Player_ActionToMagicSpell(this, this->itemAction) < 0)) {
        s32 pad;

        sp7C = D_80126058[this->mergeShipForm];
        sp78 = D_80126060[this->mergeShipForm];
        sp74 = D_80126068[this->mergeShipForm] - this->unk_6C4;

        Matrix_Push();
        Matrix_TranslateRotateZYX(pos, rot);
        Matrix_MultVec3f(&sZeroVec, &spA4);
        Matrix_TranslateRotateZYX(&D_80126038[this->mergeShipForm], &skelAnime->jointTable[shinLimbIndex]);
        Matrix_Translate(D_80126050[this->mergeShipForm], 0.0f, 0.0f, MTXMODE_APPLY);
        Matrix_MultVec3f(&sZeroVec, &sp98);
        Matrix_MultVec3f(&D_80126070, &footprintPos);
        Matrix_Pop();

        footprintPos.y += 15.0f;
        // MergeShip2: MM Giant's Mask -- the grown Link's legs are longer in world units (MergeShipGiantsMask.inc).
        MergeShip_GiantsMask_ScaleFootIK(this, &sp7C, &sp78, &sp74, &footprintPos);

        sp80 = BgCheck_EntityRaycastFloor4(&play->colCtx, &sp88, &sp84, &this->actor, &footprintPos) + sp74;

        if (sp98.y < sp80) {
            sp70 = sp98.x - spA4.x;
            sp6C = sp98.y - spA4.y;
            sp68 = sp98.z - spA4.z;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp54 = Math_FAtan2F(sp58, sp60);

            sp6C = sp80 - spA4.y;

            sp64 = sqrtf(SQ(sp70) + SQ(sp6C) + SQ(sp68));
            sp60 = (SQ(sp64) + sp78) / (2.0f * sp64);
            sp5C = sp64 - sp60;

            sp58 = sp7C - SQ(sp60);
            sp58 = (sp7C < SQ(sp60)) ? 0.0f : sqrtf(sp58);

            sp50 = Math_FAtan2F(sp58, sp60);

            temp1 = (M_PI - (Math_FAtan2F(sp5C, sp58) + ((M_PI / 2) - sp50))) * (0x8000 / M_PI);
            temp1 = temp1 - skelAnime->jointTable[shinLimbIndex].z;

            if ((s16)(ABS(skelAnime->jointTable[shinLimbIndex].x) + ABS(skelAnime->jointTable[shinLimbIndex].y)) < 0) {
                temp1 += 0x8000;
            }

            temp2 = (sp50 - sp54) * (0x8000 / M_PI);
            rot->z -= temp2;

            skelAnime->jointTable[thighLimbIndex].z = skelAnime->jointTable[thighLimbIndex].z - temp2;
            skelAnime->jointTable[shinLimbIndex].z = skelAnime->jointTable[shinLimbIndex].z + temp1;
            skelAnime->jointTable[footLimbIndex].z = skelAnime->jointTable[footLimbIndex].z + temp2 - temp1;

            temp3 = SurfaceType_GetFloorType(&play->colCtx, sp88, sp84);

            if ((temp3 >= 2) && (temp3 < 4) && !SurfaceType_IsWallDamage(&play->colCtx, sp88, sp84)) {
                footprintPos.y = sp80;
                EffectSsGFire_Spawn(play, &footprintPos);
            }
        }
    }
}

#include <mergeship/Sprint.h> // MergeShip2: sprinting's pose
extern MsSprint gMergeShipSprint;             // z_player.c
extern MsSprintParams gMergeShipSprintParams; // z_player.c

s32 Player_OverrideLimbDrawGameplayCommon(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                          void* thisx) {
    Player* this = (Player*)thisx;

    // MergeShip2: sprinting's pose (include/mergeship/Sprint.h), the player's own: each limb as the animation left it.
    if ((gMergeShipSprint.blend > 0.0f) && (this == GET_PLAYER(play))) {
        MsSprint_LimbRot(&gMergeShipSprint, &gMergeShipSprintParams, (const char*)this->skelAnime.animation,
                         (const int16_t*)this->skelAnime.jointTable, limbIndex, (int16_t*)rot);
    }

    if (CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0) &&
        CVarGetInteger(CVAR_ENHANCEMENT("ScaleAdultEquipmentAsChild"), 0) && LINK_IS_CHILD &&
        !MERGESHIP_FORM_IS_MM(this->mergeShipForm) && // MergeShip2: Link's equipment only, never a form's
        !MERGESHIP_MM_CHILD(this)) { // MergeShip2: MM's young Link fits the adult's gear itself (MergeShipChildLinkDraw.inc)
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            if ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 sLeftHandType == PLAYER_MODELTYPE_LH_SWORD) ||
                (sLeftHandType == PLAYER_MODELTYPE_LH_BGS) || (sLeftHandType == PLAYER_MODELTYPE_LH_HAMMER)) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_R_HAND) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR && sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) ||
                sRightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT ||
                (sRightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT && Player_HoldsBow(this))) {
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->currentShield == PLAYER_SHIELD_MIRROR ||
                 (this->currentShield == PLAYER_SHIELD_HYLIAN &&
                  (gSaveContext.equips.buttonItems[0] == ITEM_SWORD_MASTER ||
                   gSaveContext.equips.buttonItems[0] == ITEM_SWORD_BGS))) &&
                ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) || (this->sheathType == PLAYER_MODELTYPE_SHEATH_17) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_19))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
            if ((this->currentShield == PLAYER_SHIELD_DEKU && gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI &&
                 (this->sheathType == PLAYER_MODELTYPE_SHEATH_16 || this->sheathType == PLAYER_MODELTYPE_SHEATH_17))) {
                Matrix_Translate(218, -100, 62, MTXMODE_APPLY);
                Matrix_Scale(0.8, 0.8, 0.8, MTXMODE_APPLY);
            }
        }
    }

    if (limbIndex == PLAYER_LIMB_ROOT) {
        sLeftHandType = this->leftHandType;
        sRightHandType = this->rightHandType;
        D_80160000 = &this->meleeWeaponInfo[2].base;

        // MergeShip2: MM Player_OverrideLimbDrawGameplayCommon -- an MM form's root translation is scaled by
        // its own unk_08 (Deku 0.3, Goron 0.74, Zora 1.0) whatever Link's age, and the Fierce Deity's never.
        if ((this->mergeShipForm >= PLAYER_FORM_DEKU) && (this->mergeShipForm != PLAYER_FORM_FIERCE_DEITY)) {
            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 1)) {
                pos->x *= this->ageProperties->unk_08;
                pos->z *= this->ageProperties->unk_08;
            }

            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 2)) {
                pos->y *= this->ageProperties->unk_08;
            }
        } else if (!LINK_IS_ADULT && (this->mergeShipForm != PLAYER_FORM_FIERCE_DEITY)) {
            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 1)) {
                pos->x *= 0.64f;
                pos->z *= 0.64f;
            }

            if (!(this->skelAnime.movementFlags & 4) || (this->skelAnime.movementFlags & 2)) {
                pos->y *= 0.64f;
            }
        }

        pos->y -= this->unk_6C4;

        // MergeShip2: MM -- the gliding Deku hangs from its petals (PLAYER_STATE3_2000).
        if (this->mergeShipForm == PLAYER_FORM_DEKU) {
            MergeShip_Deku_OverrideLimbRoot(this, pos, rot);
        }

        // MergeShip2: MM -- the Zora's fast swim pitches and rolls the body (and carries the barrier shell).
        if (MergeShip_Zora_OverrideRootLimb(play, this, pos, rot)) {
        } else if (this->unk_6C2 != 0) {
            Matrix_Translate(pos->x, ((Math_CosS(this->unk_6C2) - 1.0f) * 200.0f) + pos->y, pos->z, MTXMODE_APPLY);
            Matrix_RotateX(this->unk_6C2 * (M_PI / 0x8000), MTXMODE_APPLY);
            Matrix_RotateZYX(rot->x, rot->y, rot->z, MTXMODE_APPLY);
            pos->x = pos->y = pos->z = 0.0f;
            rot->x = rot->y = rot->z = 0;
        }
    } else {
        if (*dList != NULL) {
            D_80160000++;
        }

        // MergeShip2: MM -- the Zora's torso breathes and stretches while swimming; its head fins widen.
        MergeShip_Zora_OverrideLimb(play, this, limbIndex, pos, rot);

        // MergeShip2: MM Player_OverrideLimbDrawGameplayDefault -- the hands follow the petals while gliding.
        if (((limbIndex == PLAYER_LIMB_L_HAND) || (limbIndex == PLAYER_LIMB_R_HAND)) &&
            (this->stateFlags3 & PLAYER_STATE3_2000)) {
            rot->z -= this->unk_B8C;
        }

        if (limbIndex == PLAYER_LIMB_HEAD) {
            // MergeShip2: the head-scale cosmetic is sized for Link's head (320/200 offsets); not for a form.
            if (CVarGetInteger(CVAR_COSMETIC("Link.HeadScale.Changed"), 0) &&
                !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.HeadScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    if (scale > 1.2f) {
                        Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * scale), 0.0f, 0.0f, MTXMODE_APPLY);
                    } else if (scale < 1.0f) {
                        Matrix_Translate((LINK_IS_ADULT ? 3600.0f : 2900.0f) * ABS(scale - 1.0f), 0.0f, 0.0f,
                                         MTXMODE_APPLY);
                    }
                }
            }
            rot->x += this->headLimbRot.z;
            rot->y -= this->headLimbRot.y;
            rot->z += this->headLimbRot.x;

            // MergeShip2: MM -- the Deku's head swells with the bubble, the spit and the drink.
            if (this->mergeShipForm == PLAYER_FORM_DEKU) {
                MergeShip_Deku_OverrideLimbHead(play, this, pos, rot);
            }
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            // MergeShip2: the sword-scale cosmetic is sized for Link's hand (320/200 offsets); not for a form.
            if (CVarGetInteger(CVAR_COSMETIC("Link.SwordScale.Changed"), 0) &&
                !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
                f32 scale = CVarGetFloat(CVAR_COSMETIC("Link.SwordScale.Value"), 1.0f);
                if (scale != 1.0f) {
                    Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
                    Matrix_Translate(-((LINK_IS_ADULT ? 320.0f : 200.0f) * (scale - 1.0f)), 0.0f, 0.0f, MTXMODE_APPLY);
                }
            }
        } else if (limbIndex == PLAYER_LIMB_UPPER) {
            if (this->upperLimbYawSecondary != 0) {
                Matrix_RotateZ(0x44C * (M_PI / 0x8000), MTXMODE_APPLY);
                Matrix_RotateY(this->upperLimbYawSecondary * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.y != 0) {
                Matrix_RotateY(this->upperLimbRot.y * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.x != 0) {
                Matrix_RotateX(this->upperLimbRot.x * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            if (this->upperLimbRot.z != 0) {
                Matrix_RotateZ(this->upperLimbRot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            }
            // MergeShip2: MM -- the Deku leans back as its bubble grows.
            if (this->mergeShipForm == PLAYER_FORM_DEKU) {
                MergeShip_Deku_OverrideLimbUpper(this);
            }
        } else if (limbIndex == PLAYER_LIMB_TORSO) {
            MergeShip_FormLib_GoronTorso(this, pos, rot); // MergeShip2: MM -- the Goron's torso breathes and swells
        } else if (limbIndex == PLAYER_LIMB_L_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_L_THIGH, PLAYER_LIMB_L_SHIN,
                          PLAYER_LIMB_L_FOOT);
        } else if (limbIndex == PLAYER_LIMB_R_THIGH) {
            func_8008F87C(play, this, &this->skelAnime, pos, rot, PLAYER_LIMB_R_THIGH, PLAYER_LIMB_R_SHIN,
                          PLAYER_LIMB_R_FOOT);
            return false;
        } else {
            return false;
        }
    }

    return false;
}

s32 Player_OverrideLimbDrawGameplayDefault(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                           void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (limbIndex == PLAYER_LIMB_L_HAND) {
            Gfx** dLists = this->leftHandDLists;

            if (MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
                // MergeShip2: MM Player_OverrideLimbDrawGameplayDefault -- running fists, then the animation's hand.
                dLists = MergeShip_FormLib_LeftHandDLists(this, dLists);
            } else if ((sLeftHandType == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f)) {
                dLists += 2 * PLAYER_FORM_MAX;
            } else if ((sLeftHandType == PLAYER_MODELTYPE_LH_BOOMERANG) &&
                       (this->stateFlags1 & PLAYER_STATE1_BOOMERANG_THROWN)) {
                dLists = &gPlayerLeftHandOpenDLs[this->mergeShipForm];
                sLeftHandType = PLAYER_MODELTYPE_LH_OPEN;
            } else if ((this->leftHandType == PLAYER_MODELTYPE_LH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &gPlayerLeftHandClosedDLs[this->mergeShipForm];
                sLeftHandType = PLAYER_MODELTYPE_LH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
            MergeShip_FormLib_GoronFists(this, limbIndex, pos, rot); // MergeShip2: MM pg_punchA's swelling fist
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            Gfx** dLists = this->rightHandDLists;

            if (MERGESHIP_FORM_IS_MM(this->mergeShipForm)) {
                // MergeShip2: MM Player_OverrideLimbDrawGameplayDefault -- running fists, then the animation's hand.
                dLists = MergeShip_FormLib_RightHandDLists(this, dLists);
            } else if (sRightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
                dLists += this->currentShield * 2 * PLAYER_FORM_MAX;
            } else if ((this->rightHandType == PLAYER_MODELTYPE_RH_OPEN) && (this->actor.speedXZ > 2.0f) &&
                       !(this->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
                dLists = &sPlayerRightHandClosedDLs[this->mergeShipForm];
                sRightHandType = PLAYER_MODELTYPE_RH_CLOSED;
            }

            *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
            MergeShip_FormLib_GoronFists(this, limbIndex, pos, rot); // MergeShip2: MM pg_punchB's swelling fist
        } else if (limbIndex == PLAYER_LIMB_SHEATH) {
            Gfx** dLists = this->sheathDLists;

            // MergeShip2: MM Player_OverrideLimbDrawGameplayDefault -- an MM form draws its own column's sheath as
            // is. OoT's sword/shield offsets below jump to whole rows of the table and would land on OoT Link's
            // scabbard and shield.
            if (this->mergeShipForm >= PLAYER_FORM_DEKU) {
            } else if ((this->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                       (this->sheathType == PLAYER_MODELTYPE_SHEATH_19)) {
                dLists += this->currentShield * 2 * PLAYER_FORM_MAX;
                if (!LINK_IS_ADULT && (this->currentShield < PLAYER_SHIELD_HYLIAN) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                    // MergeShip2: OoT's 2-age stride (PLAYER_SHIELD_MAX * 4) was a whole row of 6 forms short.
                    dLists += PLAYER_SHIELD_MAX * 2 * PLAYER_FORM_MAX;
                }
            } else if (!MERGESHIP_EQUIPMENT_VISIBLE(this) ||
                       (MERGESHIP_EQUIPMENT_VISIBLE(this) &&
                        ((gSaveContext.equips.buttonItems[0] != ITEM_SWORD_MASTER &&
                          gSaveContext.equips.buttonItems[0] != ITEM_SWORD_BGS) &&
                         this->currentShield == PLAYER_SHIELD_DEKU))) {
                if (!LINK_IS_ADULT &&
                    ((this->sheathType == PLAYER_MODELTYPE_SHEATH_16) ||
                     (this->sheathType == PLAYER_MODELTYPE_SHEATH_17)) &&
                    (gSaveContext.equips.buttonItems[0] != ITEM_SWORD_KOKIRI)) {
                    dLists = &sSheathWithSwordDLs[PLAYER_SHIELD_MAX * 2 * PLAYER_FORM_MAX];
                }
            }

            if (dLists[sDListsLodOffset] != NULL) {
                *dList = ResourceMgr_LoadGfxByName(dLists[sDListsLodOffset]);
            } else {
                *dList = NULL;
            }

        } else if (limbIndex == PLAYER_LIMB_WAIST) {

            // MergeShip2: a custom Link model never replaces an MM form's waist.
            if (MERGESHIP_FORM_IS_MM(this->mergeShipForm) || !Player_IsCustomLinkModel()) {
                *dList = ResourceMgr_LoadGfxByName(
                    this->waistDLists[sDListsLodOffset]); // NOTE: This needs to be disabled when using custom
                                                          // characters - they're not going to have LODs anyways...
            }
        }

        // MergeShip2: MM's swords, shields, bow and hookshot in hand and on the back (MergeShipMMEquipmentDraw.inc);
        // otherwise MM's human, as the child -- MM's hands holding OoT's equipment, OoT's sheath where OoT's sheath
        // limb is, MM's waist (MergeShipChildLinkDraw.inc).
        if (!MergeShip_MMEquip_OverrideLimb(play, this, limbIndex, dList, pos)) {
            MergeShip_MMChildLink_OverrideLimb(play, this, limbIndex, dList, pos);
        }
        // MergeShip2: MM's young Link's forearms and hands, for the gauntlet plates Player_DrawImpl puts on them.
        MergeShip_MMChildLink_CaptureArm(play, MERGESHIP_MM_CHILD(this), limbIndex, pos, rot);

        // MergeShip2: MM -- the Zora's open hands (transforming, getting an item, swimming) and its guitar.
        if (this->mergeShipForm == PLAYER_FORM_ZORA) {
            MergeShip_Zora_OverrideHandLimb(play, this, limbIndex, dList, pos, rot);
        }
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    if (GameInteractor_InvisibleLinkActive()) {
        this->actor.shape.shadowDraw = NULL;
        *dList = NULL;
    }

    return false;
}

s32 Player_OverrideLimbDrawGameplayFirstPerson(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                               void* thisx) {
    Player* this = (Player*)thisx;

    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        if (this->unk_6AD != 2) {
            *dList = NULL;
        } else if (MergeShip_FormLib_FirstPersonLimb(this, limbIndex, dList)) {
            // MergeShip2: MM Player_OverrideLimbDrawGameplayFirstPerson -- a form aims with its own arms.
        } else if (limbIndex == PLAYER_LIMB_L_FOREARM) {
            *dList = sFirstPersonLeftForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_L_HAND) {
            s32 handOutDlIndex = gSaveContext.linkAge;
            if ((CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                 CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) &&
                LINK_IS_ADULT && Player_HoldsSlingshot(this)) {
                handOutDlIndex = 1;
            }
            *dList = sFirstPersonLeftHandDLs[handOutDlIndex];
        } else if (limbIndex == PLAYER_LIMB_R_SHOULDER) {
            *dList = sFirstPersonRightShoulderDLs[gSaveContext.linkAge];
            MergeShip_MMChildLink_OverrideLimbFirstPerson(this, limbIndex, dList); // MergeShip2: MM's human shoulder
        } else if (limbIndex == PLAYER_LIMB_R_FOREARM) {
            *dList = sFirstPersonForearmDLs[gSaveContext.linkAge];
        } else if (limbIndex == PLAYER_LIMB_R_HAND) {
            s32 firstPersonWeaponIndex = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                if (Player_HoldsBow(this)) {
                    firstPersonWeaponIndex = 0;
                } else if (Player_HoldsSlingshot(this)) {
                    firstPersonWeaponIndex = 1;
                }
            }
            *dList = Player_HoldsHookshot(this) ? gLinkAdultRightHandHoldingHookshotFarDL
                                                : sFirstPersonRightHandHoldingWeaponDLs[firstPersonWeaponIndex];
        } else {
            *dList = NULL;
        }
        // MergeShip2: MM's first-person Hero's Bow and Hookshot (MergeShipMMEquipmentDraw.inc).
        MergeShip_MMEquip_OverrideLimbFirstPerson(play, this, limbIndex, dList);
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW, true, limbIndex, dList, thisx, play);

    return false;
}

s32 Player_OverrideLimbDrawGameplayCrawling(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                            void* thisx) {
    if (!Player_OverrideLimbDrawGameplayCommon(play, limbIndex, dList, pos, rot, thisx)) {
        *dList = NULL;
    }

    return false;
}

u8 func_80090480(PlayState* play, ColliderQuad* collider, WeaponInfo* weaponInfo, Vec3f* newTip, Vec3f* newBase) {
    if (weaponInfo->active == 0) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        weaponInfo->active = 1;
        return 1;
    } else if ((weaponInfo->tip.x == newTip->x) && (weaponInfo->tip.y == newTip->y) &&
               (weaponInfo->tip.z == newTip->z) && (weaponInfo->base.x == newBase->x) &&
               (weaponInfo->base.y == newBase->y) && (weaponInfo->base.z == newBase->z)) {
        if (collider != NULL) {
            Collider_ResetQuadAT(play, &collider->base);
        }
        return 0;
    } else {
        if (collider != NULL) {
            Collider_SetQuadVertices(collider, newBase, newTip, &weaponInfo->base, &weaponInfo->tip);
            CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
        }
        Math_Vec3f_Copy(&weaponInfo->base, newBase);
        Math_Vec3f_Copy(&weaponInfo->tip, newTip);
        weaponInfo->active = 1;
        return 1;
    }
}

void Player_UpdateShieldCollider(PlayState* play, Player* this, ColliderQuad* collider, Vec3f* quadSrc) {
    static u8 shieldColTypes[PLAYER_SHIELD_MAX] = {
        COLTYPE_METAL,
        COLTYPE_WOOD,
        COLTYPE_METAL,
        COLTYPE_METAL,
    };

    if (this->stateFlags1 & PLAYER_STATE1_SHIELDING) {
        Vec3f quadDest[4];

        this->shieldQuad.base.colType = shieldColTypes[this->currentShield];

        Matrix_MultVec3f(&quadSrc[0], &quadDest[0]);
        Matrix_MultVec3f(&quadSrc[1], &quadDest[1]);
        Matrix_MultVec3f(&quadSrc[2], &quadDest[2]);
        Matrix_MultVec3f(&quadSrc[3], &quadDest[3]);
        Collider_SetQuadVertices(collider, &quadDest[0], &quadDest[1], &quadDest[2], &quadDest[3]);

        CollisionCheck_SetAC(play, &play->colChkCtx, &collider->base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &collider->base);
    }
}

Vec3f D_80126080 = { 5000.0f, 400.0f, 0.0f };
Vec3f D_8012608C = { 5000.0f, -400.0f, 1000.0f };
Vec3f D_80126098 = { 5000.0f, 1400.0f, -1000.0f };

Vec3f D_801260A4[3] = {
    { 0.0f, 400.0f, 0.0f },
    { 0.0f, 1400.0f, -1000.0f },
    { 0.0f, -400.0f, 1000.0f },
};

void func_800906D4(PlayState* play, Player* this, Vec3f* newTipPos) {
    Vec3f newBasePos[3];

    Matrix_MultVec3f(&D_801260A4[0], &newBasePos[0]);
    Matrix_MultVec3f(&D_801260A4[1], &newBasePos[1]);
    Matrix_MultVec3f(&D_801260A4[2], &newBasePos[2]);

    if (func_80090480(play, NULL, &this->meleeWeaponInfo[0], &newTipPos[0], &newBasePos[0]) &&
        !(this->stateFlags1 & PLAYER_STATE1_SHIELDING) &&
        !CVarGetInteger(CVAR_ENHANCEMENT("DisableLinkSwordTrail"), 0)) {
        EffectBlure_AddVertex(Effect_GetByIndex(this->meleeWeaponEffectIndex), &this->meleeWeaponInfo[0].tip,
                              &this->meleeWeaponInfo[0].base);
    }

    if ((this->meleeWeaponState > 0) &&
        ((this->meleeWeaponAnimation < 0x18) || (this->stateFlags2 & PLAYER_STATE2_SPIN_ATTACKING))) {
        func_80090480(play, &this->meleeWeaponQuads[0], &this->meleeWeaponInfo[1], &newTipPos[1], &newBasePos[1]);
        func_80090480(play, &this->meleeWeaponQuads[1], &this->meleeWeaponInfo[2], &newTipPos[2], &newBasePos[2]);
    }
}

void Player_DrawGetItemIceTrap(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne, f32 height) {
    OPEN_DISPS(play->state.gfxCtx);

    if (CVarGetInteger(CVAR_GENERAL("LetItSnow"), 0)) {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);

        Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);

        gDPSetGrayscaleColor(POLY_OPA_DISP++, 75, 75, 75, 255);
        gSPGrayscale(POLY_OPA_DISP++, true);

        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)gSilverRockDL);

        gSPGrayscale(POLY_OPA_DISP++, false);
    } else {
        if (iceTrapScale < 0.01) {
            iceTrapScale += 0.001f;
        } else if (iceTrapScale < 0.8f) {
            iceTrapScale += 0.2f;
        }

        // Draw the ice only after a bit so it doesn't spoil the fact that it's a trap
        if (iceTrapScale >= 0.01) {
            gSPSegment(POLY_XLU_DISP++, 0x08,
                       Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, (0 - play->gameplayFrames) % 128, 32, 32, 1, 0,
                                          (play->gameplayFrames * -2) % 128, 32, 32, 0, -1, 0, -2));

            Matrix_Translate(0.0f, -40.0f, 0.0f, MTXMODE_APPLY);
            Matrix_Scale(iceTrapScale, iceTrapScale, iceTrapScale, MTXMODE_APPLY);
            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gDPSetEnvColor(POLY_XLU_DISP++, 0, 50, 100, 255);
            gSPDisplayList(POLY_XLU_DISP++, gEffIceFragment3DL);

            // Reset matrix for the fake item model because we're animating the size of the ice block around it before
            // this.
            Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                             refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)),
                             MTXMODE_NEW);
            Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
            Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);
        }

        // Draw fake item model.
        if (this->getItemEntry.drawFunc != NULL) {
            this->getItemEntry.drawFunc(play, &this->getItemEntry);
        } else {
            GetItem_Draw(play, drawIdPlusOne - 1);
        }
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItemImpl(PlayState* play, Player* this, Vec3f* refPos, s32 drawIdPlusOne) {
    f32 height = (this->exchangeItemId != EXCH_ITEM_NONE) ? 6.0f : 14.0f;

    OPEN_DISPS(play->state.gfxCtx);

    gSegments[6] = VIRTUAL_TO_PHYSICAL(this->giObjectSegment);

    gSPSegment(POLY_OPA_DISP++, 0x06, this->giObjectSegment);
    gSPSegment(POLY_XLU_DISP++, 0x06, this->giObjectSegment);

    Matrix_Translate(refPos->x + (3.3f * Math_SinS(this->actor.shape.rot.y)), refPos->y + height,
                     refPos->z + ((3.3f + (IREG(90) / 10.0f)) * Math_CosS(this->actor.shape.rot.y)), MTXMODE_NEW);
    Matrix_RotateZYX(0, play->gameplayFrames * 1000, 0, MTXMODE_APPLY);
    Matrix_Scale(0.2f, 0.2f, 0.2f, MTXMODE_APPLY);

    if (this->getItemEntry.modIndex == MOD_RANDOMIZER && this->getItemEntry.getItemId == RG_ICE_TRAP) {
        Player_DrawGetItemIceTrap(play, this, refPos, drawIdPlusOne, height);
    } else if (this->getItemEntry.modIndex == MOD_RANDOMIZER &&
               (this->getItemEntry.getItemId == RG_TRIFORCE_PIECE || this->getItemEntry.getItemId == RG_TRIFORCE)) {
        Randomizer_DrawTriforcePieceGI(play, this->getItemEntry);
    } else if (this->getItemEntry.drawFunc != NULL) {
        this->getItemEntry.drawFunc(play, &this->getItemEntry);
    } else {
        GetItem_Draw(play, drawIdPlusOne - 1);
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawGetItem(PlayState* play, Player* this) {
    // if (!this->giObjectLoading || !osRecvMesg(&this->giObjectLoadQueue, NULL, OS_MESG_NOBLOCK)) // OTRTODO: Do
    // something about osRecvMesg here...
    {
        this->giObjectLoading = false;
        Player_DrawGetItemImpl(play, this, &sGetItemRefPos, ABS(this->unk_862));
    }
}

void func_80090A28(Player* this, Vec3f* vecs) {
    D_8012608C.x = D_80126080.x;

    if (this->unk_845 >= 3) {
        this->unk_845 += 1;
        D_8012608C.x *= 1.0f + ((9 - this->unk_845) * 0.1f);
    }

    D_8012608C.x += 1200.0f;
    D_80126098.x = D_8012608C.x;

    Matrix_MultVec3f(&D_80126080, &vecs[0]);
    Matrix_MultVec3f(&D_8012608C, &vecs[1]);
    Matrix_MultVec3f(&D_80126098, &vecs[2]);
}

void Player_DrawHookshotReticle(PlayState* play, Player* this, f32 hookshotRange) {
    static Vec3f D_801260C8 = { -500.0f, -100.0f, 0.0f };
    CollisionPoly* colPoly;
    s32 bgId;
    Vec3f hookshotStart;
    Vec3f hookshotEnd;
    Vec3f firstHit;
    Vec3f sp68;
    f32 sp64;

    D_801260C8.z = 0.0f;
    Matrix_MultVec3f(&D_801260C8, &hookshotStart);
    D_801260C8.z = hookshotRange;
    Matrix_MultVec3f(&D_801260C8, &hookshotEnd);

    if (BgCheck_AnyLineTest3(&play->colCtx, &hookshotStart, &hookshotEnd, &firstHit, &colPoly, 1, 1, 1, 1, &bgId)) {
        OPEN_DISPS(play->state.gfxCtx);

        OVERLAY_DISP = Gfx_SetupDL(OVERLAY_DISP, 0x07);

        SkinMatrix_Vec3fMtxFMultXYZW(&play->viewProjectionMtxF, &firstHit, &sp68, &sp64);

        const f32 sp60 = (sp64 < 200.0f) ? 0.08f : (sp64 / 200.0f) * 0.08f;

        Matrix_Translate(firstHit.x, firstHit.y, firstHit.z, MTXMODE_NEW);
        Matrix_Scale(sp60, sp60, sp60, MTXMODE_APPLY);

        gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (GameInteractor_Should(VB_TARGETABLE_HOOKSHOT_RETICLE, true, colPoly, bgId)) {
            gSPSegment(OVERLAY_DISP++, 0x06, play->objectCtx.status[this->actor.objBankIndex].segment);
            gSPDisplayList(OVERLAY_DISP++, gLinkAdultHookshotReticleDL);
        }

        CLOSE_DISPS(play->state.gfxCtx);
    }
}

// Coordinates of the player focus position, in the head limb's own model space.
Vec3f sPlayerFocusOffsetFromHead = { 1100.0f, -700.0f, 0.0f };

f32 sMeleeWeaponLengths[] = {
    0.0f, 4000.0f, 3000.0f, 5500.0f, 0.0f, 2500.0f,
    5500.0f, // MergeShip2: MERGESHIP_MELEEWEAPON_MM_TWO_HANDED (MM sMeleeWeaponLengths SWORD_TWO_HANDED)
    3000.0f, // MergeShip2: MERGESHIP_MELEEWEAPON_MM_RAZOR (MM sMeleeWeaponLengths SWORD_RAZOR)
    4000.0f, // MergeShip2: MERGESHIP_MELEEWEAPON_MM_GILDED (MM sMeleeWeaponLengths SWORD_GILDED)
};

f32 sSwordTypes[] = {
    TRAIL_TYPE_REST,           TRAIL_TYPE_MASTER_SWORD, TRAIL_TYPE_KOKIRI_SWORD,
    TRAIL_TYPE_BIGGORON_SWORD, TRAIL_TYPE_REST,         TRAIL_TYPE_HAMMER,
    TRAIL_TYPE_REST, // MergeShip2: Fierce Deity's sword keeps MM's colours (set by the player; no SoH recolour)
    // MergeShip2: MM's Razor and Gilded Swords -- MM's one white sword trail, which SoH's sword default also is.
    TRAIL_TYPE_KOKIRI_SWORD, TRAIL_TYPE_KOKIRI_SWORD,
};

Gfx* sBottleDLists[] = { gLinkAdultBottleDL, gLinkChildBottleDL };

Color_RGB8 sBottleColors[] = {
    { 255, 255, 255 }, { 80, 80, 255 },   { 255, 100, 255 }, { 0, 0, 255 }, { 255, 0, 255 },
    { 255, 0, 255 },   { 200, 200, 100 }, { 255, 0, 0 },     { 0, 0, 255 }, { 0, 255, 0 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 80, 80, 255 },
    { 255, 255, 255 }, // PLAYER_BOTTLE_MM_CHATEAU // MergeShip2: MM sPlayerBottleColors[PLAYER_BOTTLE_CHATEAU]
    // MergeShip2: MM sPlayerBottleColors for MM's other contents (MergeShipMMItems.h PLAYER_BOTTLE_MM_*).
    { 248, 200, 0 },   // PLAYER_BOTTLE_MM_DEKU_PRINCESS
    { 184, 232, 232 }, // PLAYER_BOTTLE_MM_ZORA_EGG
    { 255, 180, 0 },   // PLAYER_BOTTLE_MM_GOLD_DUST
    { 131, 0, 174 },   // PLAYER_BOTTLE_MM_MUSHROOM
    { 252, 238, 0 },   // PLAYER_BOTTLE_MM_SEAHORSE
    { 64, 64, 32 },    // PLAYER_BOTTLE_MM_HYLIAN_LOACH
    { 136, 192, 255 }, // PLAYER_BOTTLE_MM_SPRING_WATER
    { 136, 192, 255 }, // PLAYER_BOTTLE_MM_HOT_SPRING_WATER
};
MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(sBottleColors) == PLAYER_BOTTLE_MM_LAST + 1,
                        "sBottleColors needs a colour for every Player_ActionToBottle result");

Vec3f sLeftHandArrowVec3 = { 398.0f, 1419.0f, 244.0f };

BowStringData sBowStringData[] = {
    { gLinkAdultBowStringDL, { 0.0f, -360.4f, 0.0f } },        // bow
    { gLinkChildSlingshotStringDL, { 606.0f, 236.0f, 0.0f } }, // slingshot
};

Vec3f sRightHandLimbModelShieldQuadVertices[] = {
    { -4500.0f, -3000.0f, -600.0f },
    { 1500.0f, -3000.0f, -600.0f },
    { -4500.0f, 3000.0f, -600.0f },
    { 1500.0f, 3000.0f, -600.0f },
};

Vec3f D_80126184 = { 100.0f, 1500.0f, 0.0f };
Vec3f D_80126190 = { 100.0f, 1640.0f, 0.0f };

Vec3f sSheathLimbModelShieldQuadVertices[] = {
    { -3000.0f, -3000.0f, -900.0f },
    { 3000.0f, -3000.0f, -900.0f },
    { -3000.0f, 3000.0f, -900.0f },
    { 3000.0f, 3000.0f, -900.0f },
};

Vec3f sSheathLimbModelShieldOnBackPos = { 630.0f, 100.0f, -30.0f };
Vec3s sSheathLimbModelShieldOnBackZyxRot = { 0, 0, 0x7FFF };

Vec3f sLeftRightFootLimbModelFootPos[] = {
    { 200.0f, 300.0f, 0.0f },
    { 200.0f, 200.0f, 0.0f },
};

// MergeShip2 -- Majora's Mask's transformation masks, drawn where MM draws them.
//
// MM z_player_lib.c: the PLAYER_LIMB_HEAD block of Player_PostLimbDrawGameplay (the mask on the face,
// squashed by unk_B10[2]/[3] as the face deforms, and swapped for its "taking hold" model late in
// the animation) and func_80128640 (the mask held in the left hand as it is lifted to the face or
// taken off). Animation checks compare resource paths, as 2S2H's BEN_ANIM_EQUAL does, because each
// translation unit has its own copy of an animation's path string.

// MM D_801C0B20[PLAYER_MASK_FIERCE_DEITY - 1 ...], then the four "+4" models.
static const char* sMMTransformationMaskDLs[8] = {
    mm_gFierceDeityMaskDL, mm_gGoronMaskDL, mm_gZoraMaskDL, mm_gDekuMaskDL,
    mm_object_mask_boy_DL_000900, mm_object_mask_goron_DL_0014A0, mm_object_mask_zora_DL_000DB0,
    mm_object_mask_nuts_DL_001D90,
};

static s32 MergeShip_AnimIs(LinkAnimationHeader* anim, const char* path) {
    return (anim != NULL) && (strcmp((const char*)anim, path) == 0);
}

// MM D_801C0E04: where a transformation mask sits on each form's head.
static Vec2f sMMTransformationMaskHeadOffsets[MM_FORM_MAX] = {
    { 140.0f, -130.0f }, // MM_FORM_FIERCE_DEITY
    { 0.0f, -200.0f },   // MM_FORM_GORON
    { -160.0f, 0.0f },   // MM_FORM_ZORA
    { 220.0f, -200.0f }, // MM_FORM_DEKU
    { 0.0f, 0.0f },      // MM_FORM_HUMAN
};

static void MergeShip_DrawMaskOnHead(PlayState* play, Player* player, Gfx** dList) {
    u8 mmForm = MergeShip_FormToMM(player->mergeShipForm);
    s32 mask = player->currentMask;
    LinkAnimationHeader* anim = player->skelAnime.animation;
    s32 index;

    // MergeShip2 (BUG-0126): first person (Player_Draw's unk_6AD) puts the camera in his head, where Ocarina of Time's
    // masks fit; MM's are bigger than his head (the Captain's Hat's brim, the Bunny Hood's ears) and were seen from
    // inside it. None of them while the camera is at his head (MM draws no mask in first person either).
    if ((player->unk_6AD != 0) && (Math_Vec3f_DistXYZ(&play->view.eye, &player->actor.focus.pos) < 40.0f)) {
        return;
    }
    if ((*dList != NULL) && PLAYER_MASK_IS_MM_REGULAR(mask)) {
        // MergeShip2: MM -- a human's mask is not on his face for the first 12 frames of gPlayerAnim_cl_setmask (the
        // Giant's Mask is in his hand then, MergeShip_GiantsMask_DrawInHand).
        if (!MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_setmask) || (player->skelAnime.curFrame >= 12.0f)) {
            MergeShip_MMMasks_DrawOnHead(play, player); // MM's other twenty masks (MergeShipMMMasks.c)
        }
        return;
    }
    if ((*dList == NULL) || !PLAYER_MASK_IS_MM_TRANSFORMATION(mask)) {
        return;
    }
    if (!(((mmForm == MM_FORM_HUMAN) &&
           (!MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_setmask) || (player->skelAnime.curFrame >= 12.0f))) ||
          ((mmForm != MM_FORM_HUMAN) && ((mmForm + PLAYER_MASK_MM_FIERCE_DEITY) != mask) &&
           (player->skelAnime.curFrame >= 10.0f)))) {
        return;
    }

    index = mask - PLAYER_MASK_MM_FIERCE_DEITY;

    if (MergeShip_Ganondorf_IsPlayer(player)) {
        // MergeShip2: Ganondorf mode -- his own masks, squeezed onto his face as MM's are
        Matrix_Push();
        Matrix_Scale(1.0f, 1.0f - player->unk_B10[3], 1.0f - player->unk_B10[2], MTXMODE_APPLY);
        MergeShip_Ganondorf_DrawMask(play, player, mask, false);
        Matrix_Pop();
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    {
        Vec2f* offset = &sMMTransformationMaskHeadOffsets[mmForm];

        Matrix_Push();
        // MM's Vec2f is { x, z }: Matrix_Translate(offset->x, offset->z, 0.0f). OoT's is { x, y }.
        Matrix_Translate(offset->x, offset->y, 0.0f, MTXMODE_APPLY);
        Matrix_Scale(1.0f, 1.0f - player->unk_B10[3], 1.0f - player->unk_B10[2], MTXMODE_APPLY);
        MergeShip_AdultMask_ApplyHeadFit(player); // MergeShip2: the Child Mask on the adult's head
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        Matrix_Pop();
    }

    if ((MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_setmask) && (player->skelAnime.curFrame >= 51.0f)) ||
        MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_setmaskend)) {
        index += 4;
    }

    // MergeShip2: the Adult Mask (the Child Mask, the adult's) is drawn as itself (MergeShipAdultMask.h)
    gSPDisplayList(POLY_OPA_DISP++, (((index == 0) || (index == 4)) && MergeShip_AdultMask_DrawingKind(player))
                                        ? (Gfx*)MergeShip_AdultMask_DLFor(player, index == 4)
                                        : (Gfx*)sMMTransformationMaskDLs[index]);

    CLOSE_DISPS(play->state.gfxCtx);
}

// MergeShip2: MM func_80128640 for the Giant's Mask, defined in MergeShipGiantsMask.inc (z_player.c).
void MergeShip_GiantsMask_DrawInHand(PlayState* play, Player* player, s32 mask);

// MM func_80128640's mask branch.
static void MergeShip_DrawMaskInHand(PlayState* play, Player* player) {
    LinkAnimationHeader* anim = player->skelAnime.animation;
    s32 takingOff = MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_maskoff);
    s32 mask;
    f32 frame;

    if (!(takingOff || ((player->currentMask != PLAYER_MASK_NONE) && MergeShip_AnimIs(anim, mm_gPlayerAnim_cl_setmask) &&
                        (frame = player->skelAnime.curFrame - 8.0f, (frame >= 0.0f)) && (frame < 4.0f)))) {
        return;
    }

    mask = takingOff ? player->prevMask : player->currentMask;
    if (!PLAYER_MASK_IS_MM_TRANSFORMATION(mask)) {
        MergeShip_GiantsMask_DrawInHand(play, player, mask); // MergeShip2: MM func_80128640 for the Giant's Mask
        return;
    }

    if (MergeShip_Ganondorf_IsPlayer(player)) {
        // MergeShip2: Ganondorf mode -- his own mask in his hand
        Matrix_Push();
        Matrix_Translate(-323.67f, 412.15f, -969.96f, MTXMODE_APPLY);
        Matrix_RotateZYX(-0x32BE, -0x50DE, -0x7717, MTXMODE_APPLY);
        MergeShip_Ganondorf_DrawMask(play, player, mask, true);
        Matrix_Pop();
        return;
    }

    OPEN_DISPS(play->state.gfxCtx);

    Matrix_Push();
    Matrix_Translate(-323.67f, 412.15f, -969.96f, MTXMODE_APPLY);
    Matrix_RotateZYX(-0x32BE, -0x50DE, -0x7717, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    // MergeShip2: the Adult Mask going on is drawn as itself (MergeShipAdultMask.h)
    gSPDisplayList(POLY_OPA_DISP++,
                   (!takingOff && (mask == PLAYER_MASK_MM_FIERCE_DEITY) && MergeShip_AdultMask_DrawingKind(player))
                       ? (Gfx*)MergeShip_AdultMask_DLFor(player, false)
                       : (Gfx*)sMMTransformationMaskDLs[mask - PLAYER_MASK_MM_FIERCE_DEITY]);
    Matrix_Pop();

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_PostLimbDrawGameplay(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    Player* this = (Player*)thisx;

    if (*dList != NULL) {
        Matrix_MultVec3f(&sZeroVec, D_80160000);
    }

    // MergeShip2: MM's transformation masks.
    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MergeShip_DrawMaskInHand(play, this);
    } else if (limbIndex == PLAYER_LIMB_HEAD) {
        MergeShip_DrawMaskOnHead(play, this, dList);
        MergeShip_FormLib_DrawTransformGlow(play, this); // MergeShip2: MM's blue glow as the mask takes hold
        MergeShip_SohFd_DrawOnHead(play, this, dList); // MergeShip2 (soh_fd): Hyrule's masks on a form, Bunny Hood Fit
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        MergeShip_SohFd_DrawFdOcarina(play, this); // MergeShip2 (soh_fd): the Fierce Deity's ocarina
    }

    // MergeShip2: MM func_80126BD0 and the right shin -- the Zora's fins and its punch/kick colliders.
    MergeShip_Zora_PostLimbDraw(play, this, limbIndex);

    // MergeShip2: MM Player_PostLimbDrawGameplay's Goron parts (the fists' colliders and punch flashes, the
    // drums). A Goron has no sword, so the sword collider update below is skipped for it.
    MergeShip_FormLib_PopGoronTorso(); // MergeShip2: MM func_8012536C -- the torso's scale stops at its children
    s32 mergeShipGoronFists =
        (this->mergeShipForm == PLAYER_FORM_GORON) && MergeShip_Goron_PostLimbDraw(play, this, limbIndex, dList);

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        MtxF sp14C;
        Actor* hookedActor;

        Math_Vec3f_Copy(&this->leftHandPos, D_80160000);

        if (this->itemAction == PLAYER_IA_DEKU_STICK) {
            Vec3f sp124[3];

            OPEN_DISPS(play->state.gfxCtx);

            if (this->actor.scale.y >= 0.0f) {
                D_80126080.x = this->unk_85C * 5000.0f;
                func_80090A28(this, sp124);
                if (this->meleeWeaponState != 0) {
                    EffectBlureShip_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex), TRAIL_TYPE_STICK);
                    func_800906D4(play, this, sp124);
                } else {
                    Math_Vec3f_Copy(&this->meleeWeaponInfo[0].tip, &sp124[0]);
                }
            }

            Matrix_Translate(-428.26f, 267.2f, -33.82f, MTXMODE_APPLY);
            Matrix_RotateZYX(-0x8000, 0, 0x4000, MTXMODE_APPLY);
            Matrix_Scale(1.0f, this->unk_85C, 1.0f, MTXMODE_APPLY);

            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, gLinkChildLinkDekuStickDL);

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->meleeWeaponState != 0) && !mergeShipGoronFists &&
                   !MergeShip_Zora_SkipSwordQuads(this)) { // MergeShip2: the Zora's colliders are on its fins
            Vec3f spE4[3];

            if (Player_HoldsBrokenKnife(this)) {
                D_80126080.x = 1500.0f;
            } else {
                // MergeShip2: an MM sword on adult Link is drawn scaled by its fit; its reach scales with it.
                D_80126080.x =
                    sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)] * MergeShip_MMEquip_MeleeLengthScale(this);
                EffectBlureShip_ChangeType(Effect_GetByIndex(this->meleeWeaponEffectIndex),
                                           sSwordTypes[Player_GetMeleeWeaponHeld(this)]);
            }

            func_80090A28(this, spE4);
            func_800906D4(play, this, spE4);
        } else if ((this->actor.scale.y >= 0.0f) && (this->mergeShipForm == PLAYER_FORM_FIERCE_DEITY)) {
            // MergeShip2: MM Player_PostLimbDrawGameplay's Fierce Deity branch -- the sword's tip and base are
            // tracked every frame (MM func_8012669C with PLAYER_MELEE_WEAPON_STATE_0), which the Z-target
            // sparkles (MM func_80844D80) are drawn along.
            Vec3f spE4[3];
            Vec3f base;

            D_80126080.x = sMeleeWeaponLengths[Player_GetMeleeWeaponHeld(this)];
            func_80090A28(this, spE4);
            Matrix_MultVec3f(&D_801260A4[0], &base);
            Math_Vec3f_Copy(&this->meleeWeaponInfo[0].tip, &spE4[0]);
            Math_Vec3f_Copy(&this->meleeWeaponInfo[0].base, &base);
        } else if ((*dList != NULL) && (this->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE) &&
                   !MERGESHIP_FORM_IS_MM(this->mergeShipForm)) { // MergeShip2: a form's bottle is drawn below
            Color_RGB8* bottleColor = &sBottleColors[Player_ActionToBottle(this, this->itemAction)];

            OPEN_DISPS(play->state.gfxCtx);

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            if (GameInteractor_Should(VB_PLAYER_DRAW_BOTTLE, true, this, play)) {
                gDPSetEnvColor(POLY_XLU_DISP++, bottleColor->r, bottleColor->g, bottleColor->b, 0);
                gSPDisplayList(POLY_XLU_DISP++, sBottleDLists[gSaveContext.linkAge]);
            }

            CLOSE_DISPS(play->state.gfxCtx);
        }

        // MergeShip2: MM func_80128640 -- a form holds MM's bottle at its own hand offset, whatever else the hand
        // does (the Fierce Deity's sword tracking above runs as well, as in MM).
        if ((this->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE) && MERGESHIP_FORM_IS_MM(this->mergeShipForm) &&
            GameInteractor_Should(VB_PLAYER_DRAW_BOTTLE, true, this, play)) {
            s32 bottle = Player_ActionToBottle(this, this->itemAction);

            if (bottle < 0) {
                bottle = 0;
            }
            MergeShip_FormLib_DrawBottle(play, this, &sBottleColors[bottle], bottle == 0);
        }

        if (this->actor.scale.y >= 0.0f) {
            if (!Player_HoldsHookshot(this) && ((hookedActor = this->heldActor) != NULL)) {
                // MergeShip2: MM -- the Deku's bubble sits at its mouth (MergeShip_Deku_PostLimbDraw), not in hand.
                if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->mergeShipForm != PLAYER_FORM_DEKU)) {
                    Matrix_MultVec3f(MergeShip_MMEquip_ArrowOffset(play, this, &sLeftHandArrowVec3), // MergeShip2
                                     &hookedActor->world.pos);
                    Matrix_RotateZYX(0x69E8, -0x5708, 0x458E, MTXMODE_APPLY);
                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &hookedActor->world.rot, 0);
                    hookedActor->shape.rot = hookedActor->world.rot;
                } else if (this->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) {
                    Vec3s spB8;

                    Matrix_Get(&sp14C);
                    Matrix_MtxFToYXZRotS(&sp14C, &spB8, 0);

                    if (hookedActor->flags & ACTOR_FLAG_CARRY_X_ROT_INFLUENCE) {
                        hookedActor->world.rot.x = hookedActor->shape.rot.x = spB8.x - this->unk_3BC.x;
                    } else {
                        hookedActor->world.rot.y = hookedActor->shape.rot.y = this->actor.shape.rot.y + this->unk_3BC.y;
                    }
                }
            } else {
                Matrix_Get(&this->mf_9E0);
                Matrix_MtxFToYXZRotS(&this->mf_9E0, &this->unk_3BC, 0);
            }
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        Actor* heldActor = this->heldActor;

        if (this->rightHandType == PLAYER_MODELTYPE_RH_FF) {
            Matrix_Get(&this->shieldMf);
        } else if (((this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT) ||
                    (this->rightHandType == PLAYER_MODELTYPE_RH_BOW_SLINGSHOT_2)) &&
                   !MergeShip_MMEquip_DrawBowString(play, this, D_80160000)) { // MergeShip2: the Hero's Bow's own
            s32 stringModelToUse = gSaveContext.linkAge;
            if (CVarGetInteger(CVAR_ENHANCEMENT("BowSlingshotAmmoFix"), 0) ||
                CVarGetInteger(CVAR_ENHANCEMENT("EquipmentAlwaysVisible"), 0)) {
                stringModelToUse = Player_HoldsSlingshot(this);
            }
            BowStringData* stringData = &sBowStringData[stringModelToUse];

            OPEN_DISPS(play->state.gfxCtx);

            Matrix_Push();
            MergeShip_MMChildLink_FitAdultBowString(this, stringModelToUse); // MergeShip2: the adult's bow, fitted
            Matrix_Translate(stringData->pos.x, stringData->pos.y, stringData->pos.z, MTXMODE_APPLY);

            if ((this->stateFlags1 & PLAYER_STATE1_READY_TO_FIRE) && (this->unk_860 >= 0) && (this->unk_834 <= 10)) {
                Vec3f sp90;
                f32 distXYZ;

                Matrix_MultVec3f(&sZeroVec, &sp90);
                distXYZ = Math_Vec3f_DistXYZ(D_80160000, &sp90);

                this->unk_858 = distXYZ - 3.0f;
                if (distXYZ < 3.0f) {
                    this->unk_858 = 0.0f;
                } else {
                    this->unk_858 *= 1.6f;
                    if (this->unk_858 > 1.0f) {
                        this->unk_858 = 1.0f;
                    }
                }

                this->unk_85C = -0.5f;
            }

            Matrix_Scale(1.0f, this->unk_858, 1.0f, MTXMODE_APPLY);

            if (!LINK_IS_ADULT) {
                Matrix_RotateZ(this->unk_858 * -0.2f, MTXMODE_APPLY);
            }

            gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_XLU_DISP++, stringData->dList);

            Matrix_Pop();

            CLOSE_DISPS(play->state.gfxCtx);
        } else if ((this->actor.scale.y >= 0.0f) && (this->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) {
            Matrix_Get(&this->shieldMf);
            Player_UpdateShieldCollider(play, this, &this->shieldQuad, sRightHandLimbModelShieldQuadVertices);
        }

        if (this->actor.scale.y >= 0.0f) {
            if (GameInteractor_Should(VB_DRAW_ADDITIONAL_RETICLES,
                                      (this->heldItemAction == PLAYER_IA_HOOKSHOT) ||
                                          (this->heldItemAction == PLAYER_IA_LONGSHOT),
                                      this)) {
                // MergeShip2: MM's Hookshot's hand point and hook (MM D_801C0DD8 / D_801C0DE4).
                Matrix_MultVec3f(MergeShip_MMEquip_HookshotOffset(this, &D_80126184, false), &this->unk_3C8);

                if (heldActor != NULL) {
                    MtxF sp44;
                    s32 pad;

                    Matrix_MultVec3f(MergeShip_MMEquip_HookshotOffset(this, &D_80126190, true), &heldActor->world.pos);
                    Matrix_RotateZYX(0, -0x4000, -0x4000, MTXMODE_APPLY);
                    Matrix_Get(&sp44);
                    Matrix_MtxFToYXZRotS(&sp44, &heldActor->world.rot, 0);
                    heldActor->shape.rot = heldActor->world.rot;

                    if (func_8002DD78(this) != 0) {
                        Matrix_Translate(500.0f, 300.0f, 0.0f, MTXMODE_APPLY);
                        Player_DrawHookshotReticle(
                            play, this,
                            MergeShip_MMEquip_HookshotReticleRange( // MergeShip2: MM's Hookshot reaches 77600
                                this, ((this->heldItemAction == PLAYER_IA_HOOKSHOT) ? 38600.0f : 77600.0f) *
                                          CVarGetFloat(CVAR_CHEAT("HookshotReachMultiplier"), 1.0f)));
                    }
                }
            }

            if ((this->unk_862 != 0) || ((func_8002DD6C(this) == 0) && (heldActor != NULL))) {
                if (!(this->stateFlags1 & PLAYER_STATE1_GETTING_ITEM) && (this->unk_862 != 0) &&
                    (this->exchangeItemId != EXCH_ITEM_NONE)) {
                    Math_Vec3f_Copy(&sGetItemRefPos, &this->leftHandPos);
                } else {
                    sGetItemRefPos.x = (this->bodyPartsPos[15].x + this->leftHandPos.x) * 0.5f;
                    sGetItemRefPos.y = (this->bodyPartsPos[15].y + this->leftHandPos.y) * 0.5f;
                    sGetItemRefPos.z = (this->bodyPartsPos[15].z + this->leftHandPos.z) * 0.5f;
                }

                if (this->unk_862 == 0) {
                    Math_Vec3f_Copy(&heldActor->world.pos, &sGetItemRefPos);
                }
            }
        }
    } else if (this->actor.scale.y >= 0.0f) {
        if (limbIndex == PLAYER_LIMB_SHEATH) {
            if ((this->rightHandType != PLAYER_MODELTYPE_RH_SHIELD) &&
                (this->rightHandType != PLAYER_MODELTYPE_RH_FF)) {
                if (Player_IsChildWithHylianShield(this)) {
                    Player_UpdateShieldCollider(play, this, &this->shieldQuad, sSheathLimbModelShieldQuadVertices);
                }

                Matrix_TranslateRotateZYX(&sSheathLimbModelShieldOnBackPos, &sSheathLimbModelShieldOnBackZyxRot);
                Matrix_Get(&this->shieldMf);
            }
        } else if (limbIndex == PLAYER_LIMB_HEAD) {
            Matrix_MultVec3f(&sPlayerFocusOffsetFromHead, &this->actor.focus.pos);
        } else {
            Vec3f* vec = &sLeftRightFootLimbModelFootPos[(gSaveContext.linkAge)];

            // MergeShip2: MM sPlayerFootPos -- a form's feet (and so its shadow) sit where MM puts them.
            if (MergeShip_FormLib_FootPos(this) != NULL) {
                vec = MergeShip_FormLib_FootPos(this);
            }

            Actor_SetFeetPos(&this->actor, limbIndex, PLAYER_LIMB_L_FOOT, vec, PLAYER_LIMB_R_FOOT, vec);
        }
    }

    // MergeShip2: MM Player_PostLimbDrawGameplay's Deku parts (petals, guard shell, pipes, bubble, spin trail),
    // and MM's func_8012536C, which ends every post-limb draw.
    MergeShip_Deku_PostLimbDraw(play, this, limbIndex, dList, D_80160000);
}

u32 func_80091738(PlayState* play, u8* segment, SkelAnime* skelAnime) {
    s16 linkObjectId = gLinkObjectIds[gSaveContext.linkAge];
    size_t size;
    void* ptr;

    size = gObjectTable[OBJECT_GAMEPLAY_KEEP].vromEnd - gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart;
    ptr = segment + 0x3800;
    DmaMgr_SendRequest1(ptr, gObjectTable[OBJECT_GAMEPLAY_KEEP].vromStart, size, __FILE__, __LINE__);

    size = gObjectTable[linkObjectId].vromEnd - gObjectTable[linkObjectId].vromStart;
    ptr = segment + 0x8800;
    DmaMgr_SendRequest1(ptr, gObjectTable[linkObjectId].vromStart, size, __FILE__, __LINE__);

    ptr = (void*)ALIGN16((intptr_t)ptr + size);

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    // MergeShip2 (soh_fd): the equipment screen's Link is the form the player is in.
    SkelAnime_InitLink(play, skelAnime, gPlayerSkelHeaders[MergeShip_SohFd_PauseForm()], &gPlayerAnim_link_normal_wait,
                       9, ptr, ptr, PLAYER_LIMB_MAX);

    return size + 0x8800 + 0x90;
}

u8 sPauseModelGroupBySword[] = {
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_KOKIRI
    PLAYER_MODELGROUP_SWORD_AND_SHIELD, // PLAYER_SWORD_MASTER
    PLAYER_MODELGROUP_BGS,              // PLAYER_SWORD_BIGGORON
};

s32 Player_OverrideLimbDrawPause(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot, void* arg) {
    u8* playerSwordAndShield = arg;
    // SOH: Ensure positive value from playerSwordAndShield[] to avoid OOB array access.
    //      This can occur in the case where playerSwordAndShield[0] is PLAYER_SWORD_NONE
    u8 modelGroup =
        sPauseModelGroupBySword[playerSwordAndShield[0] > 0 ? playerSwordAndShield[0] - PLAYER_SWORD_KOKIRI : 0];
    s32 type;
    s32 dListOffset = 0;
    Gfx** dLists;
    const size_t entriesPerModel = 2 * PLAYER_FORM_MAX;
    u8 form = MergeShip_SohFd_PauseForm(); // MergeShip2 (soh_fd): the form on the equipment screen

    // MergeShip2: MM's young Link's forearms and hands, for the gauntlet plates Player_DrawImpl puts on them.
    MergeShip_MMChildLink_CaptureArm(play, gMergeShipMMChildLink && (form == PLAYER_FORM_CHILD), limbIndex, pos, rot);

    if ((modelGroup == PLAYER_MODELGROUP_SWORD_AND_SHIELD) && (form == PLAYER_FORM_CHILD) &&
        (playerSwordAndShield[1] == PLAYER_SHIELD_HYLIAN) &&
        !MergeShip_MMEquip_ChildHoldsHeroShield()) { // MergeShip2: the Hero's Shield is held in front
        modelGroup = PLAYER_MODELGROUP_CHILD_HYLIAN_SHIELD;
    }

    if (limbIndex == PLAYER_LIMB_L_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_LEFT_HAND];
        sLeftHandType = type;

        // SOH: Handle unexpected swordless case. Previously OOB array access is avoided, but we want the
        //      hand model-type to be set to open (otherwise it is set to holding sword model-type)
        if (playerSwordAndShield[0] == PLAYER_SWORD_NONE) {
            type = PLAYER_MODELTYPE_LH_OPEN;
        }

        if ((type == PLAYER_MODELTYPE_LH_BGS) && (gSaveContext.swordHealth <= 0.0f)) {
            dListOffset = entriesPerModel;
        }
        // MergeShip2 (soh_fd): the Fierce Deity holds his own sword, whichever Link has equipped.
        if (form == PLAYER_FORM_FIERCE_DEITY) {
            type = PLAYER_MODELTYPE_LH_BGS;
            dListOffset = 0;
        }
    } else if (limbIndex == PLAYER_LIMB_R_HAND) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_RIGHT_HAND];
        sRightHandType = type;
        if (type == PLAYER_MODELTYPE_RH_SHIELD) {
            dListOffset = playerSwordAndShield[1] * entriesPerModel;
        }
    } else if (limbIndex == PLAYER_LIMB_SHEATH) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_SHEATH];
        if ((type == PLAYER_MODELTYPE_SHEATH_18) || (type == PLAYER_MODELTYPE_SHEATH_19)) {
            dListOffset = playerSwordAndShield[1] * entriesPerModel;
        }
    } else if (limbIndex == PLAYER_LIMB_WAIST) {
        type = gPlayerModelTypes[modelGroup][PLAYER_MODELGROUPENTRY_WAIST];

        if (Player_IsCustomLinkModel()) {
            return 0;
        }
    } else {
        return 0;
    }

    dLists = &sPlayerDListGroups[type][form];
    *dList = dLists[dListOffset];

    // MergeShip2: MM's equipment switched to in the pause screen; otherwise MM's human wearing OoT's gear.
    // MergeShip2 (soh_fd): neither is for a form on the equipment screen, which keeps its own hands and waist.
    if (!MERGESHIP_FORM_IS_MM(form) &&
        !MergeShip_MMEquip_OverrideLimbPause(play, limbIndex, type, dList, playerSwordAndShield)) {
        MergeShip_MMChildLink_OverrideLimbPause(play, limbIndex, type, dList, pos, playerSwordAndShield);
    }

    GameInteractor_Should(VB_PLAYER_OVERRIDE_LIMB_DRAW_PAUSE, true, limbIndex, dList, GET_PLAYER(play), play);

    return 0;
}

#include <overlays/actors/ovl_Demo_Effect/z_demo_effect.h>
void DemoEffect_DrawTriforceSpot(Actor* thisx, PlayState* play);

void Pause_DrawTriforceSpot(PlayState* play, s32 showLightColumn) {
    static DemoEffect triforce;
    static s16 rotation = 0;

    triforce.triforceSpot.crystalLightOpacity = 244;
    triforce.triforceSpot.triforceSpotOpacity = 249;
    triforce.triforceSpot.lightColumnOpacity = showLightColumn ? 244 : 0;
    triforce.triforceSpot.rotation = rotation;

    DemoEffect_DrawTriforceSpot(&triforce, play);

    rotation += 0x03E8;
}

void Player_DrawPauseImpl(PlayState* play, void* gameplayKeep, void* linkObject, SkelAnime* skelAnime, Vec3f* pos,
                          Vec3s* rot, f32 scale, s32 sword, s32 tunic, s32 shield, s32 boots, s32 width, s32 height,
                          Vec3f* eye, Vec3f* at, f32 fovy, void* colorFrameBuffer, void* depthFrameBuffer) {
    // Note: the viewport x and y values are overwritten below, before usage
    static Vp viewport = { (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0,
                           (PAUSE_EQUIP_PLAYER_WIDTH / 2) << 2, (PAUSE_EQUIP_PLAYER_HEIGHT / 2) << 2, G_MAXZ / 2, 0 };
    static Lights1 lights1 = gdSPDefLights1(80, 80, 80, 255, 255, 255, 84, 84, -84);
    static Vec3f lightDir = { 89.8f, 0.0f, 89.8f };
    u8 playerSwordAndShield[2];
    Gfx* opaRef;
    Gfx* xluRef;
    u16 perspNorm;
    Mtx* perspMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));
    Mtx* lookAtMtx = Graph_Alloc(play->state.gfxCtx, sizeof(Mtx));

    u8 mirrorWorldActive = CVarGetInteger(CVAR_ENHANCEMENT("MirroredWorld"), 0);

    OPEN_DISPS(play->state.gfxCtx);

    opaRef = POLY_OPA_DISP;
    POLY_OPA_DISP++;

    xluRef = POLY_XLU_DISP;
    POLY_XLU_DISP++;

    gSPDisplayList(WORK_DISP++, POLY_OPA_DISP);
    gSPDisplayList(WORK_DISP++, POLY_XLU_DISP);

    if (mirrorWorldActive) {
        gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPSegment(POLY_OPA_DISP++, 0x00, NULL);

    gDPPipeSync(POLY_OPA_DISP++);

    gSPLoadGeometryMode(POLY_OPA_DISP++, 0);
    gSPTexture(POLY_OPA_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
    gDPSetOtherMode(POLY_OPA_DISP++,
                    G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_FILL | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2);

    // Also matches if some of the previous graphics commands are moved inside this block too. Possible macro?
    if (1) {
        s32 pad[2];

        gSPLoadGeometryMode(POLY_OPA_DISP++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_LIGHTING | G_SHADING_SMOOTH);
    }

    gDPSetScissor(POLY_OPA_DISP++, G_SC_NON_INTERLACE, 0, 0, width, height);
    gSPClipRatio(POLY_OPA_DISP++, FRUSTRATIO_1);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, depthFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_ZDZ(G_MAXFBZ, 0) << 16) | GPACK_ZDZ(G_MAXFBZ, 0));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetColorImage(POLY_OPA_DISP++, G_IM_FMT_RGBA, G_IM_SIZ_16b, width, colorFrameBuffer);
    gDPSetCycleType(POLY_OPA_DISP++, G_CYC_FILL);
    gDPSetRenderMode(POLY_OPA_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(POLY_OPA_DISP++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(POLY_OPA_DISP++, 0, 0, width - 1, height - 1);

    gDPPipeSync(POLY_OPA_DISP++);

    gDPSetDepthImage(POLY_OPA_DISP++, depthFrameBuffer);

    viewport.vp.vscale[0] = viewport.vp.vtrans[0] = width * ((1 << 2) / 2);
    viewport.vp.vscale[1] = viewport.vp.vtrans[1] = height * ((1 << 2) / 2);
    gSPViewport(POLY_OPA_DISP++, &viewport);

    guPerspective(perspMtx, &perspNorm, fovy, (f32)width / (f32)height, 10.0f, 4000.0f, 1.0f);

    gSPPerspNormalize(POLY_OPA_DISP++, perspNorm);
    gSPMatrix(POLY_OPA_DISP++, perspMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);

    guLookAt(lookAtMtx, eye->x, eye->y, eye->z, at->x, at->y, at->z, 0.0f, 1.0f, 0.0f);

    gSPMatrix(POLY_OPA_DISP++, lookAtMtx, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

    playerSwordAndShield[0] = sword;
    playerSwordAndShield[1] = shield;

    // MergeShip2 (soh_fd): the form on the equipment screen stands where it should for its own skeleton.
    Matrix_SetTranslateRotateYXZ(
        pos->x - (MergeShip_SohFd_PauseShiftsLeft(MergeShip_SohFd_PauseForm(),
                                                  CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0))
                      ? 25
                      : 0),
        pos->y - (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0) ? 16 : 0) -
            MergeShip_SohFd_PauseDropY(MergeShip_SohFd_PauseForm(),
                                       CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) ||
                                           CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)),
        pos->z, rot);
    Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale, scale, MTXMODE_APPLY);

    gSPSegment(POLY_OPA_DISP++, 0x04, gameplayKeep);
    gSPSegment(POLY_OPA_DISP++, 0x06, linkObject);

    gSPSetLights1(POLY_OPA_DISP++, lights1);

    func_80093C80(play);

    POLY_OPA_DISP = Gfx_SetFog2(POLY_OPA_DISP++, 0, 0, 0, 0, 997, 1000);

    func_8002EABC(pos, &play->view.eye, &lightDir, play->state.gfxCtx);

    gSPSegment(POLY_OPA_DISP++, 0x0C, gCullBackDList);

    Player_DrawImpl(play, skelAnime->skeleton, skelAnime->jointTable, skelAnime->dListCount, 0, tunic, boots, 0,
                    Player_OverrideLimbDrawPause, NULL, &playerSwordAndShield);

    if (CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        Matrix_SetTranslateRotateYXZ(pos->x - (LINK_AGE_IN_YEARS == YEARS_ADULT ? 25 : 0),
                                     pos->y + 280 + (LINK_AGE_IN_YEARS == YEARS_ADULT ? 48 : 0), pos->z, rot);
        Matrix_Scale(scale * (mirrorWorldActive ? -1 : 1), scale * 1, scale * 1, MTXMODE_APPLY);

        Pause_DrawTriforceSpot(play, 1);
    }

    if (mirrorWorldActive) {
        gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    }

    gSPEndDisplayList(POLY_OPA_DISP++);
    gSPEndDisplayList(POLY_XLU_DISP++);

    gSPBranchList(opaRef, POLY_OPA_DISP);
    gSPBranchList(xluRef, POLY_XLU_DISP);

    CLOSE_DISPS(play->state.gfxCtx);
}

void Player_DrawPause(PlayState* play, u8* segment, SkelAnime* skelAnime, Vec3f* pos, Vec3s* rot, f32 scale, s32 sword,
                      s32 tunic, s32 shield, s32 boots) {
    Input* p1Input = &play->state.input[0];
    Vec3f eye = { 0.0f, 0.0f, -400.0f };
    Vec3f at = { 0.0f, 0.0f, 0.0f };
    Vec3s* destTable;
    Vec3s* srcTable;
    s32 i;
    bool canswitchrnd = false;

    gSegments[4] = VIRTUAL_TO_PHYSICAL(segment + 0x3800);
    gSegments[6] = VIRTUAL_TO_PHYSICAL(segment + 0x8800);

    uintptr_t* PauseMenuAnimSet[4] = { // IDLE                       // Two Handed                       // No shield //
                                       // Kid Hylian Shield
                                       gPlayerAnim_link_normal_wait, gPlayerAnim_link_fighter_wait_long,
                                       gPlayerAnim_link_normal_wait_free, gPlayerAnim_link_normal_wait_free
    };
    u8 pauseForm = MergeShip_SohFd_PauseForm(); // MergeShip2 (soh_fd): the form on the equipment screen

    // MergeShip2 (soh_fd): the Deku, Goron and Zora hold no sword or shield.
    if ((pauseForm == PLAYER_FORM_DEKU) || (pauseForm == PLAYER_FORM_GORON) || (pauseForm == PLAYER_FORM_ZORA)) {
        sword = 0;
        shield = 0;
    }

    if (((pauseForm == PLAYER_FORM_GORON) || (pauseForm == PLAYER_FORM_ZORA)) &&
        !CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        // MergeShip2 (soh_fd): the Goron and the Zora stand in their own idle (MM Player_GetIdleAnim) -- still, or
        // breathing with the animated pause option. Ocarina of Time's pause poses are made for Link's skeleton.
        LinkAnimationHeader* idle = (pauseForm == PLAYER_FORM_GORON) ? (LinkAnimationHeader*)mm_gPlayerAnim_pg_wait
                                                                     : (LinkAnimationHeader*)mm_gPlayerAnim_pz_wait;
        f32 idleSpeed = CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) ? 1.0f : 0.0f;

        if (skelAnime->animation != idle) {
            LinkAnimation_Change(play, skelAnime, idle, idleSpeed, 0.0f, Animation_GetLastFrame(idle), ANIMMODE_LOOP,
                                 0.0f);
        }
        skelAnime->playSpeed = idleSpeed;
        LinkAnimation_Update(play, skelAnime);
    } else if (CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 0) ||
               CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
        uintptr_t anim = 0; // Initialise anim

        s16 EquipedStance;
        if ((pauseForm == PLAYER_FORM_DEKU) || (pauseForm == PLAYER_FORM_FIERCE_DEITY)) {
            // MergeShip2 (soh_fd): the Deku idles empty-handed; the Fierce Deity keeps his sword and holds no shield.
            EquipedStance = 2;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_BIGGORON) {
            EquipedStance = 1;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_NONE) {
            EquipedStance = 2;
        } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SWORD_MASTER && LINK_AGE_IN_YEARS == YEARS_CHILD) {
            EquipedStance = 3;
        } else {
            // Link is idle so revert to 0
            EquipedStance = 0;
        }

        if (!CVarGetInteger(CVAR_GENERAL("PauseMenuAnimatedLinkTriforce"), 0)) {
            anim = PauseMenuAnimSet[EquipedStance];
        } else {
            anim = gPlayerAnim_link_magic_kaze2;
            sword = 0;
            shield = 0;
        }

        if (skelAnime->animation != anim) {
            LinkAnimation_Change(play, skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -6.0f);
        }

        LinkAnimation_Update(play, skelAnime);

        // MergeShip2 (soh_fd): as for the child, for the Deku and the Fierce Deity (soh_fd's placement).
        if ((pauseForm == PLAYER_FORM_CHILD) || (pauseForm == PLAYER_FORM_DEKU) ||
            (pauseForm == PLAYER_FORM_FIERCE_DEITY)) {
            // Link is placed too far up by default when animating
            at.y += 60;
        }
    } else {

        // MergeShip2 (soh_fd): the Deku and the Fierce Deity take the adult's pose.
        if (pauseForm == PLAYER_FORM_CHILD) {
            if (shield == PLAYER_SHIELD_DEKU) {
                srcTable = gLinkPauseChildDekuShieldJointTable;
            } else {
                srcTable = gLinkPauseChildJointTable;
            }
        } else {
            if (sword == PLAYER_SWORD_BIGGORON) {
                srcTable = gLinkPauseAdultBgsJointTable;
            } else if (shield != PLAYER_SHIELD_NONE) {
                srcTable = gLinkPauseAdultShieldJointTable;
            } else {
                srcTable = gLinkPauseAdultJointTable;
            }
        }

        srcTable = ResourceMgr_LoadArrayByNameAsVec3s(srcTable);
        Vec3s* ogSrcTable = srcTable;
        destTable = skelAnime->jointTable;
        for (i = 0; i < skelAnime->limbCount; i++) {
            *destTable++ = *srcTable++;
        }
        free(ogSrcTable);
    }

    Player_DrawPauseImpl(play, segment + 0x3800, segment + 0x8800, skelAnime, pos, rot, scale, sword, tunic, shield,
                         boots, PAUSE_EQUIP_PLAYER_WIDTH, PAUSE_EQUIP_PLAYER_HEIGHT, &eye, &at, 60.0f,
                         play->state.gfxCtx->curFrameBuffer,
                         play->state.gfxCtx->curFrameBuffer + (PAUSE_EQUIP_PLAYER_WIDTH * PAUSE_EQUIP_PLAYER_HEIGHT));
}

// MergeShip2: Majora's Mask's Zora drawing (see the file).
#include "soh/MergeShip/forms/MergeShipZoraDraw.inc"
// MergeShip2: Majora's Mask's human drawn as the child (see the file).
#include "soh/MergeShip/MergeShipChildLinkDraw.inc"
// MergeShip2: Majora's Mask's swords, shields, bow and hookshot drawn on Link (see the file).
#include "soh/MergeShip/MergeShipMMEquipmentDraw.inc"
