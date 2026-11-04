// MergeShip2 for Android: libmain.so, which SDL's Java activity starts (SDL_main). It does what mergeship.exe does
// (main.cpp) -- runs Ship of Harkinian (libsoh.so) and 2 Ship 2 Harkinian (lib2ship.so) in one process and one
// window, on one libultraship.so -- without the Windows test harness (screenshots, recording, hotkeys, switch tests).
//
// Everything (configs, logs, saves, the o2r archives the player copies over) lives in the app's external files folder
// (Android/data/<package>/files), which is where libultraship looks too (SDL_AndroidGetExternalStoragePath).
#include "EngineModule.h"
#include "NetService.h"

#include <mergeship/AnchorNet.h>
#include <mergeship/SharedSave.h>
#include <ship/SharedAbi.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <android/log.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
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
    __android_log_print(ANDROID_LOG_INFO, "mergeship", "%s", buffer);
    if (sLog != nullptr) {
        const time_t now = time(nullptr);
        struct tm local = {};
        localtime_r(&now, &local);
        fprintf(sLog, "[%02d:%02d:%02d] [host] %s\n", local.tm_hour, local.tm_min, local.tm_sec, buffer);
        fflush(sLog);
    }
}

void HostLog(void*, int32_t level, const char* message) {
    Logf("%s%s", level >= 2 ? "ERROR: " : level == 1 ? "warning: " : "", message);
}

const char* EngineKey(int32_t id) {
    return id == MS_ENGINE_SOH ? "soh" : id == MS_ENGINE_2SHIP ? "2ship" : "none";
}

// The shared saves (include/mergeship/SharedSave.h), one per Ocarina of Time file slot, as on Windows:
// Save/mergeship_shared_N.bin next to SoH's own saves.
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
            if (FILE* file = fopen(Path(slot).c_str(), "rb")) {
                auto loadedBuffer = std::make_unique<MsSharedSave>();
                MsSharedSave& loaded = *loadedBuffer;
                memset(&loaded, 0, sizeof(loaded));
                const size_t read = fread(&loaded, 1, sizeof(loaded), file);
                fclose(file);
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
        mkdir("Save", 0755);
        const std::string path = Path(slot);
        const std::string temp = path + ".tmp";
        FILE* file = fopen(temp.c_str(), "wb");
        if (file == nullptr) {
            Logf("ERROR: cannot write %s", temp.c_str());
            return;
        }
        const bool ok = fwrite(save, 1, sizeof(*save), file) == sizeof(*save);
        fclose(file);
        if (!ok || rename(temp.c_str(), path.c_str()) != 0) {
            Logf("ERROR: cannot write %s", path.c_str());
            return;
        }
        Logf("shared save %d written (generation %u, by %s)", slot, save->generation, EngineKey(save->lastWriter));
    }

  private:
    static std::string Path(int32_t slot) {
        return "Save/mergeship_shared_" + std::to_string(slot + 1) + ".bin";
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
    EngineModule module;
};

class Host {
  public:
    int Run();

    void RequestTransfer(const MsTransfer& transfer);
    MsSharedSave* SharedSave(int32_t slot) {
        return mSharedSaves.Get(slot);
    }
    void CommitSharedSave(int32_t slot) {
        mSharedSaves.Commit(slot);
    }
    void* EngineContext(int32_t engineId) {
        for (auto& engine : mEngines) {
            if (engine->module.Id() == engineId) {
                return engine->module.Context();
            }
        }
        return nullptr;
    }

  private:
    bool LoadEngines();
    bool InitEngines();
    void Transfer();
    [[noreturn]] void Exit(int code);

    std::vector<std::unique_ptr<Engine>> mEngines;
    size_t mActive = 0;
    MsHostServices mServices = {};
    uint64_t mFrames = 0;
    double mLastRateReport = 0.0;
    uint64_t mLastRateFrames = 0;
    bool mTransferPending = false;
    MsTransfer mTransfer = {};
    int mTransfers = 0;
    SharedSaveStore mSharedSaves;
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

// No ROM extraction on the phone: the player copies oot.o2r and mm.o2r over.
int32_t HostExtractRom(void*, int32_t, const char*, const char*, void (*)(void*, uint64_t, uint64_t), void*) {
    return -1;
}

void Host::RequestTransfer(const MsTransfer& transfer) {
    if (mTransferPending) {
        Logf("warning: transfer %s -> %s ignored: one is already pending", EngineKey(transfer.from),
             EngineKey(transfer.to));
        return;
    }
    mTransfer = {};
    memcpy(&mTransfer, &transfer, transfer.size < sizeof(MsTransfer) ? transfer.size : sizeof(MsTransfer));
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
    Logf("transfer requested: %s -> %s, entrance 0x%X, flags 0x%X, file slot %d", EngineKey(transfer.from),
         EngineKey(transfer.to), transfer.entrance, mTransfer.flags, mTransfer.saveSlot);
}

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
    Logf("transfer %d: now %s (entrance 0x%X), took %.1f ms", mTransfers, mEngines[mActive]->module.Name().c_str(),
         mTransfer.entrance, std::chrono::duration<double, std::milli>(Clock::now() - start).count());
}

