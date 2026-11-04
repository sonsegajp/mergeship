#pragma once
// OoT3D asset converter: CMB geometry -> N64 display lists (shared by rooms, objects and skinned actors).
//
// Triangles are grouped per output bucket (a room's mesh entry, or one object display list) and per material, then
// written as F3DEX2 lists: material state, full-resolution textures (Soh::HdTexture), and 32-vertex batches whose
// texture coordinates are re-centered per triangle so they fit the N64's s10.5 range.

#include "Oot3dCmb.h"
#include "Oot3dExtract.h"
#include "Oot3dMaterial.h"
#include "Oot3dSoh.h"
#include "Oot3dZip.h"

#include <functional>
#include <string>
#include <vector>

namespace Oot3d {

class TextureStore;

struct MeshOptions {
    SceneLight light;        // for baking 3DS vertex lighting into SHADE
    bool allowFog = true;    // false: never G_FOG (N64 prerendered rooms)
    bool n64Lighting = false; // G_LIGHTING with normals (actors and objects the N64 lights) instead of baked SHADE
    // The vanilla list this replaces is a glow (intensity textures the actor blends): the 3DS actor code draws the
    // model additively though its materials don't say so.
    bool glow = false;
    // The game draws it translucent (POLY_XLU): materials that say opaque keep their alpha and blend.
    bool drawnTranslucent = false;
    // The vanilla list is tinted by an ENV / PRIM color its actor sets: a greyscale 3DS texture takes that tint.
    bool tintEnv = false, tintPrim = false;
    // The N64 list is one model recolored per use (the Goron and Zora tunics): the 3DS texture goes to luminance so
    // the actor's color takes it (only with tintEnv / tintPrim).
    bool desaturate = false;
    // The actor flashes the model with ENV (a bomb about to blow): ENV is added to the material's color and left to
    // the actor.
    bool envFlash = false;
    // The vanilla list's alpha takes PRIM alpha the actor sets (a fade): translucent materials multiply by it.
    bool primAlphaFade = false;
    // Sphere-mapped reflections added on top (N64Material::envSlot) get a G_TEXTURE_GEN pass (static lists).
    bool envPass = false;
    // Static objects: a black constant scaling the color (TranslateMaterial's envTint, a runtime tint on characters)
    // takes the actor's PRIM or ENV when the vanilla list does (shadows, the Z-target arrow).
    bool constantTints = false;
};

struct MeshTri {
    Vec3 pos[3];
    Vec3 normal[3];
    std::array<float, 2> uv0[3]; // TEXEL0 coordinates, in texture repeats
    std::array<float, 2> uv1[3]; // TEXEL1
    std::array<uint8_t, 4> shade[3];
    uint64_t key[3]; // vertex identity, for sharing within a batch
};

struct MeshGroup {
    int order = 0;
    int tag = 0;
    const Cmb* cmb = nullptr;
    const CmbMaterial* src = nullptr;
    N64Material mat;
    std::vector<MeshTri> tris;
    bool vertexAlphaVaries = false;
};

// Skinned output: the engine skins positions/normals each frame; batches keep everything else of the vertices.
struct SkinBatchVertex {
    uint64_t key; // MeshTri::key of the source vertex
    Soh::Vtx vtx; // texture coordinates and SHADE (or alpha only, when lit)
    uint8_t envAlpha = 0; // the reflection pass's SHADE alpha (SkinGroup::envState)
};
struct SkinBatch {
    std::vector<SkinBatchVertex> vertices; // <= 32
    std::vector<std::array<uint8_t, 3>> tris;
};
struct SkinGroup {
    std::string statePath; // display list with the material state (textures, combiner, modes)
    bool lit = false, translucent = false;
    int tag = 0; // MeshBuilder::Add's tag of its sets
    int material = -1; // the CMB material it draws
    // Texture palette (eyes, mouths): the state list again with each palette texture in place of the animated one.
    std::vector<std::string> palettePaths;
    // A sphere-mapped reflection (N64Material::envSlot): its state list (empty: none), written at envStatePath; the
    // engine draws the batches again under it with normals and SkinBatchVertex::envAlpha.
    Bytes envState;
    std::string envStatePath;
    std::vector<SkinBatch> batches;
};

// A material's texture palette (CMAB): the images that replace texture slot `slot`.
struct SkinPalette {
    int slot = 0;
    std::vector<Image> images;
};

// A material's texture scroll (CMAB translation tracks), in texture repeats per 3DS frame (30 per second).
struct ScrollRate {
    float s[2] = { 0, 0 }, t[2] = { 0, 0 }; // per texture slot of the CMB material (TEXEL0 / TEXEL1 sources)
};
// One scrolling material in the output: a small list of the tiles' SetTileSize the room geometry calls, rewritten
// by the engine every frame (MergeShip3dsScroll).
struct ScrollEntry {
    std::string path;
    struct Tile {
        uint8_t tile = 0;
        uint16_t width = 0, height = 0; // logical texels
        uint16_t uls = 0, ult = 0;       // base origin, 10.2
        float rateS = 0, rateT = 0;      // 10.2 units per 3DS frame
    };
    std::vector<Tile> tiles;
};

struct MeshBucket {
    std::vector<MeshGroup> opa, xlu;
    bool Empty() const {
        return opa.empty() && xlu.empty();
    }
};

class MeshBuilder {
  public:
    MeshBuilder(ZipWriter& zip, TextureStore& textures, const MeshOptions& opt,
                std::function<void(LogLevel, const std::string&)> log);

