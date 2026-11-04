// OoT3D asset converter: diagnostics for the command line tool (check, dump, tevstats).

#include "Oot3dCmb.h"
#include "Oot3dCsab.h"
#include "Oot3dExtract.h"
#include "Oot3dMaterial.h"
#include "Oot3dRom.h"
#include "Oot3dSkeleton.h"
#include "Oot3dAnim.h"
#include "Oot3dAnimPair.h"
#include "Oot3dZip.h"
#include "Oot3dSoh.h"
#include "Oot3dVanilla.h"
#include "Oot3dZar.h"
#include "Oot3dZsi.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace Oot3d {

namespace fs = std::filesystem;

std::string CheckReport::Summary() const {
    std::ostringstream s;
    s << files << " files checked: " << zars << " archives, " << cmbs << " models, " << csabs << " animations, "
      << ctxbs << " texture files, " << zsis << " scene/room files, " << textures << " textures decoded; " << failures
      << " failure(s)";
    return s.str();
}

static void WriteFile(const fs::path& p, const void* data, size_t size) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary).write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
}

static void CheckCmb(View data, CheckReport& r) {
    Cmb cmb = Cmb::Parse(data);
    r.cmbs++;
    try {
        cmb.Decode();
    } catch (const FormatError& e) {
        throw FormatError(std::string("decoding vertices: ") + e.what());
    }
    cmb.BindPose();
    for (size_t i = 0; i < cmb.textures.size(); i++) {
        if (cmb.textures[i].format == PicaFormat::Unknown) {
            throw FormatError("texture " + cmb.textures[i].name + " has unknown format");
        }
        if (!cmb.texData.Empty()) {
            cmb.DecodeTexture(i);
            r.textures++;
        }
    }
}

static void CheckInner(const std::string& name, View data, CheckReport& r) {
    const std::string lower = ToLower(name);
    if (EndsWith(lower, ".cmb")) {
        CheckCmb(data, r);
    } else if (EndsWith(lower, ".csab")) {
        Csab::Parse(data);
        r.csabs++;
    } else if (EndsWith(lower, ".ctxb")) {
        Ctxb c = ParseCtxb(data);
        for (size_t i = 0; i < c.textures.size(); i++) {
            c.Decode(i);
            r.textures++;
        }
        r.ctxbs++;
    }
}

CheckReport CheckRom(const RomFS& rom, const std::function<void(const std::string&)>& print) {
    CheckReport r;
    for (const std::string& path : rom.List("*")) {
        const std::string lower = ToLower(path);
        const bool zar = EndsWith(lower, ".zar") || EndsWith(lower, ".gar");
        const bool zsi = EndsWith(lower, ".zsi");
        if (!zar && !zsi && !EndsWith(lower, ".cmb") && !EndsWith(lower, ".ctxb") && !EndsWith(lower, ".csab")) {
            continue;
        }
        r.files++;
        std::string where = path;
        try {
            if (zar) {
                Zar z = Zar::Parse(rom.Read(path));
                r.zars++;
                for (const ZarFile& f : z.files) {
                    where = path + ":" + f.name;
                    CheckInner(f.name, f.data, r);
                }
            } else if (zsi) {
                ZsiRoom room = ZsiRoom::Parse(rom.Read(path));
                r.zsis++;
                for (auto& setup : room.setups) {
                    for (auto* m : { &setup.opaque, &setup.translucent }) {
                        if (*m) {
                            (*m)->Decode();
                            r.cmbs++;
                            for (size_t i = 0; i < (*m)->textures.size(); i++) {
                                if (!(*m)->texData.Empty()) {
                                    (*m)->DecodeTexture(i);
                                    r.textures++;
                                }
                            }
                        }
                    }
                }
            } else {
                Bytes b = rom.Read(path);
                CheckInner(path, View(b), r);
            }
        } catch (const std::exception& e) {
            r.failures++;
            print("FAIL " + where + ": " + e.what());
        }
    }
    return r;
}

