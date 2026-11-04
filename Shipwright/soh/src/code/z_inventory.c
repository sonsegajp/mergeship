#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"
#include "textures/icon_item_static/icon_item_static.h"
#include "textures/icon_item_24_static/icon_item_24_static.h"
#include "textures/parameter_static/parameter_static.h"
#include <soh_assets.h>
#include "mm/archives/icon_item_static/icon_item_static_yar.h"

// Bit Flag array in which gBitFlags[n] is literally (1 << n)
u32 gBitFlags[] = {
    (1 << 0),  (1 << 1),  (1 << 2),  (1 << 3),  (1 << 4),  (1 << 5),  (1 << 6),  (1 << 7),
    (1 << 8),  (1 << 9),  (1 << 10), (1 << 11), (1 << 12), (1 << 13), (1 << 14), (1 << 15),
    (1 << 16), (1 << 17), (1 << 18), (1 << 19), (1 << 20), (1 << 21), (1 << 22), (1 << 23),
    (1 << 24), (1 << 25), (1 << 26), (1 << 27), (1 << 28), (1 << 29), (1 << 30), (1 << 31),
};

u16 gEquipMasks[] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };
u16 gEquipNegMasks[] = { 0xFFF0, 0xFF0F, 0xF0FF, 0x0FFF };
u32 gUpgradeMasks[] = {
    0x00000007, 0x00000038, 0x000001C0, 0x00000E00, 0x00003000, 0x0001C000, 0x000E0000, 0x00700000,
};
u32 gUpgradeNegMasks[] = {
    0xFFFFFFF8, 0xFFFFFFC7, 0xFFFFFE3F, 0xFFFFF1FF, 0xFFFFCFFF, 0xFFFE3FFF, 0xFFF1FFFF, 0xFF8FFFFF,
};
u8 gEquipShifts[] = { 0, 4, 8, 12 };
u8 gUpgradeShifts[] = { 0, 3, 6, 9, 12, 14, 17, 20 };

u16 gUpgradeCapacities[][4] = {
    { 0, 30, 40, 50 },     // Quivers
    { 0, 20, 30, 40 },     // Bomb Bags
    { 0, 0, 0, 0 },        // Unused (Scale)
    { 0, 0, 0, 0 },        // Unused (Strength)
    { 99, 200, 500, 999 }, // Wallets
    { 0, 30, 40, 50 },     // Deku Seed Bullet Bags
    { 0, 10, 20, 30 },     // Deku Stick Upgrades
    { 0, 20, 30, 40 },     // Deku Nut Upgrades
};

u32 gGsFlagsMasks[] = { 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000 };
u32 gGsFlagsShifts[] = { 0, 8, 16, 24 };

// MergeShip2: the Adult Mask's icon (include/mergeship/AdultMask.hpp, MS_ADULT_MASK_ICON).
static const ALIGN_ASSET(2) char sMergeShipAdultMaskIconTex[] = "__OTR__mergeship/adultmask/gAdultMaskIconTex";

