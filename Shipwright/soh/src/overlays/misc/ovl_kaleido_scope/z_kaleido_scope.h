#ifndef Z_KALEIDO_SCOPE_H
#define Z_KALEIDO_SCOPE_H

#include <libultraship/libultra.h>
#include "global.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

extern u8 gAmmoItems[];
extern s16 D_8082AAEC[];
extern s16 D_8082AB2C[];
extern u8 gEquipAgeReqs[][4];
extern u8 gSlotAgeReqs[];
extern u8 gItemAgeReqs[];
extern u8 gAreaGsFlags[];

#define MAP_48x85_TEX_WIDTH 48
#define MAP_48x85_TEX_HEIGHT 85
#define MAP_48x85_TEX_SIZE ((MAP_48x85_TEX_WIDTH * MAP_48x85_TEX_HEIGHT) / 2) // 48x85 CI4 texture

#define AGE_REQ_ADULT LINK_AGE_ADULT
#define AGE_REQ_CHILD LINK_AGE_CHILD
#define AGE_REQ_NONE 9

// MergeShip2: a transformation form wears no Hylian tunic -- like equipment the wrong age can't use, the Goron and
// Zora Tunics are greyed out and can't be equipped while transformed (MergeShipItems.c).
s32 MergeShip_FormMayEquip(s32 equipType, s32 equipValue);
s32 MergeShip_FormMayUseEquipItem(s32 item);
// MergeShip2: with TimelessMMGear, the swords and shields that have MM partners are usable at either age
// (MergeShipMMEquipment.h).
s32 MergeShip_MMEquip_EquipSlotIgnoresAge(s32 equipType, s32 equipValue);

#define CHECK_AGE_REQ_EQUIP(i, j)                                           \
    ((GameInteractor_Should(VB_PLAYER_MEETS_AGE_REQ,                        \
                            (gEquipAgeReqs[i][j] == AGE_REQ_NONE) ||        \
                                (gEquipAgeReqs[i][j] == ((void)0, gSaveContext.linkAge)), \
                            gEquipAgeReqs[i][j]) ||                         \
      MergeShip_MMEquip_EquipSlotIgnoresAge((i), (j))) &&                   \
     MergeShip_FormMayEquip((i), (j)))

#define CHECK_AGE_REQ_SLOT(slotIndex)                                       \
    GameInteractor_Should(VB_SLOT_MEETS_AGE_REQ,                            \
                          (gSlotAgeReqs[slotIndex] == AGE_REQ_NONE) ||      \
                              (gSlotAgeReqs[slotIndex] == ((void)0, gSaveContext.linkAge)), \
                          slotIndex)

#define CHECK_AGE_REQ_ITEM(itemIndex)                                       \
    (GameInteractor_Should(VB_ITEM_MEETS_AGE_REQ,                           \
                           (gItemAgeReqs[itemIndex] == AGE_REQ_NONE) ||     \
                               (gItemAgeReqs[itemIndex] == gSaveContext.linkAge), \
                           itemIndex) &&                                    \
     MergeShip_FormMayUseEquipItem(itemIndex))

void KaleidoScope_DrawQuestStatus(PlayState* play, GraphicsContext* gfxCtx);
s32 KaleidoScope_UpdateQuestStatusPoint(PauseContext* pauseCtx, s32 point);
void KaleidoScope_DrawDebugEditor(PlayState* play);
void KaleidoScope_DrawPlayerWork(PlayState* play);
void KaleidoScope_DrawEquipment(PlayState* play);
void KaleidoScope_SetCursorVtx(PauseContext* pauseCtx, u16 index, Vtx* vtx);
void KaleidoScope_DrawItemSelect(PlayState* play);
void KaleidoScope_SetupItemEquip(PlayState* play, u16 item, u16 slot, s16 animX, s16 animY);
void KaleidoScope_UpdateItemEquip(PlayState* play);
void KaleidoScope_DrawDungeonMap(PlayState* play, GraphicsContext* gfxCtx);
void KaleidoScope_DrawWorldMap(PlayState* play, GraphicsContext* gfxCtx);
void KaleidoScope_UpdatePrompt(PlayState* play);
Gfx* KaleidoScope_QuadTextureIA4(Gfx* gfx, void* texture, s16 width, s16 height, u16 point);
Gfx* KaleidoScope_QuadTextureIA8(Gfx* gfx, void* texture, s16 width, s16 height, u16 point);
void KaleidoScope_MoveCursorToSpecialPos(PlayState* play, u16 specialPos);
void KaleidoScope_DrawQuadTextureRGBA32(GraphicsContext* gfxCtx, void* texture, u16 width, u16 height, u16 point);
void KaleidoScope_ProcessPlayerPreRender();
void KaleidoScope_SetupPlayerPreRender(PlayState* play);
void KaleidoScope_DrawCursor(PlayState* play, u16 pageIndex);
void KaleidoScope_UpdateDungeonMap(PlayState* play);

void PauseMapMark_Draw(PlayState* play);

void KaleidoScope_UpdateCursorSize(PauseContext* pauseCtx);

void KaleidoScope_ResetItemCycling();

// MergeShip2: the Masks page, z_kaleido_mask.c (MM z_kaleido_mask.c).
void MergeShip_Kaleido_DrawMaskSelect(PlayState* play);
void MergeShip_Kaleido_UpdateMaskNamePanel(PlayState* play);
u16 MergeShip_Kaleido_GetMaskCursorItem(void);
void MergeShip_Kaleido_FinishMaskEquip(PlayState* play);

// MergeShip2: the Items (Termina) face, z_kaleido_mm_item.c (MM z_kaleido_item.c for MM's items without an OoT slot).
void MergeShip_Kaleido_DrawMMItemSelect(PlayState* play);
void MergeShip_Kaleido_UpdateMMItemNamePanel(PlayState* play);
u16 MergeShip_Kaleido_GetMMItemCursorItem(void);

// MergeShip2: z_kaleido_equipment.c's Link picture, for the Equipment face's Termina view.
void KaleidoScope_DrawEquipmentImage(PlayState* play, void* source, u32 width, u32 height);
// MergeShip2: the Equipment face's Termina view (MM's equipment), z_kaleido_mergeship.c.
s32 MergeShip_EquipTermina_IsShown(PlayState* play);
void MergeShip_EquipTermina_Draw(PlayState* play);
void MergeShip_EquipTermina_UpdateNamePanel(PlayState* play);
// MergeShip2: C-Down switches the Item, Quest and Equipment faces between Ocarina of Time's screen and Majora's Mask's
// (z_kaleido_mergeship.c). KaleidoScope_DrawPages runs the switch before drawing the faces; KaleidoScope_Draw draws the
// "[C-Down] Termina" / "[C-Down] Hyrule" label after the info panel.
void MergeShip_Kaleido_UpdateWorldSwitch(PlayState* play);
void MergeShip_Kaleido_DrawWorldSwitchLabel(PlayState* play);

#endif
