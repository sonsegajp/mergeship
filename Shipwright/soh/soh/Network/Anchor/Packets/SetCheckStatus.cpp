#include <nlohmann/json.hpp>

#include "soh/Network/Anchor/Anchor.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/randomizer/randomizer_check_tracker.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/randomizerEnumStrings.h"
#include "soh/MergeShip/MergeShipHost.h"    // MergeShip2: the shared save
#include "soh/MergeShip/MergeShipUnified.h" // MergeShipUnified_Active

extern "C" {
#include <mergeship/SharedSave.h>
void MergeShipTest_Log(const char* fmt, ...);
}

static bool isResultOfHandling = false;

/**
 * SET_CHECK_STATUS
 *
 * Fired when a check status is updated or skipped
 */

void Anchor::SendPacket_SetCheckStatus(RandomizerCheck rc) {
    if (!IsSaveLoaded() || isResultOfHandling) {
        return;
    }

    auto randoContext = Rando::Context::GetInstance();

    nlohmann::json payload;
    payload["type"] = SET_CHECK_STATUS;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["rc"] = rc;
    payload["status"] = randoContext->GetItemLocation(rc)->GetCheckStatus();
    payload["skipped"] = randoContext->GetItemLocation(rc)->GetIsSkipped();
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_SetCheckStatus(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    auto randoContext = Rando::Context::GetInstance();

    RandomizerCheck rc = payload.at("rc").get<RandomizerCheck>();
    if (rc < 0 || rc >= RC_MAX) {
        SPDLOG_ERROR("[Anchor] SET_CHECK_STATUS: {} out of range", rc);
        return;
    }
    RandomizerCheckStatus status = payload.at("status").get<RandomizerCheckStatus>();
    bool skipped = payload.at("skipped").get<bool>();

    isResultOfHandling = true;

    if (randoContext->GetItemLocation(rc)->GetCheckStatus() != status) {
        randoContext->GetItemLocation(rc)->SetCheckStatus(status);
    }
    if (randoContext->GetItemLocation(rc)->GetIsSkipped() != skipped) {
        randoContext->GetItemLocation(rc)->SetIsSkipped(skipped);
    }

    CheckTracker::RecalculateAllAreaTotals();
    CheckTracker::RecalculateAvailableChecks();
    isResultOfHandling = false;
}

/**
 * MM_SET_CHECK_STATUS (2Ship's; AnchorNet.h)
 *
 * MergeShip2: a Termina check a teammate obtained. Termina's engine marks it in its own file when it runs, but that
 * file is written only at MM's own saves, and while the player is here in Hyrule it only holds the packet -- a restart
 * before Termina runs again forgot the find while its item stayed given (the shared save, the ledger), and finding
 * the check again gave a second copy to the whole team (the user's second Deku Princess, 2026-09-30). So Hyrule keeps
 * it in the shared save at once (MsSharedSave.teamTerminaChecks) by the sender's RandoCheckId ("msCheck"); Termina
 * marks it found whenever it loads or resumes (2Ship Rando::MergeShip::ApplyTeamChecks). Unified seeds only.
 */
void Anchor::HandlePacket_MMSetCheckStatus(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags || !payload.value("obtained", false) ||
        !MergeShipUnified_Active()) {
        return;
    }
    const int32_t check = payload.value("msCheck", 0);
    MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
    if ((check > 0) && MsShared_NoteTeamTerminaCheck(shared, (uint32_t)check)) {
        MergeShipHost_CommitSharedSave();
        MergeShipTest_Log("TEAMCHECK +%d (a teammate's Termina check, noted in Hyrule)", check);
    }
}
