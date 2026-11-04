#include "Oot3dAnimPair.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Oot3d {

namespace {

constexpr int kSamples = 17;

using Samples = std::vector<std::vector<Vec3>>; // [sample][joint] model-space positions, root at the origin

Samples SampleN64(const VanillaSkeleton& skel, const VanillaAnimation& a) {
    Samples s(kSamples);
    for (int i = 0; i < kSamples; i++) {
        const int frame = a.frameCount > 1 ? int(std::lround(double(i) / (kSamples - 1) * (a.frameCount - 1))) : 0;
        for (const Mat34& m : N64Pose(skel, a, frame)) {
            s[i].push_back(m.Translation());
        }
    }
    return s;
}

Samples SampleCsab(const Cmb& cmb, const Csab& c) {
    Samples s(kSamples);
    for (int i = 0; i < kSamples; i++) {
        const float frame = c.duration > 1 ? float(i) / (kSamples - 1) * float(c.duration - 1) : 0.0f;
        for (const Mat34& m : CsabPose(cmb, c, frame)) {
            s[i].push_back(m.Translation());
        }
    }
    return s;
}

float Extent(const Samples& s) {
    Vec3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
    for (const auto& f : s) {
        for (const Vec3& p : f) {
            mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
            mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
        }
    }
    return std::max(1.0f, (mx - mn).Length());
}

// Greedy unique limb -> bone assignment by mean distance over the samples; limb 0 is always bone 0.
std::vector<int> Correspond(const Samples& n, const Samples& c, float scale, float& error) {
    const size_t limbs = n.empty() ? 0 : n[0].size(), bones = c.empty() ? 0 : c[0].size();
    struct Cost {
        float d;
        size_t l, b;
    };
    std::vector<Cost> costs;
    for (size_t l = 1; l < limbs; l++) {
        for (size_t b = 1; b < bones; b++) {
            float d = 0;
            for (int s = 0; s < kSamples; s++) {
                d += (n[s][l] - c[s][b] * scale).Length();
            }
            costs.push_back({ d / kSamples, l, b });
        }
    }
    std::sort(costs.begin(), costs.end(), [](const Cost& a, const Cost& b) { return a.d < b.d; });
    std::vector<int> map(limbs, -1);
    std::vector<bool> used(bones, false);
    if (limbs && bones) {
        map[0] = 0;
        used[0] = true;
    }
    float sum = 0;
    size_t count = 0;
    for (const Cost& k : costs) {
        if (map[k.l] >= 0 || used[k.b]) {
            continue;
        }
        map[k.l] = static_cast<int>(k.b);
        used[k.b] = true;
        sum += k.d;
        count++;
    }
    error = count ? sum / count : 1e9f;
    return map;
}

// How differently two animations move: the angle between corresponding limb directions (parent joint -> joint,
// independent of Grezzo's retuned proportions) plus half the position error, both averaged over the samples.
float MotionError(const Samples& n, const Samples& c, const std::vector<int>& map, const std::vector<int>& parents,
                  float scale, float height) {
    float angle = 0, pos = 0;
    size_t angles = 0, positions = 0;
    for (int s = 0; s < kSamples; s++) {
        for (size_t l = 1; l < map.size(); l++) {
            if (map[l] < 0 || size_t(map[l]) >= c[s].size()) {
                continue;
            }
            pos += (n[s][l] - c[s][map[l]] * scale).Length();
            positions++;
            const int p = parents[l];
            if (p < 0 || map[p] < 0) {
                continue;
            }
            const Vec3 dn = n[s][l] - n[s][p], dc = c[s][map[l]] - c[s][map[p]];
            if (dn.Length() < 1e-3f || dc.Length() < 1e-3f) {
                continue;
            }
            const float cosA = std::clamp(dn.Normalized().Dot(dc.Normalized()), -1.0f, 1.0f);
            angle += std::acos(cosA) / 3.14159265f;
            angles++;
        }
    }
    const float a = angles ? angle / angles : 0.5f;
    const float d = positions ? pos / positions / height : 1.0f;
    return a + 0.5f * d;
}

bool FramesMatch(int n64Frames, uint32_t csabDuration) {
    return std::abs(int(csabDuration) - n64Frames) <= 1;
}

} // namespace

