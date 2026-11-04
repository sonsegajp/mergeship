// OoT3D asset converter: room geometry.
//
// For every vanilla room SoH has, the matching 3DS room (ZSI) is converted into new display lists, and the room
// resource is rewritten so its mesh command points at them (every other command is copied byte for byte). N64
// prerendered rooms (shops, houses) become real 3D rooms this way. Culling spheres of the vanilla room are kept, and
// each is widened to contain the 3DS geometry assigned to it.

#include "Oot3dMaps.h"

#include "Oot3dMesh.h"
#include "Oot3dRom.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZsi.h"
#include "Oot3dCmab.h"
#include "Oot3dZar.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace Oot3d {

// -- textures --------------------------------------------------------------------------------------------------------

TextureStore::TextureStore(ZipWriter& zip) : mZip(zip) {
}

std::string TextureStore::Add(const Image& img, uint32_t logicalW, uint32_t logicalH) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (uint8_t b : img.rgba) {
        h = (h ^ b) * 0x100000001B3ULL;
    }
    h ^= (uint64_t(img.width) << 32) | img.height;
    char name[96];
    snprintf(name, sizeof(name), "oot3d/tex/%016llx_%ux%u", static_cast<unsigned long long>(h), logicalW, logicalH);
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (!mWritten.insert(name).second) {
            return name;
        }
    }
    Soh::HdTexture t;
    t.image = img;
    t.logicalWidth = logicalW;
    t.logicalHeight = logicalH;
    mZip.Add(name, Soh::TextureResource(t));
    mCount++;
    return name;
}

// -- scenes ----------------------------------------------------------------------------------------------------------

// Vanilla (N64/SoH) scene base name -> Grezzo's 3DS name, where they differ (lower case).
static const std::map<std::string, std::string> kSceneAliases = {
    { "hakadanch", "hakadan_ch" },   { "hakadan_bs", "hakadan_boss" }, { "mizusin_bs", "mizusin_boss" },
    { "alley_shop", "shop_alley" },  { "drag", "shop_drag" },          { "enrui", "entra_ruins" },
    { "entra", "entra_day" },        { "entra_n", "entra_night" },     { "face_shop", "shop_face" },
    { "golon", "shop_golon" },       { "impa", "kakariko_impa" },      { "jyasinboss", "jyasinzou_boss" },
    { "kakariko3", "kakariko_home3" }, { "kokiri_home", "k_home" },    { "kokiri_home3", "k_home3" },
    { "kokiri_home4", "k_home4" },   { "kokiri_home5", "k_home5" },    { "kokiri_shop", "kokiri" },
    { "link_home", "link" },         { "malon_stable", "stable" },     { "moribossroom", "moriboss" },
    { "night_shop", "shop_night" },  { "shop1", "shop" },
};

SceneLight LightFromEnvironment(const ZsiEnvironment* env) {
    SceneLight l;
    if (!env) {
        return l;
    }
    auto dir = [](const std::array<int8_t, 3>& d) { return Vec3(d[0] / 127.0f, d[1] / 127.0f, d[2] / 127.0f); };
    const Vec3 d0 = dir(env->light0Dir), d1 = dir(env->light1Dir);
    if (d0.Length() > 1e-4f || d1.Length() > 1e-4f) {
        l.dir[0] = d0;
        l.dir[1] = d1;
    }
    for (int c = 0; c < 3; c++) {
        l.diffuse[0][c] = env->light0Color[c] / 255.0f;
        l.diffuse[1][c] = env->light1Color[c] / 255.0f;
        l.ambient[0][c] = env->ambient[c] / 255.0f;
        l.ambient[1][c] = 0;
    }
    return l;
}

