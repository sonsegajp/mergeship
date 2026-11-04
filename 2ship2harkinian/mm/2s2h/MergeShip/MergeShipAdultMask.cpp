// MergeShip2 -- the Adult Mask in Termina (MergeShipAdultMask.h): its art, mounted at startup; its icon and form rules;
// its menu option.

#include "MergeShipAdultMask.h"

#include <mergeship/AdultMask.hpp>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"
#include "2s2h/BenGui/MenuTypes.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "z64item.h"
}

static bool sArtMounted = false;
static bool sScreamMounted = false; // the mask as Link screams (MsAdultMask: MS_ADULT_MASK_SCREAM_DL)
static bool sChildMounted = false;  // the Child Mask (MsAdultMask: MS_CHILD_MASK_*)

extern "C" s32 MergeShip_AdultMask_ArtMounted(void) {
    return sArtMounted;
}

extern "C" s32 MergeShip_AdultMask_ScreamMounted(void) {
    return sArtMounted && sScreamMounted;
}

extern "C" s32 MergeShip_AdultMask_ChildMounted(void) {
    return sArtMounted && sChildMounted;
}

// BenPort.cpp, after Ocarina of Time's archive is mounted (MergeShip_MountOcarinaAssets): the mask's art from the
// player's archives (Majora's Mask's under mm/, Ocarina of Time's under oot/), its icon, and its form rules (those of the
// Fierce Deity's Mask, whose use it is).
extern "C" void MergeShip_AdultMask_Mount(void) {
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();

    sArtMounted = MsAdultMask::Mount(archives, "mm/", "oot/", &sScreamMounted, &sChildMounted);
    if (!sArtMounted) {
        return;
    }
    static const char sIcon[] = "__OTR__" MS_ADULT_MASK_ICON;
    gItemIcons[ITEM_MS_MASK_ADULT] = (TexturePtr)sIcon; // the Child Mask's while the human is the adult (PlayerUpdate)
    gItemSlots[ITEM_MS_MASK_ADULT] = SLOT_MASK_FIERCE_DEITY; // its cell (SLOT() of it is where a button holds it from)
    for (s32 form = 0; form < PLAYER_FORM_MAX; form++) {
        gPlayerFormItemRestrictions[form][ITEM_MS_MASK_ADULT] = gPlayerFormItemRestrictions[form][ITEM_MASK_FIERCE_DEITY];
    }
}

// ---- its description on the Masks page ------------------------------------------------------------------------------------
// A on a mask shows Majora's Mask's message 0x1700 + its item, and ITEM_CB has none, so the box showed whatever text was
// shown last (BUG-0025). Its own text, in the Fierce Deity's Mask's description box; the Child Mask's while the human is
// the adult.

static void RegisterAdultMaskDescription() {
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(
        0x1700 + ITEM_MS_MASK_ADULT, [](u16* textId, bool* loadFromMessageTable) {
            CustomMessage::Entry entry = CustomMessage::LoadVanillaMessageTableEntry(0x1700 + ITEM_MASK_FIERCE_DEITY);
            entry.icon = ITEM_MS_MASK_ADULT; // its own icon (gItemIcons), not the Fierce Deity's Mask's
            entry.msg = MergeShip_AdultMask_IsChildMask()
                            ? "%rChild Mask%w\x11The face the Skull Kid wore. Put it on to become young Link again."
                            : "%rAdult Mask%w\x11" "A hero's face from another time. Put it on to become adult Link; "
                              "put it on again to return.";
            entry.autoFormat = true;
            *loadFromMessageTable = false;
            CustomMessage::LoadCustomMessageIntoFont(entry);
        });
}

static RegisterShipInitFunc sAdultMaskDescriptionInit(RegisterAdultMaskDescription);

// ---- menu ("Enhancements" > "Mergeship", BenMenu.cpp) ---------------------------------------------------------------------

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
using namespace UIWidgets;

static void RegisterAdultMaskMenu() {
    WidgetPath path = { "Enhancements", "Mergeship", SECTION_COLUMN_1 };
    mBenMenu->AddWidget(path, "Adult Mask", WIDGET_CVAR_CHECKBOX)
        .CVar(MERGESHIP_ADULT_MASK_CVAR)
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Every file starts with the Adult Mask, on the Fierce Deity's Mask's cell of the Masks page. Put on as "
            "young Link it transforms him, with Majora's Mask's transformation cutscene, into Ocarina of Time's adult "
            "Link. While he is the adult it is the Child Mask -- the Skull Kid's Link mask -- and putting it on plays "
            "the same cutscene and turns him back into young Link (another transformation mask ends the adult too). "
            "Once you have the Fierce Deity's Mask too, press A then Right on that cell to swap which mask it holds. "
            "Needs the Adult Link option and Ocarina of Time's archive."));
}

static RegisterMenuInitFunc sAdultMaskMenuInit(RegisterAdultMaskMenu);
} // namespace BenGui