AnimPairing PairAnimations(const VanillaSkeleton& skel, const std::vector<VanillaAnimation>& n64, const Cmb& cmb,
                           const std::vector<AnimCandidate>& csabs) {
    AnimPairing out;
    std::vector<Samples> ns, cs;
    for (const auto& a : n64) {
        ns.push_back(SampleN64(skel, a));
    }
    for (const auto& c : csabs) {
        cs.push_back(SampleCsab(cmb, *c.csab));
    }
    if (ns.empty() || cs.empty()) {
        for (const auto& a : n64) {
            out.pairs.push_back({ a.path, a.frameCount, -1, 1e9f, false });
        }
        return out;
    }
    // Scale 3DS positions to the N64's size (Grezzo sometimes retuned proportions).
    float n64Size = 0, c3dsSize = 0;
    for (const auto& s : ns) {
        n64Size = std::max(n64Size, Extent(s));
    }
    for (const auto& s : cs) {
        c3dsSize = std::max(c3dsSize, Extent(s));
    }
    const float scale = n64Size / std::max(1.0f, c3dsSize);
    const float height = n64Size;

    // The limb -> bone correspondence: identity when the rigs are the same, else from the best anchor pair.
    std::vector<int> parents;
    for (const auto& b : cmb.bones) {
        parents.push_back(b.parent);
    }
    if (parents == ParentsOf(skel)) {
        out.limbToBone.resize(skel.limbs.size());
        for (size_t i = 0; i < skel.limbs.size(); i++) {
            out.limbToBone[i] = static_cast<int>(i);
        }
    } else {
        std::map<int, int> n64Count, csabCount;
        for (const auto& a : n64) {
            n64Count[a.frameCount]++;
        }
        for (const auto& c : csabs) {
            csabCount[int(c.csab->duration)]++;
        }
        struct Anchor {
            int uniqueness;
            size_t a, c;
        };
        std::vector<Anchor> anchors;
        for (size_t a = 0; a < n64.size(); a++) {
            for (size_t c = 0; c < csabs.size(); c++) {
                if (FramesMatch(n64[a].frameCount, csabs[c].csab->duration)) {
                    anchors.push_back({ n64Count[n64[a].frameCount] + csabCount[int(csabs[c].csab->duration)], a, c });
                }
            }
        }
        if (anchors.empty()) {
            for (size_t a = 0; a < n64.size(); a++) {
                for (size_t c = 0; c < csabs.size(); c++) {
                    anchors.push_back({ 100, a, c });
                }
            }
        }
        std::sort(anchors.begin(), anchors.end(), [](const Anchor& x, const Anchor& y) {
            return x.uniqueness < y.uniqueness;
        });
        if (anchors.size() > 24) {
            anchors.resize(24);
        }
        float best = 1e30f;
        for (const Anchor& an : anchors) {
            float err = 0;
            auto map = Correspond(ns[an.a], cs[an.c], scale, err);
            if (err < best) {
                best = err;
                out.limbToBone = std::move(map);
            }
        }
        out.correspondenceError = best / height;
    }

    // Cost of every pairing.
    const std::vector<int> limbParents = ParentsOf(skel);
    const size_t na = n64.size(), nc = csabs.size();
    std::vector<std::vector<float>> cost(na, std::vector<float>(nc));
    for (size_t a = 0; a < na; a++) {
        for (size_t c = 0; c < nc; c++) {
            const bool match = FramesMatch(n64[a].frameCount, csabs[c].csab->duration);
            const float frameRatio = float(csabs[c].csab->duration) / std::max(1, int(n64[a].frameCount));
            const float penalty = match ? 0.0f : 0.02f + 0.05f * std::fabs(std::log(std::max(frameRatio, 0.01f)));
            cost[a][c] = MotionError(ns[a], cs[c], out.limbToBone, limbParents, scale, height) + penalty;
        }
    }
    // Grezzo ported each animation once, so pair one to one (minimum total cost); N64 animations left over when the
    // 3DS has fewer clips then take their best match.
    const std::vector<int> assigned = AssignMinCost(cost);
    for (size_t a = 0; a < na; a++) {
        AnimPair p;
        p.n64Path = n64[a].path;
        p.n64Frames = n64[a].frameCount;
        int c = assigned[a];
        if (c < 0) {
            c = static_cast<int>(std::min_element(cost[a].begin(), cost[a].end()) - cost[a].begin());
        }
        p.csab = c;
        p.score = cost[a][c];
        p.frameCountMatches = FramesMatch(n64[a].frameCount, csabs[c].csab->duration);
        out.pairs.push_back(p);
    }
    return out;
}

std::vector<int> AssignMinCost(const std::vector<std::vector<float>>& cost) {
    // Hungarian algorithm (Kuhn-Munkres, potentials form) on a square matrix padded with a high cost.
    const size_t rows = cost.size(), cols = rows ? cost[0].size() : 0;
    const size_t n = std::max(rows, cols);
    const double pad = 1e6;
    auto c = [&](size_t i, size_t j) -> double { return (i < rows && j < cols) ? cost[i][j] : pad; };
    std::vector<double> u(n + 1, 0), v(n + 1, 0);
    std::vector<size_t> p(n + 1, 0), way(n + 1, 0);
    for (size_t i = 1; i <= n; i++) {
        p[0] = i;
        size_t j0 = 0;
        std::vector<double> minv(n + 1, 1e18);
        std::vector<bool> used(n + 1, false);
        do {
            used[j0] = true;
            const size_t i0 = p[j0];
            double delta = 1e18;
            size_t j1 = 0;
            for (size_t j = 1; j <= n; j++) {
                if (used[j]) {
                    continue;
                }
                const double cur = c(i0 - 1, j - 1) - u[i0] - v[j];
                if (cur < minv[j]) {
                    minv[j] = cur;
                    way[j] = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = j;
                }
            }
            for (size_t j = 0; j <= n; j++) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const size_t j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }
    std::vector<int> out(rows, -1);
    for (size_t j = 1; j <= n; j++) {
        if (p[j] >= 1 && p[j] - 1 < rows && j - 1 < cols) {
            out[p[j] - 1] = static_cast<int>(j - 1);
        }
    }
    return out;
}

} // namespace Oot3d
