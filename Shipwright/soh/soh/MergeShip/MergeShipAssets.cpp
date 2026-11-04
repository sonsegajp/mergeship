#include "MergeShipAssets.h"
#include <mergeship/AssetArchive.h>

#include <filesystem>
#include <string>
#include <cstring>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/ResourceManager.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"

#include "soh/OTRGlobals.h"
#include "soh/MergeShip/MergeShipMMExtract.h"
#include "soh/resource/type/Animation.h"

namespace {

bool sMajorasMaskMounted = false;

// A resource that only a correctly namespaced mm.o2r can contain: the Goron form's skeleton.
// Used to tell a MergeShip2 archive apart from a stock 2 Ship 2 Harkinian one, whose paths
// would collide with Hyrule's rather than sit beside them.
constexpr const char* kNamespaceProbe = MERGESHIP_MM_NAMESPACE "/objects/object_link_goron/gLinkGoronSkel";

} // namespace

extern "C" int MergeShip_IsMajorasMaskMounted(void) {
    return sMajorasMaskMounted ? 1 : 0;
}

bool MergeShip_IsNamespacedMajorasMaskArchive(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    auto archive = std::make_shared<MergeShip::AssetArchive>(path, MERGESHIP_MM_NAMESPACE);
    archive->Load();
    const bool usable = archive->IsLoaded() && archive->HasFile(std::string(kNamespaceProbe));
    archive->Unload(); // closes the file: the caller may replace it
    return usable;
}

void MergeShip_MountMajorasMask(const std::shared_ptr<Ship::ArchiveManager>& archiveManager) {
    sMajorasMaskMounted = false;
    std::string mmPath = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", appShortName);

    if (!std::filesystem::exists(mmPath)) {
        SPDLOG_INFO("MergeShip: no mm.o2r found");
        // Offer to build one from the player's own Majora's Mask ROM (bundled mm-extractor).
        if (!MergeShip_OfferMajorasMaskExtraction("")) {
            SPDLOG_INFO("MergeShip: Majora's Mask content will be unavailable");
            return;
        }
        mmPath = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", appShortName);
    }

    auto archive = std::make_shared<MergeShip::AssetArchive>(mmPath, MERGESHIP_MM_NAMESPACE);
    archive->Load();
    // Validate before mounting: rejection must never change native resource ownership.
    if (!archive->IsLoaded() || !archive->HasFile(std::string(kNamespaceProbe))) {
        SPDLOG_ERROR("MergeShip: {} is not a complete namespaced MM archive; not mounted", mmPath);
        // A stock 2 Ship 2 Harkinian mm.o2r: offer to build MergeShip's from the ROM instead.
        archive = nullptr;
        if (!MergeShip_OfferMajorasMaskExtraction(mmPath)) {
            return;
        }
        mmPath = Ship::Context::LocateFileAcrossAppDirs("mm.o2r", appShortName);
        archive = std::make_shared<MergeShip::AssetArchive>(mmPath, MERGESHIP_MM_NAMESPACE);
        archive->Load();
        if (!archive->IsLoaded() || !archive->HasFile(std::string(kNamespaceProbe))) {
            SPDLOG_ERROR("MergeShip: the newly built {} is not a complete namespaced MM archive", mmPath);
            return;
        }
    }
    if (archiveManager->AddArchive(archive) == nullptr) {
        SPDLOG_ERROR("MergeShip: failed to mount {}", mmPath);
        return;
    }

    sMajorasMaskMounted = true;
    SPDLOG_INFO("MergeShip: mounted {} under the \"{}/\" namespace", mmPath, MERGESHIP_MM_NAMESPACE);
}

void MergeShip_VerifyMajorasMaskAssets() {
    if (!sMajorasMaskMounted) {
        return;
    }

    // The archive containing a file and SoH being able to *parse* it are different claims. MM's
    // skeletons were written by 2 Ship 2 Harkinian's exporter and are read here by Ship of
    // Harkinian's factories; nothing guarantees the two agree until it is tried. Doing it at
    // startup turns a would-be crash on first transformation into one line in the log.
    //
    // This runs after OTRGlobals::Initialize registers the resource factories, because before
    // that point every load fails for an unrelated reason.
    static const char* kFormSkeletons[] = {
        MM_ASSET("objects/object_link_nuts/gLinkDekuSkel"),
        MM_ASSET("objects/object_link_goron/gLinkGoronSkel"),
        MM_ASSET("objects/object_link_zora/gLinkZoraSkel"),
        MM_ASSET("objects/object_link_boy/gLinkFierceDeitySkel"),
    };

    size_t loaded = 0;
    for (const char* path : kFormSkeletons) {
        // Skip the "__OTR__" marker the game-side macros carry; the resource manager wants the
        // bare path.
        const char* resPath = path + strlen("__OTR__");
        auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(resPath);
        if (res == nullptr) {
            SPDLOG_ERROR("MergeShip: form skeleton {} is in mm.o2r but did not load", resPath);
        } else {
            loaded++;
        }
    }

    SPDLOG_INFO("MergeShip: {} of {} transformation skeletons loaded from mm.o2r", loaded,
                sizeof(kFormSkeletons) / sizeof(kFormSkeletons[0]));

    // Check real native form motions, including their referenced frame streams.
    static const char* kFormAnimations[] = {
        "mm/objects/gameplay_keep/gPlayerAnim_pg_wait",
        "mm/objects/gameplay_keep/gPlayerAnim_pz_wait",
        "mm/objects/gameplay_keep/gPlayerAnim_pn_attack",
        "mm/objects/gameplay_keep/gPlayerAnim_pg_maru_change",
        "mm/objects/gameplay_keep/gPlayerAnim_link_fighter_wait_long",
    };
    size_t animationsLoaded = 0;
    for (const char* path : kFormAnimations) {
        auto animation = std::dynamic_pointer_cast<SOH::Animation>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
        if (animation && animation->type == SOH::AnimationType::Link &&
            animation->animationData.linkAnimationHeader.common.frameCount > 0 &&
            animation->animationData.linkAnimationHeader.segment != nullptr) {
            ++animationsLoaded;
        } else {
            SPDLOG_ERROR("MergeShip: native MM player animation or frame stream failed: {}", path);
        }
    }
    SPDLOG_INFO("MergeShip: {} of {} native MM player animations and frame streams loaded", animationsLoaded,
                sizeof(kFormAnimations) / sizeof(kFormAnimations[0]));

    // Echo the two CVars this bring-up is driven by, so a configuration that never reaches the
    // CVar system is distinguishable from one that reaches it and is then ignored.
    SPDLOG_INFO("MergeShip: config -- MergeShipForm={} BootSequence={}",
                CVarGetInteger(CVAR_GENERAL("MergeShipForm"), -1),
                CVarGetInteger(CVAR_SETTING("BootSequence"), -1));
}

extern "C" void MergeShip_LogFormSelected(int form, int requested) {
    static const char* kNames[] = { "adult", "child", "Deku", "Goron", "Zora", "Fierce Deity" };
    if ((form < 0) || (form >= (int)(sizeof(kNames) / sizeof(kNames[0])))) {
        SPDLOG_ERROR("MergeShip: player form {} is out of range", form);
        return;
    }
    if (requested != form) {
        SPDLOG_WARN("MergeShip: player initialising as {} -- form {} was requested but not used", kNames[form],
                    requested);
    } else {
        SPDLOG_INFO("MergeShip: player initialising as {}", kNames[form]);
    }
}
