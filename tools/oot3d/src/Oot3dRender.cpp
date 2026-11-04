// OoT3D asset converter: software renders for checking characters without the game.
//
// RenderModel draws a 3DS model straight from its CMB data, optionally posed by a CSAB frame: what the 3DS data itself
// describes. SkinRender draws a converted character package from a mod with the engine's own code
// (MergeShip3dsSkinCore.h): what the game draws. Both use the same view, framing and shading, so the two images
// compare directly.

#include "Oot3dAnim.h"
#include "Oot3dCmb.h"
#include "Oot3dExtract.h"
#include "Oot3dN64Texture.h"
#include "Oot3dRom.h"
#include "Oot3dSkeleton.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZar.h"
#include "Oot3dZip.h"

#include "MergeShip3dsSkinCore.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace Oot3d {

namespace {

struct RVert {
    Vec3 p, n;
    float u = 0, v = 0;
    std::array<float, 4> c{ 1, 1, 1, 1 };
};

struct RSurface {
    const Image* tex = nullptr;
    uint8_t wrapS = Soh::Gbi::TxWrap, wrapT = Soh::Gbi::TxWrap;
    int cull = 0; // 0 none, 1 back, 2 front
    bool alphaTest = false;
    bool shadeLit = true; // Lambert shading from the normals (else the vertex colors alone)
    bool blend = false;   // alpha blended over what is behind it (drawn after the opaque surfaces, no depth write)
    std::vector<RVert> verts;
    std::vector<std::array<uint32_t, 3>> tris;
};

float WrapCoord(float x, uint8_t mode) {
    if (mode == Soh::Gbi::TxClamp) {
        return std::clamp(x, 0.0f, 0.9999f);
    }
    if (mode == Soh::Gbi::TxMirror) {
        float f = x - 2.0f * std::floor(x * 0.5f);
        return f < 1.0f ? f : 2.0f - f - 1e-4f;
    }
    return x - std::floor(x);
}

// Orthographic view of the surfaces (already in view space: +y up, +z toward the viewer), framed to their bounds.
// `frame` (optional): {cx, cy, scale} instead of fitting the view to the surfaces.
Image RasterizeImage(const std::vector<RSurface>& surfaces, int size, const float* frame = nullptr) {
    Vec3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
    for (const auto& s : surfaces) {
        for (const auto& v : s.verts) {
            mn = { std::min(mn.x, v.p.x), std::min(mn.y, v.p.y), std::min(mn.z, v.p.z) };
            mx = { std::max(mx.x, v.p.x), std::max(mx.y, v.p.y), std::max(mx.z, v.p.z) };
        }
    }
    float scale = 0.9f * size / std::max(1.0f, std::max(mx.x - mn.x, mx.y - mn.y));
    float cx = (mn.x + mx.x) * 0.5f, cy = (mn.y + mx.y) * 0.5f;
    if (frame) {
        cx = frame[0];
        cy = frame[1];
        scale = frame[2];
    }
    Image img;
    img.width = img.height = static_cast<uint32_t>(size);
    img.rgba.assign(size_t(size) * size * 4, 0);
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
        img.rgba[i] = 90;
        img.rgba[i + 1] = 90;
        img.rgba[i + 2] = 110;
        img.rgba[i + 3] = 255;
    }
    std::vector<float> depth(size_t(size) * size, -1e30f);
    const Vec3 light = Vec3(0.3f, 0.6f, 0.75f).Normalized();
    std::vector<const RSurface*> order;
    for (const auto& s : surfaces) {
        if (!s.blend) {
            order.push_back(&s);
        }
    }
    for (const auto& s : surfaces) {
        if (s.blend) {
            order.push_back(&s);
        }
    }
    for (const RSurface* sp : order) {
        const RSurface& s = *sp;
        for (const auto& tri : s.tris) {
            const RVert* p[3] = { &s.verts[tri[0]], &s.verts[tri[1]], &s.verts[tri[2]] };
            float sx[3], sy[3];
            for (int k = 0; k < 3; k++) {
                sx[k] = (p[k]->p.x - cx) * scale + size * 0.5f;
                sy[k] = size * 0.5f - (p[k]->p.y - cy) * scale;
            }
            const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (std::fabs(area) < 1e-6f) {
                continue;
            }
            // Screen y points down, so counter-clockwise (front) faces come out with negative area.
            const bool front = area < 0;
            if ((s.cull == 1 && !front) || (s.cull == 2 && front)) {
                continue;
            }
            const int x0 = std::max(0, int(std::floor(std::min({ sx[0], sx[1], sx[2] }))));
            const int x1 = std::min(size - 1, int(std::ceil(std::max({ sx[0], sx[1], sx[2] }))));
            const int y0 = std::max(0, int(std::floor(std::min({ sy[0], sy[1], sy[2] }))));
            const int y1 = std::min(size - 1, int(std::ceil(std::max({ sy[0], sy[1], sy[2] }))));
            for (int y = y0; y <= y1; y++) {
                for (int x = x0; x <= x1; x++) {
                    const float px = x + 0.5f, py = y + 0.5f;
                    const float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) / area;
                    const float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) / area;
                    const float w2 = 1 - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) {
                        continue;
                    }
                    const float z = w0 * p[0]->p.z + w1 * p[1]->p.z + w2 * p[2]->p.z;
                    float& d = depth[size_t(y) * size + x];
                    if (z <= d) {
                        continue;
                    }
                    float col[4] = { 1, 1, 1, 1 };
                    if (s.tex) {
                        // Bilinear, as both the 3DS and SoH filter (texel centers at half-texel offsets).
                        const float u = (w0 * p[0]->u + w1 * p[1]->u + w2 * p[2]->u) * s.tex->width - 0.5f;
                        const float v = (w0 * p[0]->v + w1 * p[1]->v + w2 * p[2]->v) * s.tex->height - 0.5f;
                        const float fu = std::floor(u), fv = std::floor(v);
                        const float au = u - fu, av = v - fv;
                        for (int c = 0; c < 4; c++) {
                            col[c] = 0;
                        }
                        for (int k = 0; k < 4; k++) {
                            const float tu = (fu + (k & 1) + 0.5f) / s.tex->width;
                            const float tv = (fv + (k >> 1) + 0.5f) / s.tex->height;
                            const uint32_t xi = std::min(s.tex->width - 1, uint32_t(WrapCoord(tu, s.wrapS) * s.tex->width));
                            const uint32_t yi =
                                std::min(s.tex->height - 1, uint32_t(WrapCoord(tv, s.wrapT) * s.tex->height));
                            const float wt = ((k & 1) ? au : 1 - au) * ((k >> 1) ? av : 1 - av);
                            const uint8_t* texel = s.tex->Px(xi, yi);
                            for (int c = 0; c < 4; c++) {
                                col[c] += wt * texel[c] / 255.0f;
                            }
                        }
                    }
                    if (s.alphaTest && col[3] < 0.5f) {
                        continue;
                    }
                    float lit = 1.0f;
                    if (s.shadeLit) {
                        const Vec3 n = (p[0]->n * w0 + p[1]->n * w1 + p[2]->n * w2).Normalized();
                        lit = 0.45f + 0.55f * std::max(0.0f, n.Dot(light));
                    }
                    uint8_t* o = img.Px(x, y);
                    const float va = w0 * p[0]->c[3] + w1 * p[1]->c[3] + w2 * p[2]->c[3];
                    const float a = s.blend ? std::clamp(col[3] * va, 0.0f, 1.0f) : 1.0f;
                    if (!s.blend) {
                        d = z;
                    }
                    for (int c = 0; c < 3; c++) {
                        const float vc = w0 * p[0]->c[c] + w1 * p[1]->c[c] + w2 * p[2]->c[c];
                        const float v = std::clamp(col[c] * vc * lit * 255.0f, 0.0f, 255.0f);
                        o[c] = static_cast<uint8_t>(v * a + o[c] * (1.0f - a));
                    }
                }
            }
        }
    }
    return img;
}