    // Adds a model's triangles at its bind pose, moved by `transform`; `bucket(centroid)` picks each triangle's bucket.
    // `tagOf` (optional) splits groups further: sets with different tags never share a group (skins use it for
    // meshes shown only with a given vanilla display list, see SkinGroup::tag).
    void Add(const Cmb& cmb, int cmbIndex, const Mat34& transform,
             const std::function<size_t(const Vec3&)>& bucket = nullptr,
             const std::function<int(const CmbTriangleSet&)>& tagOf = nullptr);

    size_t BucketCount() const {
        return mBuckets.size();
    }
    const MeshBucket& Bucket(size_t i) const {
        return mBuckets.at(i);
    }
    // Largest distance of bucket geometry from `center`.
    float Radius(size_t bucket, const Vec3& center) const;

    enum class Part { Opaque, Translucent, Both };
    // Writes the bucket's groups as a display list resource at `path`, its vertices at `vtxPath` (default
    // `path`_vtx). Returns "" when empty.
    std::string Emit(size_t bucket, Part part, const std::string& path, const std::string& vtxPath = "");
    // Skinned models: one state display list per material group (`prefix`_g<n>) and the batches to skin at runtime,
    // opaque groups first.
    std::vector<SkinGroup> EmitSkin(size_t bucket, const std::string& prefix);

    uint32_t warnings = 0;
    // For materials tinted at runtime (N64Material::envTint): the segmented display list (0x0X000000) that sets the
    // tint, as the vanilla limb there calls it; 0 leaves ENV white.
    std::function<uint32_t(const MeshGroup&)> segmentFor;
    // Skins: the texture palette animating a group's material, if any (EmitSkin writes a state list per image).
    std::function<const SkinPalette*(const MeshGroup&)> paletteFor;
    // Rooms: the scroll of a group's material (CMAB), if any; every scrolling group then calls its own tile-size list.
    std::function<const ScrollRate*(const MeshGroup&)> scrollFor;
    // Characters split over several N64 skeletons (the cow and its tail): a triangle is added when at least two of its
    // vertices pass.
    std::function<bool(const CmbVertex&)> keepVertex;
    std::vector<ScrollEntry> scrolls;

  private:
    void EmitGroup(Soh::DisplayList& dl, const MeshGroup& g, std::vector<Soh::Vtx>& verts, const std::string& vtxPath,
                   SkinGroup* skin = nullptr, const Image* overrideTex = nullptr, int overrideSlot = -1);

    ZipWriter& mZip;
    TextureStore& mTextures;
    MeshOptions mOpt;
    std::function<void(LogLevel, const std::string&)> mLog;
    std::vector<MeshBucket> mBuckets;
};

} // namespace Oot3d
