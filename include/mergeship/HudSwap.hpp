#pragma once
/*
 * MergeShip2 -- the other game's HUD textures (BUG-0013, the testers' suggestion, 2026-10-07): an option per group
 * (C/B button backgrounds, hearts, the rupee counter's icon, Link's head on the dungeon map) puts the other game's
 * texture at this game's own path. The files are read from the other game's archive file (mm.o2r in Hyrule, oot.o2r in
 * Termina: each engine reads it as the transformation scream reads mergeship2.o2r) and mounted as one in-memory archive
 * over the game's own; changing an option remounts it and unloads the textures so they are read again. Both games'
 * textures are the same size and format (checked: the files are byte-identical in their headers).
 */
#include <mergeship/AdultMask.hpp> // MsAdultMask::MemoryArchive
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/archive/O2rArchive.h>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" void gfx_texture_cache_clear();

namespace MsHudSwap {

struct Swap {
    const char* here;  // this game's own path (without __OTR__), e.g. "textures/parameter_static/gHeartFullTex"
    const char* there; // the same texture in the other game's archive file
};

struct Group {
    const Swap* swaps;
    size_t count;
};

// Mounts the swaps of every group whose bit is set in `mask` (and unmounts the previous mount). Returns the new mount
// (null when nothing is swapped). `otherArchive`: the other game's archive file name ("mm.o2r" / "oot.o2r");
// `appName`: LocateFileAcrossAppDirs's application name.
inline std::shared_ptr<MsAdultMask::MemoryArchive> Apply(const Group* groups, size_t groupCount, uint32_t mask,
                                                         const char* otherArchive, const char* appName,
                                                         std::shared_ptr<MsAdultMask::MemoryArchive> previous,
                                                         const char* mountName) {
    auto resources = Ship::Context::GetRawInstance()->GetResourceManager();
    auto archives = resources->GetArchiveManager();
    if (previous != nullptr) {
        archives->RemoveArchive(previous);
    }

    std::map<std::string, std::vector<char>> files;
    if (mask != 0) {
        const std::string path = Ship::Context::LocateFileAcrossAppDirs(otherArchive, appName);
        if (std::filesystem::exists(path)) {
            auto other = std::make_shared<Ship::O2rArchive>(path);
            other->Load();
            for (size_t g = 0; other->IsLoaded() && (g < groupCount); g++) {
                if (!(mask & (1u << g))) {
                    continue;
                }
                for (size_t i = 0; i < groups[g].count; i++) {
                    std::shared_ptr<Ship::File> file = other->LoadFile(std::string(groups[g].swaps[i].there));
                    if ((file != nullptr) && (file->Buffer != nullptr)) {
                        files[groups[g].swaps[i].here] = std::vector<char>(file->Buffer->begin(), file->Buffer->end());
                    }
                }
            }
        }
    }

    std::shared_ptr<MsAdultMask::MemoryArchive> mounted = nullptr;
    if (!files.empty()) {
        uint32_t gameVersion = 0;
        for (const auto& a : *archives->GetArchives()) {
            if (a->HasGameVersion()) {
                gameVersion = a->GetGameVersion();
                break;
            }
        }
        mounted = std::make_shared<MsAdultMask::MemoryArchive>(std::move(files), gameVersion, mountName);
        mounted->Load();
        if (!mounted->IsLoaded() || (archives->AddArchive(mounted) == nullptr)) {
            mounted = nullptr;
        }
    }
    // Every texture of every group is read again (the swapped ones from the mount, the others from the game's own).
    for (size_t g = 0; g < groupCount; g++) {
        for (size_t i = 0; i < groups[g].count; i++) {
            resources->UnloadResource(std::string(groups[g].swaps[i].here));
        }
    }
    gfx_texture_cache_clear();
    return mounted;
}

} // namespace MsHudSwap
