#include "Oot3dMesh.h"

#include "Oot3dMaps.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace Oot3d {

using namespace Soh;

namespace {

uint8_t WrapMode(uint16_t gl) {
    switch (gl) {
        case Gl::MirroredRepeat: return Gbi::TxMirror;
        case Gl::Clamp:
        case Gl::ClampToEdge: return Gbi::TxClamp;
        default: return Gbi::TxWrap;
    }
}

uint32_t Log2(uint32_t v) {
    uint32_t r = 0;
    while ((1u << (r + 1)) <= v) {
        r++;
    }
    return r;
}

// Picks a logical texture width/height that keeps the widest triangle's texture coordinates inside the N64's s10.5
// vertex range, capped by what MakeHdTexture would choose for the image.
void LogicalSize(const Image& img, float maxSpanS, float maxSpanT, uint32_t& w, uint32_t& h) {
    HdTexture def = MakeHdTexture(Image{ img.width, img.height, {} });
    w = def.logicalWidth;
    h = def.logicalHeight;
    // |S| must stay below 1024 texels after centering a triangle; allow 900.
    while (w > 1 && maxSpanS * w > 900.0f) {
        w /= 2;
    }
    while (h > 1 && maxSpanT * h > 900.0f) {
        h /= 2;
    }
    w = std::max<uint32_t>(w, 4); // RGBA16 lines are at least 8 bytes
}

bool UsesColorInput(const CcCycle& c, uint8_t aIn, uint8_t cIn, uint8_t dIn) {
    return c.a == aIn || c.b == aIn || c.c == cIn || c.d == dIn;
}

} // namespace

MeshBuilder::MeshBuilder(ZipWriter& zip, TextureStore& textures, const MeshOptions& opt,
                         std::function<void(LogLevel, const std::string&)> log)
    : mZip(zip), mTextures(textures), mOpt(opt), mLog(std::move(log)) {
}

void MeshBuilder::Add(const Cmb& cmb, int cmbIndex, const Mat34& transform,
                      const std::function<size_t(const Vec3&)>& bucketOf,
                      const std::function<int(const CmbTriangleSet&)>& tagOf) {
    const auto world = cmb.BindPose();
    std::vector<Mat34> inv(world.size());
    for (size_t i = 0; i < world.size(); i++) {
        inv[i] = world[i].Inverse();
    }
    std::map<int, N64Material> mats;
    const auto sets = cmb.Decode();
    std::map<std::tuple<size_t, int, int, int>, size_t> groupIndex; // (bucket, xlu, material, tag) -> group
    int setNo = 0;
    for (const CmbTriangleSet& set : sets) {
        setNo++;
        if (set.material >= int(cmb.materials.size())) {
            continue;
        }
        const CmbMaterial& m = cmb.materials[set.material];
        auto mit = mats.find(set.material);
        if (mit == mats.end()) {
            mit = mats.emplace(set.material, TranslateMaterial(cmb, m, mOpt.drawnTranslucent)).first;
            if (mit->second.approximate) {
                warnings++;
                if (mLog) {
                    mLog(LogLevel::Detail, "  approximated material " + cmb.name + "#" + std::to_string(set.material) +
                                               ": " + mit->second.description);
                }
            }
        }
        const N64Material& mat = mit->second;
        const int tag = tagOf ? tagOf(set) : 0;
        std::vector<Vec3> pos(set.vertices.size()), nrm(set.vertices.size());
        for (size_t i = 0; i < set.vertices.size(); i++) {
            pos[i] = transform.Point(SkinPosition(set.vertices[i], set.skinning, world, inv));
            nrm[i] = transform.Normal(SkinNormal(set.vertices[i], set.skinning, world, inv));
        }
        auto texCoords = [&](int slot, const CmbVertex& v) -> std::array<float, 2> {
            if (slot < 0) {
                return { 0, 0 };
            }
            const CmbTexCoordinator& c = m.coordinators[m.CoordinatorForSlot(slot)];
            const auto& uv = v.uv[std::min<int>(c.source, 2)];
            float s, t;
            c.Apply(uv[0], uv[1], s, t);
            return { s, t };
        };
        for (const auto& tri : set.triangles) {
            if (keepVertex && int(keepVertex(set.vertices[tri[0]])) + int(keepVertex(set.vertices[tri[1]])) +
                                      int(keepVertex(set.vertices[tri[2]])) < 2) {
                continue;
            }
            MeshTri p;
            Vec3 centroid;
            for (int k = 0; k < 3; k++) {
                const CmbVertex& v = set.vertices[tri[k]];
                p.pos[k] = pos[tri[k]];
                p.normal[k] = nrm[tri[k]];
                p.uv0[k] = texCoords(mat.texSlot[0], v);
                p.uv1[k] = texCoords(mat.texSlot[1], v);
                p.shade[k] = BakeShade(mat, m, v, nrm[tri[k]], mOpt.light, set.hasVertexColors);
                p.key[k] = (uint64_t(cmbIndex) << 48) | (uint64_t(setNo) << 24) | tri[k];
                centroid += p.pos[k] * (1.0f / 3.0f);
            }
            const size_t b = bucketOf ? bucketOf(centroid) : 0;
            if (b >= mBuckets.size()) {
                mBuckets.resize(b + 1);
            }
            const int xlu = mat.translucent ? 1 : 0;
            const auto key = std::make_tuple(b, xlu, set.material + cmbIndex * 1000, tag);
            auto git = groupIndex.find(key);
            std::vector<MeshGroup>& list = xlu ? mBuckets[b].xlu : mBuckets[b].opa;
            if (git == groupIndex.end()) {
                MeshGroup g;
                g.order = cmbIndex * 100000 + m.renderLayer * 1000 + set.mesh;
                g.tag = tag;
                g.cmb = &cmb;
                g.src = &m;
                g.mat = mat;
                list.push_back(std::move(g));
                git = groupIndex.emplace(key, list.size() - 1).first;
            }
            MeshGroup& g = list[git->second];
            for (int k = 0; k < 3; k++) {
                if (p.shade[k][3] != p.shade[0][3] || (!g.tris.empty() && g.tris[0].shade[0][3] != p.shade[k][3])) {
                    g.vertexAlphaVaries = true;
                }
            }
            g.tris.push_back(p);
        }
    }
}

