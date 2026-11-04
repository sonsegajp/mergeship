// mergeship.exe: runs Ship of Harkinian (soh.dll) and 2 Ship 2 Harkinian (2ship.dll) in one process and
// one window, on one libultraship.dll. See Shipwright/mergeship2/docs/HOST_DESIGN.md.
//
//   mergeship.exe                      both games, SoH first (Termina is reached through the Lost Woods door)
//   --debug-switch                     Ctrl+F12 switches games by hand (debug: it opens the other game's title
//                                      and file select, which normal play never shows)
//   mergeship.exe --engine 2ship       both games, 2Ship first
//   mergeship.exe --only soh           just one game (behaves like the standalone exe)
//   --muted                            games start with master volume 0
//   --shots DIR                        where the automated tests put screenshots
//   --quit-after SECONDS               screenshot (with --shots) and exit after this long
//   --switch-test N                    switch N times automatically, with screenshots, then exit
//   --switch-seconds S                 time between automated switches after the first three (default 0.75)
//   --transfer-test N                  automated door test: the games' own test scripts walk the player through
//                                      the Lost Woods <-> Clock Tower door; the host screenshots every arrival and
//                                      exits (0) after the N-th, or (5) at --quit-after seconds without it
//   --settle SECONDS                   how long the last arrival is watched before the test ends (default 10)
//   --record FILE                      record every frame to FILE (a video, through ffmpeg) and FILE.frames.csv
//   --ffmpeg PATH                      the ffmpeg used by --record (default: ffmpeg on PATH)
#include "EngineModule.h"
#include "FrameRecorder.h"
#include "NetService.h"
#include "Screenshot.h"

#include <mergeship/AnchorNet.h>
#include <mergeship/SharedSave.h>
#include <ship/SharedAbi.h>

#include <windows.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace MergeShipHost;
using Clock = std::chrono::steady_clock;

namespace {

FILE* sLog = nullptr;

void Logf(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    SYSTEMTIME time;
    GetLocalTime(&time);
    char line[2200];
    snprintf(line, sizeof(line), "[%02d:%02d:%02d.%03d] [host] %s\n", time.wHour, time.wMinute, time.wSecond,
             time.wMilliseconds, buffer);
    fputs(line, stdout);
    fflush(stdout);
    if (sLog != nullptr) {
        fputs(line, sLog);
        fflush(sLog);
    }
}

struct Options {
    std::string first = "soh";
    std::string only;
    bool muted = false;
    std::wstring shots;
    double quitAfter = 0.0;
    int switchTest = 0;
    double switchSeconds = 0.75;
    int transferTest = 0;
    double settleSeconds = 10.0;
    std::wstring record;
    std::wstring ffmpeg;
    bool debugSwitch = false;
};

std::wstring Widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring out(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), length);
    return out;
}

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (arg == "--engine") {
            options.first = next();
        } else if (arg == "--only") {
            options.only = next();
            options.first = options.only;
        } else if (arg == "--muted") {
            options.muted = true;
        } else if (arg == "--shots") {
            options.shots = Widen(next());
        } else if (arg == "--quit-after") {
            options.quitAfter = atof(next().c_str());
        } else if (arg == "--switch-test") {
            options.switchTest = atoi(next().c_str());
        } else if (arg == "--switch-seconds") {
            options.switchSeconds = atof(next().c_str());
        } else if (arg == "--transfer-test") {
            options.transferTest = atoi(next().c_str());
        } else if (arg == "--settle") {
            options.settleSeconds = atof(next().c_str());
        } else if (arg == "--debug-switch") {
            options.debugSwitch = true;
        } else if (arg == "--record") {
            options.record = Widen(next());
        } else if (arg == "--ffmpeg") {
            options.ffmpeg = Widen(next());
        } else {
            Logf("ignoring unknown argument %s", arg.c_str());
        }
    }
    return options;
}

std::wstring ExeDirectory() {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path, length);
    return dir.substr(0, dir.find_last_of(L"\\/"));
}

