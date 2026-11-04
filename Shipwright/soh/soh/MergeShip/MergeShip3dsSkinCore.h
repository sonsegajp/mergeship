#pragma once


// MergeShip2 -- Ocarina of Time 3D characters: the package reader, the 3DS animation sampling and the skinning, with
// no SoH dependencies. MergeShip3dsSkin.cpp draws with it in the game; tools/oot3d renders with the same code offline
// (oot3d-extract skinrender), so what the converter checks is exactly what the game computes. The package layout is
// documented in tools/oot3d/src/Oot3dSkin.h.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ms3ds {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 2.0f * kPi;

struct Mat {
    float m[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };

    // T * Rz * Ry * Rx * S (the CMB bone convention).
    static Mat Srt(const float s[3], const float r[3], const float t[3]) {
        const float sx = std::sin(r[0]), cx = std::cos(r[0]);
        const float sy = std::sin(r[1]), cy = std::cos(r[1]);
        const float sz = std::sin(r[2]), cz = std::cos(r[2]);
        Mat o;
        o.m[0][0] = s[0] * (cy * cz);
        o.m[1][0] = s[0] * (sz * cy);
        o.m[2][0] = s[0] * (-sy);
        o.m[0][1] = s[1] * (sx * cz * sy - cx * sz);
        o.m[1][1] = s[1] * (sx * sz * sy + cx * cz);
        o.m[2][1] = s[1] * (sx * cy);
        o.m[0][2] = s[2] * (cx * cz * sy + sx * sz);
        o.m[1][2] = s[2] * (cx * sz * sy - sx * cz);
        o.m[2][2] = s[2] * (cy * cx);
        o.m[0][3] = t[0];
        o.m[1][3] = t[1];
        o.m[2][3] = t[2];
        return o;
    }
    Mat operator*(const Mat& b) const {
        Mat o;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                float v = m[r][0] * b.m[0][c] + m[r][1] * b.m[1][c] + m[r][2] * b.m[2][c];
                if (c == 3) {
                    v += m[r][3];
                }
                o.m[r][c] = v;
            }
        }
        return o;
    }
};

inline float WrapAngle(float d) {
    d = std::fmod(d, kTau);
    if (d > kPi) {
        d -= kTau;
    } else if (d < -kPi) {
        d += kTau;
    }
    return d;
}

struct Track {
    uint8_t type = 1; // 1 linear, 2 hermite
    float timeEnd = 0;
    std::vector<float> keys; // stride 2 (t, v) or 4 (t, v, in, out)

    float Sample(float frame, bool angle) const {
        const size_t stride = type == 2 ? 4 : 2;
        const size_t n = keys.size() / stride;
        if (n == 0) {
            return 0;
        }
        size_t i1 = 0;
        while (i1 < n && !(frame < keys[i1 * stride])) {
            i1++;
        }
        if (type != 2) {
            if (i1 == 0) {
                return keys[1];
            }
            if (i1 == n) {
                return keys[(n - 1) * stride + 1];
            }
            const float t0 = keys[(i1 - 1) * stride], v0 = keys[(i1 - 1) * stride + 1];
            const float t1 = keys[i1 * stride], v1 = keys[i1 * stride + 1];
            const float t = (frame - t0) / (t1 - t0);
            if (angle) {
                return v0 + WrapAngle(v1 - v0) * t;
            }
            return v0 + (v1 - v0) * t;
        }
        const float* k0;
        const float* k1;
        if (i1 == 0 || i1 == n) {
            k0 = &keys[(n - 1) * stride];
            k1 = &keys[0];
        } else {
            k0 = &keys[(i1 - 1) * stride];
            k1 = &keys[i1 * stride];
        }
        float length = k1[0] - k0[0];
        if (timeEnd > 0) {
            length = std::fmod(length, timeEnd);
            if (length < 0) {
                length += timeEnd;
            }
        }
        if (length <= 0) {
            return k0[1];
        }
        float t = (frame - k0[0]) / length;
        if (t < 0 && timeEnd > 0) {
            t += timeEnd / length;
        }
        const float t2 = t * t, t3 = t2 * t;
        const float s0 = k0[3] * length, s1 = k1[2] * length;
        // Angles crossing +-180 between keys take the short way (as the linear case does).
        const float v1 = angle ? k0[1] + WrapAngle(k1[1] - k0[1]) : k1[1];
        return (2 * t3 - 3 * t2 + 1) * k0[1] + (t3 - 2 * t2 + t) * s0 + (-2 * t3 + 3 * t2) * v1 + (t3 - t2) * s1;
    }
};

