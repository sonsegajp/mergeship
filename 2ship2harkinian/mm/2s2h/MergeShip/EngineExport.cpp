// MergeShip2 host engine exports for 2 Ship 2 Harkinian (2ship.dll).
//
// mergeship.exe loads soh.dll and 2ship.dll, which share one libultraship.dll, one window and one audio
// device, and switches between them. This file is the whole of 2Ship's side of that contract
// (include/mergeship/EngineApi.h in the MergeShip2 repository); it is compiled only when MERGESHIP_HOSTED
// is ON. See Shipwright/mergeship2/docs/HOST_DESIGN.md.
#ifdef MERGESHIP_HOSTED

#define MS_ENGINE_BUILDING
#include <mergeship/EngineApi.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/audio/Audio.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/bridge/crashhandlerbridge.h>

#include "BenPort.h"
#include "2s2h/Extractor/Extract.h"
#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/MergeShip/MergeShipSettingsSync.h"
#include "2s2h/ShipInit.hpp"
#include <mergeship/MenuMirror.hpp> // BUG-0096: each menu shows the other game's settings

#ifndef LUS_ABI_ID
#error "LUS_ABI_ID must be defined for this file (see mm/CMakeLists.txt, MERGESHIP_HOSTED)"
#endif

extern "C" {
int ShipHosted_Init(int argc, char* argv[]); // mm/src/code/main.c
int32_t Graph_RunHostedFrame(void);          // mm/src/code/graph.c
void gfx_texture_cache_clear();              // libultraship
// 2s2h/Network/Anchor/MergeShipAnchorHost.cpp: Anchor on the host's one connection, shared with Hyrule
void MergeShipAnchor_SetHostServices(const MsHostServices* services);
void MergeShipAnchor_OnEngineReady(void);
void MergeShipAnchor_OnEngineSuspend(void);
void MergeShipAnchor_OnEngineResume(void);
}

// libultraship.dll exports its own __osMaxControllers, but data can only be imported through a dllimport
// declaration, and the game declares it plainly (a static libultraship made that fine). Nothing writes it
// (the libultra io sources that would are not built), so the game keeps its own copy with the same value.
extern "C" uint8_t __osMaxControllers = 4; // MAXCONTROLLERS

namespace {
constexpr int32_t kResumeInputBlockId = 0x4D53; // 'MS'
constexpr int32_t kDesiredBuffered = 1680;      // OTRGlobals::Initialize's InitAudio

const MsHostServices* sHost = nullptr;
Ship::Context* sContext = nullptr;
Ship::GuiEngineState sGui;
bool sActive = false;
bool sInputBlocked = false;

void HostLog(int32_t level, const char* message) {
    if (sHost != nullptr && sHost->Log != nullptr) {
        sHost->Log(sHost->host, level, message);
    }
}

// 2ship.dll links its own copy of ImGui; it must draw into the context libultraship.dll created.
void SelectImGuiContext(ImGuiContext* context) {
    ImGui::SetCurrentContext(context);
}

void UseContextLogger() {
    if (sContext != nullptr && sContext->GetLogger() != nullptr) {
        spdlog::set_default_logger(sContext->GetLogger());
    }
}

void ApplyMute() {
    if (sHost != nullptr && (sHost->flags & MS_HOST_FLAG_MUTED)) {
        CVarSetFloat("gSettings.Audio.MasterVolume", 0.0f);
    }
}
} // namespace

namespace BenGui {
const MsMenuMirror* MergeShip_BuildMenuMirror(); // MergeShipHostMenu.cpp
}

// BUG-0096 (MergeShipHostMenu.cpp's "Hyrule Settings"): SoH's menu as plain data, and its CVars.
const MsMenuMirror* MergeShip_OtherMenuMirror() {
    if ((sHost == nullptr) || !MS_HOST_HAS(sHost, GetMenuMirror) || (sHost->GetMenuMirror == nullptr)) {
        return nullptr;
    }
    return sHost->GetMenuMirror(sHost->host, MS_ENGINE_SOH);
}

