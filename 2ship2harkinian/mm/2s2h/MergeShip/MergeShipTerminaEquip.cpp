// MergeShip2 -- Termina Equipment: the hooks that keep Termina's record of MM's swords and shields, and the test harness
// commands. See MergeShipTerminaEquip.h.

#include "MergeShipTerminaEquip.h"
#include "MergeShipOoTEquip.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#ifdef MERGESHIP_HOSTED
#include "MergeShipSharedSave.h"
#include <mergeship/SharedSave.h>
#endif

#include <libultraship/bridge/consolevariablebridge.h>

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

// ---- hooks ---------------------------------------------------------------------------------------------------------------
//
// Always on: they only note which of MM's swords and shields Termina holds (MergeShip_TerminaEquip_Observe), so the view
// is right from the moment its option is turned on. Nothing is equipped here.

static void RegisterTerminaEquipment() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, [](Actor* actor) {
        if ((gPlayState != nullptr) && (actor == &GET_PLAYER(gPlayState)->actor)) {
            MergeShip_TerminaEquip_Observe();
        }
    });
    COND_HOOK(ShouldItemGive, true, [](u8 item, bool* should) {
        if (*should) {
            MergeShip_TerminaEquip_BeforeItemGiven(item);
        }
    });
    COND_HOOK(OnItemGive, true, [](u8 item) { MergeShip_TerminaEquip_OnItemGiven(item); });
    COND_HOOK(BeforeEndOfCycleSave, true, []() { MergeShip_TerminaEquip_OnEndOfCycle(); });
    COND_HOOK(OnSaveLoad, true, [](s16 fileNum) { MergeShip_TerminaEquip_ResetObserver(); });
    COND_HOOK(OnSaveInit, true, [](s16 fileNum) { MergeShip_TerminaEquip_ResetObserver(); });
    COND_HOOK(OnKaleidoUpdate, true, [](PauseContext* pauseCtx) {
        // Leaving the pause screen closes the view (the Quest face keeps it while other faces are turned to).
        if (MergeShip_TerminaEquipPause_IsOpen() &&
            ((pauseCtx->state == PAUSE_STATE_OFF) || (pauseCtx->state >= PAUSE_STATE_UNPAUSE_SETUP) ||
             !MergeShip_TerminaEquip_Enabled())) {
            MergeShip_TerminaEquipPause_Close();
        }
    });
}

static RegisterShipInitFunc sTerminaEquipInit(RegisterTerminaEquipment);

// ---- test harness commands (MergeShipTest.cpp) ---------------------------------------------------------------------------

void MergeShipTest_TerminaEquipLogState(const char* tag) {
    Player* player = (gPlayState != nullptr) ? GET_PLAYER(gPlayState) : nullptr;

    MergeShipTest_Log("%s swords=%X shields=%X sword=%d shield=%d B=0x%02X ootSword=%d ootShield=%d razor=%d gfs=%d "
                      "equipped[kokiri=%d razor=%d gilded=%d gfs=%d hero=%d mirror=%d] form=%d heldItem=0x%02X ia=%d "
                      "curShield=%d open=%d enabled=%d",
                      tag, OOT_EQUIP.terminaSwords, OOT_EQUIP.terminaShields, GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD),
                      GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), OOT_EQUIP.sword,
                      OOT_EQUIP.shield, gSaveContext.save.saveInfo.playerData.swordHealth,
                      MergeShip_TerminaEquip_Owns(ITEM_SWORD_GREAT_FAIRY),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SWORD_KOKIRI),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SWORD_RAZOR),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SWORD_GILDED),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SWORD_GREAT_FAIRY),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SHIELD_HERO),
                      MergeShip_TerminaEquip_IsEquipped(ITEM_SHIELD_MIRROR), GET_PLAYER_FORM,
                      (player != nullptr) ? player->heldItemId : 0xFF, (player != nullptr) ? player->heldItemAction : -1,
                      (player != nullptr) ? player->currentShield : -1, MergeShip_TerminaEquipPause_IsOpen(),
                      MergeShip_TerminaEquip_Enabled());
}