void DumpFiles(const RomFS& rom, const std::string& pattern, const std::string& outDir,
               const std::function<void(const std::string&)>& print) {
    const fs::path out = U8Path(outDir);
    auto dumpTextures = [&](const fs::path& base, const Cmb& cmb) {
        for (size_t i = 0; i < cmb.textures.size() && !cmb.texData.Empty(); i++) {
            Image img = cmb.DecodeTexture(i);
            Bytes png = EncodePng(img);
            char name[160];
            snprintf(name, sizeof(name), "%s_tex%02zu_%s_%s.png", cmb.name.c_str(), i, cmb.textures[i].name.c_str(),
                     PicaFormatName(cmb.textures[i].format));
            WriteFile(base / name, png.data(), png.size());
        }
    };
    for (const std::string& path : rom.List(pattern)) {
        Bytes b = rom.Read(path);
        const fs::path dst = out / U8Path(path);
        WriteFile(dst, b.data(), b.size());
        const std::string lower = ToLower(path);
        try {
            if (EndsWith(lower, ".ctxb")) {
                Ctxb c = ParseCtxb(View(b));
                for (size_t i = 0; i < c.textures.size(); i++) {
                    Bytes png = EncodePng(c.Decode(i));
                    WriteFile(fs::path(dst).replace_extension("." + std::to_string(i) + ".png"), png.data(),
                              png.size());
                }
            } else if (EndsWith(lower, ".zar") || EndsWith(lower, ".gar")) {
                Zar z = Zar::Parse(std::move(b));
                const fs::path dir = fs::path(dst).replace_extension("");
                for (const ZarFile& f : z.files) {
                    WriteFile(dir / U8Path(f.name), f.data.Data(), f.data.Size());
                    if (EndsWith(ToLower(f.name), ".cmb")) {
                        dumpTextures(dir / "textures", Cmb::Parse(f.data));
                    } else if (EndsWith(ToLower(f.name), ".ctxb")) {
                        Ctxb c = ParseCtxb(f.data);
                        for (size_t i = 0; i < c.textures.size(); i++) {
                            Bytes png = EncodePng(c.Decode(i));
                            WriteFile(dir / "textures" / (StemOf(f.name) + "_" + std::to_string(i) + ".png"),
                                      png.data(), png.size());
                        }
                    }
                }
            } else if (EndsWith(lower, ".zsi")) {
                ZsiRoom room = ZsiRoom::Parse(std::move(b));
                const fs::path dir = fs::path(dst).replace_extension("");
                int s = 0;
                for (auto& setup : room.setups) {
                    for (auto* m : { &setup.opaque, &setup.translucent }) {
                        if (*m) {
                            const std::string nm =
                                "setup" + std::to_string(s) + (m == &setup.opaque ? "_opa.cmb" : "_xlu.cmb");
                            WriteFile(dir / nm, (*m)->file.Data(), (*m)->file.Size());
                            dumpTextures(dir / "textures", **m);
                        }
                    }
                    s++;
                }
            }
        } catch (const std::exception& e) {
            print("  " + path + ": " + e.what());
        }
        print(path);
    }
}

// -- TEV statistics ------------------------------------------------------------------------------------------------

static const char* TevSrcName(uint16_t s) {
    switch (s) {
        case Tev::Texture0: return "T0";
        case Tev::Texture1: return "T1";
        case Tev::Texture2: return "T2";
        case Tev::Texture3: return "T3";
        case Tev::Constant: return "K";
        case Tev::PrimaryColor: return "V";
        case Tev::Previous: return "P";
        case Tev::PreviousBuffer: return "B";
        case Tev::FragPrimary: return "FP";
        case Tev::FragSecondary: return "FS";
        default: return "?";
    }
}