struct Anim {
    std::string n64Path;
    int16_t n64Frames = 0;
    float frames = 0;
    std::vector<std::array<int32_t, 9>> channels; // per bone: track index or -1
    std::vector<Track> tracks;
};

struct Bone {
    int16_t parent = -1;
    float scale[3], rot[3], trans[3];
    Mat invBind;
};

struct Vert {
    float pos[3], normal[3];
    uint8_t bone[4], weight[4];
};

struct BatchVertex {
    uint32_t vertex;
    int16_t s, t;
    uint8_t rgba[4];
    uint8_t envAlpha = 0; // v5: the reflection pass's SHADE alpha
};

struct Batch {
    std::vector<BatchVertex> vertices;
    std::vector<uint8_t> tris;
};

struct Group {
    std::string statePath; // the material's state display list (resource path, no "__OTR__")
    bool lit = false, translucent = false;
    // Drawn only while vanilla limb `limb` draws display list `list` (a head or held-prop variant); -1: always.
    int16_t limb = -1;
    std::string list;
    // v3 face palette: the state list per palette texture (resource paths, no "__OTR__"), Grezzo's texture-index
    // track (time, value) over `paletteFrames`, and the N64 textures (resource paths) paired with a palette index.
    std::vector<std::string> palettePaths;
    std::vector<std::array<float, 2>> paletteKeys;
    uint32_t paletteFrames = 0;
    bool paletteLoop = true;
    std::vector<std::pair<std::string, int>> paletteN64;
    // v5: a sphere-mapped reflection (G_TEXTURE_GEN): its state list, drawn over the batches again (empty: none).
    std::string envStatePath;
    std::vector<Batch> batches;
};

// The palette texture Grezzo's track shows at `frame` (stepped, as an integer track).
inline int PaletteIndexAt(const Group& g, float frame) {
    if (g.paletteKeys.empty()) {
        return -1;
    }
    if (g.paletteFrames > 0) {
        frame = g.paletteLoop ? std::fmod(frame, float(g.paletteFrames)) : std::min(frame, float(g.paletteFrames));
    }
    float v = g.paletteKeys.front()[1];
    for (const auto& k : g.paletteKeys) {
        if (frame < k[0]) {
            break;
        }
        v = k[1];
    }
    return static_cast<int>(v);
}

struct Skin {
    std::vector<Bone> bones;
    std::vector<Vert> vertices;
    std::vector<Group> groups;
    std::vector<Anim> anims;
    std::unordered_map<std::string, int> animByPath;
    bool anyTranslucent = false;
    std::vector<int16_t> keepLimbs; // v3: N64 limbs whose own lists still draw (effects)
    float offset[3] = { 0, 0, 0 }; // v4: where the model sits in the actor's space (one model over several actors)
};

struct Pose {
    std::array<float, 9> v; // tx ty tz rx ry rz sx sy sz
};

class Reader {
  public:
    Reader(const uint8_t* d, size_t n) : mData(d), mSize(n) {
    }
    template <typename T> T Get() {
        T v{};
        if (mPos + sizeof(T) <= mSize) {
            memcpy(&v, mData + mPos, sizeof(T));
        } else {
            mBad = true;
        }
        mPos += sizeof(T);
        return v;
    }
    std::string Str() {
        const uint32_t n = Get<uint32_t>();
        if (mPos + n > mSize) {
            mBad = true;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(mData + mPos), n);
        mPos += n;
        return s;
    }
    void Align(size_t a) {
        mPos = (mPos + a - 1) & ~(a - 1);
    }
    bool Bad() const {
        return mBad;
    }