// Returns true when `op` is one of these commands:
//   terminaeqstate [tag]      log Termina's swords and shields, the ones in use, B and the player
//   terminaeq <item>          equip as the view's A does (MM item id: 0x4D Kokiri .. 0x4F Gilded, 0x10 Great Fairy's,
//                             0x51 Hero's, 0x52 Mirror)
//   terminaeqtrip [sword] [shield]
//                             a trip through the door without leaving: capture into a fresh shared save, set MM's sword
//                             and shield to what MM alone would have (by default the best held: its own "never a
//                             downgrade"), apply the shared save back, and log what Termina has in use then
//                             (mergeship.exe only)
bool MergeShipTest_TerminaEquipCommand(const std::string& op, const std::vector<std::string>& args) {
    PlayState* play = gPlayState;

    if (op == "terminaeqstate") {
        MergeShipTest_TerminaEquipLogState(args.empty() ? "TERMINAEQSTATE" : args[0].c_str());
        return true;
    }
    if (op == "terminaeq") {
        u8 item = args.empty() ? ITEM_NONE : (u8)strtol(args[0].c_str(), nullptr, 0);
        s32 ok = (play != nullptr) && MergeShip_TerminaEquip_Equip(play, item);
        MergeShipTest_Log("TERMINAEQ equip 0x%02X -> %d", item, ok);
        MergeShipTest_TerminaEquipLogState("TERMINAEQSTATE");
        return true;
    }
    if (op == "terminaeqtrip") {
#ifdef MERGESHIP_HOSTED
        auto shared = std::make_unique<MsSharedSave>();
        *shared = {};
        shared->magic = MS_SHARED_SAVE_MAGIC;
        shared->version = MS_SHARED_SAVE_VERSION;
        shared->size = sizeof(MsSharedSave);
        MergeShipShared_Capture(shared.get());
        MergeShipTest_Log("TERMINAEQ trip captured swords=%X shields=%X sword=0x%02X shield=%d seen=%X/%X valid=%d",
                          shared->terminaSwords, shared->terminaShields, shared->terminaSword, shared->terminaShield,
                          shared->terminaSeenSwords, shared->terminaSeenShields, shared->terminaGearValid);
        // What Termina's own save would say without this block: the best sword and shield it holds.
        s32 sword = (args.size() > 0) ? (s32)strtol(args[0].c_str(), nullptr, 0)
                    : (OOT_EQUIP.terminaSwords & 4) ? EQUIP_VALUE_SWORD_GILDED
                    : (OOT_EQUIP.terminaSwords & 2) ? EQUIP_VALUE_SWORD_RAZOR
                                                    : EQUIP_VALUE_SWORD_KOKIRI;
        s32 shield = (args.size() > 1) ? (s32)strtol(args[1].c_str(), nullptr, 0)
                     : (OOT_EQUIP.terminaShields & 2) ? EQUIP_VALUE_SHIELD_MIRROR
                                                      : EQUIP_VALUE_SHIELD_HERO;
        SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, sword);
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, shield);
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_SWORD_KOKIRI + sword - EQUIP_VALUE_SWORD_KOKIRI;
        MergeShipTest_TerminaEquipLogState("TERMINAEQTRIP scrambled");
        MergeShipShared_Apply(shared.get(), 0);
        if (play != nullptr) {
            Interface_LoadItemIcon(play, EQUIP_SLOT_B);
            Player_SetEquipmentData(play, GET_PLAYER(play));
        }
        MergeShipTest_TerminaEquipLogState("TERMINAEQTRIP applied");
#else
        MergeShipTest_Log("TERMINAEQ trip: 2ship.exe has no shared save (mergeship.exe only)");
#endif
        return true;
    }
    return false;
}
