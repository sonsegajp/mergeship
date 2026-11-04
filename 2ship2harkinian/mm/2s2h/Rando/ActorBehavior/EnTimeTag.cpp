#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/MergeShip/UnifiedHyrule.h" // MergeShip2: Hyrule's checks in a unified seed

extern "C" {
#include "variables.h"
#include "src/overlays/actors/ovl_En_Toto/z_en_toto.h"
extern void func_80BA36C0(EnToto* enToto, PlayState* play, s32 index);
}

// The cutscene skip is handled elsewhere, and forced on for rando.
// Upon interacting with the engraving, this text ID is opened.
#define ENGRAVING_TEXT_ID 0xC02

void Rando::ActorBehavior::InitEnTimeTagBehavior() {
    COND_ID_HOOK(OnOpenText, ENGRAVING_TEXT_ID, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        if (!RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].cycleObtained) {
            RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible = true;
        }

        if (RANDO_SAVE_OPTIONS[RO_HINTS_SONG_OF_SOARING]) {
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = "A note is carved into the stone...\x10What you seek lies %y{{location}}%w.";

            RandoCheckId randoCheckId = Rando::FindItemPlacement(RI_SONG_SOARING);
            // MergeShip2: or its Hyrule check, in a unified seed (UnifiedHyrule.h)
            CustomMessage::Replace(&entry.msg, "{{location}}",
                                   Rando::MergeShip::ItemHintLocation(RI_SONG_SOARING, randoCheckId, false));
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        }
    });
}
