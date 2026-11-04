#pragma once
// OoT3D asset converter: SoH's (vanilla N64) animations, and sampling of both N64 and 3DS (CSAB) poses.

#include "Oot3dCmb.h"
#include "Oot3dCsab.h"
#include "Oot3dSkeleton.h"
#include "Oot3dZip.h"

#include <optional>
#include <string>
#include <vector>

namespace Oot3d {

struct VanillaAnimation {
    std::string path;
    uint32_t type = 0; // 0 normal, 1 link (player), 2 curve, 3 legacy
    int16_t frameCount = 0;
    std::vector<uint16_t> values;
    std::vector<std::array<uint16_t, 3>> joints; // [0] root translation, then one rotation per limb
    int16_t staticIndexMax = 0;
    std::string playerDataPath; // type 1

    // Joint j's x/y/z raw value at an integer frame.
    int16_t Value(size_t joint, int axis, int frame) const;
};

std::optional<VanillaAnimation> ReadVanillaAnimation(const ZipReader& oot, const std::string& path);

// Model-space joint matrices of an N64 skeleton at a frame (limb order), with the root at the origin.
std::vector<Mat34> N64Pose(const VanillaSkeleton& skel, const VanillaAnimation& anim, int frame);
// Model-space bone matrices of a CMB skeleton under a CSAB frame (bones without a node keep their bind values).
std::vector<Mat34> CsabPose(const Cmb& cmb, const Csab& anim, float frame);

} // namespace Oot3d