bool Host::LoadEngines() {
    // SoH first: it owns the window, audio device and controllers; 2Ship borrows them.
    const std::pair<const char*, const char*> wanted[] = { { "soh", "libsoh.so" }, { "2ship", "lib2ship.so" } };
    const char* lusAbi = LUS_GetAbiId();
    Logf("libultraship.so ABI %s", lusAbi);
    for (const auto& [key, library] : wanted) {
        auto engine = std::make_unique<Engine>();
        engine->key = key;
        if (!engine->module.Load(library)) {
            Logf("ERROR: cannot load %s: %s", key, engine->module.Error().c_str());
            return false;
        }
        if (!engine->module.CheckAbi(lusAbi)) {
            Logf("ERROR: %s does not match this host: %s", key, engine->module.Error().c_str());
            return false;
        }
        Logf("loaded %s (%s)", key, engine->module.Name().c_str());
        mEngines.push_back(std::move(engine));
    }
    return true;
}

bool Host::InitEngines() {
    static char program[] = "mergeship";
    static char* argv[] = { program, nullptr };
    mServices.size = sizeof(MsHostServices);
    mServices.apiVersion = MS_ENGINE_API_VERSION;
    mServices.flags = MS_HOST_FLAG_NONE;
    mServices.host = this;
    mServices.RequestTransfer = HostRequestTransfer;
    mServices.Log = HostLog;
    mServices.GetSharedSave = HostGetSharedSave;
    mServices.CommitSharedSave = HostCommitSharedSave;
    mServices.GetEngineContext = HostGetEngineContext;
    mServices.ExtractRom = HostExtractRom;
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
            mEngines[0]->module.Suspend();
            mServices.shareContext = mEngines[0]->module.Context();
        }
        const auto start = Clock::now();
        Logf("initializing %s", engine.module.Name().c_str());
        if (engine.module.Init(&mServices, 1, argv) != 0) {
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

void Host::Exit(int code) {
    Logf("shutting down (exit code %d)", code);
    for (auto& engine : mEngines) {
        engine->module.Shutdown();
    }
    mNet.Shutdown();
    Logf("exit %d", code);
    if (sLog != nullptr) {
        fclose(sLog);
    }
    // As on Windows: the games share one window, GL context and audio device and cannot tear them down while the
    // other still points into them. Everything worth keeping was written by Shutdown above.
    _exit(code);
}

int Host::Run() {
    if (!LoadEngines()) {
        Exit(2);
    }
    if (!InitEngines()) {
        Exit(3);
    }
    const auto begin = Clock::now();
    while (true) {
        const int32_t result = mEngines[mActive]->module.RunFrame();
        mFrames++;
        if (result == MS_FRAME_EXITED) {
            Logf("%s exited after %llu frames", mEngines[mActive]->module.Name().c_str(),
                 (unsigned long long)mFrames);
            Exit(0);
        }
        if (result == MS_FRAME_ERROR) {
            Logf("ERROR: %s reported an unrecoverable error", mEngines[mActive]->module.Name().c_str());
            Exit(4);
        }
        if (mTransferPending) {
            Transfer();
            continue;
        }
        const double sinceStart = std::chrono::duration<double>(Clock::now() - begin).count();
        if (sinceStart - mLastRateReport >= 30.0) {
            Logf("%s: %.1f frames/s", mEngines[mActive]->module.Name().c_str(),
                 (mFrames - mLastRateFrames) / (sinceStart - mLastRateReport));
            mLastRateReport = sinceStart;
            mLastRateFrames = mFrames;
        }
    }
}

// The games' static tables are built in functions with very large frames: the Windows host reserves 16 MB for the
// thread they run on (host/CMakeLists.txt /STACK), and so does this one.
void* GameThread(void*) {
    Host* host = new Host();
    host->Run();
    return nullptr;
}

} // namespace

extern "C" int SDL_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // The app's own folder on shared storage, where libultraship also keeps configs, logs and archives.
    const char* dir = SDL_AndroidGetExternalStoragePath();
    if (dir != nullptr) {
        mkdir(dir, 0755);
        chdir(dir);
    }
    sLog = fopen("mergeship-host.log", "w");
    Logf("MergeShip2 (Android) starting in %s", dir != nullptr ? dir : "(no external storage)");

    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 16 * 1024 * 1024);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, GameThread, nullptr) != 0) {
        Logf("ERROR: cannot start the game thread");
        return 1;
    }
    pthread_attr_destroy(&attributes);
    pthread_join(thread, nullptr);
    return 0;
}
