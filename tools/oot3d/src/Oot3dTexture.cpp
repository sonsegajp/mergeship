#include "Oot3dTexture.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Oot3d {

PicaFormat PicaFormatFromGl(uint32_t gl) {
    switch (gl) {
        case 0x14016752: return PicaFormat::RGBA8;
        case 0x14016754: return PicaFormat::RGB8;
        case 0x80346752: return PicaFormat::RGBA5551;
        case 0x83636754: return PicaFormat::RGB565;
        case 0x80336752: return PicaFormat::RGBA4444;
        case 0x14016758: return PicaFormat::LA8;
        case 0x14016759: return PicaFormat::HILO8;
        case 0x14016757: return PicaFormat::L8;
        case 0x14016756: return PicaFormat::A8;
        case 0x67606758: return PicaFormat::LA4;
        case 0x67616757: return PicaFormat::L4;
        case 0x67616756: return PicaFormat::A4;
        case 0x0000675A: return PicaFormat::ETC1;
        case 0x0000675B: return PicaFormat::ETC1A4;
        default: return PicaFormat::Unknown;
    }
}

const char* PicaFormatName(PicaFormat f) {
    static const char* kNames[] = { "RGBA8", "RGB8", "RGBA5551", "RGB565", "RGBA4444", "LA8", "HILO8", "L8",
                                    "A8",    "LA4",  "L4",       "A4",     "ETC1",     "ETC1A4", "?" };
    return kNames[static_cast<int>(f)];
}

size_t PicaByteSize(PicaFormat f, uint32_t w, uint32_t h) {
    const size_t n = size_t(w) * h;
    switch (f) {
        case PicaFormat::RGBA8: return n * 4;
        case PicaFormat::RGB8: return n * 3;
        case PicaFormat::RGBA5551:
        case PicaFormat::RGB565:
        case PicaFormat::RGBA4444:
        case PicaFormat::LA8:
        case PicaFormat::HILO8: return n * 2;
        case PicaFormat::L8:
        case PicaFormat::A8:
        case PicaFormat::LA4:
        case PicaFormat::ETC1A4: return n;
        case PicaFormat::L4:
        case PicaFormat::A4:
        case PicaFormat::ETC1: return n / 2;
        default: return 0;
    }
}

bool PicaFormatHasAlpha(PicaFormat f) {
    switch (f) {
        case PicaFormat::RGBA8:
        case PicaFormat::RGBA5551:
        case PicaFormat::RGBA4444:
        case PicaFormat::LA8:
        case PicaFormat::A8:
        case PicaFormat::LA4:
        case PicaFormat::A4:
        case PicaFormat::ETC1A4: return true;
        default: return false;
    }
}

bool PicaFormatIsGray(PicaFormat f) {
    switch (f) {
        case PicaFormat::LA8:
        case PicaFormat::L8:
        case PicaFormat::A8:
        case PicaFormat::LA4:
        case PicaFormat::L4:
        case PicaFormat::A4: return true;
        default: return false;
    }
}

static inline uint8_t Ex4(uint32_t n) {
    return static_cast<uint8_t>((n << 4) | n);
}
static inline uint8_t Ex5(uint32_t n) {
    return static_cast<uint8_t>((n << 3) | (n >> 2));
}
static inline uint8_t Ex6(uint32_t n) {
    return static_cast<uint8_t>((n << 2) | (n >> 4));
}

// Morton order inside an 8x8 tile: index -> (x, y).
static const std::array<std::pair<uint8_t, uint8_t>, 64>& MortonTable() {
    static const auto table = [] {
        std::array<std::pair<uint8_t, uint8_t>, 64> t{};
        for (uint32_t i = 0; i < 64; i++) {
            auto m = [](uint32_t n) { return ((n >> 2) & 4) | ((n >> 1) & 2) | (n & 1); };
            t[i] = { static_cast<uint8_t>(m(i)), static_cast<uint8_t>(m(i >> 1)) };
        }
        return t;
    }();
    return table;
}

// Decodes tiled pixels; `px(i, dst)` writes pixel number i (in storage order) to dst[0..3].
template <typename F> static void DecodeTiled(Image& img, F&& px) {
    const auto& morton = MortonTable();
    uint32_t i = 0;
    for (uint32_t ty = 0; ty < img.height; ty += 8) {
        for (uint32_t tx = 0; tx < img.width; tx += 8) {
            for (uint32_t k = 0; k < 64; k++, i++) {
                const uint32_t x = tx + morton[k].first, y = ty + morton[k].second;
                if (x < img.width && y < img.height) {
                    px(i, img.Px(x, y));
                }
            }
        }
    }
}

