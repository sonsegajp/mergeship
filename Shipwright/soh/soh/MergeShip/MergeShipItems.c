#include "global.h"
#include "soh/MergeShip/MergeShipItems.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipHud.h"
#include "soh/MergeShip/MergeShipDekuBeans.h"
#include "soh/MergeShip/MergeShipPlayerExt.h"
#include "soh/MergeShip/MergeShipMaskItems.h"
#include "soh/MergeShip/MergeShipChateau.h" // MergeShip2: MM's Chateau Romani, a bottle
#include "soh/MergeShip/MergeShipMMItems.h" // MergeShip2: MM's keg, box, bottle contents, trade items
#include "soh/MergeShip/MergeShipSohFd.h"
#include "soh/MergeShip/MergeShipMMEquipment.h" // MergeShip2 (BUG-0143): MERGESHIP_ITEM_IS_MM_SWORD
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

// MergeShip2 -- Majora's Mask's per-form item rules and B-button states, see MergeShipItems.h.
//
// Sources: 2ship2harkinian/mm/src/code/z_parameter.c (gPlayerFormItemRestrictions,
// Interface_UpdateButtonsPart2, Interface_DrawBButtonIcons, Interface_SetBButtonPlayerDoAction).

// ---- MM's item ids, as they index gPlayerFormItemRestrictions (MM z64item.h ItemId 0x00-0x71). ----
// Generated from the table's own comments, so the rows below are MM's verbatim.
typedef enum MMItemId {
    /* 0x00 */ MM_ITEM_OCARINA_OF_TIME,
    /* 0x01 */ MM_ITEM_BOW,
    /* 0x02 */ MM_ITEM_ARROW_FIRE,
    /* 0x03 */ MM_ITEM_ARROW_ICE,
    /* 0x04 */ MM_ITEM_ARROW_LIGHT,
    /* 0x05 */ MM_ITEM_OCARINA_FAIRY,
    /* 0x06 */ MM_ITEM_BOMB,
    /* 0x07 */ MM_ITEM_BOMBCHU,
    /* 0x08 */ MM_ITEM_DEKU_STICK,
    /* 0x09 */ MM_ITEM_DEKU_NUT,
    /* 0x0A */ MM_ITEM_MAGIC_BEANS,
    /* 0x0B */ MM_ITEM_SLINGSHOT,
    /* 0x0C */ MM_ITEM_POWDER_KEG,
    /* 0x0D */ MM_ITEM_PICTOGRAPH_BOX,
    /* 0x0E */ MM_ITEM_LENS_OF_TRUTH,
    /* 0x0F */ MM_ITEM_HOOKSHOT,
    /* 0x10 */ MM_ITEM_SWORD_GREAT_FAIRY,
    /* 0x11 */ MM_ITEM_LONGSHOT,
    /* 0x12 */ MM_ITEM_BOTTLE,
    /* 0x13 */ MM_ITEM_POTION_RED,
    /* 0x14 */ MM_ITEM_POTION_GREEN,
    /* 0x15 */ MM_ITEM_POTION_BLUE,
    /* 0x16 */ MM_ITEM_FAIRY,
    /* 0x17 */ MM_ITEM_DEKU_PRINCESS,
    /* 0x18 */ MM_ITEM_MILK_BOTTLE,
    /* 0x19 */ MM_ITEM_MILK_HALF,
    /* 0x1A */ MM_ITEM_FISH,
    /* 0x1B */ MM_ITEM_BUG,
    /* 0x1C */ MM_ITEM_BLUE_FIRE,
    /* 0x1D */ MM_ITEM_POE,
    /* 0x1E */ MM_ITEM_BIG_POE,
    /* 0x1F */ MM_ITEM_SPRING_WATER,
    /* 0x20 */ MM_ITEM_HOT_SPRING_WATER,
    /* 0x21 */ MM_ITEM_ZORA_EGG,
    /* 0x22 */ MM_ITEM_GOLD_DUST,
    /* 0x23 */ MM_ITEM_MUSHROOM,
    /* 0x24 */ MM_ITEM_SEAHORSE,
    /* 0x25 */ MM_ITEM_CHATEAU,
    /* 0x26 */ MM_ITEM_HYLIAN_LOACH,
    /* 0x27 */ MM_ITEM_OBABA_DRINK,
    /* 0x28 */ MM_ITEM_MOONS_TEAR,
    /* 0x29 */ MM_ITEM_DEED_LAND,
    /* 0x2A */ MM_ITEM_DEED_SWAMP,
    /* 0x2B */ MM_ITEM_DEED_MOUNTAIN,
    /* 0x2C */ MM_ITEM_DEED_OCEAN,
    /* 0x2D */ MM_ITEM_ROOM_KEY,
    /* 0x2E */ MM_ITEM_LETTER_MAMA,
    /* 0x2F */ MM_ITEM_LETTER_TO_KAFEI,
    /* 0x30 */ MM_ITEM_PENDANT_OF_MEMORIES,
    /* 0x31 */ MM_ITEM_TINGLE_MAP,
    /* 0x32 */ MM_ITEM_MASK_DEKU,
    /* 0x33 */ MM_ITEM_MASK_GORON,
    /* 0x34 */ MM_ITEM_MASK_ZORA,
    /* 0x35 */ MM_ITEM_MASK_FIERCE_DEITY,
    /* 0x36 */ MM_ITEM_MASK_TRUTH,
    /* 0x37 */ MM_ITEM_MASK_KAFEIS_MASK,
    /* 0x38 */ MM_ITEM_MASK_ALL_NIGHT,
    /* 0x39 */ MM_ITEM_MASK_BUNNY,
    /* 0x3A */ MM_ITEM_MASK_KEATON,
    /* 0x3B */ MM_ITEM_MASK_GARO,
    /* 0x3C */ MM_ITEM_MASK_ROMANI,
    /* 0x3D */ MM_ITEM_MASK_CIRCUS_LEADER,
    /* 0x3E */ MM_ITEM_MASK_POSTMAN,
    /* 0x3F */ MM_ITEM_MASK_COUPLE,
    /* 0x40 */ MM_ITEM_MASK_GREAT_FAIRY,
    /* 0x41 */ MM_ITEM_MASK_GIBDO,
    /* 0x42 */ MM_ITEM_MASK_DON_GERO,
    /* 0x43 */ MM_ITEM_MASK_KAMARO,
    /* 0x44 */ MM_ITEM_MASK_CAPTAIN,
    /* 0x45 */ MM_ITEM_MASK_STONE,
    /* 0x46 */ MM_ITEM_MASK_BREMEN,
    /* 0x47 */ MM_ITEM_MASK_BLAST,
    /* 0x48 */ MM_ITEM_MASK_SCENTS,
    /* 0x49 */ MM_ITEM_MASK_GIANT,
    /* 0x4A */ MM_ITEM_BOW_FIRE,
    /* 0x4B */ MM_ITEM_BOW_ICE,
    /* 0x4C */ MM_ITEM_BOW_LIGHT,
    /* 0x4D */ MM_ITEM_SWORD_KOKIRI,
    /* 0x4E */ MM_ITEM_SWORD_RAZOR,
    /* 0x4F */ MM_ITEM_SWORD_GILDED,
    /* 0x50 */ MM_ITEM_SWORD_DEITY,
    /* 0x51 */ MM_ITEM_SHIELD_HERO,
    /* 0x52 */ MM_ITEM_SHIELD_MIRROR,
    /* 0x53 */ MM_ITEM_QUIVER_30,
    /* 0x54 */ MM_ITEM_QUIVER_40,
    /* 0x55 */ MM_ITEM_QUIVER_50,
    /* 0x56 */ MM_ITEM_BOMB_BAG_20,
    /* 0x57 */ MM_ITEM_BOMB_BAG_30,
    /* 0x58 */ MM_ITEM_BOMB_BAG_40,
    /* 0x59 */ MM_ITEM_WALLET_DEFAULT,
    /* 0x5A */ MM_ITEM_WALLET_ADULT,
    /* 0x5B */ MM_ITEM_WALLET_GIANT,
    /* 0x5C */ MM_ITEM_FISHING_ROD,
    /* 0x5D */ MM_ITEM_REMAINS_ODOLWA,
    /* 0x5E */ MM_ITEM_REMAINS_GOHT,
    /* 0x5F */ MM_ITEM_REMAINS_GYORG,
    /* 0x60 */ MM_ITEM_REMAINS_TWINMOLD,
    /* 0x61 */ MM_ITEM_SONG_SONATA,
    /* 0x62 */ MM_ITEM_SONG_LULLABY,
    /* 0x63 */ MM_ITEM_SONG_NOVA,
    /* 0x64 */ MM_ITEM_SONG_ELEGY,
    /* 0x65 */ MM_ITEM_SONG_OATH,
    /* 0x66 */ MM_ITEM_SONG_SARIA,
    /* 0x67 */ MM_ITEM_SONG_TIME,
    /* 0x68 */ MM_ITEM_SONG_HEALING,
    /* 0x69 */ MM_ITEM_SONG_EPONA,
    /* 0x6A */ MM_ITEM_SONG_SOARING,
    /* 0x6B */ MM_ITEM_SONG_STORMS,
    /* 0x6C */ MM_ITEM_SONG_SUN,
    /* 0x6D */ MM_ITEM_BOMBERS_NOTEBOOK,
    /* 0x6E */ MM_ITEM_SKULL_TOKEN,
    /* 0x6F */ MM_ITEM_HEART_CONTAINER,
    /* 0x70 */ MM_ITEM_HEART_PIECE,
    /* 0x71 */ MM_ITEM_71,
    /* 0x72 */ MM_ITEM_RESTRICTION_COUNT
} MMItemId;

