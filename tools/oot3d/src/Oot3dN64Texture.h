#pragma once
// OoT3D asset converter: decoding SoH's vanilla (N64) texture resources, to compare them with 3DS textures (character
// colors, icon sizes).

#include "Oot3dMath.h"
#include "Oot3dTexture.h"
#include "Oot3dZip.h"

#include <optional>
#include <string>

namespace Oot3d {

struct VanillaTexture {
    uint32_t type = 0; // SoH TextureType: 1 RGBA32, 2 RGBA16, 3 CI4, 4 CI8, 5 I4, 6 I8, 7 IA4, 8 IA8, 9 IA16
    uint32_t width = 0, height = 0;
    Image image; // decoded RGBA8; color-indexed textures decode as gray unless a palette is given
};

// `palette` (a TLUT resource path) colors CI4/CI8 textures.
std::optional<VanillaTexture> ReadVanillaTexture(const ZipReader& oot, const std::string& path,
                                                 const std::string& palette = "");

// Mean color of an image (alpha-weighted), 0..1.
Vec3 MeanColor(const Image& img);

} // namespace Oot3d