static std::string TevOperand(uint16_t src, uint16_t op, bool alpha) {
    std::string s = TevSrcName(src);
    switch (op) {
        case Tev::SrcColor: return alpha ? s + ".a?" : s;
        case Tev::OneMinusSrcColor: return "(1-" + s + ")";
        case Tev::SrcAlpha: return alpha ? s : s + ".a";
        case Tev::OneMinusSrcAlpha: return "(1-" + s + ".a)";
        case Tev::SrcR: return s + ".r";
        case Tev::SrcG: return s + ".g";
        case Tev::SrcB: return s + ".b";
        case Tev::OneMinusSrcR: return "(1-" + s + ".r)";
        case Tev::OneMinusSrcG: return "(1-" + s + ".g)";
        case Tev::OneMinusSrcB: return "(1-" + s + ".b)";
        default: return s + "?op";
    }
}

static std::string TevExpr(uint16_t combine, const std::array<uint16_t, 3>& src, const std::array<uint16_t, 3>& op,
                           uint16_t scale, bool alpha) {
    const std::string a = TevOperand(src[0], op[0], alpha), b = TevOperand(src[1], op[1], alpha),
                      c = TevOperand(src[2], op[2], alpha);
    std::string e;
    switch (combine) {
        case Tev::Replace: e = a; break;
        case Tev::Modulate: e = a + "*" + b; break;
        case Tev::Add: e = a + "+" + b; break;
        case Tev::AddSigned: e = a + "+" + b + "-.5"; break;
        case Tev::Interpolate: e = "lerp(" + b + "," + a + "," + c + ")"; break;
        case Tev::Subtract: e = a + "-" + b; break;
        case Tev::Dot3Rgb: e = "dot3(" + a + "," + b + ")"; break;
        case Tev::Dot3Rgba: e = "dot3a(" + a + "," + b + ")"; break;
        case Tev::MultAdd: e = a + "*" + b + "+" + c; break;
        case Tev::AddMult: e = "(" + a + "+" + b + ")*" + c; break;
        default: e = "?"; break;
    }
    if (scale != 1) {
        e = "(" + e + ")x" + std::to_string(scale);
    }
    return e;
}

std::string DescribeMaterial(const CmbMaterial& m) {
    std::ostringstream s;
    int textures = 0;
    for (const auto& b : m.bindings) {
        textures += b.texture >= 0;
    }
    s << "tex" << textures << (m.Blended() ? " blend" : "") << (m.alphaTest ? " atest" : "")
      << (m.vertexLighting ? " vlit" : "") << (m.fragmentLighting ? " flit" : "");
    if (m.Blended()) {
        s << " [" << std::hex << m.blendSrcRgb << "," << m.blendDstRgb << std::dec << "]";
    }
    for (int i = 0; i < 3; i++) {
        if (m.bindings[i].texture < 0) {
            continue;
        }
        const CmbTexCoordinator& c = m.coordinators[m.CoordinatorForSlot(i)];
        s << " T" << i << ":uv" << int(c.source);
        if (c.mapping != TexMapping::Uv) {
            s << "/map" << int(c.mapping);
        }
    }
    for (size_t i = 0; i < m.combiners.size(); i++) {
        const CmbCombiner& k = m.combiners[i];
        const bool passRgb = k.combineRgb == Tev::Replace && k.srcRgb[0] == Tev::Previous &&
                             k.opRgb[0] == Tev::SrcColor && k.scaleRgb == 1;
        const bool passA = k.combineAlpha == Tev::Replace && k.srcAlpha[0] == Tev::Previous &&
                           k.opAlpha[0] == Tev::SrcAlpha && k.scaleAlpha == 1;
        if (passRgb && passA) {
            continue;
        }
        s << " | " << i << ": " << (passRgb ? "-" : TevExpr(k.combineRgb, k.srcRgb, k.opRgb, k.scaleRgb, false))
          << " / " << (passA ? "-" : TevExpr(k.combineAlpha, k.srcAlpha, k.opAlpha, k.scaleAlpha, true));
    }
    return s.str();
}

