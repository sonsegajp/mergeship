/*
 * File: z_kaleido_mergeship.c
 * Overlay: ovl_kaleido_scope
 * Description: MergeShip2 -- the Hyrule quest status sub-page (MergeShipHyrule.h).
 *
 * Majora's Mask's quest page has no room for Ocarina of Time's medallions, Spiritual Stones, Stone of Agony, Gerudo
 * Card and songs, so C-Up on it switches to a sub-page with Ocarina of Time's quest page layout for them (OoT
 * z_kaleido_collect.c KaleidoScope_DrawQuestStatus, OoT's quest vertices from z_kaleido_scope_PAL.c
 * D_8082B138 / D_8082B198 / D_8082B1F8, OoT's cursor links D_8082A1AC, OoT's song colours, OoT's icons and item names
 * from oot/textures), and back. The two games share the page coordinates (a page of 3 x 5 panels of 80 x 32 around the
 * origin), so Ocarina of Time's numbers are used as they are. Points Ocarina of Time has on its page but Majora's Mask
 * shows on its own (Gold Skulltula count, heart pieces) are skipped. Epona's Song, the Song of Time, the Song of Storms,
 * Saria's Song and the Sun's Song show where Ocarina of Time puts them, from Majora's Mask's own quest bits.
 *
 * Mirrors SoH's Termina sub-page (Shipwright soh/src/overlays/misc/ovl_kaleido_scope/z_kaleido_mergeship.c).
 *
 * C-Up cycles the Quest face through its views: Termina's quest status (MM's) -> Hyrule's quest status (this
 * sub-page) -> Hyrule's equipment (Ocarina of Time's equipment page, MergeShipOoTEquipPause.c) -> Termina's equipment
 * (MM's swords and shields, MergeShipTerminaEquipPause.c; SoH's Termina view of its Equipment page) -> Termina's. The
 * Hyrule views are drawn on Ocarina of Time's own page panels (oot/textures icon_item_static / icon_item_nes_static),
 * Termina's equipment on Ocarina of Time's equipment panels in MM's page colour (as SoH draws it).
 * C-Up on the Item face switches it between MM's Select Item and Ocarina of Time's (z_kaleido_oot_item.c), as SoH's
 * Item face switches to Majora's Mask's.
 */

#include "z_kaleido_scope.h"
#include "interface/parameter_static/parameter_static.h"
#include "2s2h/MergeShip/MergeShipHyrule.h"
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipPause.h"
#include "2s2h/MergeShip/MergeShipTerminaEquip.h"
#include <libultraship/bridge/consolevariablebridge.h>

// Ocarina of Time's quest points (OoT QuestItem order), the sub-page's cursor points.
#define HQ_MEDALLION_FOREST 0
#define HQ_MEDALLION_LIGHT 5
#define HQ_SONG_MINUET 6
#define HQ_SONG_PRELUDE 11
#define HQ_SONG_LULLABY 12
#define HQ_SONG_EPONA 13
#define HQ_SONG_SARIA 14
#define HQ_SONG_SUN 15
#define HQ_SONG_TIME 16
#define HQ_SONG_STORMS 17
#define HQ_KOKIRI_EMERALD 18
#define HQ_ZORA_SAPPHIRE 20
#define HQ_STONE_OF_AGONY 21
#define HQ_GERUDO_CARD 22
#define HQ_SKULL_TOKEN 23 // Ocarina of Time's; not on this sub-page
#define HQ_HEART_PIECE 24 // Ocarina of Time's; not on this sub-page
#define HQ_POINTS 23
#define HQ_ALL_POINTS 25

#define HQ_NONE -1
#define HQ_TO_RIGHT -2
#define HQ_TO_LEFT -3

// OoT D_8082B138 / D_8082B198 / D_8082B1F8 (z_kaleido_scope_PAL.c), points 0..24.
static s16 sHyruleQuestX[HQ_ALL_POINTS] = { 74,   74,  46,  18,  18,  46,   -108, -90,  -72, -54, -36, -18, -108,
                                            -90,  -72, -54, -36, -18, 20,   46,   72,   -110, -86, -110, -54 };
static s16 sHyruleQuestY[HQ_ALL_POINTS] = { 38, 6,   -12, 6,   38,  56,  -20, -20, -20, -20, -20, -20, 2,
                                            2,  2,   2,   2,   2,   -46, -46, -46, 58,  58,  34,  58 };
static s16 sHyruleQuestSize[HQ_ALL_POINTS] = { 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
                                               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 48 };

// OoT D_8082A1AC: up, down, left, right. 0xFF none, 0xFE page right, 0xFD page left.
static s8 sHyruleQuestLinks[HQ_ALL_POINTS][4] = {
    { 0x05, 0x01, 0x05, 0xFE }, { 0x00, 0x02, 0x02, 0xFE }, { 0xFF, 0x13, 0x03, 0x01 }, { 0x04, 0x02, 0x11, 0x02 },
    { 0x05, 0x03, 0x18, 0x05 }, { 0xFF, 0xFF, 0x04, 0x00 }, { 0x0C, 0xFF, 0xFD, 0x07 }, { 0x0D, 0xFF, 0x06, 0x08 },
    { 0x0E, 0xFF, 0x07, 0x09 }, { 0x0F, 0xFF, 0x08, 0x0A }, { 0x10, 0xFF, 0x09, 0x0B }, { 0x11, 0xFF, 0x0A, 0x12 },
    { 0x17, 0x06, 0xFD, 0x0D }, { 0x17, 0x07, 0x0C, 0x0E }, { 0x17, 0x08, 0x0D, 0x0F }, { 0x18, 0x09, 0x0E, 0x10 },
    { 0x18, 0x0A, 0x0F, 0x11 }, { 0x18, 0x0B, 0x10, 0x03 }, { 0x02, 0xFF, 0x0B, 0x13 }, { 0x02, 0xFF, 0x12, 0x14 },
    { 0x02, 0xFF, 0x13, 0xFE }, { 0xFF, 0x17, 0xFD, 0x16 }, { 0xFF, 0x17, 0x15, 0x18 }, { 0x15, 0x0C, 0xFD, 0x18 },
    { 0xFF, 0x10, 0x16, 0x04 },
};

