#include "Oot3dCmb.h"

#include <cstdlib>

#include <functional>
#include <map>
#include <unordered_map>

namespace Oot3d {

static constexpr float kPi = 3.14159265358979323846f;

void CmbTexCoordinator::Apply(float u, float v, float& s, float& t) const {
    const float theta = rotation * kPi;
    const float sinR = std::sin(theta), cosR = std::cos(theta);
    const float m0 = scaleS * cosR, m1 = scaleT * sinR, m4 = scaleS * -sinR, m5 = scaleT * cosR;
    const float m12 = scaleS * ((0.5f * sinR - 0.5f * cosR) + 0.5f - translateS);
    const float m13 = scaleT * ((0.5f * -sinR - 0.5f * cosR) + 0.5f - translateT);
    s = m0 * u + m4 * v + m12;
    t = m1 * u + m5 * v + m13;
}

bool CmbTexCoordinator::IsIdentity() const {
    return scaleS == 1 && scaleT == 1 && translateS == 0 && translateT == 0 && rotation == 0;
}

int CmbMaterial::CoordinatorForSlot(int slot) const {
    // OoT3D has no explicit texCoordConfig; noclip infers config 0110 (texture 2 reuses coordinator 1) when there are
    // more texture bindings than coordinators.
    if (slot == 2 && bindingCount > coordinatorCount) {
        return 1;
    }
    return slot;
}

static Rgba8 ReadRgba(View v, size_t off) {
    return { v.U8(off), v.U8(off + 1), v.U8(off + 2), v.U8(off + 3) };
}

static size_t TypeSize(uint16_t type) {
    switch (type) {
        case 0x1400:
        case 0x1401: return 1;
        case 0x1402:
        case 0x1403: return 2;
        case 0x1404:
        case 0x1405:
        case 0x1406: return 4;
        default: throw FormatError("bad CMB vertex data type");
    }
}

static float ReadComponent(View v, size_t off, uint16_t type) {
    switch (type) {
        case 0x1400: return v.S8(off);
        case 0x1401: return v.U8(off);
        case 0x1402: return v.S16(off);
        case 0x1403: return v.U16(off);
        case 0x1404: return static_cast<float>(v.S32(off));
        case 0x1405: return static_cast<float>(v.U32(off));
        case 0x1406: return v.F32(off);
        default: throw FormatError("bad CMB vertex data type");
    }
}

static CmbAttrib ReadAttrib(View v, size_t off) {
    CmbAttrib a;
    a.start = v.U32(off);
    a.scale = v.F32(off + 4);
    a.type = v.U16(off + 8);
    a.mode = v.U16(off + 10);
    for (int i = 0; i < 4; i++) {
        a.constant[i] = v.F32(off + 12 + i * 4);
    }
    return a;
}

// Reads `n` components of vertex `index` of an attribute; constant attributes ignore the index.
static void ReadVector(const CmbAttrib& a, View slice, uint32_t index, int n, float* out) {
    if (a.mode == 1) {
        for (int i = 0; i < n; i++) {
            out[i] = a.constant[i];
        }
        return;
    }
    const size_t ts = TypeSize(a.type);
    const size_t base = a.start + size_t(index) * ts * n;
    for (int i = 0; i < n; i++) {
        out[i] = ReadComponent(slice, base + i * ts, a.type) * a.scale;
    }
}

static void ParseMaterials(Cmb& cmb, View c) {
    if (!c.Magic(0, "mats")) {
        throw FormatError("missing mats chunk");
    }
    const uint32_t count = c.U32(0x08);
    const size_t stride = cmb.version > 6 ? 0x16C : 0x15C;
    const size_t combinerTable = 0x0C + count * stride;
    for (uint32_t i = 0; i < count; i++) {
        const size_t o = 0x0C + i * stride;
        CmbMaterial m;
        m.fragmentLighting = c.U8(o + 0x00) != 0;
        m.vertexLighting = c.U8(o + 0x01) != 0;
        m.fog = c.U8(o + 0x02) != 0;
        m.renderLayer = c.U8(o + 0x03);
        m.cullMode = c.U8(o + 0x04);
        m.polygonOffset = c.U8(o + 0x05) != 0;
        m.polygonOffsetUnit = c.S8(o + 0x07);
        m.bindingCount = c.U32(o + 0x08);
        m.coordinatorCount = c.U32(o + 0x0C);
        for (int j = 0; j < 3; j++) {
            const size_t b = o + 0x10 + j * 0x18;
            CmbTexBinding& t = m.bindings[j];
            t.texture = c.S16(b + 0x00);
            t.minFilter = c.U16(b + 0x04);
            t.magFilter = c.U16(b + 0x06);
            t.wrapS = c.U16(b + 0x08);
            t.wrapT = c.U16(b + 0x0A);
            t.minLod = c.F32(b + 0x10);
            t.lodBias = c.F32(b + 0x14);
        }
        for (int j = 0; j < 3; j++) {
            const size_t b = o + 0x58 + j * 0x18;
            CmbTexCoordinator& t = m.coordinators[j];
            t.source = c.U8(b + 0x00);
            t.referenceCamera = c.U8(b + 0x01);
            t.mapping = static_cast<TexMapping>(c.U8(b + 0x02));
            t.matrixMode = c.U8(b + 0x03);
            t.scaleS = c.F32(b + 0x04);
            t.scaleT = c.F32(b + 0x08);
            t.translateS = c.F32(b + 0x0C);
            t.translateT = c.F32(b + 0x10);
            t.rotation = c.F32(b + 0x14);
        }
        m.emission = ReadRgba(c, o + 0xA0);
        m.ambient = ReadRgba(c, o + 0xA4);
        m.diffuse = ReadRgba(c, o + 0xA8);
        m.specular0 = ReadRgba(c, o + 0xAC);
        m.specular1 = ReadRgba(c, o + 0xB0);
        for (int j = 0; j < 6; j++) {
            m.constants[j] = ReadRgba(c, o + 0xB4 + j * 4);
        }
        for (int j = 0; j < 4; j++) {
            m.bufferColor[j] = c.F32(o + 0xCC + j * 4);
        }
        const uint32_t combinerCount = c.U32(o + 0x120);
        for (uint32_t j = 0; j < combinerCount && j < 6; j++) {
            const size_t e = combinerTable + size_t(c.U16(o + 0x124 + j * 2)) * 0x28;
            CmbCombiner k;
            k.combineRgb = c.U16(e + 0x00);
            k.combineAlpha = c.U16(e + 0x02);
            k.scaleRgb = c.U16(e + 0x04);
            k.scaleAlpha = c.U16(e + 0x06);
            k.bufferInputRgb = c.U16(e + 0x08);
            k.bufferInputAlpha = c.U16(e + 0x0A);
            for (int s = 0; s < 3; s++) {
                k.srcRgb[s] = c.U16(e + 0x0C + s * 2);
                k.opRgb[s] = c.U16(e + 0x12 + s * 2);
                k.srcAlpha[s] = c.U16(e + 0x18 + s * 2);
                k.opAlpha[s] = c.U16(e + 0x1E + s * 2);
            }
            k.constantIndex = c.U32(e + 0x24);
            if (k.constantIndex > 5) {
                k.constantIndex = 0;
            }
            m.combiners.push_back(k);
        }
        m.alphaTest = c.U8(o + 0x130) != 0;
        m.alphaRef = c.U8(o + 0x131);
        m.alphaFunc = c.U16(o + 0x132);
        m.depthTest = c.U8(o + 0x134) != 0;
        m.depthWrite = c.U8(o + 0x135) != 0;
        m.depthFunc = c.U16(o + 0x136);
        m.blendMode = c.U8(o + 0x138);
        m.blendSrcRgb = c.U16(o + 0x13C);
        m.blendDstRgb = c.U16(o + 0x13E);
        m.blendOpRgb = c.U16(o + 0x140);
        m.blendSrcAlpha = c.U16(o + 0x144);
        m.blendDstAlpha = c.U16(o + 0x146);
        m.blendOpAlpha = c.U16(o + 0x148);
        for (int j = 0; j < 4; j++) {
            m.blendColor[j] = c.F32(o + 0x14C + j * 4);
        }
        cmb.materials.push_back(std::move(m));
    }
}

static CmbShape ParseSepd(const Cmb& cmb, View s) {
    if (!s.Magic(0, "sepd")) {
        throw FormatError("missing sepd chunk");
    }
    CmbShape shape;
    const uint32_t prmsCount = s.U16(0x08);
    shape.flags = s.U16(0x0A);
    shape.center = { s.F32(0x0C), s.F32(0x10), s.F32(0x14) };
    shape.positionOffset = { s.F32(0x18), s.F32(0x1C), s.F32(0x20) };
    size_t a = 0x24;
    auto next = [&]() {
        CmbAttrib r = ReadAttrib(s, a);
        a += 0x1C;
        return r;
    };
    shape.position = next();
    shape.normal = next();
    if (cmb.version > 6) {
        next(); // tangent
    }
    shape.color = next();
    shape.uv[0] = next();
    shape.uv[1] = next();
    shape.uv[2] = next();
    shape.boneIndices = next();
    shape.boneWeights = next();
    shape.boneDimension = s.U16(a);
    a += 4;
    shape.hasVertexColors = cmb.version > 6 ? ((shape.flags >> 3) & 1) : ((shape.flags >> 2) & 1);

    for (uint32_t i = 0; i < prmsCount; i++) {
        View p = s.From(s.U16(a + i * 2));
        if (!p.Magic(0, "prms")) {
            throw FormatError("missing prms chunk");
        }
        CmbPrimitive prim;
        prim.skinning = static_cast<Skinning>(p.U16(0x0C));
        const uint32_t boneCount = p.U16(0x0E);
        const uint32_t boneTable = p.U32(0x10);
        for (uint32_t b = 0; b < boneCount; b++) {
            prim.boneTable.push_back(p.U16(boneTable + b * 2));
        }
        View prm = p.From(p.U32(0x14));
        if (!prm.Magic(0, "prm ")) {
            throw FormatError("missing prm chunk");
        }
        prim.indexType = prm.U16(0x10);
        prim.indexCount = prm.U16(0x14);
        prim.indexByteOffset = uint32_t(prm.U16(0x16)) * 2;
        shape.primitives.push_back(std::move(prim));
    }
    return shape;
}

Cmb Cmb::Parse(View f) {
    if (!f.Magic(0, "cmb ")) {
        throw FormatError("not a CMB model");
    }
    Cmb cmb;
    cmb.file = f.Sub(0, std::min<size_t>(f.U32(0x04), f.Size()));
    f = cmb.file;
    cmb.version = f.U32(0x08);
    cmb.name = f.Str(0x10, 0x10);
    const uint32_t indexCount = f.U32(0x20);
    size_t chunk = 0x24;
    auto nextOffset = [&]() {
        const uint32_t v = f.U32(chunk);
        chunk += 4;
        return v;
    };
    const uint32_t skl = nextOffset();
    if (cmb.version > 6) {
        nextOffset(); // qtrs
    }
    const uint32_t mats = nextOffset();
    const uint32_t tex = nextOffset();
    const uint32_t sklm = nextOffset();
    const uint32_t luts = nextOffset();
    const uint32_t vatr = nextOffset();
    const uint32_t indices = nextOffset();
    const uint32_t texData = nextOffset();
    (void)luts;

    // skl
    {
        View c = f.From(skl);
        if (!c.Magic(0, "skl ")) {
            throw FormatError("missing skl chunk");
        }
        const uint32_t count = c.U32(0x08);
        const size_t stride = cmb.version > 6 ? 0x2C : 0x28;
        for (uint32_t i = 0; i < count; i++) {
            const size_t o = 0x10 + i * stride;
            CmbBone b;
            b.id = c.S16(o) & 0x0FFF;
            b.parent = c.S16(o + 2);
            b.scale = { c.F32(o + 0x04), c.F32(o + 0x08), c.F32(o + 0x0C) };
            b.rotation = { c.F32(o + 0x10), c.F32(o + 0x14), c.F32(o + 0x18) };
            b.translation = { c.F32(o + 0x1C), c.F32(o + 0x20), c.F32(o + 0x24) };
            cmb.bones.push_back(b);
        }
    }
    try {
        ParseMaterials(cmb, f.From(mats));
    } catch (const FormatError& e) {
        throw FormatError(std::string("mats: ") + e.what());
    }
    cmb.textures = ParseTexChunk(f.From(tex));
    if (texData) {
        cmb.texData = f.From(texData);
    }
    // sklm -> mshs + shp
    {
        View c = f.From(sklm);
        if (!c.Magic(0, "sklm")) {
            throw FormatError("missing sklm chunk");
        }
        View mshs = c.From(c.U32(0x08));
        if (!mshs.Magic(0, "mshs")) {
            throw FormatError("missing mshs chunk");
        }
        const uint32_t meshCount = mshs.U32(0x08);
        cmb.opaqueMeshCount = mshs.U16(0x0C);
        const size_t stride = cmb.version > 6 ? 0x0C : 0x04;
        for (uint32_t i = 0; i < meshCount; i++) {
            cmb.meshes.push_back({ mshs.U16(0x10 + i * stride), mshs.U8(0x10 + i * stride + 2) });
        }
        View shp = c.From(c.U32(0x0C));
        if (!shp.Magic(0, "shp ")) {
            throw FormatError("missing shp chunk");
        }
        const uint32_t shapeCount = shp.U32(0x08);
        for (uint32_t i = 0; i < shapeCount; i++) {
            try {
                cmb.shapes.push_back(ParseSepd(cmb, shp.From(shp.U16(0x10 + i * 2))));
            } catch (const FormatError& e) {
                throw FormatError("shape " + std::to_string(i) + "/" + std::to_string(shapeCount) + ": " + e.what());
            }
        }
    }
    // vatr
    {
        View c = f.From(vatr);
        if (!c.Magic(0, "vatr")) {
            throw FormatError("missing vatr chunk");
        }
        size_t o = 0x0C;
        auto slice = [&]() {
            const uint32_t size = c.U32(o), off = c.U32(o + 4);
            o += 8;
            return size ? c.Sub(off, size) : View();
        };
        cmb.vatrPosition = slice();
        cmb.vatrNormal = slice();
        if (cmb.version > 6) {
            slice(); // tangent
        }
        cmb.vatrColor = slice();
        cmb.vatrUv[0] = slice();
        cmb.vatrUv[1] = slice();
        cmb.vatrUv[2] = slice();
        cmb.vatrBoneIndices = slice();
        cmb.vatrBoneWeights = slice();
    }
    cmb.indexBuffer = f.Sub(indices, size_t(indexCount) * 2);
    return cmb;
}

std::vector<Mat34> Cmb::BindPose() const {
    std::vector<Mat34> world(bones.size());
    std::vector<int> state(bones.size(), 0); // 0 todo, 1 in progress, 2 done
    std::function<void(size_t)> solve = [&](size_t i) {
        if (state[i] == 2) {
            return;
        }
        state[i] = 1;
        const CmbBone& b = bones[i];
        Mat34 local = Mat34::FromSRT(b.scale, b.rotation, b.translation);
        if (b.parent >= 0 && size_t(b.parent) < bones.size() && state[b.parent] != 1) {
            solve(b.parent);
            world[i] = world[b.parent] * local;
        } else {
            world[i] = local;
        }
        state[i] = 2;
    };
    for (size_t i = 0; i < bones.size(); i++) {
        solve(i);
    }
    return world;
}

std::vector<CmbTriangleSet> Cmb::Decode() const {
    std::vector<CmbTriangleSet> out;
    decodeIssues = 0;
    for (size_t meshIdx = 0; meshIdx < meshes.size(); meshIdx++) {
        const CmbMesh& mesh = meshes[meshIdx];
        if (mesh.shape >= shapes.size()) {
            continue;
        }
        const CmbShape& shape = shapes[mesh.shape];
        for (const CmbPrimitive& prim : shape.primitives) {
            CmbTriangleSet set;
            set.mesh = static_cast<int>(meshIdx);
            set.material = mesh.material;
            set.skinning = prim.skinning;
            set.hasVertexColors = shape.hasVertexColors;
            std::unordered_map<uint32_t, uint32_t> remap;
            const size_t isize = prim.indexType == 0x1401 ? 1 : 2;
            std::vector<uint32_t> idx(prim.indexCount);
            for (uint32_t i = 0; i < prim.indexCount; i++) {
                const size_t off = prim.indexByteOffset + i * isize;
                idx[i] = isize == 1 ? indexBuffer.U8(off) : indexBuffer.U16(off);
            }
            auto vertex = [&](uint32_t src) -> uint32_t {
                auto it = remap.find(src);
                if (it != remap.end()) {
                    return it->second;
                }
                CmbVertex v;
                float tmp[4];
                // An attribute whose array is too short for this vertex (seen on a few effect/boss models) is
                // treated as absent, as the 3DS GPU would read garbage there anyway.
                auto read = [&](const CmbAttrib& a, View slice, int n, float* out, const char* what) {
                    if (a.mode != 1 && slice.Empty()) {
                        return false;
                    }
                    try {
                        ReadVector(a, slice, src, n, out);
                        return true;
                    } catch (const FormatError&) {
                        if (std::string(what) == "position") {
                            throw;
                        }
                        decodeIssues++;
                        return false;
                    }
                };
                read(shape.position, vatrPosition, 3, tmp, "position");
                v.pos = { tmp[0], tmp[1], tmp[2] };
                if (read(shape.normal, vatrNormal, 3, tmp, "normal")) {
                    v.normal = { tmp[0], tmp[1], tmp[2] };
                }
                if (shape.hasVertexColors) {
                    read(shape.color, vatrColor, 4, v.color.data(), "color");
                }
                for (int u = 0; u < 3; u++) {
                    read(shape.uv[u], vatrUv[u], 2, v.uv[u].data(), "uv");
                }
                const int dim = std::clamp<int>(shape.boneDimension, 1, 4);
                float bi[4] = { 0, 0, 0, 0 }, bw[4] = { 1, 0, 0, 0 };
                // Indices come from the array when it holds bytes, else from the attribute's constant (noclip: a
                // non-byte array is not bound and the GPU reads the constant). A shape whose vertices all blend the
                // same bones keeps them in the constant (Kokiri heads: bones 0 and 1 of the table, weighted per
                // vertex); ignoring it put every such vertex on the table's first bone.
                if (prim.skinning != Skinning::SingleBone &&
                    (shape.boneIndices.type == 0x1401 || (shape.boneIndices.mode == 1 && !getenv("OOT3D_OLD_BONEIDX")))) {
                    CmbAttrib a = shape.boneIndices;
                    if (a.type != 0x1401) {
                        a.mode = 1;
                    }
                    read(a, vatrBoneIndices, dim, bi, "bone indices");
                }
                if (prim.skinning == Skinning::Smooth) {
                    read(shape.boneWeights, vatrBoneWeights, dim, bw, "bone weights");
                }
                v.boneCount = prim.skinning == Skinning::Smooth ? dim : 1;
                for (int b = 0; b < 4; b++) {
                    const int local = static_cast<int>(bi[b]);
                    v.bones[b] = (local >= 0 && size_t(local) < prim.boneTable.size()) ? prim.boneTable[local] : 0;
                    v.weights[b] = b < v.boneCount ? bw[b] : 0.0f;
                }
                if (prim.skinning != Skinning::Smooth) {
                    v.weights = { 1, 0, 0, 0 };
                }
                const uint32_t id = static_cast<uint32_t>(set.vertices.size());
                set.vertices.push_back(v);
                remap.emplace(src, id);
                return id;
            };
            for (uint32_t i = 0; i + 2 < prim.indexCount; i += 3) {
                const uint32_t a = vertex(idx[i]), b = vertex(idx[i + 1]), c = vertex(idx[i + 2]);
                if (a != b && b != c && a != c) {
                    set.triangles.push_back({ a, b, c });
                }
            }
            if (!set.triangles.empty()) {
                out.push_back(std::move(set));
            }
        }
    }
    return out;
}

Image Cmb::DecodeTexture(size_t index) const {
    const TextureDesc& t = textures.at(index);
    if (texData.Empty()) {
        throw FormatError("model " + name + " has no texture data");
    }
    return DecodePica(t.format, t.width, t.height,
                      texData.Sub(t.dataOffset, PicaByteSize(t.format, t.width, t.height)));
}

static Mat34 SkinMatrix(const CmbVertex& v, Skinning mode, const std::vector<Mat34>& world,
                        const std::vector<Mat34>& invBind) {
    auto boneWorld = [&](int b) { return size_t(b) < world.size() ? world[b] : Mat34::Identity(); };
    if (mode != Skinning::Smooth) {
        return boneWorld(v.bones[0]);
    }
    Mat34 acc;
    for (auto& row : acc.m) {
        for (float& x : row) {
            x = 0;
        }
    }
    float total = 0;
    for (int i = 0; i < v.boneCount; i++) {
        if (v.weights[i] <= 0) {
            continue;
        }
        const int b = v.bones[i];
        const Mat34 inv = size_t(b) < invBind.size() ? invBind[b] : Mat34::Identity();
        acc += (boneWorld(b) * inv).Scaled(v.weights[i]);
        total += v.weights[i];
    }
    return total > 0 ? acc.Scaled(1.0f / total) : Mat34::Identity();
}

Vec3 SkinPosition(const CmbVertex& v, Skinning mode, const std::vector<Mat34>& world,
                  const std::vector<Mat34>& invBind) {
    return SkinMatrix(v, mode, world, invBind).Point(v.pos);
}

Vec3 SkinNormal(const CmbVertex& v, Skinning mode, const std::vector<Mat34>& world,
                const std::vector<Mat34>& invBind) {
    return SkinMatrix(v, mode, world, invBind).Normal(v.normal);
}

} // namespace Oot3d