void HostLog(void*, int32_t level, const char* message) {
    Logf("%s%s", level >= 2 ? "ERROR: " : level == 1 ? "warning: " : "", message);
}

const char* EngineKey(int32_t id) {
    return id == MS_ENGINE_SOH ? "soh" : id == MS_ENGINE_2SHIP ? "2ship" : "none";
}

// BUG-0084: the games' settings -- their configs, controller binds included -- follow the player to a new folder
// (each update is a fresh extract): a copy is kept in %APPDATA%\MergeShip2\config, refreshed at every start (the last
// session's) and exit, and a folder without its own config takes that copy. Test runs (a script set) neither take
// nor refresh it: their configs are the test's.
void CarryConfigs(bool exiting) {
    static const wchar_t* kFiles[] = { L"shipofharkinian.json", L"2ship2harkinian.json", L"imgui.ini" };
    const wchar_t* appData = _wgetenv(L"APPDATA");

    if ((appData == nullptr) || (getenv("MERGESHIP_TEST_SCRIPT_SOH") != nullptr) ||
        (getenv("MERGESHIP_TEST_SCRIPT_2SHIP") != nullptr)) {
        return;
    }
    const std::wstring root = std::wstring(appData) + L"\\MergeShip2";
    const std::wstring dir = root + L"\\config";
    CreateDirectoryW(root.c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    for (const wchar_t* name : kFiles) {
        const std::wstring saved = dir + L"\\" + name;
        if (GetFileAttributesW(name) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(name, saved.c_str(), FALSE);
        } else if (!exiting && (GetFileAttributesW(saved.c_str()) != INVALID_FILE_ATTRIBUTES) &&
                   CopyFileW(saved.c_str(), name, TRUE)) {
            Logf("settings: %ls taken from the last install (%ls)", name, saved.c_str());
        }
    }
}

// The shared saves (include/mergeship/SharedSave.h), one per Ocarina of Time file slot, kept next to SoH's own
// save files (Save/fileN.sav) as Save/mergeship_shared_N.bin. The unsaved debug file (-1) lives only in memory.
class SharedSaveStore {
  public:
    MsSharedSave* Get(int32_t slot) {
        auto it = mSaves.find(slot);
        if (it != mSaves.end()) {
            return it->second.get();
        }
        auto save = std::make_unique<MsSharedSave>();
        Reset(*save, slot);
        if (slot >= 0) {
            if (FILE* file = _wfopen(Path(slot).c_str(), L"rb")) {
                auto loadedBuffer = std::make_unique<MsSharedSave>(); // the ledger makes it too big for the stack
                MsSharedSave& loaded = *loadedBuffer;
                memset(&loaded, 0, sizeof(loaded));
                const size_t read = fread(&loaded, 1, sizeof(loaded), file);
                fclose(file);
                // Version 1 (M4) is version 2's prefix: it loads with an empty ledger (M3, SharedSave.h).
                const bool v1 = (read == MS_SHARED_SAVE_V1_SIZE) && (loaded.version == 1u) &&
                                (loaded.size == MS_SHARED_SAVE_V1_SIZE);
                // Version 2 is version 3's prefix: it loads with no team checks yet (SharedSave.h).
                const bool v2 = (read == MS_SHARED_SAVE_V2_SIZE) && (loaded.version == 2u) &&
                                (loaded.size == MS_SHARED_SAVE_V2_SIZE);
                // Version 3 is version 4's prefix: it loads with no buttons to carry (BUG-0101).
                const bool v3 = (read == MS_SHARED_SAVE_V3_SIZE) && (loaded.version == 3u) &&
                                (loaded.size == MS_SHARED_SAVE_V3_SIZE);
                // Version 4 is version 5's prefix: no B sword to carry, no mask given away (BUG-0120, BUG-0127).
                const bool v4 = (read == MS_SHARED_SAVE_V4_SIZE) && (loaded.version == 4u) &&
                                (loaded.size == MS_SHARED_SAVE_V4_SIZE);
                if (loaded.magic == MS_SHARED_SAVE_MAGIC &&
                    (v1 || v2 || v3 || v4 || (read == sizeof(loaded) && loaded.version == MS_SHARED_SAVE_VERSION &&
                                  loaded.size == sizeof(MsSharedSave)))) {
                    *save = loaded;
                    save->version = MS_SHARED_SAVE_VERSION;
                    save->size = sizeof(MsSharedSave);
                    save->saveSlot = slot;
                    Logf("shared save %d loaded (generation %u, last written by %s)", slot, save->generation,
                         EngineKey(save->lastWriter));
                } else {
                    Logf("warning: shared save %d on disk is not version %u (%zu bytes); starting a new one", slot,
                         MS_SHARED_SAVE_VERSION, read);
                }
            }
        }
        MsSharedSave* result = save.get();
        mSaves[slot] = std::move(save);
        return result;
    }

    void Commit(int32_t slot) {
        if (slot < 0) {
            return;
        }
        MsSharedSave* save = Get(slot);
        CreateDirectoryW(L"Save", nullptr);
        const std::wstring path = Path(slot);
        const std::wstring temp = path + L".tmp";
        FILE* file = _wfopen(temp.c_str(), L"wb");
        if (file == nullptr) {
            Logf("ERROR: cannot write %ls", temp.c_str());
            return;
        }
        const bool ok = fwrite(save, 1, sizeof(*save), file) == sizeof(*save);
        fclose(file);
        if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            Logf("ERROR: cannot write %ls", path.c_str());
            return;
        }
        Logf("shared save %d written (generation %u, by %s)", slot, save->generation, EngineKey(save->lastWriter));
    }

  private:
    static std::wstring Path(int32_t slot) {
        return L"Save\\mergeship_shared_" + std::to_wstring(slot + 1) + L".bin";
    }
    static void Reset(MsSharedSave& save, int32_t slot) {
        memset(&save, 0, sizeof(save));
        save.magic = MS_SHARED_SAVE_MAGIC;
        save.version = MS_SHARED_SAVE_VERSION;
        save.size = sizeof(MsSharedSave);
        save.saveSlot = slot;
    }

    std::map<int32_t, std::unique_ptr<MsSharedSave>> mSaves;
};