// OoT D_8082A164 / D_8082A17C / D_8082A194: the songs' colours (Minuet .. Storms).
static s16 sHyruleSongPrim[12][3] = {
    { 150, 255, 100 }, { 255, 80, 40 },   { 100, 150, 255 }, { 255, 160, 0 },   { 255, 100, 255 }, { 255, 240, 100 },
    { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 },
};

// OoT D_8082A090 / D_8082A0D8.. and ZREG(61..64): the medallions' pulsing glow.
static s16 sHyruleMedallionEnvTargets[12][3] = {
    { 0, 0, 0 },  { 0, 0, 0 },  { 0, 0, 0 },    { 0, 0, 0 },   { 0, 0, 0 },   { 0, 0, 0 },
    { 0, 60, 0 }, { 90, 0, 0 }, { 0, 40, 110 }, { 80, 40, 0 }, { 70, 0, 90 }, { 90, 90, 0 },
};
static s16 sHyruleMedallionEnv[6][3] = {
    { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 }, { 255, 255, 150 },
};
static s16 sHyruleMedallionTimerInit[4] = { 120, 60, 2, 80 }; // MM's sQuestRemainsColorTimerInit (OoT ZREG(61..64))
static s16 sHyruleMedallionTimer = 20;
static s16 sHyruleMedallionTimerIndex = 0;

// MM pauseCtx->ocarinaButtonsY by OCARINA_BTN_*, and the eight button columns under the page.
static s16 sHyruleSongButtonY[5] = { -62, -56, -49, -46, -41 };
static s16 sHyruleSongButtonX[8] = { -98, -86, -74, -62, -50, -38, -26, -14 };

static s32 sHyruleQuestShown = false;
static s16 sHyruleQuestCursor = HQ_MEDALLION_FOREST;
static u16 sSavedMMQuestCursorItem = PAUSE_ITEM_NONE;
static s32 sHyrulePlayback = false;

// Name panel ids for the sub-page's points (MM's cursorItem / namedItem; Kaleido_LoadItemNameStatic asks for the
// texture): the ItemIds where the item has one, 0x300 + point otherwise.
#define HQ_NAME_ID_BASE 0x300

// The ocarina song a point plays, or 0xFF when not learned / not a song.
static u8 MergeShip_QuestHyrule_PointSong(s16 point) {
    switch (point) {
        case HQ_SONG_LULLABY:
            return MergeShip_Hyrule_HasQuestItem(MERGESHIP_OOT_QUEST_SONG_LULLABY) ? OCARINA_SONG_ZELDAS_LULLABY
                                                                                   : 0xFF;
        case HQ_SONG_EPONA:
            return CHECK_QUEST_ITEM(QUEST_SONG_EPONA) ? OCARINA_SONG_EPONAS : 0xFF;
        case HQ_SONG_SARIA:
            return CHECK_QUEST_ITEM(QUEST_SONG_SARIA) ? OCARINA_SONG_SARIAS : 0xFF;
        case HQ_SONG_SUN:
            return CHECK_QUEST_ITEM(QUEST_SONG_SUN) ? OCARINA_SONG_SUNS : 0xFF;
        case HQ_SONG_TIME:
            return CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? OCARINA_SONG_TIME : 0xFF;
        case HQ_SONG_STORMS:
            return CHECK_QUEST_ITEM(QUEST_SONG_STORMS) ? OCARINA_SONG_STORMS : 0xFF;
        default:
            if ((point >= HQ_SONG_MINUET) && (point <= HQ_SONG_PRELUDE) &&
                MergeShip_Hyrule_HasQuestItem(MERGESHIP_OOT_QUEST_SONG_MINUET + (point - HQ_SONG_MINUET))) {
                return MERGESHIP_SONG_MINUET + (point - HQ_SONG_MINUET);
            }
            return 0xFF;
    }
}

static s32 MergeShip_QuestHyrule_PointOwned(s16 point) {
    return (point >= 0) && (point < HQ_POINTS) && MergeShip_Hyrule_HasQuestItem((u8)point);
}

static u16 MergeShip_QuestHyrule_PointItem(s16 point) {
    if (!MergeShip_QuestHyrule_PointOwned(point)) {
        return PAUSE_ITEM_NONE;
    }
    return HQ_NAME_ID_BASE + point;
}

// Ocarina of Time's item names (oot/textures/item_name_static, 128 x 16 IA4 like Majora's Mask's).
static const char* sHyruleQuestNames[HQ_POINTS] = {
    OOT_ASSET("textures/item_name_static/gForestMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gFireMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gWaterMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSpiritMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gShadowMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gLightMedallionItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gMinuetOfForestItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gBoleroOfFireItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSerenadeOfWaterItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gRequiemOfSpiritItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gNocturneOfShadowItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gPreludeOfLightItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gZeldasLullabyItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gEponasSongItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSariasSongItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSunsSongItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSongOfTimeItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gSongOfStormsItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gKokiriEmeraldItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gGoronsRubyItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gZorasSapphireItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gStoneofAgonyItemNameENGTex"),
    OOT_ASSET("textures/item_name_static/gGerudosCardItemNameENGTex"),
};

// Ocarina of Time's quest icons (oot/textures/icon_item_24_static, 24 x 24 RGBA32).
static const char* sHyruleQuestIcons[HQ_POINTS] = {
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionForestTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionFireTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionWaterTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionSpiritTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionShadowTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconMedallionLightTex"),
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // songs: gSongNoteTex
    OOT_ASSET("textures/icon_item_24_static/gQuestIconKokiriEmeraldTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconGoronRubyTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconZoraSapphireTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconStoneOfAgonyTex"),
    OOT_ASSET("textures/icon_item_24_static/gQuestIconGerudosCardTex"),
};

