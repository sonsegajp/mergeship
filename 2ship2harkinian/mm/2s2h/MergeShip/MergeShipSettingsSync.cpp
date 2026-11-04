// MergeShip2 -- settings shared from Ship of Harkinian: see MergeShipSettingsSync.h and include/mergeship/SettingsSync.h
// (the table: include/mergeship/SettingsMap.def).
//
// SoH's CVars live in SoH's own Ship::Context (shipofharkinian.json); 2Ship's in its own (2ship2harkinian.json). Both
// are libultraship.dll's ConsoleVariable objects, so 2Ship reads and writes SoH's directly. Only Get/Set are used on
// SoH's: ClearVariable, Save and Load work on the *active* context's config file, which here is 2Ship's.
//
// 2ship.exe (MERGESHIP_HOSTED off) gets the stubs at the end.

#include "2s2h/MergeShip/MergeShipSettingsSync.h"

#ifdef MERGESHIP_HOSTED

#include <mergeship/EngineApi.h>
#include <mergeship/SettingsSync.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <ship/Context.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/window/Window.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipHost.h"

extern "C" {
#include "z64.h"
#include "functions.h"
}

namespace {

const MsHostServices* sHost = nullptr;

// A CVar's value as the sync compares it: unset, a number, or a colour.
struct Value {
    bool set = false;
    bool color = false;
    bool color24 = false;
    double num = 0.0;
    Color_RGBA8 rgba = {};
    Color_RGB8 rgb = {};
};

// Each row's 2Ship value right after the last pull: a value that differs at suspend was changed in 2Ship.
struct Seen {
    bool valid = false;
    Value value;
};
Seen sSeen[MS_SETTINGS_COUNT];

// Each row's SoH value at the last pull that applied it. A one-way row (MS_F_TO_2SHIP) is only applied again when SoH's
// value has changed since: the player's own change in 2Ship's menu (which cannot go back to SoH) is not undone by the
// next arrival in Termina. Last writer wins, as for the two-way rows.
Seen sSohApplied[MS_SETTINGS_COUNT];

void HostLog(int32_t level, const char* message) {
    if ((sHost != nullptr) && (sHost->Log != nullptr)) {
        sHost->Log(sHost->host, level, message);
    }
}

Ship::Context* SohContext() {
    if (sHost == nullptr) {
        return nullptr;
    }
    if (MS_HOST_HAS(sHost, GetEngineContext) && (sHost->GetEngineContext != nullptr)) {
        return static_cast<Ship::Context*>(sHost->GetEngineContext(sHost->host, MS_ENGINE_SOH));
    }
    // An older host: when SoH is the first engine, the context 2Ship borrows the window from is SoH's.
    return static_cast<Ship::Context*>(sHost->shareContext);
}

bool Muted() {
    return (sHost != nullptr) && (sHost->flags & MS_HOST_FLAG_MUTED);
}

// The automated tests' muting must not be shared (nor written back into SoH's config).
bool IsMasterVolume(const MsSettingMapping& m) {
    return strcmp(m.ship, "gSettings.Audio.MasterVolume") == 0;
}

Value Read(Ship::ConsoleVariable* cvars, const char* name, MsSettingType type, double def) {
    Value v;
    v.num = def;
    std::shared_ptr<Ship::CVar> var = cvars->Get(name);
    if (var == nullptr) {
        return v;
    }
    switch (var->Type) {
        case Ship::ConsoleVariableType::Integer:
            v.set = (type != MS_T_COLOR);
            v.num = var->Integer;
            break;
        case Ship::ConsoleVariableType::Float:
            v.set = (type != MS_T_COLOR);
            v.num = var->Float;
            break;
        case Ship::ConsoleVariableType::Color:
            v.set = v.color = (type == MS_T_COLOR);
            v.rgba = var->Color;
            break;
        case Ship::ConsoleVariableType::Color24:
            v.set = v.color = v.color24 = (type == MS_T_COLOR);
            v.rgb = var->Color24;
            break;
        default:
            break;
    }
    if (!v.set) {
        v.num = def; // a CVar of another type reads as absent, as CVarGetInteger/Float do
    }
    return v;
}

bool Same(const Value& a, const Value& b, MsSettingType type) {
    if (type == MS_T_COLOR) {
        if (a.set != b.set || a.color24 != b.color24) {
            return false;
        }
        if (!a.set) {
            return true;
        }
        return a.color24 ? (memcmp(&a.rgb, &b.rgb, sizeof(a.rgb)) == 0) : (memcmp(&a.rgba, &b.rgba, sizeof(a.rgba)) == 0);
    }
    return std::fabs(a.num - b.num) < 1e-4;
}

void Write(Ship::ConsoleVariable* cvars, const char* name, MsSettingType type, const Value& v) {
    if (type == MS_T_COLOR) {
        if (v.color24) {
            cvars->SetColor24(name, v.rgb);
        } else {
            cvars->SetColor(name, v.rgba);
        }
    } else if (type == MS_T_FLOAT) {
        cvars->SetFloat(name, (float)v.num);
    } else {
        cvars->SetInteger(name, (int32_t)std::lround(v.num));
    }
}

std::string Describe(const Value& v, MsSettingType type) {
    char text[64];
    if (!v.set) {
        snprintf(text, sizeof(text), (type == MS_T_COLOR) ? "unset" : "unset (%g)", v.num);
    } else if (type == MS_T_COLOR) {
        if (v.color24) {
            snprintf(text, sizeof(text), "rgb(%d,%d,%d)", v.rgb.r, v.rgb.g, v.rgb.b);
        } else {
            snprintf(text, sizeof(text), "rgba(%d,%d,%d,%d)", v.rgba.r, v.rgba.g, v.rgba.b, v.rgba.a);
        }
    } else {
        snprintf(text, sizeof(text), "%g", v.num);
    }
    return text;
}

void Remember(size_t i, Ship::ConsoleVariable* shipCvars) {
    const MsSettingMapping& m = kMsSettingsMap[i];
    sSeen[i].valid = true;
    sSeen[i].value = Read(shipCvars, m.ship, m.shipType, m.shipDefault);
}

// What 2Ship's menu does besides setting the CVar (BenMenu.cpp's widget callbacks); ShipInit covers the rest.
void ApplySpecial(const std::string& name) {
    Ship::Context* context = Ship::Context::GetRawInstance();
    auto window = (context != nullptr) ? context->GetWindow() : nullptr;
    if (name == CVAR_INTERNAL_RESOLUTION) {
        if (window != nullptr) {
            window->SetResolutionMultiplier(CVarGetFloat(CVAR_INTERNAL_RESOLUTION, 1));
        }
    } else if (name == CVAR_MSAA_VALUE) {
        if (window != nullptr) {
            window->SetMsaaLevel(CVarGetInteger(CVAR_MSAA_VALUE, 1));
        }
    } else if (name == "gSettings.CursorVisibility") {
        if (window != nullptr) {
            window->SetForceCursorVisibility(CVarGetInteger("gSettings.CursorVisibility", 0));
        }
    } else if (name == "gSettings.Audio.MainMusicVolume") {
        AudioSeq_SetPortVolumeScale(SEQ_PLAYER_BGM_MAIN, CVarGetFloat("gSettings.Audio.MainMusicVolume", 1.0f));
    } else if (name == "gSettings.Audio.SubMusicVolume") {
        AudioSeq_SetPortVolumeScale(SEQ_PLAYER_BGM_SUB, CVarGetFloat("gSettings.Audio.SubMusicVolume", 1.0f));
    } else if (name == "gSettings.Audio.SoundEffectsVolume") {
        AudioSeq_SetPortVolumeScale(SEQ_PLAYER_SFX, CVarGetFloat("gSettings.Audio.SoundEffectsVolume", 1.0f));
    } else if (name == "gSettings.Audio.FanfareVolume") {
        AudioSeq_SetPortVolumeScale(SEQ_PLAYER_FANFARE, CVarGetFloat("gSettings.Audio.FanfareVolume", 1.0f));
    }
}

bool ShareOn(Ship::ConsoleVariable* sohCvars) {
    return sohCvars->GetInteger(MS_CVAR_SHARE_SETTINGS, 1) != 0;
}

} // namespace

