#include "Oot3dExtract.h"

#include "Oot3dMaps.h"
#include "Oot3dObjects.h"
#include "Oot3dRings.h"
#include "Oot3dRom.h"
#include "Oot3dSkin.h"
#include "Oot3dRomIndex.h"
#include "Oot3dZip.h"
#include "Oot3dSoh.h"
#include "Oot3dTexture.h"
#include "Oot3dZar.h"
#include "Oot3dCmb.h"
#include "Oot3dMesh.h"

#include <algorithm>

#include <sstream>

namespace Oot3d {

uint32_t ParseParts(const std::string& csv) {
    uint32_t parts = 0;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = ToLower(item);
        if (item == "maps") {
            parts |= PartMaps;
        } else if (item == "objects") {
            parts |= PartObjects;
        } else if (item == "icons") {
            parts |= PartIcons;
        } else if (item == "actors") {
            parts |= PartActors;
        } else if (item == "all") {
            parts |= PartAll;
        } else if (!item.empty()) {
            throw std::runtime_error("unknown part \"" + item + "\" (maps, objects, icons, actors, all)");
        }
    }
    return parts;
}

std::string ExtractResult::Summary() const {
    std::ostringstream s;
    if (!ok) {
        s << "FAILED: " << error << "; ";
    }
    s << roomsConverted << " rooms converted (" << roomsSkipped << " skipped), " << objectsConverted
      << " object models, " << skins << " characters (" << animations << " 3DS animations, " << unpairedAnimations
      << " unpaired), " << texturesWritten << " textures, " << resourcesWritten << " resources, " << warnings
      << " warning(s)";
    return s.str();
}