void WritePng(const Image& img, const std::string& outPng) {
    const Bytes png = EncodePng(img);
    std::ofstream(U8Path(outPng), std::ios::binary)
        .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

void Rasterize(const std::vector<RSurface>& surfaces, int size, const std::string& outPng) {
    WritePng(RasterizeImage(surfaces, size), outPng);
}

Mat34 ViewMatrix(float yawDeg, float pitchDeg) {
    const float d2r = 3.14159265f / 180.0f;
    return Mat34::FromSRT(Vec3(1, 1, 1), Vec3(pitchDeg * d2r, 0, 0), Vec3(0, 0, 0)) *
           Mat34::FromSRT(Vec3(1, 1, 1), Vec3(0, yawDeg * d2r, 0), Vec3(0, 0, 0));
}

std::optional<Bytes> ReadResource(const ZipReader& zip, const std::string& path) {
    if (auto b = zip.Read("alt/" + path)) {
        return b;
    }
    return zip.Read(path);
}

} // namespace

void RenderModel(const RomFS& rom, const std::string& zarPath, const std::string& model, const std::string& anim,
                 float frame, float yawDeg, float pitchDeg, const std::string& outPng, int size,
                 const std::function<void(const std::string&)>& print) {
    Zar zar = Zar::Parse(rom.Read(zarPath));
    const ZarFile* mf = zar.Find(model);
    if (!mf) {
        print("model not found");
        return;
    }
    Cmb cmb = Cmb::Parse(mf->data);
    const auto bind = cmb.BindPose();
    std::vector<Mat34> inv;
    for (const auto& w : bind) {
        inv.push_back(w.Inverse());
    }
    std::vector<Mat34> world = bind;
    if (!anim.empty() && anim != "-") {
        if (const ZarFile* af = zar.Find(anim)) {
            world = CsabPose(cmb, Csab::Parse(af->data), frame);
        } else {
            print("animation not found: " + anim);
        }
    }
    std::vector<Image> tex(cmb.textures.size());
    for (size_t i = 0; i < tex.size(); i++) {
        try {
            tex[i] = cmb.DecodeTexture(i);
        } catch (const std::exception&) {
        }
    }
    const Mat34 view = ViewMatrix(yawDeg, pitchDeg);
    std::vector<RSurface> surfaces;
    // OOT3D_RENDER_MESH=<n>: only that CMB mesh (to look at a model's variants one by one).
    const char* onlyMesh = getenv("OOT3D_RENDER_MESH");
    for (const auto& set : cmb.Decode()) {
        if (onlyMesh && *onlyMesh && set.mesh != atoi(onlyMesh)) {
            continue;
        }
        const CmbMaterial& m = cmb.materials[set.material];
        const CmbTexCoordinator& co = m.coordinators[0];
        RSurface s;
        const int ti = m.bindings[0].texture;
        if (ti >= 0 && size_t(ti) < tex.size() && tex[ti].Valid()) {
            s.tex = &tex[ti];
        }
        s.cull = m.cullMode;
        s.alphaTest = m.alphaTest;
        for (const auto& v : set.vertices) {
            RVert o;
            o.p = view.Point(SkinPosition(v, set.skinning, world, inv));
            o.n = view.Dir(SkinNormal(v, set.skinning, world, inv)).Normalized();
            const auto& uv = v.uv[std::min<int>(co.source, 2)];
            co.Apply(uv[0], uv[1], o.u, o.v);
            if (set.hasVertexColors) {
                o.c = v.color;
            }
            s.verts.push_back(o);
        }
        for (const auto& t : set.triangles) {
            s.tris.push_back({ t[0], t[1], t[2] });
        }
        surfaces.push_back(std::move(s));
    }
    Rasterize(surfaces, size, outPng);
    print("wrote " + outPng);
}