  private:
    const uint8_t* mData;
    size_t mSize, mPos = 0;
    bool mBad = false;
};

// The package (the Blob resource's data). nullptr when it is damaged.
inline std::shared_ptr<Skin> Parse(const uint8_t* data, size_t size) {
    Reader r(data, size);
    if (r.Get<uint32_t>() != 0x4B53334F) {
        return nullptr;
    }
    const uint32_t version = r.Get<uint32_t>();
    if (version < 1 || version > 5) {
        return nullptr;
    }
    auto skin = std::make_shared<Skin>();
    const uint32_t boneCount = r.Get<uint32_t>(), vertexCount = r.Get<uint32_t>(), groupCount = r.Get<uint32_t>();
    const uint32_t animCount = r.Get<uint32_t>();
    const uint32_t limbCount = r.Get<uint32_t>();
    if (boneCount > 255 || vertexCount > 200000 || groupCount > 4096 || animCount > 65535) {
        return nullptr;
    }
    skin->bones.resize(boneCount);
    for (Bone& b : skin->bones) {
        b.parent = r.Get<int16_t>();
        r.Get<uint16_t>();
        for (float* dst : { b.scale, b.rot, b.trans }) {
            for (int k = 0; k < 3; k++) {
                dst[k] = r.Get<float>();
            }
        }
        for (auto& row : b.invBind.m) {
            for (float& x : row) {
                x = r.Get<float>();
            }
        }
    }
    skin->vertices.resize(vertexCount);
    for (Vert& v : skin->vertices) {
        for (float& f : v.pos) {
            f = r.Get<float>();
        }
        for (float& f : v.normal) {
            f = r.Get<float>();
        }
        for (uint8_t& b : v.bone) {
            b = r.Get<uint8_t>();
            if (b >= boneCount) {
                b = 0;
            }
        }
        for (uint8_t& w : v.weight) {
            w = r.Get<uint8_t>();
        }
    }
    skin->groups.resize(groupCount);
    for (Group& g : skin->groups) {
        g.statePath = r.Str();
        g.lit = r.Get<uint8_t>() != 0;
        g.translucent = r.Get<uint8_t>() != 0;
        if (version >= 2) {
            g.limb = r.Get<int16_t>();
            g.list = r.Str();
        }
        if (version >= 3) {
            const uint8_t paletteCount = r.Get<uint8_t>();
            if (paletteCount > 0) {
                g.paletteLoop = r.Get<uint8_t>() != 0;
                r.Get<uint16_t>();
                g.paletteFrames = r.Get<uint32_t>();
                for (uint8_t k = 0; k < paletteCount; k++) {
                    g.palettePaths.push_back(r.Str());
                }
                const uint16_t keys = r.Get<uint16_t>();
                r.Get<uint16_t>();
                for (uint16_t k = 0; k < keys; k++) {
                    const float t = r.Get<float>();
                    const float v = r.Get<float>();
                    g.paletteKeys.push_back({ t, v });
                }
                const uint16_t n64 = r.Get<uint16_t>();
                r.Get<uint16_t>();
                for (uint16_t k = 0; k < n64; k++) {
                    std::string path = r.Str();
                    const int idx = static_cast<int>(r.Get<uint32_t>());
                    g.paletteN64.emplace_back(std::move(path), idx);
                }
            }
        }
        if (version >= 5) {
            g.envStatePath = r.Str();
        }
        skin->anyTranslucent |= g.translucent;
        const uint16_t batches = r.Get<uint16_t>();
        g.batches.resize(batches);
        for (Batch& b : g.batches) {
            const uint8_t vc = r.Get<uint8_t>(), tc = r.Get<uint8_t>();
            r.Get<uint16_t>();
            b.vertices.resize(vc);
            for (BatchVertex& v : b.vertices) {
                v.vertex = r.Get<uint32_t>();
                if (v.vertex >= vertexCount) {
                    v.vertex = 0;
                }
                v.s = r.Get<int16_t>();
                v.t = r.Get<int16_t>();
                for (uint8_t& c : v.rgba) {
                    c = r.Get<uint8_t>();
                }
                if (version >= 5) {
                    v.envAlpha = r.Get<uint8_t>();
                }
            }
            b.tris.resize(size_t(tc) * 3);
            for (uint8_t& i : b.tris) {
                i = r.Get<uint8_t>();
                if (i >= vc) {
                    i = 0;
                }
            }
            r.Align(4);
        }
    }
    skin->anims.resize(animCount);
    for (uint32_t a = 0; a < animCount; a++) {
        Anim& an = skin->anims[a];
        an.n64Path = r.Str();
        an.n64Frames = r.Get<int16_t>();
        r.Get<uint16_t>();
        an.frames = r.Get<float>();
        an.channels.resize(boneCount);
        for (uint32_t b = 0; b < boneCount; b++) {
            const uint16_t mask = r.Get<uint16_t>();
            for (int k = 0; k < 9; k++) {
                an.channels[b][k] = -1;
                if (!(mask & (1u << k))) {
                    continue;
                }
                Track t;
                t.type = r.Get<uint8_t>();
                r.Get<uint8_t>();
                const uint16_t keys = r.Get<uint16_t>();
                t.timeEnd = r.Get<float>();
                t.keys.resize(size_t(keys) * (t.type == 2 ? 4 : 2));
                for (float& f : t.keys) {
                    f = r.Get<float>();
                }
                an.channels[b][k] = static_cast<int32_t>(an.tracks.size());
                an.tracks.push_back(std::move(t));
            }
        }
        skin->animByPath[an.n64Path] = static_cast<int>(a);
    }
    if (version >= 3) {
        for (uint32_t l = 0; l < limbCount; l++) {
            r.Get<int16_t>(); // limb -> bone
        }
        const uint16_t keep = r.Get<uint16_t>();
        for (uint16_t k = 0; k < keep; k++) {
            skin->keepLimbs.push_back(r.Get<int16_t>());
        }
    }
    if (version >= 4) {
        for (int c = 0; c < 3; c++) {
            skin->offset[c] = r.Get<float>();
        }
    }
    if (r.Bad()) {
        return nullptr;
    }
    return skin;
}