float MeshBuilder::Radius(size_t bucket, const Vec3& c) const {
    float r = 0;
    if (bucket >= mBuckets.size()) {
        return r;
    }
    for (const auto* list : { &mBuckets[bucket].opa, &mBuckets[bucket].xlu }) {
        for (const MeshGroup& g : *list) {
            for (const MeshTri& p : g.tris) {
                for (const Vec3& v : p.pos) {
                    r = std::max(r, (v - c).Length());
                }
            }
        }
    }
    return r;
}

void MeshBuilder::EmitGroup(DisplayList& dl, const MeshGroup& g, std::vector<Vtx>& verts, const std::string& vtxPath,
                            SkinGroup* skin, const Image* overrideTex, int overrideSlot) {
    N64Material mat = g.mat;
    const CmbMaterial& m = *g.src;
    const Cmb& cmb = *g.cmb;
    // N64 lighting stands in for the 3DS vertex lighting; unlit 3DS materials keep their baked SHADE.
    const bool lit = mOpt.n64Lighting && mat.vertexLighting;

    struct TexUse {
        bool on = false;
        Image img;
        std::string path;
        uint32_t lw = 0, lh = 0;
        uint8_t cms = 0, cmt = 0;
    } tex[2];
    for (int i = 0; i < 2; i++) {
        const int slot = mat.texSlot[i];
        if (slot < 0) {
            continue;
        }
        tex[i].on = true;
        try {
            tex[i].img = (overrideTex && slot == overrideSlot) ? *overrideTex : cmb.DecodeTexture(m.bindings[slot].texture);
        } catch (const std::exception& e) {
            // A texture outside the model file (kept beside it in the archive) or damaged: white keeps the
            // combiner's shape.
            warnings++;
            if (mLog) {
                mLog(LogLevel::Detail, "  missing texture " + std::to_string(m.bindings[slot].texture) + " in " +
                                           cmb.name + ": " + e.what());
            }
            tex[i].img.width = tex[i].img.height = 8;
            tex[i].img.rgba.assign(8 * 8 * 4, 255);
        }
        tex[i].cms = WrapMode(m.bindings[slot].wrapS);
        tex[i].cmt = WrapMode(m.bindings[slot].wrapT);
    }
    // A glow the actor blends: a 3DS texture with no alpha of its own (light on black) is drawn additively.
    if (mOpt.glow && tex[0].on && !tex[0].img.HasAlpha()) {
        mat.additive = true;
        mat.translucent = true;
        mat.alphaTest = false;
    }
    // Additive blending (glows, fire, sparkles) has no Fast3D render mode: the texture's brightness becomes its
    // alpha (black transparent, white opaque) and its color is brought back to full strength, so alpha blending
    // gives nearly the same light.
    if (mat.additive) {
        for (int i = 0; i < 2; i++) {
            if (!tex[i].on || !tex[i].img.Valid()) {
                continue;
            }
            for (size_t k = 0; k + 3 < tex[i].img.rgba.size(); k += 4) {
                uint8_t* px = &tex[i].img.rgba[k];
                const int mx = std::max({ px[0], px[1], px[2] });
                for (int c = 0; c < 3; c++) {
                    px[c] = static_cast<uint8_t>(mx > 0 ? std::min(255, px[c] * 255 / mx) : 0);
                }
                px[3] = static_cast<uint8_t>(px[3] * mx / 255);
            }
        }
    }
    // (X + Y) * X (TranslateMaterial's squareMask): the mask texture's channels carry the product.
    if (mat.squareMask >= 0 && tex[mat.squareMask].on && tex[mat.squareMask].img.Valid()) {
        std::vector<uint8_t>& x = tex[mat.squareMask].img.rgba;
        if (mat.additive) {
            // Alpha (the intensity, X after the normalization above) = f(X) = X * (X + y) / (1 + y), y the other
            // texture's mean: (Y + 1) * f(X) is X * (X + Y) where Y is at its mean, exact at X = 1.
            float y = 0.5f;
            const auto& yt = tex[1 - mat.squareMask];
            if (yt.on && yt.img.Valid() && yt.img.rgba.size() >= 4) {
                double sum = 0;
                for (size_t k = 3; k < yt.img.rgba.size(); k += 4) {
                    sum += yt.img.rgba[k] / 255.0;
                }
                y = static_cast<float>(sum / double(yt.img.rgba.size() / 4));
            }
            for (size_t k = 0; k + 3 < x.size(); k += 4) {
                const float v = x[k + 3] / 255.0f;
                x[k + 3] = static_cast<uint8_t>(std::clamp(v * (v + y) / (1.0f + y) * 255.0f + 0.5f, 0.0f, 255.0f));
            }
        } else {
            // Color X^2, alpha X (a gray mask).
            for (size_t k = 0; k + 3 < x.size(); k += 4) {
                const int v = (x[k] * 77 + x[k + 1] * 150 + x[k + 2] * 29) >> 8;
                x[k] = x[k + 1] = x[k + 2] = static_cast<uint8_t>((v * v + 127) / 255);
                x[k + 3] = static_cast<uint8_t>(v);
            }
        }
    }
    float span[2][2] = { { 0, 0 }, { 0, 0 } };
    for (const MeshTri& p : g.tris) {
        for (int i = 0; i < 2; i++) {
            const auto* uv = i == 0 ? p.uv0 : p.uv1;
            for (int c = 0; c < 2; c++) {
                const float lo = std::min({ uv[0][c], uv[1][c], uv[2][c] });
                const float hi = std::max({ uv[0][c], uv[1][c], uv[2][c] });
                span[i][c] = std::max(span[i][c], hi - lo);
            }
        }
    }
    for (int i = 0; i < 2; i++) {
        if (tex[i].on) {
            // Clamped textures need their whole range representable, not just one triangle's.
            LogicalSize(tex[i].img, span[i][0] + (tex[i].cms == Gbi::TxClamp ? 2.0f : 1.0f),
                        span[i][1] + (tex[i].cmt == Gbi::TxClamp ? 2.0f : 1.0f), tex[i].lw, tex[i].lh);
            tex[i].path = mTextures.Add(tex[i].img, tex[i].lw, tex[i].lh);
        }
    }

    const uint32_t filter =
        (tex[0].on && m.bindings[mat.texSlot[0]].magFilter == Gl::Nearest) ? Gbi::TfPoint : Gbi::TfBilerp;
    // Under bilinear filtering SoH's renderer adds half a texel to every coordinate, as the N64 samples, and that texel
    // is the logical one (2+ real texels of an HD texture): the vertices carry the opposite offset, so the 3DS
    // coordinates are sampled exactly (not shifted into a neighbouring atlas region).
    const float half = filter != Gbi::TfPoint ? 0.5f : 0.0f;

    // TEXEL1 rides TEXEL0's coordinates through tile 1's shift and origin: s1 = s0 * f + o.
    int shiftS = 0, shiftT = 0;
    float originS = 0, originT = 0;
    if (tex[0].on && tex[1].on && !g.tris.empty()) {
        for (int c = 0; c < 2; c++) {
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            int n = 0;
            for (const MeshTri& p : g.tris) {
                for (int k = 0; k < 3; k++) {
                    const double x = p.uv0[k][c], y = p.uv1[k][c];
                    sx += x, sy += y, sxx += x * x, sxy += x * y;
                    n++;
                }
            }
            const double den = n * sxx - sx * sx;
            const double a = std::fabs(den) > 1e-9 ? (n * sxy - sx * sy) / den : 1.0;
            const double b = (sy - a * sx) / n;
            const double l0 = c == 0 ? tex[0].lw : tex[0].lh, l1 = c == 0 ? tex[1].lw : tex[1].lh;
            double f = a * l1 / l0;
            if (f <= 0) {
                f = 1;
            }
            const int e = std::clamp(static_cast<int>(std::lround(std::log2(f))), -10, 5);
            // Tile shift: 1..10 divides, 11..15 multiplies by 2^(16-shift).
            (c == 0 ? shiftS : shiftT) = e < 0 ? -e : (e > 0 ? 16 - e : 0);
            // The vertices' half-texel offset reaches tile 1 scaled by 2^e; the origin takes that out and the
            // renderer's own half texel back.
            double origin = std::fmod(-b * l1 + half - half * std::ldexp(1.0, e), l1);
            if (origin < 0) {
                origin += l1;
            }
            (c == 0 ? originS : originT) = static_cast<float>(origin);
        }
    }

    // Render state.
    uint32_t geo = Gbi::ZBuffer | Gbi::Shade | Gbi::ShadingSmooth;
    if (mat.cullMode == 1) {
        geo |= Gbi::CullBack;
    } else if (mat.cullMode == 2) {
        geo |= Gbi::CullFront;
    }
    if (lit) {
        geo |= Gbi::Lighting;
    }
    bool fog = mat.fog && g.src->fog && mOpt.allowFog;
    bool shadeAlpha = mat.usesShadeAlpha;
    CcCycle c1 = mat.cycle1, c2 = mat.cycle2;
    std::array<uint8_t, 4> prim = mat.prim;
    if (shadeAlpha && !g.vertexAlphaVaries && !g.tris.empty()) {
        // Constant vertex alpha: move it to PRIMITIVE alpha, so fog may use SHADE alpha.
        bool primAlphaFree = true;
        for (uint8_t x : { c1.aa, c1.ba, c1.ca, c1.da, c2.aa, c2.ba, c2.ca, c2.da }) {
            if (x == Gbi::AcPrim) {
                primAlphaFree = false;
            }
        }
        if (primAlphaFree) {
            for (uint8_t* x : { &c1.aa, &c1.ba, &c1.ca, &c1.da, &c2.aa, &c2.ba, &c2.ca, &c2.da }) {
                if (*x == Gbi::AcShade) {
                    *x = Gbi::AcPrim;
                }
            }
            prim[3] = g.tris[0].shade[0][3];
            shadeAlpha = false;
        }
    }
    if (shadeAlpha) {
        fog = false; // under G_FOG the RSP puts the fog factor in SHADE alpha
    }
    if (lit) {
        // SHADE is the N64's lighting now, so the material's 3DS color scale (combiner x2 times its diffuse color)
        // goes into a spare combiner cycle instead of the vertices.
        std::array<float, 3> e;
        for (int c = 0; c < 3; c++) {
            const float material = mat.vertexLighting ? std::max(m.diffuse[c], m.ambient[c]) / 255.0f : 1.0f;
            e[c] = mat.shadeScale[c] * (mat.shadeUsesVertex ? material : 1.0f);
        }
        const bool primFree = !UsesColorInput(c1, Gbi::APrim, Gbi::CPrim, Gbi::DPrim) &&
                              !UsesColorInput(c2, Gbi::APrim, Gbi::CPrim, Gbi::DPrim);
        const float emax = std::max({ e[0], e[1], e[2] });
        if (!mat.twoCycle && primFree && std::fabs(emax - 1.0f) > 0.02f) {
            if (emax <= 1.0f) {
                c2 = { Gbi::AComb, Gbi::B0, Gbi::CPrim, Gbi::D0, c2.aa, c2.ba, c2.ca, c2.da };
                for (int c = 0; c < 3; c++) {
                    prim[c] = static_cast<uint8_t>(std::clamp(e[c] * 255.0f + 0.5f, 0.0f, 255.0f));
                }
            } else {
                c2 = { Gbi::AComb, Gbi::B0, Gbi::CPrim, Gbi::DComb, c2.aa, c2.ba, c2.ca, c2.da };
                for (int c = 0; c < 3; c++) {
                    prim[c] = static_cast<uint8_t>(std::clamp((e[c] - 1.0f) * 255.0f + 0.5f, 0.0f, 255.0f));
                }
            }
        }
    }
    if (mat.additive && tex[0].on) {
        // The glow's alpha now lives in the texture: make sure the result's alpha takes it.
        bool usesTexAlpha = false;
        for (uint8_t x : { c1.aa, c1.ba, c1.ca, c1.da, c2.aa, c2.ba, c2.ca, c2.da }) {
            usesTexAlpha |= x == Gbi::AcTex0;
        }
        if (!usesTexAlpha) {
            if (!mat.twoCycle) {
                c2.aa = Gbi::AcComb;
                c2.ba = Gbi::Ac0;
                c2.ca = Gbi::AcTex0;
                c2.da = Gbi::Ac0;
            } else {
                warnings++;
            }
        }
    }
    // A greyscale texture (the tree's leaves) where the vanilla list takes the actor's color: multiply by it.
    bool tintFromActor = false;
    const bool lerpTint = mOpt.tintEnv && mOpt.tintPrim;
    if (mOpt.desaturate && (mOpt.tintEnv || mOpt.tintPrim) && tex[0].on && tex[0].img.Valid()) {
        for (size_t k = 0; k + 3 < tex[0].img.rgba.size(); k += 4) {
            uint8_t* px = &tex[0].img.rgba[k];
            const uint8_t l = static_cast<uint8_t>((px[0] * 77 + px[1] * 150 + px[2] * 29) >> 8);
            px[0] = px[1] = px[2] = l;
        }
        tex[0].path = mTextures.Add(tex[0].img, tex[0].lw, tex[0].lh);
    }
    if ((mOpt.tintEnv || mOpt.tintPrim) && tex[0].on && (!mat.twoCycle || lerpTint) && !mat.envTint) {
        bool grey = true;
        for (size_t k = 0; k + 3 < tex[0].img.rgba.size() && grey; k += 4) {
            const uint8_t* px = &tex[0].img.rgba[k];
            grey = std::abs(px[0] - px[1]) <= 2 && std::abs(px[1] - px[2]) <= 2;
        }
        if (grey) {
            tintFromActor = true;
            if (lerpTint) {
                // Both colors the actor's (the Flare Dancer's fire): ENV where the texture is dark, PRIM where bright.
                c2 = { Gbi::APrim, Gbi::BEnv, Gbi::CComb, Gbi::DEnv, c2.aa, c2.ba, c2.ca, c2.da };
            } else {
                const uint8_t in = mOpt.tintEnv ? Gbi::CEnv : Gbi::CPrim;
                c2 = { Gbi::AComb, Gbi::B0, in, Gbi::D0, c2.aa, c2.ba, c2.ca, c2.da };
            }
        }
    }
    // A static object's runtime tint is its actor's color, PRIM or ENV, as the vanilla list takes it (the foot shadows'
    // black PRIM, the Z-target arrow's category color); otherwise white, as for characters without a tint segment.
    const bool actorTint = mOpt.constantTints && !skin && (mOpt.tintPrim || mOpt.tintEnv);
    const bool actorTintPrim = actorTint && !mOpt.tintEnv;
    if (mat.envTint) {
        if (!mat.twoCycle) {
            // The runtime tint takes the second cycle (a lit material's color scale, ~1 for characters, gives way).
            c2 = { Gbi::AComb, Gbi::B0, actorTintPrim ? Gbi::CPrim : Gbi::CEnv, Gbi::D0, c2.aa, c2.ba, c2.ca, c2.da };
        } else {
            warnings++;
        }
    }
    bool whitePrim = false;
    if (mOpt.envFlash) {
        // (COMBINED - 0) * PRIM + ENV: the material's color, plus the actor's flash.
        whitePrim = c2.c != Gbi::CPrim;
        c2 = { Gbi::AComb, Gbi::B0, Gbi::CPrim, Gbi::DEnv, c2.aa, c2.ba, c2.ca, c2.da };
    }
    if (fog) {
        geo |= Gbi::Fog;
    }
    const uint32_t omH = Gbi::AdDisable | Gbi::CdMagicSq | Gbi::TcFilt | filter | Gbi::TpPersp | Gbi::Cyc2Cycle |
                         Gbi::PmNPrimitive;
    const uint32_t c1Blend = fog ? Gbi::Blend1(Gbi::BlClrFog, Gbi::BlAShade, Gbi::BlClrIn, Gbi::Bl1MA)
                                 : Gbi::Blend1(Gbi::BlClrIn, Gbi::BlA0, Gbi::BlClrIn, Gbi::Bl1);
    const uint32_t zmode = mat.decal ? Gbi::ZmodeDec : 0;
    uint32_t surf;
    if (mat.translucent) {
        surf = Gbi::AaEn | Gbi::ZCmp | Gbi::ImRd | Gbi::CvgDstWrap | Gbi::ClrOnCvg | Gbi::ForceBl |
               (mat.decal ? Gbi::ZmodeDec : Gbi::ZmodeXlu) |
               Gbi::Blend2(Gbi::BlClrIn, Gbi::BlAIn, Gbi::BlClrMem, Gbi::Bl1MA);
    } else if (mat.alphaTest) {
        surf = Gbi::AaEn | Gbi::ZCmp | Gbi::ZUpd | Gbi::ImRd | Gbi::CvgDstClamp | Gbi::CvgXAlpha | Gbi::AlphaCvgSel |
               zmode | Gbi::Blend2(Gbi::BlClrIn, Gbi::BlAIn, Gbi::BlClrMem, Gbi::BlAMem);
    } else {
        surf = Gbi::AaEn | Gbi::ZCmp | Gbi::ZUpd | Gbi::ImRd | Gbi::CvgDstClamp | Gbi::AlphaCvgSel | zmode |
               Gbi::Blend2(Gbi::BlClrIn, Gbi::BlAIn, Gbi::BlClrMem, Gbi::BlAMem);
    }

    // The actor fades the vanilla list through PRIM alpha (the spin attack's trail, as it ends): the material's alpha
    // takes it too, PRIM left to the actor, when the material's own colors do not need PRIM.
    bool primFade = false;
    if (mOpt.primAlphaFade && mat.translucent) {
        auto primColor = [](const Soh::CcCycle& c) {
            return c.a == Gbi::APrim || c.b == Gbi::BPrim || c.c == Gbi::CPrim || c.c == Gbi::CPrimAlpha ||
                   c.d == Gbi::DPrim;
        };
        auto primAlpha = [](const Soh::CcCycle& c) {
            return int(c.aa == Gbi::AcPrim) + int(c.ba == Gbi::AcPrim) + int(c.ca == Gbi::AcPrim) +
                   int(c.da == Gbi::AcPrim);
        };
        bool shadeAlphaOpaque = true;
        for (const MeshTri& p : g.tris) {
            for (int k = 0; k < 3; k++) {
                shadeAlphaOpaque &= p.shade[k][3] == 255;
            }
        }
        const bool actorPrim = tintFromActor && mOpt.tintPrim;
        const bool c2Times = c2.aa == Gbi::AcComb && c2.ba == Gbi::Ac0 && c2.da == Gbi::Ac0;
        if (actorPrim || (!primColor(c1) && !primColor(c2))) {
            if (primAlpha(c1) + primAlpha(c2) == 0) {
                if (c2.aa == Gbi::Ac0 && c2.ba == Gbi::Ac0 && c2.ca == Gbi::Ac0 && c2.da == Gbi::AcComb) {
                    c2.aa = Gbi::AcComb, c2.ca = Gbi::AcPrim, c2.da = Gbi::Ac0; // (COMBINED - 0) * PRIM + 0
                    primFade = true;
                } else if (c2Times && c2.ca == Gbi::AcShade && shadeAlphaOpaque) {
                    c2.ca = Gbi::AcPrim; // SHADE alpha is 1 throughout: PRIM's place
                    primFade = true;
                }
            } else if (primAlpha(c1) + primAlpha(c2) == 1 && prim[3] == 255 &&
                       ((c1.ca == Gbi::AcPrim && c1.ba != Gbi::AcPrim) || (c2.ca == Gbi::AcPrim && c2.ba != Gbi::AcPrim))) {
                // PRIM alpha only multiplies and the material's is 1 (or the constant vertex alpha went there above):
                // it is the actor's instead (the foot shadows' fade with height).
                primFade = true;
            }
        }
    }

    dl.PipeSync();
    dl.GeometryMode(0xFFFFFFFF, geo);
    dl.OtherMode(omH, Gbi::AcNone | c1Blend | surf);
    dl.Combine(c1, c2);
    if (!(tintFromActor && mOpt.tintPrim) && !primFade && !(mat.envTint && !mat.twoCycle && actorTintPrim)) {
        if (whitePrim) {
            dl.PrimColor(0, 0, 255, 255, 255, prim[3]);
        } else {
            dl.PrimColor(0, 0, prim[0], prim[1], prim[2], prim[3]);
        }
    }
    if (mOpt.envFlash) {
        // ENV is the actor's flash.
    } else if (tintFromActor && mOpt.tintEnv) {
        // ENV is the actor's.
    } else if (mat.envTint) {
        const uint32_t seg = segmentFor ? segmentFor(g) : 0;
        if (seg != 0) {
            dl.CallSegment(seg);
        } else if (actorTint && !mat.twoCycle) {
            // The actor's color (above): ENV stays the actor's; under PRIM, ENV is unused.
            if (actorTintPrim) {
                dl.EnvColor(255, 255, 255, mat.env[3]);
            }
            if (mLog) {
                mLog(LogLevel::Detail, "  actor tint " + cmb.name + "#" + std::to_string(int(&m - cmb.materials.data())) +
                                           (actorTintPrim ? " (PRIM)" : " (ENV)"));
            }
        } else {
            dl.EnvColor(255, 255, 255, mat.env[3]);
        }
    } else {
        dl.EnvColor(mat.env[0], mat.env[1], mat.env[2], mat.env[3]);
    }
    if (tex[0].on) {
        dl.Texture(0xFFFF, 0xFFFF, 0, 0, true);
        auto masks = [](uint32_t size, uint8_t mode) -> uint8_t {
            return mode == Gbi::TxClamp ? 0 : static_cast<uint8_t>(Log2(size));
        };
        HdTexture h0;
        h0.logicalWidth = tex[0].lw, h0.logicalHeight = tex[0].lh;
        dl.LoadTexture(tex[0].path, h0, 0, 0, tex[0].cms, tex[0].cmt, masks(tex[0].lw, tex[0].cms),
                       masks(tex[0].lh, tex[0].cmt), 0, 0);
        if (tex[1].on) {
            HdTexture h1;
            h1.logicalWidth = tex[1].lw, h1.logicalHeight = tex[1].lh;
            dl.LoadTexture(tex[1].path, h1, 1, 256, tex[1].cms, tex[1].cmt, masks(tex[1].lw, tex[1].cms),
                           masks(tex[1].lh, tex[1].cmt), static_cast<uint8_t>(shiftS), static_cast<uint8_t>(shiftT),
                           static_cast<uint32_t>(originS * 4.0f), static_cast<uint32_t>(originT * 4.0f));
        }
    } else {
        dl.Texture(0xFFFF, 0xFFFF, 0, 0, false);
    }
    // A scrolling material (CMAB translation): its tiles' sizes come from a list the engine rewrites every frame.
    if (const ScrollRate* sr = (scrollFor && tex[0].on) ? scrollFor(g) : nullptr) {
        ScrollEntry se;
        for (int i = 0; i < 2; i++) {
            const int slot = mat.texSlot[i];
            if (!tex[i].on || slot < 0 || slot > 1) {
                continue;
            }
            const CmbTexCoordinator& co = m.coordinators[m.CoordinatorForSlot(slot)];
            ScrollEntry::Tile t;
            t.tile = static_cast<uint8_t>(i);
            t.width = static_cast<uint16_t>(tex[i].lw);
            t.height = static_cast<uint16_t>(tex[i].lh);
            if (i == 1) {
                t.uls = static_cast<uint16_t>(std::max(0.0f, originS * 4.0f));
                t.ult = static_cast<uint16_t>(std::max(0.0f, originT * 4.0f));
            }
            // Sampled s = scale * (u - translate): a translation T moves the texture by scale * T repeats, which the
            // tile origin reproduces as scale * T * width texels (10.2). Clamped axes do not scroll.
            t.rateS = tex[i].cms == Gbi::TxClamp ? 0.0f : co.scaleS * sr->s[slot] * float(tex[i].lw) * 4.0f;
            t.rateT = tex[i].cmt == Gbi::TxClamp ? 0.0f : co.scaleT * sr->t[slot] * float(tex[i].lh) * 4.0f;
            if (t.rateS != 0.0f || t.rateT != 0.0f) {
                se.tiles.push_back(t);
            }
        }
        if (!se.tiles.empty()) {
            Soh::DisplayList sdl;
            for (const auto& t : se.tiles) {
                sdl.TileSize(t.tile, t.uls, t.ult, ((t.width - 1u) << 2) + t.uls, ((t.height - 1u) << 2) + t.ult);
            }
            sdl.End();
            static uint32_t sScrollCount = 0;
            se.path = "oot3d/scroll/" + std::to_string(sScrollCount++);
            mZip.Add(se.path, sdl.Resource());
            dl.Call(se.path);
            scrolls.push_back(std::move(se));
        }
    }

    // A reflection added on (N64Material::envSlot): the same triangles again with G_TEXTURE_GEN (the N64's own sphere
    // mapping, from the normals and the actor's look-at), the map blended over the base with its brightness as alpha
    // (as the additive glows), its coefficient per vertex in SHADE alpha and its hue in PRIM. Skins get the state list
    // and per-vertex alphas; the engine draws their batches again.
    Image env;
    std::array<double, 3> sum{ 0, 0, 0 };
    double smax = 0;
    // The coefficient at a vertex: the other textures sampled at its coordinates, the vertex color taken as 1.
    auto sample = [&](int i, const std::array<float, 2>& uv) -> std::array<float, 4> {
        const Image& im = tex[i].img;
        if (!tex[i].on || !im.Valid() || im.width == 0 || im.height == 0) {
            return { 1, 1, 1, 1 };
        }
        const float u = uv[0] - std::floor(uv[0]), v = uv[1] - std::floor(uv[1]);
        const uint32_t x = std::min(im.width - 1, static_cast<uint32_t>(u * float(im.width)));
        const uint32_t y = std::min(im.height - 1, static_cast<uint32_t>(v * float(im.height)));
        const uint8_t* px = &im.rgba[(size_t(y) * im.width + x) * 4];
        return { px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f, px[3] / 255.0f };
    };
    auto strength = [&](const MeshTri& p, int k) {
        std::array<float, 3> s{ 0, 0, 0 };
        for (const auto& [mono, c] : mat.envTerm.terms) {
            std::array<float, 3> f{ c[0], c[1], c[2] };
            for (uint8_t at : mono) {
                const bool color = at <= AT2, alpha = at >= AT0a && at <= AT2a;
                if (!color && !alpha) {
                    continue;
                }
                const int cmbSlot = color ? at : at - AT0a;
                const int i = mat.texSlot[0] == cmbSlot ? 0 : (mat.texSlot[1] == cmbSlot ? 1 : -1);
                if (i < 0) {
                    continue;
                }
                const auto t = sample(i, i == 0 ? p.uv0[k] : p.uv1[k]);
                for (int ch = 0; ch < 3; ch++) {
                    f[ch] *= color ? t[ch] : t[3];
                }
            }
            for (int ch = 0; ch < 3; ch++) {
                s[ch] += f[ch];
            }
        }
        // A blended material's reflection reaches the screen through its alpha too (the hearts' glass).
        if (mat.translucent && mat.usesShadeAlpha) {
            for (int ch = 0; ch < 3; ch++) {
                s[ch] *= p.shade[k][3] / 255.0f;
            }
        }
        return s;
    };
    auto envAlpha = [&](const MeshTri& p, int k) -> uint8_t {
        const auto s = strength(p, k);
        return static_cast<uint8_t>(std::clamp(std::max({ s[0], s[1], s[2] }) * 255.0f + 0.5f, 0.0f, 255.0f));
    };
    if (mOpt.envPass && mat.envSlot >= 0 && mat.envSlot < int(m.bindingCount) &&
        m.bindings[mat.envSlot].texture >= 0 && !g.tris.empty()) {
        try {
            env = cmb.DecodeTexture(m.bindings[mat.envSlot].texture);
        } catch (const std::exception&) {
            env = Image();
        }
        if (env.Valid()) {
            for (size_t k = 0; k + 3 < env.rgba.size(); k += 4) {
                uint8_t* px = &env.rgba[k];
                const int mx = std::max({ px[0], px[1], px[2] });
                for (int c = 0; c < 3; c++) {
                    px[c] = static_cast<uint8_t>(mx > 0 ? std::min(255, px[c] * 255 / mx) : 0);
                }
                px[3] = static_cast<uint8_t>(px[3] * mx / 255);
            }
            for (const MeshTri& p : g.tris) {
                for (int k = 0; k < 3; k++) {
                    const auto s = strength(p, k);
                    for (int ch = 0; ch < 3; ch++) {
                        sum[ch] += std::max(0.0f, s[ch]);
                    }
                }
            }
            smax = std::max({ sum[0], sum[1], sum[2] });
        }
    }
    const bool envOn = smax > 1e-6;
    // The reflection pass's state: G_LIGHTING (normals) and G_TEXTURE_GEN, the map blended over what is drawn.
    auto emitEnvState = [&](DisplayList& d) {
        uint32_t ew = 0, eh = 0;
        LogicalSize(env, 1.0f, 1.0f, ew, eh);
        const std::string envPath = mTextures.Add(env, ew, eh);
        const uint8_t ecms = WrapMode(m.bindings[mat.envSlot].wrapS);
        const uint8_t ecmt = WrapMode(m.bindings[mat.envSlot].wrapT);
        auto emask = [](uint32_t size, uint8_t mode) -> uint8_t {
            return mode == Gbi::TxClamp ? 0 : static_cast<uint8_t>(Log2(size));
        };
        uint32_t geoE = Gbi::ZBuffer | Gbi::Shade | Gbi::ShadingSmooth | Gbi::Lighting | Gbi::TextureGen;
        if (mat.cullMode == 1) {
            geoE |= Gbi::CullBack;
        } else if (mat.cullMode == 2) {
            geoE |= Gbi::CullFront;
        }
        d.PipeSync();
        d.GeometryMode(0xFFFFFFFF, geoE);
        d.OtherMode(Gbi::AdDisable | Gbi::CdMagicSq | Gbi::TcFilt | Gbi::TfBilerp | Gbi::TpPersp | Gbi::Cyc2Cycle |
                        Gbi::PmNPrimitive,
                    Gbi::AcNone | Gbi::Blend1(Gbi::BlClrIn, Gbi::BlA0, Gbi::BlClrIn, Gbi::Bl1) | Gbi::AaEn | Gbi::ZCmp |
                        Gbi::ImRd | Gbi::CvgDstWrap | Gbi::ClrOnCvg | Gbi::ForceBl | Gbi::ZmodeDec |
                        Gbi::Blend2(Gbi::BlClrIn, Gbi::BlAIn, Gbi::BlClrMem, Gbi::Bl1MA));
        d.Combine({ Gbi::ATex0, Gbi::B0, Gbi::CPrim, Gbi::D0, Gbi::AcTex0, Gbi::Ac0, Gbi::AcShade, Gbi::Ac0 },
                  { Gbi::A0, Gbi::B0, Gbi::C0, Gbi::DComb, Gbi::Ac0, Gbi::Ac0, Gbi::Ac0, Gbi::AcComb });
        d.PrimColor(0, 0, static_cast<uint8_t>(255.0 * sum[0] / smax + 0.5),
                    static_cast<uint8_t>(255.0 * sum[1] / smax + 0.5), static_cast<uint8_t>(255.0 * sum[2] / smax + 0.5),
                    255);
        // G_TEXTURE_GEN gives (n . lookat + 1) / 4 of the scale: 62 per texel spans the map (as the N64's own sphere
        // maps, gsSPTexture(0x07C0, 0x07C0) for 32x32).
        d.Texture(static_cast<uint16_t>(std::min<uint32_t>(0xFFFF, 62 * ew)),
                  static_cast<uint16_t>(std::min<uint32_t>(0xFFFF, 62 * eh)), 0, 0, true);
        HdTexture he;
        he.logicalWidth = ew, he.logicalHeight = eh;
        d.LoadTexture(envPath, he, 0, 0, ecms, ecmt, emask(ew, ecms), emask(eh, ecmt), 0, 0);
    };

    // Geometry, in batches of up to 32 vertices.
    const float lw = tex[0].on ? float(tex[0].lw) : 32.0f, lh = tex[0].on ? float(tex[0].lh) : 32.0f;
    const bool mirrorS = tex[0].on && tex[0].cms == Gbi::TxMirror, mirrorT = tex[0].on && tex[0].cmt == Gbi::TxMirror;
    const bool clampS = tex[0].on && tex[0].cms == Gbi::TxClamp, clampT = tex[0].on && tex[0].cmt == Gbi::TxClamp;
    // Coordinates may move by whole periods without changing what either texture samples.
    const float periodS = (mirrorS ? 2.0f : 1.0f) * (shiftS >= 1 && shiftS <= 10 ? float(1 << shiftS) : 1.0f);
    const float periodT = (mirrorT ? 2.0f : 1.0f) * (shiftT >= 1 && shiftT <= 10 ? float(1 << shiftT) : 1.0f);

    std::map<std::tuple<uint64_t, float, float>, uint8_t> slot;
    std::vector<Vtx> batch;
    std::vector<uint64_t> batchKeys;
    std::vector<uint8_t> batchEnvA; // skins: the reflection pass's alpha per vertex
    std::vector<std::array<uint8_t, 3>> batchTris;
    if (skin) {
        skin->lit = lit;
        skin->translucent = mat.translucent;
    }
    auto flush = [&]() {
        if (batch.empty()) {
            return;
        }
        if (skin) {
            // Skinned geometry: the engine fills positions (and normals) every frame; the batch keeps the rest.
            SkinBatch sb;
            for (size_t i = 0; i < batch.size(); i++) {
                sb.vertices.push_back({ batchKeys[i], batch[i], batchEnvA[i] });
            }
            sb.tris = batchTris;
            skin->batches.push_back(std::move(sb));
        } else {
            const uint32_t first = static_cast<uint32_t>(verts.size());
            verts.insert(verts.end(), batch.begin(), batch.end());
            dl.Vertices(vtxPath, first, static_cast<uint32_t>(batch.size()));
            for (size_t i = 0; i < batchTris.size(); i += 2) {
                const auto& a = batchTris[i];
                if (i + 1 < batchTris.size()) {
                    const auto& b = batchTris[i + 1];
                    dl.Tri2(a[0], a[1], a[2], b[0], b[1], b[2]);
                } else {
                    dl.Tri1(a[0], a[1], a[2]);
                }
            }
        }
        batch.clear();
        batchKeys.clear();
        batchEnvA.clear();
        batchTris.clear();
        slot.clear();
    };
    auto s16 = [](float v) { return static_cast<int16_t>(std::clamp(std::lround(v), -32768L, 32767L)); };
    for (const MeshTri& p : g.tris) {
        float offS = 0, offT = 0;
        if (tex[0].on) {
            const float minS = std::min({ p.uv0[0][0], p.uv0[1][0], p.uv0[2][0] });
            const float minT = std::min({ p.uv0[0][1], p.uv0[1][1], p.uv0[2][1] });
            if (!clampS) {
                offS = std::floor(minS / periodS) * periodS;
            }
            if (!clampT) {
                offT = std::floor(minT / periodT) * periodT;
            }
        }
        int need = 0;
        for (int k = 0; k < 3; k++) {
            if (!slot.count({ p.key[k], offS, offT })) {
                need++;
            }
        }
        if (batch.size() + need > 32) {
            flush();
        }
        uint8_t idx[3];
        for (int k = 0; k < 3; k++) {
            const auto key = std::make_tuple(p.key[k], offS, offT);
            auto it = slot.find(key);
            if (it != slot.end()) {
                idx[k] = it->second;
                continue;
            }
            Vtx v;
            v.x = s16(p.pos[k].x);
            v.y = s16(p.pos[k].y);
            v.z = s16(p.pos[k].z);
            v.s = s16(((p.uv0[k][0] - offS) * lw - half) * 32.0f);
            v.t = s16(((p.uv0[k][1] - offT) * lh - half) * 32.0f);
            if (lit) {
                const Vec3 n = p.normal[k].Normalized();
                v.r = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.x * 127.0f), -127L, 127L)));
                v.g = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.y * 127.0f), -127L, 127L)));
                v.b = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.z * 127.0f), -127L, 127L)));
            } else {
                v.r = p.shade[k][0];
                v.g = p.shade[k][1];
                v.b = p.shade[k][2];
            }
            v.a = p.shade[k][3];
            idx[k] = static_cast<uint8_t>(batch.size());
            slot.emplace(key, idx[k]);
            batch.push_back(v);
            batchKeys.push_back(p.key[k]);
            batchEnvA.push_back(envOn ? envAlpha(p, k) : 0);
        }
        batchTris.push_back({ idx[0], idx[1], idx[2] });
    }
    flush();

    if (envOn && skin) {
        DisplayList edl;
        emitEnvState(edl);
        edl.End();
        skin->envState = edl.Resource();
    } else if (envOn) {
        emitEnvState(dl);
        std::map<uint64_t, uint8_t> eslot;
        for (const MeshTri& p : g.tris) {
            int need = 0;
            for (int k = 0; k < 3; k++) {
                need += eslot.count(p.key[k]) ? 0 : 1;
            }
            if (batch.size() + need > 32) {
                flush();
                eslot.clear();
            }
            uint8_t idx[3];
            for (int k = 0; k < 3; k++) {
                if (auto it = eslot.find(p.key[k]); it != eslot.end()) {
                    idx[k] = it->second;
                    continue;
                }
                const Vec3 n = p.normal[k].Normalized();
                Vtx v;
                v.x = s16(p.pos[k].x);
                v.y = s16(p.pos[k].y);
                v.z = s16(p.pos[k].z);
                v.s = v.t = 0;
                v.r = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.x * 127.0f), -127L, 127L)));
                v.g = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.y * 127.0f), -127L, 127L)));
                v.b = static_cast<uint8_t>(static_cast<int8_t>(std::clamp(std::lround(n.z * 127.0f), -127L, 127L)));
                v.a = envAlpha(p, k);
                idx[k] = static_cast<uint8_t>(batch.size());
                eslot.emplace(p.key[k], idx[k]);
                batch.push_back(v);
                batchKeys.push_back(p.key[k]);
                batchEnvA.push_back(0);
            }
            batchTris.push_back({ idx[0], idx[1], idx[2] });
        }
        flush();
        // Texture generation stays off for whatever the actor draws next.
        dl.PipeSync();
        dl.GeometryMode(0xFFFFFFFF, geo);
    }
}