struct Engine {
    std::string key; // "soh" / "2ship"
    std::wstring dll;
    EngineModule module;
};

class Host {
  public:
    Host(Options options) : mOptions(std::move(options)) {
    }

    int Run(int argc, char** argv);

    void RequestTransfer(const MsTransfer& transfer);
    MsSharedSave* SharedSave(int32_t slot) {
        return mSharedSaves.Get(slot);
    }
    void CommitSharedSave(int32_t slot) {
        mSharedSaves.Commit(slot);
    }
    // MsHostServices.GetEngineContext: a loaded engine's Ship::Context* (NULL until its MsEngine_Init has made it).
    void* EngineContext(int32_t engineId) {
        for (auto& engine : mEngines) {
            if (engine->module.Id() == engineId) {
                return engine->module.Context();
            }
        }
        return nullptr;
    }
    // MsHostServices.ExtractRom: a loaded engine's own ROM extractor, usable before its MsEngine_Init (first run).
    int32_t ExtractRom(int32_t engineId, const char* romPath, const char* destDir,
                       void (*progress)(void* user, uint64_t done, uint64_t total), void* user) {
        for (auto& engine : mEngines) {
            if (engine->module.Id() == engineId) {
                if (romPath == nullptr) {
                    return engine->module.ExtractRom(nullptr, nullptr, nullptr, nullptr);
                }
                Logf("extracting %s with %s's extractor into %s", romPath, EngineKey(engineId), destDir);
                const int32_t result = engine->module.ExtractRom(romPath, destDir, progress, user);
                Logf("%s extraction %s (%d)", EngineKey(engineId), result == 0 ? "done" : "FAILED", result);
                return result;
            }
        }
        return -1;
    }