// MM gPlayerFormItemRestrictions (2S2H z_parameter.c), the four transformed forms' rows, in MM order.
static const u8 sMMFormItemRestrictions[MM_FORM_HUMAN][MM_ITEM_RESTRICTION_COUNT] = {
    // MM_FORM_FIERCE_DEITY
    {
        false, // MM_ITEM_OCARINA_OF_TIME
        false, // MM_ITEM_BOW
        false, // MM_ITEM_ARROW_FIRE
        false, // MM_ITEM_ARROW_ICE
        false, // MM_ITEM_ARROW_LIGHT
        false, // MM_ITEM_OCARINA_FAIRY
        false, // MM_ITEM_BOMB
        false, // MM_ITEM_BOMBCHU
        false, // MM_ITEM_DEKU_STICK
        false, // MM_ITEM_DEKU_NUT
        false, // MM_ITEM_MAGIC_BEANS
        false, // MM_ITEM_SLINGSHOT
        false, // MM_ITEM_POWDER_KEG
        false, // MM_ITEM_PICTOGRAPH_BOX
        false, // MM_ITEM_LENS_OF_TRUTH
        false, // MM_ITEM_HOOKSHOT
        false, // MM_ITEM_SWORD_GREAT_FAIRY
        false, // MM_ITEM_LONGSHOT
        true,  // MM_ITEM_BOTTLE
        true,  // MM_ITEM_POTION_RED
        true,  // MM_ITEM_POTION_GREEN
        true,  // MM_ITEM_POTION_BLUE
        true,  // MM_ITEM_FAIRY
        true,  // MM_ITEM_DEKU_PRINCESS
        true,  // MM_ITEM_MILK_BOTTLE
        true,  // MM_ITEM_MILK_HALF
        true,  // MM_ITEM_FISH
        true,  // MM_ITEM_BUG
        true,  // MM_ITEM_BLUE_FIRE
        true,  // MM_ITEM_POE
        true,  // MM_ITEM_BIG_POE
        true,  // MM_ITEM_SPRING_WATER
        true,  // MM_ITEM_HOT_SPRING_WATER
        true,  // MM_ITEM_ZORA_EGG
        true,  // MM_ITEM_GOLD_DUST
        true,  // MM_ITEM_MUSHROOM
        true,  // MM_ITEM_SEAHORSE
        true,  // MM_ITEM_CHATEAU
        true,  // MM_ITEM_HYLIAN_LOACH
        true,  // MM_ITEM_OBABA_DRINK
        false, // MM_ITEM_MOONS_TEAR
        false, // MM_ITEM_DEED_LAND
        false, // MM_ITEM_DEED_SWAMP
        false, // MM_ITEM_DEED_MOUNTAIN
        false, // MM_ITEM_DEED_OCEAN
        false, // MM_ITEM_ROOM_KEY
        false, // MM_ITEM_LETTER_MAMA
        false, // MM_ITEM_LETTER_TO_KAFEI
        false, // MM_ITEM_PENDANT_OF_MEMORIES
        false, // MM_ITEM_TINGLE_MAP
        false, // MM_ITEM_MASK_DEKU
        false, // MM_ITEM_MASK_GORON
        false, // MM_ITEM_MASK_ZORA
        true,  // MM_ITEM_MASK_FIERCE_DEITY
        false, // MM_ITEM_MASK_TRUTH
        false, // MM_ITEM_MASK_KAFEIS_MASK
        false, // MM_ITEM_MASK_ALL_NIGHT
        false, // MM_ITEM_MASK_BUNNY
        false, // MM_ITEM_MASK_KEATON
        false, // MM_ITEM_MASK_GARO
        false, // MM_ITEM_MASK_ROMANI
        false, // MM_ITEM_MASK_CIRCUS_LEADER
        false, // MM_ITEM_MASK_POSTMAN
        false, // MM_ITEM_MASK_COUPLE
        false, // MM_ITEM_MASK_GREAT_FAIRY
        false, // MM_ITEM_MASK_GIBDO
        false, // MM_ITEM_MASK_DON_GERO
        false, // MM_ITEM_MASK_KAMARO
        false, // MM_ITEM_MASK_CAPTAIN
        false, // MM_ITEM_MASK_STONE
        false, // MM_ITEM_MASK_BREMEN
        false, // MM_ITEM_MASK_BLAST
        false, // MM_ITEM_MASK_SCENTS
        false, // MM_ITEM_MASK_GIANT
        false, // MM_ITEM_BOW_FIRE
        false, // MM_ITEM_BOW_ICE
        false, // MM_ITEM_BOW_LIGHT
        false, // MM_ITEM_SWORD_KOKIRI
        false, // MM_ITEM_SWORD_RAZOR
        false, // MM_ITEM_SWORD_GILDED
        false, // MM_ITEM_SWORD_DEITY
        false, // MM_ITEM_SHIELD_HERO
        false, // MM_ITEM_SHIELD_MIRROR
        false, // MM_ITEM_QUIVER_30
        false, // MM_ITEM_QUIVER_40
        false, // MM_ITEM_QUIVER_50
        false, // MM_ITEM_BOMB_BAG_20
        false, // MM_ITEM_BOMB_BAG_30
        false, // MM_ITEM_BOMB_BAG_40
        false, // MM_ITEM_WALLET_DEFAULT
        false, // MM_ITEM_WALLET_ADULT
        false, // MM_ITEM_WALLET_GIANT
        false, // MM_ITEM_FISHING_ROD
        false, // MM_ITEM_REMAINS_ODOLWA
        false, // MM_ITEM_REMAINS_GOHT
        false, // MM_ITEM_REMAINS_GYORG
        false, // MM_ITEM_REMAINS_TWINMOLD
        false, // MM_ITEM_SONG_SONATA
        false, // MM_ITEM_SONG_LULLABY
        false, // MM_ITEM_SONG_NOVA
        false, // MM_ITEM_SONG_ELEGY
        false, // MM_ITEM_SONG_OATH
        false, // MM_ITEM_SONG_SARIA
        false, // MM_ITEM_SONG_TIME
        false, // MM_ITEM_SONG_HEALING
        false, // MM_ITEM_SONG_EPONA
        false, // MM_ITEM_SONG_SOARING
        false, // MM_ITEM_SONG_STORMS
        false, // MM_ITEM_SONG_SUN
        false, // MM_ITEM_BOMBERS_NOTEBOOK
        false, // MM_ITEM_SKULL_TOKEN
        false, // MM_ITEM_HEART_CONTAINER
        false, // MM_ITEM_HEART_PIECE
        false, // MM_ITEM_71
    },
    // MM_FORM_GORON
    {
        true,  // MM_ITEM_OCARINA_OF_TIME
        false, // MM_ITEM_BOW
        false, // MM_ITEM_ARROW_FIRE
        false, // MM_ITEM_ARROW_ICE
        false, // MM_ITEM_ARROW_LIGHT
        false, // MM_ITEM_OCARINA_FAIRY
        false, // MM_ITEM_BOMB
        false, // MM_ITEM_BOMBCHU
        false, // MM_ITEM_DEKU_STICK
        false, // MM_ITEM_DEKU_NUT
        false, // MM_ITEM_MAGIC_BEANS
        false, // MM_ITEM_SLINGSHOT
        true,  // MM_ITEM_POWDER_KEG
        true,  // MM_ITEM_PICTOGRAPH_BOX
        true,  // MM_ITEM_LENS_OF_TRUTH
        false, // MM_ITEM_HOOKSHOT
        false, // MM_ITEM_SWORD_GREAT_FAIRY
        false, // MM_ITEM_LONGSHOT
        true,  // MM_ITEM_BOTTLE
        true,  // MM_ITEM_POTION_RED
        true,  // MM_ITEM_POTION_GREEN
        true,  // MM_ITEM_POTION_BLUE
        true,  // MM_ITEM_FAIRY
        true,  // MM_ITEM_DEKU_PRINCESS
        true,  // MM_ITEM_MILK_BOTTLE
        true,  // MM_ITEM_MILK_HALF
        true,  // MM_ITEM_FISH
        true,  // MM_ITEM_BUG
        true,  // MM_ITEM_BLUE_FIRE
        true,  // MM_ITEM_POE
        true,  // MM_ITEM_BIG_POE
        true,  // MM_ITEM_SPRING_WATER
        true,  // MM_ITEM_HOT_SPRING_WATER
        true,  // MM_ITEM_ZORA_EGG
        true,  // MM_ITEM_GOLD_DUST
        true,  // MM_ITEM_MUSHROOM
        true,  // MM_ITEM_SEAHORSE
        true,  // MM_ITEM_CHATEAU
        true,  // MM_ITEM_HYLIAN_LOACH
        true,  // MM_ITEM_OBABA_DRINK
        true,  // MM_ITEM_MOONS_TEAR
        true,  // MM_ITEM_DEED_LAND
        true,  // MM_ITEM_DEED_SWAMP
        true,  // MM_ITEM_DEED_MOUNTAIN
        true,  // MM_ITEM_DEED_OCEAN
        true,  // MM_ITEM_ROOM_KEY
        true,  // MM_ITEM_LETTER_MAMA
        true,  // MM_ITEM_LETTER_TO_KAFEI
        true,  // MM_ITEM_PENDANT_OF_MEMORIES
        true,  // MM_ITEM_TINGLE_MAP
        true,  // MM_ITEM_MASK_DEKU
        true,  // MM_ITEM_MASK_GORON
        true,  // MM_ITEM_MASK_ZORA
        true,  // MM_ITEM_MASK_FIERCE_DEITY
        false, // MM_ITEM_MASK_TRUTH
        false, // MM_ITEM_MASK_KAFEIS_MASK
        false, // MM_ITEM_MASK_ALL_NIGHT
        false, // MM_ITEM_MASK_BUNNY
        false, // MM_ITEM_MASK_KEATON
        false, // MM_ITEM_MASK_GARO
        false, // MM_ITEM_MASK_ROMANI
        false, // MM_ITEM_MASK_CIRCUS_LEADER
        false, // MM_ITEM_MASK_POSTMAN
        false, // MM_ITEM_MASK_COUPLE
        false, // MM_ITEM_MASK_GREAT_FAIRY
        false, // MM_ITEM_MASK_GIBDO
        false, // MM_ITEM_MASK_DON_GERO
        false, // MM_ITEM_MASK_KAMARO
        false, // MM_ITEM_MASK_CAPTAIN
        false, // MM_ITEM_MASK_STONE
        false, // MM_ITEM_MASK_BREMEN
        false, // MM_ITEM_MASK_BLAST
        false, // MM_ITEM_MASK_SCENTS
        false, // MM_ITEM_MASK_GIANT
        false, // MM_ITEM_BOW_FIRE
        false, // MM_ITEM_BOW_ICE
        false, // MM_ITEM_BOW_LIGHT
        false, // MM_ITEM_SWORD_KOKIRI
        false, // MM_ITEM_SWORD_RAZOR
        false, // MM_ITEM_SWORD_GILDED
        false, // MM_ITEM_SWORD_DEITY
        false, // MM_ITEM_SHIELD_HERO
        false, // MM_ITEM_SHIELD_MIRROR
        false, // MM_ITEM_QUIVER_30
        false, // MM_ITEM_QUIVER_40
        false, // MM_ITEM_QUIVER_50
        false, // MM_ITEM_BOMB_BAG_20
        false, // MM_ITEM_BOMB_BAG_30
        false, // MM_ITEM_BOMB_BAG_40
        false, // MM_ITEM_WALLET_DEFAULT
        false, // MM_ITEM_WALLET_ADULT
        false, // MM_ITEM_WALLET_GIANT
        false, // MM_ITEM_FISHING_ROD
        false, // MM_ITEM_REMAINS_ODOLWA
        false, // MM_ITEM_REMAINS_GOHT
        false, // MM_ITEM_REMAINS_GYORG
        false, // MM_ITEM_REMAINS_TWINMOLD
        false, // MM_ITEM_SONG_SONATA
        false, // MM_ITEM_SONG_LULLABY
        false, // MM_ITEM_SONG_NOVA
        false, // MM_ITEM_SONG_ELEGY
        false, // MM_ITEM_SONG_OATH
        false, // MM_ITEM_SONG_SARIA
        false, // MM_ITEM_SONG_TIME
        false, // MM_ITEM_SONG_HEALING
        false, // MM_ITEM_SONG_EPONA
        false, // MM_ITEM_SONG_SOARING
        false, // MM_ITEM_SONG_STORMS
        false, // MM_ITEM_SONG_SUN
        false, // MM_ITEM_BOMBERS_NOTEBOOK
        false, // MM_ITEM_SKULL_TOKEN
        false, // MM_ITEM_HEART_CONTAINER
        false, // MM_ITEM_HEART_PIECE
        false, // MM_ITEM_71
    },
    // MM_FORM_ZORA
    {
        true,  // MM_ITEM_OCARINA_OF_TIME
        false, // MM_ITEM_BOW
        false, // MM_ITEM_ARROW_FIRE
        false, // MM_ITEM_ARROW_ICE
        false, // MM_ITEM_ARROW_LIGHT
        false, // MM_ITEM_OCARINA_FAIRY
        false, // MM_ITEM_BOMB
        false, // MM_ITEM_BOMBCHU
        false, // MM_ITEM_DEKU_STICK
        false, // MM_ITEM_DEKU_NUT
        false, // MM_ITEM_MAGIC_BEANS
        false, // MM_ITEM_SLINGSHOT
        false, // MM_ITEM_POWDER_KEG
        true,  // MM_ITEM_PICTOGRAPH_BOX
        true,  // MM_ITEM_LENS_OF_TRUTH
        false, // MM_ITEM_HOOKSHOT
        false, // MM_ITEM_SWORD_GREAT_FAIRY
        false, // MM_ITEM_LONGSHOT
        true,  // MM_ITEM_BOTTLE
        true,  // MM_ITEM_POTION_RED
        true,  // MM_ITEM_POTION_GREEN
        true,  // MM_ITEM_POTION_BLUE
        true,  // MM_ITEM_FAIRY
        true,  // MM_ITEM_DEKU_PRINCESS
        true,  // MM_ITEM_MILK_BOTTLE
        true,  // MM_ITEM_MILK_HALF
        true,  // MM_ITEM_FISH
        true,  // MM_ITEM_BUG
        true,  // MM_ITEM_BLUE_FIRE
        true,  // MM_ITEM_POE
        true,  // MM_ITEM_BIG_POE
        true,  // MM_ITEM_SPRING_WATER
        true,  // MM_ITEM_HOT_SPRING_WATER
        true,  // MM_ITEM_ZORA_EGG
        true,  // MM_ITEM_GOLD_DUST
        true,  // MM_ITEM_MUSHROOM
        true,  // MM_ITEM_SEAHORSE
        true,  // MM_ITEM_CHATEAU
        true,  // MM_ITEM_HYLIAN_LOACH
        true,  // MM_ITEM_OBABA_DRINK
        true,  // MM_ITEM_MOONS_TEAR
        true,  // MM_ITEM_DEED_LAND
        true,  // MM_ITEM_DEED_SWAMP
        true,  // MM_ITEM_DEED_MOUNTAIN
        true,  // MM_ITEM_DEED_OCEAN
        true,  // MM_ITEM_ROOM_KEY
        true,  // MM_ITEM_LETTER_MAMA
        true,  // MM_ITEM_LETTER_TO_KAFEI
        true,  // MM_ITEM_PENDANT_OF_MEMORIES
        true,  // MM_ITEM_TINGLE_MAP
        true,  // MM_ITEM_MASK_DEKU
        true,  // MM_ITEM_MASK_GORON
        true,  // MM_ITEM_MASK_ZORA
        true,  // MM_ITEM_MASK_FIERCE_DEITY
        false, // MM_ITEM_MASK_TRUTH
        false, // MM_ITEM_MASK_KAFEIS_MASK
        false, // MM_ITEM_MASK_ALL_NIGHT
        false, // MM_ITEM_MASK_BUNNY
        false, // MM_ITEM_MASK_KEATON
        false, // MM_ITEM_MASK_GARO
        false, // MM_ITEM_MASK_ROMANI
        false, // MM_ITEM_MASK_CIRCUS_LEADER
        false, // MM_ITEM_MASK_POSTMAN
        false, // MM_ITEM_MASK_COUPLE
        false, // MM_ITEM_MASK_GREAT_FAIRY
        false, // MM_ITEM_MASK_GIBDO
        false, // MM_ITEM_MASK_DON_GERO
        false, // MM_ITEM_MASK_KAMARO
        false, // MM_ITEM_MASK_CAPTAIN
        false, // MM_ITEM_MASK_STONE
        false, // MM_ITEM_MASK_BREMEN
        false, // MM_ITEM_MASK_BLAST
        false, // MM_ITEM_MASK_SCENTS
        false, // MM_ITEM_MASK_GIANT
        false, // MM_ITEM_BOW_FIRE
        false, // MM_ITEM_BOW_ICE
        false, // MM_ITEM_BOW_LIGHT
        false, // MM_ITEM_SWORD_KOKIRI
        false, // MM_ITEM_SWORD_RAZOR
        false, // MM_ITEM_SWORD_GILDED
        false, // MM_ITEM_SWORD_DEITY
        false, // MM_ITEM_SHIELD_HERO
        false, // MM_ITEM_SHIELD_MIRROR
        false, // MM_ITEM_QUIVER_30
        false, // MM_ITEM_QUIVER_40
        false, // MM_ITEM_QUIVER_50
        false, // MM_ITEM_BOMB_BAG_20
        false, // MM_ITEM_BOMB_BAG_30
        false, // MM_ITEM_BOMB_BAG_40
        false, // MM_ITEM_WALLET_DEFAULT
        false, // MM_ITEM_WALLET_ADULT
        false, // MM_ITEM_WALLET_GIANT
        false, // MM_ITEM_FISHING_ROD
        false, // MM_ITEM_REMAINS_ODOLWA
        false, // MM_ITEM_REMAINS_GOHT
        false, // MM_ITEM_REMAINS_GYORG
        false, // MM_ITEM_REMAINS_TWINMOLD
        false, // MM_ITEM_SONG_SONATA
        false, // MM_ITEM_SONG_LULLABY
        false, // MM_ITEM_SONG_NOVA
        false, // MM_ITEM_SONG_ELEGY
        false, // MM_ITEM_SONG_OATH
        false, // MM_ITEM_SONG_SARIA
        false, // MM_ITEM_SONG_TIME
        false, // MM_ITEM_SONG_HEALING
        false, // MM_ITEM_SONG_EPONA
        false, // MM_ITEM_SONG_SOARING
        false, // MM_ITEM_SONG_STORMS
        false, // MM_ITEM_SONG_SUN
        false, // MM_ITEM_BOMBERS_NOTEBOOK
        false, // MM_ITEM_SKULL_TOKEN
        false, // MM_ITEM_HEART_CONTAINER
        false, // MM_ITEM_HEART_PIECE
        false, // MM_ITEM_71
    },
    // MM_FORM_DEKU
    {
        true,  // MM_ITEM_OCARINA_OF_TIME
        false, // MM_ITEM_BOW
        false, // MM_ITEM_ARROW_FIRE
        false, // MM_ITEM_ARROW_ICE
        false, // MM_ITEM_ARROW_LIGHT
        false, // MM_ITEM_OCARINA_FAIRY
        false, // MM_ITEM_BOMB
        false, // MM_ITEM_BOMBCHU
        false, // MM_ITEM_DEKU_STICK
        true,  // MM_ITEM_DEKU_NUT
        false, // MM_ITEM_MAGIC_BEANS
        false, // MM_ITEM_SLINGSHOT
        false, // MM_ITEM_POWDER_KEG
        true,  // MM_ITEM_PICTOGRAPH_BOX
        true,  // MM_ITEM_LENS_OF_TRUTH
        false, // MM_ITEM_HOOKSHOT
        false, // MM_ITEM_SWORD_GREAT_FAIRY
        false, // MM_ITEM_LONGSHOT
        true,  // MM_ITEM_BOTTLE
        true,  // MM_ITEM_POTION_RED
        true,  // MM_ITEM_POTION_GREEN
        true,  // MM_ITEM_POTION_BLUE
        true,  // MM_ITEM_FAIRY
        true,  // MM_ITEM_DEKU_PRINCESS
        true,  // MM_ITEM_MILK_BOTTLE
        true,  // MM_ITEM_MILK_HALF
        true,  // MM_ITEM_FISH
        true,  // MM_ITEM_BUG
        true,  // MM_ITEM_BLUE_FIRE
        true,  // MM_ITEM_POE
        true,  // MM_ITEM_BIG_POE
        true,  // MM_ITEM_SPRING_WATER
        true,  // MM_ITEM_HOT_SPRING_WATER
        true,  // MM_ITEM_ZORA_EGG
        true,  // MM_ITEM_GOLD_DUST
        true,  // MM_ITEM_MUSHROOM
        true,  // MM_ITEM_SEAHORSE
        true,  // MM_ITEM_CHATEAU
        true,  // MM_ITEM_HYLIAN_LOACH
        true,  // MM_ITEM_OBABA_DRINK
        true,  // MM_ITEM_MOONS_TEAR
        true,  // MM_ITEM_DEED_LAND
        true,  // MM_ITEM_DEED_SWAMP
        true,  // MM_ITEM_DEED_MOUNTAIN
        true,  // MM_ITEM_DEED_OCEAN
        true,  // MM_ITEM_ROOM_KEY
        true,  // MM_ITEM_LETTER_MAMA
        true,  // MM_ITEM_LETTER_TO_KAFEI
        true,  // MM_ITEM_PENDANT_OF_MEMORIES
        true,  // MM_ITEM_TINGLE_MAP
        true,  // MM_ITEM_MASK_DEKU
        true,  // MM_ITEM_MASK_GORON
        true,  // MM_ITEM_MASK_ZORA
        true,  // MM_ITEM_MASK_FIERCE_DEITY
        false, // MM_ITEM_MASK_TRUTH
        false, // MM_ITEM_MASK_KAFEIS_MASK
        false, // MM_ITEM_MASK_ALL_NIGHT
        false, // MM_ITEM_MASK_BUNNY
        false, // MM_ITEM_MASK_KEATON
        false, // MM_ITEM_MASK_GARO
        false, // MM_ITEM_MASK_ROMANI
        false, // MM_ITEM_MASK_CIRCUS_LEADER
        false, // MM_ITEM_MASK_POSTMAN
        false, // MM_ITEM_MASK_COUPLE
        false, // MM_ITEM_MASK_GREAT_FAIRY
        false, // MM_ITEM_MASK_GIBDO
        false, // MM_ITEM_MASK_DON_GERO
        false, // MM_ITEM_MASK_KAMARO
        false, // MM_ITEM_MASK_CAPTAIN
        false, // MM_ITEM_MASK_STONE
        false, // MM_ITEM_MASK_BREMEN
        false, // MM_ITEM_MASK_BLAST
        false, // MM_ITEM_MASK_SCENTS
        false, // MM_ITEM_MASK_GIANT
        false, // MM_ITEM_BOW_FIRE
        false, // MM_ITEM_BOW_ICE
        false, // MM_ITEM_BOW_LIGHT
        false, // MM_ITEM_SWORD_KOKIRI
        false, // MM_ITEM_SWORD_RAZOR
        false, // MM_ITEM_SWORD_GILDED
        false, // MM_ITEM_SWORD_DEITY
        false, // MM_ITEM_SHIELD_HERO
        false, // MM_ITEM_SHIELD_MIRROR
        false, // MM_ITEM_QUIVER_30
        false, // MM_ITEM_QUIVER_40
        false, // MM_ITEM_QUIVER_50
        false, // MM_ITEM_BOMB_BAG_20
        false, // MM_ITEM_BOMB_BAG_30
        false, // MM_ITEM_BOMB_BAG_40
        false, // MM_ITEM_WALLET_DEFAULT
        false, // MM_ITEM_WALLET_ADULT
        false, // MM_ITEM_WALLET_GIANT
        false, // MM_ITEM_FISHING_ROD
        false, // MM_ITEM_REMAINS_ODOLWA
        false, // MM_ITEM_REMAINS_GOHT
        false, // MM_ITEM_REMAINS_GYORG
        false, // MM_ITEM_REMAINS_TWINMOLD
        false, // MM_ITEM_SONG_SONATA
        false, // MM_ITEM_SONG_LULLABY
        false, // MM_ITEM_SONG_NOVA
        false, // MM_ITEM_SONG_ELEGY
        false, // MM_ITEM_SONG_OATH
        false, // MM_ITEM_SONG_SARIA
        false, // MM_ITEM_SONG_TIME
        false, // MM_ITEM_SONG_HEALING
        false, // MM_ITEM_SONG_EPONA
        false, // MM_ITEM_SONG_SOARING
        false, // MM_ITEM_SONG_STORMS
        false, // MM_ITEM_SONG_SUN
        false, // MM_ITEM_BOMBERS_NOTEBOOK
        false, // MM_ITEM_SKULL_TOKEN
        false, // MM_ITEM_HEART_CONTAINER
        false, // MM_ITEM_HEART_PIECE
        false, // MM_ITEM_71
    },
};

