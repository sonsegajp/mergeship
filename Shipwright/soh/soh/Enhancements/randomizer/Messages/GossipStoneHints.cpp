/**
 * This file handles the custom messages for Gossip Stone
 * hints.
 */

#include <soh/OTRGlobals.h>
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/MergeShip/MergeShipMaskMessages.h" // MergeShip2: MM Mask of Truth

extern "C" {
#include <macros.h>
#include <functions.h>
#include <variables.h>
#include "soh/MergeShip/MergeShipMaskReactions.h" // MergeShip2: MM Mask of Truth
extern PlayState* gPlayState;
}

// Resolves a hint's message for textbox display, firing OnRandoHintRevealed so
// observers such as the Hint Tracker know the player has seen the hint.
static CustomMessage ReadHintMessage(RandomizerHint rh, MessageFormat format = MF_AUTO_FORMAT, size_t id = 0) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoHintRevealed>(rh);
    return OTRGlobals::Instance->gRandoContext->GetHint(rh)->GetHintMessage(format, id);
}

void BuildHintStoneMessage(uint16_t* textId, bool* loadFromMessageTable) {
    if ((RAND_GET_OPTION(RSK_GOSSIP_STONE_HINTS).Is(RO_GOSSIP_STONES_NEED_TRUTH) &&
         // MergeShip2: MM Mask of Truth -- reads the stones as OoT's does (MergeShip.TruthGossipStones).
         !MergeShip_ReadsGossipStones(gPlayState)) ||
        (RAND_GET_OPTION(RSK_GOSSIP_STONE_HINTS).Is(RO_GOSSIP_STONES_NEED_STONE) &&
         CHECK_QUEST_ITEM(QUEST_STONE_OF_AGONY) == 0)) {
        return;
    }
    CustomMessage msg;
    Actor* stone = GET_PLAYER(gPlayState)->talkActor;
    RandomizerHint stoneHint = RH_NONE;
    int16_t hintParams = stone->params & 0xFF;

    if (Rando::StaticData::stoneParamsToHint.contains(hintParams)) {
        stoneHint = Rando::StaticData::stoneParamsToHint[hintParams];
    } else if (hintParams == 0x18) {
        for (size_t i = 0; i < ACTORCAT_MAX; i++) {
            if (gPlayState->actorCtx.actorLists[i].length) {
                if (gPlayState->actorCtx.actorLists[i].head->id == 10 &&
                    Rando::StaticData::grottoChestParamsToHint.contains(
                        gPlayState->actorCtx.actorLists[i].head->params)) {
                    stoneHint =
                        Rando::StaticData::grottoChestParamsToHint[gPlayState->actorCtx.actorLists[i].head->params];
                }
            }
        }
    }
    if (stoneHint == RH_NONE) {
        msg = CustomMessage("INVALID STONE. PARAMS: " + std::to_string(hintParams));
    } else {
        msg = ReadHintMessage(stoneHint, MF_AUTO_FORMAT);
        // MergeShip2: MM Mask of Truth -- one extra hint from the seed (MergeShip.TruthExtraHint).
        MergeShip_AppendTruthExtraHint(msg, stoneHint);
    }
    // Remove "Buy " if present.
    msg.Replace("Buy ", "");
    msg.Replace("Acheter: ", "");
    msg.Replace(" kaufen ", "");
    msg.Replace(" kaufen", "");
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void RegisterGossipStoneHints() {
    COND_ID_HOOK(OnOpenText, TEXT_RANDOMIZER_GOSSIP_STONE_HINTS,
                 RAND_GET_OPTION(RSK_GOSSIP_STONE_HINTS).IsNot(RO_GOSSIP_STONES_NONE), BuildHintStoneMessage);
}

static RegisterShipInitFunc initFunc(RegisterGossipStoneHints, { "IS_RANDO" });