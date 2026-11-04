// MergeShip2 -- Ship of Harkinian's side of settings sharing: see MergeShipSettingsSync.h. The table is
// include/mergeship/SettingsMap.def; 2Ship does the copying (2s2h/MergeShip/MergeShipSettingsSync.cpp).

#include "soh/MergeShip/MergeShipSettingsSync.h"

#ifdef MERGESHIP_HOSTED

#include <mergeship/SettingsSync.h>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/window/Window.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "z64.h"
#include "include/z64audio.h"
void MergeShipTest_Log(const char* fmt, ...); // MergeShipTest.cpp (a no-op without a test script)
}

namespace {

// Each shared SoH CVar as SoH left it when it was suspended (raw: unset / number / colour).
struct Snapshot {
    bool set = false;
    Ship::ConsoleVariableType type = Ship::ConsoleVariableType::Integer;
    int32_t i = 0;
    float f = 0.0f;
    Color_RGBA8 rgba = {};
    Color_RGB8 rgb = {};
};
bool sHaveSnapshot = false;
Snapshot sSnapshot[MS_SETTINGS_COUNT];

Snapshot Take(Ship::ConsoleVariable* cvars, const char* name) {
    Snapshot s;
    std::shared_ptr<Ship::CVar> var = cvars->Get(name);
    if (var == nullptr) {
        return s;
    }
    s.set = true;
    s.type = var->Type;
    switch (var->Type) {
        case Ship::ConsoleVariableType::Integer:
            s.i = var->Integer;
            break;
        case Ship::ConsoleVariableType::Float:
            s.f = var->Float;
            break;
        case Ship::ConsoleVariableType::Color:
            s.rgba = var->Color;
            break;
        case Ship::ConsoleVariableType::Color24:
            s.rgb = var->Color24;
            break;
        default:
            s.set = false; // strings are not shared
            break;
    }
    return s;
}

bool Same(const Snapshot& a, const Snapshot& b) {
    if (a.set != b.set) {
        return false;
    }
    if (!a.set) {
        return true;
    }
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case Ship::ConsoleVariableType::Integer:
            return a.i == b.i;
        case Ship::ConsoleVariableType::Float:
            return std::fabs(a.f - b.f) < 1e-5f;
        case Ship::ConsoleVariableType::Color:
            return memcmp(&a.rgba, &b.rgba, sizeof(a.rgba)) == 0;
        case Ship::ConsoleVariableType::Color24:
            return memcmp(&a.rgb, &b.rgb, sizeof(a.rgb)) == 0;
        default:
            return true;
    }
}

// What SoH's menu does besides setting the CVar (SohMenuSettings.cpp's widget callbacks); ShipInit covers the rest.
void ApplySpecial(const std::string& name) {
    auto window = Ship::Context::GetRawInstance()->GetWindow();
    if (name == CVAR_INTERNAL_RESOLUTION) {
        if (window != nullptr) {
            window->SetResolutionMultiplier(CVarGetFloat(CVAR_INTERNAL_RESOLUTION, 1));
        }
    } else if (name == CVAR_MSAA_VALUE) {
        if (window != nullptr) {
            window->SetMsaaLevel(CVarGetInteger(CVAR_MSAA_VALUE, 1));
        }
    } else if (name == CVAR_SETTING("Volume.MainMusic")) {
        Audio_SetGameVolume(SEQ_PLAYER_BGM_MAIN, (float)CVarGetInteger(CVAR_SETTING("Volume.MainMusic"), 100) / 100.0f);
    } else if (name == CVAR_SETTING("Volume.SubMusic")) {
        Audio_SetGameVolume(SEQ_PLAYER_BGM_SUB, (float)CVarGetInteger(CVAR_SETTING("Volume.SubMusic"), 100) / 100.0f);
    } else if (name == CVAR_SETTING("Volume.Fanfare")) {
        Audio_SetGameVolume(SEQ_PLAYER_FANFARE, (float)CVarGetInteger(CVAR_SETTING("Volume.Fanfare"), 100) / 100.0f);
    } else if (name == CVAR_SETTING("Volume.SFX")) {
        Audio_SetGameVolume(SEQ_PLAYER_SFX, (float)CVarGetInteger(CVAR_SETTING("Volume.SFX"), 100) / 100.0f);
    }
}

} // namespace

void MergeShipSettings_OnSuspend() {
    auto cvars = Ship::Context::GetRawInstance()->GetConsoleVariables();
    if (cvars == nullptr) {
        return;
    }
    for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
        sSnapshot[i] = Take(cvars.get(), kMsSettingsMap[i].soh);
    }
    sHaveSnapshot = true;
}

void MergeShipSettings_OnResume() {
    auto cvars = Ship::Context::GetRawInstance()->GetConsoleVariables();
    if (!sHaveSnapshot || (cvars == nullptr)) {
        return;
    }
    // Several rows can share a SoH CVar (one SoH option, two 2Ship ones): each changed name once.
    std::vector<std::string> changed;
    for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
        const char* name = kMsSettingsMap[i].soh;
        if (Same(sSnapshot[i], Take(cvars.get(), name))) {
            continue;
        }
        bool listed = false;
        for (const std::string& c : changed) {
            listed = listed || (c == name);
        }
        if (!listed) {
            changed.push_back(name);
        }
    }
    for (const std::string& name : changed) {
        // As the menu does (UIWidgets.cpp): hooks registered for this CVar re-register with its new value.
        ShipInit::Init(name);
        ApplySpecial(name);
        MergeShipTest_Log("SETTINGS from 2Ship: %s int=%d float=%g", name.c_str(), CVarGetInteger(name.c_str(), -1),
                          CVarGetFloat(name.c_str(), -1.0f));
    }
    if (!changed.empty()) {
        SPDLOG_INFO("MergeShip2: {} shared setting(s) changed in 2 Ship 2 Harkinian's menu", changed.size());
    }
}

#else // !MERGESHIP_HOSTED: soh.exe

void MergeShipSettings_OnSuspend() {
}
void MergeShipSettings_OnResume() {
}

#endif // MERGESHIP_HOSTED