// A converted character package's surfaces, skinned by the engine's code for a pose (`showGroup`: which groups).
void CollectSkin(const ZipReader& zip, const ms3ds::Skin& skin, const std::vector<ms3ds::Pose>& pose, const Mat34& view,
                 std::vector<RSurface>& surfaces, std::map<std::string, Image>& textures,
                 const std::function<bool(const ms3ds::Group&)>& showGroup) {
    std::vector<float> pos, nrm;
    ms3ds::SkinVertices(skin, pose, pos, nrm);
    for (const ms3ds::Group& g : skin.groups) {
        if (showGroup && !showGroup(g)) {
            continue;
        }
        // The group's state list: its texture (tile 0), wrap modes, logical size and cull mode.
        RSurface s;
        uint32_t lw = 32, lh = 32;
        bool bilerp = true;
        if (auto dl = zip.Read(g.statePath)) {
            const View r(*dl);
            size_t off = 64 + 8;
            while (off + 8 <= r.Size()) {
                const uint32_t w0 = r.U32(off), w1 = r.U32(off + 4);
                off += 8;
                const uint8_t op = static_cast<uint8_t>(w0 >> 24);
                if (op == 0x20 || op == 0x31 || op == 0x32) {
                    if (off + 8 > r.Size()) {
                        break;
                    }
                    const uint64_t hash = (uint64_t(r.U32(off)) << 32) | r.U32(off + 4);
                    off += 8;
                    const std::string* name = zip.NameOfHash(hash);
                    if (op == 0x20 && name && !s.tex) {
                        auto it = textures.find(*name);
                        if (it == textures.end()) {
                            Image img;
                            if (auto t = zip.Read(*name); t && t->size() > 64 + 28) {
                                const View tv(*t);
                                img.width = tv.U32(64 + 4);
                                img.height = tv.U32(64 + 8);
                                const uint32_t n = tv.U32(64 + 24);
                                if (n == img.width * img.height * 4 && t->size() >= 64 + 28 + size_t(n)) {
                                    img.rgba.assign(t->begin() + 64 + 28, t->begin() + 64 + 28 + n);
                                }
                            }
                            it = textures.emplace(*name, std::move(img)).first;
                        }
                        s.tex = it->second.Valid() ? &it->second : nullptr;
                    }
                } else if (op == 0xF5 && ((w1 >> 24) & 7) == 0) {
                    s.wrapS = (w1 >> 8) & 3;
                    s.wrapT = (w1 >> 18) & 3;
                } else if (op == 0xF2 && ((w1 >> 24) & 7) == 0) {
                    lw = (((w1 >> 12) & 0xFFF) - ((w0 >> 12) & 0xFFF)) / 4 + 1;
                    lh = ((w1 & 0xFFF) - (w0 & 0xFFF)) / 4 + 1;
                } else if (op == 0xEF) {
                    bilerp = ((w0 >> 12) & 3) != 0; // G_MDSFT_TEXTFILT: G_TF_POINT is 0
                } else if (op == 0xD9) {
                    s.cull = (w1 & Soh::Gbi::CullBack) ? 1 : (w1 & Soh::Gbi::CullFront) ? 2 : 0;
                } else if (op == 0xDF) {
                    break;
                }
            }
        }
        s.shadeLit = true;
        s.blend = g.translucent;
        for (const ms3ds::Batch& b : g.batches) {
            const uint32_t base = static_cast<uint32_t>(s.verts.size());
            for (const ms3ds::BatchVertex& bv : b.vertices) {
                RVert o;
                // The engine rounds positions to the N64's integer vertex coordinates.
                const float* p = &pos[size_t(bv.vertex) * 3];
                o.p = view.Point(Vec3(std::round(p[0]), std::round(p[1]), std::round(p[2])));
                const float* n = &nrm[size_t(bv.vertex) * 3];
                o.n = view.Dir(Vec3(n[0], n[1], n[2])).Normalized();
                // SoH's renderer adds half a (logical) texel under bilinear filtering, as the N64 samples.
                o.u = (bv.s / 32.0f + (bilerp ? 0.5f : 0.0f)) / lw;
                o.v = (bv.t / 32.0f + (bilerp ? 0.5f : 0.0f)) / lh;
                if (!g.lit) {
                    for (int c = 0; c < 4; c++) {
                        o.c[c] = bv.rgba[c] / 255.0f;
                    }
                }
                s.verts.push_back(o);
            }
            for (size_t t = 0; t + 2 < b.tris.size(); t += 3) {
                s.tris.push_back({ base + b.tris[t], base + b.tris[t + 1], base + b.tris[t + 2] });
            }
        }
        surfaces.push_back(std::move(s));
    }
}