const char* MergeShip_Hyrule_QuestIconTexture(u8 ootQuest) {
    return (ootQuest < HQ_POINTS) ? sHyruleQuestIcons[ootQuest] : NULL;
}

const char* MergeShip_QuestHyrule_NameTexture(u32 namedItem) {
    if (!sHyruleQuestShown || (namedItem < HQ_NAME_ID_BASE) || (namedItem >= HQ_NAME_ID_BASE + HQ_POINTS)) {
        return NULL;
    }
    return sHyruleQuestNames[namedItem - HQ_NAME_ID_BASE];
}

void MergeShip_QuestHyrule_Reset(void) {
    sHyruleQuestShown = false;
    sHyrulePlayback = false;
}

void MergeShip_QuestHyrule_Set(s32 shown, s16 point) {
    PlayState* play = gPlayState;

    if ((point >= 0) && (point < HQ_POINTS)) {
        sHyruleQuestCursor = point;
    }
    if (shown && !sHyruleQuestShown && (play != NULL)) {
        sSavedMMQuestCursorItem = play->pauseCtx.cursorItem[PAUSE_QUEST];
    }
    sHyruleQuestShown = shown;
    if ((play != NULL) && shown) {
        play->pauseCtx.cursorItem[PAUSE_QUEST] = MergeShip_QuestHyrule_PointItem(sHyruleQuestCursor);
    }
}

s32 MergeShip_QuestHyrule_IsShown(PlayState* play) {
    if (!MergeShip_Hyrule_Enabled() || !MergeShip_IsOcarinaMounted()) {
        sHyruleQuestShown = false;
        return false;
    }
    if (play->pauseCtx.state == PAUSE_STATE_OFF) {
        MergeShip_QuestHyrule_Reset();
        return false;
    }
    // C-Up switches it (MergeShip_QuestPage_PreUpdate, below).
    return sHyruleQuestShown;
}

// ==== C-Up: the Quest face's views ===============================================================================
//
// C-Up on the Quest face cycles Termina's quest status (MM's own) -> Hyrule's quest status (this sub-page) ->
// Hyrule's equipment (MergeShipOoTEquipPause.c) -> Termina's equipment (MergeShipTerminaEquipPause.c) -> Termina's
// quest status, skipping a view whose option is off (MERGESHIP_OOT_SONGS_CVAR, MERGESHIP_OOT_EQUIP_CVAR,
// MERGESHIP_TERMINA_EQUIP_CVAR). Termina's equipment comes right after Hyrule's, so C-Up turns Hyrule's equipment page
// to Termina's as SoH's C-Up turns its Equipment page to its Termina view, and every other C-Up keeps the place it had.
// B on either equipment view goes back to Termina's quest status without closing the pause screen. The press is taken
// there and then (KaleidoScope_Update, before the pause screen's own input), so nothing else on the page sees it.
// 2S2H's Song Items (Enhancements > Songs) keeps C-Up while the cursor rests on a learned song of MM's quest status (it
// puts the song on C-Up); the label is grey then. A label under the page, right of the name panel, names the next view:
// "[C-Up] Hyrule", "[C-Up] Equipment" (the next equipment view, from a quest status), "[C-Up] Termina" (Termina's
// equipment from Hyrule's, or Termina's quest status from a Hyrule view), "[C-Up] Quest Status" (from Termina's
// equipment). Each pause screen opens on Termina's quest status.

typedef enum {
    MS_QUESTVIEW_TERMINA,       // MM's quest status
    MS_QUESTVIEW_HYRULE_QUEST,  // Ocarina of Time's quest status (this sub-page)
    MS_QUESTVIEW_HYRULE_EQUIP,  // Ocarina of Time's equipment page (MergeShipOoTEquipPause.c)
    MS_QUESTVIEW_TERMINA_EQUIP, // MM's swords and shields (MergeShipTerminaEquipPause.c)
    MS_QUESTVIEW_MAX
} MergeShipQuestView;

static s32 MergeShip_QuestPage_ViewAvailable(s32 view) {
    switch (view) {
        case MS_QUESTVIEW_HYRULE_QUEST:
            return MergeShip_Hyrule_Enabled() && MergeShip_IsOcarinaMounted();
        case MS_QUESTVIEW_HYRULE_EQUIP:
            return MergeShip_OoTEquip_Enabled();
        case MS_QUESTVIEW_TERMINA_EQUIP:
            return MergeShip_TerminaEquip_Enabled();
        default:
            return true;
    }
}

static s32 MergeShip_QuestPage_View(void) {
    if (MergeShip_TerminaEquipPause_IsOpen() && MergeShip_QuestPage_ViewAvailable(MS_QUESTVIEW_TERMINA_EQUIP)) {
        return MS_QUESTVIEW_TERMINA_EQUIP;
    }
    if (MergeShip_OoTEquipPause_IsOpen() && MergeShip_QuestPage_ViewAvailable(MS_QUESTVIEW_HYRULE_EQUIP)) {
        return MS_QUESTVIEW_HYRULE_EQUIP;
    }
    if (sHyruleQuestShown && MergeShip_QuestPage_ViewAvailable(MS_QUESTVIEW_HYRULE_QUEST)) {
        return MS_QUESTVIEW_HYRULE_QUEST;
    }
    return MS_QUESTVIEW_TERMINA;
}

static s32 MergeShip_QuestPage_NextView(s32 view) {
    s32 i;

    for (i = 1; i < MS_QUESTVIEW_MAX; i++) {
        s32 next = (view + i) % MS_QUESTVIEW_MAX;

        if (MergeShip_QuestPage_ViewAvailable(next)) {
            return next;
        }
    }
    return view;
}

// 2S2H's Song Items: C-Up on a learned song of MM's quest status puts the song on the button.
static s32 MergeShip_QuestPage_SongEquipHover(PauseContext* pauseCtx) {
    return CVarGetInteger("gEnhancements.Songs.SongItems", 0) &&
           (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG);
}

