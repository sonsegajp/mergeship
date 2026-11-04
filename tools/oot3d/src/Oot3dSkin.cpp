#include "Oot3dSkin.h"

#include "Oot3dAlign.h"
#include "Oot3dAnim.h"
#include "Oot3dAnimPair.h"
#include "Oot3dCmab.h"
#include "Oot3dMaps.h"
#include "Oot3dMatch.h"
#include "Oot3dMesh.h"
#include "Oot3dN64Texture.h"
#include "Oot3dRom.h"
#include "Oot3dRomIndex.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZar.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>
#include <optional>
#include <set>

namespace Oot3d {

namespace {

constexpr uint32_t kTypeBlob = 0x4F424C42; // OBLB

void Str(Writer& w, const std::string& s) {
    w.U32(static_cast<uint32_t>(s.size()));
    w.Raw(s.data(), s.size());
}

// The 3DS model standing in for a vanilla skeleton: the same hierarchy if one has it, else the one most of the
// archive's animations were made for, else the one with the most bones.
const Cmb* PickModel(const VanillaSkeleton& skel, const std::vector<std::pair<std::string, Cmb>>& cmbs,
                     const std::vector<std::pair<std::string, Csab>>& csabs, std::string& name) {
    const auto parents = ParentsOf(skel);
    const Cmb* best = nullptr;
    size_t bestFit = 0;
    for (const auto& [n, c] : cmbs) {
        if (c.bones.size() < 2) {
            continue;
        }
        std::vector<int> cp;
        for (const auto& b : c.bones) {
            cp.push_back(b.parent);
        }
        size_t fit = c.bones.size();
        for (const auto& [an, a] : csabs) {
            fit += 1000 * (a.boneToNode.size() == c.bones.size());
        }
        if (cp == parents) {
            fit += 10000000;
        }
        if (!best || fit > bestFit) {
            best = &c;
            name = n;
            bestFit = fit;
        }
    }
    return best;
}

// A model's folder inside its archive ("door/model/door_model.cmb" -> "door"; "Model/x.cmb" -> ""): archives that
// group several props (zelda_keep) keep each one's clips beside it.
std::string FolderOf(const std::string& path) {
    const size_t a = path.find('/');
    if (a == std::string::npos || path.find('/', a + 1) == std::string::npos) {
        return "";
    }
    return ToLower(path.substr(0, a));
}

// Joint offset lengths, sorted and normalized to their total, as a cumulative curve at 16 points.
std::array<float, 16> LengthCurve(std::vector<float> lengths, float& total) {
    std::sort(lengths.begin(), lengths.end());
    total = 0;
    for (float l : lengths) {
        total += l;
    }
    std::array<float, 16> out{};
    if (lengths.empty() || total <= 0) {
        return out;
    }
    std::vector<float> cum(lengths.size());
    float acc = 0;
    for (size_t i = 0; i < lengths.size(); i++) {
        acc += lengths[i] / total;
        cum[i] = acc;
    }
    for (int k = 0; k < 16; k++) {
        const float x = (k + 0.5f) / 16.0f * lengths.size();
        out[k] = cum[std::min(lengths.size() - 1, size_t(x))];
    }
    return out;
}

// Cost of standing a 3DS model in for a vanilla skeleton (lower is better; above kRigReject: not this character).
constexpr float kRigReject = 1.2f;
float RigCost(const VanillaSkeleton& skel, const std::vector<int>& parents, const std::string& modelPath,
              const Cmb& model, bool hasClips, bool wantsClips, float color, std::string* why = nullptr) {
    std::vector<float> ln, lm;
    for (size_t l = 1; l < skel.limbs.size(); l++) {
        ln.push_back(Vec3(skel.limbs[l].trans[0], skel.limbs[l].trans[1], skel.limbs[l].trans[2]).Length());
    }
    std::vector<int> mp;
    for (size_t b = 0; b < model.bones.size(); b++) {
        mp.push_back(model.bones[b].parent);
        if (b > 0) {
            lm.push_back(model.bones[b].translation.Length());
        }
    }
    float tn = 0, tm = 0;
    const auto cn = LengthCurve(ln, tn), cm = LengthCurve(lm, tm);
    float shape = 0;
    for (int k = 0; k < 16; k++) {
        shape += std::fabs(cn[k] - cm[k]);
    }
    shape /= 16.0f;
    const float bones = std::fabs(std::log(float(model.bones.size()) / float(std::max<size_t>(1, skel.limbs.size()))));
    const float scale = std::min(3.0f, std::fabs(std::log(std::max(1.0f, tm) / std::max(1.0f, tn))));
    const float name = NameMatch(NameTokens(skel.path), modelPath);
    // Grezzo's rigs often have far fewer bones than the N64's, but the size and colors carry over.
    float cost = 0.3f * bones + shape + scale + 0.5f * color;
    if (mp == parents) {
        cost -= 0.3f;
    }
    cost -= 0.5f * name;
    if (wantsClips && !hasClips) {
        cost += 0.4f;
    }
    if (why) {
        char buf[200];
        snprintf(buf, sizeof(buf), "%s %.2f (bones %zu/%zu, shape %.2f, scale %.2f, color %.2f%s, name %.2f%s)",
                 modelPath.c_str(), cost, model.bones.size(), skel.limbs.size(), shape, scale, color,
                 mp == parents ? ", same hierarchy" : "", name, wantsClips && !hasClips ? ", no clips" : "");
        *why = buf;
    }
    return cost;
}

// The animations actor sources play on each vanilla skeleton, including other objects' (tools/oot3d/gen_anim_table.py).
struct AnimTableEntry {
    const char* skeleton;
    std::vector<const char*> anims;
};
const std::vector<AnimTableEntry>& AnimTable() {
    static const std::vector<AnimTableEntry> kTable = {
#include "Oot3dAnimTable.inc"
    };
    return kTable;
}

// How far a 3DS rig's bind translations are from the N64 skeleton's joint offsets (0 = the same proportions).
float JointDistance(const VanillaSkeleton& skel, const RomModel& m) {
    double diff = 0, size = 0;
    for (size_t l = 1; l < skel.limbs.size() && l < m.boneTrans.size(); l++) {
        const Vec3 n(skel.limbs[l].trans[0], skel.limbs[l].trans[1], skel.limbs[l].trans[2]);
        diff += (n - m.boneTrans[l]).Length();
        size += n.Length();
    }
    return float(diff / std::max(1.0, size));
}

// Display lists the actors using each vanilla skeleton reference (tools/oot3d/gen_anim_table.py).
const std::map<std::string, std::vector<std::string>>& DlTable() {
    static const std::map<std::string, std::vector<std::string>> kTable = [] {
        const std::vector<AnimTableEntry> entries = {
#include "Oot3dDlTable.inc"
        };
        std::map<std::string, std::vector<std::string>> out;
        for (const AnimTableEntry& e : entries) {
            for (const char* d : e.anims) {
                out[e.skeleton].push_back(d);
            }
        }
        return out;
    }();
    return kTable;
}

} // namespace

const std::set<std::string>& ActorReferencedLists() {
    static const std::set<std::string> kLists = [] {
        std::set<std::string> out;
        for (const auto& [skel, lists] : DlTable()) {
            out.insert(lists.begin(), lists.end());
        }
        return out;
    }();
    return kLists;
}

namespace {

// Meshes of a 3DS model shown only while the vanilla actor draws a given display list. Where the N64 swaps a limb's
// list (En_Hy's sixteen heads on eight bodies, Saria's hand holding the ocarina) or draws an extra list from a limb
// callback (a hair piece, a held prop), Grezzo kept every alternative in the one model and shows the one the game
// picks. Both sides are posed by the same paired animation and each mesh is fitted onto the candidate lists (the
// limb's own and those the actor sources name, on that limb or the limbs above it):
//  1. whole alternatives (a complete head fitting a list both ways) are assigned one to one;
//  2. an eye or mouth mesh goes with the alternative whose list takes its eyes from the actor (segment textures);
//  3. a part fitting one list clearly best (the ocarina, a hair piece the actor draws on the chest) is that list's;
//  4. a part inside an assigned alternative's bounds goes with it;
//  5. anything else always shows (a mesh fitting two or more lists near-exactly is shared, like the hand under the
//     ocarina; a body part on the limb matches none), except a whole alternative no list took (a head no vanilla type
//     uses), which never does.
// A limb only gets variants with an alternative to its own list and with its own list still showing something.
const char* const kNeverShown = "<none>";
constexpr bool kAttachProps = false;
constexpr int kActorDrawn = -2; // condition limb: the actor draws the list itself during the skeleton draw

struct Variants {
    std::vector<std::pair<int, std::string>> conditions{ { -1, "" } }; // tag -> (limb, list); tag 0: always
    std::map<int, int> meshTag;                                       // CMB mesh index -> tag
};

Variants FindVariants(const VanillaSkeleton& skel, const Cmb& model, const std::vector<int>& limbToBone,
                      const VanillaAnimation* n64Anim, const Csab* csab, const ZipReader& oot,
                      const std::function<void(LogLevel, const std::string&)>& log) {
    Variants out;
    auto tit = DlTable().find(skel.path);
    if (tit == DlTable().end() || !n64Anim || !csab) {
        return out;
    }
    std::set<std::string> ownLists;
    for (const VanillaLimb& l : skel.limbs) {
        ownLists.insert(l.dList);
    }
    const auto bind = model.BindPose();
    std::vector<Mat34> inv;
    for (const auto& w : bind) {
        inv.push_back(w.Inverse());
    }
    // Both sides posed by the same motion (the best paired animations, first frame), the 3DS at the N64's size.
    const std::vector<Mat34> n64Pose = N64Pose(skel, *n64Anim, 0);
    const std::vector<Mat34> posed = CsabPose(model, *csab, 0.0f);
    float n64Extent = 0, extent3ds = 0;
    for (const Mat34& m : n64Pose) {
        n64Extent = std::max(n64Extent, m.Translation().Length());
    }
    for (const Mat34& m : posed) {
        extent3ds = std::max(extent3ds, m.Translation().Length());
    }
    const float scale = extent3ds > 1.0f ? n64Extent / extent3ds : 1.0f;
    std::vector<int> boneLimb(model.bones.size(), -1);
    for (size_t l = 0; l < limbToBone.size(); l++) {
        if (limbToBone[l] >= 0 && size_t(limbToBone[l]) < boneLimb.size()) {
            boneLimb[limbToBone[l]] = static_cast<int>(l);
        }
    }
    auto limbOfBone = [&](int b) {
        while (b >= 0 && boneLimb[b] < 0) {
            b = model.bones[b].parent;
        }
        return b >= 0 ? boneLimb[b] : -1;
    };
    struct MeshInfo {
        std::vector<Vec3> pts; // posed, N64 scale
        std::map<int, float> boneWeight;
        bool eyeLike = false; // an eye or mouth texture (the N64 swaps those at runtime)
    };
    std::map<int, MeshInfo> meshes;
    for (const CmbTriangleSet& set : model.Decode()) {
        MeshInfo& mi = meshes[set.mesh];
        for (const CmbVertex& v : set.vertices) {
            mi.pts.push_back(SkinPosition(v, set.skinning, posed, inv) * scale);
            if (set.skinning == Skinning::Smooth) {
                for (int k = 0; k < v.boneCount && k < 4; k++) {
                    mi.boneWeight[v.bones[k]] += std::max(0.0f, v.weights[k]);
                }
            } else {
                mi.boneWeight[v.bones[0]] += 1.0f;
            }
        }
        if (set.material < int(model.materials.size())) {
            const int ti = model.materials[set.material].bindings[0].texture;
            if (ti >= 0 && size_t(ti) < model.textures.size()) {
                const std::string n = ToLower(model.textures[ti].name);
                mi.eyeLike |= n.find("eye") != std::string::npos || n.find("mouth") != std::string::npos;
            }
        }
    }
    std::map<int, std::vector<int>> limbMeshes;
    for (const auto& [m, mi] : meshes) {
        int bone = -1;
        float best = 0;
        for (const auto& [b, w] : mi.boneWeight) {
            if (w > best) {
                best = w;
                bone = b;
            }
        }
        const int l = limbOfBone(bone);
        if (l >= 0) {
            limbMeshes[l].push_back(m);
        }
    }
    std::map<std::string, VanillaDlInfo> infoCache;
    auto dlInfo = [&](const std::string& path) -> const VanillaDlInfo& {
        auto it = infoCache.find(path);
        if (it == infoCache.end()) {
            it = infoCache.emplace(path, InspectVanillaDl(oot, path, true)).first;
        }
        return it->second;
    };
    auto dlPoints = [&](const std::string& path, int l) {
        std::vector<Vec3> pts;
        for (const Vec3& p : dlInfo(path).positions) {
            pts.push_back(n64Pose[l].Point(p));
        }
        return pts;
    };
    auto boxOf = [](const std::vector<Vec3>& pts, Vec3& mn, Vec3& mx) {
        mn = Vec3(1e9f, 1e9f, 1e9f);
        mx = Vec3(-1e9f, -1e9f, -1e9f);
        for (const Vec3& p : pts) {
            mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
            mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
        }
    };
    auto diagOf = [&](const std::vector<Vec3>& pts) {
        Vec3 mn, mx;
        boxOf(pts, mn, mx);
        return pts.empty() ? 0.0f : (mx - mn).Length();
    };
    auto overlap = [&](const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
        Vec3 amn, amx, bmn, bmx;
        boxOf(a, amn, amx);
        boxOf(b, bmn, bmx);
        const float ix = std::max(0.0f, std::min(amx.x, bmx.x) - std::max(amn.x, bmn.x));
        const float iy = std::max(0.0f, std::min(amx.y, bmx.y) - std::max(amn.y, bmn.y));
        const float iz = std::max(0.0f, std::min(amx.z, bmx.z) - std::max(amn.z, bmn.z));
        const float inter = ix * iy * iz;
        const Vec3 sa = amx - amn, sb = bmx - bmn;
        const float uni = sa.x * sa.y * sa.z + sb.x * sb.y * sb.z - inter;
        return uni > 0 ? inter / uni : 0.0f;
    };
    const std::string objectDir = skel.path.substr(0, skel.path.find_last_of('/') + 1);
    for (const auto& [l, ms] : limbMeshes) {
        if (size_t(l) >= n64Pose.size()) {
            continue;
        }
        const std::string& own = skel.limbs[l].dList;
        std::vector<std::string> cands;
        if (!own.empty()) {
            cands.push_back(own);
        }
        for (const std::string& d : tit->second) {
            if (d != own && !ownLists.count(d) && StartsWith(d, objectDir)) {
                cands.push_back(d);
            }
        }
        if (cands.size() < 2) {
            continue;
        }
        std::vector<Vec3> limbPts;
        for (int m : ms) {
            limbPts.insert(limbPts.end(), meshes[m].pts.begin(), meshes[m].pts.end());
        }
        const float limbDiag = diagOf(limbPts);
        // Candidate placements: a list on this limb (it replaces the limb's own list, so it sits where that one
        // does), or drawn by the actor on this limb or one of the three above it.
        const std::vector<Vec3> ownPts = own.empty() ? std::vector<Vec3>{} : dlPoints(own, l);
        struct Placement {
            std::string list;
            int limb;       // where it is drawn
            bool override;  // replaces the limb's own list (else: drawn by the actor)
            std::vector<Vec3> pts;
        };
        std::vector<Placement> places;
        std::vector<int> above;
        for (int a = skel.limbs[l].parent, n = 0; a >= 0 && n < 3; a = skel.limbs[a].parent, n++) {
            above.push_back(a);
        }
        Vec3 lmn, lmx;
        boxOf(limbPts, lmn, lmx);
        for (const std::string& d : cands) {
            if (d == own) {
                if (ownPts.size() >= 3) {
                    places.push_back({ d, l, false, ownPts });
                }
                continue;
            }
            std::vector<Vec3> here = dlPoints(d, l);
            const float dg = diagOf(here);
            if (getenv("OOT3D_VARIANT_DEBUG")) {
                char b2[240];
                snprintf(b2, sizeof(b2), "    candidate %s: %zu points, size %.0f (limb meshes %.0f)", d.c_str(),
                         here.size(), dg, limbDiag);
                log(LogLevel::Detail, b2);
            }
            if (here.size() < 3 || dg < 0.1f * limbDiag || dg > 3.0f * limbDiag) {
                continue;
            }
            // As an override: on any limb where it could replace the limb's list (the limb has none, or it sits
            // where that one does) and lands near these meshes.
            for (size_t l2 = 0; l2 < skel.limbs.size() && l2 < n64Pose.size(); l2++) {
                std::vector<Vec3> pts = int(l2) == l ? here : dlPoints(d, int(l2));
                const std::string& own2 = skel.limbs[l2].dList;
                if (!own2.empty()) {
                    const std::vector<Vec3> o2 = int(l2) == l ? ownPts : dlPoints(own2, int(l2));
                    if (o2.size() >= 3 && overlap(pts, o2) < 0.25f) {
                        continue;
                    }
                }
                if (overlap(pts, limbPts) < 0.05f) {
                    continue;
                }
                places.push_back({ d, int(l2), true, std::move(pts) });
            }
            // As drawn by the actor: on this limb or one of the three above it.
            places.push_back({ d, l, false, here });
            for (int a : above) {
                places.push_back({ d, a, false, dlPoints(d, a) });
            }
        }
        size_t overrideLists = 0;
        std::set<std::string> distinct;
        for (const Placement& p : places) {
            distinct.insert(p.list);
            overrideLists += p.override || p.list == own;
        }
        if (distinct.size() < 2) {
            continue;
        }
        // Each mesh against each placement: a short refinement from the posed placement, then how far the mesh is
        // from the list (a part) and the list from the mesh (a whole alternative).
        struct Fit {
            float part = 1e9f, whole = 1e9f;
        };
        std::map<int, std::vector<Fit>> fit;
        for (int m : ms) {
            if (meshes[m].pts.size() < 3) {
                continue;
            }
            for (const Placement& p : places) {
                const float dg = std::max(1.0f, diagOf(p.pts));
                float e = 0;
                const Mat34 r = Refine(meshes[m].pts, p.pts, Mat34::Identity(), 8, e);
                Fit f;
                f.part = e / dg;
                if (p.override || (p.list == own && p.limb == l)) {
                    f.whole = 0.5f * (e + MeanNearest(Subsample(p.pts, 600), Subsample(meshes[m].pts, 600), r.Inverse())) / dg;
                }
                fit[m].push_back(f);
                if (getenv("OOT3D_VARIANT_DEBUG")) {
                    char b2[240];
                    snprintf(b2, sizeof(b2), "      mesh %d ~ %s @%d%s part %.3f whole %.3f", m, p.list.c_str(),
                             p.limb, p.override ? " (override)" : "", f.part, f.whole);
                    log(LogLevel::Detail, b2);
                }
            }
        }
        auto condOf = [&](size_t placement) -> std::pair<int, std::string> {
            const Placement& p = places[placement];
            if (p.override) {
                return { p.limb, p.list };
            }
            return { p.list == own ? l : kActorDrawn, p.list };
        };
        auto fitsTightly = [&](int m) {
            std::set<std::string> lists;
            for (size_t j = 0; j < fit[m].size(); j++) {
                if (fit[m][j].part < 0.035f) {
                    lists.insert(places[j].list);
                }
            }
            return lists.size() >= 2;
        };
        // 1. whole alternatives
        std::vector<int> wholeMeshes;
        for (const auto& [m, fs] : fit) {
            float best = 1e9f;
            for (const Fit& f : fs) {
                best = std::min(best, f.whole);
            }
            if (best < 0.08f && !fitsTightly(m) && !meshes[m].eyeLike) {
                wholeMeshes.push_back(m);
            }
        }
        std::map<int, std::pair<int, std::string>> cond;
        std::set<int> assignedMeshes;
        if (!wholeMeshes.empty()) {
            std::vector<std::vector<float>> cost(wholeMeshes.size(), std::vector<float>(places.size()));
            for (size_t i = 0; i < wholeMeshes.size(); i++) {
                for (size_t j = 0; j < places.size(); j++) {
                    cost[i][j] = fit[wholeMeshes[i]][j].whole;
                }
            }
            const std::vector<int> a = AssignMinCost(cost);
            for (size_t i = 0; i < wholeMeshes.size(); i++) {
                if (a[i] >= 0 && cost[i][a[i]] < 0.08f) {
                    cond[wholeMeshes[i]] = condOf(size_t(a[i]));
                    assignedMeshes.insert(wholeMeshes[i]);
                }
            }
        }
        auto inside = [&](int part, int host) {
            Vec3 pmn, pmx, hmn, hmx;
            boxOf(meshes[part].pts, pmn, pmx);
            boxOf(meshes[host].pts, hmn, hmx);
            const Vec3 slack = (hmx - hmn) * 0.05f;
            return pmn.x >= hmn.x - slack.x && pmn.y >= hmn.y - slack.y && pmn.z >= hmn.z - slack.z &&
                   pmx.x <= hmx.x + slack.x && pmx.y <= hmx.y + slack.y && pmx.z <= hmx.z + slack.z;
        };
        for (const auto& [m, fs] : fit) {
            if (cond.count(m)) {
                continue;
            }
            // 2. eyes and mouths
            if (meshes[m].eyeLike) {
                std::vector<int> hosts;
                for (int w : assignedMeshes) {
                    if (inside(m, w) && dlInfo(cond[w].second).segmentTextures) {
                        hosts.push_back(w);
                    }
                }
                if (hosts.size() == 1) {
                    cond[m] = cond[hosts[0]];
                }
                continue;
            }
            // 3. a part of one list
            std::vector<std::pair<float, size_t>> order;
            for (size_t j = 0; j < fs.size(); j++) {
                order.emplace_back(fs[j].part, j);
            }
            std::sort(order.begin(), order.end());
            float second = 1e9f;
            for (const auto& [e, j] : order) {
                if (places[j].list != places[order[0].second].list) {
                    second = e;
                    break;
                }
            }
            if (order[0].first < 0.06f && second > 1.5f * order[0].first + 0.01f) {
                cond[m] = condOf(order[0].second);
                continue;
            }
            // 4. inside an assigned alternative
            int host = -1;
            for (int w : assignedMeshes) {
                if (inside(m, w) && (host < 0 || meshes[w].pts.size() < meshes[host].pts.size())) {
                    host = w;
                }
            }
            const bool whole = std::find(wholeMeshes.begin(), wholeMeshes.end(), m) != wholeMeshes.end();
            if (host >= 0 && !whole && !fitsTightly(m)) {
                cond[m] = cond[host];
                continue;
            }
            // 5. shared, or anything else (a body part on this limb): always; only a whole alternative no list took
            // (a head no vanilla type uses) never shows
            if (!fitsTightly(m) && std::find(wholeMeshes.begin(), wholeMeshes.end(), m) != wholeMeshes.end()) {
                cond[m] = { l, kNeverShown };
            }
        }
        bool hasAlternative = false, ownShows = own.empty();
        for (const auto& [m, c] : cond) {
            hasAlternative |= c.second != own && c.second != kNeverShown;
            ownShows |= c.second == own;
        }
        if (!own.empty() && !ownShows) {
            for (int m : ms) {
                ownShows |= !cond.count(m); // always-on meshes still show with the own list
            }
        }
        if (own.empty() && assignedMeshes.size() < 2) {
            hasAlternative = false; // nothing to check the lists' place against: only a set of alternatives counts
        }
        char head[200];
        snprintf(head, sizeof(head), "  limb %d (own %s): ", l, own.empty() ? "none" : own.c_str());
        if (!hasAlternative || !ownShows) {
            if (!cond.empty()) {
                log(LogLevel::Detail, std::string(head) + "no variants (" +
                                          (!hasAlternative ? "no alternative" : "own list would show nothing") + ")");
            }
            continue;
        }
        for (const auto& [m, c] : cond) {
            log(LogLevel::Detail, std::string(head) + "variant mesh " + std::to_string(m) + " <- " +
                                      (c.first == kActorDrawn ? "drawn by the actor: "
                                                              : "limb " + std::to_string(c.first) + " draws ") +
                                      c.second);
            int tag = -1;
            for (size_t t = 0; t < out.conditions.size(); t++) {
                if (out.conditions[t] == c) {
                    tag = static_cast<int>(t);
                }
            }
            if (tag < 0) {
                tag = static_cast<int>(out.conditions.size());
                out.conditions.push_back(c);
            }
            out.meshTag[m] = tag;
        }
    }
    return out;
}

// Faces: a material whose texture a CMAB palette swaps (the eye and mouth textures), with Grezzo's own track for it
// and the N64 textures the actor puts in a segment for the same states.
struct FacePalette {
    SkinPalette pal;
    CmabTrack track;
    uint32_t duration = 0;
    bool loop = true;
    std::vector<std::string> names;
    std::vector<std::pair<std::string, int>> n64; // N64 texture -> palette index
};

// The actors' eye and mouth texture arrays (tools/oot3d/gen_face_table.py).
struct FaceArray {
    char kind; // 'e' eyes, 'm' mouth
    std::vector<const char*> paths;
};
const std::vector<FaceArray>& FaceTable() {
    static const std::vector<FaceArray> kTable = {
#include "Oot3dFaceTable.inc"
    };
    return kTable;
}

// The CMABs animating a model (named after it: <model>.cmab, <model>_eye.cmab, <model>_mouth.cmab, ...), the first
// archive having one winning.
std::vector<Cmab> ModelCmabs(const RomIndex& index, const std::vector<std::string>& archives, const std::string& model) {
    std::vector<Cmab> out;
    std::set<std::string> seen;
    const std::string stem = ToLower(StemOf(model));
    for (const std::string& z : archives) {
        const RomIndex::Archive* a = index.Load(z);
        if (!a || !a->zar) {
            continue;
        }
        for (const ZarFile& f : a->zar->files) {
            const std::string n = ToLower(f.name);
            if (!EndsWith(n, ".cmab")) {
                continue;
            }
            const std::string cs = ToLower(StemOf(n));
            if ((cs != stem && !StartsWith(cs, stem + "_")) || !seen.insert(cs).second) {
                continue;
            }
            try {
                out.push_back(Cmab::Parse(f.data));
            } catch (const std::exception&) {
            }
        }
    }
    return out;
}

// Hand-curated visibility (Oot3dSkinCuration.inc) replaces what FindVariants worked out for a skeleton.
struct CuratedMesh {
    int mesh;
    const char* limbList; // "" always/never, "@actor" drawn by the actor, else the limb's own list
    const char* show;     // "" always, "-" never, else the list
};
struct CuratedSkeleton {
    const char* skeleton;
    std::vector<CuratedMesh> meshes;
};
const std::vector<CuratedSkeleton>& Curation() {
    static const std::vector<CuratedSkeleton> kTable = {
#include "Oot3dSkinCuration.inc"
    };
    return kTable;
}

void ApplyCuration(const VanillaSkeleton& skel, Variants& v,
                   const std::function<void(LogLevel, const std::string&)>& log) {
    const CuratedSkeleton* cs = nullptr;
    for (const CuratedSkeleton& c : Curation()) {
        if (skel.path == c.skeleton) {
            cs = &c;
        }
    }
    if (!cs) {
        return;
    }
    Variants out;
    for (const CuratedMesh& m : cs->meshes) {
        std::pair<int, std::string> cond;
        if (!*m.show) {
            continue; // always: tag 0
        } else if (std::string(m.show) == "-") {
            cond = { 0, kNeverShown }; // a real limb that never draws "<none>" (limb -1 means always)
        } else if (std::string(m.limbList) == "@actor") {
            cond = { kActorDrawn, m.show };
        } else {
            int limb = -1;
            for (size_t l = 0; l < skel.limbs.size(); l++) {
                if (skel.limbs[l].dList == m.limbList) {
                    limb = static_cast<int>(l);
                }
            }
            if (limb < 0) {
                log(LogLevel::Warning, "curation: " + skel.path + " has no limb drawing " + m.limbList);
                continue;
            }
            cond = { limb, m.show };
        }
        int tag = -1;
        for (size_t t = 0; t < out.conditions.size(); t++) {
            if (out.conditions[t] == cond) {
                tag = static_cast<int>(t);
            }
        }
        if (tag < 0) {
            tag = static_cast<int>(out.conditions.size());
            out.conditions.push_back(cond);
        }
        out.meshTag[m.mesh] = tag;
    }
    log(LogLevel::Detail, "  curated visibility (" + std::to_string(cs->meshes.size()) + " meshes)");
    v = std::move(out);
}

} // namespace

SkinResult ConvertSkins(const RomFS& rom, const RomIndex& index, const ZipReader& oot, ZipWriter& zip,
                        TextureStore& textures, const ExtractOptions& opt) {
    (void)rom;
    SkinResult res;
    auto log = [&](LogLevel l, const std::string& s) {
        if (opt.log) {
            opt.log(l, s);
        }
    };
    std::map<std::string, std::vector<std::string>> objects;
    for (const std::string& n : oot.Names()) {
        if (!StartsWith(n, "objects/")) {
            continue;
        }
        const size_t slash = n.find('/', 8);
        if (slash != std::string::npos) {
            objects[n.substr(8, slash - 8)].push_back(n);
        }
    }
    std::map<std::string, std::vector<std::string>> tableAnims;
    for (const AnimTableEntry& e : AnimTable()) {
        for (const char* a : e.anims) {
            tableAnims[e.skeleton].push_back(a);
        }
    }
    std::map<std::string, std::optional<VanillaAnimation>> animCache;
    auto readAnim = [&](const std::string& path) -> const VanillaAnimation* {
        auto it = animCache.find(path);
        if (it == animCache.end()) {
            it = animCache.emplace(path, ReadVanillaAnimation(oot, path)).first;
        }
        return it->second ? &*it->second : nullptr;
    };
    // Archives holding each model file name with each rig: the same character's other archives share its clips.
    std::map<std::pair<std::string, std::vector<int>>, std::set<std::string>> rigArchives;
    for (const RomModel& m : index.Models()) {
        rigArchives[{ ToLower(StemOf(m.name)), m.parents }].insert(m.zar);
    }

    const std::string prefix = opt.altAssets ? "alt/" : "";
    uint64_t done = 0;
    for (const auto& [object, names] : objects) {
        if (opt.cancel && *opt.cancel) {
            break;
        }
        if (opt.progress) {
            opt.progress("Characters", done, objects.size());
        }
        done++;
        if (StartsWith(object, "object_link_")) {
            continue; // Link has his own converter (player animations, equipment)
        }
        std::vector<VanillaSkeleton> skels;
        std::vector<std::string> ownAnims;
        for (const std::string& n : names) {
            if (const VanillaAnimation* a = readAnim(n)) {
                if (a->type == 0) {
                    ownAnims.push_back(n);
                }
            } else if (auto sk = ReadVanillaSkeleton(oot, n)) {
                skels.push_back(*sk);
            }
        }
        const std::vector<std::string> ownArchives = index.ArchivesFor(object);
        // N64 animations per skeleton: the object's own and those the actor sources play on it, that fit it.
        auto animsFor = [&](const VanillaSkeleton& skel) {
            std::set<std::string> animPaths(ownAnims.begin(), ownAnims.end());
            if (auto it = tableAnims.find(skel.path); it != tableAnims.end()) {
                animPaths.insert(it->second.begin(), it->second.end());
            }
            std::vector<VanillaAnimation> out;
            for (const std::string& ap : animPaths) {
                const VanillaAnimation* a = readAnim(ap);
                if (a && a->type == 0 && a->joints.size() == skel.limbs.size() + 1) {
                    out.push_back(*a);
                }
            }
            return out;
        };
        // The object's rigged 3DS models (with whether clips for their rig sit beside them).
        struct Choice {
            std::string zar, name;
            const Cmb* cmb;
            bool hasClips;
            std::optional<Vec3> color;
        };
        // Hand-checked rigs: the model is the character though the rig cost says otherwise (Volvagia's head: the 3DS
        // head carries the jaw differently; the cow's tail is part of the cow; Demo_Ik's one-limb armor pieces are
        // Grezzo's one-bone pieces with clips of their own).
        static const std::map<std::string, std::string> kForcedRig = {
            { "objects/object_fd/gVolvagiaHeadSkel", "valbasiahead.cmb" },
            { "objects/object_cow/gCowTailSkel", "cow.cmb" },
            { "objects/object_ik/object_ik_Skel_000C90", "front_armer_drop.cmb" },
            { "objects/object_ik/object_ik_Skel_000660", "rshoulderb_drop.cmb" },
            { "objects/object_ik/object_ik_Skel_000380", "lshouldera_drop.cmb" },
            { "objects/object_ik/object_ik_Skel_01EB40", "kabutol_damage_demo.cmb" },
            { "objects/object_ik/object_ik_Skel_01EE60", "kabutor_damage_demo.cmb" },
            { "objects/object_ik/object_ik_Skel_000F30", "frontarmer_damage_demo.cmb" },
            { "objects/object_ik/object_ik_Skel_000900", "backarmer_damage_demo.cmb" },
            // Both 29 bones: the Iron Knuckle every fight draws (EnIk_Init) is ironknack; the defeat skeleton plays
            // Nabooru's unmasking (func_80A7735C), Grezzo's naborknack_damage_demo.
            { "objects/object_ik/gIronKnuckleSkel", "ironknack.cmb" },
            { "objects/object_ik/gIronKnuckleDefeatSkel", "naborknack_damage_demo.cmb" },
        };
        std::vector<Choice> choices;
        for (const std::string& z : ownArchives) {
            const RomIndex::Archive* a = index.Load(z);
            if (!a) {
                continue;
            }
            for (const auto& [n, c] : a->cmbs) {
                bool forced = false;
                for (const auto& [sk, m] : kForcedRig) {
                    forced |= EndsWith(ToLower(n), m);
                }
                if (c.bones.size() < 2 && !forced) {
                    continue;
                }
                bool clips = false;
                for (const auto& [cn, cs] : a->csabs) {
                    clips |= cs.boneToNode.size() == c.bones.size() && FolderOf(cn) == FolderOf(n);
                }
                choices.push_back({ z, n, &c, clips, ModelColor(c) });
            }
        }
        std::vector<const VanillaSkeleton*> eligible;
        for (const VanillaSkeleton& skel : skels) {
            if (skel.type == 2 || skel.limbType == 3) {
                res.skipped++;
                log(LogLevel::Detail, "skin " + skel.path + " skipped (" +
                                          (skel.type == 2 ? "curve skeleton" : "skin-limb skeleton") + ", later)");
                continue;
            }
            eligible.push_back(&skel);
        }
        // One model per skeleton, the lowest total cost over the object; a model serves a second skeleton only at a
        // cost (the N64 sometimes has two skeletons for one 3DS character, e.g. Wolfos and White Wolfos).
        std::vector<std::vector<VanillaAnimation>> eligibleAnims;
        for (const VanillaSkeleton* sk : eligible) {
            eligibleAnims.push_back(animsFor(*sk));
        }
        std::vector<int> assigned(eligible.size(), -1);
        std::vector<float> assignedCost(eligible.size(), 1e9f);
        if (!choices.empty() && !eligible.empty()) {
            const size_t copies = eligible.size();
            std::vector<std::vector<float>> cost(eligible.size(), std::vector<float>(choices.size() * copies));
            for (size_t i = 0; i < eligible.size(); i++) {
                const std::vector<int> parents = ParentsOf(*eligible[i]);
                const std::optional<Vec3> skelColor = SkeletonColor(oot, *eligible[i]);
                std::vector<std::pair<float, std::string>> report;
                for (size_t c = 0; c < choices.size(); c++) {
                    std::string why;
                    const float color = skelColor && choices[c].color ? ColorDistance(*skelColor, *choices[c].color)
                                                                      : 0.3f;
                    const float base = RigCost(*eligible[i], parents, choices[c].name, *choices[c].cmb,
                                               choices[c].hasClips, !eligibleAnims[i].empty(), color, &why);
                    report.emplace_back(base, why);
                    for (size_t k = 0; k < copies; k++) {
                        cost[i][c * copies + k] = base + 0.35f * float(k);
                    }
                    // A second skeleton may share a model only as a variant of the first (the same rig, e.g. White
                    // Wolfos); a 3DS model that merges several N64 skeletons (Volvagia's head, the cow's tail) is
                    // not drawn twice.
                    bool variantOk = false;
                    for (size_t j = 0; j < eligible.size(); j++) {
                        if (j == i || ParentsOf(*eligible[j]) != parents) {
                            continue;
                        }
                        double diff = 0, size = 0;
                        for (size_t l = 1; l < parents.size(); l++) {
                            const auto& a = eligible[i]->limbs[l].trans;
                            const auto& b = eligible[j]->limbs[l].trans;
                            diff += Vec3(float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])).Length();
                            size += Vec3(a[0], a[1], a[2]).Length();
                        }
                        variantOk |= diff <= 0.1 * std::max(1.0, size);
                    }
                    if (!variantOk) {
                        for (size_t k = 1; k < copies; k++) {
                            cost[i][c * copies + k] = 1e6f;
                        }
                    }
                }
                std::sort(report.begin(), report.end(),
                          [](const auto& x, const auto& y) { return x.first < y.first; });
                log(LogLevel::Detail, "rig candidates for " + eligible[i]->path + ":");
                for (size_t r = 0; r < report.size() && r < 4; r++) {
                    log(LogLevel::Detail, "    " + report[r].second);
                }
            }
            const std::vector<int> a = AssignMinCost(cost);
            for (size_t i = 0; i < eligible.size(); i++) {
                if (a[i] >= 0 && cost[i][a[i]] < 1e5f) {
                    assigned[i] = a[i] / static_cast<int>(copies);
                    assignedCost[i] = cost[i][a[i]] - 0.35f * float(a[i] % copies);
                }
            }
        }
        for (size_t si = 0; si < eligible.size(); si++) {
            const VanillaSkeleton& skel = *eligible[si];
            const RomIndex::Archive* archive = nullptr;
            const Cmb* model = nullptr;
            std::string zarPath, modelName, how;
            bool onlyChoice = false;
            if (assigned[si] >= 0) {
                int withClips = 0;
                for (const Choice& c : choices) {
                    withClips += c.hasClips;
                }
                const Choice& c = choices[assigned[si]];
                std::vector<float> ln, lm;
                for (size_t l = 1; l < skel.limbs.size(); l++) {
                    ln.push_back(Vec3(skel.limbs[l].trans[0], skel.limbs[l].trans[1], skel.limbs[l].trans[2]).Length());
                }
                for (size_t b = 1; b < c.cmb->bones.size(); b++) {
                    lm.push_back(c.cmb->bones[b].translation.Length());
                }
                float tn = 0, tm = 0;
                LengthCurve(ln, tn);
                LengthCurve(lm, tm);
                // The object's one animated model, at the skeleton's size: the character, however re-rigged.
                onlyChoice = withClips == 1 && c.hasClips && eligible.size() == 1 &&
                             std::fabs(std::log(std::max(1.0f, tm) / std::max(1.0f, tn))) < 0.5f;
            }
            if (auto fr = kForcedRig.find(skel.path); fr != kForcedRig.end()) {
                for (size_t c = 0; c < choices.size(); c++) {
                    if (EndsWith(ToLower(choices[c].name), fr->second)) {
                        assigned[si] = static_cast<int>(c);
                        onlyChoice = true;
                    }
                }
            }
            if (assigned[si] >= 0 && (assignedCost[si] <= kRigReject || onlyChoice)) {
                const Choice& c = choices[assigned[si]];
                zarPath = c.zar;
                archive = index.Load(zarPath);
                model = c.cmb;
                modelName = c.name;
                char buf[64];
                snprintf(buf, sizeof(buf), "rig cost %.2f", assignedCost[si]);
                how = buf;
            } else if (assigned[si] >= 0) {
                log(LogLevel::Warning, "NO 3DS MODEL for skeleton " + skel.path + " (closest " +
                                           choices[assigned[si]].name + ", rig cost " +
                                           std::to_string(assignedCost[si]) + ")");
                res.skipped++;
                continue;
            }
            if (!model && ownArchives.empty()) {
                // No archive of its own: the same bone hierarchy with close proportions anywhere (simple chains are
                // common, so both are required).
                const std::vector<int> parents = ParentsOf(skel);
                const RomModel* pick = nullptr;
                float pickScore = 1e30f;
                for (const RomModel& m : index.Models()) {
                    if (m.parents != parents || m.parents.size() < 4) {
                        continue;
                    }
                    float score = JointDistance(skel, m);
                    if (score > 0.3f) {
                        continue;
                    }
                    if (m.zar == "actor/zelda_ec.zar") {
                        score += 0.5f; // the credits' copies, only when nothing else has the character
                    }
                    if (score < pickScore) {
                        pickScore = score;
                        pick = &m;
                    }
                }
                if (pick) {
                    zarPath = pick->zar;
                    archive = index.Load(zarPath);
                    model = archive ? archive->FindCmb(pick->name) : nullptr;
                    modelName = pick->name;
                    how = "same rig in another archive";
                }
            }
            if (!model) {
                res.skipped++;
                log(LogLevel::Warning, "NO 3DS MODEL for skeleton " + skel.path);
                continue;
            }
            std::vector<VanillaAnimation>& mine = eligibleAnims[si];
            // The 3DS clips: the model's archive and the other archives with the same character and rig.
            std::vector<AnimCandidate> cands;
            std::set<std::string> seenClip;
            std::set<std::string> clipArchives{ zarPath };
            {
                std::vector<int> mp;
                for (const CmbBone& b : model->bones) {
                    mp.push_back(b.parent);
                }
                if (auto it = rigArchives.find({ ToLower(StemOf(modelName)), mp }); it != rigArchives.end()) {
                    for (const std::string& z : it->second) {
                        if (z != "actor/zelda_ec.zar" || zarPath == z) {
                            clipArchives.insert(z);
                        }
                    }
                }
            }
            for (const std::string& z : clipArchives) {
                const RomIndex::Archive* a = index.Load(z);
                if (!a) {
                    continue;
                }
                for (const auto& [n, c] : a->csabs) {
                    if (z == zarPath && FolderOf(n) != FolderOf(modelName)) {
                        continue; // another prop's clips in a grouped archive
                    }
                    if (c.boneToNode.size() == model->bones.size() && seenClip.insert(ToLower(StemOf(n))).second) {
                        cands.push_back({ z == zarPath ? n : z + ":" + n, &c });
                    }
                }
            }
            AnimPairing pairing = PairAnimations(skel, mine, *model, cands);
            // The end credits (Demo_Ec) play their animations on many characters' skeletons; Grezzo's credits clips
            // are in zelda_ec / zelda_ec2, kept out of the matching above (their models share names with the field
            // ones). They join the candidates now, for the hand-checked pairs below.
            if (zarPath != "actor/zelda_ec.zar") {
                for (const char* z : { "actor/zelda_ec.zar", "actor/zelda_ec2.zar" }) {
                    const RomIndex::Archive* a = index.Load(z);
                    if (!a) {
                        continue;
                    }
                    for (const auto& [n, c] : a->csabs) {
                        if (c.boneToNode.size() == model->bones.size() && seenClip.insert(ToLower(StemOf(n))).second) {
                            cands.push_back({ std::string(z) + ":" + n, &c });
                        }
                    }
                }
            }
            // Hand-checked pairs (Oot3dAnimCuration.inc) replace the matcher's pick. A clip "a|b|c" lets the motion
            // decide among those clips; the skeleton's animations with the same choice are paired one to one.
            {
                struct AnimCurated {
                    const char* skeleton;
                    const char* n64;
                    const char* clip;
                };
                static const std::vector<AnimCurated> kAnims = {
#include "Oot3dAnimCuration.inc"
                };
                auto clipIndex = [&](const std::string& clip) {
                    for (size_t k = 0; k < cands.size(); k++) {
                        if (EndsWith(ToLower(cands[k].name), ToLower(clip))) {
                            return static_cast<int>(k);
                        }
                    }
                    return -1;
                };
                std::map<std::string, std::vector<std::string>> choices; // "a|b|c" -> the N64 animations choosing
                for (const AnimCurated& ac : kAnims) {
                    if (skel.path != ac.skeleton) {
                        continue;
                    }
                    const std::string clip = ac.clip;
                    if (clip.find('|') != std::string::npos) {
                        choices[clip].push_back(ac.n64);
                        continue;
                    }
                    const int k = clipIndex(clip);
                    for (AnimPair& p : pairing.pairs) {
                        if (p.n64Path == ac.n64 && k >= 0) {
                            p.csab = k;
                            p.score = 0.0f;
                        }
                    }
                    if (k < 0) {
                        log(LogLevel::Warning, "  curated clip " + clip + " not among " + skel.path + "'s clips");
                    }
                }
                for (const auto& [alts, n64s] : choices) {
                    std::vector<AnimCandidate> sub;
                    std::vector<int> subIndex;
                    std::stringstream ss(alts);
                    for (std::string c; std::getline(ss, c, '|');) {
                        if (const int k = clipIndex(c); k >= 0) {
                            sub.push_back(cands[k]);
                            subIndex.push_back(k);
                        }
                    }
                    std::vector<VanillaAnimation> subAnims;
                    for (const std::string& n : n64s) {
                        for (const VanillaAnimation& a : mine) {
                            if (a.path == n) {
                                subAnims.push_back(a);
                            }
                        }
                    }
                    if (sub.empty() || subAnims.empty()) {
                        log(LogLevel::Warning, "  curated choice " + alts + " has no clip for " + skel.path);
                        continue;
                    }
                    const AnimPairing sp = PairAnimations(skel, subAnims, *model, sub);
                    for (const AnimPair& q : sp.pairs) {
                        for (AnimPair& p : pairing.pairs) {
                            if (p.n64Path == q.n64Path && q.csab >= 0) {
                                p.csab = subIndex[q.csab];
                                p.score = q.score;
                            }
                        }
                    }
                }
            }
            log(LogLevel::Detail, "skin " + skel.path + " <- " + zarPath + ":" + modelName + " (" + how + ", " +
                                      std::to_string(mine.size()) + " N64 animations, " +
                                      std::to_string(cands.size()) + " 3DS clips from " +
                                      std::to_string(clipArchives.size()) + " archive(s))");

            // Material animations (CMAB). Constant and material colors Grezzo animates (the Flare Dancer's fire) take
            // their mean over the animation; texture palettes (eyes, mouths) become per-texture state lists the
            // engine picks from: the one matching the N64 texture the actor put in a segment, else Grezzo's track.
            Cmb modelCopy = *model;
            std::map<int, FacePalette> faces;
            {
                std::vector<std::string> cmabArchives{ zarPath };
                for (const std::string& z : clipArchives) {
                    if (z != zarPath) {
                        cmabArchives.push_back(z);
                    }
                }
                cmabArchives.push_back("actor/zelda_ec.zar");
                for (const Cmab& c : ModelCmabs(index, cmabArchives, modelName)) {
                    for (const CmabEntry& e : c.entries) {
                        if (e.material >= modelCopy.materials.size()) {
                            continue;
                        }
                        CmbMaterial& mat = modelCopy.materials[e.material];
                        const bool color = e.type == CmabEntry::ConstColor || e.type == CmabEntry::DiffuseColor;
                        if (color) {
                            Rgba8* dst = e.type == CmabEntry::DiffuseColor
                                             ? &mat.diffuse
                                             : (e.channel < mat.constants.size() ? &mat.constants[e.channel] : nullptr);
                            for (size_t k = 0; dst && k < 4 && k < e.tracks.size(); k++) {
                                if (!e.present[k]) {
                                    continue;
                                }
                                double sum = 0, mx = 0;
                                for (const CmabKey& key : e.tracks[k].keys) {
                                    sum += key.value;
                                    mx = std::max(mx, double(key.value));
                                }
                                double v = sum / e.tracks[k].keys.size();
                                if (mx <= 1.5) {
                                    v *= 255.0;
                                }
                                (*dst)[k] = static_cast<uint8_t>(std::clamp(std::lround(v), 0L, 255L));
                            }
                        } else if (e.type == CmabEntry::TexturePalette && !c.textures.empty() && e.present[0] &&
                                   !faces.count(int(e.material))) {
                            FacePalette fp;
                            fp.pal.slot = int(e.channel);
                            for (const CmabTexture& t : c.textures) {
                                fp.pal.images.push_back(t.image);
                                fp.names.push_back(t.name);
                            }
                            fp.track = e.tracks[0];
                            fp.duration = c.duration;
                            fp.loop = c.loop;
                            faces[int(e.material)] = std::move(fp);
                        }
                    }
                }
                // The N64 eye and mouth textures the actor puts in a segment, in its order (open, half, closed, ...):
                // the actor arrays of the skeleton's object and of the objects its curated lists come from (Fado's
                // head is object_fa's). Entry i shows palette texture i (Grezzo kept the N64 order).
                std::set<std::string> dirs{ skel.path.substr(0, skel.path.find_last_of('/') + 1) };
                for (const CuratedSkeleton& c : Curation()) {
                    if (skel.path == c.skeleton) {
                        for (const CuratedMesh& m : c.meshes) {
                            for (const char* l : { m.limbList, m.show }) {
                                const std::string ls = l;
                                if (StartsWith(ls, "objects/")) {
                                    dirs.insert(ls.substr(0, ls.find_last_of('/') + 1));
                                }
                            }
                        }
                    }
                }
                for (auto& [mi, fp] : faces) {
                    bool eye = false, mouth = false;
                    for (const std::string& n : fp.names) {
                        eye |= ToLower(n).find("eye") != std::string::npos;
                        mouth |= ToLower(n).find("mouth") != std::string::npos;
                    }
                    std::set<std::string> paired;
                    for (const FaceArray& fa : FaceTable()) {
                        if ((fa.kind == 'e' && !eye) || (fa.kind == 'm' && !mouth) || fa.paths.empty()) {
                            continue;
                        }
                        const std::string first = fa.paths[0];
                        if (!dirs.count(first.substr(0, first.find_last_of('/') + 1))) {
                            continue;
                        }
                        for (size_t i = 0; i < fa.paths.size(); i++) {
                            if (paired.insert(fa.paths[i]).second) {
                                fp.n64.emplace_back(fa.paths[i], int(std::min(i, fp.pal.images.size() - 1)));
                            }
                        }
                    }
                    // Grezzo's palette tracks only list the textures (frame i = texture i: the 3DS code picks the face by
                    // frame), so they are no clock. Eyes the actor sets no texture for blink every few seconds (open,
                    // half, closed, half); anything else holds its first texture.
                    fp.track.keys.clear();
                    fp.loop = true;
                    if (eye && fp.pal.images.size() >= 3) {
                        for (const auto& [t, v] : std::vector<std::pair<float, float>>{
                                 { 0.f, 0.f }, { 84.f, 1.f }, { 86.f, 2.f }, { 89.f, 1.f }, { 91.f, 0.f } }) {
                            CmabKey k;
                            k.time = t;
                            k.value = v;
                            fp.track.keys.push_back(k);
                        }
                        fp.duration = 100;
                    } else {
                        fp.track.keys.assign(1, CmabKey{});
                        fp.duration = 1;
                    }
                    log(LogLevel::Detail, "  face palette on material " + std::to_string(mi) + " (" +
                                              (eye ? "eyes" : mouth ? "mouth" : "?") + "): " +
                                              std::to_string(fp.pal.images.size()) + " textures, " +
                                              std::to_string(fp.n64.size()) + " N64 textures paired");
                }
            }
            const Cmb* sourceModel = model;
            model = &modelCopy;

            // Mesh groups (material state lists + batches) and the vertices they use.
            MeshOptions mo;
            mo.n64Lighting = true;
            mo.envPass = true; // metal and jewels: the sphere-mapped reflection pass (MergeShip3dsSkin draws it)
            // Characters whose color the actor sets each frame (PRIM and ENV) where Grezzo's code sets a material
            // constant instead: their greyscale textures take the actor's colors (the Flare Dancer's fire changes
            // from green to blue to red as it weakens).
            static const std::set<std::string> kActorColored = { "objects/object_fw/gFlareDancerSkel" };
            if (kActorColored.count(skel.path)) {
                mo.tintEnv = mo.tintPrim = true;
            }
            MeshBuilder mesh(zip, textures, mo, opt.log);
            mesh.paletteFor = [&](const MeshGroup& g) -> const SkinPalette* {
                if (g.cmb != model || !g.src) {
                    return nullptr;
                }
                auto it = faces.find(static_cast<int>(g.src - model->materials.data()));
                return it == faces.end() ? nullptr : &it->second.pal;
            };
            const VanillaAnimation* bestN64 = nullptr;
            const Csab* bestCsab = nullptr;
            {
                float best = 1e30f;
                for (size_t a = 0; a < pairing.pairs.size() && a < mine.size(); a++) {
                    const AnimPair& p = pairing.pairs[a];
                    if (p.csab >= 0 && p.score < best) {
                        best = p.score;
                        bestN64 = &mine[a];
                        bestCsab = cands[p.csab].csab;
                    }
                }
            }
            Variants variants = FindVariants(skel, *model, pairing.limbToBone, bestN64, bestCsab, oot, log);
            ApplyCuration(skel, variants, log);
            // One 3DS model over several N64 skeletons: each skeleton draws the vertices of its bones (by the bone
            // weighing most), placed by the offset of its actor from the first one (model units).
            struct BoneSplit {
                const char* skel;
                std::vector<int> bones;
                bool only; // only these bones (else all but these)
                Vec3 offset;
            };
            static const std::vector<BoneSplit> kBoneSplits = {
                // EnCow spawns the tail 57 up and 36 back (EnCow_SetTailPos, scale 0.01).
                { "objects/object_cow/gCowBodySkel", { 5, 6, 7, 8, 9 }, false, Vec3(0, 0, 0) },
                { "objects/object_cow/gCowTailSkel", { 5, 6, 7, 8, 9 }, true, Vec3(0, -5700, 3600) },
            };
            Vec3 skinOffset(0, 0, 0);
            mesh.keepVertex = nullptr;
            for (const BoneSplit& bs : kBoneSplits) {
                if (skel.path != bs.skel) {
                    continue;
                }
                skinOffset = bs.offset;
                const std::vector<int> bones = bs.bones;
                const bool only = bs.only;
                mesh.keepVertex = [bones, only](const CmbVertex& v) {
                    int best = 0;
                    for (int i = 1; i < v.boneCount && i < 4; i++) {
                        if (v.weights[i] > v.weights[best]) {
                            best = i;
                        }
                    }
                    const bool in = std::find(bones.begin(), bones.end(), v.bones[best]) != bones.end();
                    return in == only;
                };
                log(LogLevel::Detail, std::string("  bone split: ") + (only ? "only" : "without") + " the listed bones");
            }
            // Models a skeleton swaps by the list its actor puts on a limb (EnDoor: limb 4 draws each dungeon's own
            // door, picked by scene, its two sides by the camera): Grezzo made a model per door, both sides in one.
            // Each model shows while its list is drawn, in place of the matched model.
            struct ModelVariant {
                const char* skel;
                int limb; // 0-based
                const char* list;
                const char* zar;
                const char* model;
            };
            static const std::vector<ModelVariant> kModelVariants = {
#define MS_DOOR(l, r, z, m)                                                                                                { "objects/gameplay_keep/gDoorSkel", 3, l, z, m }, { "objects/gameplay_keep/gDoorSkel", 3, r, z, m }
                MS_DOOR("objects/gameplay_keep/gDoorLeftDL", "objects/gameplay_keep/gDoorRightDL", "actor/zelda_keep.zar",
                        "door/model/door_model.cmb"),
                MS_DOOR("objects/object_hidan_objects/gFireTempleDoorWithHandleLeftDL",
                        "objects/object_hidan_objects/gFireTempleDoorWithHandleRightDL", "actor/zelda_keep.zar",
                        "door/model/m_Fnormaldoor_omote_model.cmb"),
                MS_DOOR("objects/object_mizu_objects/gWaterTempleDoorLeftDL",
                        "objects/object_mizu_objects/gWaterTempleDoorRightDL", "actor/zelda_keep.zar",
                        "door/model/m_Wnormaldoor_omote_model.cmb"),
                MS_DOOR("objects/object_haka_door/gShadowDoorLeftDL", "objects/object_haka_door/gShadowDoorRightDL",
                        "actor/zelda_keep.zar", "door/model/m_Hnormaldoor_omote_model.cmb"),
                MS_DOOR("objects/gameplay_field_keep/gFieldDoorLeftDL", "objects/gameplay_field_keep/gFieldDoorRightDL",
                        "actor/zelda_field_keep.zar", "Model/obj_door_omote_model.cmb"),
#undef MS_DOOR
            };
            std::vector<std::pair<const Cmb*, int>> variantModels; // model, tag
            for (const ModelVariant& mv : kModelVariants) {
                if (skel.path != mv.skel) {
                    continue;
                }
                const RomIndex::Archive* va = index.Load(mv.zar);
                const Cmb* vc = va ? va->FindCmb(mv.model) : nullptr;
                if (!vc || vc->bones.size() != model->bones.size()) {
                    log(LogLevel::Warning, std::string("  model variant ") + mv.model + ": missing or another rig");
                    continue;
                }
                const std::pair<int, std::string> cond{ mv.limb, mv.list };
                int tag = -1;
                for (size_t t = 0; t < variants.conditions.size(); t++) {
                    if (variants.conditions[t] == cond) {
                        tag = static_cast<int>(t);
                    }
                }
                if (tag < 0) {
                    tag = static_cast<int>(variants.conditions.size());
                    variants.conditions.push_back(cond);
                }
                variantModels.emplace_back(vc, tag);
                log(LogLevel::Detail, std::string("  model variant ") + mv.model + " while limb " +
                                          std::to_string(mv.limb) + " draws " + mv.list);
            }
            if (variantModels.empty()) {
                mesh.Add(*model, 0, Mat34::Identity(), nullptr, [&](const CmbTriangleSet& set) {
                    auto it = variants.meshTag.find(set.mesh);
                    return it == variants.meshTag.end() ? 0 : it->second;
                });
            } else {
                for (size_t i = 0; i < variantModels.size(); i++) {
                    const int tag = variantModels[i].second;
                    mesh.Add(*variantModels[i].first, static_cast<int>(1 + i), Mat34::Identity(), nullptr,
                             [tag](const CmbTriangleSet&) { return tag; });
                }
            }
            // Companion models: Grezzo splits some characters over several models on the same rig (the Bari's
            // tentacles, bell and core). Others in the archive with the model's bones that sit elsewhere join it;
            // ones covering the same space are alternatives (the two cows) and stay out.
            std::vector<const Cmb*> parts{ model };
            std::vector<int> partBone{ -1 }; // for a one-bone prop: the model bone it rides (-1: its own rig)
            std::vector<bool> partLocal{ false }; // the prop is authored in that bone's space (else the model's)
            for (const auto& [vc, tag] : variantModels) {
                parts.push_back(vc); // cmbIndex 1 + i, as added above
                partBone.push_back(-1);
                partLocal.push_back(false);
            }
            {
                const RomIndex::Archive* arch = index.Load(zarPath);
                auto boxOfCmb = [](const Cmb& c, Vec3& mn, Vec3& mx) {
                    const auto w = c.BindPose();
                    std::vector<Mat34> iv;
                    for (const auto& m : w) {
                        iv.push_back(m.Inverse());
                    }
                    mn = Vec3(1e9f, 1e9f, 1e9f);
                    mx = Vec3(-1e9f, -1e9f, -1e9f);
                    size_t n = 0;
                    for (const CmbTriangleSet& set : c.Decode()) {
                        for (const CmbVertex& v : set.vertices) {
                            const Vec3 p = SkinPosition(v, set.skinning, w, iv);
                            mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
                            mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
                            n++;
                        }
                    }
                    return n;
                };
                Vec3 mmn, mmx;
                boxOfCmb(*model, mmn, mmx);
                // One-bone props (the Bari's bell) are placed where a vanilla limb draws the same thing: both posed by
                // the best paired animation's first frame, the prop on the 3DS bone of each limb in turn.
                std::vector<Mat34> n64Pose, posed3, inv3;
                float scale3 = 1.0f;
                if (bestN64 && bestCsab) {
                    n64Pose = N64Pose(skel, *bestN64, 0);
                    posed3 = CsabPose(*model, *bestCsab, 0.0f);
                    for (const auto& w : model->BindPose()) {
                        inv3.push_back(w.Inverse());
                    }
                    float a = 0, b = 0;
                    for (const Mat34& m : n64Pose) {
                        a = std::max(a, m.Translation().Length());
                    }
                    for (const Mat34& m : posed3) {
                        b = std::max(b, m.Translation().Length());
                    }
                    scale3 = b > 1.0f ? a / b : 1.0f;
                }
                static const std::vector<std::pair<std::string, Cmb>> kNoModels;
                for (const auto& [n, c] : arch ? arch->cmbs : kNoModels) {
                    // A one-bone model is a piece on its own (Demo_Ik's armor): the others are other actors' pieces.
                    if (&c == sourceModel || FolderOf(n) != FolderOf(modelName) || model->bones.size() < 2) {
                        continue;
                    }
                    // Disabled: shape alone also matches parts the main model already has (the Iron Knuckle's
                    // break-away armor); one-bone parts the N64 draws as separate lists go through ConvertObjects.
                    if (c.bones.size() == 1 && !n64Pose.empty() && kAttachProps) {
                        std::vector<Vec3> pts;
                        for (const CmbTriangleSet& set : c.Decode()) {
                            for (const CmbVertex& v : set.vertices) {
                                pts.push_back(v.pos);
                            }
                        }
                        if (pts.size() < 3) {
                            continue;
                        }
                        pts = Subsample(pts, 300);
                        int bestLimb = -1;
                        bool bestLocal = false;
                        float bestErr = 1e30f;
                        for (size_t l = 0; l < skel.limbs.size() && l < n64Pose.size() && l < pairing.limbToBone.size();
                             l++) {
                            const int b = pairing.limbToBone[l];
                            if (skel.limbs[l].dList.empty() || b < 0 || size_t(b) >= posed3.size()) {
                                continue;
                            }
                            std::vector<Vec3> dl;
                            for (const Vec3& q : InspectVanillaDl(oot, skel.limbs[l].dList, true).positions) {
                                dl.push_back(n64Pose[l].Point(q));
                            }
                            if (dl.size() < 3) {
                                continue;
                            }
                            Vec3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
                            for (const Vec3& q : dl) {
                                mn = { std::min(mn.x, q.x), std::min(mn.y, q.y), std::min(mn.z, q.z) };
                                mx = { std::max(mx.x, q.x), std::max(mx.y, q.y), std::max(mx.z, q.z) };
                            }
                            const std::vector<Vec3> dlS = Subsample(dl, 600);
                            // Authored in the model's bind space, or in the bone's own space.
                            for (bool local : { false, true }) {
                                const Mat34 place = local ? posed3[b] : posed3[b] * inv3[b];
                                std::vector<Vec3> at;
                                for (const Vec3& q : pts) {
                                    at.push_back(place.Point(q) * scale3);
                                }
                                // Both ways (the prop is the limb's list, not just near it), at a similar size.
                                Vec3 pmn(1e9f, 1e9f, 1e9f), pmx(-1e9f, -1e9f, -1e9f);
                                for (const Vec3& q : at) {
                                    pmn = { std::min(pmn.x, q.x), std::min(pmn.y, q.y), std::min(pmn.z, q.z) };
                                    pmx = { std::max(pmx.x, q.x), std::max(pmx.y, q.y), std::max(pmx.z, q.z) };
                                }
                                const float dd = std::max(1.0f, (mx - mn).Length()), pd = (pmx - pmn).Length();
                                if (pd < 0.5f * dd || pd > 2.0f * dd) {
                                    continue;
                                }
                                const float e = 0.5f *
                                                (MeanNearest(at, dlS, Mat34::Identity()) +
                                                 MeanNearest(dlS, at, Mat34::Identity())) /
                                                dd;
                                if (e < bestErr) {
                                    bestErr = e;
                                    bestLimb = static_cast<int>(l);
                                    bestLocal = local;
                                }
                            }
                        }
                        log(LogLevel::Detail, "  prop " + n + ": best limb " + std::to_string(bestLimb) + " fit " +
                                                  std::to_string(bestErr));
                        if (bestLimb >= 0 && bestErr < 0.06f) {
                            const int k = static_cast<int>(parts.size());
                            parts.push_back(&c);
                            partBone.push_back(pairing.limbToBone[bestLimb]);
                            partLocal.push_back(bestLocal);
                            mesh.Add(c, k, Mat34::Identity());
                            log(LogLevel::Detail, "  with " + n + " on limb " + std::to_string(bestLimb) +
                                                      (bestLocal ? " (bone space" : " (model space") + ", fit " +
                                                      std::to_string(bestErr) + ")");
                        }
                        continue;
                    }
                    if (c.bones.size() != model->bones.size()) {
                        continue;
                    }
                    bool sameRig = true;
                    for (size_t b = 0; b < c.bones.size() && sameRig; b++) {
                        sameRig = c.bones[b].parent == model->bones[b].parent &&
                                  (c.bones[b].translation - model->bones[b].translation).Length() <= 0.5f;
                    }
                    Vec3 cmn, cmx;
                    if (!sameRig || boxOfCmb(c, cmn, cmx) < 3) {
                        continue;
                    }
                    const float ix = std::max(0.0f, std::min(mmx.x, cmx.x) - std::max(mmn.x, cmn.x));
                    const float iy = std::max(0.0f, std::min(mmx.y, cmx.y) - std::max(mmn.y, cmn.y));
                    const float iz = std::max(0.0f, std::min(mmx.z, cmx.z) - std::max(mmn.z, cmn.z));
                    const Vec3 sa = mmx - mmn, sb = cmx - cmn;
                    const float inter = ix * iy * iz;
                    const float iou = inter / std::max(1e-6f, sa.x * sa.y * sa.z + sb.x * sb.y * sb.z - inter);
                    if (iou >= 0.6f) {
                        continue; // an alternative of the model, not a part of it
                    }
                    const int k = static_cast<int>(parts.size());
                    parts.push_back(&c);
                    partBone.push_back(-1);
                    partLocal.push_back(false);
                    mesh.Add(c, k, Mat34::Identity());
                    log(LogLevel::Detail, "  with " + n + " (same rig, overlap " + std::to_string(iou) + ")");
                }
            }
            const std::string skelName = skel.path.substr(skel.path.find_last_of('/') + 1);
            // Runtime-tinted materials take their color from the segment list the vanilla limb nearest to them calls
            // (En_Hy's and En_Ko's clothing colors).
            {
                const auto bind = model->BindPose();
                std::vector<uint32_t> limbSegment(skel.limbs.size(), 0);
                for (size_t l = 0; l < skel.limbs.size(); l++) {
                    for (const std::string& d : { skel.limbs[l].dList, skel.limbs[l].dList2 }) {
                        if (!d.empty() && limbSegment[l] == 0) {
                            const VanillaDlInfo info = InspectVanillaDl(oot, d);
                            for (uint32_t seg : info.segmentCalls) {
                                if (seg >= 8 && seg <= 0xF) {
                                    limbSegment[l] = seg << 24;
                                    break;
                                }
                            }
                        }
                    }
                }
                mesh.segmentFor = [&, bind, limbSegment](const MeshGroup& g) -> uint32_t {
                    Vec3 c;
                    size_t n = 0;
                    for (const MeshTri& t : g.tris) {
                        for (const Vec3& p : t.pos) {
                            c += p;
                            n++;
                        }
                    }
                    if (n == 0) {
                        return 0;
                    }
                    c = c * (1.0f / n);
                    uint32_t best = 0;
                    float bestD = 1e30f;
                    for (size_t l = 0; l < limbSegment.size() && l < pairing.limbToBone.size(); l++) {
                        const int b = pairing.limbToBone[l];
                        if (limbSegment[l] == 0 || b < 0 || size_t(b) >= bind.size()) {
                            continue;
                        }
                        const float d = (bind[b].Translation() - c).Length();
                        if (d < bestD) {
                            bestD = d;
                            best = limbSegment[l];
                        }
                    }
                    return best;
                };
            }
            const auto groups = mesh.EmitSkin(0, "oot3d/skin/" + object + "/" + skelName);
            res.warnings += mesh.warnings;
            if (groups.empty()) {
                continue;
            }
            const auto world = model->BindPose();
            std::vector<Mat34> inv(world.size());
            for (size_t i = 0; i < world.size(); i++) {
                inv[i] = world[i].Inverse();
            }
            struct SkinVertex {
                Vec3 pos, normal;
                uint8_t bone[4] = { 0, 0, 0, 0 }, weight[4] = { 255, 0, 0, 0 };
            };
            std::map<uint64_t, SkinVertex> source;
            for (size_t part = 0; part < parts.size(); part++) {
                int setNo = 0;
                for (const CmbTriangleSet& set : parts[part]->Decode()) {
                    setNo++;
                    for (size_t vi = 0; vi < set.vertices.size(); vi++) {
                        const CmbVertex& v = set.vertices[vi];
                        SkinVertex sv;
                        if (partBone[part] >= 0) {
                            sv.pos = partLocal[part] ? world[partBone[part]].Point(v.pos) : v.pos;
                            sv.normal = partLocal[part] ? world[partBone[part]].Dir(v.normal) : v.normal;
                            sv.bone[0] = static_cast<uint8_t>(partBone[part]);
                            source[(uint64_t(part) << 48) | (uint64_t(setNo) << 24) | vi] = sv;
                            continue;
                        }
                        sv.pos = SkinPosition(v, set.skinning, world, inv);
                        sv.normal = SkinNormal(v, set.skinning, world, inv);
                        if (set.skinning == Skinning::Smooth) {
                            float total = 0;
                            for (int k = 0; k < v.boneCount; k++) {
                                total += std::max(0.0f, v.weights[k]);
                            }
                            int given = 0;
                            for (int k = 0; k < 4; k++) {
                                sv.bone[k] = static_cast<uint8_t>(v.bones[k]);
                                const float w = k < v.boneCount && total > 0 ? std::max(0.0f, v.weights[k]) / total : 0;
                                sv.weight[k] = static_cast<uint8_t>(std::lround(w * 255.0f));
                                given += sv.weight[k];
                            }
                            sv.weight[0] = static_cast<uint8_t>(std::clamp(sv.weight[0] + (255 - given), 0, 255));
                        } else {
                            sv.bone[0] = static_cast<uint8_t>(v.bones[0]);
                        }
                        source[(uint64_t(part) << 48) | (uint64_t(setNo) << 24) | vi] = sv;
                    }
                }
            }
            std::map<uint64_t, uint32_t> index;
            std::vector<const SkinVertex*> vertices;
            for (const SkinGroup& g : groups) {
                for (const SkinBatch& b : g.batches) {
                    for (const SkinBatchVertex& v : b.vertices) {
                        if (!index.count(v.key)) {
                            auto it = source.find(v.key);
                            if (it == source.end()) {
                                continue;
                            }
                            index[v.key] = static_cast<uint32_t>(vertices.size());
                            vertices.push_back(&it->second);
                        }
                    }
                }
            }

            // The blob.
            Writer w;
            w.U32(0x4B53334F); // "O3SK"
            w.U32(5);
            w.U32(static_cast<uint32_t>(model->bones.size()));
            w.U32(static_cast<uint32_t>(vertices.size()));
            w.U32(static_cast<uint32_t>(groups.size()));
            size_t animCount = 0;
            for (const AnimPair& p : pairing.pairs) {
                animCount += p.csab >= 0;
            }
            w.U32(static_cast<uint32_t>(animCount));
            w.U32(static_cast<uint32_t>(skel.limbs.size()));
            for (size_t b = 0; b < model->bones.size(); b++) {
                const CmbBone& bone = model->bones[b];
                w.S16(static_cast<int16_t>(bone.parent));
                w.U16(0);
                for (const Vec3& v3 : { bone.scale, bone.rotation, bone.translation }) {
                    w.F32(v3.x);
                    w.F32(v3.y);
                    w.F32(v3.z);
                }
                for (int r = 0; r < 3; r++) {
                    for (int c = 0; c < 4; c++) {
                        w.F32(inv[b].m[r][c]);
                    }
                }
            }
            for (const SkinVertex* v : vertices) {
                for (float f : { v->pos.x, v->pos.y, v->pos.z, v->normal.x, v->normal.y, v->normal.z }) {
                    w.F32(f);
                }
                w.Raw(v->bone, 4);
                w.Raw(v->weight, 4);
            }
            for (const SkinGroup& g : groups) {
                Str(w, g.statePath);
                w.U8(g.lit ? 1 : 0);
                w.U8(g.translucent ? 1 : 0);
                const auto& cond = variants.conditions[size_t(g.tag) < variants.conditions.size() ? g.tag : 0];
                w.S16(static_cast<int16_t>(cond.first));
                Str(w, cond.second);
                // v3: the face palette (state lists per texture, Grezzo's track, the N64 textures paired).
                auto fit = g.palettePaths.empty() ? faces.end() : faces.find(g.material);
                if (fit == faces.end()) {
                    w.U8(0);
                } else {
                    const FacePalette& fp = fit->second;
                    w.U8(static_cast<uint8_t>(g.palettePaths.size()));
                    w.U8(fp.loop ? 1 : 0);
                    w.U16(0);
                    w.U32(fp.duration);
                    for (const std::string& pp : g.palettePaths) {
                        Str(w, pp);
                    }
                    w.U16(static_cast<uint16_t>(fp.track.keys.size()));
                    w.U16(0);
                    for (const CmabKey& key : fp.track.keys) {
                        w.F32(key.time);
                        w.F32(key.value);
                    }
                    w.U16(static_cast<uint16_t>(fp.n64.size()));
                    w.U16(0);
                    for (const auto& [path, idx] : fp.n64) {
                        Str(w, path);
                        w.U32(static_cast<uint32_t>(idx));
                    }
                }
                // v5: the reflection pass's state list ("": none).
                Str(w, g.envStatePath);
                w.U16(static_cast<uint16_t>(g.batches.size()));
                for (const SkinBatch& b : g.batches) {
                    w.U8(static_cast<uint8_t>(b.vertices.size()));
                    w.U8(static_cast<uint8_t>(b.tris.size()));
                    w.U16(0);
                    for (const SkinBatchVertex& v : b.vertices) {
                        auto it = index.find(v.key);
                        w.U32(it == index.end() ? 0 : it->second);
                        w.S16(v.vtx.s);
                        w.S16(v.vtx.t);
                        w.U8(v.vtx.r);
                        w.U8(v.vtx.g);
                        w.U8(v.vtx.b);
                        w.U8(v.vtx.a);
                        w.U8(v.envAlpha); // v5
                    }
                    for (const auto& t : b.tris) {
                        w.U8(t[0]);
                        w.U8(t[1]);
                        w.U8(t[2]);
                    }
                    w.Pad(4);
                }
            }
            for (const AnimPair& p : pairing.pairs) {
                if (p.csab < 0) {
                    res.unpaired++;
                    log(LogLevel::Warning, "UNPAIRED animation " + p.n64Path + " (" + skel.path + ")");
                    continue;
                }
                const Csab& c = *cands[p.csab].csab;
                Str(w, p.n64Path);
                w.S16(static_cast<int16_t>(p.n64Frames));
                w.U16(0);
                w.F32(static_cast<float>(c.duration));
                for (size_t b = 0; b < model->bones.size(); b++) {
                    const CsabNode* node = c.NodeForBone(b);
                    uint16_t mask = 0;
                    for (int k = 0; k < 9; k++) {
                        if (node && node->tracks[k] && !node->tracks[k]->keys.empty()) {
                            mask |= uint16_t(1u << k);
                        }
                    }
                    w.U16(mask);
                    for (int k = 0; k < 9; k++) {
                        if (!(mask & (1u << k))) {
                            continue;
                        }
                        const CsabTrack& t = *node->tracks[k];
                        const bool hermite = t.type == CsabTrack::Type::Hermite;
                        w.U8(hermite ? 2 : 1);
                        w.U8(0);
                        w.U16(static_cast<uint16_t>(t.keys.size()));
                        w.F32(static_cast<float>(t.timeEnd));
                        for (const CsabKey& key : t.keys) {
                            w.F32(key.time);
                            w.F32(key.value);
                            if (hermite) {
                                w.F32(key.tangentIn);
                                w.F32(key.tangentOut);
                            }
                        }
                    }
                }
                res.animations++;
                log(LogLevel::Detail, "  anim " + p.n64Path + " -> " + cands[p.csab].name + " (score " +
                                          std::to_string(p.score) + (p.frameCountMatches ? "" : ", frames differ") +
                                          ")");
            }
            for (size_t l = 0; l < skel.limbs.size(); l++) {
                w.S16(static_cast<int16_t>(l < pairing.limbToBone.size() ? pairing.limbToBone[l] : -1));
            }
            // v3: N64 limbs that keep drawing their own list beside the 3DS mesh: effects Grezzo draws apart from the
            // character (a fairy's glowing body, Twinrova's broom jets and trails).
            {
                std::vector<int16_t> keep;
                for (size_t l = 0; l < skel.limbs.size(); l++) {
                    const std::string leaf = ToLower(skel.limbs[l].dList.substr(skel.limbs[l].dList.find_last_of('/') + 1));
                    for (const char* k : { "glow", "jet", "trail", "fire", "flame", "spark", "aura" }) {
                        if (!leaf.empty() && leaf.find(k) != std::string::npos) {
                            keep.push_back(static_cast<int16_t>(l));
                            break;
                        }
                    }
                }
                w.U16(static_cast<uint16_t>(keep.size()));
                for (int16_t l : keep) {
                    w.S16(l);
                }
                // v4: where the model sits in the actor's space (model units).
                w.F32(skinOffset.x);
                w.F32(skinOffset.y);
                w.F32(skinOffset.z);
                if (!keep.empty()) {
                    std::string ls;
                    for (int16_t l : keep) {
                        ls += " " + skel.limbs[l].dList.substr(skel.limbs[l].dList.find_last_of('/') + 1);
                    }
                    log(LogLevel::Detail, "  N64 limbs kept:" + ls);
                }
            }

            Writer blob;
            Bytes hdr = Soh::Header(kTypeBlob, 0);
            blob.Raw(hdr.data(), hdr.size());
            blob.U32(static_cast<uint32_t>(w.Size()));
            blob.Raw(w.Out().data(), w.Size());
            zip.Add(prefix + "oot3d/skin/" + skel.path, blob.Out());
            res.skins++;
            log(LogLevel::Detail, "  " + std::to_string(vertices.size()) + " vertices, " +
                                      std::to_string(groups.size()) + " materials, " + std::to_string(animCount) + "/" +
                                      std::to_string(pairing.pairs.size()) + " animations");
        }
    }
    if (opt.progress) {
        opt.progress("Characters", objects.size(), objects.size());
    }
    return res;
}

} // namespace Oot3d