// ---- Hyrule's items, by meaning, onto MM's table ----
//
// Each Ocarina of Time item takes the row entry of the MM item that means the same thing; an item
// Termina does not have follows the closest MM rule, noted beside it. An entry of 0 has no MM
// counterpart that could ever sit on a C button and is refused to every transformed form (MM's table
// is false for all four forms on every such item). Stored as MM id + 1 so that 0 can mean "none".
#define MMRULE(mmItem) ((u8)((mmItem) + 1))

static const u8 sOoTItemMMRule[ITEM_MM_LAST + 1] = {
    [ITEM_STICK] = MMRULE(MM_ITEM_DEKU_STICK),
    [ITEM_NUT] = MMRULE(MM_ITEM_DEKU_NUT),
    [ITEM_BOMB] = MMRULE(MM_ITEM_BOMB),
    [ITEM_BOW] = MMRULE(MM_ITEM_BOW),
    [ITEM_ARROW_FIRE] = MMRULE(MM_ITEM_ARROW_FIRE),
    // Hyrule-only: the three spells are magic cast by the human's own hands. Closest MM rule: the
    // Great Fairy's Sword, the Great Fairy's human-only gift (false for every form).
    [ITEM_DINS_FIRE] = MMRULE(MM_ITEM_SWORD_GREAT_FAIRY),
    [ITEM_SLINGSHOT] = MMRULE(MM_ITEM_SLINGSHOT),
    // MM's own ITEM_OCARINA_FAIRY row entry is a leftover item Termina never gives out. By meaning the
    // Fairy Ocarina is the player's ocarina, which every form plays as its own instrument in MM.
    [ITEM_OCARINA_FAIRY] = MMRULE(MM_ITEM_OCARINA_OF_TIME),
    [ITEM_OCARINA_TIME] = MMRULE(MM_ITEM_OCARINA_OF_TIME),
    [ITEM_BOMBCHU] = MMRULE(MM_ITEM_BOMBCHU),
    [ITEM_HOOKSHOT] = MMRULE(MM_ITEM_HOOKSHOT),
    [ITEM_LONGSHOT] = MMRULE(MM_ITEM_LONGSHOT),
    [ITEM_ARROW_ICE] = MMRULE(MM_ITEM_ARROW_ICE),
    [ITEM_FARORES_WIND] = MMRULE(MM_ITEM_SWORD_GREAT_FAIRY), // Hyrule-only spell, see Din's Fire
    // Hyrule-only: a human's thrown weapon. Closest MM rule: the slingshot (a child's ranged weapon).
    [ITEM_BOOMERANG] = MMRULE(MM_ITEM_SLINGSHOT),
    [ITEM_LENS] = MMRULE(MM_ITEM_LENS_OF_TRUTH),
    [ITEM_BEAN] = MMRULE(MM_ITEM_MAGIC_BEANS),
    // Hyrule-only: a two-handed human weapon on a C button. Closest MM rule: the Great Fairy's Sword.
    [ITEM_HAMMER] = MMRULE(MM_ITEM_SWORD_GREAT_FAIRY),
    [ITEM_ARROW_LIGHT] = MMRULE(MM_ITEM_ARROW_LIGHT),
    [ITEM_NAYRUS_LOVE] = MMRULE(MM_ITEM_SWORD_GREAT_FAIRY), // Hyrule-only spell, see Din's Fire
    [ITEM_BOTTLE] = MMRULE(MM_ITEM_BOTTLE),
    [ITEM_POTION_RED] = MMRULE(MM_ITEM_POTION_RED),
    [ITEM_POTION_GREEN] = MMRULE(MM_ITEM_POTION_GREEN),
    [ITEM_POTION_BLUE] = MMRULE(MM_ITEM_POTION_BLUE),
    [ITEM_FAIRY] = MMRULE(MM_ITEM_FAIRY),
    [ITEM_FISH] = MMRULE(MM_ITEM_FISH),
    [ITEM_MILK_BOTTLE] = MMRULE(MM_ITEM_MILK_BOTTLE),
    // Hyrule-only: bottled cargo to be delivered. Closest MM rule: the Deku Princess in a bottle.
    [ITEM_LETTER_RUTO] = MMRULE(MM_ITEM_DEKU_PRINCESS),
    [ITEM_BLUE_FIRE] = MMRULE(MM_ITEM_BLUE_FIRE),
    [ITEM_BUG] = MMRULE(MM_ITEM_BUG),
    [ITEM_BIG_POE] = MMRULE(MM_ITEM_BIG_POE),
    [ITEM_MILK_HALF] = MMRULE(MM_ITEM_MILK_HALF),
    [ITEM_POE] = MMRULE(MM_ITEM_POE),
    // Hyrule's trade sequences. Closest MM rule: MM's trade items (Moon's Tear, the deeds, the room key,
    // the letters), which share one row value; letters map to MM's letters.
    [ITEM_WEIRD_EGG] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_CHICKEN] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_LETTER_ZELDA] = MMRULE(MM_ITEM_LETTER_TO_KAFEI),
    // Hyrule's wearable masks are MM's non-transformation masks: human only.
    [ITEM_MASK_KEATON] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_SKULL] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_SPOOKY] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_BUNNY] = MMRULE(MM_ITEM_MASK_BUNNY),
    // Ocarina of Time's Goron and Zora masks, while they are cosmetic masks. While they are the transformation
    // masks (MergeShipMaskItems.cpp), MergeShip_ResolveMaskItem turns them into those first.
    [ITEM_MASK_GORON] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_ZORA] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_GERUDO] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MASK_TRUTH] = MMRULE(MM_ITEM_MASK_TRUTH),
    // ITEM_SOLD_OUT: a placeholder in the mask slot, never usable; no rule.
    [ITEM_POCKET_EGG] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_POCKET_CUCCO] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_COJIRO] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_ODD_MUSHROOM] = MMRULE(MM_ITEM_MOONS_TEAR), // a trade item, not MM's bottled Magic Mushroom
    [ITEM_ODD_POTION] = MMRULE(MM_ITEM_MOONS_TEAR),   // likewise not in a bottle
    [ITEM_SAW] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_SWORD_BROKEN] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_PRESCRIPTION] = MMRULE(MM_ITEM_LETTER_MAMA),
    [ITEM_FROG] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_EYEDROPS] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_CLAIM_CHECK] = MMRULE(MM_ITEM_LETTER_MAMA),
    [ITEM_BOW_ARROW_FIRE] = MMRULE(MM_ITEM_BOW_FIRE),
    [ITEM_BOW_ARROW_ICE] = MMRULE(MM_ITEM_BOW_ICE),
    [ITEM_BOW_ARROW_LIGHT] = MMRULE(MM_ITEM_BOW_LIGHT),
    [ITEM_SWORD_KOKIRI] = MMRULE(MM_ITEM_SWORD_KOKIRI),
    [ITEM_SWORD_MASTER] = MMRULE(MM_ITEM_SWORD_RAZOR),
    [ITEM_SWORD_BGS] = MMRULE(MM_ITEM_SWORD_GILDED),
    [ITEM_SHIELD_DEKU] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_SHIELD_HYLIAN] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_SHIELD_MIRROR] = MMRULE(MM_ITEM_SHIELD_MIRROR),
    // Hyrule-only: tunics and boots on C buttons (SoH). Closest MM rule: the human's equipment (shield).
    [ITEM_TUNIC_KOKIRI] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_TUNIC_GORON] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_TUNIC_ZORA] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_BOOTS_KOKIRI] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_BOOTS_IRON] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_BOOTS_HOVER] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_FISHING_POLE] = MMRULE(MM_ITEM_FISHING_ROD),
    // SoH's Roc's Feather: a human traversal tool. Closest MM rule: the hookshot.
    [ITEM_ROCS_FEATHER] = MMRULE(MM_ITEM_HOOKSHOT),
    [ITEM_MM_MASK_FIERCE_DEITY] = MMRULE(MM_ITEM_MASK_FIERCE_DEITY),
    [ITEM_MM_MASK_GORON] = MMRULE(MM_ITEM_MASK_GORON),
    [ITEM_MM_MASK_ZORA] = MMRULE(MM_ITEM_MASK_ZORA),
    [ITEM_MM_MASK_DEKU] = MMRULE(MM_ITEM_MASK_DEKU),
    [ITEM_MM_SWORD_FIERCE_DEITY] = MMRULE(MM_ITEM_SWORD_DEITY),
    // MergeShip2: the Adult Mask is used as the Fierce Deity's Mask (MergeShipAdultMask.h).
    [ITEM_MM_MASK_ADULT] = MMRULE(MM_ITEM_MASK_FIERCE_DEITY),
    [ITEM_MM_MASK_TRUTH] = MMRULE(MM_ITEM_MASK_TRUTH),
    [ITEM_MM_MASK_KAFEIS_MASK] = MMRULE(MM_ITEM_MASK_KAFEIS_MASK),
    [ITEM_MM_MASK_ALL_NIGHT] = MMRULE(MM_ITEM_MASK_ALL_NIGHT),
    [ITEM_MM_MASK_BUNNY] = MMRULE(MM_ITEM_MASK_BUNNY),
    [ITEM_MM_MASK_KEATON] = MMRULE(MM_ITEM_MASK_KEATON),
    [ITEM_MM_MASK_GARO] = MMRULE(MM_ITEM_MASK_GARO),
    [ITEM_MM_MASK_ROMANI] = MMRULE(MM_ITEM_MASK_ROMANI),
    [ITEM_MM_MASK_CIRCUS_LEADER] = MMRULE(MM_ITEM_MASK_CIRCUS_LEADER),
    [ITEM_MM_MASK_POSTMAN] = MMRULE(MM_ITEM_MASK_POSTMAN),
    [ITEM_MM_MASK_COUPLE] = MMRULE(MM_ITEM_MASK_COUPLE),
    [ITEM_MM_MASK_GREAT_FAIRY] = MMRULE(MM_ITEM_MASK_GREAT_FAIRY),
    [ITEM_MM_MASK_GIBDO] = MMRULE(MM_ITEM_MASK_GIBDO),
    [ITEM_MM_MASK_DON_GERO] = MMRULE(MM_ITEM_MASK_DON_GERO),
    [ITEM_MM_MASK_KAMARO] = MMRULE(MM_ITEM_MASK_KAMARO),
    [ITEM_MM_MASK_CAPTAIN] = MMRULE(MM_ITEM_MASK_CAPTAIN),
    [ITEM_MM_MASK_STONE] = MMRULE(MM_ITEM_MASK_STONE),
    [ITEM_MM_MASK_BREMEN] = MMRULE(MM_ITEM_MASK_BREMEN),
    [ITEM_MM_MASK_BLAST] = MMRULE(MM_ITEM_MASK_BLAST),
    [ITEM_MM_MASK_SCENTS] = MMRULE(MM_ITEM_MASK_SCENTS),
    [ITEM_MM_MASK_GIANT] = MMRULE(MM_ITEM_MASK_GIANT),
    [ITEM_MM_CHATEAU] = MMRULE(MM_ITEM_CHATEAU), // MergeShip2: MM's Chateau Romani (MergeShipChateau.h)
    // MergeShip2: Majora's Mask's other items (the stray fairy and the Goron Lullaby's intro have no MM rule).
    [ITEM_MM_SWORD_RAZOR] = MMRULE(MM_ITEM_SWORD_RAZOR),
    [ITEM_MM_SWORD_KOKIRI] = MMRULE(MM_ITEM_SWORD_KOKIRI),
    [ITEM_MM_SWORD_GILDED] = MMRULE(MM_ITEM_SWORD_GILDED),
    [ITEM_MM_SWORD_GREAT_FAIRY] = MMRULE(MM_ITEM_SWORD_GREAT_FAIRY),
    [ITEM_MM_SHIELD_HERO] = MMRULE(MM_ITEM_SHIELD_HERO),
    [ITEM_MM_SHIELD_MIRROR] = MMRULE(MM_ITEM_SHIELD_MIRROR),
    [ITEM_MM_HOOKSHOT] = MMRULE(MM_ITEM_HOOKSHOT),
    [ITEM_MM_BOW_HERO] = MMRULE(MM_ITEM_BOW),
    [ITEM_MM_POWDER_KEG] = MMRULE(MM_ITEM_POWDER_KEG),
    [ITEM_MM_PICTOGRAPH_BOX] = MMRULE(MM_ITEM_PICTOGRAPH_BOX),
    [ITEM_MM_DEKU_PRINCESS] = MMRULE(MM_ITEM_DEKU_PRINCESS),
    [ITEM_MM_ZORA_EGG] = MMRULE(MM_ITEM_ZORA_EGG),
    [ITEM_MM_GOLD_DUST] = MMRULE(MM_ITEM_GOLD_DUST),
    [ITEM_MM_MUSHROOM] = MMRULE(MM_ITEM_MUSHROOM),
    [ITEM_MM_SEAHORSE] = MMRULE(MM_ITEM_SEAHORSE),
    [ITEM_MM_HYLIAN_LOACH] = MMRULE(MM_ITEM_HYLIAN_LOACH),
    [ITEM_MM_SPRING_WATER] = MMRULE(MM_ITEM_SPRING_WATER),
    [ITEM_MM_HOT_SPRING_WATER] = MMRULE(MM_ITEM_HOT_SPRING_WATER),
    [ITEM_MM_MOONS_TEAR] = MMRULE(MM_ITEM_MOONS_TEAR),
    [ITEM_MM_DEED_LAND] = MMRULE(MM_ITEM_DEED_LAND),
    [ITEM_MM_DEED_SWAMP] = MMRULE(MM_ITEM_DEED_SWAMP),
    [ITEM_MM_DEED_MOUNTAIN] = MMRULE(MM_ITEM_DEED_MOUNTAIN),
    [ITEM_MM_DEED_OCEAN] = MMRULE(MM_ITEM_DEED_OCEAN),
    [ITEM_MM_ROOM_KEY] = MMRULE(MM_ITEM_ROOM_KEY),
    [ITEM_MM_LETTER_TO_MAMA] = MMRULE(MM_ITEM_LETTER_MAMA),
    [ITEM_MM_LETTER_TO_KAFEI] = MMRULE(MM_ITEM_LETTER_TO_KAFEI),
    [ITEM_MM_PENDANT_OF_MEMORIES] = MMRULE(MM_ITEM_PENDANT_OF_MEMORIES),
    [ITEM_MM_REMAINS_ODOLWA] = MMRULE(MM_ITEM_REMAINS_ODOLWA),
    [ITEM_MM_REMAINS_GOHT] = MMRULE(MM_ITEM_REMAINS_GOHT),
    [ITEM_MM_REMAINS_GYORG] = MMRULE(MM_ITEM_REMAINS_GYORG),
    [ITEM_MM_REMAINS_TWINMOLD] = MMRULE(MM_ITEM_REMAINS_TWINMOLD),
    [ITEM_MM_BOMBERS_NOTEBOOK] = MMRULE(MM_ITEM_BOMBERS_NOTEBOOK),
    [ITEM_MM_SONG_SONATA] = MMRULE(MM_ITEM_SONG_SONATA),
    [ITEM_MM_SONG_GORON_LULLABY] = MMRULE(MM_ITEM_SONG_LULLABY),
    [ITEM_MM_SONG_NEW_WAVE] = MMRULE(MM_ITEM_SONG_NOVA),
    [ITEM_MM_SONG_ELEGY] = MMRULE(MM_ITEM_SONG_ELEGY),
    [ITEM_MM_SONG_OATH] = MMRULE(MM_ITEM_SONG_OATH),
    [ITEM_MM_SONG_HEALING] = MMRULE(MM_ITEM_SONG_HEALING),
    [ITEM_MM_SONG_SOARING] = MMRULE(MM_ITEM_SONG_SOARING),
};
MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(sOoTItemMMRule) == ITEM_MM_LAST + 1,
                        "sOoTItemMMRule needs an entry for every item id up to the MM items");