static void DecodeEtc1(Image& img, View src, bool alpha) {
    static const int kIntensity[8][4] = {
        { 2, 8, -2, -8 },     { 5, 17, -5, -17 },   { 9, 29, -9, -29 },     { 13, 42, -13, -42 },
        { 18, 60, -18, -60 }, { 24, 80, -24, -80 }, { 33, 106, -33, -106 }, { 47, 183, -47, -183 },
    };
    size_t off = 0;
    for (uint32_t yy = 0; yy < img.height; yy += 8) {
        for (uint32_t xx = 0; xx < img.width; xx += 8) {
            for (uint32_t by = 0; by < 8; by += 4) {
                for (uint32_t bx = 0; bx < 8; bx += 4) {
                    uint64_t alphaBits = ~0ULL;
                    if (alpha) {
                        alphaBits = src.U64(off);
                        off += 8;
                    }
                    const uint32_t w2 = src.U32(off), w1 = src.U32(off + 4);
                    off += 8;

                    const bool diff = (w1 & 2) != 0, flip = (w1 & 1) != 0;
                    const int* table1 = kIntensity[(w1 >> 5) & 7];
                    const int* table2 = kIntensity[(w1 >> 2) & 7];
                    int base1[3], base2[3];
                    if (diff) {
                        auto sign3 = [](uint32_t n) { return int(n << 29) >> 29; };
                        const int r = (w1 >> 27) & 0x1F, g = (w1 >> 19) & 0x1F, b = (w1 >> 11) & 0x1F;
                        const int dr = sign3((w1 >> 24) & 7), dg = sign3((w1 >> 16) & 7), db = sign3((w1 >> 8) & 7);
                        base1[0] = Ex5(r);
                        base1[1] = Ex5(g);
                        base1[2] = Ex5(b);
                        base2[0] = Ex5(static_cast<uint32_t>((r + dr) & 0x1F));
                        base2[1] = Ex5(static_cast<uint32_t>((g + dg) & 0x1F));
                        base2[2] = Ex5(static_cast<uint32_t>((b + db) & 0x1F));
                    } else {
                        base1[0] = Ex4((w1 >> 28) & 0xF);
                        base2[0] = Ex4((w1 >> 24) & 0xF);
                        base1[1] = Ex4((w1 >> 20) & 0xF);
                        base2[1] = Ex4((w1 >> 16) & 0xF);
                        base1[2] = Ex4((w1 >> 12) & 0xF);
                        base2[2] = Ex4((w1 >> 8) & 0xF);
                    }
                    for (uint32_t i = 0; i < 16; i++) {
                        const uint32_t x = i >> 2, y = i & 3; // column-major within the 4x4 block
                        const uint32_t px = xx + bx + x, py = yy + by + y;
                        if (px >= img.width || py >= img.height) {
                            continue;
                        }
                        const uint32_t lookup = (((w2 >> (16 + i)) & 1) << 1) | ((w2 >> i) & 1);
                        const bool second = flip ? (y & 2) != 0 : (x & 2) != 0;
                        const int* base = second ? base2 : base1;
                        const int mod = (second ? table2 : table1)[lookup];
                        uint8_t* d = img.Px(px, py);
                        for (int c = 0; c < 3; c++) {
                            d[c] = static_cast<uint8_t>(std::clamp(base[c] + mod, 0, 255));
                        }
                        d[3] = Ex4(static_cast<uint32_t>((alphaBits >> (4 * i)) & 0xF));
                    }
                }
            }
        }
    }
}