void TevStats(const RomFS& rom, const std::string& pattern, const std::function<void(const std::string&)>& print) {
    std::map<std::string, std::pair<int, std::string>> counts; // description -> (count, example)
    auto visit = [&](const Cmb& cmb, const std::string& where) {
        for (const CmbMaterial& m : cmb.materials) {
            auto& e = counts[DescribeMaterial(m)];
            if (e.first++ == 0) {
                e.second = where + " (" + cmb.name + ")";
            }
        }
    };
    for (const std::string& path : rom.List(pattern)) {
        const std::string lower = ToLower(path);
        try {
            if (EndsWith(lower, ".zsi")) {
                ZsiRoom room = ZsiRoom::Parse(rom.Read(path));
                for (auto& setup : room.setups) {
                    for (auto* m : { &setup.opaque, &setup.translucent }) {
                        if (*m) {
                            visit(**m, path);
                        }
                    }
                }
            } else if (EndsWith(lower, ".zar")) {
                Zar z = Zar::Parse(rom.Read(path));
                for (const ZarFile& f : z.files) {
                    if (EndsWith(ToLower(f.name), ".cmb")) {
                        visit(Cmb::Parse(f.data), path + ":" + f.name);
                    }
                }
            }
        } catch (const std::exception& e) {
            print("FAIL " + path + ": " + e.what());
        }
    }
    std::vector<std::pair<int, std::string>> order;
    for (const auto& [k, v] : counts) {
        order.emplace_back(v.first, k);
    }
    std::sort(order.rbegin(), order.rend());
    for (const auto& [n, k] : order) {
        print(std::to_string(n) + "\t" + k + "\t e.g. " + counts[k].second);
    }
}

} // namespace Oot3d

namespace Oot3d {

static std::string CycleText(const Soh::CcCycle& c) {
    static const char* kA[] = { "COMB", "T0", "T1", "PRIM", "SHADE", "ENV", "1", "NOISE", "?", "?", "?", "?",
                                "?",    "?",  "?",  "0" };
    static const char* kC[] = { "COMB", "T0", "T1", "PRIM", "SHADE", "ENV", "SCALE", "COMB_A", "T0_A", "T1_A", "PRIM_A",
                                "SHADE_A", "ENV_A", "LOD", "PRIMLOD", "K5" };
    static const char* kD[] = { "COMB", "T0", "T1", "PRIM", "SHADE", "ENV", "1", "0" };
    static const char* kAc[] = { "COMB", "T0", "T1", "PRIM", "SHADE", "ENV", "1", "0" };
    auto c5 = [&](uint8_t v) { return v == 31 ? "0" : (v < 16 ? kC[v] : "?"); };
    std::ostringstream s;
    s << "(" << kA[c.a & 15] << "-" << kA[c.b & 15] << ")*" << c5(c.c) << "+" << kD[c.d & 7] << " / (" << kAc[c.aa & 7]
      << "-" << kAc[c.ba & 7] << ")*" << kAc[c.ca & 7] << "+" << kAc[c.da & 7];
    return s.str();
}

void DumpMaterials(const RomFS& rom, const std::string& pattern, const std::function<void(const std::string&)>& print) {
    auto show = [&](const Cmb& cmb) {
        print("model " + cmb.name);
        for (size_t i = 0; i < cmb.materials.size(); i++) {
            const CmbMaterial& m = cmb.materials[i];
            const N64Material n = TranslateMaterial(cmb, m);
            std::ostringstream s;
            s << "  #" << i << " " << DescribeMaterial(m);
            for (uint32_t b = 0; b < m.bindingCount && b < 3; b++) {
                const int t = m.bindings[b].texture;
                s << " [T" << b << "=" << (t >= 0 && size_t(t) < cmb.textures.size() ? cmb.textures[t].name : "-")
                  << "]";
            }
            s << "\n      -> c1 " << CycleText(n.cycle1) << "  c2 "
              << CycleText(n.cycle2) << "  shade x(" << n.shadeScale[0] << "," << n.shadeScale[1] << ","
              << n.shadeScale[2] << ") prim " << int(n.prim[0]) << "," << int(n.prim[1]) << "," << int(n.prim[2])
              << "," << int(n.prim[3]) << " env " << int(n.env[0]) << "," << int(n.env[1]) << "," << int(n.env[2])
              << " tex " << n.texSlot[0] << "," << n.texSlot[1] << (n.approximate ? " APPROX" : "")
              << "  diffuse " << int(m.diffuse[0]) << "," << int(m.diffuse[1]) << "," << int(m.diffuse[2])
              << " ambient " << int(m.ambient[0]) << "," << int(m.ambient[1]) << "," << int(m.ambient[2]);
            for (int k = 0; k < 6; k++) {
                s << " K" << k << "=" << int(m.constants[k][0]) << "," << int(m.constants[k][1]) << ","
                  << int(m.constants[k][2]) << "," << int(m.constants[k][3]);
            }
            print(s.str());
        }
    };
    for (const std::string& path : rom.List(pattern)) {
        const std::string lower = ToLower(path);
        if (EndsWith(lower, ".zsi")) {
            ZsiRoom room = ZsiRoom::Parse(rom.Read(path));
            for (auto& setup : room.setups) {
                for (auto* m : { &setup.opaque, &setup.translucent }) {
                    if (*m) {
                        show(**m);
                    }
                }
                break;
            }
        } else if (EndsWith(lower, ".zar")) {
            Zar z = Zar::Parse(rom.Read(path));
            for (const ZarFile& f : z.files) {
                if (EndsWith(ToLower(f.name), ".cmb")) {
                    show(Cmb::Parse(f.data));
                }
            }
        }
    }
}

} // namespace Oot3d

