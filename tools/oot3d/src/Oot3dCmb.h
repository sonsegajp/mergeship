#pragma once
// OoT3D asset converter: CMB models (version 6 = OoT3D; 10 = MM3D is parsed too).
// Layout per noclip.website's OcarinaOfTime3D/cmb.ts, which renders these files correctly.

#include "Oot3dBinary.h"
#include "Oot3dMath.h"
#include "Oot3dTexture.h"

#include <array>
#include <string>
#include <vector>

namespace Oot3d {

// GL enums used by CMB materials.
namespace Gl {
constexpr uint16_t Nearest = 0x2600, Linear = 0x2601;
constexpr uint16_t Clamp = 0x2900, Repeat = 0x2901, ClampToEdge = 0x812F, MirroredRepeat = 0x8370;
constexpr uint16_t Zero = 0x0000, One = 0x0001, SrcColor = 0x0300, OneMinusSrcColor = 0x0301, SrcAlpha = 0x0302,
                   OneMinusSrcAlpha = 0x0303, DstAlpha = 0x0304, OneMinusDstAlpha = 0x0305, DstColor = 0x0306,
                   OneMinusDstColor = 0x0307;
constexpr uint16_t Never = 0x0200, Less = 0x0201, Equal = 0x0202, LEqual = 0x0203, Greater = 0x0204,
                   NotEqual = 0x0205, GEqual = 0x0206, Always = 0x0207;
} // namespace Gl

// PICA texture combiner ("TEV") enums (GL_*_DMP).
namespace Tev {
constexpr uint16_t Replace = 0x1E01, Modulate = 0x2100, Add = 0x0104, AddSigned = 0x8574, Interpolate = 0x8575,
                   Subtract = 0x84E7, Dot3Rgb = 0x86AE, Dot3Rgba = 0x86AF, MultAdd = 0x6401, AddMult = 0x6402;
constexpr uint16_t Texture0 = 0x84C0, Texture1 = 0x84C1, Texture2 = 0x84C2, Texture3 = 0x84C3, Constant = 0x8576,
                   PrimaryColor = 0x8577, Previous = 0x8578, PreviousBuffer = 0x8579, FragPrimary = 0x6210,
                   FragSecondary = 0x6211;
constexpr uint16_t SrcColor = 0x0300, OneMinusSrcColor = 0x0301, SrcAlpha = 0x0302, OneMinusSrcAlpha = 0x0303,
                   SrcR = 0x8580, SrcG = 0x8581, SrcB = 0x8582, OneMinusSrcR = 0x8583, OneMinusSrcG = 0x8584,
                   OneMinusSrcB = 0x8585;
} // namespace Tev

struct CmbBone {
    int id = 0;
    int parent = -1;
    Vec3 scale{ 1, 1, 1 }, rotation, translation;
};

struct CmbTexBinding {
    int16_t texture = -1;
    uint16_t minFilter = Gl::Linear, magFilter = Gl::Linear;
    uint16_t wrapS = Gl::Repeat, wrapT = Gl::Repeat;
    float minLod = 0, lodBias = 0;
};

enum class TexMapping : uint8_t { None = 0, Uv = 1, CubeEnv = 2, SphereEnv = 3, Projection = 4 };

struct CmbTexCoordinator {
    uint8_t source = 0; // which vertex UV set
    uint8_t referenceCamera = 0;
    TexMapping mapping = TexMapping::Uv;
    uint8_t matrixMode = 0;
    float scaleS = 1, scaleT = 1, translateS = 0, translateT = 0, rotation = 0;
    // Applies the texture matrix (noclip calcTexMtx) to a UV.
    void Apply(float u, float v, float& s, float& t) const;
    bool IsIdentity() const;
};

struct CmbCombiner {
    uint16_t combineRgb = Tev::Replace, combineAlpha = Tev::Replace;
    uint16_t scaleRgb = 1, scaleAlpha = 1;
    uint16_t bufferInputRgb = Tev::Previous, bufferInputAlpha = Tev::Previous;
    std::array<uint16_t, 3> srcRgb{}, opRgb{}, srcAlpha{}, opAlpha{};
    uint32_t constantIndex = 0;
};

using Rgba8 = std::array<uint8_t, 4>;

struct CmbMaterial {
    bool fragmentLighting = false, vertexLighting = false, fog = true;
    uint8_t renderLayer = 0;
    uint8_t cullMode = 1; // 0/3 none, 1 back, 2 front
    bool polygonOffset = false;
    int8_t polygonOffsetUnit = 0;
    uint32_t bindingCount = 0, coordinatorCount = 0;
    std::array<CmbTexBinding, 3> bindings{};
    std::array<CmbTexCoordinator, 3> coordinators{};
    Rgba8 emission{}, ambient{}, diffuse{ 255, 255, 255, 255 }, specular0{}, specular1{};
    std::array<Rgba8, 6> constants{};
    std::array<float, 4> bufferColor{};
    std::vector<CmbCombiner> combiners;
    bool alphaTest = false;
    uint8_t alphaRef = 0;
    uint16_t alphaFunc = Gl::Always;
    bool depthTest = true, depthWrite = true;
    uint16_t depthFunc = Gl::LEqual;
    uint8_t blendMode = 0; // 0 off; nonzero: blending (and 3 = logic op in later games)
    uint16_t blendSrcRgb = Gl::One, blendDstRgb = Gl::Zero, blendOpRgb = 0x8006;
    uint16_t blendSrcAlpha = Gl::One, blendDstAlpha = Gl::Zero, blendOpAlpha = 0x8006;
    std::array<float, 4> blendColor{};

