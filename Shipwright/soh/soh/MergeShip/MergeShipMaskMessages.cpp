// MergeShip2 -- the dialogue and randomizer hints of Majora's Mask's mask reactions (MergeShipMaskReactions.h).
//
// Text: MM's English messages, read out of mm.o2r (mm/text/message_data_static) and re-encoded with Ocarina of Time's
// control codes (MM 0x11 newline -> OoT 0x01, MM 0x19 event -> OoT 0x0B, MM 0xBF end -> OoT 0x02). MM text sound
// effects (0x1E xx xx) name MM's sound tables, so the actor plays them through MergeShip_PlayMMSfx* instead.
//
// Randomizer hints come from the seed's Gossip Stone hints, the way a stone reads its own:
//   * the pool is every Gossip Stone hint of the seed that is enabled, deduplicated by text (the generator places
//     most hints on two stones), without the "Junk" distribution unless nothing else is left;
//   * a Stalchild spoken to through the Captain's Hat picks from that pool once, the first time it needs a hint, and
//     keeps it -- it never rerolls. Successive Stalchildren walk through the pool in order from a seed-derived start,
//     so each new one reports a different hint until the pool wraps;
//   * a Gossip Stone read through MM's Mask of Truth adds pool[hash(seed, stone) % size], skipping its own hint's
//     text: the same stone always gives the same extra hint in a seed.
// Every hint shown fires OnRandoHintRevealed, as GossipStoneHints.cpp does, so the Hint Tracker records it.

#include "MergeShipMaskMessages.h"

#include <set>
#include <string>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/MergeShip/MergeShipUnifiedHints.h"

extern "C" {
#include <macros.h>
#include <functions.h>
#include <variables.h>
#include "soh/cvar_prefixes.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipMaskReactions.h"
#include "src/overlays/actors/ovl_En_Skb/z_en_skb.h"
extern PlayState* gPlayState;
}

// OoT message control codes (include/message_data_fmt.h).
#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_BOX_BREAK "\x04"
#define MS_EVENT "\x0B"

// ---- Majora's Mask's lines -------------------------------------------------------------------------------------

// MM 0x13F6: "Huh?" 0x19 0xBF
static const std::string sSkbHuh = "Huh?" MS_EVENT;
// MM 0x13F7: 0x1E 0x3AA8 "Oh, Captain!" 0x11 "E-excuse me, sir!" 0xBF
static const std::string sSkbCaptain = "Oh, Captain!" MS_NEWLINE "E-excuse me, sir!";
// MM 0x13F8: "The squad leader is in front of" 0x11 "that grave over there, sir." 0xBF
static const std::string sSkbSquadLeader = "The squad leader is in front of" MS_NEWLINE "that grave over there, sir.";
// MM 0x13B2: "Brrrinnnggg them!" 0x19 0xBF
static const std::string sGibdoBringThem = "Brrrinnnggg them!" MS_EVENT;
// MM 0x13B3: "Those inssside...Our friendsss..." 0x11 "Brrrriiiiinnnng them!" 0xBF
static const std::string sGibdoOurFriends = "Those inssside...Our friendsss..." MS_NEWLINE "Brrrriiiiinnnng them!";