s32 MergeShip_FormCanUseItem(u8 form, u8 item) {
    u8 mmForm;

    if ((item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected()) {
        // MergeShip2 Deku Beans (MergeShipDekuBeans.cpp): the bean slot holds Deku Beans, which only Deku Link
        // plants. Link of either age and the other forms cannot use them.
        return form == PLAYER_FORM_DEKU;
    }
    if (MergeShip_MMItems_Handles(item) && !MergeShip_MMItems_Enabled()) {
        // MergeShip2: MM's keg, box, bottle contents and trade items with CVAR MergeShip.MMItems off.
        return false;
    }
    if (!MERGESHIP_FORM_IS_MM(form)) {
        return true;
    }
    mmForm = MergeShip_FormToMM(form);
    item = MergeShip_ResolveMaskItem(item);

    // MergeShip2 (soh_fd): Roc's Feather, trade masks over a form and the Fierce Deity options.
    {
        s32 sohFd = MergeShip_SohFd_FormItemOverride(form, item);
        if (sohFd >= 0) {
            return sohFd;
        }
    }

    if (item == ITEM_NONE) {
        //! MM @bug (kept): an empty C button is item 255, read past the table's 114 columns. The read lands
        //! on a later form's row for the Fierce Deity, Goron and Zora (enabled) and past the table for the
        //! Deku (disabled), so only the Deku's empty buttons are dimmed.
        return mmForm != MM_FORM_DEKU;
    }
    if ((item >= ARRAY_COUNT(sOoTItemMMRule)) || (sOoTItemMMRule[item] == 0)) {
        return false;
    }
    return sMMFormItemRestrictions[mmForm][sOoTItemMMRule[item] - 1];
}

s32 MergeShip_FierceDeityMaskAllowedHere(PlayState* play) {
    // MM: Odolwa's, Goht's, Gyorg's, Twinmold's and Majora's lairs. Hyrule's counterparts: the boss
    // rooms of its dungeons and both of Ganon's arenas. gGeneral.MergeShipFierceDeityAnywhere mirrors
    // 2S2H's "Fierce Deity's Mask anywhere" enhancement (VB_DISABLE_FD_MASK); off by default. The
    // gGeneral.MergeShipDpadMasks quick-play setting (all four masks on the D-pad, for trying the forms)
    // implies it, so every quick-play mask can be tried wherever the player is.
    // MergeShip2 (soh_fd): soh_fd's rule adds the fishing hole (Majora's Mask 3D lets the Fierce Deity fish); the
    // list lives in MergeShip_SohFd_FierceDeityAllowedInScene, which the scene-load revert uses too.
    return MergeShip_SohFd_FierceDeityAllowedInScene(play->sceneNum);
}

s32 MergeShip_GiantsMaskAllowedHere(PlayState* play) {
    // MM Interface_UpdateButtonsPart2: the Giant's Mask is disabled everywhere but Twinmold's lair
    // (SCENE_INISIE_BS), which Hyrule has no counterpart for. CVAR_ENHANCEMENT("MergeShip.GiantsMaskInLairs")
    // lets it be worn where the Fierce Deity's Mask may be: Hyrule's boss lairs.
    // (Its own lair list: "Fierce Deity Anywhere" and the D-pad quick-play masks do not extend to it.)
    if (!CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.GiantsMaskInLairs"), 0)) {
        return false;
    }
    switch (play->sceneNum) {
        case SCENE_DEKU_TREE_BOSS:
        case SCENE_DODONGOS_CAVERN_BOSS:
        case SCENE_JABU_JABU_BOSS:
        case SCENE_FOREST_TEMPLE_BOSS:
        case SCENE_FIRE_TEMPLE_BOSS:
        case SCENE_WATER_TEMPLE_BOSS:
        case SCENE_SPIRIT_TEMPLE_BOSS:
        case SCENE_SHADOW_TEMPLE_BOSS:
        case SCENE_GANONDORF_BOSS:
        case SCENE_GANON_BOSS:
            return true;
        default:
            return false;
    }
}

// MM Player_GetEnvironmentalHazard (z_player_lib.c), without its "It's hot" text box (Ocarina of
// Time's own call raises Hyrule's text boxes).
s32 MergeShip_GetEnvironmentalHazard(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((player == NULL) || !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return Player_GetEnvironmentalHazard(play);
    }
    if (play->roomCtx.curRoom.behaviorType2 == ROOM_BEHAVIOR_TYPE2_3) {
        return PLAYER_ENV_HAZARD_HOTROOM;
    }
    if ((player->mergeShipForm != PLAYER_FORM_ZORA) && (player->underwaterTimer > 80)) {
        return PLAYER_ENV_HAZARD_UNDERWATER_FREE;
    }
    if (player->stateFlags1 & PLAYER_STATE1_IN_WATER) {
        if ((player->mergeShipForm == PLAYER_FORM_ZORA) && (gMergeShipPlayerExt.mmBoots >= MM_BOOTS_ZORA_UNDERWATER) &&
            (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND)) {
            return PLAYER_ENV_HAZARD_UNDERWATER_FLOOR;
        }
        return PLAYER_ENV_HAZARD_SWIMMING;
    }
    return PLAYER_ENV_HAZARD_NONE;
}

// OoT's buttonItems index (1..7: C-Left, C-Down, C-Right, D-Up, D-Down, D-Left, D-Right) to its
// buttonStatus index (the A button sits at 4).
#define MS_STATUS_INDEX(i) (((i) >= 4) ? ((i) + 1) : (i))
#define MS_NUM_ITEM_BUTTONS 8 // ARRAY_COUNT(gSaveContext.equips.buttonItems)

static void MergeShip_SetButtonStatus(s32 statusIndex, u8 status, s16* restoreHud) {
    if (gSaveContext.buttonStatus[statusIndex] != status) {
        *restoreHud = true;
    }
    gSaveContext.buttonStatus[statusIndex] = status;
}

static void MergeShip_DisableAllItemButtons(void) {
    s32 i;

    for (i = 1; i < MS_NUM_ITEM_BUTTONS; i++) {
        gSaveContext.buttonStatus[MS_STATUS_INDEX(i)] = BTN_DISABLED;
    }
}

// Ocarina of Time's scene restrictions (func_80083108's per-category loops) for one item a form may
// use: 1 enabled, 0 disabled, -1 unchanged. MM applies its own scene restrictions at this point; the
// scenes are Hyrule's, so Hyrule's restrictions apply to the items the form is allowed.
static s32 MergeShip_OoTSceneAllowsItem(PlayState* play, u8 item) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s32 isOcarina = (item == ITEM_OCARINA_FAIRY) || (item == ITEM_OCARINA_TIME);
    s32 isBottle = MERGESHIP_ITEM_IS_BOTTLE(item); // MergeShip2: with MM's Chateau Romani
    s32 isTrade = ((item >= ITEM_WEIRD_EGG) && (item <= ITEM_CLAIM_CHECK)) ||
                  MERGESHIP_ITEM_IS_MM_TRADE(item); // MergeShip2: MM's trade items are Hyrule's trade class too
    s32 isEquip = (item >= ITEM_SHIELD_DEKU) && (item <= ITEM_BOOTS_HOVER);
    s32 isHookshot = (item == ITEM_HOOKSHOT) || (item == ITEM_LONGSHOT);
    s32 isDinsNayrus = (item == ITEM_DINS_FIRE) || (item == ITEM_NAYRUS_LOVE);
    s32 result = -1;

    if (isEquip) {
        result = 1;
    } else if (isBottle) {
        result = !interfaceCtx->restrictions.bottles;
    } else if (isTrade) {
        result = (interfaceCtx->restrictions.tradeItems == 0) ||
                 !GameInteractor_Should(VB_DISABLE_TRADE_ITEM_BUTTON, true, item);
    } else if (isHookshot) {
        result = !interfaceCtx->restrictions.hookshot;
    } else if (isOcarina) {
        result = !interfaceCtx->restrictions.ocarina;
    } else if (item == ITEM_FARORES_WIND) {
        result = !interfaceCtx->restrictions.farores;
    } else if (isDinsNayrus) {
        result = !interfaceCtx->restrictions.dinsNayrus;
    }

    if (interfaceCtx->restrictions.all != 0) {
        if (!isOcarina && !isBottle && !isEquip && !isTrade) {
            result = ((play->sceneNum == SCENE_TREASURE_BOX_SHOP) && (item == ITEM_LENS)) ? 1 : 0;
        }
    } else if (!isDinsNayrus && !isHookshot && (item != ITEM_FARORES_WIND) && !isOcarina && !isBottle && !isTrade) {
        result = 1;
    }
    return result;
}

