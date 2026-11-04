#pragma once

// MergeShip2 -- "Deku Beans": a Majora's Mask flavour of Ocarina of Time's Magic Beans (MergeShipDekuBeans.cpp).
//
// Like SoH's Mask Select, the Magic Bean slot on the pause Items page can be switched (A, then left/right) between
// Ocarina of Time's Magic Beans and Deku Beans. Deku Beans are the same inventory item (ITEM_BEAN) shown with an
// earthy recoloured icon and their own name; which of the two the slot holds is saved per file. Only Deku Link can
// use Deku Beans: from a C button or the D-pad he plants one on the floor in front of him, anywhere, and a Majora's
// Mask Deku Flower (MM Obj_Etcetera) sprouts at once. Gated by CVAR_ENHANCEMENT("MergeShip.DekuBeans"), default on.

#include <libultraship/libultra/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// The feature's CVar (default on).
s32 MergeShip_DekuBeans_IsEnabled(void);
// The bean slot may be switched to Deku Beans: the feature is on, the Magic Beans are owned and so is the Deku Mask.
s32 MergeShip_DekuBeans_CanSelect(void);
// The pause screen can switch the slot between Hyrule's Magic Beans and the Deku Beans (both there; BUG-0068).
s32 MergeShip_DekuBeans_CanSwitch(void);
// The bean slot holds Deku Beans (ITEM_BEAN then means Deku Beans everywhere: icons, name, use).
s32 MergeShip_DekuBeans_IsSelected(void);
// BUG-0012: Deku Beans as ammo (option on, Deku Beans in the slot): the count of the kind in use. False: unlimited.
s32 MergeShip_DekuBeans_Ammo(s16* out);
// Harness / cross-game: set a kind's count (0..10).
void MergeShip_DekuBeans_Refill(s32 gold, s32 count);
// BUG-0014, the shared save: whether Hyrule has each kind, the counts, and whether they are ammo.
s32 MergeShip_DekuBeans_GoldOwned(void);
s32 MergeShip_DekuBeans_Count(s32 gold);
s32 MergeShip_DekuBeans_AmmoMode(void);
void MergeShip_DekuBeans_SetSelected(s32 selected);

// Icon texture to draw for `texture`: the Deku Beans icon in place of the Magic Bean icon while Deku Beans are
// selected, `texture` otherwise.
void* MergeShip_DekuBeans_SwapIcon(void* texture);
// The Deku Beans icon (the Magic Bean icon when it could not be made).
void* MergeShip_DekuBeans_GetIcon(void);
// Name texture for the pause name panel: the Deku Beans name while the Items page names the bean slot holding Deku
// Beans, `nameSegment` otherwise.
void* MergeShip_DekuBeans_NameTexture(struct PlayState* play, void* nameSegment);

// Progressive Deku Beans' first step (randomizer.cpp): the Magic Beans, ten, as a Magic Bean Pack gives them.
void MergeShip_DekuBeans_GiveMagicBeans(void);
// Its get-item model (item_list.cpp's custom draw): the step it is about to give -- the Magic Beans' own model, or
// that model in the Deku Beans' earth tones or the Gold Deku Beans' gold (the icons' recolours, on its colours).
struct GetItemEntry;
void MergeShip_DekuBeans_DrawGi(struct PlayState* play, struct GetItemEntry* getItemEntry);
// Which step the next copy gives: 1 Magic Beans, 2 Deku Beans, 3 Gold Deku Beans.
s32 MergeShip_DekuBeans_NextStep(void);

#ifdef __cplusplus
}
#endif