namespace {

// Converts one 3DS room into display lists for the vanilla room's mesh entries; returns the new entries.
std::vector<VanillaMeshEntry> ConvertRoom(const ZsiRoom& room, const VanillaRoom& vanilla, const std::string& base,
                                          MeshBuilder& mesh, int& meshType, std::string& note) {
    const ZsiRoomSetup* setup = nullptr;
    for (const auto& s : room.setups) {
        if (s.opaque || s.translucent) {
            setup = &s;
            break;
        }
    }
    if (!setup) {
        throw FormatError("the 3DS room has no mesh");
    }
    meshType = vanilla.meshType == 2 ? 2 : 0;
    const size_t entryCount = meshType == 2 ? std::max<size_t>(vanilla.entries.size(), 1) : 1;
    // Each triangle goes to the vanilla culling sphere it is deepest inside of.
    auto bucket = [&](const Vec3& centroid) -> size_t {
        if (meshType != 2 || entryCount < 2) {
            return 0;
        }
        size_t entry = 0;
        float best = 1e30f;
        for (size_t e = 0; e < vanilla.entries.size(); e++) {
            const auto& ve = vanilla.entries[e];
            const float d = (centroid - Vec3(ve.x, ve.y, ve.z)).Length() - std::max<float>(ve.radius, 1.0f);
            if (d < best) {
                best = d;
                entry = e;
            }
        }
        return entry;
    };
    if (setup->opaque) {
        mesh.Add(*setup->opaque, 0, Mat34::Identity(), bucket);
    }
    if (setup->translucent) {
        mesh.Add(*setup->translucent, 1, Mat34::Identity(), bucket);
    }
    std::vector<VanillaMeshEntry> out;
    for (size_t e = 0; e < entryCount; e++) {
        VanillaMeshEntry ve;
        if (meshType == 2) {
            const VanillaMeshEntry& src = vanilla.entries[e];
            ve.hasSphere = true;
            ve.x = src.x, ve.y = src.y, ve.z = src.z;
            const float radius = std::max<float>(src.radius, mesh.Radius(e, Vec3(src.x, src.y, src.z)) + 1.0f);
            ve.radius = src.radius < 0 ? src.radius : static_cast<int16_t>(std::min(radius, 32767.0f));
        }
        const std::string prefix = base + "_e" + std::to_string(e);
        ve.opa = mesh.Emit(e, MeshBuilder::Part::Opaque, prefix + "_opa");
        ve.xlu = mesh.Emit(e, MeshBuilder::Part::Translucent, prefix + "_xlu");
        out.push_back(ve);
    }
    std::ostringstream n;
    n << entryCount << " entr" << (entryCount == 1 ? "y" : "ies");
    note = n.str();
    return out;
}

} // namespace

