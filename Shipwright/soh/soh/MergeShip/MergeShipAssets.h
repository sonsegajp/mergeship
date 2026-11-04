#pragma once

// MergeShip2 — Majora's Mask assets, read at run time out of mm.o2r.
//
// Ocarina of Time and Majora's Mask name 2562 resources identically: object_link_child,
// object_link_boy, 623 entries under gameplay_keep, and the audio tables among them.
// libultraship's resource namespace is flat and last-mounted-wins, so a stock mm.o2r mounted
// beside oot.o2r replaces Hyrule's assets instead of adding Termina's, and mounting it at
// lower priority makes the collided MM assets unreachable instead.
//
// Every MM resource is therefore exported under the "mm" namespace by our OTRExporter patch
// (see 2ship2harkinian/OTRExporter/OTRExporter/MergeShipNamespace.h). Both the archive entry
// name and the references embedded inside exported data come from the same function there, so
// the two agree by construction.
//
// Keep this value identical to MERGESHIP_MM_NAMESPACE on the exporter side.
#define MERGESHIP_MM_NAMESPACE "mm"

// Addresses a Majora's Mask resource, e.g.
//   MM_ASSET("objects/object_link_goron/gLinkGoronSkel")
// Every SoH by-path loader accepts these: ResourceMgr_LoadSkeletonByName,
// ResourceMgr_LoadPlayerAnimByName, ResourceMgr_LoadGfxByName, ResourceMgr_LoadTexOrDListByName,
// ResourceMgr_LoadAudioSoundFontByName, ResourceMgr_LoadSeqByName, and the rest.
//
// Display lists must go through ResourceMgr_LoadGfxByName. A raw __OTR__ path handed straight
// to gSPDisplayList resolves and executes, then dies in gfx_vtx_handler_f3dex2 on the first
// G_VTX because its internal vertex references were never retargeted. Textures are leaf
// resources and do not have this problem.
#define MM_ASSET(path) "__OTR__" MERGESHIP_MM_NAMESPACE "/" path

#ifdef __cplusplus
extern "C" {
#endif

// True once mm.o2r has been mounted and its namespace verified. Game code that draws MM
// content must check this: the archive is supplied by the player and may be absent.
int MergeShip_IsMajorasMaskMounted(void);

// Reports which form the player is being built as. Called when the selection changes, so the
// log shows a transformation actually taking effect rather than merely being configured.
void MergeShip_LogFormSelected(int form, int requested);

#ifdef __cplusplus
}

// C++ even when this header is included inside an extern "C" block (as some .cpp files do): clang's libc++ refuses
// templates with C linkage, which MSVC let pass.
extern "C++" {
#include <memory>
#include <string>

namespace Ship {
class ArchiveManager;
}

// Mounts mm.o2r if present. Called from OTRGlobals::Initialize alongside oot.o2r, and takes the
// archive manager from that call site rather than reaching for a global, because this runs before
// the Ship context has finished initializing.
void MergeShip_MountMajorasMask(const std::shared_ptr<Ship::ArchiveManager>& archiveManager);

// True if the archive at `path` is a complete namespaced MM archive (what MergeShip_MountMajorasMask accepts). Used by
// mergeship.exe's first-run flow (OTRGlobals::RunExtract) to decide whether mm.o2r must be built.
bool MergeShip_IsNamespacedMajorasMaskArchive(const std::string& path);

// Loads each transformation skeleton once, to prove SoH's resource factories can actually parse
// what 2S2H's exporter wrote. Must run after the resource factories are registered.
void MergeShip_VerifyMajorasMaskAssets();
} // extern "C++"
#endif
