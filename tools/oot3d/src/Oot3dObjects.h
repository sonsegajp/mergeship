#pragma once
// OoT3D asset converter: static object models (see Oot3dObjects.cpp).

#include "Oot3dExtract.h"
#include "Oot3dMesh.h"
#include "Oot3dZip.h"

#include <vector>

namespace Oot3d {

class RomFS;
class TextureStore;
class RomIndex;

struct ObjectsResult {
    uint32_t converted = 0, emptied = 0, kept = 0, warnings = 0;
    std::vector<ScrollEntry> scrolls; // converted lists' CMAB texture scrolls, for the engine's table
};

ObjectsResult ConvertObjects(const RomFS& rom, const RomIndex& index, const ZipReader& oot, ZipWriter& zip,
                             TextureStore& textures, const ExtractOptions& opt);

} // namespace Oot3d