void SkinRender(const std::string& modPath, const std::string& skeleton, const std::string& n64Anim, float n64Frame,
                float yawDeg, float pitchDeg, const std::string& outPng, int size,
                const std::function<void(const std::string&)>& print) {
    auto zip = ZipReader::Open(modPath);
    const auto blob = ReadResource(*zip, "oot3d/skin/" + skeleton);
    if (!blob || blob->size() < 68) {
        print("no package for " + skeleton);
        return;
    }
    auto skin = ms3ds::Parse(blob->data() + 68, blob->size() - 68);
    if (!skin) {
        print("damaged package");
        return;
    }
    std::vector<ms3ds::Pose> pose;
    if (n64Anim.empty() || n64Anim == "-") {
        ms3ds::BindPose(*skin, pose);
    } else {
        auto it = skin->animByPath.find(n64Anim);
        if (it == skin->animByPath.end()) {
            print("animation not in the package; it has:");
            for (const auto& a : skin->anims) {
                print("  " + a.n64Path);
            }
            return;
        }
        ms3ds::SamplePose(*skin, skin->anims[it->second], n64Frame, pose);
        char line[160];
        snprintf(line, sizeof(line), "N64 frame %.1f -> 3DS frame %.2f (%d / %.0f frames)", n64Frame,
                 ms3ds::AnimFrame(skin->anims[it->second], n64Frame), skin->anims[it->second].n64Frames,
                 skin->anims[it->second].frames);
        print(line);
    }
    const Mat34 view = ViewMatrix(yawDeg, pitchDeg);
    std::map<std::string, Image> textures;
    std::vector<RSurface> surfaces;
    CollectSkin(*zip, *skin, pose, view, surfaces, textures, nullptr);
    Rasterize(surfaces, size, outPng);
    print("wrote " + outPng + " (" + std::to_string(skin->vertices.size()) + " vertices, " +
          std::to_string(skin->groups.size()) + " groups, " + std::to_string(skin->anims.size()) + " animations)");
}

