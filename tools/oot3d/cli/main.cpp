// oot3d-extract: command line front end of the OoT3D -> Ship of Harkinian converter (tools/oot3d/src).
//
//   oot3d-extract info  <rom>                          title, file count
//   oot3d-extract list  <rom> [pattern]                RomFS paths (glob, e.g. "actor/zelda_gi_*")
//   oot3d-extract dump  <rom> <pattern> <outdir>       raw files; .ctxb as PNG; .zar unpacked (+ textures as PNG)
//   oot3d-extract check <rom>                          parse every archive, model, scene and animation
//   oot3d-extract tevstats <rom> [pattern]             distinct material/combiner setups, most common first
//   oot3d-extract build <rom> --oot <oot.o2r> --out <mod.o2r> [--only maps,objects,icons] [--no-alt]
//
// <rom> is a decrypted .3ds/.cci/.cxi, or a folder with an extracted RomFS.

#include "Oot3dExtract.h"
#include "Oot3dRom.h"
#include "Oot3dCmb.h"
#include "Oot3dZar.h"
#include "Oot3dVanilla.h"
#include "Oot3dZip.h"
#include "Oot3dCmab.h"
#include "Oot3dCsab.h"
#include "Oot3dZsi.h"
#include "Oot3dTexture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Oot3d;
namespace fs = std::filesystem;

