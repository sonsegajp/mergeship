// MergeShip2: Ganondorf mode's password on the file select (MergeShipGanondorf.h): "demonking" typed on the keyboard
// turns the mode on or off; the file select's music follows (Ganondorf's theme while it is on).

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <string>
#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipGanondorf.h"
#include "seqcmd.h"
}

namespace {

constexpr const char* kPassword = "demonking";
std::string sTyped;
// After the password: the secret jingle, then (this many game frames on) the file select's music.
int sJingleFrames;

// Presses are found from the keys' down state, sampled each game frame: ImGui's own "pressed" lasts one ImGui frame,
// and with frame interpolation (several drawn frames per game frame) most of those fall between game updates.
bool sWasDown[36];

ImGuiKey KeyOf(int k) {
    return (k < 26) ? (ImGuiKey)(ImGuiKey_A + k) : (ImGuiKey)(ImGuiKey_0 + (k - 26));
}

char CharOf(int k) {
    return (k < 26) ? (char)('a' + k) : (char)('0' + (k - 26));
}

std::string NewPresses() {
    std::string out;
    for (int k = 0; k < 36; k++) {
        bool down = ImGui::IsKeyDown(KeyOf(k)) || ((k >= 26) && ImGui::IsKeyDown((ImGuiKey)(ImGuiKey_Keypad0 + (k - 26))));
        if (down && !sWasDown[k]) {
            out += CharOf(k);
        }
        sWasDown[k] = down;
    }
    return out;
}

} // namespace

extern "C" void MergeShip_Ganondorf_FileSelectUpdate(void) {
    if ((sJingleFrames > 0) && (--sJingleFrames == 0)) {
        // the jingle over: Ganondorf's theme (or, the mode off, the file select's own)
        Audio_PlaySequenceWithSeqPlayerIO(SEQ_PLAYER_BGM_MAIN, MergeShip_Ganondorf_FileSelectBgm(), 0, 7, 1);
    }
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    // Typing into one of the menus' text boxes is not the password.
    if (ImGui::GetIO().WantTextInput) {
        sTyped.clear();
        return;
    }
    const std::string pressed = NewPresses();
    if (pressed.empty()) {
        return;
    }
    sTyped += pressed;
    const size_t n = std::string(kPassword).size();
    if (sTyped.size() > n) {
        sTyped.erase(0, sTyped.size() - n);
    }
    if (sTyped != kPassword) {
        return;
    }
    sTyped.clear();
    CVarSetInteger(MERGESHIP_GANONDORF_CVAR, MergeShip_Ganondorf_Enabled() ? 0 : 1);
    CVarSave();
    SPDLOG_INFO("[MergeShip] Ganondorf mode {}", MergeShip_Ganondorf_Enabled() ? "on" : "off");
    // The secret jingle (a puzzle solved), the music stopped under it; then the file select's music, Ganondorf's theme
    // while the mode is on (the user, 2026-10-16).
    SEQCMD_STOP_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0);
    Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    sJingleFrames = 45;
}
