#pragma once
// OoT3D asset converter: ZSI scene and room files ("ZSI\1").
// The N64 header command stream, little-endian, with room meshes stored as embedded CMB models. Word 0 of each
// command is big-endian like the N64's (type in the top byte), word 1 is a little-endian offset from the header start.

#include "Oot3dBinary.h"
#include "Oot3dCmb.h"

#include <optional>
#include <string>
#include <vector>

namespace Oot3d {

struct ZsiActor {
    uint16_t id = 0;
    int16_t pos[3] = {};
    int16_t rot[3] = {};
    uint16_t params = 0;
};

struct ZsiRoomSetup {
    std::optional<Cmb> opaque, translucent;
    std::vector<ZsiActor> actors;
};

struct ZsiRoom {
    std::string codename;
    Bytes bytes; // keeps the CMB views alive
    std::vector<ZsiRoomSetup> setups; // [0] is the main header, then alternate headers (may be empty)

    static ZsiRoom Parse(Bytes bytes);
};

// One environment (lighting) setting; OoT's time-of-day scenes have 4 (dawn, day, dusk, night) or more.
struct ZsiEnvironment {
    std::array<uint8_t, 3> ambient{}, light0Color{}, light1Color{}, fogColor{};
    std::array<int8_t, 3> light0Dir{}, light1Dir{};
    uint16_t fogStart = 996;
    float fogEnd = 12800, drawDistance = 20000;
};

struct ZsiScene {
    std::string codename;
    std::vector<std::string> rooms; // room file names ("ydan_0_info.zsi" style names from the room list)
    std::vector<ZsiEnvironment> environments;
    uint8_t skyboxId = 0;
    uint8_t lightMode = 1; // 0: time of day picks the environment (1 = day), 1: fixed settings (0 unless changed)

    // The environment a room normally shows (daytime for time-of-day scenes).
    const ZsiEnvironment* DefaultEnvironment() const {
        if (environments.empty()) {
            return nullptr;
        }
        return &environments[lightMode == 0 && environments.size() >= 4 ? 1 : 0];
    }

    static ZsiScene Parse(const Bytes& bytes);
};

} // namespace Oot3d