  private:
    bool LoadEngines();
    bool InitEngines(int argc, char** argv);
    void Switch(const char* why);
    void Transfer();
    void TransferTestStep(double sinceStart);
    void Shot(const wchar_t* name);
    void SetTitle();
    [[noreturn]] void Exit(int code);

    Options mOptions;
    FrameRecorder mRecorder;
    std::vector<std::unique_ptr<Engine>> mEngines;
    size_t mActive = 0;
    MsHostServices mServices = {};
    int mSwitches = 0;
    uint64_t mFrames = 0;
    uint64_t mFramesSinceSwitch = 0;
    Clock::time_point mSwitchTime;
    double mLastRateReport = 0.0;
    int mSecondShots = 0;
    bool mHotkeyShotPending = false;
    uint64_t mLastRateFrames = 0;
    // A transfer is acted on after the frame of the engine that asked for it.
    bool mTransferPending = false;
    MsTransfer mTransfer = {};
    int mTransfers = 0;
    Clock::time_point mArrivalTime;
    int mArrivalShots = 0;
    SharedSaveStore mSharedSaves;
    // The one Anchor connection both engines share (AnchorNet.h): switching worlds never leaves the room.
    NetService mNet;
};

void HostRequestTransfer(void* host, const MsTransfer* transfer) {
    if (host != nullptr && transfer != nullptr) {
        static_cast<Host*>(host)->RequestTransfer(*transfer);
    }
}

void* HostGetSharedSave(void* host, int32_t slot) {
    return static_cast<Host*>(host)->SharedSave(slot);
}

void HostCommitSharedSave(void* host, int32_t slot) {
    static_cast<Host*>(host)->CommitSharedSave(slot);
}

void* HostGetEngineContext(void* host, int32_t engineId) {
    return static_cast<Host*>(host)->EngineContext(engineId);
}

// MsHostServices.SetMenuMirror / GetMenuMirror (BUG-0096): each engine's menu as plain data, for the other's menu.
static const struct MsMenuMirror* sMenuMirrors[4] = {};

void HostSetMenuMirror(void*, int32_t engineId, const struct MsMenuMirror* mirror) {
    if ((engineId >= 0) && (engineId < 4)) {
        sMenuMirrors[engineId] = mirror;
    }
}

const struct MsMenuMirror* HostGetMenuMirror(void*, int32_t engineId) {
    return ((engineId >= 0) && (engineId < 4)) ? sMenuMirrors[engineId] : nullptr;
}

int32_t HostExtractRom(void* host, int32_t engineId, const char* romPath, const char* destDir,
                       void (*progress)(void* user, uint64_t done, uint64_t total), void* user) {
    return static_cast<Host*>(host)->ExtractRom(engineId, romPath, destDir, progress, user);
}

void Host::RequestTransfer(const MsTransfer& transfer) {
    if (mTransferPending) {
        Logf("warning: transfer %s -> %s ignored: one is already pending", EngineKey(transfer.from),
             EngineKey(transfer.to));
        return;
    }
    mTransfer = {};
    memcpy(&mTransfer, &transfer, transfer.size < sizeof(MsTransfer) ? transfer.size : sizeof(MsTransfer));
    // Fields an older sender did not have read as "not given".
    if (!MS_TRANSFER_HAS(&transfer, saveSlot)) {
        mTransfer.saveSlot = -1;
    }
    if (!MS_TRANSFER_HAS(&transfer, playerForm)) {
        mTransfer.flags &= ~MS_TRANSFER_FLAG_PLAYER_BODY;
        mTransfer.playerAge = mTransfer.playerForm = -1;
    }
    mTransfer.size = sizeof(MsTransfer);
    mTransfer.playerName[sizeof(mTransfer.playerName) - 1] = '\0';
    mTransfer.unifiedSeedFile[sizeof(mTransfer.unifiedSeedFile) - 1] = '\0';
    mTransferPending = true;
    if (mTransfer.flags & MS_TRANSFER_FLAG_UNIFIED_SEED) {
        Logf("transfer carries the unified seed %u (%s)", mTransfer.unifiedSeed,
             (mTransfer.unifiedSeedFile[0] != '\0') ? mTransfer.unifiedSeedFile : "no seed file");
    }
    Logf("transfer requested: %s -> %s, entrance 0x%X, flags 0x%X, file slot %d (\"%s\"), age %d, form %d",
         EngineKey(transfer.from), EngineKey(transfer.to), transfer.entrance, mTransfer.flags, mTransfer.saveSlot,
         mTransfer.playerName, mTransfer.playerAge, mTransfer.playerForm);
}