Ship::ConsoleVariable* MergeShip_OtherConsoleVariables() {
    if ((sHost == nullptr) || !MS_HOST_HAS(sHost, GetEngineContext) || (sHost->GetEngineContext == nullptr)) {
        return nullptr;
    }
    auto* context = static_cast<Ship::Context*>(sHost->GetEngineContext(sHost->host, MS_ENGINE_SOH));
    return (context != nullptr) ? context->GetConsoleVariables().get() : nullptr;
}

extern "C" {

MS_ENGINE_EXPORT const MsEngineAbi* MsEngine_GetAbi(void) {
    static const MsEngineAbi sAbi = {
        sizeof(MsEngineAbi), MS_ENGINE_API_VERSION, MS_ENGINE_2SHIP, "2 Ship 2 Harkinian", LUS_ABI_ID,
    };
    return &sAbi;
}

MS_ENGINE_EXPORT int32_t MsEngine_Init(const MsHostServices* host, int32_t argc, char** argv) {
    sHost = host;
    MergeShipHost_SetServices(host);
    MergeShipSettings_SetServices(host);
    MergeShipAnchor_SetHostServices(host);
    Ship::Gui::AddImGuiContextListener(SelectImGuiContext);

    if (host != nullptr && host->shareContext != nullptr) {
        // Another game owns the window: borrow it (and audio, controllers, log) and bring up only our own
        // config, CVars and resources. OTRGlobals' constructor then finds this Context already active.
        Ship::Context::CreateSharedInstance("2 Ship 2 Harkinian", appShortName, "2ship2harkinian.json",
                                            static_cast<Ship::Context*>(host->shareContext));
        sContext = Ship::Context::GetRawInstance();
        UseContextLogger();
    }

    if (ShipHosted_Init(argc, argv) != 0) {
        HostLog(2, "2 Ship 2 Harkinian failed to start (see its log)");
        return 1;
    }

    sContext = Ship::Context::GetRawInstance();
    UseContextLogger();
    // The settings the player set in SoH (MergeShipSettingsSync.h), when SoH started first; otherwise on the first resume.
    MergeShipSettings_Pull("start");
    ApplyMute();
    sActive = true;
    MergeShipAnchor_OnEngineReady();
    // BUG-0096: this menu, for Hyrule's ("Termina Settings").
    if ((host != nullptr) && MS_HOST_HAS(host, GetMenuMirror) && (host->SetMenuMirror != nullptr)) {
        host->SetMenuMirror(host->host, MS_ENGINE_2SHIP, BenGui::MergeShip_BuildMenuMirror());
    }
    MergeShipHostTest_Init(); // MERGESHIP_TEST_SCRIPT_2SHIP, if set
    SPDLOG_INFO("MergeShip2: 2 Ship 2 Harkinian started in the host (libultraship {})", LUS_ABI_ID);
    return 0;
}

MS_ENGINE_EXPORT int32_t MsEngine_RunFrame(void) {
    // An arrival from Hyrule replaces the game state before the frame (MergeShipHost.cpp); then the test script.
    MergeShipHost_BeforeFrame();
    MergeShipHostTest_Frame();
    const int32_t running = Graph_RunHostedFrame();
    if (sInputBlocked) {
        sContext->GetControlDeck()->UnblockGameInput(kResumeInputBlockId);
        sInputBlocked = false;
    }
    if (!running) {
        return MS_FRAME_EXITED;
    }
    // The Clock Tower's passage asked the host for a transfer during this frame.
    return MergeShipHost_TakeTransferRequested() ? MS_FRAME_TRANSFER : MS_FRAME_RUNNING;
}

MS_ENGINE_EXPORT void MsEngine_Suspend(void) {
    if (!sActive) {
        return;
    }
    MergeShipAnchor_OnEngineSuspend();
    // 2Ship's audio thread only mixes when a frame signals it, so stopping frames parks it.
    if (sInputBlocked) {
        sContext->GetControlDeck()->UnblockGameInput(kResumeInputBlockId);
        sInputBlocked = false;
    }
    // Shared settings the player changed in 2Ship's menu go back to SoH (MergeShipSettingsSync.h).
    MergeShipSettings_PushBack();
    sContext->GetConsoleVariables()->Save();

    auto window = sContext->GetWindow();
    auto gui = window->GetGui();
    gui->SwapEngineWindows(sGui);
    gui->SwapMenu(sGui);
    window->SetMouseCapture(false);
    // Texture caches are keyed by resource name, and both games name thousands of resources alike.
    gfx_texture_cache_clear();
    sActive = false;
}

MS_ENGINE_EXPORT void MsEngine_Resume(const MsTransfer* arrival) {
    if (sActive) {
        // The host turned a transfer down and resumed the requester: release the door.
        MergeShipHost_OnResume(arrival);
        return;
    }
    Ship::Context::SetActiveInstance(sContext);
    ImGui::SetCurrentContext(Ship::Gui::GetImGuiContext());
    UseContextLogger();

    auto window = sContext->GetWindow();
    auto gui = window->GetGui();
    gui->SwapEngineWindows(sGui);
    gui->SwapMenu(sGui);
    gfx_texture_cache_clear();
    // SoH's settings as they are now (the player may have changed them in Hyrule): MergeShipSettingsSync.h.
    MergeShipSettings_Pull("resume");
    // BUG-0096: options Hyrule's menu changed here ("Termina Settings") run what this menu would have (ShipInit).
    MsMenuMirror_ApplyDirty(sContext->GetConsoleVariables().get(), [](const std::string& name) { ShipInit::Init(name); });

    window->SetAutoCaptureMouse(CVarGetInteger("gSettings.EnableMouse", 0) &&
                                CVarGetInteger("gSettings.AutoCaptureMouse", 1));
    window->SetForceCursorVisibility(CVarGetInteger("gSettings.CursorVisibility", 0));
    if (auto audio = sContext->GetAudio()) {
        if (auto player = audio->GetAudioPlayer()) {
            player->SetDesiredBuffered(kDesiredBuffered);
        }
    }
    CrashHandlerRegisterCallback(CrashHandler_PrintExt);
    ApplyMute();

    // The key that switched games is still down; don't let the game see it.
    if (auto deck = sContext->GetControlDeck()) {
        deck->BlockGameInput(kResumeInputBlockId);
        sInputBlocked = true;
    }
    sActive = true;
    MergeShipAnchor_OnEngineResume();
    // Arriving from Hyrule (or a manual switch while the player was away): MergeShipHost.cpp.
    MergeShipHost_OnResume(arrival);
}

MS_ENGINE_EXPORT void MsEngine_Shutdown(void) {
    if (sContext == nullptr) {
        return;
    }
    Ship::Context* previous = Ship::Context::GetRawInstance();
    Ship::Context::SetActiveInstance(sContext);
    sContext->GetConsoleVariables()->Save();
    if (!sContext->IsSharedInstance() && sContext->GetWindow() != nullptr) {
        sContext->GetWindow()->SaveWindowToConfig();
    }
    if (sContext->GetLogger() != nullptr) {
        sContext->GetLogger()->flush();
    }
    // Deliberately not DeinitOTR: the window, ImGui and audio are shared with the other game, and the
    // host ends the process once every engine has saved.
    Ship::Context::SetActiveInstance(previous);
}

MS_ENGINE_EXPORT void* MsEngine_GetContext(void) {
    return sContext;
}

// MsHostServices.ExtractRom (EngineApi.h): builds mm.o2r from a Majora's Mask ROM with 2Ship's own extractor, exactly
// as 2Ship's first-run flow does (BenPort.cpp, ES_EXTRACT_ARGS), but callable before MsEngine_Init. On a first run of
// mergeship.exe, SoH (the first engine) asks for the ROM and calls this, so that its own init already finds the
// namespaced mm.o2r both games read, and 2Ship's init finds it too instead of asking again. The host logs the result
// (sHost is not set yet, so this logs through spdlog only).
MS_ENGINE_EXPORT int32_t MsEngine_ExtractRom(const char* romPath, const char* destDir,
                                             void (*progress)(void* user, uint64_t done, uint64_t total), void* user) {
    namespace fs = std::filesystem;
    if ((romPath == nullptr) || (destDir == nullptr)) {
        return 1;
    }
    auto extract = std::make_unique<Extractor>();
    if (!extract->RunFileStandalone(romPath)) {
        SPDLOG_ERROR("2Ship: {} is not a supported Majora's Mask ROM (US 1.0 N64 or US GameCube, .z64/.n64/.v64)",
                     romPath);
        return 2;
    }
    std::error_code ec;
    // The host's assets/ folder also holds SoH's extraction files (about 100 MB); CallZapd copies whatever folder it
    // is given, so give it one with only 2Ship's (xml/, filelists/, symbols/, Config_*.xml, EnumData.xml).
    const fs::path bundleAssets = fs::absolute(Ship::Context::GetAppBundlePath()) / "assets";
    const fs::path install = extract->Mkdtemp();
    try {
        fs::create_directories(install / "assets");
        for (const auto& entry : fs::directory_iterator(bundleAssets)) {
            const std::string name = entry.path().filename().string();
            if ((name == "xml") || (name == "filelists") || (name == "symbols") || (name == "EnumData.xml") ||
                name.starts_with("Config_")) {
                fs::copy(entry.path(), install / "assets" / name, fs::copy_options::recursive);
            }
        }
    } catch (const std::exception& e) {
        SPDLOG_ERROR("2Ship: cannot prepare the MM extractor's files from {}: {}", bundleAssets.string(), e.what());
        fs::remove_all(install, ec);
        return 3;
    }
    const fs::path out = fs::path(destDir) / "mm.o2r";
    // CallZapd works in a temporary folder by changing the process's current directory; put it back whatever happens.
    const fs::path previousDirectory = fs::current_path(ec);
    std::atomic<size_t> count = 0;
    std::atomic<size_t> total = 0;
    std::atomic<bool> finished = false;
    bool threw = false;
    std::thread worker([&]() {
        try {
            extract->CallZapd(install.string(), destDir, &count, &total);
        } catch (const std::exception& e) {
            threw = true;
            SPDLOG_ERROR("2Ship: MM extraction failed: {}", e.what());
        } catch (...) {
            threw = true;
            SPDLOG_ERROR("2Ship: MM extraction failed");
        }
        finished = true;
    });
    while (!finished) {
        if (progress != nullptr) {
            progress(user, count, total);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    worker.join();
    if (progress != nullptr) {
        progress(user, count, total);
    }
    fs::current_path(previousDirectory, ec);
    fs::remove_all(install, ec);
    return (!threw && fs::exists(out, ec)) ? 0 : 4;
}

// MergeShip2: how game code asks the host for an engine transfer (MergeShipHyrule.cpp's
// MergeShip_RequestHyruleWarp). Not an export: 2ship.dll's own code calls it. Returns 0 when the host offers
// no RequestTransfer; the host acts on the request after the current frame (MsEngine_RunFrame).
int32_t MergeShip_Host_RequestTransfer(const MsTransfer* transfer) {
    if ((sHost == nullptr) || (sHost->RequestTransfer == nullptr) || (transfer == nullptr)) {
        return 0;
    }
    sHost->RequestTransfer(sHost->host, transfer);
    return 1;
}

} // extern "C"

#endif // MERGESHIP_HOSTED
