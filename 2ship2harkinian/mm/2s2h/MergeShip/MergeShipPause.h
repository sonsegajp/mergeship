#pragma once

// MergeShip2 -- Majora's Mask's pause menu as an N-faced carousel.
//
// Majora's Mask's pause menu is a four-faced box (Item, Map, Quest, Masks): every face is drawn by a block of its own,
// turned by 0, -1.57, -3.14 or +1.57 radians about Y and pushed 93 units out, and the camera is stepped between four
// hard-coded eye points (PAUSE_EYE_DIST = 64 opposite the face it looks at, z_kaleido_scope.h). The merged game adds
// faces, so the box becomes a prism of N faces: every face's turn, distance and eye point is computed from its place on
// the ring, and while only MM's four faces are on it (N = 4) every number is exactly Majora's Mask's -- the same turn
// constants, the same 93, the same integer eye points and eye steps (MM sPageSwitchEyeDx/Dz), so MM's own matrices are
// left untouched.
//
// Ring order (R turns right, Z turns left): Item, Map, Quest, Masks -- MM's own box. Masks stays one step left of Item
// and Map one step right, exactly as in MM. More faces can be registered in sRingFaces (MergeShipPause.c), each with an
// "active" predicate.
// MergeShip2: the Items (Hyrule) page (MERGESHIP_PAUSE_OOT_ITEM) is not a face of its own: C-Up on the Item face switches
// it between MM's Select Item and Ocarina of Time's (z_kaleido_mergeship.c), which then takes the Item face's place on
// the ring (MergeShip_Pause_SetOoTItemView); N stays 4. Each pause screen opens on MM's Select Item.
//
// The same design as Ocarina of Time's side (Shipwright soh/soh/MergeShip/MergeShipPause.h, the "Items (Termina)"
// face), ported to MM's kaleido (z_kaleido_scope_NES.c).

#include "ultra64.h"
#include "z64pause_menu.h"