// After the requesting engine's frame (the one that finished fading out): park it, hand the player over.
void Host::Transfer() {
    mTransferPending = false;
    size_t target = mEngines.size();
    for (size_t i = 0; i < mEngines.size(); i++) {
        if (mEngines[i]->module.Id() == mTransfer.to) {
            target = i;
        }
    }
    if (target == mEngines.size() || target == mActive) {
        Logf("ERROR: transfer to %s impossible (not loaded, or already active)", EngineKey(mTransfer.to));
        // Turn it down: a transfer back to the requester releases the black screen it holds.
        MsTransfer back = mTransfer;
        back.from = mTransfer.to;
        back.to = mTransfer.from;
        back.entrance = -1;
        mEngines[mActive]->module.Suspend();
        mEngines[mActive]->module.Resume(&back);
        return;
    }
    mTransfer.sharedSave = mSharedSaves.Get(mTransfer.saveSlot);
    mTransfer.sharedSaveSize = sizeof(MsSharedSave);
    const auto start = Clock::now();
    mEngines[mActive]->module.Suspend();
    mNet.SetActiveEngine(mEngines[target]->module.Id());
    mEngines[target]->module.Resume(&mTransfer);
    mActive = target;
    mTransfers++;
    mSwitches++;
    mFramesSinceSwitch = 0;
    mSwitchTime = mArrivalTime = Clock::now();
    mArrivalShots = 0;
    SetTitle();
    Logf("transfer %d: now %s (entrance 0x%X), took %.1f ms", mTransfers, mEngines[mActive]->module.Name().c_str(),
         mTransfer.entrance, std::chrono::duration<double, std::milli>(mSwitchTime - start).count());
}

// --transfer-test: the games' scripts do the walking; the host screenshots each arrival and ends the run.
void Host::TransferTestStep(double sinceStart) {
    if (mTransfers > 0) {
        static const double kShotTimes[] = { 1.0, 4.0 };
        const double sinceArrival = std::chrono::duration<double>(Clock::now() - mArrivalTime).count();
        while (mArrivalShots < (int)(sizeof(kShotTimes) / sizeof(kShotTimes[0])) &&
               sinceArrival >= kShotTimes[mArrivalShots]) {
            wchar_t name[96];
            swprintf(name, 96, L"host_transfer_%d_%hs_%02ds.bmp", mTransfers,
                     EngineKey(mEngines[mActive]->module.Id()), (int)kShotTimes[mArrivalShots]);
            Shot(name);
            mArrivalShots++;
        }
        if (mTransfers >= mOptions.transferTest && sinceArrival >= mOptions.settleSeconds) {
            wchar_t name[96];
            swprintf(name, 96, L"host_transfer_%d_%hs_final.bmp", mTransfers,
                     EngineKey(mEngines[mActive]->module.Id()));
            Shot(name);
            Logf("TRANSFER TEST PASSED: %d transfers, %llu frames, %.1f s", mTransfers, (unsigned long long)mFrames,
                 sinceStart);
            Exit(0);
        }
    }
    if (mOptions.quitAfter > 0.0 && sinceStart >= mOptions.quitAfter) {
        Shot(L"host_transfer_timeout.bmp");
        Logf("TRANSFER TEST FAILED: %d of %d transfers after %.1f s", mTransfers, mOptions.transferTest, sinceStart);
        Exit(5);
    }
}