static void MergeShip_QuestPage_SetView(PlayState* play, s32 view) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 old = MergeShip_QuestPage_View();

    if (old == view) {
        return;
    }
    switch (old) {
        case MS_QUESTVIEW_TERMINA:
            // MM's cursor item, given back on return; MM's song hover (and its ocarina) ends.
            sSavedMMQuestCursorItem = pauseCtx->cursorItem[PAUSE_QUEST];
            pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
            break;
        case MS_QUESTVIEW_HYRULE_QUEST:
            sHyruleQuestShown = false;
            sHyrulePlayback = false;
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
            break;
        case MS_QUESTVIEW_TERMINA_EQUIP:
            MergeShip_TerminaEquipPause_Close();
            break;
        default:
            MergeShip_OoTEquipPause_Close();
            // Its A label "Put Away" (on the equipped Ocarina of Time sword / shield) does not stay on the next view.
            if (play->interfaceCtx.aButtonDoAction == DO_ACTION_PUTAWAY) {
                Interface_SetAButtonDoAction(play, DO_ACTION_DECIDE);
            }
            break;
    }

    // Off a page arrow, onto the view's own cell (each view keeps its cursor).
    if (pauseCtx->cursorSpecialPos != 0) {
        KaleidoScope_MoveCursorFromSpecialPos(play);
    }

    switch (view) {
        case MS_QUESTVIEW_HYRULE_QUEST:
            sHyruleQuestShown = true;
            pauseCtx->cursorItem[PAUSE_QUEST] = MergeShip_QuestHyrule_PointItem(sHyruleQuestCursor);
            break;
        case MS_QUESTVIEW_HYRULE_EQUIP:
            MergeShip_OoTEquipPause_Open(play);
            break;
        case MS_QUESTVIEW_TERMINA_EQUIP:
            MergeShip_TerminaEquipPause_Open(play);
            break;
        default:
            pauseCtx->cursorItem[PAUSE_QUEST] = sSavedMMQuestCursorItem;
            break;
    }
    pauseCtx->cursorShrinkRate = 4.0f;
    pauseCtx->nameDisplayTimer = 0;
}

// ==== C-Up: the Item face's views ================================================================================
//
// C-Up on the Item face switches it between MM's Select Item and Ocarina of Time's (the Items (Hyrule) page,
// z_kaleido_oot_item.c), which takes the Item face's place on the ring (MergeShip_Pause_SetOoTItemView) -- as SoH's
// Item face switches to Majora's Mask's. MM's C-Up is Tatl's, never an item button on the pause screen, so it meets no
// equipping there: C-Left / C-Down / C-Right equip from either view. The switch works on a cell or a page arrow (the
// cursor stays where it is); the "[C-Up] Hyrule" / "[C-Up] Termina" label under the page names the other view. Each
// pause screen opens on MM's Select Item (MergeShip_Pause_SetupOpening). Needs MergeShip_OoTItems_Enabled.

// C-Up would switch the Item face now.
static s32 MergeShip_ItemPage_CanSwitch(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    return ((pauseCtx->pageIndex == PAUSE_ITEM) || (pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM)) &&
           MergeShip_OoTItems_Enabled() && (pauseCtx->state == PAUSE_STATE_MAIN) &&
           (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) && !pauseCtx->itemDescriptionOn &&
           (pauseCtx->debugEditor == DEBUG_EDITOR_NONE);
}

static void MergeShip_ItemPage_PreUpdate(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s32 toHyrule;

    // The Ocarina of Time view went away with its option: MM's Select Item is the face again.
    if ((pauseCtx->pageIndex == MERGESHIP_PAUSE_OOT_ITEM) && !MergeShip_Pause_IsOoTItemViewShown()) {
        pauseCtx->pageIndex = PAUSE_ITEM;
    }
    if (!MergeShip_ItemPage_CanSwitch(play) || (play->msgCtx.msgLength != 0) ||
        !CHECK_BTN_ALL(input->press.button, BTN_CUP)) {
        return;
    }
    toHyrule = (pauseCtx->pageIndex == PAUSE_ITEM);
    MergeShip_Pause_SetOoTItemView(pauseCtx, toHyrule);
    // Nothing else on the page sees this press.
    input->press.button &= ~BTN_CUP;
    // The new view names its own item (KaleidoScope_UpdateNamePanel reloads when namedItem differs).
    pauseCtx->namedItem = PAUSE_ITEM_NONE;
    pauseCtx->nameDisplayTimer = 0;
    pauseCtx->cursorShrinkRate = 4.0f;
    // MM's page-turn sound (KaleidoScope_SwitchPage).
    Audio_PlaySfx(toHyrule ? NA_SE_SY_WIN_SCROLL_LEFT : NA_SE_SY_WIN_SCROLL_RIGHT);
    MergeShipTest_Log("ITEMPAGE view=%s page=%d special=%d", toHyrule ? "hyrule" : "termina", pauseCtx->pageIndex,
                      pauseCtx->cursorSpecialPos);
}

