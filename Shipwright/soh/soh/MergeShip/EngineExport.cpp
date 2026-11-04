// MergeShip2 host engine exports for Ship of Harkinian (soh.dll).
//
// mergeship.exe loads soh.dll and 2ship.dll, which share one libultraship.dll, one window and one audio
// device, and switches between them. This file is the whole of SoH's side of that contract
// (include/mergeship/EngineApi.h); it is compiled only when MERGESHIP_HOSTED is ON.
// See mergeship2/docs/HOST_DESIGN.md.
#ifdef MERGESHIP_HOSTED

#define MS_ENGINE_BUILDING
#include <mergeship/EngineApi.h>

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

#include "soh/OTRGlobals.h"
#include "soh/CrashHandlerExt.h"
#include "soh/MergeShip/MergeShipHost.h"
#include "soh/MergeShip/MergeShipSettingsSync.h"
#include "soh/ShipInit.hpp"
#include <mergeship/MenuMirror.hpp> // BUG-0096: each menu shows the other game's settings

#ifndef LUS_ABI_ID
#error "LUS_ABI_ID must be defined for this file (see soh/CMakeLists.txt, MERGESHIP_HOSTED)"
#endif

extern "C" {
int SohHosted_Init(int argc, char* argv[]); // soh/src/code/main.c
void SohHosted_Shutdown(void);              // soh/src/code/main.c
int32_t Graph_RunHostedFrame(void);         // soh/src/code/graph.c
void OTRAudio_Exit();                       // OTRGlobals.cpp
void SaveManager_ThreadPoolWait();          // OTRGlobals.cpp
void gfx_texture_cache_clear();             // libultraship
}
void OTRAudio_Init(); // OTRGlobals.cpp
// soh/Network/Anchor/MergeShipAnchorHost.cpp: Anchor on the host's one connection, shared with Termina
extern "C" {
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
constexpr int32_t kDesiredBuffered = 4096;      // OTRGlobals::Initialize's InitAudio

const MsHostServices* sHost = nullptr;
Ship::Context* sContext = nullptr;
Ship::GuiEngineState sGui;
bool sActive = false;
bool sAudioRunning = false;
bool sInputBlocked = false;

void HostLog(int32_t level, const char* message) {
    if (sHost != nullptr && sHost->Log != nullptr) {
        sHost->Log(sHost->host, level, message);
    }
}

// soh.dll links its own copy of ImGui; it must draw into the context libultraship.dll created.
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
        CVarSetInteger("gSettings.Volume.Master", 0);
    }
}

void BlockInputForAFrame() {
    if (auto deck = sContext->GetControlDeck()) {
        deck->BlockGameInput(kResumeInputBlockId);
        sInputBlocked = true;
    }
}
} // namespace

namespace SohGui {
const MsMenuMirror* MergeShip_BuildMenuMirror(); // SohMenuMergeShip.cpp
}

// BUG-0096 (SohMenuMergeShip.cpp's "Termina Settings"): 2Ship's menu as plain data, and its CVars.
const MsMenuMirror* MergeShip_OtherMenuMirror() {
    if ((sHost == nullptr) || !MS_HOST_HAS(sHost, GetMenuMirror) || (sHost->GetMenuMirror == nullptr)) {
        return nullptr;
    }
    return sHost->GetMenuMirror(sHost->host, MS_ENGINE_2SHIP);
}

Ship::ConsoleVariable* MergeShip_OtherConsoleVariables() {
    if ((sHost == nullptr) || !MS_HOST_HAS(sHost, GetEngineContext) || (sHost->GetEngineContext == nullptr)) {
        return nullptr;
    }
    auto* context = static_cast<Ship::Context*>(sHost->GetEngineContext(sHost->host, MS_ENGINE_2SHIP));
    return (context != nullptr) ? context->GetConsoleVariables().get() : nullptr;
}