bool Host::LoadEngines() {
    std::vector<std::pair<std::string, std::wstring>> wanted;
    if (!mOptions.only.empty()) {
        wanted.push_back({ mOptions.only, Widen(mOptions.only) + L".dll" });
    } else {
        // The first engine owns the window, audio device and controllers.
        const std::string second = mOptions.first == "soh" ? "2ship" : "soh";
        wanted.push_back({ mOptions.first, Widen(mOptions.first) + L".dll" });
        wanted.push_back({ second, Widen(second) + L".dll" });
    }

    const char* lusAbi = LUS_GetAbiId();
    Logf("libultraship.dll ABI %s", lusAbi);
    for (auto& [key, dll] : wanted) {
        auto engine = std::make_unique<Engine>();
        engine->key = key;
        engine->dll = dll;
        if (!engine->module.Load(dll)) {
            Logf("ERROR: cannot load %s: %s", key.c_str(), engine->module.Error().c_str());
            return false;
        }
        if (!engine->module.CheckAbi(lusAbi)) {
            Logf("ERROR: %s does not match this host: %s", key.c_str(), engine->module.Error().c_str());
            return false;
        }
        Logf("loaded %s (%s), built against libultraship %s", key.c_str(), engine->module.Name().c_str(),
             engine->module.Abi()->lusAbiId);
        mEngines.push_back(std::move(engine));
    }
    return true;
}

bool Host::InitEngines(int argc, char** argv) {
    mServices.size = sizeof(MsHostServices);
    mServices.apiVersion = MS_ENGINE_API_VERSION;
    mServices.flags = mOptions.muted ? MS_HOST_FLAG_MUTED : MS_HOST_FLAG_NONE;
    mServices.host = this;
    mServices.RequestTransfer = HostRequestTransfer;
    mServices.Log = HostLog;
    mServices.GetSharedSave = HostGetSharedSave;
    mServices.CommitSharedSave = HostCommitSharedSave;
    mServices.GetEngineContext = HostGetEngineContext; // SettingsSync.h: 2Ship reads SoH's settings
    mServices.ExtractRom = HostExtractRom; // first run: SoH builds mm.o2r with 2Ship's extractor (HOST_DESIGN.md)
    mServices.SetMenuMirror = HostSetMenuMirror; // BUG-0096: each menu shows the other game's settings
    mServices.GetMenuMirror = HostGetMenuMirror;
    std::vector<int32_t> engineIds;
    for (auto& engine : mEngines) {
        engineIds.push_back(engine->module.Id());
    }
    mNet.Init(engineIds, [](const char* message) { Logf("%s", message); });
    mServices.net = mNet.Api();

    for (size_t i = 0; i < mEngines.size(); i++) {
        auto& engine = *mEngines[i];
        if (i > 0) {
            // Park the window's owner; the next game borrows its window, audio and controllers.
            mEngines[0]->module.Suspend();
            mServices.shareContext = mEngines[0]->module.Context();
        }
        const auto start = Clock::now();
        Logf("initializing %s", engine.module.Name().c_str());
        if (engine.module.Init(&mServices, argc, argv) != 0) {
            Logf("ERROR: %s failed to initialize", engine.module.Name().c_str());
            return false;
        }
        Logf("%s initialized in %.2f s", engine.module.Name().c_str(),
             std::chrono::duration<double>(Clock::now() - start).count());
        if (i > 0) {
            engine.module.Suspend();
            mEngines[0]->module.Resume(nullptr);
        }
    }
    mActive = 0;
    mNet.SetActiveEngine(mEngines[mActive]->module.Id());
    return true;
}

void Host::SetTitle() {
    if (HWND window = static_cast<HWND>(FindGameWindow())) {
        const std::wstring title = L"MergeShip2 - " + Widen(mEngines[mActive]->module.Name());
        SetWindowTextW(window, title.c_str());
    }
}