void MergeShip_QuestPage_PreUpdate(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s32 view;
    s32 next;

    // MergeShip2: C-Up on the Item face (above).
    MergeShip_ItemPage_PreUpdate(play);

    // Each pause screen opens on Termina's quest status, and closes back to it.
    if ((pauseCtx->state <= PAUSE_STATE_OPENING_2) || (pauseCtx->state >= PAUSE_STATE_UNPAUSE_SETUP)) {
        MergeShip_QuestHyrule_Reset();
        MergeShip_OoTEquipPause_Close();
        MergeShip_TerminaEquipPause_Close();
        return;
    }
    if ((pauseCtx->pageIndex != PAUSE_QUEST) || (pauseCtx->state != PAUSE_STATE_MAIN) ||
        ((pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE) &&
         (pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG)) ||
        pauseCtx->itemDescriptionOn || (play->msgCtx.msgLength != 0) ||
        (pauseCtx->debugEditor != DEBUG_EDITOR_NONE)) {
        return;
    }
    view = MergeShip_QuestPage_View();

    // B on Hyrule's or Termina's equipment: back to Termina's quest status (MM's B would close the pause screen).
    if (((view == MS_QUESTVIEW_HYRULE_EQUIP) || (view == MS_QUESTVIEW_TERMINA_EQUIP)) &&
        CHECK_BTN_ALL(input->press.button, BTN_B)) {
        MergeShip_QuestPage_SetView(play, MS_QUESTVIEW_TERMINA);
        input->press.button &= ~BTN_B;
        Audio_PlaySfx(NA_SE_SY_WIN_SCROLL_RIGHT);
        MergeShipTest_Log("QUESTPAGE view=%d (B) hyrule=%d", MS_QUESTVIEW_TERMINA, sHyruleQuestShown);
        return;
    }

    // C-Up (MM's C-Up is Tatl's, unused on the pause screen, so it never meets 2S2H's song equipping).
    if (!CHECK_BTN_ALL(input->press.button, BTN_CUP)) {
        return;
    }
    next = MergeShip_QuestPage_NextView(view);
    if (next == view) {
        return;
    }
    MergeShip_QuestPage_SetView(play, next);
    input->press.button &= ~BTN_CUP;
    // MM's page-turn sound (KaleidoScope_SwitchPage).
    Audio_PlaySfx((next == MS_QUESTVIEW_TERMINA) ? NA_SE_SY_WIN_SCROLL_RIGHT : NA_SE_SY_WIN_SCROLL_LEFT);
    MergeShipTest_Log("QUESTPAGE view=%d hyrule=%d cursor=%d", next, sHyruleQuestShown, sHyruleQuestCursor);
}

// Ocarina of Time's page panels (OoT z_kaleido_scope_PAL.c sQuestStatusENGTexs / sEquipmentENGTexs), IA8 80 x 32 as
// MM's; the English titles.
static TexturePtr sOoTQuestPageBgTextures[] = {
    OOT_ASSET("textures/icon_item_nes_static/gPauseQuestStatus00ENGTex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus01Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus02Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus03Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus04Tex"),
    OOT_ASSET("textures/icon_item_nes_static/gPauseQuestStatus10ENGTex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus11Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus12Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus13Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus14Tex"),
    OOT_ASSET("textures/icon_item_nes_static/gPauseQuestStatus20ENGTex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus21Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus22Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus23Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseQuestStatus24Tex"),
};
static TexturePtr sOoTEquipPageBgTextures[] = {
    OOT_ASSET("textures/icon_item_static/gPauseEquipment00Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment01Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment02Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment03Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment04Tex"),
    OOT_ASSET("textures/icon_item_nes_static/gPauseEquipment10ENGTex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment11Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment12Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment13Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment14Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment20Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment21Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment22Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment23Tex"),
    OOT_ASSET("textures/icon_item_static/gPauseEquipment24Tex"),
};

Gfx* MergeShip_QuestPage_DrawBg(Gfx* gfx, Vtx* vertices, TexturePtr* mmTextures) {
    s32 view = MergeShip_QuestPage_View();

    if (view == MS_QUESTVIEW_TERMINA) {
        return KaleidoScope_DrawPageSections(gfx, vertices, mmTextures);
    }
    if (view == MS_QUESTVIEW_TERMINA_EQUIP) {
        // Termina's equipment: Ocarina of Time's equipment panels in MM's page colour (the primitive colour MM set for
        // its own panels), as SoH draws its Termina view (KaleidoScope_SetMMPanelColor).
        return KaleidoScope_DrawPageSections(gfx, vertices, sOoTEquipPageBgTextures);
    }
    // Ocarina of Time's panels in Ocarina of Time's page colour (OoT func_80823A0C pageColors, the Quest Status page's
    // 120 / 120 / 70 and the Equipment page's 90 / 100 / 60 at their brightest) as MM's primitive colour.
    gDPPipeSync(gfx++);
    if (view == MS_QUESTVIEW_HYRULE_QUEST) {
        gDPSetPrimColor(gfx++, 0, 0, 120, 120, 70, 255);
    } else {
        gDPSetPrimColor(gfx++, 0, 0, 90, 100, 60, 255);
    }
    return KaleidoScope_DrawPageSections(
        gfx, vertices, (view == MS_QUESTVIEW_HYRULE_QUEST) ? sOoTQuestPageBgTextures : sOoTEquipPageBgTextures);
}

// The Hyrule views' cursor cell in page coordinates: left, top, right, bottom.
static s16 sQuestPageCursorQuad[4] = { 0, 0, 0, 0 };

void MergeShip_QuestPage_SetCursorQuad(s16 left, s16 top, s16 width, s16 height) {
    sQuestPageCursorQuad[0] = left;
    sQuestPageCursorQuad[1] = top;
    sQuestPageCursorQuad[2] = left + width;
    sQuestPageCursorQuad[3] = top - height;
}

s32 MergeShip_QuestPage_SetCursorPos(PauseContext* pauseCtx) {
    f32 centerX;
    f32 centerY;
    f32 size;

    if (MergeShip_QuestPage_View() == MS_QUESTVIEW_TERMINA) {
        return false;
    }
    centerX = (sQuestPageCursorQuad[0] + sQuestPageCursorQuad[2]) / 2.0f;
    centerY = (sQuestPageCursorQuad[1] + sQuestPageCursorQuad[3]) / 2.0f;
    // MM's cursor points (sQuestStatusCursorsX / Y) are the page's points as the cursor's view sees them:
    // x = 0.81 * page x + 3, y = 0.8 * page y - 3.5 (fitted to MM's own: the remains, sword, shield, songs, notebook
    // and bags, within a unit or two). MM's circles sit half a cell plus 2 from the centre (a 32 cell: 18).
    pauseCtx->cursorX = 0.81f * centerX + 3.0f;
    pauseCtx->cursorY = 0.8f * centerY - 3.5f;
    size = (f32)MAX(sQuestPageCursorQuad[2] - sQuestPageCursorQuad[0],
                    sQuestPageCursorQuad[1] - sQuestPageCursorQuad[3]);
    pauseCtx->cursorWidth = pauseCtx->cursorHeight = size / 2.0f + 2.0f;
    return true;
}

// ---- the label ---------------------------------------------------------------------------------------------------