void* gItemIcons[] = {
    gItemIconDekuStickTex,
    gItemIconDekuNutTex,
    gItemIconBombTex,
    gItemIconBowTex,
    gItemIconArrowFireTex,
    gItemIconDinsFireTex,
    gItemIconSlingshotTex,
    gItemIconOcarinaFairyTex,
    gItemIconOcarinaOfTimeTex,
    gItemIconBombchuTex,
    gItemIconHookshotTex,
    gItemIconLongshotTex,
    gItemIconArrowIceTex,
    gItemIconFaroresWindTex,
    gItemIconBoomerangTex,
    gItemIconLensOfTruthTex,
    gItemIconMagicBeanTex,
    gItemIconHammerTex,
    gItemIconArrowLightTex,
    gItemIconNayrusLoveTex,
    gItemIconBottleEmptyTex,
    gItemIconBottlePotionRedTex,
    gItemIconBottlePotionGreenTex,
    gItemIconBottlePotionBlueTex,
    gItemIconBottleFairyTex,
    gItemIconBottleFishTex,
    gItemIconBottleMilkFullTex,
    gItemIconBottleRutosLetterTex,
    gItemIconBottleBlueFireTex,
    gItemIconBottleBugTex,
    gItemIconBottleBigPoeTex,
    gItemIconBottleMilkHalfTex,
    gItemIconBottlePoeTex,
    gItemIconWeirdEggTex,
    gItemIconChickenTex,
    gItemIconZeldasLetterTex,
    gItemIconMaskKeatonTex,
    gItemIconMaskSkullTex,
    gItemIconMaskSpookyTex,
    gItemIconMaskBunnyHoodTex,
    gItemIconMaskGoronTex,
    gItemIconMaskZoraTex,
    gItemIconMaskGerudoTex,
    gItemIconMaskTruthTex,
    gItemIconSoldOutTex,
    gItemIconPocketEggTex,
    gItemIconPocketCuccoTex,
    gItemIconCojiroTex,
    gItemIconOddMushroomTex,
    gItemIconOddPotionTex,
    gItemIconPoachersSawTex,
    gItemIconBrokenGoronsSwordTex,
    gItemIconPrescriptionTex,
    gItemIconEyeballFrogTex,
    gItemIconEyeDropsTex,
    gItemIconClaimCheckTex,
    gItemIconBowFireTex,
    gItemIconBowIceTex,
    gItemIconBowLightTex,
    gItemIconSwordKokiriTex,
    gItemIconSwordMasterTex,
    gItemIconSwordBiggoronTex,
    gItemIconShieldDekuTex,
    gItemIconShieldHylianTex,
    gItemIconShieldMirrorTex,
    gItemIconTunicKokiriTex,
    gItemIconTunicGoronTex,
    gItemIconTunicZoraTex,
    gItemIconBootsKokiriTex,
    gItemIconBootsIronTex,
    gItemIconBootsHoverTex,
    gItemIconBulletBag30Tex,
    gItemIconBulletBag40Tex,
    gItemIconBulletBag50Tex,
    gItemIconQuiver30Tex,
    gItemIconQuiver40Tex,
    gItemIconQuiver50Tex,
    gItemIconBombBag20Tex,
    gItemIconBombBag30Tex,
    gItemIconBombBag40Tex,
    gItemIconGoronsBraceletTex,
    gItemIconSilverGauntletsTex,
    gItemIconGoldenGauntletsTex,
    gItemIconScaleSilverTex,
    gItemIconScaleGoldenTex,
    gItemIconBrokenGiantsKnifeTex,
    gItemIconAdultsWalletTex,
    gItemIconGiantsWalletTex,
    gItemIconDekuSeedsTex,
    gItemIconFishingPoleTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gSongNoteTex,
    gQuestIconMedallionForestTex,
    gQuestIconMedallionFireTex,
    gQuestIconMedallionWaterTex,
    gQuestIconMedallionSpiritTex,
    gQuestIconMedallionShadowTex,
    gQuestIconMedallionLightTex,
    gQuestIconKokiriEmeraldTex,
    gQuestIconGoronRubyTex,
    gQuestIconZoraSapphireTex,
    gQuestIconStoneOfAgonyTex,
    gQuestIconGerudosCardTex,
    gQuestIconGoldSkulltulaTex,
    gQuestIconHeartContainerTex,
    gQuestIconHeartPieceTex,
    gQuestIconDungeonBossKeyTex,
    gQuestIconDungeonCompassTex,
    gQuestIconDungeonMapTex,
    gQuestIconSmallKeyTex,
    gQuestIconMagicJarSmallTex,
    gQuestIconMagicJarBigTex,
    gHeartPieceIcon1Tex,
    gHeartPieceIcon2Tex,
    gHeartPieceIcon3Tex,
    gOcarinaBtnIconCUpTex,
    gOcarinaBtnIconCDownTex,
    gOcarinaBtnIconCLeftTex,
    gOcarinaBtnIconCRightTex,
    gOcarinaBtnIconATex,
    // Push down array to reach newly added item IDs
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "", // ITEM_CUSTOM
    // Start custom items
    gRocsFeatherTex,
    // MergeShip2: Majora's Mask's transformation masks and Fierce Deity's sword, read from mm.o2r.
    mm_gItemIconFierceDeityMaskTex, // ITEM_MM_MASK_FIERCE_DEITY
    mm_gItemIconGoronMaskTex,       // ITEM_MM_MASK_GORON
    mm_gItemIconZoraMaskTex,        // ITEM_MM_MASK_ZORA
    mm_gItemIconDekuMaskTex,        // ITEM_MM_MASK_DEKU
    mm_gItemIconFierceDeitySwordTex, // ITEM_MM_SWORD_FIERCE_DEITY
    // MergeShip2: Majora's Mask's other masks.
    mm_gItemIconMaskOfTruthTex, // ITEM_MM_MASK_TRUTH
    mm_gItemIconKafeisMaskTex, // ITEM_MM_MASK_KAFEIS_MASK
    mm_gItemIconAllNightMaskTex, // ITEM_MM_MASK_ALL_NIGHT
    mm_gItemIconBunnyHoodTex, // ITEM_MM_MASK_BUNNY
    mm_gItemIconKeatonMaskTex, // ITEM_MM_MASK_KEATON
    mm_gItemIconGaroMaskTex, // ITEM_MM_MASK_GARO
    mm_gItemIconRomaniMaskTex, // ITEM_MM_MASK_ROMANI
    mm_gItemIconCircusLeaderMaskTex, // ITEM_MM_MASK_CIRCUS_LEADER
    mm_gItemIconPostmansHatTex, // ITEM_MM_MASK_POSTMAN
    mm_gItemIconCouplesMaskTex, // ITEM_MM_MASK_COUPLE
    mm_gItemIconGreatFairyMaskTex, // ITEM_MM_MASK_GREAT_FAIRY
    mm_gItemIconGibdoMaskTex, // ITEM_MM_MASK_GIBDO
    mm_gItemIconDonGeroMaskTex, // ITEM_MM_MASK_DON_GERO
    mm_gItemIconKamaroMaskTex, // ITEM_MM_MASK_KAMARO
    mm_gItemIconCaptainsHatTex, // ITEM_MM_MASK_CAPTAIN
    mm_gItemIconStoneMaskTex, // ITEM_MM_MASK_STONE
    mm_gItemIconBremenMaskTex, // ITEM_MM_MASK_BREMEN
    mm_gItemIconBlastMaskTex, // ITEM_MM_MASK_BLAST
    mm_gItemIconMaskOfScentsTex, // ITEM_MM_MASK_SCENTS
    mm_gItemIconGiantsMaskTex, // ITEM_MM_MASK_GIANT
    mm_gItemIconChateauRomaniTex, // ITEM_MM_CHATEAU // MergeShip2: MM gItemIcons[ITEM_CHATEAU]
    // MergeShip2: Majora's Mask's other items.
    mm_gItemIconRazorSwordTex, // ITEM_MM_SWORD_RAZOR
    mm_gItemIconGildedSwordTex, // ITEM_MM_SWORD_GILDED
    mm_gItemIconGreatFairysSwordTex, // ITEM_MM_SWORD_GREAT_FAIRY
    mm_gItemIconHerosShieldTex, // ITEM_MM_SHIELD_HERO
    mm_gItemIconMirrorShieldTex, // ITEM_MM_SHIELD_MIRROR
    mm_gItemIconHookshotTex, // ITEM_MM_HOOKSHOT
    mm_gItemIconBowTex, // ITEM_MM_BOW_HERO
    mm_gItemIconPowderKegTex, // ITEM_MM_POWDER_KEG
    mm_gItemIconPictographBoxTex, // ITEM_MM_PICTOGRAPH_BOX
    mm_gItemIconBottledDekuPrincessTex, // ITEM_MM_DEKU_PRINCESS
    mm_gItemIconBottledZoraEggTex, // ITEM_MM_ZORA_EGG
    mm_gItemIconBottledGoldDustTex, // ITEM_MM_GOLD_DUST
    mm_gItemIconBottledMushroomTex, // ITEM_MM_MUSHROOM
    mm_gItemIconBottledSeahorseTex, // ITEM_MM_SEAHORSE
    mm_gItemIconBottledHylianLoachTex, // ITEM_MM_HYLIAN_LOACH
    mm_gItemIconSpringWaterTex, // ITEM_MM_SPRING_WATER
    mm_gItemIconHotSpringWaterTex, // ITEM_MM_HOT_SPRING_WATER
    mm_gItemIconMoonsTearTex, // ITEM_MM_MOONS_TEAR
    mm_gItemIconLandDeedTex, // ITEM_MM_DEED_LAND
    mm_gItemIconSwampDeedTex, // ITEM_MM_DEED_SWAMP
    mm_gItemIconMountainDeedTex, // ITEM_MM_DEED_MOUNTAIN
    mm_gItemIconOceanDeedTex, // ITEM_MM_DEED_OCEAN
    mm_gItemIconRoomKeyTex, // ITEM_MM_ROOM_KEY
    mm_gItemIconLetterToMamaTex, // ITEM_MM_LETTER_TO_MAMA
    mm_gItemIconLetterToKafeiTex, // ITEM_MM_LETTER_TO_KAFEI
    mm_gItemIconPendantOfMemoriesTex, // ITEM_MM_PENDANT_OF_MEMORIES
    mm_gItemIconOdolwasRemainsTex, // ITEM_MM_REMAINS_ODOLWA
    mm_gItemIconGohtsRemainsTex, // ITEM_MM_REMAINS_GOHT
    mm_gItemIconGyorgsRemainsTex, // ITEM_MM_REMAINS_GYORG
    mm_gItemIconTwinmoldsRemainsTex, // ITEM_MM_REMAINS_TWINMOLD
    mm_gItemIconBombersNotebookTex, // ITEM_MM_BOMBERS_NOTEBOOK
    mm_gItemIconBottledFairyTex, // ITEM_MM_STRAY_FAIRY (placeholder: MM has no 32x32 stray fairy item icon)
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_SONATA
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_GORON_LULLABY
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_NEW_WAVE
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_ELEGY
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_OATH
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_HEALING
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_SOARING
    mm_gItemIconSongNoteTex, // ITEM_MM_SONG_GORON_LULLABY_INTRO
    mm_gItemIconKokiriSwordTex, // ITEM_MM_SWORD_KOKIRI
    sMergeShipAdultMaskIconTex, // ITEM_MM_MASK_ADULT (made at startup: soh/MergeShip/MergeShipAdultMask.h)
};
MERGESHIP_STATIC_ASSERT(ARRAY_COUNT(gItemIcons) == ITEM_MM_LAST + 1,
                        "gItemIcons needs an icon for every item up to ITEM_MM_LAST");