ExtractResult Extract(const RomFS& rom, const ExtractOptions& opt) {
    ExtractResult r;
    try {
        auto oot = ZipReader::Open(opt.ootArchive);
        ZipWriter zip(opt.outPath);
        TextureStore textures(zip);
        if (opt.parts & PartMaps) {
            MapsResult m = ConvertMaps(rom, *oot, zip, textures, opt);
            r.roomsConverted = m.converted;
            r.roomsSkipped = m.skipped;
            r.warnings += m.warnings;
        }
        std::unique_ptr<RomIndex> index;
        if (opt.parts & (PartObjects | PartActors)) {
            if (opt.progress) {
                opt.progress("Indexing models", 0, 1);
            }
            index = std::make_unique<RomIndex>(rom);
            if (opt.log) {
                opt.log(LogLevel::Info, "indexed " + std::to_string(index->Models().size()) + " models and " +
                                            std::to_string(index->Anims().size()) + " animations");
            }
        }
        std::vector<ScrollEntry> objectScrolls; // converted object lists' texture scrolls (table_obj, below)
        if ((opt.parts & PartObjects) && !(opt.cancel && *opt.cancel)) {
            ObjectsResult o = ConvertObjects(rom, *index, *oot, zip, textures, opt);
            r.objectsConverted = o.converted;
            r.warnings += o.warnings;
            objectScrolls = std::move(o.scrolls);
        }
        if ((opt.parts & PartObjects) && !(opt.cancel && *opt.cancel)) {
            // Textures the game draws through code of its own (Ganondorf's cloth-simulated cape): Grezzo's redrawn
            // texture at the vanilla path, at the vanilla size so the game's per-texel masks (burn holes) still fit.
            // 3DS images run bottom-up.
            struct TexSwap {
                const char* vanilla;
                const char* zar;
                const char* file;
                uint32_t width, height;
            };
            static const TexSwap kTexSwaps[] = {
                { "overlays/ovl_En_Ganon_Mant/gMantTex", "actor/zelda_ganon.zar", "tex/mant_model_01.ctxb", 32, 64 },
            };
            for (const TexSwap& t : kTexSwaps) {
                try {
                    const Zar z = Zar::Parse(rom.Read(t.zar));
                    const ZarFile* f = z.Find(t.file);
                    if (!f) {
                        continue;
                    }
                    Image img = ParseCtxb(f->data).Decode(0);
                    if (img.width != t.width || img.height != t.height) {
                        if (opt.log) {
                            opt.log(LogLevel::Warning, std::string("texture swap ") + t.vanilla + ": size differs");
                        }
                        continue;
                    }
                    Image flipped = img;
                    const size_t row = size_t(img.width) * 4;
                    for (uint32_t y = 0; y < img.height; y++) {
                        std::copy_n(&img.rgba[(img.height - 1 - y) * row], row, &flipped.rgba[y * row]);
                    }
                    Soh::HdTexture hd;
                    hd.image = std::move(flipped);
                    hd.logicalWidth = t.width;
                    hd.logicalHeight = t.height;
                    zip.Add((opt.altAssets ? "alt/" : "") + std::string(t.vanilla), Soh::TextureResource(hd));
                    if (opt.log) {
                        opt.log(LogLevel::Detail, std::string("texture ") + t.vanilla + " <- " + t.zar + ":" + t.file);
                    }
                } catch (const std::exception& e) {
                    if (opt.log) {
                        opt.log(LogLevel::Warning, std::string("texture swap ") + t.vanilla + ": " + e.what());
                    }
                }
            }
        }
        if ((opt.parts & PartObjects) && !(opt.cancel && *opt.cancel)) {
            // Field drops: the N64 draws recovery hearts, bombs, arrows, nuts, sticks, seeds, magic jars and keys as
            // camera-facing sprites; the 3DS has a model for each (zelda_keep item00/), at the sprite's scale. They
            // go to oot3d/drop/<model>; En_Item00 draws them (spinning) in place of the sprite.
            try {
                const Zar z = Zar::Parse(rom.Read("actor/zelda_keep.zar"));
                int drops = 0;
                for (const ZarFile& f : z.files) {
                    const std::string n = ToLower(f.name);
                    if (!StartsWith(n, "item00/model/drop_gi_") || !EndsWith(n, ".cmb")) {
                        continue;
                    }
                    const Cmb cmb = Cmb::Parse(f.data);
                    const std::string stem = StemOf(f.name);
                    MeshOptions mo;
                    mo.n64Lighting = true;
                    mo.envPass = true;
                    mo.constantTints = true;
                    MeshBuilder mesh(zip, textures, mo, opt.log);
                    mesh.Add(cmb, 0, Mat34::Identity());
                    if (!mesh.Emit(0, MeshBuilder::Part::Both, "oot3d/drop/" + stem, "oot3d/drop/" + stem + "_vtx")
                             .empty()) {
                        drops++;
                    }
                }
                if (opt.log) {
                    opt.log(LogLevel::Info, std::to_string(drops) + " field drop models");
                }
            } catch (const std::exception& e) {
                if (opt.log) {
                    opt.log(LogLevel::Warning, std::string("field drops: ") + e.what());
                }
            }
        }
        if ((opt.parts & PartObjects) && !(opt.cancel && *opt.cancel)) {
            RingsResult rr = ConvertRingModels(rom, *oot, zip, textures, opt);
            for (ScrollEntry& se : rr.scrolls) {
                objectScrolls.push_back(std::move(se));
            }
            if (!objectScrolls.empty()) {
                // Objects' and ring bodies' texture scrolls: a second table beside the rooms' (MergeShip3dsScroll
                // reads both).
                Writer w;
                w.Raw("SCR1", 4);
                w.U32(static_cast<uint32_t>(objectScrolls.size()));
                for (const ScrollEntry& se : objectScrolls) {
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
                const Bytes hdr = Soh::Header(0x4F424C42, 0);
                blob.Raw(hdr.data(), hdr.size());
                blob.U32(static_cast<uint32_t>(w.Size()));
                blob.Raw(w.Out().data(), w.Size());
                zip.Add("oot3d/scroll/table_obj", blob.Out());
            }
        }
        if ((opt.parts & PartActors) && !(opt.cancel && *opt.cancel)) {
            SkinResult s = ConvertSkins(rom, *index, *oot, zip, textures, opt);
            r.skins = s.skins;
            r.animations = s.animations;
            r.unpairedAnimations = s.unpaired;
            r.warnings += s.warnings;
        }
        if (opt.cancel && *opt.cancel) {
            zip.Abort();
            r.error = "cancelled";
            return r;
        }
        r.texturesWritten = textures.Count();
        r.resourcesWritten = static_cast<uint32_t>(zip.Count());
        zip.Close();
        r.ok = true;
    } catch (const std::exception& e) {
        r.error = e.what();
    }
    return r;
}

} // namespace Oot3d
