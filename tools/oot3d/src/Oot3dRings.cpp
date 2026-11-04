// OoT3D asset converter: bodies the N64 draws as rings of vertices, each ring under its own matrix.
//
// Morpha's tentacle is a list per ring (BossMo_DrawTentacle loads each ring's matrix, then the ring's list); the Like
// Like's body is one list that loads each ring's matrix from segment 0x0C (EnRr_Draw fills it). Either way a ring's
// vertices are transformed when loaded and triangles join them to the previous ring's still in the vertex buffer.
// Grezzo skinned both as tubes, a bone per ring (every vertex weighs on one bone); each ring's vertices go to its
// bone's place and are loaded the same way, so the actors' own matrices pose the 3DS bodies.

#include "Oot3dRings.h"

#include "Oot3dCmab.h"
#include "Oot3dCmb.h"
#include "Oot3dMesh.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZar.h"

#include <algorithm>
#include <map>
#include <set>

namespace Oot3d {

namespace {

struct RingVertex {
    Soh::Vtx vtx;
    int ring = 0;
};
struct RingTri {
    int v[3];
};

struct RingModel {
    std::string statePath;
    std::vector<RingVertex> verts;
    std::vector<RingTri> tris;
    int rings = 0;
};

// The model's one material state list and its vertices in their rings' frames: position minus the ring bone's bind
// position, scaled. Returns false when the model is not one material of rigid rings.
bool BuildRingModel(const Cmb& cmb, MeshBuilder& mesh, const std::string& prefix, float scale, RingModel& out,
                    const std::function<void(LogLevel, const std::string&)>& log) {
    mesh.Add(cmb, 0, Mat34::Identity());
    const std::vector<SkinGroup> groups = mesh.EmitSkin(0, prefix + "_state");
    if (groups.size() != 1) {
        log(LogLevel::Warning, prefix + ": " + std::to_string(groups.size()) + " materials (one expected)");
        return false;
    }
    out.statePath = groups[0].statePath;
    const std::vector<Mat34> world = cmb.BindPose();
    std::vector<Mat34> inv;
    for (const Mat34& m : world) {
        inv.push_back(m.Inverse());
    }
    std::map<uint64_t, std::pair<Vec3, int>> source; // key -> bind position, ring
    int setNo = 0;
    for (const CmbTriangleSet& set : cmb.Decode()) {
        setNo++;
        for (size_t vi = 0; vi < set.vertices.size(); vi++) {
            const CmbVertex& v = set.vertices[vi];
            int best = 0;
            for (int k = 1; k < v.boneCount && k < 4; k++) {
                if (v.weights[k] > v.weights[best]) {
                    best = k;
                }
            }
            source[(uint64_t(setNo) << 24) | vi] = { SkinPosition(v, set.skinning, world, inv), v.bones[best] };
        }
    }
    std::map<uint64_t, int> index;
    for (const SkinBatch& b : groups[0].batches) {
        std::vector<int> local;
        for (const SkinBatchVertex& bv : b.vertices) {
            auto it = index.find(bv.key);
            if (it == index.end()) {
                auto sit = source.find(bv.key);
                if (sit == source.end()) {
                    local.push_back(-1);
                    continue;
                }
                RingVertex rv;
                rv.vtx = bv.vtx;
                rv.ring = sit->second.second;
                const Vec3 p = (sit->second.first - world[size_t(rv.ring)].Point(Vec3(0, 0, 0))) * scale;
                rv.vtx.x = static_cast<int16_t>(std::lround(p.x));
                rv.vtx.y = static_cast<int16_t>(std::lround(p.y));
                rv.vtx.z = static_cast<int16_t>(std::lround(p.z));
                it = index.emplace(bv.key, static_cast<int>(out.verts.size())).first;
                out.verts.push_back(rv);
                out.rings = std::max(out.rings, rv.ring + 1);
            }
            local.push_back(it->second);
        }
        for (const auto& t : b.tris) {
            RingTri rt{ { local[t[0]], local[t[1]], local[t[2]] } };
            if (rt.v[0] >= 0 && rt.v[1] >= 0 && rt.v[2] >= 0) {
                out.tris.push_back(rt);
            }
        }
    }
    return !out.verts.empty();
}

int MaxRing(const RingModel& m, const RingTri& t) {
    return std::max({ m.verts[t.v[0]].ring, m.verts[t.v[1]].ring, m.verts[t.v[2]].ring });
}
int MinRing(const RingModel& m, const RingTri& t) {
    return std::min({ m.verts[t.v[0]].ring, m.verts[t.v[1]].ring, m.verts[t.v[2]].ring });
}

} // namespace

// The material's CMAB texture scroll, as the rooms' (Oot3dMaps.cpp).
void ScrollFromCmab(const Zar& z, const std::string& cmabName, MeshBuilder& mesh, std::map<int, ScrollRate>& rates) {
    const ZarFile* f = z.Find(cmabName);
    if (!f) {
        return;
    }
    const Cmab c = Cmab::Parse(f->data);
    for (const CmabEntry& e : c.entries) {
        if (e.type != CmabEntry::Translation || e.channel > 1) {
            continue;
        }
        ScrollRate& r = rates[int(e.material)];
        for (int k = 0; k < 2 && k < int(e.tracks.size()); k++) {
            const auto& keys = e.tracks[k].keys;
            if (!e.present[k] || keys.size() < 2 || keys.back().time <= keys.front().time) {
                continue;
            }
            (k == 0 ? r.s : r.t)[e.channel] =
                (keys.back().value - keys.front().value) / (keys.back().time - keys.front().time);
        }
    }
    mesh.scrollFor = [&rates](const MeshGroup& g) -> const ScrollRate* {
        if (!g.cmb || !g.src) {
            return nullptr;
        }
        auto it = rates.find(static_cast<int>(g.src - g.cmb->materials.data()));
        return it == rates.end() ? nullptr : &it->second;
    };
}

RingsResult ConvertRingModels(const RomFS& rom, const ZipReader& oot, ZipWriter& zip, TextureStore& textures,
                              const ExtractOptions& opt) {
    RingsResult res;
    auto log = [&](LogLevel l, const std::string& s) {
        if (opt.log) {
            opt.log(l, s);
        }
    };
    const std::string prefix = opt.altAssets ? "alt/" : "";

    // Morpha's tentacle (BossMo_DrawTentacle): rings 0..40 are gMorphaTentacleBaseDL and Part0..39, each drawn after
    // its ring's matrix. The 3DS tube runs along +Z with its rings in XY, as the N64 lists; 1680 radius vs 1000.
    try {
        const Zar z = Zar::Parse(rom.Read("actor/zelda_mo.zar"));
        const ZarFile* f = z.Find("Model/morpha.cmb");
        if (f) {
            const Cmb cmb = Cmb::Parse(f->data);
            MeshOptions mo;
            mo.drawnTranslucent = true;
            mo.n64Lighting = InspectVanillaDl(oot, "objects/object_mo/gMorphaTentaclePart5DL").lighting;
            MeshBuilder mesh(zip, textures, mo, opt.log);
            std::map<int, ScrollRate> rates;
            ScrollFromCmab(z, "misc/morpha.cmab", mesh, rates);
            RingModel m;
            if (BuildRingModel(cmb, mesh, "oot3d/rings/morpha", 1000.0f / 1680.0f, m, log) && m.rings == 41) {
                // Each ring's vertices, contiguous, in one array; ring r in the vertex buffer at (r % 2) * 32.
                std::vector<int> order, first(42, 0), slot(m.verts.size(), 0);
                for (int r = 0; r < m.rings; r++) {
                    first[r] = static_cast<int>(order.size());
                    for (size_t i = 0; i < m.verts.size(); i++) {
                        if (m.verts[i].ring == r) {
                            slot[i] = (r % 2) * 32 + (static_cast<int>(order.size()) - first[r]);
                            order.push_back(static_cast<int>(i));
                        }
                    }
                }
                first[m.rings] = static_cast<int>(order.size());
                std::vector<Soh::Vtx> arr;
                for (int i : order) {
                    arr.push_back(m.verts[size_t(i)].vtx);
                }
                const std::string arrayPath = "oot3d/rings/morpha_vtx";
                zip.Add(arrayPath, Soh::VertexArrayResource(arr));
                bool ok = true;
                for (int r = 0; r < m.rings && ok; r++) {
                    Soh::DisplayList dl;
                    if (r == 0) {
                        dl.Call(m.statePath);
                    }
                    const int n = first[r + 1] - first[r];
                    if (n > 32) {
                        ok = false;
                        break;
                    }
                    dl.Vertices(arrayPath, uint32_t(first[r]), uint32_t(n), uint32_t((r % 2) * 32));
                    for (const RingTri& t : m.tris) {
                        if (MaxRing(m, t) != r) {
                            continue;
                        }
                        if (MinRing(m, t) < r - 1) {
                            ok = false;
                            break;
                        }
                        dl.Tri1(uint8_t(slot[t.v[0]]), uint8_t(slot[t.v[1]]), uint8_t(slot[t.v[2]]));
                    }
                    dl.End();
                    const std::string list = r == 0 ? std::string("objects/object_mo/gMorphaTentacleBaseDL")
                                                     : "objects/object_mo/gMorphaTentaclePart" + std::to_string(r - 1) +
                                                           "DL";
                    zip.Add(prefix + list, dl.Resource());
                }
                if (ok) {
                    res.models++;
                    log(LogLevel::Detail, "rings: Morpha's tentacle <- zelda_mo.zar:Model/morpha.cmb (41 rings)");
                } else {
                    log(LogLevel::Warning, "rings: Morpha's tentacle does not fit the ring scheme");
                }
            }
            for (ScrollEntry& se : mesh.scrolls) {
                res.scrolls.push_back(std::move(se));
            }
        }
    } catch (const std::exception& e) {
        log(LogLevel::Warning, std::string("rings: Morpha: ") + e.what());
    }

    // The Like Like (EnRr_Draw): ring 0 under the matrix loaded before gLikeLikeDL, rings 1..4 under segment 0x0C's
    // matrices; the 3DS body has the same five rings (bones up +Y, the mouth above the last).
    try {
        const Zar z = Zar::Parse(rom.Read("actor/zelda_rr.zar"));
        const ZarFile* f = z.Find("Model/likelike.cmb");
        if (f) {
            const Cmb cmb = Cmb::Parse(f->data);
            MeshOptions mo;
            mo.drawnTranslucent = true;
            mo.n64Lighting = InspectVanillaDl(oot, "objects/object_rr/gLikeLikeDL").lighting;
            MeshBuilder mesh(zip, textures, mo, opt.log);
            std::map<int, ScrollRate> rates;
            ScrollFromCmab(z, "Misc/likelike.cmab", mesh, rates);
            RingModel m;
            if (BuildRingModel(cmb, mesh, "oot3d/rings/likelike", 1.0f, m, log) && m.rings == 5) {
                // Triangles by their rings; chunks of up to 64 vertices, each ring's part loaded under its matrix.
                std::vector<RingTri> tris = m.tris;
                std::stable_sort(tris.begin(), tris.end(), [&](const RingTri& a, const RingTri& b) {
                    return std::make_pair(MaxRing(m, a), MinRing(m, a)) < std::make_pair(MaxRing(m, b), MinRing(m, b));
                });
                Soh::DisplayList dl;
                dl.Call(m.statePath);
                bool ok = true, baseDone = false;
                size_t t0 = 0;
                int chunkNo = 0;
                while (t0 < tris.size() && ok) {
                    // Grow the chunk while its vertices fit; ring 0 (the actor's own matrix) only in the first.
                    std::set<int> used;
                    size_t t1 = t0;
                    while (t1 < tris.size()) {
                        std::set<int> next = used;
                        for (int k = 0; k < 3; k++) {
                            next.insert(tris[t1].v[k]);
                        }
                        if (next.size() > 64) {
                            break;
                        }
                        if (baseDone && MinRing(m, tris[t1]) == 0) {
                            ok = false;
                            break;
                        }
                        used = std::move(next);
                        t1++;
                    }
                    if (!ok || t1 == t0) {
                        ok = false;
                        break;
                    }
                    // The chunk's vertices by ring, contiguous, in an array of its own.
                    std::vector<Soh::Vtx> arr;
                    std::map<int, int> slotOf;
                    std::vector<std::pair<int, std::pair<int, int>>> loads; // ring, (first, count)
                    for (int r = 0; r < m.rings; r++) {
                        const int start = static_cast<int>(arr.size());
                        for (int v : used) {
                            if (m.verts[size_t(v)].ring == r) {
                                slotOf[v] = static_cast<int>(arr.size());
                                arr.push_back(m.verts[size_t(v)].vtx);
                            }
                        }
                        if (static_cast<int>(arr.size()) > start) {
                            loads.push_back({ r, { start, static_cast<int>(arr.size()) - start } });
                        }
                    }
                    const std::string arrayPath = "oot3d/rings/likelike_vtx" + std::to_string(chunkNo++);
                    zip.Add(arrayPath, Soh::VertexArrayResource(arr));
                    for (const auto& [r, range] : loads) {
                        if (r > 0) {
                            dl.MatrixLoad(0x0C000000u + uint32_t(r - 1) * 0x40u);
                        }
                        dl.Vertices(arrayPath, uint32_t(range.first), uint32_t(range.second), uint32_t(range.first));
                    }
                    baseDone = true;
                    for (size_t t = t0; t < t1; t++) {
                        dl.Tri1(uint8_t(slotOf[tris[t].v[0]]), uint8_t(slotOf[tris[t].v[1]]),
                                uint8_t(slotOf[tris[t].v[2]]));
                    }
                    t0 = t1;
                }
                dl.End();
                if (ok) {
                    zip.Add(prefix + "objects/object_rr/gLikeLikeDL", dl.Resource());
                    res.models++;
                    log(LogLevel::Detail, "rings: Like Like <- zelda_rr.zar:Model/likelike.cmb (5 rings, " +
                                              std::to_string(chunkNo) + " chunks)");
                } else {
                    log(LogLevel::Warning, "rings: the Like Like does not fit the ring scheme");
                }
            }
            for (ScrollEntry& se : mesh.scrolls) {
                res.scrolls.push_back(std::move(se));
            }
        }
    } catch (const std::exception& e) {
        log(LogLevel::Warning, std::string("rings: Like Like: ") + e.what());
    }
    return res;
}

} // namespace Oot3d