// MM Interface_UpdateButtonsPart2's per-C-button loop (its "C buttons" block) for one button.
static void MergeShip_UpdateItemButtonMM(PlayState* play, Player* player, s32 i, s16* restoreHud) {
    u8 item = MergeShip_ResolveMaskItem(gSaveContext.equips.buttonItems[i]);
    s32 statusIndex = MS_STATUS_INDEX(i);
    s32 allowed;

    if (!MergeShip_FormCanUseItem(player->mergeShipForm, item)) {
        // Item not usable in the current form.
        MergeShip_SetButtonStatus(statusIndex, BTN_DISABLED, restoreHud);
    } else if (item == ITEM_MM_MASK_FIERCE_DEITY) {
        MergeShip_SetButtonStatus(statusIndex, MergeShip_FierceDeityMaskAllowedHere(play) ? BTN_ENABLED : BTN_DISABLED,
                                  restoreHud);
    } else if (item == ITEM_MM_MASK_GIANT) {
        MergeShip_SetButtonStatus(statusIndex, MergeShip_GiantsMaskAllowedHere(play) ? BTN_ENABLED : BTN_DISABLED,
                                  restoreHud);
    } else if (MERGESHIP_ITEM_IS_MM_MASK(item)) {
        // MM's masks restriction class. Hyrule's scenes carry no such restriction.
        MergeShip_SetButtonStatus(statusIndex, BTN_ENABLED, restoreHud);
    } else {
        allowed = MergeShip_OoTSceneAllowsItem(play, item);
        if (allowed >= 0) {
            MergeShip_SetButtonStatus(statusIndex, allowed ? BTN_ENABLED : BTN_DISABLED, restoreHud);
        }
    }
}