// PauseContext::pageIndex of the "Items (Hyrule)" page (z_kaleido_oot_item.c, MergeShipOoTItems.h): Ocarina of Time's
// item page for OoT's C-button, mask and trade items -- the Item face's Ocarina of Time view (C-Up). MM's pages are
// PAUSE_ITEM 0 .. PAUSE_MASK 3; PAUSE_WORLD_MAP (4) is a sub-page of the map, used only as a cursor index, so this page
// takes 5 (PAUSE_PAGE_MAX). PauseContext's per-page arrays (cursorPoint/XIndex/YIndex/Item/Slot[PAUSE_PAGE_MAX]) cannot
// be grown, so the page keeps its own cursor state and every per-page array access in the kaleido code is guarded
// against page ids >= PAUSE_PAGE_MAX.
#define MERGESHIP_PAUSE_OOT_ITEM 5
// Room on the ring for faces registered later.
#define MERGESHIP_PAUSE_MAX_FACES 8
// MM's own four faces.
#define MERGESHIP_PAUSE_IS_MM_PAGE(page) ((page) <= PAUSE_MASK)

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Faces on the ring now (4: MM's own; the Item face's place holds MM's or Ocarina of Time's Select Item).
u16 MergeShip_Pause_PageCount(void);
// MergeShip2: whether the Item face shows Ocarina of Time's Select Item (MERGESHIP_PAUSE_OOT_ITEM; needs
// MergeShip_OoTItems_Enabled), and switching it. Switching also moves pageIndex between PAUSE_ITEM and
// MERGESHIP_PAUSE_OOT_ITEM when it is one of them.
s32 MergeShip_Pause_IsOoTItemViewShown(void);
void MergeShip_Pause_SetOoTItemView(PauseContext* pauseCtx, s32 shown);
// Whether only MM's four faces are on the ring (then all geometry is MM's own).
s32 MergeShip_Pause_IsMMRing(void);
// Whether `page` is on the ring now.
s32 MergeShip_Pause_PageOnRing(u16 page);
// One step right (toRight, R) or left (Z) from `page`.
u16 MergeShip_Pause_NextPage(u16 page, s32 toRight);
// MM's page-switch mode is `page * 2 + (left ? 1 : 0)` (PauseContext::nextPageMode); the page it turns to
// (MM sPageSwitchNextPageIndex).
u16 MergeShip_Pause_ModeTarget(u16 mode);
// The eye point (x, z) that looks straight at `page` (MM: PAUSE_EYE_DIST * -PAUSE_<page>_X/Z).
void MergeShip_Pause_GetEye(u16 page, f32* x, f32* z);
// One frame of a page turn: pauseCtx->eye += MM sPageSwitchEyeDx/Dz[nextPageMode] * 2.
void MergeShip_Pause_StepEye(PauseContext* pauseCtx);
// End of a page turn: pageIndex = the mode's target (MM sPageSwitchNextPageIndex) and the eye on its point (MM's eye
// already is there exactly; on the N-gon the float steps are snapped so they cannot drift).
void MergeShip_Pause_FinishSwitch(PauseContext* pauseCtx);
// func_800F4A10 (z_kaleido_setup.c): the eye and page one step right of pageIndex, so the opening turns left onto it
// (MM sKaleidoSetupRightPageEyeX/Z, sKaleidoSetupRightPageIndex). The Item face goes back to MM's Select Item; a page no
// longer on the ring opens as PAUSE_ITEM.
void MergeShip_Pause_SetupOpening(PauseContext* pauseCtx);
// MM gPageSwitchNextButtonStatus[page + 1] (z_message.c): B / C / A button status on `page`. Faces past MM's four use
// the Item page's row.
const u8* MergeShip_Pause_ButtonStatus(u16 page);
// MM's per-face matrix: RotY(turn) Translate(0, y, -dist) Scale(0.78) RotX(-roll / 100), for faces with no block of
// their own in KaleidoScope_DrawPages.
void MergeShip_Pause_LoadPageMatrix(u16 page, f32 y, f32 roll);
// Moves the matrix MM just built for one of its four faces (a face block, the world map, the prompt page) onto that
// face's place on the ring. Does nothing while only MM's four faces are on it.
void MergeShip_Pause_ToRing(u16 page);
// The save / game-over prompt page's matrix: after MM's per-page switch in KaleidoScope_DrawPages (MM's faces), or
// loaded here for faces MM has no case for.
void MergeShip_Pause_PromptMatrix(PauseContext* pauseCtx, f32 y);
// The open/close roll (1/100 rad) of a face that has none in PauseContext: MM's faces all share one value except the
// current one, which the prompt page flips to roll + 314 (KaleidoScope_DrawPages).
f32 MergeShip_Pause_FaceRoll(PauseContext* pauseCtx, u16 page);
// Whether PauseContext's per-page arrays have an entry for `page`.
#define MERGESHIP_PAUSE_HAS_ARRAYS(page) ((page) < PAUSE_PAGE_MAX)

// The Items (Hyrule) page, the Item face's Ocarina of Time view (z_kaleido_oot_item.c).
// KaleidoScope_DrawPages: draws it while it is the view on the Item face's place (current == false: while it is not the
// current face -- not while the Quest face, opposite it, is; true: while it is).
void MergeShip_Kaleido_DrawOoTItemPage(struct PlayState* play, s32 current);
// KaleidoScope_Update: the face's cursor, equip and A button.
void MergeShip_Kaleido_UpdateOoTItemCursor(struct PlayState* play);
// KaleidoScope_UpdateNamePanel for the face. Returns true when it handled the panel (the face is current).
s32 MergeShip_Kaleido_UpdateOoTItemNamePanel(struct PlayState* play);
// KaleidoScope_UpdateCursorSize: the face's cursor position (MM's item grid cursor points).
void MergeShip_Kaleido_SetOoTItemCursorPos(PauseContext* pauseCtx);
// KaleidoScope_UpdateItemEquip's magic arrows (as Ocarina of Time's, for one equipped from the face): the bow cell the
// arrow flies onto -- the face's own for a Hyrule arrow; false (x / y untouched) for Majora's Mask's, which fly onto
// its page's SLOT_BOW -- and the slot the bow with that arrow then goes to the button with (the face's bow cell's
// button slot for a Hyrule arrow, else MM's SLOT_BOW).
s32 MergeShip_Kaleido_OoTMagicArrowBowCell(PauseContext* pauseCtx, s16* x, s16* y);
u16 MergeShip_Kaleido_MagicArrowBowSlot(PauseContext* pauseCtx);
// KaleidoScope_DrawInfoPanel: the "To <page>" label for the page one step left (toRight false) or right of pageIndex.
Gfx* MergeShip_Kaleido_DrawToPageLabel(Gfx* gfx, u16 page, s32 toRight);

#ifdef __cplusplus
}
#endif
