#pragma once
// OoT3D asset converter: Ship of Harkinian resources (binary, as SoH's own archives store them) and an F3DEX2
// display list builder that emits LUS's hash-referencing commands.

#include "Oot3dBinary.h"
#include "Oot3dTexture.h"

#include <string>
#include <vector>

namespace Oot3d::Soh {

// Resource type fourccs (the header stores them as little-endian u32).
constexpr uint32_t kTypeTexture = 0x4F544558;     // OTEX
constexpr uint32_t kTypeDisplayList = 0x4F444C54; // ODLT
constexpr uint32_t kTypeArray = 0x4F415252;       // OARR
constexpr uint32_t kTypeVertex = 0x4F565458;      // OVTX

// The 64-byte header in front of every binary resource (ResourceLoader::ReadResourceInitDataBinary).
Bytes Header(uint32_t type, uint32_t version);

// N64 texture formats (G_IM_FMT_*, G_IM_SIZ_*) and LUS's Texture::Type values.
enum ImFmt : uint8_t { FmtRgba = 0, FmtYuv = 1, FmtCi = 2, FmtIa = 3, FmtI = 4 };
enum ImSiz : uint8_t { Siz4b = 0, Siz8b = 1, Siz16b = 2, Siz32b = 3 };
enum class TexType : uint32_t {
    RGBA32 = 1,
    RGBA16 = 2,
    CI4 = 3,
    CI8 = 4,
    I4 = 5,
    I8 = 6,
    IA4 = 7,
    IA8 = 8,
    IA16 = 9,
};

// A texture resource holding a full-resolution RGBA8 image that display lists load as a smaller "logical" N64
// texture: LUS's HD path (Texture v1, TEX_FLAG_LOAD_AS_RAW) uploads the whole image while the display list keeps
// N64-sized tiles, so any resolution works without TMEM limits. `logical*` is what the display list declares.
struct HdTexture {
    Image image;
    ImFmt logicalFmt = FmtRgba;
    ImSiz logicalSiz = Siz16b;
    uint32_t logicalWidth = 0, logicalHeight = 0;
};
Bytes TextureResource(const HdTexture& tex);
// Picks the logical size for an image: the largest power-of-two-scaled size that fits TMEM as RGBA16 (<= 2048 texels),
// keeping the aspect ratio.
HdTexture MakeHdTexture(Image image);

struct Vtx {
    int16_t x = 0, y = 0, z = 0;
    uint16_t flag = 0;
    int16_t s = 0, t = 0;
    uint8_t r = 255, g = 255, b = 255, a = 255; // color, or normal xyz + alpha under G_LIGHTING
};
// SoH's vertex arrays are Array resources (type Vertex = 0x19), the form G_VTX_OTR_HASH resolves.
Bytes VertexArrayResource(const std::vector<Vtx>& verts);

// -- GBI -------------------------------------------------------------------------------------------------------------

namespace Gbi {
// geometry mode (F3DEX2)
constexpr uint32_t ZBuffer = 0x00000001, Shade = 0x00000004, CullFront = 0x00000200, CullBack = 0x00000400,
                   Fog = 0x00010000, Lighting = 0x00020000, TextureGen = 0x00040000,
                   TextureGenLinear = 0x00080000, Lod = 0x00100000, ShadingSmooth = 0x00200000,
                   Clipping = 0x00800000;
// othermode H shifts / values
constexpr uint32_t MdsftAlphaDither = 4, MdsftRgbDither = 6, MdsftCombKey = 8, MdsftTextConv = 9,
                   MdsftTextFilt = 12, MdsftTextLut = 14, MdsftTextLod = 16, MdsftTextDetail = 17,
                   MdsftTextPersp = 19, MdsftCycleType = 20, MdsftPipeline = 23;
constexpr uint32_t Cyc1Cycle = 0 << 20, Cyc2Cycle = 1 << 20;
constexpr uint32_t TfPoint = 0 << 12, TfBilerp = 2 << 12, TfAverage = 3 << 12;
constexpr uint32_t TpPersp = 1 << 19;
constexpr uint32_t TcFilt = 6 << 9;
constexpr uint32_t PmNPrimitive = 0;
constexpr uint32_t AdDisable = 3 << 4, CdMagicSq = 0, CdDisable = 3 << 6;
// othermode L
constexpr uint32_t MdsftAlphaCompare = 0, MdsftZSrcSel = 2, MdsftRenderMode = 3;
constexpr uint32_t AcNone = 0, AcThreshold = 1, AcDither = 3;
// render mode flags
constexpr uint32_t AaEn = 0x8, ZCmp = 0x10, ZUpd = 0x20, ImRd = 0x40, ClrOnCvg = 0x80, CvgDstClamp = 0,
                   CvgDstWrap = 0x100, CvgDstFull = 0x200, CvgDstSave = 0x300, ZmodeOpa = 0, ZmodeInter = 0x400,
                   ZmodeXlu = 0x800, ZmodeDec = 0xC00, CvgXAlpha = 0x1000, AlphaCvgSel = 0x2000, ForceBl = 0x4000;
// blender inputs
constexpr uint32_t BlClrIn = 0, BlClrMem = 1, BlClrBl = 2, BlClrFog = 3;
constexpr uint32_t BlAIn = 0, BlAFog = 1, BlAShade = 2, BlA0 = 3;
constexpr uint32_t Bl1MA = 0, BlAMem = 1, Bl1 = 2, Bl0 = 3;
constexpr uint32_t Blend1(uint32_t p, uint32_t a, uint32_t m, uint32_t b) {
    return (p << 30) | (a << 26) | (m << 22) | (b << 18);
}
constexpr uint32_t Blend2(uint32_t p, uint32_t a, uint32_t m, uint32_t b) {
    return (p << 28) | (a << 24) | (m << 20) | (b << 16);
}
// tile
constexpr uint32_t TxRenderTile = 0, TxLoadTile = 7;
constexpr uint32_t TxWrap = 0, TxMirror = 1, TxClamp = 2;

// combiner inputs
enum CcA : uint8_t { AComb = 0, ATex0 = 1, ATex1 = 2, APrim = 3, AShade = 4, AEnv = 5, A1 = 6, ANoise = 7, A0 = 15 };
enum CcB : uint8_t { BComb = 0, BTex0 = 1, BTex1 = 2, BPrim = 3, BShade = 4, BEnv = 5, BCenter = 6, BK4 = 7, B0 = 15 };
enum CcC : uint8_t {
    CComb = 0,
    CTex0 = 1,
    CTex1 = 2,
    CPrim = 3,
    CShade = 4,
    CEnv = 5,
    CScale = 6,
    CCombAlpha = 7,
    CTex0Alpha = 8,
    CTex1Alpha = 9,
    CPrimAlpha = 10,
    CShadeAlpha = 11,
    CEnvAlpha = 12,
    CLodFrac = 13,
    CPrimLodFrac = 14,
    CK5 = 15,
    C0 = 31,
};
enum CcD : uint8_t { DComb = 0, DTex0 = 1, DTex1 = 2, DPrim = 3, DShade = 4, DEnv = 5, D1 = 6, D0 = 7 };
// alpha combiner inputs (A, B, D share one table; C has LOD fraction in slot 0)
enum AcIn : uint8_t { AcComb = 0, AcTex0 = 1, AcTex1 = 2, AcPrim = 3, AcShade = 4, AcEnv = 5, Ac1 = 6, Ac0 = 7 };
} // namespace Gbi

// One combiner cycle: color = (a - b) * c + d, alpha = (aa - ba) * ca + da.
struct CcCycle {
    uint8_t a = Gbi::A0, b = Gbi::B0, c = Gbi::C0, d = Gbi::D0;
    uint8_t aa = Gbi::Ac0, ba = Gbi::Ac0, ca = Gbi::Ac0, da = Gbi::Ac0;
};

class DisplayList {
  public:
    void PipeSync();
    void TileSync();
    void LoadSync();
    void GeometryMode(uint32_t clear, uint32_t set);
    void OtherModeH(uint32_t shift, uint32_t len, uint32_t value);
    void OtherModeL(uint32_t shift, uint32_t len, uint32_t value);
    void RenderMode(uint32_t c1, uint32_t c2);
    void OtherMode(uint32_t h, uint32_t l); // gDPSetOtherMode: all of othermode H (24 bits) and L
    void Combine(const CcCycle& c1, const CcCycle& c2);
    void PrimColor(uint8_t minLevel, uint8_t lodFrac, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void EnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void Texture(uint16_t s, uint16_t t, uint8_t level, uint8_t tile, bool on);
    // Loads an HdTexture into TMEM at `tmem` (in 64-bit words) for render tile `tile`, gDPLoadTextureBlock style.
    // uls/ult: the tile's origin in 10.2 fixed point (shifts the texture under the coordinates).
    void LoadTexture(const std::string& path, const HdTexture& tex, uint8_t tile, uint16_t tmem, uint8_t cms,
                     uint8_t cmt, uint8_t masks, uint8_t maskt, uint8_t shifts, uint8_t shiftt, uint32_t uls = 0,
                     uint32_t ult = 0);
    void Vertices(const std::string& arrayPath, uint32_t first, uint32_t count, uint32_t dest = 0);
    // gSPMatrix(segmentedAddress, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW): a matrix the actor put in a segment.
    void MatrixLoad(uint32_t segmentedAddress);
    void Tri1(uint8_t a, uint8_t b, uint8_t c);
    void Tri2(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f);
    void Call(const std::string& dlPath);
    void TileSize(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt); // gDPSetTileSize, 10.2
    void CallSegment(uint32_t segmentedAddress); // gSPDisplayList(0x0X000000): a list the actor put in a segment
    void End();

    bool Empty() const {
        return mWords.empty();
    }
    Bytes Resource() const; // header + ucode byte + commands

  private:
    void Put(uint32_t w0, uint32_t w1);
    void PutHash(uint64_t h);
    std::vector<std::pair<uint32_t, uint32_t>> mWords;
};

} // namespace Oot3d::Soh