void MergeShipSettings_SetServices(const MsHostServices* services) {
    sHost = services;
}

bool MergeShipSettings_Active() {
    Ship::Context* soh = SohContext();
    Ship::Context* ship = Ship::Context::GetRawInstance();
    return (soh != nullptr) && (ship != nullptr) && (soh != ship) && (soh->GetConsoleVariables() != nullptr) &&
           ShareOn(soh->GetConsoleVariables().get());
}

int MergeShipSettings_Pull(const char* why) {
    Ship::Context* soh = SohContext();
    Ship::Context* ship = Ship::Context::GetRawInstance();
    if ((soh == nullptr) || (ship == nullptr) || (soh == ship) || (soh->GetConsoleVariables() == nullptr) ||
        (ship->GetConsoleVariables() == nullptr)) {
        return 0; // not hosted with SoH, or SoH is not up yet (it starts after 2Ship with --engine 2ship)
    }
    Ship::ConsoleVariable* sohCvars = soh->GetConsoleVariables().get();
    Ship::ConsoleVariable* shipCvars = ship->GetConsoleVariables().get();
    if (!ShareOn(sohCvars)) {
        for (Seen& seen : sSeen) {
            seen.valid = false; // nothing goes back either
        }
        for (Seen& applied : sSohApplied) {
            applied.valid = false; // sharing back on: SoH's values apply again
        }
        MergeShipHostTest_Log("SETTINGS %s: not shared (SoH's \"Share Settings with Termina\" is off)", why);
        return 0;
    }
    const bool formTunics = sohCvars->GetInteger(MS_SETTINGS_FORM_TUNIC_GATE, 1) != 0;
    const bool deityTunic =
        sohCvars->GetInteger(MS_SETTINGS_FORM_TUNIC_GATE, MS_SETTINGS_FORM_TUNIC_ALL) == MS_SETTINGS_FORM_TUNIC_ALL;

    std::vector<std::string> changed;
    for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
        const MsSettingMapping& m = kMsSettingsMap[i];
        if (Muted() && IsMasterVolume(m)) {
            sSeen[i].valid = false;
            continue;
        }
        const Value sohValue = Read(sohCvars, m.soh, m.sohType, m.sohDefault);
        const Value current = Read(shipCvars, m.ship, m.shipType, m.shipDefault);
        if (((m.xform == MS_X_FORM_TUNIC) && !formTunics) || (!sohValue.set && (m.flags & MS_F_IF_SET))) {
            Remember(i, shipCvars);
            continue;
        }
        if ((m.xform == MS_X_FD_TUNIC) && !deityTunic) {
            // BUG-0123: SoH leaves the Fierce Deity in Majora's Mask's colours ("Form Tunic Colours" off, or the Deku,
            // Goron and Zora only): so does 2Ship -- his tunic unchanged, no rainbow, whatever SoH's colour holds
            if ((m.shipType != MS_T_COLOR) && current.set && (current.num != 0.0)) {
                shipCvars->ClearVariable(m.ship);
                changed.push_back(m.ship);
                MergeShipHostTest_Log("SETTINGS %s = unset (SoH's Fierce Deity keeps MM's colours)", m.ship);
            }
            Remember(i, shipCvars);
            continue;
        }
        if ((m.flags & MS_F_TO_2SHIP) && sSohApplied[i].valid && Same(sSohApplied[i].value, sohValue, m.sohType)) {
            Remember(i, shipCvars); // SoH's option unchanged since it was applied: 2Ship's own value stands
            continue;
        }
        sSohApplied[i].valid = true;
        sSohApplied[i].value = sohValue;
        Value target = sohValue;
        if (m.sohType != MS_T_COLOR) {
            target.num = MsSetting_ToShip(&m, sohValue.num);
        }
        if (!sohValue.set && ((m.flags & MS_F_UNSET) || (m.sohType == MS_T_COLOR))) {
            // Unset in SoH: unset in 2Ship too (the same code default). 2Ship is the active context: its config.
            if (current.set) {
                shipCvars->ClearVariable(m.ship);
                changed.push_back(m.ship);
                MergeShipHostTest_Log("SETTINGS %s = unset (from SoH %s)", m.ship, m.soh);
            }
        } else {
            // A number is compared as the game reads it (unset = its default): an untouched 2Ship CVar that already
            // reads as SoH's value stays unset.
            target.set = true;
            if (!Same(current, target, m.shipType)) {
                Write(shipCvars, m.ship, m.shipType, target);
                changed.push_back(m.ship);
                MergeShipHostTest_Log("SETTINGS %s = %s (from SoH %s = %s)", m.ship, Describe(target, m.shipType).c_str(),
                                      m.soh, Describe(sohValue, m.sohType).c_str());
            }
        }
        Remember(i, shipCvars);
    }

    // As the menu does (UIWidgets.cpp): what registered for a CVar re-registers with its new value.
    std::vector<std::string> done;
    for (const std::string& name : changed) {
        bool seen = false;
        for (const std::string& d : done) {
            seen = seen || (d == name);
        }
        if (seen) {
            continue;
        }
        done.push_back(name);
        ShipInit::Init(name);
        ApplySpecial(name);
    }

    char line[160];
    snprintf(line, sizeof(line), "2Ship settings from SoH (%s): %zu of %zu shared settings changed", why, done.size(),
             (size_t)MS_SETTINGS_COUNT);
    MergeShipHostTest_Log("SETTINGS %s: %zu changed", why, done.size());
    SPDLOG_INFO("MergeShip2: {}", line);
    if (!done.empty()) {
        HostLog(0, line);
    }
    return (int)done.size();
}