extern "C" {

MS_ENGINE_EXPORT const MsEngineAbi* MsEngine_GetAbi(void) {
    static const MsEngineAbi sAbi = {
        sizeof(MsEngineAbi), MS_ENGINE_API_VERSION, MS_ENGINE_SOH, "Ship of Harkinian", LUS_ABI_ID,
    };
    return &sAbi;
}

MS_ENGINE_EXPORT int32_t MsEngine_Init(const MsHostServices* host, int32_t argc, char** argv) {
    sHost = host;
    MergeShipHost_SetServices(host);
    MergeShipAnchor_SetHostServices(host);
    Ship::Gui::AddImGuiContextListener(SelectImGuiContext);

    if (host != nullptr && host->shareContext != nullptr) {
        // Another game owns the window: borrow it (and audio, controllers, log) and bring up only our own
        // config, CVars and resources. OTRGlobals' constructor then finds this Context already active.
        Ship::Context::CreateSharedInstance("Ship of Harkinian", appShortName, "shipofharkinian.json",
                                            static_cast<Ship::Context*>(host->shareContext));
        sContext = Ship::Context::GetRawInstance();
        UseContextLogger();
    }

    if (SohHosted_Init(argc, argv) != 0) {
        HostLog(2, "Ship of Harkinian failed to start (see its log)");
        return 1;
    }

    sContext = Ship::Context::GetRawInstance();
    UseContextLogger();
    ApplyMute();
    sAudioRunning = true;
    sActive = true;
    MergeShipHost_RegisterHooks();
    MergeShipAnchor_OnEngineReady();
    // BUG-0096: this menu, for Termina's ("Hyrule Settings").
    if ((host != nullptr) && MS_HOST_HAS(host, GetMenuMirror) && (host->SetMenuMirror != nullptr)) {
        host->SetMenuMirror(host->host, MS_ENGINE_SOH, SohGui::MergeShip_BuildMenuMirror());
    }
    SPDLOG_INFO("MergeShip2: Ship of Harkinian started in the host (libultraship {})", LUS_ABI_ID);
    return 0;
}

MS_ENGINE_EXPORT int32_t MsEngine_RunFrame(void) {
    const int32_t running = Graph_RunHostedFrame();
    if (sInputBlocked) {
        sContext->GetControlDeck()->UnblockGameInput(kResumeInputBlockId);
        sInputBlocked = false;
    }
    if (!running) {
        return MS_FRAME_EXITED;
    }
    // The door to Termina asked the host for a transfer during this frame (MergeShipHost.cpp).
    return MergeShipHost_TakeTransferRequested() ? MS_FRAME_TRANSFER : MS_FRAME_RUNNING;
}

MS_ENGINE_EXPORT void MsEngine_Suspend(void) {
    if (!sActive) {
        return;
    }
    MergeShipAnchor_OnEngineSuspend();
    // SoH's audio thread pumps itself every 5 ms, frames or not; left running it would keep mixing OoT
    // into the shared audio device. Stop it; Resume starts a new one.
    if (sAudioRunning) {
        OTRAudio_Exit();
        sAudioRunning = false;
    }
    if (sInputBlocked) {
        sContext->GetControlDeck()->UnblockGameInput(kResumeInputBlockId);
        sInputBlocked = false;
    }
    // The shared settings as SoH leaves them: 2Ship may write some back meanwhile (MergeShipSettingsSync.h).
    MergeShipSettings_OnSuspend();
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
    // Shared settings the player changed in 2Ship's menu take effect here too (MergeShipSettingsSync.h).
    MergeShipSettings_OnResume();
    // BUG-0096: options Termina's menu changed here ("Hyrule Settings") run what this menu would have (ShipInit).
    MsMenuMirror_ApplyDirty(sContext->GetConsoleVariables().get(), [](const std::string& name) { ShipInit::Init(name); });

    window->SetAutoCaptureMouse(CVarGetInteger("gSettings.EnableMouse", 0) &&
                                CVarGetInteger("gSettings.AutoCaptureMouse", 1));
    window->SetForceCursorVisibility(CVarGetInteger("gSettings.CursorVisibility", 0));
    if (auto audio = sContext->GetAudio()) {
        if (auto player = audio->GetAudioPlayer()) {
            player->SetDesiredBuffered(kDesiredBuffered);
        }
    }
    CrashHandlerRegisterCallback(CrashHandler_PrintSohData);
    ApplyMute();

    if (!sAudioRunning) {
        OTRAudio_Init();
        sAudioRunning = true;
    }
    // The key that switched games is still down; don't let the game see it.
    BlockInputForAFrame();
    sActive = true;
    MergeShipAnchor_OnEngineResume();
    // Coming back through the door (or a manual switch while the player was away): MergeShipHost.cpp.
    MergeShipHost_OnResume(arrival);
}

MS_ENGINE_EXPORT void MsEngine_Shutdown(void) {
    if (sContext == nullptr) {
        return;
    }
    Ship::Context* previous = Ship::Context::GetRawInstance();
    Ship::Context::SetActiveInstance(sContext);
    SaveManager_ThreadPoolWait();
    if (sAudioRunning) {
        OTRAudio_Exit();
        sAudioRunning = false;
    }
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

} // extern "C"

#endif // MERGESHIP_HOSTED