inline void BindPose(const Skin& skin, std::vector<Pose>& pose) {
    pose.resize(skin.bones.size());
    for (size_t b = 0; b < skin.bones.size(); b++) {
        const Bone& bone = skin.bones[b];
        pose[b].v = { bone.trans[0], bone.trans[1], bone.trans[2], bone.rot[0], bone.rot[1],
                      bone.rot[2],   bone.scale[0], bone.scale[1], bone.scale[2] };
    }
}

// Whether the 3DS clip plays on its own clock: its N64 animation only holds a pose (the dancing couple's two frames,
// the dance done by the actor) while Grezzo animated the whole motion.
inline bool OwnClock(const Anim& an) {
    return an.n64Frames <= 3 && an.frames > 2.0f * float(std::max<int16_t>(an.n64Frames, 1));
}

// The 3DS frame shown at the N64 animation's frame: the same frame, or the same point in time when the two
// animations differ in length; a clip on its own clock plays (looping) `clock` frames after it started.
inline float AnimFrame(const Anim& an, float n64Frame, float clock = 0.0f) {
    if (OwnClock(an)) {
        return an.frames > 1.0f ? std::fmod(clock, an.frames - 1.0f) : 0.0f;
    }
    if (std::fabs(an.frames - an.n64Frames) > 1.0f && an.n64Frames > 1) {
        return n64Frame * (an.frames - 1.0f) / float(an.n64Frames - 1);
    }
    return n64Frame;
}