s32 MergeShip_UpdateButtonsMM(PlayState* play, s16* restoreHud) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    Player* player = GET_PLAYER(play);
    s32 envHazard;
    s32 i;

    if ((player == NULL) || !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return false;
    }

    envHazard = MergeShip_GetEnvironmentalHazard(play);

    if (player->stateFlags3 & PLAYER_STATE3_1000000) {
        // MM: nuts on B while flying as Deku Link.
        if (gSaveContext.inventory.items[SLOT_NUT] == ITEM_NUT) {
            if (gSaveContext.equips.buttonItems[0] != ITEM_NUT) {
                gSaveContext.equips.buttonItems[0] = ITEM_NUT;
                Interface_LoadItemIcon1(play, 0);
                gSaveContext.buttonStatus[0] = BTN_ENABLED;
                *restoreHud = true;
            }
        } else if (gSaveContext.buttonStatus[0] == BTN_ENABLED) {
            gSaveContext.buttonStatus[0] = BTN_DISABLED;
            *restoreHud = true;
        }

        if (*restoreHud) {
            MergeShip_DisableAllItemButtons();
        }
    } else if ((gSaveContext.magicLevel == 0) && (player->mergeShipForm == PLAYER_FORM_DEKU) &&
               (gSaveContext.equips.buttonItems[0] == ITEM_NUT)) {
        // MM: nuts on B as Deku Link without magic become the form's own (empty) action.
        gSaveContext.equips.buttonItems[0] = ITEM_MM_FD;
        gSaveContext.buttonStatus[0] = BTN_DISABLED;
    } else if ((envHazard >= PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) && (envHazard <= PLAYER_ENV_HAZARD_UNDERWATER_FREE)) {
        // MM: in the water. Only the Zora keeps B; only the Zora Mask (and, standing on the floor,
        // bottles) stays on the item buttons.
        MergeShip_SetButtonStatus(0, (player->mergeShipForm == PLAYER_FORM_ZORA) ? BTN_ENABLED : BTN_DISABLED,
                                  restoreHud);

        for (i = 1; i < MS_NUM_ITEM_BUTTONS; i++) {
            u8 item = MergeShip_ResolveMaskItem(gSaveContext.equips.buttonItems[i]);

            if (item != ITEM_MM_MASK_ZORA) {
                if ((envHazard == PLAYER_ENV_HAZARD_UNDERWATER_FLOOR) && MERGESHIP_ITEM_IS_BOTTLE(item)) {
                    MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), BTN_ENABLED, restoreHud);
                } else {
                    MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), BTN_DISABLED, restoreHud);
                }
            } else {
                MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), BTN_ENABLED, restoreHud);
            }
        }

        if (*restoreHud) {
            gSaveContext.hudVisibilityMode = HUD_VISIBILITY_NO_CHANGE;
        }
        if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->transitionMode == TRANS_MODE_OFF)) {
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
        }
    } else if (player->stateFlags1 & PLAYER_STATE1_CLIMBING_LADDER) {
        // MM PLAYER_STATE1_200000: only the Lens of Truth stays usable; B is off.
        for (i = 1; i < MS_NUM_ITEM_BUTTONS; i++) {
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i),
                                      (gSaveContext.equips.buttonItems[i] == ITEM_LENS) ? BTN_ENABLED : BTN_DISABLED,
                                      restoreHud);
        }
        if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
            gSaveContext.buttonStatus[0] = BTN_DISABLED;
            *restoreHud = true;
        }
    } else if ((player->stateFlags1 & PLAYER_STATE1_HANGING_OFF_LEDGE) ||
               (player->stateFlags2 & PLAYER_STATE2_CRAWLING)) {
        // MM: hanging from a ledge turns every item button off. (Hyrule's crawlspaces, which Termina does
        // not have, take the same rule, as Ocarina of Time's own does.)
        if (gSaveContext.buttonStatus[0] != BTN_DISABLED) {
            gSaveContext.buttonStatus[0] = BTN_DISABLED;
            MergeShip_DisableAllItemButtons();
            *restoreHud = true;
            Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
        }
    } else {
        // MM: end of the special cases. B button.
        if (interfaceCtx->restrictions.bButton == 0) {
            if (gSaveContext.equips.buttonItems[0] == ITEM_NONE) {
                // MM enables an empty B only while the form's B action (bButtonPlayerDoAction) is set,
                // which the Goron, Zora and Deku always have and the Fierce Deity never does.
                MergeShip_SetButtonStatus(0,
                                          (player->mergeShipForm != PLAYER_FORM_FIERCE_DEITY) ? BTN_ENABLED
                                                                                              : BTN_DISABLED,
                                          restoreHud);
            } else {
                MergeShip_SetButtonStatus(0, BTN_ENABLED, restoreHud);
            }
        } else {
            MergeShip_SetButtonStatus(0, BTN_DISABLED, restoreHud);
        }

        // C buttons (and SoH's D-pad), only once the saved form is the one the player is built as
        // (MM: GET_PLAYER_FORM == player->transformation; false mid-transformation).
        if (MergeShip_GetPlayerForm() == player->mergeShipForm) {
            for (i = 1; i < MS_NUM_ITEM_BUTTONS; i++) {
                MergeShip_UpdateItemButtonMM(play, player, i, restoreHud);
            }
        }
    }
    return true;
}