namespace Oot3d {

// OoT3D actor archives are "actor/zelda_<object>.zar" for "objects/object_<object>", with a few renames.
std::string ZarForObject(const RomFS& rom, const std::string& object) {
    static const std::map<std::string, std::string> kAliases = {
        { "link_boy", "link_boy_new" },
        { "link_child", "link_child_new" },
        { "gameplay_keep", "keep" },
        { "gameplay_field_keep", "field_keep" },
        { "gameplay_dangeon_keep", "dangeon_keep" },
    };
    std::string base = object;
    if (StartsWith(base, "object_")) {
        base = base.substr(7);
    }
    base = ToLower(base);
    if (auto it = kAliases.find(base); it != kAliases.end()) {
        base = it->second;
    }
    const std::string p = "actor/zelda_" + base + ".zar";
    return rom.Exists(p) ? p : "";
}

void SkelMatch(const RomFS& rom, const std::string& ootPath, const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    std::map<std::string, std::vector<std::string>> byObject; // object -> skeleton paths
    for (const std::string& n : oot->Names()) {
        if (!StartsWith(n, "objects/") || !EndsWith(n, "Skel")) {
            continue;
        }
        const size_t a = 8, b = n.find('/', a);
        if (b == std::string::npos) {
            continue;
        }
        byObject[n.substr(a, b - a)].push_back(n);
    }
    int exact = 0, sameCount = 0, differ = 0, noZar = 0, total = 0;
    for (const auto& [object, skels] : byObject) {
        const std::string zarPath = ZarForObject(rom, object);
        std::vector<Cmb> cmbs;
        Zar zar;
        if (!zarPath.empty()) {
            zar = Zar::Parse(rom.Read(zarPath));
            for (const ZarFile& f : zar.files) {
                if (EndsWith(ToLower(f.name), ".cmb")) {
                    try {
                        cmbs.push_back(Cmb::Parse(f.data));
                    } catch (const std::exception&) {
                    }
                }
            }
        }
        for (const std::string& sp : skels) {
            auto skel = ReadVanillaSkeleton(*oot, sp);
            if (!skel) {
                continue;
            }
            total++;
            if (zarPath.empty()) {
                noZar++;
                print("NOZAR  " + sp + " (" + std::to_string(skel->limbs.size()) + " limbs)");
                continue;
            }
            const auto parents = ParentsOf(*skel);
            std::string best, cls = "DIFFER";
            int bestScore = -1;
            for (const Cmb& c : cmbs) {
                std::vector<int> cp;
                for (const auto& bn : c.bones) {
                    cp.push_back(bn.parent);
                }
                int score = 0;
                std::string k;
                if (cp == parents) {
                    score = 3, k = "EXACT";
                } else if (cp.size() == parents.size()) {
                    score = 2, k = "COUNT";
                } else {
                    score = 1, k = "DIFFER";
                }
                if (score > bestScore) {
                    bestScore = score;
                    cls = k;
                    best = c.name + "(" + std::to_string(c.bones.size()) + ")";
                }
            }
            if (cls == "EXACT") {
                exact++;
            } else if (cls == "COUNT") {
                sameCount++;
            } else {
                differ++;
            }
            print(cls + "  " + sp + " (" + std::to_string(skel->limbs.size()) + " limbs, type " +
                  std::to_string(skel->type) + ") <- " + zarPath + " " + best);
        }
    }
    print("skeletons " + std::to_string(total) + ": exact topology " + std::to_string(exact) + ", same bone count " +
          std::to_string(sameCount) + ", different " + std::to_string(differ) + ", no 3DS archive " +
          std::to_string(noZar));
}

} // namespace Oot3d

