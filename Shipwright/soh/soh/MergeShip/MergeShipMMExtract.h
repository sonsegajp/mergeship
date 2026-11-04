#pragma once

#include <string>

// MergeShip2: offer to build a namespaced mm.o2r from the player's Majora's Mask ROM, using the bundled
// mm-extractor (2 Ship 2 Harkinian's ZAPD with MergeShip's "mm/" prefix). `existingInvalidPath` is a present
// mm.o2r that is not a MergeShip archive (kept as mm.o2r.stock on success), or "" when none exists.
// Returns true when a new mm.o2r was written. soh.exe on Windows only (mergeship.exe asks in OTRGlobals::RunExtract
// instead); asks unless MergeShip.AskForMMRom is 0, and uses MERGESHIP_MM_ROM without asking when it is set.
bool MergeShip_OfferMajorasMaskExtraction(const std::string& existingInvalidPath);

// The pieces, shared with mergeship.exe's first-run flow (OTRGlobals::RunExtract, ES_MM), which has 2ship.dll build
// the archive through the host (MsHostServices.ExtractRom) instead of mm-extractor/ZAPD.exe:
// MERGESHIP_MM_ROM, or "".
std::string MergeShip_MajorasMaskRomFromEnvironment();
// A file box for the ROM; "" if cancelled (and on other platforms).
std::string MergeShip_PickMajorasMaskRom();
// A new private folder in the temp directory.
std::string MergeShip_MakeMajorasMaskWorkDir();
// Checks that romPath is a supported, unmodified Majora's Mask dump (US 1.0 N64 or US GameCube; z64, v64 or n64) and
// writes its big-endian copy to z64Path. On success `version` is the extractor's asset set ("N64_US" / "GC_US");
// otherwise errorTitle and errorText say why.
bool MergeShip_PrepareMajorasMaskRom(const std::string& romPath, const std::string& z64Path, std::string& version,
                                     std::string& errorTitle, std::string& errorText);
