#include "Oot3dCsab.h"

#include <cmath>

namespace Oot3d {

static constexpr float kTau = 6.28318530717958647692f;

// Hermite tangents are not stored in the keys' own units: measured against the slope between neighbouring keys over
// every CSAB in the game, f32 translation tangents are 1/40 of it (every archive: 0.025) and s16 rotation tangents
// twice it (Link: 1.995); f32 rotation tangents match it. Read as stored, translation curves ease to a stop at every
// key and Link's rotations overshoot: choppy, slow-looking motion.
static constexpr float kTranslationTangentScale = 40.0f;
static constexpr float kInt16RotationTangentScale = 0.5f;

static CsabTrack ParseTrack(View t, bool rotationInt16, int component) {
    CsabTrack tr;
    tr.type = static_cast<CsabTrack::Type>(t.U32(0x00));
    const uint32_t count = t.U32(0x04);
    tr.timeStart = t.U32(0x08);
    tr.timeEnd = t.U32(0x0C) + 1;
    size_t o = 0x10;
    tr.keys.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        CsabKey k;
        if (tr.type == CsabTrack::Type::Linear && rotationInt16) {
            // u16 frame + s16 angle (e.g. child/anim/om_get.csab's last node)
            k.time = t.U16(o);
            k.value = t.S16(o + 2) * (kTau / 65536.0f);
            o += 4;
        } else if (tr.type == CsabTrack::Type::Linear) {
            k.time = static_cast<float>(t.U32(o));
            k.value = t.F32(o + 4);
            o += 8;
        } else if (tr.type == CsabTrack::Type::Hermite) {
            if (rotationInt16) {
                k.time = t.U16(o);
                k.value = t.S16(o + 2) * (kTau / 65536.0f);
                k.tangentIn = t.S16(o + 4) * (kTau / 65536.0f) * kInt16RotationTangentScale;
                k.tangentOut = t.S16(o + 6) * (kTau / 65536.0f) * kInt16RotationTangentScale;
                o += 8;
            } else {
                const float ts = component < 3 ? kTranslationTangentScale : 1.0f;
                k.time = static_cast<float>(t.U32(o));
                k.value = t.F32(o + 4);
                k.tangentIn = t.F32(o + 8) * ts;
                k.tangentOut = t.F32(o + 12) * ts;
                o += 16;
            }
        } else {
            throw FormatError("unknown CSAB track type " + std::to_string(t.U32(0)));
        }
        tr.keys.push_back(k);
    }
    return tr;
}

Csab Csab::Parse(View f) {
    if (!f.Magic(0, "csab")) {
        throw FormatError("not a CSAB animation");
    }
    if (f.U32(0x08) != 3) {
        throw FormatError("unsupported CSAB subversion " + std::to_string(f.U32(0x08)));
    }
    Csab a;
    if (f.Size() < 0x38) {
        return a;
    }
    a.duration = f.U32(0x28) + 1;
    const uint32_t nodeCount = f.U32(0x30);
    const uint32_t boneCount = f.U32(0x34);
    size_t o = 0x38;
    for (uint32_t i = 0; i < boneCount; i++, o += 2) {
        a.boneToNode.push_back(f.S16(o));
    }
    o = AlignUp(o, 4);
    for (uint32_t i = 0; i < nodeCount; i++, o += 4) {
        View n = f.From(0x18 + f.U32(o));
        if (!n.Magic(0, "anod")) {
            throw FormatError("missing anod chunk");
        }
        CsabNode node;
        node.bone = n.U16(0x04);
        const bool rotInt16 = n.U16(0x06) != 0;
        for (int k = 0; k < 9; k++) {
            const uint16_t off = n.U16(0x08 + k * 2);
            if (off) {
                node.tracks[k] = ParseTrack(n.From(off), rotInt16 && k >= 3 && k < 6, k);
            }
        }
        a.nodes.push_back(std::move(node));
    }
    return a;
}

const CsabNode* Csab::NodeForBone(size_t bone) const {
    if (bone >= boneToNode.size() || boneToNode[bone] < 0 || size_t(boneToNode[bone]) >= nodes.size()) {
        return nullptr;
    }
    return &nodes[boneToNode[bone]];
}

static float LerpAngle(float a, float b, float t) {
    float d = std::fmod(b - a, kTau);
    if (d > kTau / 2) {
        d -= kTau;
    } else if (d < -kTau / 2) {
        d += kTau;
    }
    return a + d * t;
}

float CsabTrack::Sample(float frame, bool isAngle) const {
    if (keys.empty()) {
        return 0;
    }
    size_t i1 = 0;
    while (i1 < keys.size() && !(frame < keys[i1].time)) {
        i1++;
    }
    if (type != Type::Hermite) {
        if (i1 == 0) {
            return keys.front().value;
        }
        if (i1 == keys.size()) {
            return keys.back().value;
        }
        const CsabKey& k0 = keys[i1 - 1];
        const CsabKey& k1 = keys[i1];
        const float t = (frame - k0.time) / (k1.time - k0.time);
        return isAngle ? LerpAngle(k0.value, k1.value, t) : k0.value + (k1.value - k0.value) * t;
    }
    const CsabKey* k0;
    const CsabKey* k1;
    if (i1 == 0 || i1 == keys.size()) {
        k0 = &keys.back();
        k1 = &keys.front();
    } else {
        k0 = &keys[i1 - 1];
        k1 = &keys[i1];
    }
    float length = k1->time - k0->time;
    if (timeEnd) {
        length = std::fmod(length, static_cast<float>(timeEnd));
        if (length < 0) {
            length += static_cast<float>(timeEnd);
        }
    }
    if (length <= 0) {
        return k0->value;
    }
    float t = (frame - k0->time) / length;
    if (t < 0) {
        t += static_cast<float>(timeEnd) / length;
    }
    const float t2 = t * t, t3 = t2 * t;
    const float s0 = k0->tangentOut * length, s1 = k1->tangentIn * length;
    // Angles crossing +-180 between keys take the short way (as the linear case does).
    const float v1 = isAngle ? LerpAngle(k0->value, k1->value, 1.0f) : k1->value;
    return (2 * t3 - 3 * t2 + 1) * k0->value + (t3 - 2 * t2 + t) * s0 + (-2 * t3 + 3 * t2) * v1 + (t3 - t2) * s1;
}

} // namespace Oot3d
