#pragma once
// OoT3D asset converter: reading SoH's own (vanilla N64) resources out of oot.o2r, to learn what the mod must
// override: room mesh display lists, their culling spheres, and what the vanilla display lists draw.

#include "Oot3dBinary.h"
#include "Oot3dMath.h"
#include "Oot3dZip.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Oot3d {

struct ResourceHeaderInfo {
    uint32_t type = 0, version = 0;
};
std::optional<ResourceHeaderInfo> ParseResourceHeader(const Bytes& b);

struct VanillaMeshEntry {
    std::string opa, xlu; // display list paths (may be empty)
    bool hasSphere = false;
    int16_t x = 0, y = 0, z = 0, radius = 0; // mesh type 2: culling sphere
};

struct VanillaRoom {
    int meshType = -1; // 0 normal, 1 prerendered, 2 culled
    std::vector<VanillaMeshEntry> entries;
    std::vector<std::string> alternateHeaders;
};

struct VanillaScene {
    std::vector<std::string> rooms; // room resource paths
    std::vector<std::string> alternateHeaders;
    struct Light {
        std::array<uint8_t, 3> ambient{}, dir1{}, color1{}, dir2{}, color2{}, fog{};
        int16_t fogNear = 0;
        uint16_t fogFar = 0;
    };
    std::vector<Light> lights;
};

// Throws FormatError for anything that is not a Scene/Room resource.
VanillaRoom ParseVanillaRoom(const Bytes& resource);
VanillaScene ParseVanillaScene(const Bytes& resource);
// The room with its mesh command replaced (every other command is copied byte for byte).
Bytes RewriteRoomMesh(const Bytes& room, int meshType, const std::vector<VanillaMeshEntry>& entries);

// What a vanilla display list (and the lists it calls) draws.
struct VanillaDlInfo {
    bool valid = false;
    uint32_t vertexCount = 0;
    Vec3 min{ 1e9f, 1e9f, 1e9f }, max{ -1e9f, -1e9f, -1e9f };
    std::vector<uint32_t> segmentCalls; // gSPDisplayList(0x0X000000) segments it calls (animated materials)
    std::vector<std::string> textures;
    std::vector<std::string> calledLists; // display lists it calls by path
    std::vector<Vec3> positions;          // when asked for
    bool lighting = false;                // sets G_LIGHTING somewhere
    bool segmentTextures = false;         // loads a texture through a segment (set by the actor)
    // Its combiner takes ENV / PRIM color without the list setting it: a tint the actor sets (tree leaves).
    bool usesEnv = false, usesPrim = false, setsEnv = false, setsPrim = false;
    bool usesPrimAlpha = false, usesEnvAlpha = false; // the alpha combiner reads them (an actor-set fade)
};
VanillaDlInfo InspectVanillaDl(const ZipReader& oot, const std::string& path, bool wantPositions = false);

} // namespace Oot3d
