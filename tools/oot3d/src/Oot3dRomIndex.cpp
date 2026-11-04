#include "Oot3dRomIndex.h"

#include "Oot3dCmb.h"
#include "Oot3dCsab.h"
#include "Oot3dExtract.h"
#include "Oot3dRom.h"
#include "Oot3dZar.h"

namespace Oot3d {

namespace {

// Vanilla objects whose 3DS archive has another name. Shape matching still decides what pairs, so an alias only
// widens where the converters look.
const std::map<std::string, std::vector<std::string>> kAliases = {
    { "gameplay_keep", { "actor/zelda_keep.zar" } },
    { "gameplay_field_keep", { "actor/zelda_field_keep.zar" } },
    { "gameplay_dangeon_keep", { "actor/zelda_dangeon_keep.zar" } },
    { "object_anubice", { "actor/zelda_av.zar" } },
    { "object_bdoor", { "actor/zelda_boss_door.zar" } },
    { "object_bigokuta", { "actor/zelda_ocd.zar" } },
    { "object_demo_tre_lgt", { "actor/zelda_box.zar" } },
    { "object_door_killer", { "actor/zelda_killer_door.zar" } },
    { "object_fd2", { "actor/zelda_fd.zar" } },
    { "object_fhg", { "actor/zelda_fantomHG.zar" } },
    { "object_firefly", { "actor/zelda_ff.zar" } },
    { "object_fish", { "actor/zelda_fishing.zar" } },
    { "object_ganon_anime1", { "actor/zelda_ganon_demo.zar" } },
    { "object_ganon_anime2", { "actor/zelda_ganon_demo.zar" } },
    { "object_ganon_anime3", { "actor/zelda_ganon_down.zar" } },
    { "object_geldb", { "actor/zelda_gelb.zar" } },
    { "object_gol", { "actor/zelda_kogoma.zar" } },
    { "object_horse_link_child", { "actor/zelda_hlc.zar" } },
    { "object_kingdodongo", { "actor/zelda_kdodongo.zar" } },
    { "object_lightbox", { "actor/dk_lightbox.zar" } },
    { "object_medal", { "actor/zelda_gi_medal.zar" } },
    { "object_mjin", { "actor/zelda_mjin.zar" } },
    { "object_mjin_dark", { "actor/zelda_mjin.zar" } },
    { "object_mjin_flame", { "actor/zelda_mjin.zar" } },
    { "object_mjin_flash", { "actor/zelda_mjin.zar" } },
    { "object_mjin_ice", { "actor/zelda_mjin.zar" } },
    { "object_mjin_soul", { "actor/zelda_mjin.zar" } },
    { "object_mjin_wind", { "actor/zelda_mjin.zar" } },
    { "object_mjin_oka", { "actor/zelda_mjin_oka.zar" } },
    { "object_niw", { "actor/zelda_nw.zar" } },
    { "object_oF1d_map", { "actor/zelda_oF1d.zar" } },
    { "object_okuta", { "actor/zelda_oc2.zar" } },
    { "object_peehat", { "actor/zelda_ph.zar" } },
    { "object_poh", { "actor/zelda_po.zar" } },
    { "object_pu_box", { "actor/dk_pu_box.zar" } },
    { "object_reeba", { "actor/zelda_rb.zar" } },
    { "object_shop_dungen", { "actor/zelda_shop_tana.zar" } },
    { "object_sk2", { "actor/zelda_skelton.zar" } },
    { "object_spot04_objects", { "actor/zelda_spo04_objects.zar" } },
    { "object_tite", { "actor/zelda_tt.zar" } },
    { "object_trap", { "actor/dk_trap.zar" } },
    { "object_vase", { "actor/dk_vase.zar" } },
    { "object_wallmaster", { "actor/zelda_wm2.zar" } },
};

} // namespace

RomIndex::RomIndex(const RomFS& rom) : mRom(rom) {
    for (const std::string& path : rom.List("actor/*.zar")) {
        Zar zar;
        try {
            zar = Zar::Parse(rom.Read(path));
        } catch (const std::exception&) {
            continue;
        }
        for (const ZarFile& f : zar.files) {
            const std::string l = ToLower(f.name);
            try {
                if (EndsWith(l, ".cmb")) {
                    const Cmb c = Cmb::Parse(f.data);
                    RomModel m;
                    m.zar = path;
                    m.name = f.name;
                    for (const CmbBone& b : c.bones) {
                        m.parents.push_back(b.parent);
                        m.boneTrans.push_back(b.translation);
                    }
                    mModels.push_back(std::move(m));
                } else if (EndsWith(l, ".csab")) {
                    const Csab c = Csab::Parse(f.data);
                    mAnims.push_back({ path, f.name, uint32_t(c.boneToNode.size()), c.duration });
                }
            } catch (const std::exception&) {
            }
        }
    }
}

std::vector<std::string> RomIndex::ArchivesFor(const std::string& object) const {
    std::vector<std::string> out;
    const std::string own = ZarForObject(mRom, object);
    if (!own.empty()) {
        out.push_back(own);
    }
    if (auto it = kAliases.find(object); it != kAliases.end()) {
        for (const std::string& z : it->second) {
            if (mRom.Exists(z) && std::find(out.begin(), out.end(), z) == out.end()) {
                out.push_back(z);
            }
        }
    }
    return out;
}

const Cmb* RomIndex::Archive::FindCmb(const std::string& name) const {
    for (const auto& [n, c] : cmbs) {
        if (n == name) {
            return &c;
        }
    }
    return nullptr;
}

const RomIndex::Archive* RomIndex::Load(const std::string& zarPath) const {
    auto it = mArchives.find(zarPath);
    if (it != mArchives.end()) {
        return it->second.get();
    }
    auto a = std::make_unique<Archive>();
    try {
        a->zar = std::make_unique<Zar>(Zar::Parse(mRom.Read(zarPath)));
        for (const ZarFile& f : a->zar->files) {
            const std::string l = ToLower(f.name);
            try {
                if (EndsWith(l, ".cmb")) {
                    a->cmbs.emplace_back(f.name, Cmb::Parse(f.data));
                } else if (EndsWith(l, ".csab")) {
                    a->csabs.emplace_back(f.name, Csab::Parse(f.data));
                }
            } catch (const std::exception&) {
            }
        }
    } catch (const std::exception&) {
        a.reset();
    }
    const Archive* out = a.get();
    mArchives[zarPath] = std::move(a);
    return out;
}

} // namespace Oot3d
