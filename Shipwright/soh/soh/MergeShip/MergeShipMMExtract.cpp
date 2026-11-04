// MergeShip2 -- build mm.o2r from the player's own Majora's Mask ROM at launch.
//
// MergeShip reads Majora's Mask assets out of a *namespaced* mm.o2r: every path, and every
// reference embedded inside the exported data, lives under "mm/" so the archive can sit beside
// oot.o2r (the two games name 2562 resources identically). A stock 2 Ship 2 Harkinian mm.o2r has
// no such prefix and is refused by MergeShip_MountMajorasMask.
//
// The namespaced archive is produced by 2 Ship 2 Harkinian's own extractor (ZAPD with its OTR
// exporter), built with MergeShip's prefix (2ship2harkinian/OTRExporter, MergeShipNamespace.h):
//  - in mergeship.exe, by 2ship.dll itself, through the host (MsHostServices.ExtractRom). SoH's first-run flow
//    (OTRGlobals::RunExtract, ES_MM) asks for the ROM and validates it here, so one question produces the mm.o2r
//    both games read (HOST_DESIGN.md, "First run: archives");
//  - in soh.exe, by a bundled mm-extractor/ZAPD.exe with 2 Ship 2 Harkinian's extraction assets
//    (mm-extractor/assets: xml/, filelists/, Config_*.xml), run as a separate process so SoH never links ZAPD
//    (MergeShip_OfferMajorasMaskExtraction). This is 2s2h/Extractor/Extract.cpp's ROM validation and CallZapd.
//
// MERGESHIP_MM_ROM names a ROM to use without asking (automated first-run tests).

#include "soh/MergeShip/MergeShipMMExtract.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include "soh/cvar_prefixes.h"
#include "soh/OTRGlobals.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

extern "C" uint32_t CRC32C(unsigned char* data, size_t dataSize);
extern "C" int32_t MergeShipHost_IsHosted(void); // MergeShipHost.cpp

namespace {

// 2s2h/Extractor/Extract.cpp: header CRC at 0x10 picks the asset set; whole-ROM CRC32C (big-endian
// data) must be a known good dump.
constexpr uint32_t kMMUS10 = 0x5354631C;
constexpr uint32_t kMMUSGC = 0xB443EB08;
constexpr std::array<uint32_t, 2> kGoodCrcs = { 0x96F49400 /* MM US 1.0 */, 0xBB434787 /* MM US GC */ };
constexpr size_t kMB = 1024 * 1024;
// 2 Ship 2 Harkinian 5.0.1, the extractor these assets and ZAPD.exe come from.
constexpr const char* kPortVersion = "5.0.1";

void ShowError(const char* title, const std::string& text) {
#ifdef _WIN32
    MessageBoxA(nullptr, text.c_str(), title, MB_OK | MB_ICONERROR);
#endif
    SPDLOG_ERROR("MergeShip: {}: {}", title, text);
}

// Normalise a z64 / v64 / n64 dump to big-endian (z64) in place. False if it is none of them.
bool ToBigEndian(std::vector<uint8_t>& rom) {
    if (rom.size() < 0x40) {
        return false;
    }
    const uint8_t b0 = rom[0];
    if (b0 == 0x80) { // z64
        return true;
    }
    if (b0 == 0x37) { // v64: byte-swapped halfwords
        for (size_t i = 0; i + 1 < rom.size(); i += 2) {
            std::swap(rom[i], rom[i + 1]);
        }
        return rom[0] == 0x80;
    }
    if (b0 == 0x40) { // n64: little-endian words
        for (size_t i = 0; i + 3 < rom.size(); i += 4) {
            std::swap(rom[i], rom[i + 3]);
            std::swap(rom[i + 1], rom[i + 2]);
        }
        return rom[0] == 0x80;
    }
    return false;
}

bool IsArchiveFile(const std::vector<uint8_t>& d) {
    return (d.size() > 6) && ((d[0] == 'P' && d[1] == 'K' && d[2] == 0x03 && d[3] == 0x04) ||
                              (d[0] == 'R' && d[1] == 'a' && d[2] == 'r' && d[3] == 0x21) ||
                              (d[0] == '7' && d[1] == 'z' && d[2] == 0xBC && d[3] == 0xAF));
}

#ifdef _WIN32
// Run the extractor and wait for it. The working directory holds a private copy of its assets.
bool RunZapd(const std::string& zapdExe, const std::string& workDir, const std::string& romPath,
             const std::string& version) {
    std::string cmd = "\"" + zapdExe + "\" ed -i assets/xml/" + version + " -b \"" + romPath +
                      "\" -fl assets/filelists -gsf 0 -rconf assets/Config_" + version +
                      ".xml -se OTR --otrfile mm.o2r --portVer " + kPortVersion + " -o placeholder -osf placeholder";
    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = { 0 };
    SPDLOG_INFO("MergeShip: running {}", cmd);
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        workDir.c_str(), &si, &pi)) {
        SPDLOG_ERROR("MergeShip: could not start the MM extractor (error {})", GetLastError());
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    SPDLOG_INFO("MergeShip: MM extractor exited with {}", exitCode);
    return std::filesystem::exists(workDir + "/mm.o2r");
}
#endif

} // namespace