// MM text is English only; every language shows it. MM textbox type 0 / position 0 are OoT's black box, variable
// position.
static void LoadMMLine(const std::string& text, bool* loadFromMessageTable) {
    CustomMessage msg(text, text, text, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// ---- Randomizer hints ------------------------------------------------------------------------------------------

// As GossipStoneHints.cpp ReadHintMessage: resolve the hint's text and tell observers the player has seen it.
static CustomMessage ReadHint(RandomizerHint rh) {
    GameInteractor::Instance->ExecuteHooks<GameInteractor::OnRandoHintRevealed>(rh);
    CustomMessage msg = Rando::Context::GetInstance()->GetHint(rh)->GetHintMessage(MF_AUTO_FORMAT);
    // As BuildHintStoneMessage: remove "Buy " if present.
    msg.Replace("Buy ", "");
    msg.Replace("Acheter: ", "");
    msg.Replace(" kaufen ", "");
    msg.Replace(" kaufen", "");
    return msg;
}

static std::string HintKeyText(RandomizerHint rh) {
    return Rando::Context::GetInstance()->GetHint(rh)->GetHintMessage(MF_CLEAN).GetEnglish(MF_RAW);
}

// Every Gossip Stone hint of the seed, in RandomizerHint order (see the comment at the top of the file).
static std::vector<RandomizerHint> GossipStoneHintPool(RandomizerHint excluded) {
    auto ctx = Rando::Context::GetInstance();
    std::set<RandomizerHint> stones;
    for (const auto& [params, rh] : Rando::StaticData::stoneParamsToHint) {
        stones.insert(rh);
    }
    for (const auto& [params, rh] : Rando::StaticData::grottoChestParamsToHint) {
        stones.insert(rh);
    }

    std::set<std::string> seen;
    if (excluded != RH_NONE) {
        seen.insert(HintKeyText(excluded));
    }
    std::vector<RandomizerHint> useful;
    std::vector<RandomizerHint> junk;
    for (RandomizerHint rh : stones) {
        Rando::Hint* hint = ctx->GetHint(rh);
        if (!hint->IsEnabled() || !seen.insert(HintKeyText(rh)).second) {
            continue;
        }
        (hint->GetDistribution() == "Junk" ? junk : useful).push_back(rh);
    }
    return useful.empty() ? junk : useful;
}

static uint32_t HashHintKey(uint32_t seed, uint32_t key) {
    uint32_t h = seed ^ (key * 0x9E3779B1u);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

// ---- Captain's Hat: Stalchildren -------------------------------------------------------------------------------

static uint32_t sStalchildHintsGiven = 0;

// The hint the Stalchild the player is talking to reports, chosen the first time it is asked for; RH_NONE when the
// randomizer or MergeShip.CaptainsHatHints is off, or the seed has no Gossip Stone hints.
static RandomizerHint StalchildHint() {
    if (!IS_RANDO || !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.CaptainsHatHints"), 1) || gPlayState == nullptr) {
        return RH_NONE;
    }
    Actor* talkActor = GET_PLAYER(gPlayState)->talkActor;
    if ((talkActor == nullptr) || (talkActor->id != ACTOR_EN_SKB)) {
        return RH_NONE;
    }
    EnSkb* skb = (EnSkb*)talkActor;
    if (!skb->mergeShipHintChosen) {
        std::vector<RandomizerHint> pool = GossipStoneHintPool(RH_NONE);
        skb->mergeShipHint = RH_NONE;
        if (!pool.empty()) {
            uint32_t start = HashHintKey(Rando::Context::GetInstance()->GetSeed(), 0x534B42u); // "SKB"
            skb->mergeShipHint = pool[(start + sStalchildHintsGiven++) % pool.size()];
        }
        skb->mergeShipHintChosen = 1;
    }
    return (RandomizerHint)skb->mergeShipHint;
}

// `mmLine` then, in a second textbox, `hint`. Both keep their own control codes; the hint's box ends the message.
static CustomMessage LineThenHint(const std::string& mmLine, const CustomMessage& hint) {
    return CustomMessage(mmLine + MS_BOX_BREAK + hint.GetEnglish(MF_RAW), mmLine + MS_BOX_BREAK + hint.GetGerman(MF_RAW),
                         mmLine + MS_BOX_BREAK + hint.GetFrench(MF_RAW), TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE);
}

static void BuildStalchildHuh(uint16_t* textId, bool* loadFromMessageTable) {
    LoadMMLine(sSkbHuh + MS_END, loadFromMessageTable);
}

// Randomizer: MM's "Oh, Captain! E-excuse me, sir!", then the Stalchild's hint.
static void BuildStalchildCaptain(uint16_t* textId, bool* loadFromMessageTable) {
    RandomizerHint rh = StalchildHint();
    if (rh == RH_NONE) {
        LoadMMLine(sSkbCaptain + MS_END, loadFromMessageTable);
        return;
    }
    LineThenHint(sSkbCaptain, ReadHint(rh)).LoadIntoFont();
    *loadFromMessageTable = false;
}

// Randomizer: the hint instead of MM's directions to the squad leader, which are about Ikana's graveyard.
static void BuildStalchildSquadLeader(uint16_t* textId, bool* loadFromMessageTable) {
    RandomizerHint rh = StalchildHint();
    if (rh == RH_NONE) {
        LoadMMLine(sSkbSquadLeader + MS_END, loadFromMessageTable);
        return;
    }
    CustomMessage msg = ReadHint(rh);
    msg.SetTextBoxType(TEXTBOX_TYPE_BLACK);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// ---- Gibdo Mask: Gibdos ----------------------------------------------------------------------------------------

static void BuildGibdoBringThem(uint16_t* textId, bool* loadFromMessageTable) {
    LoadMMLine(sGibdoBringThem + MS_END, loadFromMessageTable);
}

static void BuildGibdoOurFriends(uint16_t* textId, bool* loadFromMessageTable) {
    LoadMMLine(sGibdoOurFriends + MS_END, loadFromMessageTable);
}

// ---- Mask of Truth: Gossip Stones ------------------------------------------------------------------------------

// The extra hint a stone gives through MM's Mask of Truth. In a unified seed (MergeShipUnifiedHints.h) half of the
// stones, chosen by the same hash, name one of Termina's major items and its area instead, so the extra hint can point
// into Termina even when no Gossip Stone hint does. `reveal`: tell the Hint Tracker (ReadHint).
static bool TruthExtraHint(RandomizerHint stoneHint, CustomMessage* out, bool reveal) {
    std::vector<RandomizerHint> pool = GossipStoneHintPool(stoneHint);
    const uint32_t hash = HashHintKey(Rando::Context::GetInstance()->GetSeed(), stoneHint);
    const size_t terminaCount = MergeShip_Hints_TruthTerminaCount();
    if ((terminaCount > 0) && (pool.empty() || (((hash >> 7) & 1u) != 0))) {
        *out = MergeShip_Hints_TruthTerminaHint(hash >> 8);
        return true;
    }
    if (pool.empty()) {
        return false;
    }
    RandomizerHint extra = pool[hash % pool.size()];
    *out = reveal ? ReadHint(extra) : Rando::Context::GetInstance()->GetHint(extra)->GetHintMessage(MF_AUTO_FORMAT);
    return true;
}

std::string MergeShip_TruthExtraHintText(RandomizerHint stoneHint) {
    // Any loaded seed (the harness reads a freshly generated one on its unsaved debug file, which is not a randomizer
    // file).
    CustomMessage msg;
    auto ctx = Rando::Context::GetInstance();
    if ((ctx == nullptr) || (stoneHint == RH_NONE) || (ctx->GetHint(stoneHint) == nullptr) ||
        !TruthExtraHint(stoneHint, &msg, false)) {
        return "";
    }
    return msg.GetEnglish(MF_CLEAN);
}

void MergeShip_AppendTruthExtraHint(CustomMessage& msg, RandomizerHint stoneHint) {
    if (!IS_RANDO || (stoneHint == RH_NONE) || (gPlayState == nullptr) ||
        !CVarGetInteger(CVAR_ENHANCEMENT("MergeShip.TruthExtraHint"), 1) ||
        !MERGESHIP_WEARS_MM_MASK(GET_PLAYER(gPlayState), MM_MASK_TRUTH)) {
        return;
    }
    CustomMessage extraMsg;
    if (!TruthExtraHint(stoneHint, &extraMsg, true)) {
        return;
    }

    // Both are auto-formatted and end with MESSAGE_END: drop the stone's, break the box, and let the extra hint end
    // the message. The stone's textbox type and position are kept.
    std::string parts[3] = { msg.GetEnglish(MF_RAW), msg.GetGerman(MF_RAW), msg.GetFrench(MF_RAW) };
    const std::string extras[3] = { extraMsg.GetEnglish(MF_RAW), extraMsg.GetGerman(MF_RAW),
                                    extraMsg.GetFrench(MF_RAW) };
    for (int i = 0; i < 3; i++) {
        if (!parts[i].empty() && (parts[i].back() == MS_END[0])) {
            parts[i].pop_back();
        }
        parts[i] += MS_BOX_BREAK + extras[i];
    }
    msg = CustomMessage(parts[0], parts[1], parts[2], msg.GetTextBoxType(), msg.GetTextBoxPosition());
}

// ---- Registration ----------------------------------------------------------------------------------------------

static void RegisterMergeShipMaskMessages() {
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_SKB_HUH, true, BuildStalchildHuh);
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_SKB_CAPTAIN, true, BuildStalchildCaptain);
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_SKB_SQUAD_LEADER, true, BuildStalchildSquadLeader);
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_GIBDO_BRING_THEM, true, BuildGibdoBringThem);
    COND_ID_HOOK(OnOpenText, MERGESHIP_TEXT_GIBDO_OUR_FRIENDS, true, BuildGibdoOurFriends);
}

static RegisterShipInitFunc sMergeShipMaskMessagesInit(RegisterMergeShipMaskMessages);