// Used to map item IDs to inventory slots
u8 gItemSlots[] = {
    SLOT_STICK,       SLOT_NUT,          SLOT_BOMB,        SLOT_BOW,         SLOT_ARROW_FIRE,  SLOT_DINS_FIRE,
    SLOT_SLINGSHOT,   SLOT_OCARINA,      SLOT_OCARINA,     SLOT_BOMBCHU,     SLOT_HOOKSHOT,    SLOT_HOOKSHOT,
    SLOT_ARROW_ICE,   SLOT_FARORES_WIND, SLOT_BOOMERANG,   SLOT_LENS,        SLOT_BEAN,        SLOT_HAMMER,
    SLOT_ARROW_LIGHT, SLOT_NAYRUS_LOVE,  SLOT_BOTTLE_1,    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,
    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,     SLOT_BOTTLE_1,    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,
    SLOT_BOTTLE_1,    SLOT_BOTTLE_1,     SLOT_BOTTLE_1,    SLOT_TRADE_CHILD, SLOT_TRADE_CHILD, SLOT_TRADE_CHILD,
    SLOT_TRADE_CHILD, SLOT_TRADE_CHILD,  SLOT_TRADE_CHILD, SLOT_TRADE_CHILD, SLOT_TRADE_CHILD, SLOT_TRADE_CHILD,
    SLOT_TRADE_CHILD, SLOT_TRADE_CHILD,  SLOT_TRADE_CHILD, SLOT_TRADE_ADULT, SLOT_TRADE_ADULT, SLOT_TRADE_ADULT,
    SLOT_TRADE_ADULT, SLOT_TRADE_ADULT,  SLOT_TRADE_ADULT, SLOT_TRADE_ADULT, SLOT_TRADE_ADULT, SLOT_TRADE_ADULT,
    SLOT_TRADE_ADULT, SLOT_TRADE_ADULT,
};

