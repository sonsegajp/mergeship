#include "Oot3dN64Texture.h"

#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"

namespace Oot3d {

namespace {

uint8_t Expand5(uint32_t v) {
    return static_cast<uint8_t>((v << 3) | (v >> 2));
}

// The palette's colors (RGBA16, big-endian as on the N64).
std::vector<std::array<uint8_t, 4>> ReadPalette(const ZipReader& oot, const std::string& path) {
    std::vector<std::array<uint8_t, 4>> out;
    if (path.empty()) {
        return out;
    }
    auto t = ReadVanillaTexture(oot, path);
    if (!t) {
        return out;
    }
    for (uint32_t i = 0; i < t->width * t->height; i++) {
        const uint8_t* p = t->image.Px(i % t->width, i / t->width);
        out.push_back({ p[0], p[1], p[2], p[3] });
    }
    return out;
}

} // namespace

std::optional<VanillaTexture> ReadVanillaTexture(const ZipReader& oot, const std::string& path,
                                                 const std::string& palette) {
    auto b = oot.Read(path);
    if (!b) {
        return std::nullopt;
    }
    auto h = ParseResourceHeader(*b);
    if (!h || h->type != Soh::kTypeTexture || b->size() < 64 + 16) {
        return std::nullopt;
    }
    const View v(*b);
    VanillaTexture t;
    size_t off = 64;
    t.type = v.U32(off);
    t.width = v.U32(off + 4);
    t.height = v.U32(off + 8);
    if (h->version >= 1) {
        // An HD replacement (a mod's): RGBA8 uploaded as is when TEX_FLAG_LOAD_AS_RAW.
        if (b->size() < 64 + 28 || !(v.U32(off + 12) & 1u)) {
            return std::nullopt;
        }
        const uint32_t n = v.U32(off + 24);
        if (n != t.width * t.height * 4 || b->size() < 64 + 28 + size_t(n)) {
            return std::nullopt;
        }
        t.image.width = t.width;
        t.image.height = t.height;
        t.image.rgba.assign(b->begin() + 64 + 28, b->begin() + 64 + 28 + n);
        return t;
    }
    const uint32_t size = v.U32(off + 12);
    off += 16;
    if (t.width == 0 || t.height == 0 || t.width > 4096 || t.height > 4096 || off + size > b->size()) {
        return std::nullopt;
    }
    const uint8_t* d = b->data() + off;
    const auto pal = (t.type == 3 || t.type == 4) ? ReadPalette(oot, palette) : std::vector<std::array<uint8_t, 4>>{};
    t.image.width = t.width;
    t.image.height = t.height;
    t.image.rgba.assign(size_t(t.width) * t.height * 4, 0);
    const size_t n = size_t(t.width) * t.height;
    auto put = [&](size_t i, uint8_t r, uint8_t g, uint8_t bl, uint8_t a) {
        uint8_t* p = &t.image.rgba[i * 4];
        p[0] = r, p[1] = g, p[2] = bl, p[3] = a;
    };
    auto nibble = [&](size_t i) -> uint8_t { return (i & 1) ? (d[i / 2] & 0xF) : (d[i / 2] >> 4); };
    for (size_t i = 0; i < n; i++) {
        switch (t.type) {
            case 1: // RGBA32
                if (i * 4 + 3 < size) {
                    put(i, d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3]);
                }
                break;
            case 2: // RGBA16
                if (i * 2 + 1 < size) {
                    const uint32_t c = (uint32_t(d[i * 2]) << 8) | d[i * 2 + 1];
                    put(i, Expand5(c >> 11), Expand5((c >> 6) & 31), Expand5((c >> 1) & 31), (c & 1) ? 255 : 0);
                }
                break;
            case 3: // CI4
            case 4: // CI8
            {
                const size_t idx = t.type == 3 ? (i / 2 < size ? nibble(i) : 0) : (i < size ? d[i] : 0);
                if (idx < pal.size()) {
                    put(i, pal[idx][0], pal[idx][1], pal[idx][2], pal[idx][3]);
                } else {
                    const uint8_t g = static_cast<uint8_t>(t.type == 3 ? idx * 17 : idx);
                    put(i, g, g, g, 255);
                }
                break;
            }
            case 5: // I4
                if (i / 2 < size) {
                    const uint8_t g = nibble(i) * 17;
                    put(i, g, g, g, g);
                }
                break;
            case 6: // I8
                if (i < size) {
                    put(i, d[i], d[i], d[i], d[i]);
                }
                break;
            case 7: // IA4
                if (i / 2 < size) {
                    const uint8_t x = nibble(i), g = static_cast<uint8_t>((x >> 1) * 255 / 7);
                    put(i, g, g, g, (x & 1) ? 255 : 0);
                }
                break;
            case 8: // IA8
                if (i < size) {
                    const uint8_t g = (d[i] >> 4) * 17;
                    put(i, g, g, g, (d[i] & 0xF) * 17);
                }
                break;
            case 9: // IA16
                if (i * 2 + 1 < size) {
                    put(i, d[i * 2], d[i * 2], d[i * 2], d[i * 2 + 1]);
                }
                break;
            default:
                return std::nullopt;
        }
    }
    return t;
}

Vec3 MeanColor(const Image& img) {
    double r = 0, g = 0, b = 0, w = 0;
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        const double a = img.rgba[i + 3] / 255.0;
        r += img.rgba[i] * a;
        g += img.rgba[i + 1] * a;
        b += img.rgba[i + 2] * a;
        w += a;
    }
    if (w <= 0) {
        return Vec3(0, 0, 0);
    }
    return Vec3(float(r / w / 255.0), float(g / w / 255.0), float(b / w / 255.0));
}

} // namespace Oot3d
