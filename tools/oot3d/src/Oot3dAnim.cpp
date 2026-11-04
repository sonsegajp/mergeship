#include "Oot3dAnim.h"

#include "Oot3dVanilla.h"

#include <cmath>
#include <functional>

namespace Oot3d {

static constexpr float kBinAng = 3.14159265358979323846f / 32768.0f;

int16_t VanillaAnimation::Value(size_t joint, int axis, int frame) const {
    if (joint >= joints.size()) {
        return 0;
    }
    const uint16_t idx = joints[joint][axis];
    size_t i = idx;
    if (idx >= staticIndexMax) {
        i = size_t(idx) + std::max(0, frame);
    }
    return i < values.size() ? static_cast<int16_t>(values[i]) : 0;
}

std::optional<VanillaAnimation> ReadVanillaAnimation(const ZipReader& oot, const std::string& path) {
    auto b = oot.Read(path);
    if (!b) {
        return std::nullopt;
    }
    auto h = ParseResourceHeader(*b);
    if (!h || h->type != kTypeAnimation) {
        return std::nullopt;
    }
    View v(*b);
    size_t o = 64;
    VanillaAnimation a;
    a.path = path;
    a.type = v.U32(o);
    o += 4;
    if (a.type == 0) {
        a.frameCount = v.S16(o);
        o += 2;
        const uint32_t n = v.U32(o);
        o += 4;
        a.values.resize(n);
        for (uint32_t i = 0; i < n; i++, o += 2) {
            a.values[i] = v.U16(o);
        }
        const uint32_t m = v.U32(o);
        o += 4;
        a.joints.resize(m);
        for (uint32_t i = 0; i < m; i++, o += 6) {
            a.joints[i] = { v.U16(o), v.U16(o + 2), v.U16(o + 4) };
        }
        a.staticIndexMax = v.S16(o);
    } else if (a.type == 1) {
        a.frameCount = v.S16(o);
        o += 2;
        const uint32_t n = v.U32(o);
        a.playerDataPath = v.Str(o + 4, n);
    }
    return a;
}

std::vector<Mat34> N64Pose(const VanillaSkeleton& skel, const VanillaAnimation& anim, int frame) {
    const size_t n = skel.limbs.size();
    std::vector<Mat34> out(n);
    std::vector<bool> done(n, false);
    std::function<void(size_t)> solve = [&](size_t i) {
        if (done[i]) {
            return;
        }
        const VanillaLimb& l = skel.limbs[i];
        Vec3 t(l.trans[0], l.trans[1], l.trans[2]);
        if (i == 0) {
            t = Vec3(0, 0, 0); // the root translation comes from the animation; compare poses root-relative
        }
        const Vec3 r(anim.Value(i + 1, 0, frame) * kBinAng, anim.Value(i + 1, 1, frame) * kBinAng,
                     anim.Value(i + 1, 2, frame) * kBinAng);
        const Mat34 local = Mat34::FromSRT(Vec3(1, 1, 1), r, t);
        if (l.parent >= 0 && size_t(l.parent) < n) {
            solve(l.parent);
            out[i] = out[l.parent] * local;
        } else {
            out[i] = local;
        }
        done[i] = true;
    };
    for (size_t i = 0; i < n; i++) {
        solve(i);
    }
    return out;
}

std::vector<Mat34> CsabPose(const Cmb& cmb, const Csab& anim, float frame) {
    const size_t n = cmb.bones.size();
    std::vector<Mat34> out(n);
    std::vector<bool> done(n, false);
    std::function<void(size_t)> solve = [&](size_t i) {
        if (done[i]) {
            return;
        }
        const CmbBone& b = cmb.bones[i];
        Vec3 s = b.scale, r = b.rotation, t = b.translation;
        if (const CsabNode* node = anim.NodeForBone(i)) {
            float* dst[9] = { &t.x, &t.y, &t.z, &r.x, &r.y, &r.z, &s.x, &s.y, &s.z };
            for (int k = 0; k < 9; k++) {
                if (node->tracks[k]) {
                    *dst[k] = node->tracks[k]->Sample(frame, k >= 3 && k < 6);
                }
            }
        }
        if (b.parent < 0) {
            t = Vec3(0, 0, 0);
        }
        const Mat34 local = Mat34::FromSRT(s, r, t);
        if (b.parent >= 0 && size_t(b.parent) < n) {
            solve(b.parent);
            out[i] = out[b.parent] * local;
        } else {
            out[i] = local;
        }
        done[i] = true;
    };
    for (size_t i = 0; i < n; i++) {
        solve(i);
    }
    return out;
}

} // namespace Oot3d
