#pragma once
// OoT3D asset converter: bodies the N64 draws as rings under a matrix each (Morpha's tentacle, the Like Like).

#include "Oot3dExtract.h"
#include "Oot3dMaps.h"
#include "Oot3dMesh.h"
#include "Oot3dRom.h"
#include "Oot3dZar.h"
#include "Oot3dZip.h"

#include <map>
#include <string>
#include <vector>

namespace Oot3d {

struct RingsResult {
    int models = 0;
    std::vector<ScrollEntry> scrolls; // their materials' texture scrolls, for the engine's table
};

// A model's CMAB texture scroll (`cmabName` in `z`) for `mesh`'s materials; `rates` must outlive the mesh's Emit.
void ScrollFromCmab(const Zar& z, const std::string& cmabName, MeshBuilder& mesh, std::map<int, ScrollRate>& rates);

RingsResult ConvertRingModels(const RomFS& rom, const ZipReader& oot, ZipWriter& zip, TextureStore& textures,
                              const ExtractOptions& opt);

} // namespace Oot3d