std::string MergeShip_MakeMajorasMaskWorkDir() {
    static const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string name = "mergeship-mm-";
    for (int i = 0; i < 6; i++) {
        name += charset[dist(gen)];
    }
    auto dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::create_directories(dir);
    return dir.string();
}

std::string MergeShip_MajorasMaskRomFromEnvironment() {
    const char* rom = std::getenv("MERGESHIP_MM_ROM");
    return (rom != nullptr) ? std::string(rom) : std::string();
}

std::string MergeShip_PickMajorasMaskRom() {
#ifdef _WIN32
    OPENFILENAMEA box = { 0 };
    char nameBuffer[1024];
    nameBuffer[0] = 0;
    box.lStructSize = sizeof(box);
    box.lpstrFile = nameBuffer;
    box.nMaxFile = sizeof(nameBuffer);
    box.lpstrTitle = "Select your Majora's Mask ROM (US 1.0 or US GameCube)";
    box.Flags =
        OFN_NOCHANGEDIR | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    box.lpstrFilter = "N64 Roms\0*.z64;*.v64;*.n64\0All files\0*.*\0\0";
    if (!GetOpenFileNameA(&box) || nameBuffer[0] == 0) {
        return "";
    }
    return nameBuffer;
#else
    return "";
#endif
}

bool MergeShip_PrepareMajorasMaskRom(const std::string& romPath, const std::string& z64Path, std::string& version,
                                     std::string& errorTitle, std::string& errorText) {
    std::vector<uint8_t> rom;
    {
        std::ifstream in(romPath, std::ios::binary);
        if (!in) {
            errorTitle = "Cannot read the ROM";
            errorText = "Could not open " + romPath + ".";
            return false;
        }
        rom.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (IsArchiveFile(rom)) {
        errorTitle = "File is compressed";
        errorText = "The selected file is a zip/rar/7z archive. Extract the ROM first.";
        return false;
    }
    if (rom.size() != 32 * kMB && rom.size() != 54 * kMB && rom.size() != 64 * kMB) {
        errorTitle = "Invalid ROM size";
        errorText = "Expected a 32, 54 or 64 MB Majora's Mask ROM.";
        return false;
    }
    if (!ToBigEndian(rom)) {
        errorTitle = "Not an N64 ROM";
        errorText = "The selected file is not a z64, v64 or n64 ROM.";
        return false;
    }
    const uint32_t crc = CRC32C(rom.data(), rom.size());
    bool good = false;
    for (uint32_t c : kGoodCrcs) {
        good = good || (crc == c);
    }
    const uint32_t header = ((uint32_t)rom[0x10] << 24) | ((uint32_t)rom[0x11] << 16) | ((uint32_t)rom[0x12] << 8) |
                            (uint32_t)rom[0x13];
    const char* found = (header == kMMUS10) ? "N64_US" : (header == kMMUSGC) ? "GC_US" : nullptr;
    if (!good || found == nullptr) {
        errorTitle = "Unsupported ROM";
        errorText = "That ROM is not a supported Majora's Mask dump. Use Majora's Mask US 1.0 (N64) or US GameCube, "
                    "unmodified.";
        return false;
    }
    version = found;
    // Extractors read the ROM from disk: hand them the big-endian copy.
    std::ofstream out(z64Path, std::ios::binary);
    out.write((const char*)rom.data(), (std::streamsize)rom.size());
    if (!out) {
        errorTitle = "Cannot write the ROM copy";
        errorText = "Could not write " + z64Path + ".";
        return false;
    }
    return true;
}

bool MergeShip_OfferMajorasMaskExtraction(const std::string& existingInvalidPath) {
#ifndef _WIN32
    (void)existingInvalidPath;
    return false;
#else
    // mergeship.exe asks during SoH's first-run flow instead, and builds mm.o2r with 2ship.dll (OTRGlobals.cpp, ES_MM).
    if (MergeShipHost_IsHosted()) {
        return false;
    }
    const std::string envRom = MergeShip_MajorasMaskRomFromEnvironment();
    if (envRom.empty() && !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.AskForMMRom"), 1)) {
        return false;
    }

    const std::string bundle = std::filesystem::absolute(Ship::Context::GetAppBundlePath()).string();
    const std::string zapdExe = bundle + "/mm-extractor/ZAPD.exe";
    const std::string zapdAssets = bundle + "/mm-extractor/assets";
    if (!std::filesystem::exists(zapdExe) || !std::filesystem::exists(zapdAssets)) {
        SPDLOG_WARN("MergeShip: no mm-extractor next to soh.exe; can't build mm.o2r from a ROM");
        return false;
    }

    if (envRom.empty()) {
        std::string question =
            existingInvalidPath.empty()
                ? "Majora's Mask content (the transformation masks and more) needs assets from your own Majora's "
                  "Mask ROM.\n\nSelect your Majora's Mask ROM (US 1.0 N64 or US GameCube) now to build mm.o2r? This "
                  "takes a few minutes, once.\n\nYes = select ROM, No = not now, Cancel = don't ask again."
                : "The mm.o2r next to the game is not a MergeShip archive (a stock 2 Ship 2 Harkinian mm.o2r can't "
                  "be used, because its paths collide with Ocarina of Time's).\n\nSelect your Majora's Mask ROM (US "
                  "1.0 N64 or US GameCube) now to build a MergeShip mm.o2r? The old file is kept as mm.o2r.stock.\n\n"
                  "Yes = select ROM, No = not now, Cancel = don't ask again.";
        int answer = MessageBoxA(nullptr, question.c_str(), "MergeShip: Majora's Mask assets",
                                 MB_YESNOCANCEL | MB_ICONQUESTION);
        if (answer == IDCANCEL) {
            CVarSetInteger(CVAR_ENHANCEMENT("MergeShip.AskForMMRom"), 0);
            CVarSave();
            return false;
        }
        if (answer != IDYES) {
            return false;
        }
    }

    while (true) {
        const std::string romPath = envRom.empty() ? MergeShip_PickMajorasMaskRom() : envRom;
        if (romPath.empty()) {
            return false;
        }

        // ZAPD reads the ROM from disk: hand it the big-endian copy, in a private working folder.
        const std::string work = MergeShip_MakeMajorasMaskWorkDir();
        const std::string z64 = work + "/baserom.z64";
        std::string version, errorTitle, errorText;
        if (!MergeShip_PrepareMajorasMaskRom(romPath, z64, version, errorTitle, errorText)) {
            std::error_code ec;
            std::filesystem::remove_all(work, ec);
            ShowError(errorTitle.c_str(), errorText);
            if (!envRom.empty()) {
                return false;
            }
            continue;
        }

        if (envRom.empty()) {
            MessageBoxA(nullptr,
                        "Building mm.o2r from your Majora's Mask ROM. This takes a few minutes; the game starts when "
                        "it's done.",
                        "MergeShip: extracting", MB_OK | MB_ICONINFORMATION);
        }

        bool ok = false;
        try {
            std::filesystem::copy(zapdAssets, work + "/assets", std::filesystem::copy_options::recursive);
            ok = RunZapd(std::filesystem::absolute(zapdExe).string(), work, z64, version);
            if (ok) {
                const std::string dataDir = Ship::Context::GetAppDirectoryPath(appShortName);
                const std::string dest = dataDir + "/mm.o2r";
                if (!existingInvalidPath.empty() && std::filesystem::exists(existingInvalidPath)) {
                    std::filesystem::rename(existingInvalidPath, existingInvalidPath + ".stock");
                }
                if (std::filesystem::exists(dest)) {
                    std::filesystem::rename(dest, dest + ".old");
                }
                std::filesystem::copy_file(work + "/mm.o2r", dest);
                SPDLOG_INFO("MergeShip: built {}", dest);
            }
        } catch (const std::exception& e) {
            SPDLOG_ERROR("MergeShip: MM extraction failed: {}", e.what());
            ok = false;
        }
        std::error_code ec;
        std::filesystem::remove_all(work, ec);

        if (!ok) {
            ShowError("Extraction failed", "Building mm.o2r failed; see the log. The game will start without "
                                           "Majora's Mask content.");
            return false;
        }
        return true;
    }
#endif
}