namespace Oot3d {

void ObjBoxes(const RomFS& rom, const std::string& ootPath, const std::string& object,
              const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    const std::string prefix = "objects/" + object + "/";
    for (const std::string& n : oot->Names()) {
        if (!StartsWith(n, prefix)) {
            continue;
        }
        auto b = oot->Read(n);
        auto h = b ? ParseResourceHeader(*b) : std::nullopt;
        if (!h || h->type != Soh::kTypeDisplayList) {
            continue;
        }
        VanillaDlInfo info = InspectVanillaDl(*oot, n);
        char line[256];
        snprintf(line, sizeof(line), "N64 %-50s %5u vtx  min(%.0f %.0f %.0f) max(%.0f %.0f %.0f)", n.c_str(),
                 info.vertexCount, info.min.x, info.min.y, info.min.z, info.max.x, info.max.y, info.max.z);
        print(line);
    }
    const std::string zp = ZarForObject(rom, object);
    if (zp.empty()) {
        return;
    }
    Zar zar = Zar::Parse(rom.Read(zp));
    for (const ZarFile& f : zar.files) {
        if (!EndsWith(ToLower(f.name), ".cmb")) {
            continue;
        }
        Cmb c = Cmb::Parse(f.data);
        auto world = c.BindPose();
        std::vector<Mat34> inv;
        for (auto& w : world) {
            inv.push_back(w.Inverse());
        }
        Vec3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
        size_t nv = 0;
        for (const auto& set : c.Decode()) {
            for (const auto& v : set.vertices) {
                const Vec3 p = SkinPosition(v, set.skinning, world, inv);
                mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
                mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
                nv++;
            }
        }
        char line[256];
        snprintf(line, sizeof(line), "3DS %-50s %5zu vtx  min(%.0f %.0f %.0f) max(%.0f %.0f %.0f) bones %zu",
                 f.name.c_str(), nv, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, c.bones.size());
        print(line);
    }
}

} // namespace Oot3d

