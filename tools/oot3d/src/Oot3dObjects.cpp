// OoT3D asset converter: static object models (items, props, dungeon objects).
//
// Each vanilla object's top-level display lists (not skeleton limbs, not lists other lists call) are paired with the
// 3DS models of the same object by shape: bounding boxes first, then a rigid alignment (principal axes + ICP) for
// models Grezzo re-posed. A paired list is replaced under alt/ by the 3DS model converted into the vanilla list's
// space, lit by the N64 lights when the vanilla list was.

#include "Oot3dObjects.h"

#include "Oot3dAlign.h"

#include "Oot3dMaps.h"
#include "Oot3dMatch.h"
#include "Oot3dN64Texture.h"
#include "Oot3dRings.h"
#include "Oot3dMesh.h"
#include "Oot3dRom.h"
#include "Oot3dRomIndex.h"
#include "Oot3dSkeleton.h"
#include "Oot3dSkin.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZar.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>

namespace Oot3d {

namespace {

struct Box {
    Vec3 min{ 1e9f, 1e9f, 1e9f }, max{ -1e9f, -1e9f, -1e9f };
    void Add(const Vec3& p) {
        min = { std::min(min.x, p.x), std::min(min.y, p.y), std::min(min.z, p.z) };
        max = { std::max(max.x, p.x), std::max(max.y, p.y), std::max(max.z, p.z) };
    }
    Vec3 Center() const {
        return (min + max) * 0.5f;
    }
    Vec3 Size() const {
        return max - min;
    }
    float Diag() const {
        return Size().Length();
    }
};

// 0 for identical boxes; ~1 for unrelated ones.
float BoxScore(const Box& a, const Box& b) {
    const float d = std::max(1.0f, std::max(a.Diag(), b.Diag()));
    const Vec3 sa = a.Size(), sb = b.Size();
    const float center = (a.Center() - b.Center()).Length() / d;
    const float size = (std::fabs(sa.x - sb.x) + std::fabs(sa.y - sb.y) + std::fabs(sa.z - sb.z)) / (3.0f * d) * 3.0f;
    return center + size;
}

// The model's CMAB (its texture scroll) in the archive: "<dir>/model/<stem>.cmb" beside "<dir>/misc/<stem>.cmab".
std::string CmabFor(const Zar& z, const std::string& file) {
    std::string stem = ToLower(file);
    stem = stem.substr(stem.find_last_of('/') + 1);
    if (!EndsWith(stem, ".cmb")) {
        return "";
    }
    stem = stem.substr(0, stem.size() - 4) + ".cmab";
    for (const ZarFile& f : z.files) {
        const std::string n = ToLower(f.name);
        if (n == stem || EndsWith(n, "/" + stem)) {
            return f.name;
        }
    }
    return "";
}

struct Model {
    std::string name;
    std::string zar, file; // its archive and file there (its CMAB sits beside it)
    Cmb cmb;
    std::vector<Vec3> points; // bind pose
    Box box;
};

bool IsStatic(const Cmb& c) {
    if (c.bones.size() <= 1) {
        return true;
    }
    for (const auto& shape : c.shapes) {
        for (const auto& p : shape.primitives) {
            if (p.skinning == Skinning::Smooth) {
                return false;
            }
        }
    }
    return true; // rigid multi-bone models (hinged props) are still one shape at their bind pose
}

} // namespace

// Hand-checked pairings (Oot3dObjectCuration.inc): list -> 3DS model file, "-" keep the N64 list, "0" empty it.
static const std::map<std::string, std::string>& ForcedPairs() {
    static const std::map<std::string, std::string> kForced = [] {
        std::map<std::string, std::string> m;
        for (const auto& [l, model] : std::vector<std::pair<const char*, const char*>>{
#include "Oot3dObjectCuration.inc"
             }) {
            m[l] = model;
        }
        return m;
    }();
    return kForced;
}

// A curated model with its prefixes stripped: "^" (billboard), "*<scale>:" (the N64 list is that many times the 3DS
// model's size), "rx<degrees>:" (turned about X first: the actor turns the list), "flash:" (the actor flashes it with
// ENV, MeshOptions::envFlash), "prim:" (the actor's setup list combines it with the PRIM it sets, color and fade: the
// shadows' black, the lock-on triangles' target color; the vanilla list itself has no combiner to say so).
struct CuratedOpts {
    float scale = 1.0f;
    float sy = 1.0f; // "sy<f>:": stretched in Y (a block one size between Grezzo's)
    float rx = 0.0f;
    bool flash = false;
    bool prim = false;
    Mat34 Transform() const {
        return Mat34::FromSRT(Vec3(scale, scale * sy, scale), Vec3(rx * 3.14159265f / 180.0f, 0, 0), Vec3(0, 0, 0));
    }
};
static std::string CuratedModel(const std::string& value, CuratedOpts* o = nullptr) {
    std::string m = value.starts_with("^") ? value.substr(1) : value;
    for (;;) {
        const size_t colon = m.find(':');
        if (colon == std::string::npos) {
            break;
        }
        const std::string pre = m.substr(0, colon);
        if (pre.starts_with("*")) {
            if (o) {
                o->scale = std::stof(pre.substr(1));
            }
        } else if (pre.starts_with("sy")) {
            if (o) {
                o->sy = std::stof(pre.substr(2));
            }
        } else if (pre.starts_with("rx")) {
            if (o) {
                o->rx = std::stof(pre.substr(2));
            }
        } else if (pre == "flash") {
            if (o) {
                o->flash = true;
            }
        } else if (pre == "prim") {
            if (o) {
                o->prim = true;
            }
        } else {
            break;
        }
        m = m.substr(colon + 1);
    }
    return m;
}

ObjectsResult ConvertObjects(const RomFS& rom, const RomIndex& index, const ZipReader& oot, ZipWriter& zip,
                             TextureStore& textures, const ExtractOptions& opt) {
    (void)rom;
    ObjectsResult res;
    auto log = [&](LogLevel l, const std::string& s) {
        if (opt.log) {
            opt.log(l, s);
        }
    };
    // Vanilla objects and their display lists.
    std::map<std::string, std::vector<std::string>> objects;
    for (const std::string& n : oot.Names()) {
        if (!StartsWith(n, "objects/")) {
            continue;
        }
        const size_t slash = n.find('/', 8);
        if (slash == std::string::npos) {
            continue;
        }
        objects[n.substr(8, slash - 8)].push_back(n);
    }
    uint64_t done = 0;
    const std::string prefix = opt.altAssets ? "alt/" : "";
    for (const auto& [object, names] : objects) {
        if (opt.cancel && *opt.cancel) {
            break;
        }
        if (opt.progress) {
            opt.progress("Objects", done, objects.size());
        }
        done++;
        const std::vector<std::string> archives = index.ArchivesFor(object);
        if (archives.empty()) {
            continue;
        }
        const std::string& zarPath = archives[0];
        // Top-level display lists: not a skeleton's limb list, not called by another list.
        std::set<std::string> limbLists, called;
        std::vector<std::string> lists;
        for (const std::string& n : names) {
            auto b = oot.Read(n);
            auto h = b ? ParseResourceHeader(*b) : std::nullopt;
            if (!h) {
                continue;
            }
            if (h->type == kTypeSkeleton) {
                if (auto s = ReadVanillaSkeleton(oot, n)) {
                    for (const auto& l : s->limbs) {
                        limbLists.insert(l.dList);
                        limbLists.insert(l.dList2);
                    }
                }
            } else if (h->type == Soh::kTypeDisplayList) {
                lists.push_back(n);
            }
        }
        std::map<std::string, VanillaDlInfo> info;
        for (const std::string& n : lists) {
            info[n] = InspectVanillaDl(oot, n, true);
            for (const std::string& c : info[n].calledLists) {
                called.insert(c);
            }
        }
        // Also limb lists the actor draws itself (the Bari's hood: a limb and drawn again by EnVali_Draw).
        std::vector<std::string> candidates;
        for (const std::string& n : lists) {
            const bool actorDrawn = ActorReferencedLists().count(n) != 0;
            // A list whose texture the actor picks at runtime (the item drop sprite) is many things: never replaced.
            if (info[n].segmentTextures) {
                continue;
            }
            // Link's body is the skinned OoT3D body (oot3d-tools link pack): his hand, fist and body lists stay to it
            // (the pack empties them); only his equipment is converted here.
            // Lists on Link's limbs are the Link pack's (oot3d-tools link_equip.py: rebuilt in the 3DS limb frames
            // its animations use). Only the lists drawn with their own matrices are converted here.
            if (StartsWith(object, "object_link_")) {
                static const std::set<std::string> kOwnMatrix = {
                    "objects/object_link_boy/gLinkAdultHookshotChainDL",
                    "objects/object_link_boy/gLinkAdultBrokenGiantsKnifeBladeDL",
                    "objects/object_link_child/gLinkChildDekuShieldWithMatrixDL",
                };
                if (!kOwnMatrix.count(n)) {
                    continue;
                }
            }
            if ((!limbLists.count(n) || actorDrawn) && !called.count(n) && info[n].vertexCount >= 3) {
                candidates.push_back(n);
            }
        }
        if (candidates.empty()) {
            continue;
        }
        // 3DS static models, from the object's archives.
        std::vector<Model> models;
        for (const std::string& z : archives) {
            const RomIndex::Archive* archive = index.Load(z);
            if (!archive) {
                log(LogLevel::Warning, z + ": unreadable");
                continue;
            }
            for (const auto& [fileName, cmb] : archive->cmbs) {
                if (!IsStatic(cmb)) {
                    continue;
                }
                Model m;
                m.name = archives.size() > 1 && z != zarPath ? z + ":" + fileName : fileName;
                m.zar = z;
                m.file = fileName;
                m.cmb = cmb;
                const auto world = m.cmb.BindPose();
                std::vector<Mat34> inv;
                for (const auto& w : world) {
                    inv.push_back(w.Inverse());
                }
                for (const auto& set : m.cmb.Decode()) {
                    for (const auto& v : set.vertices) {
                        const Vec3 p = SkinPosition(v, set.skinning, world, inv);
                        m.points.push_back(p);
                        m.box.Add(p);
                    }
                }
                if (m.points.size() >= 3) {
                    models.push_back(std::move(m));
                }
            }
        }
        if (models.empty()) {
            continue;
        }
        // Pair by box score, best first, one model per list; the textures' colors and the names must agree too (the
        // box alone pairs any two flat sprites, and a large archive like zelda_keep has dozens).
        struct Pair {
            float score;
            size_t list, model;
            float color, name;
        };
        std::vector<std::optional<Vec3>> modelColor;
        for (const Model& m : models) {
            modelColor.push_back(ModelColor(m.cmb));
        }
        // Lists a curated variant model is written into ("<carrier><<geometry>", below) stay out of the pairing.
        std::set<std::string> carried;
        for (const auto& [key, value] : ForcedPairs()) {
            if (key.starts_with("objects/" + object + "/") && key.find('<') != std::string::npos) {
                carried.insert(key.substr(0, key.find('<')));
            }
        }
        std::vector<Pair> pairs;
        for (size_t i = 0; i < candidates.size(); i++) {
            if (carried.count(candidates[i])) {
                continue;
            }
            Box vb;
            for (const Vec3& p : info[candidates[i]].positions) {
                vb.Add(p);
            }
            const std::optional<Vec3> listColor = ListColor(oot, candidates[i]);
            const std::vector<std::string> tokens = NameTokens(candidates[i]);
            for (size_t j = 0; j < models.size(); j++) {
                const float color = listColor && modelColor[j] ? ColorDistance(*listColor, *modelColor[j]) : 0.5f;
                const float name = NameMatch(tokens, models[j].name);
                float score = BoxScore(vb, models[j].box) + 0.3f * color - 0.2f * name;
                // Hand-checked pairings win over the scores (Oot3dObjectCuration.inc).
                const auto& kForced = ForcedPairs();
                if (auto fit = kForced.find(candidates[i]); fit != kForced.end()) {
                    const std::string want = CuratedModel(fit->second);
                    if (want == "-" || want == "0" || !EndsWith(models[j].name, want)) {
                        continue;
                    }
                    pairs.push_back({ -100.0f, i, j, 0.0f, 1.0f });
                    continue;
                }
                pairs.push_back({ score, i, j, color, name });
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](const Pair& a, const Pair& b) { return a.score < b.score; });
        std::set<size_t> usedLists, usedModels;
        const bool single = candidates.size() == 1 && models.size() == 1;
        for (const Pair& p : pairs) {
            // A hand-checked pair may share its model with another list (two block sizes from one model).
            if (usedLists.count(p.list) || (usedModels.count(p.model) && p.score > -50.0f)) {
                continue;
            }
            const std::string& listPath = candidates[p.list];
            const VanillaDlInfo& vi = info[listPath];
            Model& model = models[p.model];
            // Colors far apart (a bomb for a heart piece) or a flat sprite without its name or its colors: not it.
            // A lone list and model in their object need less.
            const Vec3 vs = [&] {
                Box b;
                for (const Vec3& q : vi.positions) {
                    b.Add(q);
                }
                return b.Size();
            }();
            const bool flat = vi.vertexCount <= 8 || std::min({ vs.x, vs.y, vs.z }) < 0.02f * vs.Length();
            if (!single && p.name < 0.75f && (p.color > 0.9f || (flat && p.color > 0.35f))) {
                log(LogLevel::Detail, "  no match " + listPath + " ~ " + model.name + " (colors " +
                                          std::to_string(p.color) + (flat ? ", flat" : "") + ")");
                continue;
            }
            Mat34 transform = Mat34::Identity();
            std::string how = "box";
            CuratedOpts curated;
            if (auto fit = ForcedPairs().find(listPath); fit != ForcedPairs().end()) {
                CuratedModel(fit->second, &curated);
                transform = curated.Transform();
            }
            if (p.score > 0.12f) {
                // Not the same placement: try a rigid alignment when the sizes are comparable.
                Box vb;
                for (const Vec3& q : vi.positions) {
                    vb.Add(q);
                }
                const float ratio = model.box.Diag() / std::max(1.0f, vb.Diag());
                if (ratio < 0.6f || ratio > 1.6f || (!single && p.score > 0.6f)) {
                    continue;
                }
                float err = 0;
                transform = Align(model.points, vi.positions, err);
                if (err > 0.08f * vb.Diag()) {
                    log(LogLevel::Detail, "  no match " + listPath + " ~ " + model.name + " (box " +
                                              std::to_string(p.score) + ", aligned error " + std::to_string(err) + ")");
                    continue;
                }
                how = "aligned";
            }
            usedLists.insert(p.list);
            usedModels.insert(p.model);
            MeshOptions mo;
            mo.n64Lighting = vi.lighting;
            static const std::set<std::string> kXlu = {
#include "Oot3dXluLists.inc"
            };
            mo.drawnTranslucent = kXlu.count(listPath) != 0;
            mo.tintEnv = vi.usesEnv && !vi.setsEnv;
            mo.tintPrim = vi.usesPrim && !vi.setsPrim;
            // One list recolored per use by the actor (get-item models drawn with a color list first).
            // Push blocks: ObjOshihiki colors them per scene (ENV); Grezzo's textures are colored (blue-grey, red).
            static const std::set<std::string> kRecolored = {
                "objects/object_gi_clothes/gGiTunicDL",   "objects/object_gi_clothes/gGiTunicCollarDL",
                "objects/object_pu_box/gBlockSmallDL",    "objects/object_pu_box/gBlockMediumDL",
                "objects/object_pu_box/gBlockTallDL",     "objects/object_pu_box/gBlockTallestDL",
            };
            mo.desaturate = kRecolored.count(listPath) != 0;
            mo.envFlash = curated.flash;
            if (mo.envFlash) {
                mo.tintEnv = mo.tintPrim = false;
            }
            if (curated.prim) {
                mo.tintPrim = true;
                mo.tintEnv = false;
            }
            // A glow: drawn translucent with every vanilla texture intensity-only (the fairy's light, sparkles).
            if (mo.drawnTranslucent) {
                bool any = false, allIntensity = true;
                for (const std::string& t : vi.textures) {
                    if (auto tex = ReadVanillaTexture(oot, t)) {
                        any = true;
                        allIntensity &= tex->type >= 5 && tex->type <= 9;
                    }
                }
                mo.glow = any && allIntensity;
            }
            mo.primAlphaFade = (vi.usesPrimAlpha && !vi.setsPrim) || curated.prim;
            mo.envPass = true;
            mo.constantTints = true;
            MeshBuilder mesh(zip, textures, mo, opt.log);
            std::map<int, ScrollRate> rates;
            if (const RomIndex::Archive* a = index.Load(model.zar); a && a->zar) {
                if (const std::string c = CmabFor(*a->zar, model.file); !c.empty()) {
                    ScrollFromCmab(*a->zar, c, mesh, rates);
                }
            }
            mesh.Add(model.cmb, 0, transform);
            const std::string name = listPath.substr(listPath.find_last_of('/') + 1);
            const std::string emitted =
                mesh.Emit(0, MeshBuilder::Part::Both, prefix + listPath, "oot3d/obj/" + object + "/" + name + "_vtx");
            res.warnings += mesh.warnings;
            for (ScrollEntry& se : mesh.scrolls) {
                log(LogLevel::Detail, "  scroll " + listPath + " -> " + se.path + " (" + std::to_string(se.tiles.size()) +
                                          " tiles)");
                res.scrolls.push_back(std::move(se));
            }
            if (!emitted.empty()) {
                // A camera-facing sprite (curated "^"): the engine billboards the list when this marker exists.
                if (auto fit = ForcedPairs().find(listPath); fit != ForcedPairs().end() && fit->second.starts_with("^")) {
                    Bytes marker(68, 0);
                    zip.Add(prefix + "oot3d/billboard/" + listPath, marker);
                }
                res.converted++;
                log(LogLevel::Detail, "object " + listPath + " <- " + zarPath + ":" + model.name + " (" + how +
                                          ", score " + std::to_string(p.score) + ", colors " + std::to_string(p.color) +
                                          ", name " + std::to_string(p.name) + (vi.lighting ? ", lit" : "") + ")");
            }
        }
        // Curated lists outside the pairing, where the N64 recolors one list per use and Grezzo made a model per variant:
        // "<list>+<variant>" writes a new list <list>_<variant> the actor picks (the yellow oval tree);
        // "<carrier><<geometry list>" writes the variant's whole model into a list only that variant draws (a get-item's
        // color list: quivers, wallets, potions), lit like the geometry list, untinted (the colors are the model's).
        for (const auto& [key, value] : ForcedPairs()) {
            if (!key.starts_with("objects/" + object + "/")) {
                continue;
            }
            const size_t plus = key.find('+'), lt = key.find('<');
            std::string base, outPath;
            bool carrier = false;
            if (plus != std::string::npos) {
                base = key.substr(0, plus);
                outPath = base + "_" + key.substr(plus + 1);
            } else if (lt != std::string::npos) {
                outPath = key.substr(0, lt);
                base = "objects/" + object + "/" + key.substr(lt + 1);
                carrier = true;
            } else {
                continue;
            }
            CuratedOpts curated;
            const std::string want = CuratedModel(value, &curated);
            auto vit = info.find(base);
            for (Model& model : models) {
                if (!EndsWith(model.name, want) || vit == info.end()) {
                    continue;
                }
                const VanillaDlInfo& vi = vit->second;
                MeshOptions mo;
                mo.n64Lighting = vi.lighting;
                mo.tintEnv = !carrier && vi.usesEnv && !vi.setsEnv;
                mo.tintPrim = !carrier && vi.usesPrim && !vi.setsPrim;
                mo.envFlash = curated.flash;
                if (mo.envFlash) {
                    mo.tintEnv = mo.tintPrim = false;
                }
                mo.primAlphaFade = !carrier && vi.usesPrimAlpha && !vi.setsPrim;
                mo.envPass = true;
                mo.constantTints = true;
                MeshBuilder mesh(zip, textures, mo, opt.log);
                std::map<int, ScrollRate> rates;
                if (const RomIndex::Archive* a = index.Load(model.zar); a && a->zar) {
                    if (const std::string c = CmabFor(*a->zar, model.file); !c.empty()) {
                        ScrollFromCmab(*a->zar, c, mesh, rates);
                    }
                }
                mesh.Add(model.cmb, 0, curated.Transform());
                const std::string name = outPath.substr(outPath.find_last_of('/') + 1);
                if (!mesh.Emit(0, MeshBuilder::Part::Both, prefix + outPath, "oot3d/obj/" + object + "/" + name + "_vtx")
                         .empty()) {
                    if (value.starts_with("^")) {
                        zip.Add(prefix + "oot3d/billboard/" + outPath, Bytes(68, 0));
                    }
                    res.converted++;
                    log(LogLevel::Detail, "object " + outPath + " <- " + zarPath + ":" + model.name + " (curated variant)");
                }
                res.warnings += mesh.warnings;
                for (ScrollEntry& se : mesh.scrolls) {
                    log(LogLevel::Detail, "  scroll " + outPath + " -> " + se.path + " (" +
                                              std::to_string(se.tiles.size()) + " tiles)");
                    res.scrolls.push_back(std::move(se));
                }
                break;
            }
        }
        for (size_t i = 0; i < candidates.size(); i++) {
            if (carried.count(candidates[i])) {
                usedLists.insert(i);
            }
        }
        // Lists curated empty ("0": a part the 3DS model already has, e.g. the Fairy Ocarina's holes).
        for (size_t i = 0; i < candidates.size(); i++) {
            auto fit = ForcedPairs().find(candidates[i]);
            if (fit != ForcedPairs().end() && fit->second == "0") {
                Soh::DisplayList empty;
                empty.End();
                zip.Add(prefix + candidates[i], empty.Resource());
                res.emptied++;
                usedLists.insert(i);
                log(LogLevel::Detail, "object " + candidates[i] + " emptied (curated)");
            }
        }
        // Unpaired lists inside a replaced model's space are the N64's separate parts of it (a quiver's inside, a
        // bottle's glass): they would draw through the 3DS model, so they become empty lists.
        if (usedLists.empty()) {
            continue;
        }
        Box covered;
        for (size_t i : usedLists) {
            for (const Vec3& q : info[candidates[i]].positions) {
                covered.Add(q);
            }
        }
        const Vec3 slack = covered.Size() * 0.1f;
        for (size_t i = 0; i < candidates.size(); i++) {
            if (usedLists.count(i)) {
                continue;
            }
            Box b;
            for (const Vec3& q : info[candidates[i]].positions) {
                b.Add(q);
            }
            const bool inside = b.min.x >= covered.min.x - slack.x && b.min.y >= covered.min.y - slack.y &&
                                b.min.z >= covered.min.z - slack.z && b.max.x <= covered.max.x + slack.x &&
                                b.max.y <= covered.max.y + slack.y && b.max.z <= covered.max.z + slack.z;
            // Get-item objects are one item each (their archives also hold variants, e.g. three quiver sizes), so
            // any unpaired piece inside it goes; elsewhere only when every 3DS model found its list.
            const bool itemObject = StartsWith(object, "object_gi_");
            if (inside && (itemObject || usedModels.size() == models.size())) {
                Soh::DisplayList empty;
                empty.End();
                zip.Add(prefix + candidates[i], empty.Resource());
                res.emptied++;
                log(LogLevel::Detail, "object " + candidates[i] + " emptied (part of a replaced model)");
            } else {
                res.kept++;
                log(LogLevel::Detail, "object " + candidates[i] + " kept (no 3DS model pairs with it)");
            }
        }
    }
    if (opt.progress) {
        opt.progress("Objects", objects.size(), objects.size());
    }
    return res;
}

} // namespace Oot3d
