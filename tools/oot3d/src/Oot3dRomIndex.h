#pragma once
// OoT3D asset converter: an index of every model and animation in the ROM's archives.
//
// Grezzo did not keep the N64's one-archive-per-object layout: characters and props were renamed (object_niw ->
// zelda_nw, gameplay_keep -> zelda_keep), and some share archives (the credits' zelda_ec holds a dozen characters).
// The converters look vanilla objects up here by rig (a skeleton's bone hierarchy) and by the alias table below
// instead of by name alone.

#include "Oot3dCmb.h"
#include "Oot3dCsab.h"
#include "Oot3dMath.h"
#include "Oot3dZar.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Oot3d {

class RomFS;

struct RomModel {
    std::string zar, name;
    std::vector<int> parents;    // bone hierarchy
    std::vector<Vec3> boneTrans; // bind translations (local)
};

struct RomAnim {
    std::string zar, name;
    uint32_t bones = 0, duration = 0;
};

class RomIndex {
  public:
    explicit RomIndex(const RomFS& rom);

    const std::vector<RomModel>& Models() const {
        return mModels;
    }
    const std::vector<RomAnim>& Anims() const {
        return mAnims;
    }
    // The archives a vanilla object's models are in: its own name (zelda_<name>), then the aliases.
    std::vector<std::string> ArchivesFor(const std::string& object) const;

    // Parsed archives, loaded on first use and kept.
    struct Archive {
        std::unique_ptr<Zar> zar;
        std::vector<std::pair<std::string, Cmb>> cmbs;
        std::vector<std::pair<std::string, Csab>> csabs;
        const Cmb* FindCmb(const std::string& name) const;
    };
    const Archive* Load(const std::string& zarPath) const;

  private:
    const RomFS& mRom;
    std::vector<RomModel> mModels;
    std::vector<RomAnim> mAnims;
    mutable std::map<std::string, std::unique_ptr<Archive>> mArchives;
};

} // namespace Oot3d