// A vanilla display list's surfaces (its called lists too), its vertices placed by `xf` (view * limb matrix).
void CollectVanillaDl(const ZipReader& oot, const std::string& dlPath, const Mat34& xf, std::vector<RSurface>& surfaces,
                      std::map<std::string, Image>& cache) {
    struct V {
        Vec3 p;
        float s = 0, t = 0;
        std::array<float, 4> c{ 1, 1, 1, 1 };
    };
    V buf[64];
    std::string img, tex, tlut;
    uint32_t tileW = 32, tileH = 32;
    uint8_t cms = 0, cmt = 0;
    bool lit = false;
    size_t cur = SIZE_MAX; // index of the surface for the current texture state
    // A new surface whenever the texture state changes.
    auto surfaceFor = [&]() -> size_t {
        RSurface s;
        s.wrapS = (cms & 2) ? Soh::Gbi::TxClamp : ((cms & 1) ? Soh::Gbi::TxMirror : Soh::Gbi::TxWrap);
        s.wrapT = (cmt & 2) ? Soh::Gbi::TxClamp : ((cmt & 1) ? Soh::Gbi::TxMirror : Soh::Gbi::TxWrap);
        s.alphaTest = true;
        if (!tex.empty()) {
            const std::string key = tex + "|" + tlut;
            auto it = cache.find(key);
            if (it == cache.end()) {
                auto t = ReadVanillaTexture(oot, tex, tlut);
                it = cache.emplace(key, t ? t->image : Image{}).first;
            }
            s.tex = it->second.Valid() ? &it->second : nullptr;
        }
        surfaces.push_back(std::move(s));
        return surfaces.size() - 1;
    };
    auto tri = [&](int a, int b, int c) {
        if (cur == SIZE_MAX) {
            cur = surfaceFor();
        }
        RSurface& sf = surfaces[cur];
        const uint32_t base = static_cast<uint32_t>(sf.verts.size());
        for (int i : { a, b, c }) {
            const V& v = buf[i & 63];
            RVert o;
            o.p = xf.Point(v.p);
            o.u = v.s / 32.0f / float(std::max(1u, tileW));
            o.v = v.t / 32.0f / float(std::max(1u, tileH));
            o.c = lit ? std::array<float, 4>{ 1, 1, 1, 1 } : v.c;
            sf.verts.push_back(o);
        }
        sf.tris.push_back({ base, base + 1, base + 2 });
    };
    std::function<void(const std::string&, int)> walk = [&](const std::string& path, int depth) {
        if (depth > 8) {
            return;
        }
        auto data = oot.Read(path);
        if (!data || data->size() < 72) {
            return;
        }
        const View d(*data);
        size_t off = 64 + 8;
        while (off + 8 <= d.Size()) {
            const uint32_t w0 = d.U32(off), w1 = d.U32(off + 4);
            const uint8_t op = static_cast<uint8_t>(w0 >> 24);
            const bool expanded = op == 0x20 || op == 0x31 || op == 0x32 || op == 0x33 || op == 0x35 || op == 0x36 ||
                                  op == 0x42;
            uint64_t hash = 0;
            if (expanded && off + 16 <= d.Size()) {
                hash = (uint64_t(d.U32(off + 8)) << 32) | d.U32(off + 12);
            }
            const std::string* name = expanded ? oot.NameOfHash(hash) : nullptr;
            switch (op) {
                case 0x32: { // G_VTX_OTR_HASH
                    const uint32_t n = (w0 >> 12) & 0xFF, dest = ((w0 >> 1) & 0x7F) - n;
                    if (name) {
                        if (auto arr = oot.Read(*name); arr && arr->size() >= 72) {
                            const View a(*arr);
                            const uint32_t total = a.U32(68), first = w1 / 16;
                            for (uint32_t k = 0; k < n && first + k < total && dest + k < 64; k++) {
                                const size_t o = 72 + size_t(first + k) * 16;
                                V& v = buf[dest + k];
                                v.p = Vec3(a.S16(o), a.S16(o + 2), a.S16(o + 4));
                                v.s = a.S16(o + 8);
                                v.t = a.S16(o + 10);
                                for (int c = 0; c < 4; c++) {
                                    v.c[c] = a.U8(o + 12 + c) / 255.0f;
                                }
                            }
                        }
                    }
                    break;
                }
                case 0x05:
                    tri((w0 >> 17) & 0x7F, (w0 >> 9) & 0x7F, (w0 >> 1) & 0x7F);
                    break;
                case 0x06:
                    tri((w0 >> 17) & 0x7F, (w0 >> 9) & 0x7F, (w0 >> 1) & 0x7F);
                    tri((w1 >> 17) & 0x7F, (w1 >> 9) & 0x7F, (w1 >> 1) & 0x7F);
                    break;
                case 0x07:
                    tri((w0 >> 17) & 0x7F, (w0 >> 9) & 0x7F, (w0 >> 1) & 0x7F);
                    tri((w0 >> 17) & 0x7F, (w0 >> 1) & 0x7F, (w1 >> 1) & 0x7F);
                    break;
                case 0x20: // SETTIMG (hash)
                    img = name ? *name : "";
                    break;
                case 0xF0: // LOADTLUT
                    tlut = img;
                    cur = SIZE_MAX;
                    break;
                case 0xF3: // LOADBLOCK
                case 0xF4: // LOADTILE
                    tex = img;
                    cur = SIZE_MAX;
                    break;
                case 0xF5: // SETTILE
                    if (((w1 >> 24) & 7) == 0) {
                        cms = (w1 >> 8) & 3;
                        cmt = (w1 >> 18) & 3;
                        cur = SIZE_MAX;
                    }
                    break;
                case 0xF2: // SETTILESIZE
                    if (((w1 >> 24) & 7) == 0) {
                        tileW = ((((w1 >> 12) & 0xFFF) - ((w0 >> 12) & 0xFFF)) >> 2) + 1;
                        tileH = (((w1 & 0xFFF) - (w0 & 0xFFF)) >> 2) + 1;
                        cur = SIZE_MAX;
                    }
                    break;
                case 0xD9: // geometry mode
                    lit = (lit && (w0 & 0x00020000)) || (w1 & 0x00020000);
                    break;
                case 0x31:
                    if (name) {
                        walk(*name, depth + 1);
                    }
                    break;
                default:
                    break;
            }
            if (op == 0xDF) {
                break;
            }
            off += expanded ? 16 : 8;
        }
    };
    walk(dlPath, 0);
}