Image DecodePica(PicaFormat format, uint32_t width, uint32_t height, View s) {
    Image img;
    img.width = width;
    img.height = height;
    img.rgba.assign(size_t(width) * height * 4, 0);
    if (s.Size() < PicaByteSize(format, width, height)) {
        throw FormatError(std::string("texture data too short for ") + PicaFormatName(format));
    }
    auto put = [](uint8_t* d, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        d[0] = r;
        d[1] = g;
        d[2] = b;
        d[3] = a;
    };
    switch (format) {
        case PicaFormat::RGBA8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t* p = s.Data() + i * 4; // stored A B G R
                put(d, p[3], p[2], p[1], p[0]);
            });
            break;
        case PicaFormat::RGB8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t* p = s.Data() + i * 3; // stored B G R
                put(d, p[2], p[1], p[0], 255);
            });
            break;
        case PicaFormat::RGBA5551:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint32_t p = s.U16(i * 2);
                put(d, Ex5((p >> 11) & 0x1F), Ex5((p >> 6) & 0x1F), Ex5((p >> 1) & 0x1F), (p & 1) ? 255 : 0);
            });
            break;
        case PicaFormat::RGB565:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint32_t p = s.U16(i * 2);
                put(d, Ex5((p >> 11) & 0x1F), Ex6((p >> 5) & 0x3F), Ex5(p & 0x1F), 255);
            });
            break;
        case PicaFormat::RGBA4444:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint32_t p = s.U16(i * 2);
                put(d, Ex4((p >> 12) & 0xF), Ex4((p >> 8) & 0xF), Ex4((p >> 4) & 0xF), Ex4(p & 0xF));
            });
            break;
        case PicaFormat::LA8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t a = s.Data()[i * 2], l = s.Data()[i * 2 + 1];
                put(d, l, l, l, a);
            });
            break;
        case PicaFormat::HILO8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t lo = s.Data()[i * 2], hi = s.Data()[i * 2 + 1];
                put(d, hi, lo, 255, 255);
            });
            break;
        case PicaFormat::L8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t l = s.Data()[i];
                put(d, l, l, l, 255);
            });
            break;
        case PicaFormat::A8:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) { put(d, 255, 255, 255, s.Data()[i]); });
            break;
        case PicaFormat::LA4:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t p = s.Data()[i];
                const uint8_t l = Ex4(p >> 4);
                put(d, l, l, l, Ex4(p & 0xF));
            });
            break;
        case PicaFormat::L4:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t p = s.Data()[i >> 1];
                const uint8_t l = Ex4((i & 1) ? (p >> 4) : (p & 0xF));
                put(d, l, l, l, 255);
            });
            break;
        case PicaFormat::A4:
            DecodeTiled(img, [&](uint32_t i, uint8_t* d) {
                const uint8_t p = s.Data()[i >> 1];
                put(d, 255, 255, 255, Ex4((i & 1) ? (p >> 4) : (p & 0xF)));
            });
            break;
        case PicaFormat::ETC1: DecodeEtc1(img, s, false); break;
        case PicaFormat::ETC1A4: DecodeEtc1(img, s, true); break;
        default: throw FormatError("unknown PICA texture format");
    }
    // The PICA's first stored row is the bottom of the picture.
    const size_t row = size_t(width) * 4;
    for (uint32_t y = 0; y < height / 2; y++) {
        std::swap_ranges(img.rgba.begin() + y * row, img.rgba.begin() + (y + 1) * row,
                         img.rgba.begin() + (height - 1 - y) * row);
    }
    return img;
}

bool Image::HasAlpha(uint8_t below) const {
    for (size_t i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] < below) {
            return true;
        }
    }
    return false;
}

bool Image::AlphaIsBinary() const {
    for (size_t i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] != 0 && rgba[i] != 255) {
            return false;
        }
    }
    return true;
}

Image Image::Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) const {
    Image out;
    out.width = w;
    out.height = h;
    out.rgba.assign(size_t(w) * h * 4, 0);
    for (uint32_t j = 0; j < h && y + j < height; j++) {
        for (uint32_t i = 0; i < w && x + i < width; i++) {
            memcpy(out.Px(i, j), Px(x + i, y + j), 4);
        }
    }
    return out;
}

