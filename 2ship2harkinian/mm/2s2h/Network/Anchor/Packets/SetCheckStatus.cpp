#include <nlohmann/json.hpp>

#include "2s2h/Network/Anchor/Anchor.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h" // MergeShip2: the team's checks

extern "C" {
#include "variables.h"
}

/**
 * MM_SET_CHECK_STATUS (SoH: SET_CHECK_STATUS)
 *
 * A randomizer check a teammate obtained or skipped (the item itself comes with MM_GIVE_ITEM). By name, so both
 * sides agree whatever the order of their check tables. MergeShip2 adds "msCheck", the RandoCheckId itself: Hyrule's
 * engine (SoH) notes a teammate's Termina check in the shared save by it (MsSharedSave.teamTerminaChecks), having no
 * table of 2Ship's names.
 */

void Anchor::SendPacket_MMSetCheckStatus(s32 randoCheckId, bool obtained, bool skipped) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }
    auto it = Rando::StaticData::Checks.find((RandoCheckId)randoCheckId);
    if (it == Rando::StaticData::Checks.end()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = MM_SET_CHECK_STATUS;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["rc"] = it->second.name;
    payload["msCheck"] = randoCheckId; // MergeShip2 (above)
    payload["obtained"] = obtained;
    payload["skipped"] = skipped;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_MMSetCheckStatus(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !IS_RANDO) {
        return;
    }

    const std::string name = payload.value("rc", std::string());
    RandoCheckId rc = Rando::StaticData::GetCheckIdFromName(name.c_str());
    if (rc <= RC_UNKNOWN || rc >= RC_MAX) {
        SPDLOG_ERROR("[Anchor] MM_SET_CHECK_STATUS: unknown check {}", name);
        return;
    }

    RandoSaveCheck& check = RANDO_SAVE_CHECKS[rc];
    if (payload.value("obtained", false)) {
        // Obtained by the team: not given again here (the item came in its own packet).
        check.obtained = true;
        check.cycleObtained = true;
        check.eligible = false;
        // MergeShip2: and kept in the shared save at once -- this file is written only at MM's own saves, and a
        // restart before one reopened the check while its item stayed given (UnifiedDelivery.h, NoteTeamCheck).
        Rando::MergeShip::NoteTeamCheck(rc);
    }
    check.skipped = payload.value("skipped", false);

    // Not sent back (PollGameState compares against this).
    if (lastCheckObtained.size() == RC_MAX) {
        lastCheckObtained[rc] = (check.obtained ? 1 : 0) | (check.skipped ? 2 : 0);
    }
}

void Anchor::MergeShip_NoteCheckStatus(s32 randoCheckId) {
    if ((randoCheckId > RC_UNKNOWN) && (randoCheckId < RC_MAX) && (lastCheckObtained.size() == RC_MAX)) {
        const RandoSaveCheck& check = RANDO_SAVE_CHECKS[randoCheckId];
        lastCheckObtained[randoCheckId] = (check.obtained ? 1 : 0) | (check.skipped ? 2 : 0);
    }
}

void MergeShipAnchor_NoteCheckStatus(s32 randoCheckId) {
    if (Anchor::Instance != nullptr) {
        Anchor::Instance->MergeShip_NoteCheckStatus(randoCheckId);
    }
}