namespace Oot3d {

void AnimList(const RomFS& rom, const std::string& ootPath, const std::string& object,
              const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    const std::string prefix = "objects/" + object + "/";
    for (const std::string& n : oot->Names()) {
        if (!StartsWith(n, prefix)) {
            continue;
        }
        if (auto a = ReadVanillaAnimation(*oot, n)) {
            char line[256];
            snprintf(line, sizeof(line), "N64 %-60s type %u frames %3d joints %zu", n.c_str(), a->type, a->frameCount,
                     a->joints.size());
            print(line);
        } else if (auto s = ReadVanillaSkeleton(*oot, n)) {
            print("N64 skeleton " + n + " limbs " + std::to_string(s->limbs.size()));
        }
    }
    const std::string zp = ZarForObject(rom, object);
    if (zp.empty()) {
        return;
    }
    Zar zar = Zar::Parse(rom.Read(zp));
    for (const ZarFile& f : zar.files) {
        const std::string l = ToLower(f.name);
        if (EndsWith(l, ".csab")) {
            Csab c = Csab::Parse(f.data);
            char line[256];
            snprintf(line, sizeof(line), "3DS %-60s frames %3u bones %zu nodes %zu", f.name.c_str(), c.duration,
                     c.boneToNode.size(), c.nodes.size());
            std::string bl = line;
            bl += " animated:";
            for (const CsabNode& nd : c.nodes) {
                bool moves = false;
                for (const auto& t : nd.tracks) {
                    if (t && t->keys.size() > 1) {
                        float lo = t->keys[0].value, hi = lo;
                        for (const auto& k : t->keys) {
                            lo = std::min(lo, k.value), hi = std::max(hi, k.value);
                        }
                        moves |= hi - lo > 1e-3f;
                    }
                }
                if (moves) {
                    bl += " " + std::to_string(nd.bone);
                }
            }
            print(bl);
        } else if (EndsWith(l, ".cmb")) {
            Cmb c = Cmb::Parse(f.data);
            print("3DS model " + f.name + " bones " + std::to_string(c.bones.size()));
        } else {
            print("3DS file " + f.name);
        }
    }
}

} // namespace Oot3d

namespace Oot3d {

void AnimPairReport(const RomFS& rom, const std::string& ootPath, const std::string& object,
                    const std::function<void(const std::string&)>& print) {
    auto oot = ZipReader::Open(ootPath);
    const std::string prefix = "objects/" + object + "/";
    std::vector<VanillaSkeleton> skels;
    std::vector<VanillaAnimation> anims;
    for (const std::string& n : oot->Names()) {
        if (!StartsWith(n, prefix)) {
            continue;
        }
        if (auto a = ReadVanillaAnimation(*oot, n)) {
            if (a->type == 0) {
                anims.push_back(*a);
            }
        } else if (auto s = ReadVanillaSkeleton(*oot, n)) {
            skels.push_back(*s);
        }
    }
    const std::string zp = ZarForObject(rom, object);
    if (zp.empty()) {
        print("no 3DS archive");
        return;
    }
    Zar zar = Zar::Parse(rom.Read(zp));
    std::vector<std::pair<std::string, Cmb>> cmbs;
    std::vector<std::pair<std::string, Csab>> csabs;
    for (const ZarFile& f : zar.files) {
        const std::string l = ToLower(f.name);
        if (EndsWith(l, ".cmb")) {
            cmbs.emplace_back(f.name, Cmb::Parse(f.data));
        } else if (EndsWith(l, ".csab")) {
            csabs.emplace_back(f.name, Csab::Parse(f.data));
        }
    }
    for (const VanillaSkeleton& skel : skels) {
        // The model: same hierarchy, else the one most animations fit, else the most bones.
        const auto parents = ParentsOf(skel);
        const Cmb* model = nullptr;
        std::string modelName;
        size_t bestFit = 0;
        for (const auto& [name, c] : cmbs) {
            std::vector<int> cp;
            for (const auto& b : c.bones) {
                cp.push_back(b.parent);
            }
            size_t fit = 0;
            for (const auto& [an, a] : csabs) {
                fit += a.boneToNode.size() == c.bones.size();
            }
            if (cp == parents) {
                fit += 100000;
            }
            if (!model || fit > bestFit) {
                model = &c;
                modelName = name;
                bestFit = fit;
            }
        }
        std::vector<VanillaAnimation> mine;
        for (const auto& a : anims) {
            if (a.joints.size() == skel.limbs.size() + 1) {
                mine.push_back(a);
            }
        }
        std::vector<AnimCandidate> cands;
        for (const auto& [name, a] : csabs) {
            if (model && a.boneToNode.size() == model->bones.size()) {
                cands.push_back({ name, &a });
            }
        }
        print("skeleton " + skel.path + " (" + std::to_string(skel.limbs.size()) + " limbs) <- " + modelName + " (" +
              std::to_string(model ? model->bones.size() : 0) + " bones), " + std::to_string(mine.size()) +
              " N64 animations, " + std::to_string(cands.size()) + " 3DS animations");
        if (!model) {
            continue;
        }
        const AnimPairing p = PairAnimations(skel, mine, *model, cands);
        std::string map = "  limb->bone:";
        for (size_t i = 0; i < p.limbToBone.size(); i++) {
            map += " " + std::to_string(i) + ":" + std::to_string(p.limbToBone[i]);
        }
        print(map + "  (error " + std::to_string(p.correspondenceError) + ")");
        for (const AnimPair& ap : p.pairs) {
            char line[320];
            snprintf(line, sizeof(line), "  %-58s %3d -> %-38s %3u  score %.3f%s", ap.n64Path.c_str() + prefix.size(),
                     ap.n64Frames, ap.csab >= 0 ? cands[ap.csab].name.c_str() : "(none)",
                     ap.csab >= 0 ? cands[ap.csab].csab->duration : 0, ap.score, ap.frameCountMatches ? "" : " FRAMES");
            print(line);
        }
    }
}

} // namespace Oot3d