// MergeShip2: MM's Giant's Mask, defined in MergeShipGiantsMask.inc (z_player.c).
s32 MergeShip_GiantsMask_IsWorn(PlayState* play);

void MergeShip_UpdateTransformationMaskButtons(PlayState* play, s16* restoreHud) {
    s32 i;

    for (i = 1; i < MS_NUM_ITEM_BUTTONS; i++) {
        u8 item = MergeShip_ResolveMaskItem(gSaveContext.equips.buttonItems[i]);

        if (MergeShip_GiantsMask_IsWorn(play)) {
            // MergeShip2: MM Interface_UpdateButtonsPart2 -- while the Giant's Mask is worn, every item button but the
            // Giant's Mask's own is disabled.
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), (item == ITEM_MM_MASK_GIANT) ? BTN_ENABLED : BTN_DISABLED,
                                      restoreHud);
        } else if (item == ITEM_MM_MASK_FIERCE_DEITY) {
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i),
                                      MergeShip_FierceDeityMaskAllowedHere(play) ? BTN_ENABLED : BTN_DISABLED,
                                      restoreHud);
        } else if (item == ITEM_MM_MASK_GIANT) {
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i),
                                      MergeShip_GiantsMaskAllowedHere(play) ? BTN_ENABLED : BTN_DISABLED, restoreHud);
        } else if (MERGESHIP_ITEM_IS_MM_MASK(item)) {
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), BTN_ENABLED, restoreHud);
        } else if ((item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected()) {
            // Deku Beans: Link cannot use them (MergeShip_FormCanUseItem).
            MergeShip_SetButtonStatus(MS_STATUS_INDEX(i), BTN_DISABLED, restoreHud);
        }
    }
}

