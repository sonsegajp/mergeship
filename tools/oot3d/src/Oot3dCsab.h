#pragma once
// OoT3D asset converter: CSAB skeletal animations (OoT3D subversion 3; layout per noclip.website csab.ts).

#include "Oot3dBinary.h"

#include <array>
#include <optional>
#include <vector>

namespace Oot3d {

struct CsabKey {
    float time = 0, value = 0, tangentIn = 0, tangentOut = 0;
};

struct CsabTrack {
    enum class Type : uint32_t { Constant = 0, Linear = 1, Hermite = 2 } type = Type::Linear;
    uint32_t timeStart = 0, timeEnd = 0;
    std::vector<CsabKey> keys;
    float Sample(float frame, bool isAngle) const;
};

struct CsabNode {
    uint16_t bone = 0;
    // translation x/y/z, rotation x/y/z (radians), scale x/y/z
    std::array<std::optional<CsabTrack>, 9> tracks;
};

struct Csab {
    uint32_t duration = 0; // frames (last frame + 1)
    std::vector<int16_t> boneToNode;
    std::vector<CsabNode> nodes;

    static Csab Parse(View data);
    const CsabNode* NodeForBone(size_t bone) const;
};

} // namespace Oot3d