void Host::Switch(const char* why) {
    if (mEngines.size() < 2) {
        return;
    }
    const auto start = Clock::now();
    const size_t next = (mActive + 1) % mEngines.size();
    mEngines[mActive]->module.Suspend();
    mNet.SetActiveEngine(mEngines[next]->module.Id());
    mEngines[next]->module.Resume(nullptr);
    mActive = next;
    mSwitches++;
    mFramesSinceSwitch = 0;
    mSwitchTime = Clock::now();
    SetTitle();
    Logf("switch %d (%s): now %s, took %.1f ms", mSwitches, why, mEngines[mActive]->module.Name().c_str(),
         std::chrono::duration<double, std::milli>(mSwitchTime - start).count());
}

void Host::Shot(const wchar_t* name) {
    if (mOptions.shots.empty()) {
        return;
    }
    const std::wstring path = mOptions.shots + L"\\" + name;
    const bool ok = CaptureWindowToBmp(FindGameWindow(), path);
    Logf("screenshot %ls: %s (active %s, frame %llu)", path.c_str(), ok ? "ok" : "FAILED",
         mEngines[mActive]->module.Name().c_str(), (unsigned long long)mFrames);
}

void Host::Exit(int code) {
    Logf("shutting down (exit code %d)", code);
    mRecorder.Close();
    for (auto& engine : mEngines) {
        engine->module.Shutdown();
    }
    mNet.Shutdown(); // closes the Anchor socket cleanly, so the server sees the player leave at once
    Logf("exit %d", code);
    if (sLog != nullptr) {
        fclose(sLog);
    }
    // The games share one window, ImGui context and audio device, and neither can tear those down while
    // the other's objects still point into them. Everything worth keeping (CVars, window placement, saves,
    // logs) was written by Shutdown above.
    TerminateProcess(GetCurrentProcess(), code);
    ExitProcess(code);
}