s32 MergeShip_GetBButtonDrawMode(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if ((player == NULL) || !MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return MERGESHIP_BDRAW_OOT;
    }
    // MM Interface_DrawBButtonIcons: a flying Deku shows its B item (nuts) with the count, not a label.
    if (!play->interfaceCtx.unk_1FA && (player->stateFlags3 & PLAYER_STATE3_1000000)) {
        // (ITEM_MM_FD has no icon; the first flying frame can still hold it before the nuts go on B.)
        return ((gSaveContext.buttonStatus[0] != BTN_DISABLED) &&
                (gSaveContext.equips.buttonItems[0] <= ITEM_MM_SWORD_FIERCE_DEITY))
                   ? MERGESHIP_BDRAW_ICON_AMMO
                   : MERGESHIP_BDRAW_NOTHING;
    }
    if (play->interfaceCtx.unk_1FA || (MergeShip_GetMMBButtonLabel() != NULL)) {
        return MERGESHIP_BDRAW_OOT; // a label
    }
    // With no label MM draws a B item icon only for the Fierce Deity (and the human).
    return (player->mergeShipForm == PLAYER_FORM_FIERCE_DEITY) ? MERGESHIP_BDRAW_OOT : MERGESHIP_BDRAW_NOTHING;
}

void MergeShip_UpdateDekuBButtonForMagic(void) {
    // MM Interface_Update: when magic is first acquired, BUTTON_ITEM_EQUIP(PLAYER_FORM_DEKU, EQUIP_SLOT_B) =
    // ITEM_DEKU_NUT. MM's Deku B is ITEM_FD only before that (or after magic is lost, MergeShip_UpdateButtonsMM),
    // so a file that already had magic when it got the Deku Mask takes the same state.
    if (gSaveContext.magicLevel == 0) {
        return;
    }
    if (gMergeShipSave.mmForm == MM_FORM_DEKU) {
        if (gSaveContext.equips.buttonItems[0] == ITEM_MM_FD) {
            gSaveContext.equips.buttonItems[0] = ITEM_NUT;
        }
    } else if (gMergeShipSave.bButtonItem[MM_FORM_DEKU] == ITEM_MM_FD) {
        gMergeShipSave.bButtonItem[MM_FORM_DEKU] = ITEM_NUT;
    }
}

// ---- Age swaps ----
static u8 sAgeSwapFormB = ITEM_NONE;
static s32 sAgeSwapInForm = false;
// BUG-0143: whether the swap leaves the child, his B then, and whether the child's set had ever been stored.
static s32 sAgeSwapLeavesChild = false;
static u8 sAgeSwapChildB = ITEM_NONE;
static s32 sAgeSwapChildSetStored = false;

void MergeShip_BeginAgeSwap(void) {
    sAgeSwapInForm = (gMergeShipSave.mmForm != MM_FORM_HUMAN);
    if (sAgeSwapInForm) {
        // The human's B is parked in gMergeShipSave while a form's B sits in equips; put the human's
        // back for Ocarina of Time's swap, which copies equips.buttonItems[0] into the age's set.
        sAgeSwapFormB = gSaveContext.equips.buttonItems[0];
        gSaveContext.equips.buttonItems[0] = gMergeShipSave.bButtonItem[MM_FORM_HUMAN];
    }
    // (Inventory_SwapAgeEquipment runs before gSaveContext.linkAge flips: the age it reads is the age being left.)
    sAgeSwapLeavesChild = (LINK_AGE_IN_YEARS == YEARS_CHILD);
    sAgeSwapChildB = gSaveContext.equips.buttonItems[0];
    sAgeSwapChildSetStored = (gSaveContext.childEquips.equipment != 0);
}

// BUG-0143 (the Adult Mask: "if great fairy sword is on B, it gets set to deku stick on young link"). Ocarina of Time's
// swap stores the child's B as the Kokiri Sword whatever he held -- the only sword its child has -- and hands the child
// his stored set back as it is. Young Link wields Majora's Mask's swords here (the Razor and Gilded Swords and MM's
// Kokiri Sword in the Kokiri Sword's slot, the Great Fairy's Sword in Biggoron's): the one he left on B is his again
// when he comes back, with its slot's sword equipped (MergeShip_MMEquip_Sanitize then keeps it or lets it go, as for
// any B). A child set that was never stored (a file whose Link had not left the child yet: all zeros) put item 0 -- the
// Deku Stick -- on B and on every C button: the child starts from empty C buttons and his own sword on B instead.
static void MergeShip_AgeSwapFixChild(void) {
    s32 i;

    if (sAgeSwapLeavesChild) {
        if (MERGESHIP_ITEM_IS_MM_SWORD(sAgeSwapChildB)) {
            gSaveContext.childEquips.buttonItems[0] = sAgeSwapChildB;
        }
        return;
    }
    // Back to the child: equips hold his set now (the human's, a form's B is parked again below).
    if (!sAgeSwapChildSetStored) {
        for (i = 1; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
            if ((gSaveContext.equips.buttonItems[i] == ITEM_STICK) && (gSaveContext.equips.cButtonSlots[i - 1] == 0)) {
                gSaveContext.equips.buttonItems[i] = ITEM_NONE;
                gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
            }
        }
        if (gSaveContext.equips.buttonItems[0] == ITEM_STICK) {
            gSaveContext.equips.buttonItems[0] =
                CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI) ? ITEM_SWORD_KOKIRI : ITEM_NONE;
        }
        return;
    }
    if (MERGESHIP_ITEM_IS_MM_SWORD(gSaveContext.childEquips.buttonItems[0])) {
        const u8 sword = gSaveContext.childEquips.buttonItems[0];
        const u16 value =
            (sword == ITEM_MM_SWORD_GREAT_FAIRY) ? EQUIP_VALUE_SWORD_BIGGORON : EQUIP_VALUE_SWORD_KOKIRI;

        gSaveContext.equips.buttonItems[0] = sword;
        gSaveContext.equips.equipment &= (u16) ~(0xF << (EQUIP_TYPE_SWORD * 4));
        gSaveContext.equips.equipment |= (u16)(value << (EQUIP_TYPE_SWORD * 4));
        Flags_UnsetInfTable(INFTABLE_SWORDLESS); // BUG-0046: an MM sword on B is a sword
    }
}

void MergeShip_EndAgeSwap(void) {
    MergeShip_AgeSwapFixChild(); // BUG-0143 (before a form's B is parked back over the human's)
    if (sAgeSwapInForm) {
        gMergeShipSave.bButtonItem[MM_FORM_HUMAN] = gSaveContext.equips.buttonItems[0];
        gSaveContext.equips.buttonItems[0] = sAgeSwapFormB;
        sAgeSwapInForm = false;
    }
}

// ---- Tunics while transformed ------------------------------------------------------------------
// A transformation form wears no Hylian tunic: it counts as wearing the Kokiri Tunic, and the pause
// screen greys the Goron and Zora Tunics out and refuses to equip them, through the same checks that
// grey out equipment the wrong age can't use (z_kaleido_scope.h CHECK_AGE_REQ_EQUIP / _ITEM).

extern PlayState* gPlayState;

static s32 MergeShip_PlayerIsTransformed(void) {
    return (gPlayState != NULL) && (GET_PLAYER(gPlayState) != NULL) &&
           MERGESHIP_FORM_IS_MM(GET_PLAYER(gPlayState)->mergeShipForm);
}

s32 MergeShip_FormMayEquip(s32 equipType, s32 equipValue) {
    return !((equipType == EQUIP_TYPE_TUNIC) && (equipValue != EQUIP_VALUE_TUNIC_KOKIRI) &&
             MergeShip_PlayerIsTransformed());
}

s32 MergeShip_FormMayUseEquipItem(s32 item) {
    return !(((item == ITEM_TUNIC_GORON) || (item == ITEM_TUNIC_ZORA)) && MergeShip_PlayerIsTransformed());
}