// The pose of the paired 3DS animation at the N64 animation's frame (bones without a track keep their bind values).
inline void SamplePose(const Skin& skin, const Anim& an, float n64Frame, std::vector<Pose>& pose, float clock = 0.0f) {
    const float frame = AnimFrame(an, n64Frame, clock);
    BindPose(skin, pose);
    for (size_t b = 0; b < skin.bones.size() && b < an.channels.size(); b++) {
        for (int k = 0; k < 9; k++) {
            const int32_t t = an.channels[b][k];
            if (t >= 0) {
                pose[b].v[k] = an.tracks[t].Sample(frame, k >= 3 && k < 6);
            }
        }
    }
}

// Blends a pose toward `from` by `w` (the N64 morph: w = 1 is all `from`).
inline void BlendPose(std::vector<Pose>& pose, const std::vector<Pose>& from, float w) {
    if (from.size() != pose.size()) {
        return;
    }
    for (size_t b = 0; b < pose.size(); b++) {
        for (int k = 0; k < 9; k++) {
            const float a = pose[b].v[k];
            float d = from[b].v[k] - a;
            if (k >= 3 && k < 6) {
                d = WrapAngle(d);
            }
            pose[b].v[k] = a + d * w;
        }
    }
}

// Bone matrices in model space.
inline std::vector<Mat> BoneWorld(const Skin& skin, const std::vector<Pose>& pose) {
    const size_t bones = skin.bones.size();
    std::vector<Mat> world(bones);
    std::vector<uint8_t> done(bones, 0);
    for (size_t pass = 0; pass < bones; pass++) {
        bool progress = false;
        for (size_t b = 0; b < bones; b++) {
            if (done[b]) {
                continue;
            }
            const int16_t parent = skin.bones[b].parent;
            if (parent >= 0 && size_t(parent) < bones && !done[parent]) {
                continue;
            }
            const Mat local = Mat::Srt(&pose[b].v[6], &pose[b].v[3], &pose[b].v[0]);
            world[b] = (parent >= 0 && size_t(parent) < bones) ? world[parent] * local : local;
            done[b] = 1;
            progress = true;
        }
        if (!progress) {
            break;
        }
    }
    return world;
}

// Skinned positions and unit normals (xyz per vertex) for a pose.
inline void SkinVertices(const Skin& skin, const std::vector<Pose>& pose, std::vector<float>& pos,
                         std::vector<float>& nrm) {
    const std::vector<Mat> world = BoneWorld(skin, pose);
    std::vector<Mat> skinMtx(world.size());
    for (size_t b = 0; b < world.size(); b++) {
        skinMtx[b] = world[b] * skin.bones[b].invBind;
    }
    const size_t nv = skin.vertices.size();
    pos.assign(nv * 3, 0.0f);
    nrm.assign(nv * 3, 0.0f);
    for (size_t i = 0; i < nv; i++) {
        const Vert& v = skin.vertices[i];
        float p[3] = { 0, 0, 0 }, n[3] = { 0, 0, 0 };
        for (int k = 0; k < 4; k++) {
            if (v.weight[k] == 0) {
                continue;
            }
            const float w = v.weight[k] / 255.0f;
            const Mat& m = skinMtx[v.bone[k]];
            for (int r = 0; r < 3; r++) {
                p[r] += w * (m.m[r][0] * v.pos[0] + m.m[r][1] * v.pos[1] + m.m[r][2] * v.pos[2] + m.m[r][3]);
                n[r] += w * (m.m[r][0] * v.normal[0] + m.m[r][1] * v.normal[1] + m.m[r][2] * v.normal[2]);
            }
        }
        const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        for (int r = 0; r < 3; r++) {
            pos[i * 3 + r] = p[r];
            nrm[i * 3 + r] = len > 1e-6f ? n[r] / len : 0.0f;
        }
    }
}

} // namespace ms3ds