extern const char* fontTbl[]; // z_kanfont.c: nes_font_static's characters, from ' '
extern f32 sNESFontWidths[];  // z_message_nes.c: their advance at scale 1

// Under the page, right of the name panel (screen coordinates, 320 x 240), in MM's message font at 0.6. The C-Up
// glyph (nes_font_static 0xB7) is drawn in the C buttons' yellow, the text white; both grey while C-Up is 2S2H's
// song equipping instead.
#define MS_LABEL_RIGHT 282
#define MS_LABEL_TOP 204
#define MS_LABEL_SCALE 0.6f
#define MS_FONT_CHAR_CDOWN 0xB6 // the C-Up glyph (nes_font_static 0xB6)

static Gfx* MergeShip_QuestPage_DrawLabelChar(Gfx* gfx, u8 character, s32 x, s32 y, u8 r, u8 g, u8 b) {
    s32 size = (s32)(16.0f * MS_LABEL_SCALE);
    s32 step = (s32)(1024.0f / MS_LABEL_SCALE);

    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, (TexturePtr)fontTbl[character - ' '], G_IM_FMT_I, 16, 16, 0,
                           G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    // The message box's drop shadow.
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
    gSPTextureRectangle(gfx++, (x + 1) << 2, (y + 1) << 2, (x + 1 + size) << 2, (y + 1 + size) << 2, G_TX_RENDERTILE,
                        0, 0, step, step);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, 255);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + size) << 2, (y + size) << 2, G_TX_RENDERTILE, 0, 0, step, step);
    return gfx;
}

static void MergeShip_Kaleido_DrawWorldLabel(PlayState* play, const char* label, s32 dim) {
    const char* c;
    f32 width;
    f32 x;

    width = sNESFontWidths[MS_FONT_CHAR_CDOWN - ' '] + 2.0f;
    for (c = label; *c != '\0'; c++) {
        width += sNESFontWidths[(u8)*c - ' '];
    }
    x = MS_LABEL_RIGHT - width * MS_LABEL_SCALE;

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL39_Overlay(play->state.gfxCtx);
    // The message box's text: the colour is the primitive's, the glyph its alpha.
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    OVERLAY_DISP = MergeShip_QuestPage_DrawLabelChar(OVERLAY_DISP, MS_FONT_CHAR_CDOWN, (s32)x, MS_LABEL_TOP,
                                                     dim ? 120 : 255, dim ? 120 : 255, dim ? 60 : 50);
    x += (sNESFontWidths[MS_FONT_CHAR_CDOWN - ' '] + 2.0f) * MS_LABEL_SCALE;
    for (c = label; *c != '\0'; c++) {
        OVERLAY_DISP = MergeShip_QuestPage_DrawLabelChar(OVERLAY_DISP, (u8)*c, (s32)x, MS_LABEL_TOP, dim ? 120 : 255,
                                                         dim ? 120 : 255, dim ? 120 : 255);
        x += sNESFontWidths[(u8)*c - ' '] * MS_LABEL_SCALE;
    }

    CLOSE_DISPS(play->state.gfxCtx);
}

void MergeShip_QuestPage_DrawSwitchLabel(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const char* label;
    s32 view;
    s32 next;

    // MergeShip2: the Item face's "[C-Up] Hyrule" (MM's Select Item) / "[C-Up] Termina" (Ocarina of Time's).
    if (MergeShip_ItemPage_CanSwitch(play)) {
        MergeShip_Kaleido_DrawWorldLabel(play, (pauseCtx->pageIndex == PAUSE_ITEM) ? "Hyrule" : "Termina", false);
        return;
    }

    if ((pauseCtx->pageIndex != PAUSE_QUEST) || (pauseCtx->state != PAUSE_STATE_MAIN) ||
        ((pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE) &&
         (pauseCtx->mainState != PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG)) ||
        pauseCtx->itemDescriptionOn || (pauseCtx->debugEditor != DEBUG_EDITOR_NONE)) {
        return;
    }
    view = MergeShip_QuestPage_View();
    next = MergeShip_QuestPage_NextView(view);
    if (next == view) {
        return;
    }
    if (next == MS_QUESTVIEW_TERMINA) {
        label = (view == MS_QUESTVIEW_TERMINA_EQUIP) ? "Quest Status" : "Termina";
    } else if (next == MS_QUESTVIEW_TERMINA_EQUIP) {
        // From Hyrule's equipment it is Termina's (SoH's "[C-Up] Termina" on its Equipment page).
        label = (view == MS_QUESTVIEW_HYRULE_EQUIP) ? "Termina" : "Equipment";
    } else {
        label = ((next == MS_QUESTVIEW_HYRULE_EQUIP) && (view == MS_QUESTVIEW_HYRULE_QUEST)) ? "Equipment" : "Hyrule";
    }
    MergeShip_Kaleido_DrawWorldLabel(play, label, false); // C-Up never equips a song
}

// Walks Ocarina of Time's link table one direction from `from`, past the points that are not on this sub-page.
static s16 MergeShip_QuestHyrule_Walk(s16 from, s32 dir) {
    s16 next = sHyruleQuestLinks[from][dir];
    s32 guard = 0;

    while ((next >= HQ_POINTS) && (++guard < HQ_ALL_POINTS)) {
        next = sHyruleQuestLinks[next][dir];
    }
    return next;
}