namespace Oot3d {

// Poses a model with one of its 3DS animations and reports vertices that coincide at the bind pose but separate
// when posed (cracks between meshes), per the converter's own skinning.
void SkinCheck(const RomFS& rom, const std::string& zarPath, const std::string& model, const std::string& anim,
               float frame, const std::function<void(const std::string&)>& print) {
    Zar zar = Zar::Parse(rom.Read(zarPath));
    const ZarFile* mf = zar.Find(model);
    const ZarFile* af = zar.Find(anim);
    if (!mf || !af) {
        print("model or animation not found");
        return;
    }
    Cmb cmb = Cmb::Parse(mf->data);
    Csab csab = Csab::Parse(af->data);
    const auto bind = cmb.BindPose();
    std::vector<Mat34> inv;
    for (const auto& w : bind) {
        inv.push_back(w.Inverse());
    }
    const auto posed = CsabPose(cmb, csab, frame);
    struct P {
        Vec3 b, p;
        int set;
        Skinning mode;
    };
    std::vector<P> pts;
    int setNo = 0;
    for (const auto& set : cmb.Decode()) {
        for (const auto& v : set.vertices) {
            pts.push_back({ SkinPosition(v, set.skinning, bind, inv), SkinPosition(v, set.skinning, posed, inv), setNo,
                            set.skinning });
        }
        setNo++;
    }
    float worst = 0;
    int cracks = 0;
    for (size_t i = 0; i < pts.size(); i++) {
        for (size_t j = i + 1; j < pts.size(); j++) {
            if ((pts[i].b - pts[j].b).Length() > 0.5f) {
                continue;
            }
            const float gap = (pts[i].p - pts[j].p).Length();
            if (gap > 2.0f) {
                cracks++;
                if (gap > worst) {
                    worst = gap;
                    char line[200];
                    snprintf(line, sizeof(line), "  gap %.1f between set %d (mode %d) and set %d (mode %d) at (%.0f %.0f %.0f)",
                             gap, pts[i].set, int(pts[i].mode), pts[j].set, int(pts[j].mode), pts[i].b.x, pts[i].b.y,
                             pts[i].b.z);
                    print(line);
                }
            }
        }
    }
    print(std::to_string(pts.size()) + " vertices, " + std::to_string(cracks) + " coincident pairs separate by > 2 units");
}

} // namespace Oot3d
