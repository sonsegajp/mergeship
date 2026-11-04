#pragma once
// OoT3D asset converter: SoH's (vanilla N64) skeleton, limb and animation resources.

#include "Oot3dBinary.h"
#include "Oot3dMath.h"
#include "Oot3dZip.h"

#include <optional>
#include <string>
#include <vector>

namespace Oot3d {

constexpr uint32_t kTypeSkeleton = 0x4F534B4C;     // OSKL
constexpr uint32_t kTypeSkeletonLimb = 0x4F534C42; // OSLB
constexpr uint32_t kTypeAnimation = 0x4F414E4D;    // OANM

struct VanillaLimb {
    std::string path;
    uint8_t limbType = 1; // 1 standard, 2 LOD, 3 skin, 4 curve, 5 legacy
    int16_t trans[3] = {};
    uint8_t childIndex = 0xFF, siblingIndex = 0xFF;
    std::string dList, dList2;
    int parent = -1; // derived
};

struct VanillaSkeleton {
    std::string path;
    uint8_t type = 0; // 0 normal, 1 flex, 2 curve
    uint8_t limbType = 1;
    uint32_t dListCount = 0;
    std::vector<VanillaLimb> limbs;
};

std::optional<VanillaSkeleton> ReadVanillaSkeleton(const ZipReader& oot, const std::string& path);

// Parent indices of a CMB/N64 hierarchy, for topology comparisons.
std::vector<int> ParentsOf(const VanillaSkeleton& s);

} // namespace Oot3d
