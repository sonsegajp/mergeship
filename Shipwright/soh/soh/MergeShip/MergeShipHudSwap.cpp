// MergeShip2 -- Majora's Mask's HUD textures in Hyrule (BUG-0013, include/mergeship/HudSwap.hpp): the C/B button
// backgrounds, the hearts, the rupee counter's icon and Link's head on the dungeon map, each an option of the
// Mergeship menu (SohMenuMergeShip.cpp). Termina's side (Ocarina of Time's textures there) is 2Ship's
// MergeShipOoTItems.cpp.

#include <mergeship/HudSwap.hpp>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" void MergeShipTest_Log(const char* fmt, ...);

namespace {

#define HUD_TEX(name) { "textures/parameter_static/" name, "mm/parameter_static/" name }

const MsHudSwap::Swap kButtons[] = { HUD_TEX("gButtonBackgroundTex") };
const MsHudSwap::Swap kHearts[] = {
    HUD_TEX("gHeartFullTex"),          HUD_TEX("gHeartEmptyTex"),        HUD_TEX("gHeartHalfTex"),
    HUD_TEX("gHeartQuarterTex"),       HUD_TEX("gHeartThreeQuarterTex"), HUD_TEX("gDefenseHeartFullTex"),
    HUD_TEX("gDefenseHeartEmptyTex"),  HUD_TEX("gDefenseHeartHalfTex"),  HUD_TEX("gDefenseHeartQuarterTex"),
    HUD_TEX("gDefenseHeartThreeQuarterTex"),
};
const MsHudSwap::Swap kRupees[] = { HUD_TEX("gRupeeCounterIconTex") };
const MsHudSwap::Swap kMapLink[] = {
    { "textures/icon_item_dungeon_static/gDungeonMapLinkHeadTex", "mm/icon_item_24_static_yar/gQuestIconLinkHumanFaceTex" },
};

const MsHudSwap::Group kGroups[] = {
    { kButtons, sizeof(kButtons) / sizeof(kButtons[0]) },
    { kHearts, sizeof(kHearts) / sizeof(kHearts[0]) },
    { kRupees, sizeof(kRupees) / sizeof(kRupees[0]) },
    { kMapLink, sizeof(kMapLink) / sizeof(kMapLink[0]) },
};
const char* const kCVars[] = {
    CVAR_ENHANCEMENT("MergeShip.MMHud.Buttons"),
    CVAR_ENHANCEMENT("MergeShip.MMHud.Hearts"),
    CVAR_ENHANCEMENT("MergeShip.MMHud.Rupees"),
    CVAR_ENHANCEMENT("MergeShip.MMHud.MapLink"),
};

std::shared_ptr<MsAdultMask::MemoryArchive> sMounted = nullptr;
uint32_t sAppliedMask = 0;

uint32_t WantedMask() {
    uint32_t mask = 0;
    for (size_t i = 0; i < sizeof(kCVars) / sizeof(kCVars[0]); i++) {
        if (CVarGetInteger(kCVars[i], 0)) {
            mask |= 1u << i;
        }
    }
    return mask;
}

void RegisterHudSwap() {
    COND_HOOK(OnGameFrameUpdate, true, []() {
        const uint32_t mask = WantedMask();
        if (mask == sAppliedMask) {
            return;
        }
        sMounted = MsHudSwap::Apply(kGroups, sizeof(kGroups) / sizeof(kGroups[0]), mask, "mm.o2r", "soh", sMounted,
                                    "mergeship-hud (Majora's Mask)");
        sAppliedMask = mask;
        MergeShipTest_Log("HUDSWAP Majora's Mask textures mask=0x%X mounted=%d", mask, (sMounted != nullptr) ? 1 : 0);
    });
}

RegisterShipInitFunc sInit(RegisterHudSwap, {});

} // namespace