int Host::Run(int argc, char** argv) {
    if (!LoadEngines()) {
        Exit(2);
    }
    if (!InitEngines(argc, argv)) {
        Exit(3);
    }
    SetTitle();
    if (!mOptions.record.empty()) {
        mRecorder.Open(mOptions.record, mOptions.ffmpeg);
        Logf("recording every frame to %ls", mOptions.record.c_str());
    }

    const auto begin = Clock::now();
    mSwitchTime = begin;
    bool hotkeyWasDown = false;
    // Automated switch test: SoH title, 2Ship title, SoH again (screenshots), then fast switches.
    int testStage = 0;

    while (true) {
        const int32_t result = mEngines[mActive]->module.RunFrame();
        mFrames++;
        mFramesSinceSwitch++;
        mRecorder.Frame(mFrames, EngineKey(mEngines[mActive]->module.Id()));
        if (result == MS_FRAME_EXITED) {
            Logf("%s exited after %llu frames", mEngines[mActive]->module.Name().c_str(),
                 (unsigned long long)mFrames);
            Exit(0);
        }
        if (result == MS_FRAME_ERROR) {
            Logf("ERROR: %s reported an unrecoverable error", mEngines[mActive]->module.Name().c_str());
            Exit(4);
        }
        if (result == MS_FRAME_TRANSFER && !mTransferPending) {
            Logf("warning: %s returned TRANSFER without requesting one", mEngines[mActive]->module.Name().c_str());
        }
        if (mTransferPending) {
            Transfer();
            continue;
        }

        // Ctrl+F12, while our window has focus.
        DWORD foregroundPid = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundPid);
        // The low bit catches a press shorter than a frame (a tap, or injected input).
        const SHORT ctrl = GetAsyncKeyState(VK_CONTROL);
        const SHORT f12 = GetAsyncKeyState(VK_F12);
        const bool hotkeyDown = foregroundPid == GetCurrentProcessId() && (ctrl & 0x8001) && (f12 & 0x8001);
        // Normal play starts in Hyrule and reaches Termina only through the door: no hand switch to the other game's
        // title and file select, unless asked for (--debug-switch).
        if (hotkeyDown && !hotkeyWasDown && mOptions.debugSwitch) {
            Switch("Ctrl+F12");
            mHotkeyShotPending = true;
        }
        if (mHotkeyShotPending && std::chrono::duration<double>(Clock::now() - mSwitchTime).count() >= 3.0) {
            wchar_t name[64];
            swprintf(name, 64, L"hotkey_switch_%02d.bmp", mSwitches);
            Shot(name);
            mHotkeyShotPending = false;
        }
        hotkeyWasDown = hotkeyDown;

        const double sinceStart = std::chrono::duration<double>(Clock::now() - begin).count();
        if (sinceStart - mLastRateReport >= 5.0) {
            Logf("%s: %.1f frames/s over the last %.1f s", mEngines[mActive]->module.Name().c_str(),
                 (mFrames - mLastRateFrames) / (sinceStart - mLastRateReport), sinceStart - mLastRateReport);
            mLastRateReport = sinceStart;
            mLastRateFrames = mFrames;
        }
        const double sinceSwitch = std::chrono::duration<double>(Clock::now() - mSwitchTime).count();

        if (mOptions.transferTest > 0) {
            TransferTestStep(sinceStart);
            continue;
        }

        if (mOptions.quitAfter > 0.0 && mOptions.switchTest == 0 && sinceStart >= mOptions.quitAfter) {
            Shot(L"quit.bmp");
            Exit(0);
        }

        if (mOptions.switchTest > 0 && mEngines.size() > 1) {
            // Long enough for each game to reach its title screen the first time.
            constexpr double kFirstTitle = 20.0;
            constexpr double kSecondTitle = 40.0;
            constexpr double kBackAgain = 5.0;
            switch (testStage) {
                case 0:
                    if (sinceSwitch >= kFirstTitle) {
                        Shot(L"01_first_title.bmp");
                        Switch("test");
                        testStage = 1;
                    }
                    break;
                case 1:
                    // The second game's boot sequence has dark stretches; keep a few frames of it.
                    if (sinceSwitch >= 5.0 * (mSecondShots + 1) && sinceSwitch < kSecondTitle) {
                        wchar_t name[64];
                        swprintf(name, 64, L"02_second_%02ds.bmp", 5 * (mSecondShots + 1));
                        Shot(name);
                        mSecondShots++;
                    }
                    if (sinceSwitch >= kSecondTitle) {
                        Shot(L"02_second_title.bmp");
                        Switch("test");
                        testStage = 2;
                    }
                    break;
                case 2:
                    if (sinceSwitch >= kBackAgain) {
                        Shot(L"03_first_again.bmp");
                        testStage = 3;
                    }
                    break;
                case 3:
                    if (mSwitches >= mOptions.switchTest) {
                        if (sinceSwitch >= 3.0) {
                            Shot(L"04_after_switches.bmp");
                            Logf("SWITCH TEST PASSED: %d switches, %llu frames, %.1f s", mSwitches,
                                 (unsigned long long)mFrames, sinceStart);
                            Exit(0);
                        }
                    } else if (sinceSwitch >= mOptions.switchSeconds && mFramesSinceSwitch >= 5) {
                        Switch("test");
                    }
                    break;
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    // Everything (configs, logs, saves, archives) lives next to mergeship.exe, as with the standalone exes.
    SetCurrentDirectoryW(ExeDirectory().c_str());
    sLog = _wfopen(L"mergeship-host.log", L"w");
    Options options = ParseOptions(argc, argv);
    Logf("mergeship.exe starting (first engine %s%s)", options.first.c_str(), options.only.empty() ? "" : ", only");
    // The games' argv: just the program name; host options are not theirs.
    static char* gameArgv[] = { argv[0], nullptr };
    CarryConfigs(false);
    Host host(options);
    const int result = host.Run(1, gameArgv);
    CarryConfigs(true);
    return result;
}