static int Usage() {
    fputs("usage:\n"
          "  oot3d-extract info  <rom>\n"
          "  oot3d-extract list  <rom> [pattern]\n"
          "  oot3d-extract dump  <rom> <pattern> <outdir>\n"
          "  oot3d-extract check <rom>\n"
          "  oot3d-extract build <rom> --oot <oot.o2r> --out <mod.o2r> [--only maps,objects,icons] [--no-alt]\n"
          "                      [--scene <name>]... [--log <file>]\n",
          stderr);
    return 2;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        return Usage();
    }
    const std::string cmd = argv[1];
    try {
        if (cmd == "audit") {
            // audit <oot.o2r> <mod.o2r> <outdir> [size]
            if (argc < 5) {
                return Usage();
            }
            RenderAudit(argv[2], argv[3], argv[4], argc > 5 ? atoi(argv[5]) : 256,
                        [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "vinfo") {
            // vinfo <oot.o2r> <dl path>...: what the converter reads off a vanilla display list
            auto oot = ZipReader::Open(argv[2]);
            for (int i = 3; i < argc; i++) {
                const VanillaDlInfo v = InspectVanillaDl(*oot, argv[i]);
                printf("%s: %u vtx, lighting %d, segTex %d, usesEnv %d usesPrim %d setsEnv %d setsPrim %d, primAlpha %d "
                       "envAlpha %d, textures %zu, calls %zu, segment calls %zu\n",
                       argv[i], v.vertexCount, v.lighting, v.segmentTextures, v.usesEnv, v.usesPrim, v.setsEnv,
                       v.setsPrim, v.usesPrimAlpha, v.usesEnvAlpha, v.textures.size(), v.calledLists.size(),
                       v.segmentCalls.size());
            }
            return 0;
        }
        if (cmd == "n64render") {
            // n64render <oot.o2r> <dl path> <yaw> <pitch> <out.png> [size]
            if (argc < 7) {
                return Usage();
            }
            RenderVanillaDl(argv[2], argv[3], float(atof(argv[4])), float(atof(argv[5])), argv[6],
                            argc > 7 ? atoi(argv[7]) : 512, [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "roomrender") {
            // roomrender <mod.o2r> <list prefix> <eye x,y,z> <target x,y,z> <out.png> [size]
            if (argc < 7) {
                return Usage();
            }
            float e[3], t[3];
            sscanf(argv[4], "%f,%f,%f", &e[0], &e[1], &e[2]);
            sscanf(argv[5], "%f,%f,%f", &t[0], &t[1], &t[2]);
            RenderRoom(argv[2], argv[3], Vec3(e[0], e[1], e[2]), Vec3(t[0], t[1], t[2]), argv[6],
                       argc > 7 ? atoi(argv[7]) : 800, [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "skinrender") {
            // skinrender <mod.o2r> <skeleton path> <n64 anim path|-> <n64 frame> <yaw> <pitch> <out.png> [size]
            if (argc < 9) {
                return Usage();
            }
            SkinRender(argv[2], argv[3], argv[4], float(atof(argv[5])), float(atof(argv[6])), float(atof(argv[7])),
                       argv[8], argc > 9 ? atoi(argv[9]) : 1024, [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        auto rom = RomFS::Open(argv[2]);
        if (cmd == "info") {
            printf("%s (%s), %zu files\n", rom->Title().c_str(), rom->ProductCode().c_str(), rom->List().size());
            return 0;
        }
        if (cmd == "list") {
            for (const auto& p : rom->List(argc > 3 ? argv[3] : "*")) {
                printf("%10llu  %s\n", static_cast<unsigned long long>(rom->FileSize(p)), p.c_str());
            }
            return 0;
        }
        if (cmd == "dump") {
            if (argc < 5) {
                return Usage();
            }
            DumpFiles(*rom, argv[3], argv[4], [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "zarindex") {
            // Every file inside every archive matching the pattern: "<archive> <file> <size>".
            for (const auto& p : rom->List(argc > 3 ? argv[3] : "*.zar")) {
                try {
                    const Zar z = Zar::Parse(rom->Read(p));
                    for (const ZarFile& f : z.files) {
                        printf("%s %s %zu\n", p.c_str(), f.name.c_str(), f.data.Size());
                    }
                } catch (const std::exception& e) {
                    printf("%s !%s\n", p.c_str(), e.what());
                }
            }
            return 0;
        }
        if (cmd == "cmbinfo") {
            // cmbinfo <rom> <zar>: each model's bones (parent, translation) and vertex count
            const Zar z = Zar::Parse(rom->Read(argv[3]));
            for (const ZarFile& f : z.files) {
                if (!EndsWith(ToLower(f.name), ".cmb")) {
                    continue;
                }
                const Cmb c = Cmb::Parse(f.data);
                size_t verts = 0;
                for (const auto& set : c.Decode()) {
                    verts += set.vertices.size();
                }
                printf("%s: %zu bones, %zu vertices, %zu meshes\n", f.name.c_str(), c.bones.size(), verts,
                       c.meshes.size());
                for (size_t b = 0; b < c.bones.size() && b < 40; b++) {
                    printf("   %2zu parent %2d t(%.0f %.0f %.0f)\n", b, c.bones[b].parent, c.bones[b].translation.x,
                           c.bones[b].translation.y, c.bones[b].translation.z);
                }
                for (size_t mi = 0; mi < c.materials.size(); mi++) {
                    const auto& d = c.materials[mi].diffuse;
                    printf("   material %zu diffuse %u %u %u %u\n", mi, d[0], d[1], d[2], d[3]);
                }
                // Per mesh: material and vertex color range (effects fade through vertex alpha).
                for (const auto& set : c.Decode()) {
                    std::array<float, 4> lo{ 9, 9, 9, 9 }, hi{ -9, -9, -9, -9 };
                    for (const auto& v : set.vertices) {
                        for (int k = 0; k < 4; k++) {
                            lo[k] = std::min(lo[k], v.color[k]);
                            hi[k] = std::max(hi[k], v.color[k]);
                        }
                    }
                    printf("   mesh %d material %d: %zu vertices, color min(%.2f %.2f %.2f %.2f) max(%.2f %.2f %.2f %.2f)\n",
                           set.mesh, set.material, set.vertices.size(), lo[0], lo[1], lo[2], lo[3], hi[0], hi[1], hi[2],
                           hi[3]);
                }
                // OOT3D_MESH_WEIGHTS=<mesh>: that mesh's skinning (mode, bone dimension, bone/weight combinations).
                if (const char* mw = getenv("OOT3D_MESH_WEIGHTS"); mw && *mw) {
                    const int want = atoi(mw);
                    if (want >= 0 && size_t(want) < c.meshes.size()) {
                        const CmbShape& sh = c.shapes[c.meshes[want].shape];
                        printf("   mesh %d: boneDimension %u, weights attr mode %u scale %g type 0x%x, indices mode %u "
                               "scale %g type 0x%x\n",
                               want, sh.boneDimension, sh.boneWeights.mode, sh.boneWeights.scale, sh.boneWeights.type,
                               sh.boneIndices.mode, sh.boneIndices.scale, sh.boneIndices.type);
                        std::map<std::string, int> combos;
                        for (const auto& set : c.Decode()) {
                            if (set.mesh != want) {
                                continue;
                            }
                            for (const auto& v : set.vertices) {
                                char k[96];
                                snprintf(k, sizeof(k), "mode %d:", int(set.skinning));
                                std::string key = k;
                                for (int i = 0; i < v.boneCount; i++) {
                                    snprintf(k, sizeof(k), " %d@%.2f", v.bones[i], v.weights[i]);
                                    key += k;
                                }
                                combos[key]++;
                            }
                        }
                        for (const auto& [k, n] : combos) {
                            printf("     %5d  %s\n", n, k.c_str());
                        }
                    }
                }
            }
            return 0;
        }
        if (cmd == "render") {
            // render <rom> <zar> <model> <anim|-> <frame> <yaw> <pitch> <out.png> [size]
            if (argc < 10) {
                return Usage();
            }
            RenderModel(*rom, argv[3], argv[4], argv[5], float(atof(argv[6])), float(atof(argv[7])),
                        float(atof(argv[8])), argv[9], argc > 10 ? atoi(argv[10]) : 1024,
                        [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "skincheck") {
            SkinCheck(*rom, argv[3], argv[4], argv[5], argc > 6 ? float(atof(argv[6])) : 10.0f,
                      [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "animpair") {
            AnimPairReport(*rom, argv[3], argv[4], [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "anims") {
            AnimList(*rom, argv[3], argv[4], [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "zsimeshes") {
            // zsimeshes <rom> <zsi>: each mesh of the room's models: texture, skinning, bones, bind-pose bounds
            const ZsiRoom room = ZsiRoom::Parse(rom->Read(argv[3]));
            for (size_t si = 0; si < room.setups.size(); si++) {
                for (const auto* opt : { &room.setups[si].opaque, &room.setups[si].translucent }) {
                    if (!opt->has_value()) {
                        continue;
                    }
                    const Cmb& c = **opt;
                    const auto bind = c.BindPose();
                    std::vector<Mat34> inv;
                    for (const auto& w : bind) {
                        inv.push_back(w.Inverse());
                    }
                    printf("setup %zu model %s: %zu bones, %zu meshes\n", si, c.name.c_str(), c.bones.size(),
                           c.meshes.size());
                    for (const auto& set : c.Decode()) {
                        Vec3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
                        std::map<int, int> bones;
                        for (const auto& v : set.vertices) {
                            const Vec3 p = SkinPosition(v, set.skinning, bind, inv);
                            mn = { std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z) };
                            mx = { std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z) };
                            bones[v.bones[0]]++;
                        }
                        const CmbMaterial& m = c.materials[set.material];
                        const int t = m.bindings[0].texture;
                        printf("  mesh %3d mat %2d %-22s skin %d bones", set.mesh, set.material,
                               t >= 0 && size_t(t) < c.textures.size() ? c.textures[t].name.c_str() : "-",
                               int(set.skinning));
                        for (const auto& [b, n] : bones) {
                            printf(" %d", b);
                        }
                        printf("  y %.0f..%.0f  x %.0f..%.0f  z %.0f..%.0f\n", mn.y, mx.y, mn.x, mx.x, mn.z, mx.z);
                    }
                }
            }
            return 0;
        }
        if (cmd == "csabpose") {
            // csabpose <rom> <zar> <csab> <frame>: each bone's sampled local rotation (degrees), translation
            const Zar z = Zar::Parse(rom->Read(argv[3]));
            const ZarFile* f = z.Find(argv[4]);
            if (!f) {
                return Usage();
            }
            const Csab c = Csab::Parse(f->data);
            const float frame = float(atof(argv[5]));
            printf("duration %u bones %zu nodes %zu\n", c.duration, c.boneToNode.size(), c.nodes.size());
            for (size_t b = 0; b < c.boneToNode.size(); b++) {
                const CsabNode* nd = c.NodeForBone(b);
                printf("bone %2zu", b);
                for (int k = 0; k < 6; k++) {
                    if (nd && nd->tracks[k]) {
                        const float v = nd->tracks[k]->Sample(frame, k >= 3);
                        printf(" %8.2f", k >= 3 ? v * 57.29578f : v);
                    } else {
                        printf("        -");
                    }
                }
                printf("\n");
            }
            return 0;
        }
        if (cmd == "cmabdump") {
            // cmabdump <rom> <zar> <cmab> <outdir>: a material animation's entries, and its palette textures as PNGs
            if (argc < 6) {
                return Usage();
            }
            const Zar z = Zar::Parse(rom->Read(argv[3]));
            const ZarFile* f = z.Find(argv[4]);
            if (!f) {
                puts("cmab not found");
                return 1;
            }
            const Cmab c = Cmab::Parse(f->data);
            printf("duration %u, %s, %zu entries, %zu textures\n", c.duration, c.loop ? "loop" : "once", c.entries.size(),
                   c.textures.size());
            for (const CmabEntry& e : c.entries) {
                printf("  type %u material %u channel %u:", e.type, e.material, e.channel);
                for (size_t k = 0; k < e.tracks.size(); k++) {
                    if (!e.present[k]) {
                        continue;
                    }
                    printf(" [");
                    for (const CmabKey& key : e.tracks[k].keys) {
                        printf(" %g:%g", key.time, key.value);
                    }
                    printf(" ]");
                }
                puts("");
            }
            fs::create_directories(U8Path(argv[5]));
            for (size_t i = 0; i < c.textures.size(); i++) {
                if (c.textures[i].image.Valid()) {
                    const Bytes png = EncodePng(c.textures[i].image);
                    std::ofstream(U8Path(std::string(argv[5]) + "/" + std::to_string(i) + "_" + c.textures[i].name + ".png"),
                                  std::ios::binary)
                        .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
                }
            }
            return 0;
        }
        if (cmd == "shapecmp") {
            // shapecmp <rom> <oot.o2r> <zar> <model> <mesh,mesh,...> <dl,dl,...>: rotation-free shape of each 3DS mesh
            // (bind pose) and each vanilla list (principal extents, largest first, and their ratios), and which list
            // each mesh is closest to: pairs hand poses, heads and other alternatives without posing either side.
            if (argc < 8) {
                return Usage();
            }
            auto oot = ZipReader::Open(argv[3]);
            const Zar z = Zar::Parse(rom->Read(argv[4]));
            const ZarFile* mf = z.Find(argv[5]);
            if (!mf) {
                puts("model not found");
                return 1;
            }
            const Cmb cmb = Cmb::Parse(mf->data);
            const auto bind = cmb.BindPose();
            std::vector<Mat34> inv;
            for (const auto& w : bind) {
                inv.push_back(w.Inverse());
            }
            auto split = [](const std::string& s) {
                std::vector<std::string> out;
                size_t a = 0;
                while (a <= s.size()) {
                    const size_t b = s.find(',', a);
                    out.push_back(s.substr(a, b == std::string::npos ? std::string::npos : b - a));
                    if (b == std::string::npos) {
                        break;
                    }
                    a = b + 1;
                }
                return out;
            };
            // Principal standard deviations of a point set (Jacobi on the 3x3 covariance).
            auto extents = [](const std::vector<Vec3>& pts) {
                std::array<double, 3> mean{ 0, 0, 0 };
                for (const Vec3& p : pts) {
                    mean[0] += p.x, mean[1] += p.y, mean[2] += p.z;
                }
                for (double& m : mean) {
                    m /= std::max<size_t>(1, pts.size());
                }
                double c[3][3] = {};
                for (const Vec3& p : pts) {
                    const double d[3] = { p.x - mean[0], p.y - mean[1], p.z - mean[2] };
                    for (int i = 0; i < 3; i++) {
                        for (int j = 0; j < 3; j++) {
                            c[i][j] += d[i] * d[j] / std::max<size_t>(1, pts.size());
                        }
                    }
                }
                for (int sweep = 0; sweep < 50; sweep++) {
                    for (int p = 0; p < 2; p++) {
                        for (int q = p + 1; q < 3; q++) {
                            if (std::fabs(c[p][q]) < 1e-12) {
                                continue;
                            }
                            const double th = 0.5 * std::atan2(2 * c[p][q], c[q][q] - c[p][p]);
                            const double cs = std::cos(th), sn = std::sin(th);
                            for (int k = 0; k < 3; k++) {
                                const double a = c[k][p], b = c[k][q];
                                c[k][p] = cs * a - sn * b;
                                c[k][q] = sn * a + cs * b;
                            }
                            for (int k = 0; k < 3; k++) {
                                const double a = c[p][k], b = c[q][k];
                                c[p][k] = cs * a - sn * b;
                                c[q][k] = sn * a + cs * b;
                            }
                        }
                    }
                }
                std::array<double, 3> e{ std::sqrt(std::max(0.0, c[0][0])), std::sqrt(std::max(0.0, c[1][1])),
                                         std::sqrt(std::max(0.0, c[2][2])) };
                std::sort(e.begin(), e.end(), std::greater<double>());
                return e;
            };
            struct Shape {
                std::string name;
                std::array<double, 3> e;
            };
            std::vector<Shape> meshes, lists;
            for (const std::string& ms : split(argv[6])) {
                std::vector<Vec3> pts;
                for (const auto& set : cmb.Decode()) {
                    if (set.mesh == atoi(ms.c_str())) {
                        for (const auto& v : set.vertices) {
                            pts.push_back(SkinPosition(v, set.skinning, bind, inv));
                        }
                    }
                }
                meshes.push_back({ "mesh " + ms, extents(pts) });
            }
            for (const std::string& d : split(argv[7])) {
                lists.push_back({ d, extents(InspectVanillaDl(*oot, d, true).positions) });
            }
            // Distance: log ratios of the three extents (shape and size), size weighted lower (the 3DS rescales).
            auto dist = [](const Shape& a, const Shape& b) {
                double s = 0;
                for (int k = 0; k < 3; k++) {
                    const double r = std::log(std::max(1e-3, a.e[k]) / std::max(1e-3, b.e[k]));
                    s += r * r;
                }
                const double shapeA1 = a.e[1] / std::max(1e-3, a.e[0]), shapeB1 = b.e[1] / std::max(1e-3, b.e[0]);
                const double shapeA2 = a.e[2] / std::max(1e-3, a.e[0]), shapeB2 = b.e[2] / std::max(1e-3, b.e[0]);
                return 0.3 * std::sqrt(s) + std::fabs(shapeA1 - shapeB1) + std::fabs(shapeA2 - shapeB2);
            };
            for (const Shape& s : lists) {
                printf("list %-60s extents %7.1f %7.1f %7.1f  ratios %.2f %.2f\n", s.name.c_str(), s.e[0], s.e[1],
                       s.e[2], s.e[1] / std::max(1e-3, s.e[0]), s.e[2] / std::max(1e-3, s.e[0]));
            }
            for (const Shape& m : meshes) {
                printf("%-8s extents %7.1f %7.1f %7.1f  ratios %.2f %.2f ", m.name.c_str(), m.e[0], m.e[1], m.e[2],
                       m.e[1] / std::max(1e-3, m.e[0]), m.e[2] / std::max(1e-3, m.e[0]));
                for (const Shape& l : lists) {
                    printf(" %.2f", dist(m, l));
                }
                puts("");
            }
            return 0;
        }
        if (cmd == "objboxes") {
            ObjBoxes(*rom, argv[3], argv[4], [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "skelmatch") {
            SkelMatch(*rom, argc > 3 ? argv[3] : "oot.o2r", [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "materials") {
            DumpMaterials(*rom, argc > 3 ? argv[3] : "*", [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "tevstats") {
            TevStats(*rom, argc > 3 ? argv[3] : "*", [](const std::string& line) { puts(line.c_str()); });
            return 0;
        }
        if (cmd == "check") {
            const CheckReport r = CheckRom(*rom, [](const std::string& line) { puts(line.c_str()); });
            printf("%s\n", r.Summary().c_str());
            return r.failures == 0 ? 0 : 1;
        }
        if (cmd == "build") {
            ExtractOptions opt;
            opt.romPath = argv[2];
            std::string logPath;
            for (int i = 3; i < argc; i++) {
                const std::string a = argv[i];
                auto next = [&]() -> std::string {
                    if (i + 1 >= argc) {
                        throw std::runtime_error(a + " needs a value");
                    }
                    return argv[++i];
                };
                if (a == "--oot") {
                    opt.ootArchive = next();
                } else if (a == "--out") {
                    opt.outPath = next();
                } else if (a == "--only") {
                    opt.parts = ParseParts(next());
                } else if (a == "--no-alt") {
                    opt.altAssets = false;
                } else if (a == "--scene") {
                    opt.onlyScenes.push_back(next());
                } else if (a == "--log") {
                    logPath = next();
                } else {
                    throw std::runtime_error("unknown option " + a);
                }
            }
            if (opt.ootArchive.empty() || opt.outPath.empty()) {
                return Usage();
            }
            std::ofstream logFile;
            if (!logPath.empty()) {
                logFile.open(U8Path(logPath));
            }
            opt.log = [&](LogLevel level, const std::string& line) {
                if (logFile) {
                    logFile << line << '\n';
                }
                if (level != LogLevel::Detail) {
                    puts(line.c_str());
                }
            };
            int lastPct = -1;
            opt.progress = [&](const std::string& stage, uint64_t done, uint64_t total) {
                const int pct = total ? int(done * 100 / total) : 0;
                if (pct != lastPct) {
                    lastPct = pct;
                    fprintf(stderr, "\r%-40s %3d%%", stage.c_str(), pct);
                    if (done == total) {
                        fputc('\n', stderr);
                    }
                }
            };
            const auto start = std::chrono::steady_clock::now();
            const ExtractResult r = Extract(*rom, opt);
            printf("%s in %.1f s\n", r.Summary().c_str(),
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
            return r.ok ? 0 : 1;
        }
        return Usage();
    } catch (const std::exception& e) {
        fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