std::string MeshBuilder::Emit(size_t bucket, Part part, const std::string& path, const std::string& vtxPathIn) {
    if (bucket >= mBuckets.size()) {
        return "";
    }
    const MeshBucket& b = mBuckets[bucket];
    std::vector<const MeshGroup*> order;
    auto take = [&](const std::vector<MeshGroup>& list) {
        std::vector<const MeshGroup*> v;
        for (const MeshGroup& g : list) {
            v.push_back(&g);
        }
        std::stable_sort(v.begin(), v.end(), [](const MeshGroup* x, const MeshGroup* y) { return x->order < y->order; });
        order.insert(order.end(), v.begin(), v.end());
    };
    if (part != Part::Translucent) {
        take(b.opa);
    }
    if (part != Part::Opaque) {
        take(b.xlu);
    }
    if (order.empty()) {
        return "";
    }
    DisplayList dl;
    std::vector<Vtx> verts;
    const std::string vtxPath = vtxPathIn.empty() ? path + "_vtx" : vtxPathIn;
    for (const MeshGroup* g : order) {
        EmitGroup(dl, *g, verts, vtxPath);
    }
    dl.PipeSync();
    dl.End();
    mZip.Add(vtxPath, VertexArrayResource(verts));
    mZip.Add(path, dl.Resource());
    return path;
}

} // namespace Oot3d

