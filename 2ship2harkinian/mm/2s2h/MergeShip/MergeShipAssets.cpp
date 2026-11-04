#include "MergeShipAssets.h"
#include <mergeship/AssetArchive.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include "2s2h/resource/type/Skeleton.h"
#include "2s2h/resource/type/Path.h"
#include <filesystem>
#include <spdlog/spdlog.h>

static bool sOcarinaMounted = false;

extern "C" int MergeShip_IsOcarinaMounted(void) { return sOcarinaMounted; }

// Ocarina of Time's assets under "oot/". The first of these found is mounted:
//  - oot.o2r / oot-mq.o2r: Ship of Harkinian's own archive, extracted once from the player's OoT ROM (in mergeship.exe
//    by SoH's first-run flow, next to this game's files). Mounted re-namespaced (AssetArchiveMode::Rebase): every entry
//    and every reference inside it is read under oot/, byte for byte what a torch --namespace export holds
//    (tools/verify_asset_namespace.py --rebase). No second archive, and nothing to regenerate when SoH re-extracts.
//  - oot-assets.o2r: an already namespaced export (tools/build_cross_assets.ps1), for a standalone 2ship.exe package.
void MergeShip_MountOcarinaAssets(const std::shared_ptr<Ship::ArchiveManager>& manager) {
    sOcarinaMounted = false;
    for (const char* name : { "oot.o2r", "oot-mq.o2r", "oot-assets.o2r" }) {
        const auto path = Ship::Context::LocateFileAcrossAppDirs(name, "2ship");
        if (!std::filesystem::exists(path)) {
            continue;
        }
        auto archive = std::make_shared<MergeShip::AssetArchive>(path, "oot", MergeShip::AssetArchiveMode::Rebase);
        archive->Load();
        if (!archive->IsLoaded() || !archive->HasFile(std::string("oot/objects/object_link_boy/gLinkAdultSkel")) ||
            !archive->HasFile(std::string("oot/objects/object_link_child/gLinkChildSkel"))) {
            SPDLOG_ERROR("MergeShip: {} is not a complete OoT archive; not mounted", path);
            continue;
        }
        sOcarinaMounted = manager->AddArchive(archive) != nullptr;
        if (sOcarinaMounted) {
            SPDLOG_INFO("MergeShip: mounted {} under oot/{}", path,
                        archive->IsRebased() ? " (namespaced as it is read)" : "");
            return;
        }
        SPDLOG_ERROR("MergeShip: failed to mount {}", path);
    }
    SPDLOG_INFO("MergeShip: no oot.o2r, oot-mq.o2r or oot-assets.o2r found; Ocarina assets unavailable");
}

void MergeShip_VerifyOcarinaAssets() {
    if (!sOcarinaMounted) return;
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    size_t loaded = 0;
    for (const char* path : { "oot/objects/object_link_boy/gLinkAdultSkel",
                             "oot/objects/object_link_child/gLinkChildSkel" }) {
        auto skeleton = std::dynamic_pointer_cast<SOH::Skeleton>(resources->LoadResource(path));
        bool valid = skeleton && skeleton->limbCount > 0 &&
                     skeleton->skeletonHeaderSegments.size() == skeleton->limbCount;
        if (valid) {
            for (void* limb : skeleton->skeletonHeaderSegments) valid &= limb != nullptr;
        }
        if (valid) ++loaded;
        else SPDLOG_ERROR("MergeShip: OoT skeleton or its limbs failed to load: {}", path);
    }
    SPDLOG_INFO("MergeShip: {} of 2 OoT player skeletons and their limbs loaded in MM", loaded);
    auto path = std::dynamic_pointer_cast<SOH::PathMM>(resources->LoadResource(
        "oot/scenes/nonmq/HAKAdanCH_scene/HAKAdanCH_scenePathwayList_000200"));
    if (path && path->paths.size() == 1 && path->paths[0].size() == 5 &&
        path->paths[0][0].x == 640 && path->paths[0][0].y == 50 && path->paths[0][0].z == -20 &&
        path->pathData[0].additionalPathIndex == 0xFF && path->pathData[0].customValue == 0) {
        SPDLOG_INFO("MergeShip: OoT path points and MM defaults verified");
    } else {
        SPDLOG_ERROR("MergeShip: OoT path conversion failed");
    }
    const char* probes[] = {
        "oot/objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL",
        "oot/textures/icon_item_static/gABtnSymbolTex",
        "oot/objects/gameplay_keep/gPlayerAnim_link_normal_wait",
        "oot/audio/fonts/00_Sound_Effects_1",
        "oot/audio/sequences/000_Sound_Effects",
    };
    size_t parsed = 0;
    for (const char* path : probes) {
        auto resource = resources->LoadResource(path);
        if (resource && resource->GetRawPointer()) ++parsed;
        else SPDLOG_ERROR("MergeShip: foreign OoT resource failed to parse: {}", path);
    }
    SPDLOG_INFO("MergeShip: {} of {} OoT graphics/texture/animation/font/sequence probes parsed in MM",
                parsed, sizeof(probes) / sizeof(probes[0]));
}