void Inventory_ChangeEquipment(s16 equipment, u16 value) {
    gSaveContext.equips.equipment &= gEquipNegMasks[equipment];
    gSaveContext.equips.equipment |= value << gEquipShifts[equipment];

    GameInteractor_ExecuteOnLinkEquipmentChange();
}

u8 Inventory_DeleteEquipment(PlayState* play, s16 equipment) {
    Player* player = GET_PLAYER(play);
    s32 pad;
    u16 equipValue = gSaveContext.equips.equipment & gEquipMasks[equipment];

    // "Erasing equipment item = %d  zzz=%d"
    osSyncPrintf("装備アイテム抹消 = %d  zzz=%d\n", equipment, equipValue);

    if (equipValue) {
        equipValue >>= gEquipShifts[equipment];

        gSaveContext.equips.equipment &= gEquipNegMasks[equipment];
        gSaveContext.inventory.equipment ^= OWNED_EQUIP_FLAG(equipment, equipValue - 1);

        if (equipment == EQUIP_TYPE_TUNIC) {
            gSaveContext.equips.equipment |= EQUIP_VALUE_TUNIC_KOKIRI << (EQUIP_TYPE_TUNIC * 4);
        }

        GameInteractor_ExecuteOnEquipmentDelete(equipment, equipValue);

        if (equipment == EQUIP_TYPE_SWORD) {
            gSaveContext.equips.buttonItems[0] = ITEM_NONE;
            gSaveContext.infTable[29] = 1;
        }

        Player_SetEquipmentData(play, player);
        play->pauseCtx.cursorSpecialPos = PAUSE_CURSOR_PAGE_LEFT;
    }

    return equipValue;
}

void Inventory_ChangeUpgrade(s16 upgrade, s16 value) {
    gSaveContext.inventory.upgrades &= gUpgradeNegMasks[upgrade];
    gSaveContext.inventory.upgrades |= value << gUpgradeShifts[upgrade];
}