// KaleidoScope_UpdateQuestCursor while the sub-page is shown (OoT's quest cursor, MM's page arrows).
void MergeShip_QuestHyrule_UpdateCursor(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = CONTROLLER1(&play->state);
    s16 old = sHyruleQuestCursor;
    s16 next = HQ_NONE;
    u8 song;

    pauseCtx->nameColorSet = PAUSE_NAME_COLOR_SET_WHITE;
    pauseCtx->cursorColorSet = PAUSE_CURSOR_COLOR_SET_WHITE;

    if ((pauseCtx->state != PAUSE_STATE_MAIN) || (pauseCtx->pageIndex != PAUSE_QUEST) || pauseCtx->itemDescriptionOn) {
        return;
    }

    if (pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) {
        if (pauseCtx->stickAdjX > 30) {
            // OoT: back from the left arrow onto the Stone of Agony.
            KaleidoScope_MoveCursorFromSpecialPos(play);
            sHyruleQuestCursor = HQ_STONE_OF_AGONY;
        }
    } else if (pauseCtx->cursorSpecialPos != 0) {
        if (pauseCtx->stickAdjX < -30) {
            // OoT: back from the right arrow onto the Forest Medallion.
            KaleidoScope_MoveCursorFromSpecialPos(play);
            sHyruleQuestCursor = HQ_MEDALLION_FOREST;
        }
    } else if (pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE) {
        if (pauseCtx->stickAdjX < -30) {
            next = MergeShip_QuestHyrule_Walk(old, 2);
        } else if (pauseCtx->stickAdjX > 30) {
            next = MergeShip_QuestHyrule_Walk(old, 3);
        }
        if (next == HQ_TO_LEFT) {
            sHyrulePlayback = false;
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
            next = HQ_NONE;
        } else if (next == HQ_TO_RIGHT) {
            sHyrulePlayback = false;
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
            next = HQ_NONE;
        } else if (next < 0) {
            if (pauseCtx->stickAdjY < -30) {
                next = MergeShip_QuestHyrule_Walk(old, 1);
            } else if (pauseCtx->stickAdjY > 30) {
                next = MergeShip_QuestHyrule_Walk(old, 0);
            }
            if (next < 0) {
                next = HQ_NONE;
            }
        }
        if (next >= 0) {
            sHyruleQuestCursor = next;
            sHyrulePlayback = false;
            pauseCtx->cursorShrinkRate = 4.0f;
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }

        // A on a learned song plays it back (OoT/MM pause song playback), on the ocarina's default instrument.
        song = MergeShip_QuestHyrule_PointSong(sHyruleQuestCursor);
        if ((song != 0xFF) && CHECK_BTN_ALL(input->press.button, BTN_A)) {
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
            AudioOcarina_SetPlaybackSong(song + 1, 1);
            pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
            pauseCtx->ocarinaStaff->pos = 0;
            sHyrulePlayback = true;
            MergeShipTest_Log("QUESTPAGE playback song=%d", song);
        }
    }

    pauseCtx->cursorItem[PAUSE_QUEST] =
        (pauseCtx->cursorSpecialPos == 0) ? MergeShip_QuestHyrule_PointItem(sHyruleQuestCursor) : PAUSE_ITEM_NONE;
}

// A quad of 4 vertices in page coordinates: top-left (x, y), w x h, texture tw x th.
static void MergeShip_QuestHyrule_SetQuad(Vtx* v, s16 x, s16 y, s16 w, s16 h, s16 tw, s16 th, u8 alpha) {
    s32 i;

    v[0].v.ob[0] = v[2].v.ob[0] = x;
    v[1].v.ob[0] = v[3].v.ob[0] = x + w;
    v[0].v.ob[1] = v[1].v.ob[1] = y;
    v[2].v.ob[1] = v[3].v.ob[1] = y - h;
    for (i = 0; i < 4; i++) {
        v[i].v.ob[2] = 0;
        v[i].v.flag = 0;
        v[i].v.cn[0] = v[i].v.cn[1] = v[i].v.cn[2] = 255;
        v[i].v.cn[3] = alpha;
    }
    v[0].v.tc[0] = v[0].v.tc[1] = v[1].v.tc[1] = v[2].v.tc[0] = 0;
    v[1].v.tc[0] = v[3].v.tc[0] = tw << 5;
    v[2].v.tc[1] = v[3].v.tc[1] = th << 5;
}