MapsResult ConvertMaps(const RomFS& rom, const ZipReader& oot, ZipWriter& zip, TextureStore& textures,
                       const ExtractOptions& opt) {
    MapsResult res;
    auto log = [&](LogLevel l, const std::string& s) {
        if (opt.log) {
            opt.log(l, s);
        }
    };
    // Vanilla rooms: scenes/<kind>/<name>_scene/<name>_room_<n>
    static const std::regex roomRx(R"(^scenes/(nonmq|shared|mq)/([^/]+)_scene/\2_room_(\d+)$)");
    struct VRoom {
        std::string path, kind, scene;
        int number;
    };
    std::vector<VRoom> vrooms;
    for (const std::string& name : oot.Names()) {
        std::smatch mm;
        if (std::regex_match(name, mm, roomRx)) {
            vrooms.push_back({ name, mm[1], mm[2], std::stoi(mm[3]) });
        }
    }
    std::sort(vrooms.begin(), vrooms.end(), [](const VRoom& a, const VRoom& b) {
        return std::tie(a.scene, a.kind, a.number) < std::tie(b.scene, b.kind, b.number);
    });
    if (!opt.onlyScenes.empty()) {
        std::set<std::string> want;
        for (const auto& s : opt.onlyScenes) {
            std::string k = ToLower(s);
            if (EndsWith(k, "_scene")) {
                k = k.substr(0, k.size() - 6);
            }
            want.insert(k);
        }
        vrooms.erase(std::remove_if(vrooms.begin(), vrooms.end(),
                                    [&](const VRoom& r) { return !want.count(ToLower(r.scene)); }),
                     vrooms.end());
    }

    std::vector<ScrollEntry> scrollTable;
    std::map<std::string, std::optional<SceneLight>> lights;
    uint64_t done = 0;
    for (const VRoom& vr : vrooms) {
        if (opt.cancel && *opt.cancel) {
            break;
        }
        if (opt.progress) {
            opt.progress("Rooms", done, vrooms.size());
        }
        done++;
        std::string name3ds = ToLower(vr.scene);
        if (auto it = kSceneAliases.find(name3ds); it != kSceneAliases.end()) {
            name3ds = it->second;
        }
        const bool mq = vr.kind == "mq";
        std::string zsiPath = "scene/" + name3ds + "_" + std::to_string(vr.number) + (mq ? "_dd" : "") + "_info.zsi";
        if (!rom.Exists(zsiPath) && mq) {
            zsiPath = "scene/" + name3ds + "_" + std::to_string(vr.number) + "_info.zsi";
        }
        if (!rom.Exists(zsiPath)) {
            res.skipped++;
            log(LogLevel::Detail, "skip " + vr.path + ": no " + zsiPath);
            continue;
        }
        // Scene lighting for baking the 3DS vertex lighting: the scene's usual environment (day, where it has one).
        const std::string lightKey = name3ds + (mq ? "_dd" : "");
        if (!lights.count(lightKey)) {
            std::optional<SceneLight> sl;
            for (const std::string& sp : { "scene/" + lightKey + "_info.zsi", "scene/" + name3ds + "_info.zsi" }) {
                if (rom.Exists(sp)) {
                    try {
                        ZsiScene sc = ZsiScene::Parse(rom.Read(sp));
                        sl = LightFromEnvironment(sc.DefaultEnvironment());
                        break;
                    } catch (const std::exception& e) {
                        log(LogLevel::Warning, sp + ": " + e.what());
                    }
                }
            }
            lights[lightKey] = sl;
        }

        try {
            auto roomBytes = oot.Read(vr.path);
            if (!roomBytes) {
                throw FormatError("unreadable");
            }
            VanillaRoom vanilla = ParseVanillaRoom(*roomBytes);
            if (vanilla.meshType < 0) {
                throw FormatError("vanilla room has no mesh");
            }
            ZsiRoom room = ZsiRoom::Parse(rom.Read(zsiPath));
            MeshOptions mo;
            mo.light = lights[lightKey].value_or(SceneLight{});
            // N64 prerendered rooms: their scene fog was never meant for geometry and washes a 3D room out.
            mo.allowFog = vanilla.meshType != 1;
            MeshBuilder mesh(zip, textures, mo, opt.log);
            // Texture scrolls (water, lava, waterfalls): the room models' CMAB translation tracks, in the scene archive
            // (ROOM<n>\<model>.cmab).
            std::map<std::string, std::map<int, ScrollRate>> scrollByModel; // lower model name -> material -> rate
            for (const std::string& zp : { "scene/" + name3ds + (mq ? "_dd" : "") + ".zar", "scene/" + name3ds + ".zar" }) {
                if (!rom.Exists(zp) || !scrollByModel.empty()) {
                    continue;
                }
                try {
                    const Zar sz = Zar::Parse(rom.Read(zp));
                    const std::string roomDir = "room" + std::to_string(vr.number);
                    for (const ZarFile& f : sz.files) {
                        std::string n = ToLower(f.name);
                        std::replace(n.begin(), n.end(), '\\', '/');
                        if (!EndsWith(n, ".cmab") || !StartsWith(n, roomDir + "/")) {
                            continue;
                        }
                        const std::string stem = n.substr(roomDir.size() + 1, n.size() - roomDir.size() - 6);
                        const Cmab c = Cmab::Parse(f.data);
                        for (const CmabEntry& e : c.entries) {
                            if (e.type != CmabEntry::Translation || e.channel > 1) {
                                continue;
                            }
                            ScrollRate& r = scrollByModel[stem][int(e.material)];
                            for (int k = 0; k < 2 && k < int(e.tracks.size()); k++) {
                                const auto& keys = e.tracks[k].keys;
                                if (!e.present[k] || keys.size() < 2 || keys.back().time <= keys.front().time) {
                                    continue;
                                }
                                const float rate = (keys.back().value - keys.front().value) /
                                                   (keys.back().time - keys.front().time);
                                (k == 0 ? r.s : r.t)[e.channel] = rate;
                            }
                        }
                    }
                } catch (const std::exception& ex) {
                    log(LogLevel::Detail, zp + " material animations: " + ex.what());
                }
            }
            mesh.scrollFor = [&](const MeshGroup& g) -> const ScrollRate* {
                if (!g.cmb || !g.src) {
                    return nullptr;
                }
                auto it = scrollByModel.find(ToLower(g.cmb->name));
                if (it == scrollByModel.end()) {
                    return nullptr;
                }
                auto jt = it->second.find(static_cast<int>(g.src - g.cmb->materials.data()));
                return jt == it->second.end() ? nullptr : &jt->second;
            };
            int meshType = 0;
            std::string note;
            const std::string base = "oot3d/" + vr.kind + "/" + vr.scene + "/room" + std::to_string(vr.number);
            auto entries = ConvertRoom(room, vanilla, base, mesh, meshType, note);
            res.warnings += mesh.warnings;
            const std::string prefix = opt.altAssets ? "alt/" : "";
            zip.Add(prefix + vr.path, RewriteRoomMesh(*roomBytes, meshType, entries));
            // Alternate headers of the room carry their own mesh command.
            for (const std::string& alt : vanilla.alternateHeaders) {
                if (alt.empty()) {
                    continue;
                }
                auto altBytes = oot.Read(alt);
                if (!altBytes) {
                    continue;
                }
                if (ParseVanillaRoom(*altBytes).meshType >= 0) {
                    zip.Add(prefix + alt, RewriteRoomMesh(*altBytes, meshType, entries));
                }
            }
            res.converted++;
            for (ScrollEntry& se : mesh.scrolls) {
                scrollTable.push_back(std::move(se));
            }
            log(LogLevel::Detail, "room " + vr.path + " <- " + zsiPath + " (" + note + ")");
        } catch (const std::exception& e) {
            res.skipped++;
            res.warnings++;
            log(LogLevel::Warning, "room " + vr.path + ": " + e.what());
        }
    }
    // The scroll table the engine walks every frame (MergeShip3dsScroll): "SCR1", count, then per entry the list path
    // and its tiles {tile, width, height, uls, ult, rate s, rate t (10.2 per 3DS frame)}.
    if (!scrollTable.empty()) {
        Writer w;
        w.Raw("SCR1", 4);
        w.U32(static_cast<uint32_t>(scrollTable.size()));
        for (const ScrollEntry& se : scrollTable) {
            w.U16(static_cast<uint16_t>(se.path.size()));
            w.Raw(se.path.data(), se.path.size());
            w.U8(static_cast<uint8_t>(se.tiles.size()));
            for (const auto& t : se.tiles) {
                w.U8(t.tile);
                w.U16(t.width);
                w.U16(t.height);
                w.U16(t.uls);
                w.U16(t.ult);
                w.F32(t.rateS);
                w.F32(t.rateT);
            }
        }
        Writer blob;
        const Bytes hdr = Soh::Header(0x4F424C42, 0); // Blob
        blob.Raw(hdr.data(), hdr.size());
        blob.U32(static_cast<uint32_t>(w.Size()));
        blob.Raw(w.Out().data(), w.Size());
        zip.Add("oot3d/scroll/table", blob.Out());
        log(LogLevel::Info, std::to_string(scrollTable.size()) + " scrolling room materials");
    }
    if (opt.progress) {
        opt.progress("Rooms", vrooms.size(), vrooms.size());
    }
    return res;
}

} // namespace Oot3d
