#pragma once

// MergeShip2 -- the dialogue and randomizer hints of Majora's Mask's mask reactions (see MergeShipMaskReactions.h).
// C++ only: used by SoH's randomizer message code.

#ifdef __cplusplus

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"

// Randomizer, MergeShip.TruthExtraHint: while MM's Mask of Truth is worn, a Gossip Stone that gives its hint gives one
// more. `msg` is the stone's own hint, already auto-formatted; the extra hint is appended in a second textbox. The
// extra hint is fixed per stone and seed (see MergeShipMaskMessages.cpp). Does nothing when not applicable.
void MergeShip_AppendTruthExtraHint(CustomMessage& msg, RandomizerHint stoneHint);
// The extra hint's text for `stoneHint` (English, clean), whether or not the mask is worn; "" when there is none. For
// the test harness (MergeShipUnifiedHints.cpp "truthhint"): in a unified seed it may be about Termina.
std::string MergeShip_TruthExtraHintText(RandomizerHint stoneHint);

#endif
