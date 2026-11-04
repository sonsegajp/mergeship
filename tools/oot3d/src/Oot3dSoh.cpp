#include "Oot3dSoh.h"

#include <algorithm>

namespace Oot3d::Soh {

Bytes Header(uint32_t type, uint32_t version) {
    Writer w;
    w.U8(0); // little-endian
    w.U8(0); // not custom
    w.U8(0);
    w.U8(0);
    w.U32(type);
    w.U32(version);
    w.U64(0xDEADBEEFDEADBEEFULL);
    w.Pad(64);
    return w.Out();
}

HdTexture MakeHdTexture(Image image) {
    HdTexture t;
    uint32_t w = std::max<uint32_t>(image.width, 4), h = std::max<uint32_t>(image.height, 1);
    // RGBA16 logical texels; 1024 of them (2 KB) so two textures always share TMEM.
    while (w * h > 1024 && (w > 4 || h > 1)) {
        if (w >= h && w > 4) {
            w /= 2;
        } else {
            h /= 2;
        }
    }
    t.logicalWidth = w;
    t.logicalHeight = h;
    t.logicalFmt = FmtRgba;
    t.logicalSiz = Siz16b;
    t.image = std::move(image);
    return t;
}

Bytes TextureResource(const HdTexture& t) {
    Writer w;
    Bytes hdr = Header(kTypeTexture, 1);
    w.Raw(hdr.data(), hdr.size());
    w.U32(static_cast<uint32_t>(TexType::RGBA16));
    w.U32(t.image.width);
    w.U32(t.image.height);
    w.U32(1u << 0); // TEX_FLAG_LOAD_AS_RAW: the data is RGBA8, uploaded as is
    const uint32_t logicalLineBytes = t.logicalWidth * 2;
    w.F32(float(t.image.width * 4) / float(logicalLineBytes));
    w.F32(float(t.image.height) / float(t.logicalHeight));
    w.U32(static_cast<uint32_t>(t.image.rgba.size()));
    w.Raw(t.image.rgba.data(), t.image.rgba.size());
    return w.Out();
}

Bytes VertexArrayResource(const std::vector<Vtx>& verts) {
    Writer w;
    Bytes hdr = Header(kTypeArray, 0);
    w.Raw(hdr.data(), hdr.size());
    w.U32(0x19); // ArrayResourceType::Vertex
    w.U32(static_cast<uint32_t>(verts.size()));
    for (const Vtx& v : verts) {
        w.S16(v.x);
        w.S16(v.y);
        w.S16(v.z);
        w.U16(v.flag);
        w.S16(v.s);
        w.S16(v.t);
        w.U8(v.r);
        w.U8(v.g);
        w.U8(v.b);
        w.U8(v.a);
    }
    return w.Out();
}

// -- display lists ---------------------------------------------------------------------------------------------------

void DisplayList::Put(uint32_t w0, uint32_t w1) {
    mWords.emplace_back(w0, w1);
}

void DisplayList::PutHash(uint64_t h) {
    Put(static_cast<uint32_t>(h >> 32), static_cast<uint32_t>(h));
}

void DisplayList::PipeSync() {
    Put(0xE7000000, 0);
}
void DisplayList::TileSync() {
    Put(0xE8000000, 0);
}
void DisplayList::LoadSync() {
    Put(0xE6000000, 0);
}

void DisplayList::GeometryMode(uint32_t clear, uint32_t set) {
    Put((0xD9u << 24) | (~clear & 0xFFFFFF), set);
}

void DisplayList::OtherModeH(uint32_t shift, uint32_t len, uint32_t value) {
    Put((0xE3u << 24) | (((32 - shift - len) & 0xFF) << 8) | ((len - 1) & 0xFF), value);
}

void DisplayList::OtherModeL(uint32_t shift, uint32_t len, uint32_t value) {
    Put((0xE2u << 24) | (((32 - shift - len) & 0xFF) << 8) | ((len - 1) & 0xFF), value);
}

void DisplayList::RenderMode(uint32_t c1, uint32_t c2) {
    OtherModeL(Gbi::MdsftRenderMode, 29, c1 | c2);
}

void DisplayList::OtherMode(uint32_t h, uint32_t l) {
    Put((0xEFu << 24) | (h & 0xFFFFFF), l);
}

void DisplayList::Combine(const CcCycle& c0, const CcCycle& c1In) {
    // In the second cycle the N64 (and SoH's renderer) reads TEXEL0 as the next tile's texel and TEXEL1 as this
    // one's: callers describe both cycles with TEXEL0 = texture 0, so the second cycle's texel inputs are swapped here.
    CcCycle c1 = c1In;
    auto swapTex = [](uint8_t& x, uint8_t t0, uint8_t t1) {
        if (x == t0) {
            x = t1;
        } else if (x == t1) {
            x = t0;
        }
    };
    swapTex(c1.a, Gbi::ATex0, Gbi::ATex1);
    swapTex(c1.b, Gbi::BTex0, Gbi::BTex1);
    swapTex(c1.c, Gbi::CTex0, Gbi::CTex1);
    swapTex(c1.c, Gbi::CTex0Alpha, Gbi::CTex1Alpha);
    swapTex(c1.d, Gbi::DTex0, Gbi::DTex1);
    for (uint8_t* x : { &c1.aa, &c1.ba, &c1.ca, &c1.da }) {
        swapTex(*x, Gbi::AcTex0, Gbi::AcTex1);
    }
    const uint32_t w0 = (0xFCu << 24) | ((c0.a & 0xF) << 20) | ((c0.c & 0x1F) << 15) | ((c0.aa & 7) << 12) |
                        ((c0.ca & 7) << 9) | ((c1.a & 0xF) << 5) | (c1.c & 0x1F);
    const uint32_t w1 = ((c0.b & 0xFu) << 28) | ((c0.d & 7) << 15) | ((c0.ba & 7) << 12) | ((c0.da & 7) << 9) |
                        ((c1.b & 0xF) << 24) | ((c1.aa & 7) << 21) | ((c1.ca & 7) << 18) | ((c1.d & 7) << 6) |
                        ((c1.ba & 7) << 3) | (c1.da & 7);
    Put(w0, w1);
}

void DisplayList::PrimColor(uint8_t minLevel, uint8_t lodFrac, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Put((0xFAu << 24) | (uint32_t(minLevel) << 8) | lodFrac,
        (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | a);
}

void DisplayList::EnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Put(0xFBu << 24, (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | a);
}

void DisplayList::Texture(uint16_t s, uint16_t t, uint8_t level, uint8_t tile, bool on) {
    Put((0xD7u << 24) | ((level & 7u) << 11) | ((tile & 7u) << 8) | ((on ? 1u : 0u) << 1),
        (uint32_t(s) << 16) | t);
}

void DisplayList::LoadTexture(const std::string& path, const HdTexture& tex, uint8_t tile, uint16_t tmem,
                              uint8_t cms, uint8_t cmt, uint8_t masks, uint8_t maskt, uint8_t shifts,
                              uint8_t shiftt, uint32_t uls, uint32_t ult) {
    const uint32_t fmt = tex.logicalFmt, w = tex.logicalWidth, h = tex.logicalHeight;
    const uint32_t bytesPerTexel = 2; // logical RGBA16
    const uint32_t texels = w * h;
    const uint32_t wordsPerLine = std::max<uint32_t>(1, w * bytesPerTexel / 8);
    const uint32_t dxt = (2048 + wordsPerLine - 1) / wordsPerLine;
    // gDPSetTextureImage (hash form) + gDPSetTile(load) + gDPLoadBlock + gDPSetTile(render) + gDPSetTileSize
    Put((0x20u << 24) | (fmt << 21) | (uint32_t(Siz16b) << 19), 0);
    PutHash(PathHash(path));
    const uint32_t tileBits = (uint32_t(cmt & 3) << 18) | (uint32_t(maskt & 0xF) << 14) |
                              (uint32_t(shiftt & 0xF) << 10) | (uint32_t(cms & 3) << 8) |
                              (uint32_t(masks & 0xF) << 4) | (shifts & 0xFu);
    Put((0xF5u << 24) | (fmt << 21) | (uint32_t(Siz16b) << 19) | (tmem & 0x1FFu),
        (uint32_t(Gbi::TxLoadTile) << 24) | tileBits);
    LoadSync();
    Put((0xF3u << 24), (uint32_t(Gbi::TxLoadTile) << 24) | ((std::min<uint32_t>(texels, 2048) - 1) << 12) | dxt);
    PipeSync();
    const uint32_t line = (w * bytesPerTexel + 7) >> 3;
    Put((0xF5u << 24) | (fmt << 21) | (uint32_t(Siz16b) << 19) | ((line & 0x1FF) << 9) | (tmem & 0x1FFu),
        (uint32_t(tile & 7) << 24) | tileBits);
    uls &= 0xFFF;
    ult &= 0xFFF;
    Put((0xF2u << 24) | (uls << 12) | ult,
        (uint32_t(tile & 7) << 24) | ((((w - 1) << 2) + uls) & 0xFFF) << 12 | ((((h - 1) << 2) + ult) & 0xFFF));
}

void DisplayList::TileSize(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
    Put((0xF2u << 24) | ((uls & 0xFFF) << 12) | (ult & 0xFFF),
        (uint32_t(tile & 7) << 24) | ((lrs & 0xFFF) << 12) | (lrt & 0xFFF));
}

void DisplayList::Vertices(const std::string& arrayPath, uint32_t first, uint32_t count, uint32_t dest) {
    Put((0x32u << 24) | ((count & 0xFF) << 12) | (((dest + count) & 0x7F) << 1), first * 16);
    PutHash(PathHash(arrayPath));
}

void DisplayList::MatrixLoad(uint32_t segmentedAddress) {
    Put(0xDA380003u, segmentedAddress | 1u); // G_MTX, 64 bytes, (LOAD | NOPUSH) ^ PUSH; bit 0: segmented (LUS)
}

void DisplayList::Tri1(uint8_t a, uint8_t b, uint8_t c) {
    Put((0x05u << 24) | (uint32_t(a * 2) << 16) | (uint32_t(b * 2) << 8) | uint32_t(c * 2), 0);
}

void DisplayList::Tri2(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
    Put((0x06u << 24) | (uint32_t(a * 2) << 16) | (uint32_t(b * 2) << 8) | uint32_t(c * 2),
        (uint32_t(d * 2) << 16) | (uint32_t(e * 2) << 8) | uint32_t(f * 2));
}

void DisplayList::Call(const std::string& dlPath) {
    Put(0x31u << 24, 0);
    PutHash(PathHash(dlPath));
}

void DisplayList::CallSegment(uint32_t segmentedAddress) {
    // SoH tags segmented addresses with the low bit (Fast3D's SegAddr resolves only those).
    Put(0xDEu << 24, segmentedAddress | 1u);
}

void DisplayList::End() {
    Put(0xDFu << 24, 0);
}

Bytes DisplayList::Resource() const {
    Writer w;
    Bytes hdr = Header(kTypeDisplayList, 0);
    w.Raw(hdr.data(), hdr.size());
    w.U8(4); // UcodeHandlers: F3DEX2
    while (w.Size() % 8) {
        w.U8(0xFF);
    }
    for (const auto& [w0, w1] : mWords) {
        w.U32(w0);
        w.U32(w1);
    }
    return w.Out();
}

} // namespace Oot3d::Soh