void RenderVanillaDl(const std::string& ootPath, const std::string& dlPath, float yawDeg, float pitchDeg,
                     const std::string& outPng, int size, const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    std::map<std::string, Image> cache;
    std::vector<RSurface> surfaces;
    CollectVanillaDl(*oot, dlPath, ViewMatrix(yawDeg, pitchDeg), surfaces, cache);
    Rasterize(surfaces, size, outPng);
    print("wrote " + outPng);
}

// A converted room seen from a camera (perspective, 60 degree field of view): every mesh entry's lists whose path
// starts with `prefix` (e.g. "oot3d/shared/spot00/room0_"), opaque and translucent.
void RenderRoom(const std::string& modPath, const std::string& prefix, const Vec3& eye, const Vec3& target,
                const std::string& outPng, int size, const std::function<void(const std::string&)>& print) {
    auto zip = ZipReader::Open(modPath);
    // Look-at view: camera at the origin looking down -z.
    const Vec3 f = (target - eye).Normalized();
    Vec3 r = Vec3(f.z, 0, -f.x).Normalized(); // right = f x up (up = +y), pointing right on screen
    r = Vec3(-r.x, -r.y, -r.z);
    const Vec3 u(r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x);
    Mat34 view = Mat34::Identity();
    const Vec3 rows[3] = { r, u, Vec3(-f.x, -f.y, -f.z) };
    for (int i = 0; i < 3; i++) {
        view.m[i][0] = rows[i].x;
        view.m[i][1] = rows[i].y;
        view.m[i][2] = rows[i].z;
        view.m[i][3] = -(rows[i].x * eye.x + rows[i].y * eye.y + rows[i].z * eye.z);
    }
    std::map<std::string, Image> cache;
    std::vector<RSurface> surfaces;
    int lists = 0;
    for (const std::string& n : zip->Names()) {
        if (StartsWith(n, prefix) && !EndsWith(n, "_vtx")) {
            CollectVanillaDl(*zip, n, view, surfaces, cache);
            lists++;
        }
    }
    // Perspective: x/(-z), y/(-z); triangles reaching behind the camera are dropped.
    const float focal = 1.0f / std::tan(30.0f * 3.14159265f / 180.0f);
    for (RSurface& sf : surfaces) {
        std::vector<std::array<uint32_t, 3>> keep;
        for (const auto& t : sf.tris) {
            bool ok = true;
            for (uint32_t k : t) {
                ok &= sf.verts[k].p.z < -20.0f;
            }
            if (ok) {
                keep.push_back(t);
            }
        }
        sf.tris = std::move(keep);
        for (RVert& v : sf.verts) {
            const float z = std::min(v.p.z, -1.0f);
            v.p = Vec3(focal * v.p.x / -z, focal * v.p.y / -z, z);
        }
        sf.shadeLit = false;
    }
    const float frame[3] = { 0.0f, 0.0f, size * 0.5f };
    WritePng(RasterizeImage(surfaces, size, frame), outPng);
    print("wrote " + outPng + " (" + std::to_string(lists) + " lists)");
}

