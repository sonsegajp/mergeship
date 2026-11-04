#pragma once
// OoT3D asset converter: the entry points (used by oot3d-extract.exe, later by soh.dll at runtime).

#include "Oot3dBinary.h"
#include "Oot3dMath.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Oot3d {

class RomFS;

enum class LogLevel { Detail, Info, Warning, Error };

enum Part : uint32_t {
    PartMaps = 1 << 0,    // room geometry, replacing the vanilla room display lists
    PartObjects = 1 << 1, // static object/item models (non-skeletal), replacing vanilla display lists
    PartIcons = 1 << 2,   // item icons and other UI textures (HD replacements of vanilla textures)
    PartActors = 1 << 3,  // skinned characters with their 3DS animations (drawn by Mergeship's engine)
    PartAll = PartMaps | PartObjects | PartIcons | PartActors,
};
uint32_t ParseParts(const std::string& csv); // "maps,objects" -> bits; throws on unknown names

struct ExtractOptions {
    std::string romPath;
    std::string ootArchive; // SoH's oot.o2r: the vanilla resources the mod overrides
    std::string outPath;    // the .o2r to write (e.g. <app>/mods/oot3d.o2r)
    uint32_t parts = PartAll;
    // Put overrides under alt/ so SoH's "Alternate Assets" toggle switches between N64 and 3DS looks.
    bool altAssets = true;
    std::vector<std::string> onlyScenes; // debug: convert just these scenes (vanilla names, e.g. "ydan_scene")
    std::function<void(LogLevel, const std::string&)> log;
    std::function<void(const std::string& stage, uint64_t done, uint64_t total)> progress;
    const std::atomic<bool>* cancel = nullptr;
};

struct ExtractResult {
    bool ok = false;
    std::string error;
    uint32_t roomsConverted = 0, roomsSkipped = 0;
    uint32_t objectsConverted = 0, texturesWritten = 0, resourcesWritten = 0;
    uint32_t skins = 0, animations = 0, unpairedAnimations = 0;
    uint32_t warnings = 0;
    std::string Summary() const;
};

ExtractResult Extract(const RomFS& rom, const ExtractOptions& options);

// Debug helpers for the command line tool.
struct CheckReport {
    uint32_t files = 0, failures = 0;
    uint32_t zars = 0, cmbs = 0, csabs = 0, ctxbs = 0, zsis = 0, textures = 0;
    std::string Summary() const;
};
CheckReport CheckRom(const RomFS& rom, const std::function<void(const std::string&)>& print);
void DumpFiles(const RomFS& rom, const std::string& pattern, const std::string& outDir,
               const std::function<void(const std::string&)>& print);
// Every distinct texture-combiner setup used by the ROM's models, most common first.
void TevStats(const RomFS& rom, const std::string& pattern, const std::function<void(const std::string&)>& print);
// How each vanilla skeleton lines up with the 3DS rigs of the same object.
void SkelMatch(const RomFS& rom, const std::string& ootPath, const std::function<void(const std::string&)>& print);
// Bounding boxes of an object's vanilla display lists and of its 3DS models.
void ObjBoxes(const RomFS& rom, const std::string& ootPath, const std::string& object,
              const std::function<void(const std::string&)>& print);
std::string ZarForObject(const RomFS& rom, const std::string& object);
// An object's vanilla animations and skeletons next to its 3DS animations and models.
void AnimList(const RomFS& rom, const std::string& ootPath, const std::string& object,
              const std::function<void(const std::string&)>& print);
// How an object's vanilla animations pair with its 3DS animations.
void AnimPairReport(const RomFS& rom, const std::string& ootPath, const std::string& object,
                    const std::function<void(const std::string&)>& print);
// Cracks a model's skinning opens when posed by one of its animations.
void SkinCheck(const RomFS& rom, const std::string& zarPath, const std::string& model, const std::string& anim,
               float frame, const std::function<void(const std::string&)>& print);
// Reference software render of a 3DS model (optionally posed by a CSAB frame) to a PNG.
void RenderModel(const RomFS& rom, const std::string& zarPath, const std::string& model, const std::string& anim,
                 float frame, float yawDeg, float pitchDeg, const std::string& outPng, int size,
                 const std::function<void(const std::string&)>& print);
// Software render of a vanilla (N64) display list from oot.o2r, with its textures (palettes applied).
void RenderVanillaDl(const std::string& ootPath, const std::string& dlPath, float yawDeg, float pitchDeg,
                     const std::string& outPng, int size, const std::function<void(const std::string&)>& print);
// A converted room seen from a camera (perspective): the mod's lists under `prefix`.
void RenderRoom(const std::string& modPath, const std::string& prefix, const Vec3& eye, const Vec3& target,
                const std::string& outPng, int size, const std::function<void(const std::string&)>& print);
// Every converted character of a mod rendered next to its vanilla skeleton (same animation, first frame), as
// <outDir>/<object>~<skeleton>~n64.png and ~3ds.png, to check models, variants and colors at a glance.
void RenderAudit(const std::string& ootPath, const std::string& modPath, const std::string& outDir, int size,
                 const std::function<void(const std::string&)>& print);
// Software render of a converted character package from a mod, posed and skinned by the engine's own code.
void SkinRender(const std::string& modPath, const std::string& skeleton, const std::string& n64Anim, float n64Frame,
                float yawDeg, float pitchDeg, const std::string& outPng, int size,
                const std::function<void(const std::string&)>& print);
// Each material of the models in the matching files, and what it translates to.
void DumpMaterials(const RomFS& rom, const std::string& pattern, const std::function<void(const std::string&)>& print);

} // namespace Oot3d