int MergeShipSettings_PushBack() {
    Ship::Context* soh = SohContext();
    Ship::Context* ship = Ship::Context::GetRawInstance();
    if ((soh == nullptr) || (ship == nullptr) || (soh == ship) || (soh->GetConsoleVariables() == nullptr) ||
        (ship->GetConsoleVariables() == nullptr)) {
        return 0;
    }
    Ship::ConsoleVariable* sohCvars = soh->GetConsoleVariables().get();
    Ship::ConsoleVariable* shipCvars = ship->GetConsoleVariables().get();
    if (!ShareOn(sohCvars)) {
        return 0;
    }
    int written = 0;
    for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
        const MsSettingMapping& m = kMsSettingsMap[i];
        if (!sSeen[i].valid || (m.flags & MS_F_TO_2SHIP) || (Muted() && IsMasterVolume(m))) {
            continue;
        }
        const Value now = Read(shipCvars, m.ship, m.shipType, m.shipDefault);
        if (Same(now, sSeen[i].value, m.shipType)) {
            continue;
        }
        sSeen[i].value = now;
        if (m.shipType == MS_T_COLOR && !now.set) {
            continue; // reset in 2Ship's editor: SoH's config cannot be cleared from here; its "Changed" row says it
        }
        Value sohValue = now;
        if (m.sohType != MS_T_COLOR) {
            sohValue.num = MsSetting_ToSoh(&m, now.num);
        }
        Write(sohCvars, m.soh, m.sohType, sohValue);
        written++;
        MergeShipHostTest_Log("SETTINGS back to SoH: %s = %s (2Ship %s = %s)", m.soh,
                              Describe(sohValue, m.sohType).c_str(), m.ship, Describe(now, m.shipType).c_str());
    }
    if (written > 0) {
        // SoH saves its config when it is suspended or shut down (with its own context active).
        char line[128];
        snprintf(line, sizeof(line), "2Ship settings back to SoH: %d changed in 2Ship's menu", written);
        HostLog(0, line);
        SPDLOG_INFO("MergeShip2: {}", line);
    }
    return written;
}