void RenderAudit(const std::string& ootPath, const std::string& modPath, const std::string& outDir, int size,
                 const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    auto mod = ZipReader::Open(modPath);
    std::filesystem::create_directories(U8Path(outDir));
    const Mat34 view = ViewMatrix(0, 0);
    int count = 0;
    for (const std::string& name : mod->Names()) {
        const std::string prefix = "alt/oot3d/skin/objects/";
        if (!StartsWith(name, prefix)) {
            continue;
        }
        const std::string skelPath = name.substr(std::string("alt/oot3d/skin/").size());
        auto blob = mod->Read(name);
        if (!blob || blob->size() < 68) {
            continue;
        }
        auto skin = ms3ds::Parse(blob->data() + 68, blob->size() - 68);
        auto skel = ReadVanillaSkeleton(*oot, skelPath);
        if (!skin || !skel || skin->anims.empty()) {
            print("skipped " + skelPath);
            continue;
        }
        // A representative pose: an idle of the character's own object (not the credits' or a cutscene bank's copy).
        size_t pick = 0;
        int pickRank = -1;
        const std::string objectDir = skelPath.substr(0, skelPath.find_last_of('/') + 1);
        for (size_t a = 0; a < skin->anims.size(); a++) {
            const std::string p = ToLower(skin->anims[a].n64Path);
            int rank = 0;
            if (StartsWith(skin->anims[a].n64Path, objectDir)) {
                rank += 4;
            }
            if (p.find("object_ec/") != std::string::npos || p.find("demo") != std::string::npos ||
                p.find("cutscene") != std::string::npos) {
                rank -= 8;
            }
            for (const char* k : { "idle", "wait", "stand", "float", "fly", "matsu", "neutral" }) {
                if (p.find(k) != std::string::npos) {
                    rank += 2;
                    break;
                }
            }
            if (rank > pickRank) {
                pickRank = rank;
                pick = a;
            }
        }
        const ms3ds::Anim& an = skin->anims[pick];
        print("audit " + skelPath + " posed by " + an.n64Path);
        auto va = ReadVanillaAnimation(*oot, an.n64Path);
        // N64: the skeleton's own limb lists, posed by the animation's first frame.
        std::map<std::string, Image> cache;
        std::vector<RSurface> n64;
        if (va) {
            const std::vector<Mat34> pose = N64Pose(*skel, *va, 0);
            for (size_t l = 0; l < skel->limbs.size() && l < pose.size(); l++) {
                if (!skel->limbs[l].dList.empty()) {
                    CollectVanillaDl(*oot, skel->limbs[l].dList, view * pose[l], n64, cache);
                }
            }
        }
        // 3DS: the package posed by the paired clip at the same frame, with the variants of the limbs' own lists.
        std::vector<ms3ds::Pose> pose3;
        ms3ds::SamplePose(*skin, an, 0.0f, pose3);
        std::map<std::string, Image> textures;
        std::vector<RSurface> ds;
        CollectSkin(*mod, *skin, pose3, view, ds, textures, [&](const ms3ds::Group& g) {
            if (g.limb == -1) {
                return true;
            }
            return g.limb >= 0 && size_t(g.limb) < skel->limbs.size() && g.list == skel->limbs[g.limb].dList;
        });
        std::string file = skelPath.substr(8);
        std::replace(file.begin(), file.end(), '/', '~');
        WritePng(RasterizeImage(n64, size), outDir + "/" + file + "~n64.png");
        WritePng(RasterizeImage(ds, size), outDir + "/" + file + "~3ds.png");
        count++;
    }
    print(std::to_string(count) + " characters rendered to " + outDir);

    // Objects: each replaced vanilla list next to its replacement.
    int objects = 0;
    for (const std::string& name : mod->Names()) {
        if (!StartsWith(name, "alt/objects/") || EndsWith(name, "_vtx")) {
            continue;
        }
        const std::string path = name.substr(4);
        auto data = mod->Read(name);
        auto h = data ? ParseResourceHeader(*data) : std::nullopt;
        if (!h || h->type != Soh::kTypeDisplayList) {
            continue;
        }
        std::map<std::string, Image> c1, c2;
        std::vector<RSurface> n64, ds;
        CollectVanillaDl(*oot, path, view, n64, c1);
        CollectVanillaDl(*mod, name, view, ds, c2);
        if (ds.empty()) {
            continue; // emptied (a part of a replaced model)
        }
        std::string file = path.substr(8);
        std::replace(file.begin(), file.end(), '/', '~');
        WritePng(RasterizeImage(n64, size), outDir + "/obj~" + file + "~n64.png");
        WritePng(RasterizeImage(ds, size), outDir + "/obj~" + file + "~3ds.png");
        objects++;
    }
    print(std::to_string(objects) + " objects rendered to " + outDir);
}

} // namespace Oot3d
