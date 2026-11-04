// MergeShip2 -- a copy of a display list that ends where one of its textures starts being loaded, for both engines
// (SoH and 2Ship share libultraship's display list resource, Fast::DisplayList).
//
// Used where a game draws an item and a hand in one list and only the item is wanted on another hand: MM's first-person
// Hookshot (object_link_child_DL_017B40, the Hookshot's body, crown and handle, then young Link's hand from its texture
// object_link_child_Tex_019240 on) drawn in adult Link's own fist.
//
// Include after the engine's z64.h (the F3DEX2 / OTR opcode macros). C++ only.
#pragma once

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <fast/resource/type/DisplayList.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

// A copy of the list at `path` (a resource path, "__OTR__" optional) that ends where the first texture whose name
// contains `texture` is loaded: at the pipe sync that starts the load of the palette it is drawn with (the palette load
// just before it), else at the pipe sync before the texture itself. Made once, kept for the process. NULL when the list
// cannot be loaded or has no such texture (the caller draws its own fallback).
inline void* MsDl_CutBeforeTexture(const char* path, const char* texture, size_t* cutIndex = nullptr) {
    using Instructions = decltype(Fast::DisplayList::Instructions);
    static std::unordered_map<std::string, Instructions> sCuts;

    const std::string key = std::string(path) + "#" + texture;
    auto found = sCuts.find(key);
    if (found != sCuts.end()) {
        return found->second.data();
    }
    const char* resPath = (strncmp(path, "__OTR__", 7) == 0) ? path + 7 : path;
    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(resourceManager->LoadResource(resPath));
    if ((dl == nullptr) || dl->Instructions.empty()) {
        return nullptr;
    }
    Instructions out(dl->Instructions.begin(), dl->Instructions.end());
    auto archiveManager = resourceManager->GetArchiveManager();
    size_t lastSync = SIZE_MAX;
    size_t paletteSync = SIZE_MAX;
    size_t cut = SIZE_MAX;

    for (size_t i = 0; (i < out.size()) && (cut == SIZE_MAX); i++) {
        const uint8_t opcode = (uint8_t)(out[i].words.w0 >> 24);

        if (opcode == G_RDPPIPESYNC) {
            lastSync = i;
        } else if ((opcode == G_SETTIMG_OTR_HASH) && (i + 1 < out.size())) {
            const uint64_t hash = ((uint64_t)out[i + 1].words.w0 << 32) | (uint64_t)(uint32_t)out[i + 1].words.w1;
            const std::string* name = archiveManager->HashToString(hash);
            if ((name != nullptr) && (name->find("TLUT") != std::string::npos)) {
                paletteSync = lastSync;
            } else if ((name != nullptr) && (name->find(texture) != std::string::npos)) {
                cut = (paletteSync != SIZE_MAX) ? paletteSync : lastSync;
            }
        }
        // LUS ResourceFactoryBinaryDisplayListV0: these take two instructions (the second is a hash).
        if ((opcode == G_SETTIMG_OTR_HASH) || (opcode == G_DL_OTR_HASH) || (opcode == G_VTX_OTR_HASH) ||
            (opcode == G_BRANCH_Z_OTR) || (opcode == G_MARKER) || (opcode == G_MTX_OTR) || (opcode == G_MOVEMEM_OTR)) {
            i++;
        }
    }
    if ((cut == SIZE_MAX) || (cut == 0)) {
        return nullptr;
    }
    out.resize(cut + 1);
    out[cut].words.w0 = (uintptr_t)G_ENDDL << 24;
    out[cut].words.w1 = 0;
    if (cutIndex != nullptr) {
        *cutIndex = cut;
    }
    auto& stored = sCuts[key];
    stored = std::move(out);
    return stored.data();
}
