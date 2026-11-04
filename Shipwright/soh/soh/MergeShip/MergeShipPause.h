#pragma once

// MergeShip2 -- the pause menu as an N-faced carousel.
//
// Ocarina of Time's pause menu is a four-faced box (Item, Map, Quest, Equipment) drawn by four
// hard-coded blocks, each translated along an axis and turned by 0, +-1.57 or 3.14 radians, with the
// camera stepped between four hard-coded eye positions. Majora's Mask is also four-faced, with its
// Masks page where Hyrule's Equipment page is. The merged game needs both, so the box becomes a prism
// of N faces: every face's position, turn and eye point is computed from its place on the ring, and
// for N = 4 the numbers are exactly Ocarina of Time's.
//
// Ring order (R turns right, Z/L left): Item, Map, Quest, Equipment, Masks. The Masks face sits
// between Equipment and Item, so from the Item page one step left reaches it -- the step that reaches
// MM's Masks page from its Item page. Without mm.o2r mounted there is no Masks page and N = 4.
// MergeShip2: the Items (Termina) page (MERGESHIP_PAUSE_MM_ITEM below) is not a face of its own: C-Down on the Item face
// switches it between Ocarina of Time's Select Item and Majora's Mask's (z_kaleido_mergeship.c), which then takes the
// Item face's place on the ring (MergeShip_Pause_SetMMItemView). N stays 5.

#include "z64.h"

// PauseContext::pageIndex of the Masks face. PAUSE_WORLD_MAP (4) is a sub-page of the map, used only
// as a cursor index, so the new face takes 5. PauseContext's per-page arrays (cursorItem[4],
// cursorSlot[4], cursorPoint[5] ...) are sized for OoT's pages and live in z64.h, which this port must
// not grow: the Masks page keeps its own cursor state (z_kaleido_mask.c) and every per-page array
// access in the kaleido code is guarded against this value.
#define MERGESHIP_PAUSE_MASK 5
// MergeShip2: the "Items (Termina)" page (z_kaleido_mm_item.c, MergeShipMMItems.h): MM's item page for the MM items
// that have no slot in Ocarina of Time's full 24-slot grid. It is the Item face's Majora's Mask view: C-Down on the Item
// face (on an empty slot or a page arrow; on an item C-Down equips it) switches between the two, and while it is shown
// it is the page in the ring's first place (Item, Map, Quest, Equipment, Masks). Available while mm.o2r is mounted and
// CVAR_ENHANCEMENT("MergeShip.MMItems") is on. Like the Masks face it keeps its own cursor state and is guarded
// wherever the kaleido code indexes a per-page array.
#define MERGESHIP_PAUSE_MM_ITEM 6
#define MERGESHIP_PAUSE_MAX_PAGES 6
// The largest pageIndex a face can have (the page ids are not contiguous: PAUSE_WORLD_MAP is 4).
#define MERGESHIP_PAUSE_LAST_PAGE_ID MERGESHIP_PAUSE_MM_ITEM

#ifdef __cplusplus
extern "C" {
#endif

// Faces on the ring: 5, or 4 without mm.o2r.
u16 MergeShip_Pause_PageCount(void);
// MergeShip2: whether the Item face shows Majora's Mask's Select Item (MERGESHIP_PAUSE_MM_ITEM), and switching it.
// Switching also moves pageIndex between PAUSE_ITEM and MERGESHIP_PAUSE_MM_ITEM when it is one of them.
s32 MergeShip_Pause_IsMMItemViewShown(void);
void MergeShip_Pause_SetMMItemView(PauseContext* pauseCtx, s32 shown);
// One step right (toRight) or left from `page`.
u16 MergeShip_Pause_NextPage(u16 page, s32 toRight);
// OoT's page-switch mode is `page * 2 + (left ? 1 : 0)`; the page it turns to.
u16 MergeShip_Pause_ModeTarget(u16 mode);
// The eye position (x, z) that looks straight at `page`.
void MergeShip_Pause_GetEye(u16 page, f32* x, f32* z);
// The point (x, z) the ring's camera at eye (eyeX, eyeZ) looks at: the centre, as in Ocarina of Time, or, once the
// eye stands past the centre (six faces), a point beyond the eye, so it still faces the page.
void MergeShip_Pause_GetLookAt(f32 eyeX, f32 eyeZ, f32* x, f32* z);
// The Masks page's name panel: " (MM)" after MM's masks' names (MergeShipMaskNames.cpp).
void* MergeShip_MaskNames_NameTexture(struct PlayState* play, void* nameSegment);
// Moves pauseCtx->eye `fraction` of the way from the mode's page to its target (OoT: 4/64 per step).
void MergeShip_Pause_StepEye(PauseContext* pauseCtx, f32 fraction);
// Loads the page's model matrix: `boxRadius` is the face distance of Ocarina of Time's square box
// (WREG(3) / 100, or the save page's unk_1F0 / 10), `flip` the page's open/close angle in 1/100 rad
// (unk_1F4 & co.), with OoT's 0.78 scale.
void MergeShip_Pause_LoadPageMatrix(u16 page, f32 boxRadius, f32 y, f32 flip);

#ifdef __cplusplus
}
#endif
