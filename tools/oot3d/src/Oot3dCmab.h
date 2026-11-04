#pragma once
// OoT3D asset converter: CMAB material animations (noclip.website OcarinaOfTime3D/cmab.ts): texture palettes (eye and
// mouth textures swapped by a track), constant / material color tracks and texture coordinate translation, rotation
// and scale tracks, each bound to a material of the model the animation belongs to.

#include "Oot3dBinary.h"
#include "Oot3dTexture.h"

#include <string>
#include <vector>

namespace Oot3d {

struct CmabKey {
    float time = 0, value = 0, tangentIn = 0, tangentOut = 0;
};

struct CmabTrack {
    enum class Type : uint32_t { Linear = 1, Hermite = 2, Integer = 3 } type = Type::Linear;
    uint32_t timeStart = 0, timeEnd = 0;
    std::vector<CmabKey> keys;
    float Sample(float frame) const;
};

struct CmabEntry {
    enum Type : uint32_t {
        Translation = 1,
        TexturePalette = 2,
        DiffuseColor = 3,
        ConstColor = 4,
        Rotation = 5,
        Scale = 6,
        AmbientColor = 7,
        Spec0Color = 8,
        Spec1Color = 9,
        EmissionColor = 10,
    };
    uint32_t type = 0;
    uint32_t material = 0;
    uint32_t channel = 0;               // texture slot (palette, SRT) or constant index (ConstColor)
    std::vector<CmabTrack> tracks;      // per component; an empty track (no keys) is absent
    std::vector<bool> present;
};

struct CmabTexture {
    std::string name;
    Image image;
};

struct Cmab {
    uint32_t duration = 0;
    bool loop = true;
    std::vector<CmabEntry> entries;
    std::vector<CmabTexture> textures; // the palette textures, decoded
    static Cmab Parse(View file);      // throws FormatError
};

} // namespace Oot3d