void MergeShip_QuestHyrule_Draw(PlayState* play) {
    static TexturePtr sOcarinaButtonTextures[] = {
        gOcarinaATex, gOcarinaCDownTex, gOcarinaCRightTex, gOcarinaCLeftTex, gOcarinaCUpTex,
    };
    PauseContext* pauseCtx = &play->pauseCtx;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Vtx* vtx = GRAPH_ALLOC(gfxCtx, (HQ_POINTS + 8) * 4 * sizeof(Vtx));
    OcarinaSongButtons* buttons;
    s16 i;
    s16 k;
    u8 song;

    // OoT KaleidoScope_InitVertices, questVtx: the medallions as they are, the rest inset by 2 (the songs 16 wide).
    for (i = 0; i < HQ_POINTS; i++) {
        s16 size = sHyruleQuestSize[i];
        s16 width = ((i >= HQ_SONG_MINUET) && (i <= HQ_SONG_STORMS)) ? 16 : size;

        if (i <= HQ_MEDALLION_LIGHT) {
            MergeShip_QuestHyrule_SetQuad(&vtx[i * 4], sHyruleQuestX[i], sHyruleQuestY[i] + pauseCtx->offsetY, size,
                                          size, size, size, pauseCtx->alpha);
        } else {
            MergeShip_QuestHyrule_SetQuad(&vtx[i * 4], sHyruleQuestX[i] + 2, sHyruleQuestY[i] + pauseCtx->offsetY - 2,
                                          width - 4, size - 4, width, size, pauseCtx->alpha);
        }
    }
    for (i = 0; i < 8; i++) {
        MergeShip_QuestHyrule_SetQuad(&vtx[(HQ_POINTS + i) * 4], sHyruleSongButtonX[i] + 2, -52, 12, 12, 16, 16,
                                      pauseCtx->alpha);
    }

    // MM's cursor goes round the point's quad (KaleidoScope_UpdateCursorSize, MergeShip_QuestPage_SetCursorPos).
    if (pauseCtx->pageIndex == PAUSE_QUEST) {
        Vtx* q = &vtx[sHyruleQuestCursor * 4];

        MergeShip_QuestPage_SetCursorQuad(q[0].v.ob[0], q[0].v.ob[1] - pauseCtx->offsetY, q[1].v.ob[0] - q[0].v.ob[0],
                                          q[0].v.ob[1] - q[2].v.ob[1]);
    }

    OPEN_DISPS(gfxCtx);

    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);

    // OoT: the medallions, their glow pulsing.
    sHyruleMedallionTimer--;
    for (i = 0; i < 6; i++) {
        if ((sHyruleMedallionTimerIndex != 1) && (sHyruleMedallionTimerIndex != 3)) {
            s16* target = sHyruleMedallionEnvTargets[(sHyruleMedallionTimerIndex != 0) ? (i + 6) : i];

            for (k = 0; k < 3; k++) {
                if (sHyruleMedallionTimer != 0) {
                    s16 step = ABS_ALT(sHyruleMedallionEnv[i][k] - target[k]) / sHyruleMedallionTimer;
                    sHyruleMedallionEnv[i][k] += (sHyruleMedallionEnv[i][k] >= target[k]) ? -step : step;
                } else {
                    sHyruleMedallionEnv[i][k] = target[k];
                }
            }
        }
        if (MergeShip_Hyrule_HasQuestItem((u8)i)) {
            gDPPipeSync(POLY_OPA_DISP++);
            gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
            gDPSetEnvColor(POLY_OPA_DISP++, sHyruleMedallionEnv[i][0], sHyruleMedallionEnv[i][1],
                           sHyruleMedallionEnv[i][2], 0);
            gSPVertex(POLY_OPA_DISP++, &vtx[i * 4], 4, 0);
            KaleidoScope_DrawTexQuadRGBA32(gfxCtx, (TexturePtr)sHyruleQuestIcons[i], 24, 24, 0);
        }
    }
    if (sHyruleMedallionTimer == 0) {
        sHyruleMedallionTimer = sHyruleMedallionTimerInit[sHyruleMedallionTimerIndex];
        if (++sHyruleMedallionTimerIndex > 3) {
            sHyruleMedallionTimerIndex = 0;
        }
    }

    // OoT: the songs (gSongNoteTex, IA8 16 x 24, OoT's colours; the one under the cursor drawn larger).
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    gDPLoadTextureBlock(POLY_OPA_DISP++, OOT_ASSET("textures/icon_item_static/gSongNoteTex"), G_IM_FMT_IA, G_IM_SIZ_8b,
                        16, 24, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                        G_TX_NOLOD, G_TX_NOLOD);
    for (i = HQ_SONG_MINUET; i <= HQ_SONG_STORMS; i++) {
        Vtx* q = &vtx[i * 4];

        if (MergeShip_QuestHyrule_PointSong(i) == 0xFF) {
            continue;
        }
        if ((i == sHyruleQuestCursor) && (pauseCtx->cursorSpecialPos == 0)) {
            q[0].v.ob[0] = q[2].v.ob[0] = q[0].v.ob[0] - 2;
            q[1].v.ob[0] = q[3].v.ob[0] = q[1].v.ob[0] + 4;
            q[0].v.ob[1] = q[1].v.ob[1] = q[0].v.ob[1] + 2;
            q[2].v.ob[1] = q[3].v.ob[1] = q[2].v.ob[1] - 4;
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, sHyruleSongPrim[i - HQ_SONG_MINUET][0],
                        sHyruleSongPrim[i - HQ_SONG_MINUET][1], sHyruleSongPrim[i - HQ_SONG_MINUET][2],
                        pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, q, 4, 0);
        gSP1Quadrangle(POLY_OPA_DISP++, 0, 2, 3, 1, 0);
    }

    // OoT: the Spiritual Stones, the Stone of Agony and the Gerudo Card.
    gDPPipeSync(POLY_OPA_DISP++);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
    gDPSetEnvColor(POLY_OPA_DISP++, 0, 0, 0, 255);
    for (i = HQ_KOKIRI_EMERALD; i <= HQ_GERUDO_CARD; i++) {
        if (MergeShip_Hyrule_HasQuestItem((u8)i)) {
            gSPVertex(POLY_OPA_DISP++, &vtx[i * 4], 4, 0);
            KaleidoScope_DrawTexQuadRGBA32(gfxCtx, (TexturePtr)sHyruleQuestIcons[i], 24, 24, 0);
        }
    }

    // The song's buttons under the page while the cursor rests on a learned song, or as the playback sounds them.
    if ((pauseCtx->state == PAUSE_STATE_MAIN) && (pauseCtx->pageIndex == PAUSE_QUEST) &&
        (pauseCtx->cursorSpecialPos == 0) &&
        ((song = MergeShip_QuestHyrule_PointSong(sHyruleQuestCursor)) != 0xFF) &&
        ((buttons = MergeShip_Ocarina_GetSongButtons(song)) != NULL)) {
        s16 shown = buttons->numButtons;

        if (sHyrulePlayback) {
            pauseCtx->ocarinaStaff = AudioOcarina_GetPlaybackStaff();
            shown = (pauseCtx->ocarinaStaff->state == 0) ? buttons->numButtons : pauseCtx->ocarinaStaff->pos;
            if (pauseCtx->ocarinaStaff->state == 0) {
                sHyrulePlayback = false;
            }
        }
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        for (i = 0; (i < buttons->numButtons) && (i < 8) && (i < shown); i++) {
            Vtx* q = &vtx[(HQ_POINTS + i) * 4];
            u8 button = buttons->buttonIndex[i];

            q[0].v.ob[1] = q[1].v.ob[1] = sHyruleSongButtonY[button];
            q[2].v.ob[1] = q[3].v.ob[1] = q[0].v.ob[1] - 12;
            gDPPipeSync(POLY_OPA_DISP++);
            if (button == OCARINA_BTN_A) {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 80, 150, 255, 200);
            } else {
                gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 50, 200);
            }
            gSPVertex(POLY_OPA_DISP++, q, 4, 0);
            POLY_OPA_DISP = Gfx_DrawTexQuadIA8(POLY_OPA_DISP, sOcarinaButtonTextures[button], 16, 16, 0);
        }
    }

    CLOSE_DISPS(gfxCtx);

    // The cursor (MM draws it after the page, at the quad set here).
    KaleidoScope_SetCursorVtxPos(pauseCtx, sHyruleQuestCursor * 4, vtx);
}