    bool Blended() const {
        return blendMode != 0;
    }
    // The coordinator feeding texture slot `slot` (texture 2 may share coordinator 1).
    int CoordinatorForSlot(int slot) const;
};

enum class Skinning : uint16_t { SingleBone = 0, Rigid = 1, Smooth = 2 };

struct CmbAttrib {
    uint32_t start = 0;
    float scale = 1;
    uint16_t type = 0x1406; // GL data type
    uint16_t mode = 0;      // 0 array, 1 constant
    std::array<float, 4> constant{};
};

struct CmbPrimitive {
    Skinning skinning = Skinning::SingleBone;
    std::vector<uint16_t> boneTable;
    uint16_t indexType = 0x1403;
    uint32_t indexCount = 0;
    uint32_t indexByteOffset = 0; // into the model's index buffer
};

struct CmbShape {
    uint16_t flags = 0;
    Vec3 center, positionOffset;
    CmbAttrib position, normal, color, uv[3], boneIndices, boneWeights;
    uint16_t boneDimension = 0;
    bool hasVertexColors = false;
    std::vector<CmbPrimitive> primitives;
};

struct CmbMesh {
    uint16_t shape = 0;
    uint8_t material = 0;
};

// One vertex, fully decoded (attribute scales applied). `pos` is in the space the primitive's skinning mode implies:
// bone-local for SingleBone/Rigid, model (bind pose) space for Smooth.
struct CmbVertex {
    Vec3 pos, normal;
    std::array<float, 4> color{ 1, 1, 1, 1 };
    std::array<std::array<float, 2>, 3> uv{};
    std::array<int, 4> bones{ 0, 0, 0, 0 };        // model bone indices (already through the bone table)
    std::array<float, 4> weights{ 1, 0, 0, 0 };
    int boneCount = 1;
};

struct CmbTriangleSet {
    int mesh = 0;
    int material = 0;
    Skinning skinning = Skinning::SingleBone;
    bool hasVertexColors = false; // without them the 3DS uses the material's diffuse color as the vertex color
    std::vector<CmbVertex> vertices;      // unique vertices of this primitive
    std::vector<std::array<uint32_t, 3>> triangles;
};

struct Cmb {
    std::string name;
    uint32_t version = 6;
    std::vector<CmbBone> bones;
    std::vector<CmbMaterial> materials;
    std::vector<TextureDesc> textures;
    std::vector<CmbMesh> meshes;
    uint32_t opaqueMeshCount = 0;
    std::vector<CmbShape> shapes;
    View texData;
    View vatrPosition, vatrNormal, vatrColor, vatrUv[3], vatrBoneIndices, vatrBoneWeights;
    View indexBuffer;
    View file; // the whole CMB
    mutable uint32_t decodeIssues = 0; // attribute reads Decode() had to skip

    static Cmb Parse(View data);
    // Bone world matrices at the bind pose (parents first).
    std::vector<Mat34> BindPose() const;
    // Every mesh's primitives decoded into self-contained triangle sets.
    std::vector<CmbTriangleSet> Decode() const;
    Image DecodeTexture(size_t index) const;
};

// Positions in model space at the bind pose (skinning applied with `boneWorld`, usually BindPose()).
Vec3 SkinPosition(const CmbVertex& v, Skinning mode, const std::vector<Mat34>& boneWorld,
                  const std::vector<Mat34>& invBind);
Vec3 SkinNormal(const CmbVertex& v, Skinning mode, const std::vector<Mat34>& boneWorld,
                const std::vector<Mat34>& invBind);

} // namespace Oot3d