void MergeShipSettings_LogAll() {
    Ship::Context* soh = SohContext();
    Ship::Context* ship = Ship::Context::GetRawInstance();
    if ((soh == nullptr) || (ship == nullptr) || (soh == ship)) {
        MergeShipHostTest_Log("SETTINGS table: no SoH context");
        return;
    }
    Ship::ConsoleVariable* sohCvars = soh->GetConsoleVariables().get();
    Ship::ConsoleVariable* shipCvars = ship->GetConsoleVariables().get();
    MergeShipHostTest_Log("SETTINGS table (%zu rows, sharing %s):", (size_t)MS_SETTINGS_COUNT,
                          ShareOn(sohCvars) ? "on" : "off");
    for (size_t i = 0; i < MS_SETTINGS_COUNT; i++) {
        const MsSettingMapping& m = kMsSettingsMap[i];
        const Value s = Read(sohCvars, m.soh, m.sohType, m.sohDefault);
        const Value h = Read(shipCvars, m.ship, m.shipType, m.shipDefault);
        MergeShipHostTest_Log("SETTING %s = %s | %s = %s", m.soh, Describe(s, m.sohType).c_str(), m.ship,
                              Describe(h, m.shipType).c_str());
    }
}

#else // !MERGESHIP_HOSTED: 2ship.exe

void MergeShipSettings_SetServices(const MsHostServices* services) {
    (void)services;
}
int MergeShipSettings_Pull(const char* why) {
    (void)why;
    return 0;
}
int MergeShipSettings_PushBack() {
    return 0;
}
bool MergeShipSettings_Active() {
    return false;
}
void MergeShipSettings_LogAll() {
}

#endif // MERGESHIP_HOSTED