Image Image::Resized(uint32_t w, uint32_t h) const {
    Image out;
    out.width = w;
    out.height = h;
    out.rgba.assign(size_t(w) * h * 4, 0);
    const double sx = double(width) / w, sy = double(height) / h;
    for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
            // Area average over the source footprint (premultiplied, so transparent texels do not bleed color).
            const double x0 = i * sx, x1 = (i + 1) * sx, y0 = j * sy, y1 = (j + 1) * sy;
            double acc[4] = { 0, 0, 0, 0 }, total = 0;
            for (uint32_t yy = uint32_t(y0); yy < std::min<double>(std::ceil(y1), height); yy++) {
                const double wy = std::min<double>(yy + 1, y1) - std::max<double>(yy, y0);
                for (uint32_t xx = uint32_t(x0); xx < std::min<double>(std::ceil(x1), width); xx++) {
                    const double wx = std::min<double>(xx + 1, x1) - std::max<double>(xx, x0);
                    const double wgt = wx * wy;
                    const uint8_t* p = Px(xx, yy);
                    const double a = p[3] / 255.0;
                    acc[0] += p[0] * a * wgt;
                    acc[1] += p[1] * a * wgt;
                    acc[2] += p[2] * a * wgt;
                    acc[3] += p[3] * wgt;
                    total += wgt;
                }
            }
            uint8_t* d = out.Px(i, j);
            const double alpha = total > 0 ? acc[3] / total : 0;
            for (int c = 0; c < 3; c++) {
                d[c] = alpha > 0 ? static_cast<uint8_t>(std::clamp(acc[c] / total / (alpha / 255.0) + 0.5, 0.0, 255.0))
                                 : 0;
            }
            d[3] = static_cast<uint8_t>(std::clamp(alpha + 0.5, 0.0, 255.0));
        }
    }
    return out;
}

std::vector<TextureDesc> ParseTexChunk(View c) {
    if (!c.Magic(0, "tex ")) {
        throw FormatError("missing tex chunk");
    }
    const uint32_t count = c.U32(0x08);
    std::vector<TextureDesc> out;
    out.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const size_t o = 0x0C + size_t(i) * 0x24;
        TextureDesc t;
        t.dataSize = c.U32(o + 0x00);
        t.mipLevels = c.U16(o + 0x04);
        t.cubemap = c.U8(o + 0x07) != 0;
        t.width = c.U16(o + 0x08);
        t.height = c.U16(o + 0x0A);
        t.glFormat = c.U32(o + 0x0C);
        t.dataOffset = c.U32(o + 0x10);
        t.name = c.Str(o + 0x14, 0x10);
        t.format = PicaFormatFromGl(t.glFormat);
        out.push_back(t);
    }
    return out;
}

Ctxb ParseCtxb(View f) {
    if (!f.Magic(0, "ctxb")) {
        throw FormatError("not a ctxb file");
    }
    Ctxb out;
    out.textures = ParseTexChunk(f.From(f.U32(0x10)));
    out.texData = f.From(f.U32(0x14));
    return out;
}

Image Ctxb::Decode(size_t index) const {
    const TextureDesc& t = textures.at(index);
    return DecodePica(t.format, t.width, t.height, texData.Sub(t.dataOffset, PicaByteSize(t.format, t.width, t.height)));
}

// -- PNG (stored deflate blocks; only for debug dumps) -------------------------------------------------------------

static uint32_t Crc32(const uint8_t* p, size_t n, uint32_t crc = 0) {
    static const auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (size_t i = 0; i < n; i++) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

Bytes EncodePng(const Image& img) {
    Bytes raw;
    raw.reserve((size_t(img.width) * 4 + 1) * img.height);
    for (uint32_t y = 0; y < img.height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), img.rgba.begin() + size_t(y) * img.width * 4,
                   img.rgba.begin() + size_t(y + 1) * img.width * 4);
    }
    Bytes z{ 0x78, 0x01 };
    for (size_t off = 0; off < raw.size() || off == 0;) {
        const size_t n = std::min<size_t>(65535, raw.size() - off);
        const bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<uint8_t>(n));
        z.push_back(static_cast<uint8_t>(n >> 8));
        z.push_back(static_cast<uint8_t>(~n));
        z.push_back(static_cast<uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
        if (last) {
            break;
        }
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    const uint32_t adler = (b << 16) | a;
    for (int s = 24; s >= 0; s -= 8) {
        z.push_back(static_cast<uint8_t>(adler >> s));
    }

    Bytes png{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    auto chunk = [&](const char* type, const Bytes& data) {
        const uint32_t n = static_cast<uint32_t>(data.size());
        for (int s = 24; s >= 0; s -= 8) {
            png.push_back(static_cast<uint8_t>(n >> s));
        }
        const size_t start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        const uint32_t crc = Crc32(png.data() + start, png.size() - start);
        for (int s = 24; s >= 0; s -= 8) {
            png.push_back(static_cast<uint8_t>(crc >> s));
        }
    };
    Bytes ihdr;
    for (uint32_t v : { img.width, img.height }) {
        for (int s = 24; s >= 0; s -= 8) {
            ihdr.push_back(static_cast<uint8_t>(v >> s));
        }
    }
    ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 });
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    return png;
}

} // namespace Oot3d
