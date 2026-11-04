#pragma once
// OoT3D asset converter: PICA200 texture formats and the CMB/CTXB "tex " chunk.

#include "Oot3dBinary.h"

#include <string>
#include <vector>

namespace Oot3d {

enum class PicaFormat : uint8_t {
    RGBA8,
    RGB8,
    RGBA5551,
    RGB565,
    RGBA4444,
    LA8,
    HILO8,
    L8,
    A8,
    LA4,
    L4,
    A4,
    ETC1,
    ETC1A4,
    Unknown,
};

PicaFormat PicaFormatFromGl(uint32_t glFormat);
const char* PicaFormatName(PicaFormat f);
size_t PicaByteSize(PicaFormat f, uint32_t width, uint32_t height);
bool PicaFormatHasAlpha(PicaFormat f);
bool PicaFormatIsGray(PicaFormat f); // L/LA/A formats: intensity textures

// An RGBA8 image. Row 0 is the top of the picture, as SoH (and any image viewer) expects; the PICA stores the bottom
// row first, which Decode undoes.
struct Image {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba;

    bool Valid() const {
        return width && height && rgba.size() == size_t(width) * height * 4;
    }
    uint8_t* Px(uint32_t x, uint32_t y) {
        return &rgba[(size_t(y) * width + x) * 4];
    }
    const uint8_t* Px(uint32_t x, uint32_t y) const {
        return &rgba[(size_t(y) * width + x) * 4];
    }
    bool HasAlpha(uint8_t below = 255) const; // any pixel with alpha < below
    bool AlphaIsBinary() const;               // alpha only 0 or 255
    Image Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) const;
    Image Resized(uint32_t w, uint32_t h) const; // box/bilinear filter
};

Image DecodePica(PicaFormat format, uint32_t width, uint32_t height, View data);

// One texture of a "tex " chunk (CMB or CTXB).
struct TextureDesc {
    std::string name;
    uint32_t width = 0, height = 0;
    uint32_t glFormat = 0;
    PicaFormat format = PicaFormat::Unknown;
    uint16_t mipLevels = 1;
    bool cubemap = false;
    uint32_t dataOffset = 0; // relative to the texture data block
    uint32_t dataSize = 0;
};

std::vector<TextureDesc> ParseTexChunk(View chunk); // at the "tex " magic

// A .ctxb file: a "tex " chunk plus its data.
struct Ctxb {
    std::vector<TextureDesc> textures;
    View texData;
    Image Decode(size_t index) const;
};
Ctxb ParseCtxb(View file);

// Encodes a PNG (RGBA8, no compression tricks) for debug dumps.
Bytes EncodePng(const Image& img);

} // namespace Oot3d