namespace Oot3d {

std::vector<SkinGroup> MeshBuilder::EmitSkin(size_t bucket, const std::string& prefix) {
    std::vector<SkinGroup> out;
    if (bucket >= mBuckets.size()) {
        return out;
    }
    const MeshBucket& b = mBuckets[bucket];
    std::vector<const MeshGroup*> order;
    for (const auto* list : { &b.opa, &b.xlu }) {
        std::vector<const MeshGroup*> v;
        for (const MeshGroup& g : *list) {
            v.push_back(&g);
        }
        std::stable_sort(v.begin(), v.end(), [](const MeshGroup* x, const MeshGroup* y) { return x->order < y->order; });
        order.insert(order.end(), v.begin(), v.end());
    }
    for (const MeshGroup* g : order) {
        SkinGroup sg;
        sg.tag = g->tag;
        sg.material = g->cmb && g->src ? static_cast<int>(g->src - g->cmb->materials.data()) : -1;
        if (g->mat.envSlot >= 0 && mLog) {
            mLog(LogLevel::Detail, "  skin reflection " + prefix + " " + (g->cmb ? g->cmb->name : std::string("?")) + "#" +
                                       std::to_string(sg.material));
        }
        Soh::DisplayList dl;
        std::vector<Soh::Vtx> unused;
        EmitGroup(dl, *g, unused, "", &sg);
        dl.End();
        sg.statePath = prefix + "_g" + std::to_string(out.size());
        mZip.Add(sg.statePath, dl.Resource());
        if (!sg.envState.empty()) {
            sg.envStatePath = sg.statePath + "_env";
            mZip.Add(sg.envStatePath, sg.envState);
        }
        if (const SkinPalette* pal = paletteFor ? paletteFor(*g) : nullptr) {
            for (size_t k = 0; k < pal->images.size(); k++) {
                SkinGroup state; // the batches are the base group's; only the state list differs
                Soh::DisplayList pdl;
                EmitGroup(pdl, *g, unused, "", &state, &pal->images[k], pal->slot);
                pdl.End();
                const std::string path = sg.statePath + "_p" + std::to_string(k);
                mZip.Add(path, pdl.Resource());
                sg.palettePaths.push_back(path);
            }
        }
        out.push_back(std::move(sg));
    }
    return out;
}

} // namespace Oot3d
