#pragma once
// OoT3D asset converter: CMB materials -> N64 (Fast3D) render state.
//
// The PICA texture combiner program is evaluated symbolically into a polynomial over the texture samples and the
// vertex ("primary") color, with the material's constant colors folded into numeric coefficients. Everything that
// varies per vertex (3DS vertex lighting x vertex color x coefficients) is then baked into the N64 SHADE color, and
// what is left over the textures is matched to a one- or two-cycle N64 color combiner.

#include "Oot3dCmb.h"
#include "Oot3dSoh.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Oot3d {

// Scene lighting used to bake 3DS vertex lighting (ZSI environment settings, noclip's interpretation).
struct SceneLight {
    Vec3 dir[2]{ { 0.57715f, 0.57715f, 0.57715f }, { -0.57715f, -0.57715f, -0.57715f } };
    std::array<float, 3> diffuse[2]{ { 1, 1, 1 }, { 0, 0.2f, 0.3f } };
    std::array<float, 3> ambient[2]{ { 0.5f, 0.5f, 0.5f }, { 0, 0, 0 } };
};

// A monomial over the per-pixel inputs. Atoms: T0..T2 color, T0a..T2a alpha, V color, Va alpha.
enum Atom : uint8_t { AT0, AT1, AT2, AT0a, AT1a, AT2a, AV, AVa, AtomCount };
using Mono = std::vector<uint8_t>; // sorted atoms
struct Poly {
    std::map<Mono, std::array<float, 3>> terms; // rgb coefficients (alpha polys use [0])
};

struct N64Material {
    // Which CMB texture slots feed TEXEL0/TEXEL1 (-1: none).
    int texSlot[2] = { -1, -1 };
    Soh::CcCycle cycle1, cycle2;
    bool twoCycle = false;
    std::array<uint8_t, 4> prim{ 255, 255, 255, 255 }, env{ 255, 255, 255, 255 };
    // Per-vertex SHADE = clamp(shadeScale * lit vertex color) (rgb) and shadeAlphaScale * vertex alpha.
    std::array<float, 3> shadeScale{ 1, 1, 1 };
    float shadeAlphaScale = 1;
    bool shadeUsesVertex = true; // false: SHADE is constant (shadeScale)
    bool vertexLighting = false;
    bool usesShadeAlpha = false;
    bool translucent = false;
    bool additive = false; // approximated with alpha blending (Fast3D has no additive render mode)
    bool alphaTest = false;
    uint8_t alphaRef = 0;
    bool decal = false;
    bool fog = true;
    uint32_t cullMode = 1; // 0 none, 1 back, 2 front
    bool approximate = false; // the combiner could not be matched exactly
    // The color is multiplied by a constant the game sets at runtime (the N64 actor's env color): the combiner ends
    // with COMBINED * ENV and the state list leaves ENV to the actor (see MeshBuilder::segmentFor).
    bool envTint = false;
    // (X + Y) * X, a glow masked by itself (torch flames, spin trails, fire circles; Y scrolls): TEXEL `squareMask`
    // is X. Blended: its texture becomes rgb X^2, alpha X, and cycle 1 is (Y - 0) * X.a + X = X*Y + X^2. Additive
    // (intensity carried by alpha): alpha is the mask's (Y + 1) * f(X), f fitted to X * (X + Y) over Y's mean.
    int squareMask = -1;
    // A sphere-mapped reflection added on top (CMB texture slot `envSlot`), left out of the base pass: the mesh draws it
    // as a second pass over the same triangles with G_TEXTURE_GEN, times `envTerm` (its coefficient over the other
    // textures and the vertex color, evaluated per vertex).
    int envSlot = -1;
    Poly envTerm;
    std::string description;
};

// `drawnTranslucent`: the game draws it blended though the material says opaque (the 3DS actor code sets the
// blending); its alpha is kept and it blends.
N64Material TranslateMaterial(const Cmb& cmb, const CmbMaterial& m, bool drawnTranslucent = false);

// SHADE for one vertex (rgba 0..255), given its world-space normal.
std::array<uint8_t, 4> BakeShade(const N64Material& mat, const CmbMaterial& m, const CmbVertex& v, const Vec3& normal,
                                 const SceneLight& light, bool hasVertexColors);

std::string DescribeMaterial(const CmbMaterial& m); // Oot3dDebug.cpp

} // namespace Oot3d
