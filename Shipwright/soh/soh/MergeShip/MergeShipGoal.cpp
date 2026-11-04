// MergeShip2 -- the unified randomizer's goal (MergeShipGoal.h).

#include "MergeShipGoal.h"
#include "MergeShipHost.h"
#include "MergeShipTerminaRando.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>

#include <spdlog/spdlog.h>
#include <mergeship/MsItems.h>
#include <mergeship/SharedSave.h>

#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/randomizer/location_access.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Notification/Notification.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
#include "soh/MergeShip/MergeShipForms.h"
}

// hook_handlers.cpp: SoH's own win-condition triggers (stones, medallions, ... Triforce pieces), and the trigger check
// that queues RC_WINCON once MeetsWinconRequirements holds.
bool MeetsHyruleWinconRequirements();
void CheckTriggers();

namespace {

// ---- settings ------------------------------------------------------------------------------------------------------

uint8_t Opt(RandomizerSettingKey rsk) {
    return Rando::Context::GetInstance()->GetOption(rsk).Get();
}

uint8_t Goal() {
    return Opt(RSK_MS_GOAL);
}

bool Unified() {
    return MergeShip_Termina_Enabled();
}

bool GoalNeedsMajora() {
    return Goal() == RO_MS_GOAL_MAJORA || Goal() == RO_MS_GOAL_BOTH;
}

// Hyrule's part of the goal is Ganon himself when SoH's win condition is Ganon (or "Anywhere", which a unified goal
// replaces with Ganon), otherwise SoH's own trigger.
bool HyruleWinconIsGanon() {
    auto& wincon = Rando::Context::GetInstance()->GetOption(RSK_WINCON);
    return wincon.Is(RO_WINCON_DEFEAT_GANON) || wincon.Is(RO_WINCON_ANYWHERE);
}

int TriforceTotal() {
    return Opt(RSK_MS_TRIFORCE_PIECES_TOTAL) + 1;
}

// 2Ship's Majora requirements (its option values; these RSK_MM_* start at 0).
int MoonRemains() {
    return Opt(RSK_MM_ACCESS_MOON_REMAINS_COUNT);
}
int MoonMasks() {
    return Opt(RSK_MM_ACCESS_MOON_MASKS_COUNT);
}
int MajoraRemains() {
    return Opt(RSK_MM_ACCESS_MAJORA_REMAINS_COUNT);
}
int MajoraMasks() {
    return Opt(RSK_MM_ACCESS_MAJORA_MASKS_COUNT);
}

// ---- items ---------------------------------------------------------------------------------------------------------

constexpr RandomizerGet kRemains[] = { RG_MM_ODOLWAS_REMAINS, RG_MM_GOHTS_REMAINS, RG_MM_GYORGS_REMAINS,
                                       RG_MM_TWINMOLDS_REMAINS };
// 2Ship's MoonMaskCount: MM's 20 masks that are not transformations (ITEM_MASK_TRUTH .. ITEM_MASK_GIANT).
constexpr RandomizerGet kMoonMasks[] = {
    RG_MM_MASK_OF_TRUTH, RG_MM_KAFEIS_MASK,   RG_MM_ALL_NIGHT_MASK, RG_MM_BUNNY_HOOD,      RG_MM_KEATON_MASK,
    RG_MM_GAROS_MASK,    RG_MM_ROMANIS_MASK,  RG_MM_CIRCUS_LEADERS_MASK, RG_MM_POSTMANS_HAT, RG_MM_COUPLES_MASK,
    RG_MM_GREAT_FAIRYS_MASK, RG_MM_GIBDO_MASK, RG_MM_DON_GEROS_MASK, RG_MM_KAMAROS_MASK,   RG_MM_CAPTAINS_HAT,
    RG_MM_STONE_MASK,    RG_MM_BREMEN_MASK,   RG_MM_BLAST_MASK,     RG_MM_MASK_OF_SCENTS,  RG_MM_GIANTS_MASK,
};
constexpr RandomizerGet kFormMasks[] = { RG_MM_DEKU_MASK, RG_MM_GORON_MASK, RG_MM_ZORA_MASK,
                                         RG_MM_FIERCE_DEITY_MASK };

template <size_t N> int CountOwned(Rando::Logic* logic, const RandomizerGet (&items)[N]) {
    int count = 0;
    for (RandomizerGet rg : items) {
        count += logic->HasItem(rg) ? 1 : 0;
    }
    return count;
}

template <size_t N> int CountPlaced(const RandomizerGet (&items)[N]) {
    auto ctx = Rando::Context::GetInstance();
    int count = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        const RandomizerGet placed = ctx->GetItemLocation(rc)->GetPlacedRandomizerGet();
        count += std::find(std::begin(items), std::end(items), placed) != std::end(items) ? 1 : 0;
    }
    return count;
}

// ---- the shared save -----------------------------------------------------------------------------------------------

// Outside mergeship.exe (soh.exe, the test harness) the goal lives here: Termina's part never changes by itself.
MsSharedSave sLocalShared;

MsSharedSave* Shared() {
    MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
    return (shared != nullptr) ? shared : &sLocalShared;
}

// Triforce pieces the ledger records, found in either world (each check once; a harness give, check 0, each time).
uint16_t LedgerTriforcePieces(const MsSharedSave* shared) {
    static const MsSharedSave* sCachedSave = nullptr;
    static uint32_t sCachedCount = UINT32_MAX;
    static uint32_t sCachedSeed = 0;
    static uint16_t sCached = 0;
    if (shared->ledgerSeed != 0 && shared->ledgerSeed != Rando::Context::GetInstance()->GetSeed()) {
        return 0; // another seed's ledger (SoH resets it when the file loads)
    }
    if (shared == sCachedSave && shared->ledgerCount == sCachedCount && shared->ledgerSeed == sCachedSeed) {
        return sCached;
    }
    std::set<uint32_t> seen;
    uint16_t pieces = 0;
    const uint32_t count = std::min<uint32_t>(shared->ledgerCount, MS_SHARED_LEDGER_MAX);
    for (uint32_t i = 0; i < count; i++) {
        const MsLedgerEntry& entry = shared->ledger[i];
        if (entry.item != MSI_TRIFORCE_PIECE) {
            continue;
        }
        if (entry.check != 0 && !seen.insert(((uint32_t)entry.world << 16) | entry.check).second) {
            continue;
        }
        pieces++;
    }
    sCachedSave = shared;
    sCachedCount = shared->ledgerCount;
    sCachedSeed = shared->ledgerSeed;
    sCached = pieces;
    return pieces;
}

// Hyrule's part of the goal, in game.
bool HyruleRuntimeMet() {
    return HyruleWinconIsGanon() ? (Flags_GetRandomizerInf(RAND_INF_DUNGEONS_DONE_GANONS_TOWER) != 0)
                                 : MeetsHyruleWinconRequirements();
}

// SoH's part of the goal block: the configuration and Hyrule's flag. Another seed's flags are cleared first.
void WriteHyrulePart(MsSharedSave* shared) {
    const uint8_t type = MergeShip_Goal_SharedType();
    shared->goalType = type;
    if (type == MS_GOAL_NONE) {
        return;
    }
    const uint32_t seed = Rando::Context::GetInstance()->GetSeed();
    if (shared->goalSeed != seed) {
        shared->goalSeed = seed;
        shared->goalFlags = 0;
    }
    shared->majoraFormMasks = Opt(RSK_MS_MAJORA_FORM_MASKS);
    shared->triforceRequired = MergeShip_Goal_TriforceRequired();
    if (HyruleRuntimeMet()) {
        shared->goalFlags |= MS_GOAL_FLAG_HYRULE;
    } else {
        shared->goalFlags &= (uint8_t)~MS_GOAL_FLAG_HYRULE;
    }
}

bool GoalMet(const MsSharedSave* shared, bool hyrule, int pieces) {
    const bool majora = (shared->goalFlags & MS_GOAL_FLAG_MAJORA) != 0;
    switch (Goal()) {
        case RO_MS_GOAL_GANON:
            return hyrule;
        case RO_MS_GOAL_MAJORA:
            return majora;
        case RO_MS_GOAL_BOTH:
            return hyrule && majora;
        case RO_MS_GOAL_TRIFORCE:
            return pieces >= MergeShip_Goal_TriforceRequired();
        default:
            return false;
    }
}

// ---- words ---------------------------------------------------------------------------------------------------------

std::string Plural(int count, const char* one, const char* many) {
    return std::to_string(count) + " " + (count == 1 ? one : many);
}

// Hyrule's part, e.g. "Ganon is defeated" or "6 Medallions are found".
std::string HyrulePartText() {
    if (HyruleWinconIsGanon()) {
        return "Ganon is defeated in Hyrule";
    }
    auto ctx = Rando::Context::GetInstance();
    switch (ctx->GetOption(RSK_WINCON).Get()) {
        case RO_WINCON_STONES:
            return Plural(ctx->GetOption(RSK_WINCON_STONE_COUNT).Get(), "Spiritual Stone", "Spiritual Stones") +
                   " are found";
        case RO_WINCON_MEDALLIONS:
            return Plural(ctx->GetOption(RSK_WINCON_MEDALLION_COUNT).Get(), "Medallion", "Medallions") +
                   " are found";
        case RO_WINCON_REWARDS:
            return Plural(ctx->GetOption(RSK_WINCON_REWARD_COUNT).Get(), "dungeon reward", "dungeon rewards") +
                   " are found";
        case RO_WINCON_DUNGEONS:
            return Plural(ctx->GetOption(RSK_WINCON_DUNGEON_COUNT).Get(), "dungeon", "dungeons") + " are completed";
        case RO_WINCON_TOKENS:
            return Plural(ctx->GetOption(RSK_WINCON_TOKEN_COUNT).Get(), "Gold Skulltula Token",
                          "Gold Skulltula Tokens") +
                   " are found";
        case RO_WINCON_TRIFORCE_PIECES:
            return Plural(ctx->GetOption(RSK_WINCON_TRIFORCE_COUNT).Get(), "Triforce piece", "Triforce pieces") +
                   " are found";
        default:
            return "Ganon is defeated in Hyrule";
    }
}

// What stands between the player and Majora, e.g. "4 remains to reach the moon".
std::string MajoraRequirementText() {
    std::string text;
    auto add = [&text](const std::string& part) { text += (text.empty() ? "" : ", ") + part; };
    const int remains = std::max(MoonRemains(), MajoraRemains());
    const int masks = std::max(MoonMasks(), MajoraMasks());
    const int forms = Opt(RSK_MS_MAJORA_FORM_MASKS);
    if (remains > 0) {
        add(Plural(remains, "boss remains", "boss remains"));
    }
    if (masks > 0) {
        add(masks >= 20 ? std::string("all 20 masks") : Plural(masks, "mask", "masks"));
    }
    if (forms > 0) {
        add(forms >= 4 ? std::string("all 4 transformation masks")
                       : Plural(forms, "transformation mask", "transformation masks"));
    }
    return text.empty() ? "nothing but the way to the moon" : text;
}

} // namespace

// ---- configuration -------------------------------------------------------------------------------------------------

bool MergeShip_Goal_OwnsWincon() {
    return Unified() && Goal() != RO_MS_GOAL_GANON;
}

uint8_t MergeShip_Goal_SharedType() {
    if (!Unified()) {
        return MS_GOAL_NONE;
    }
    return (uint8_t)(Goal() + 1); // RO_MS_GOAL_* + 1 == MS_GOAL_*
}

uint16_t MergeShip_Goal_TriforceRequired() {
    return (uint16_t)std::min<int>(Opt(RSK_MS_TRIFORCE_PIECES_REQUIRED) + 1, TriforceTotal());
}

const std::string* MergeShip_Goal_SettingText(RandomizerSettingKey rsk, bool description) {
    static const std::map<RandomizerSettingKey, std::pair<std::string, std::string>> kTexts = {
        { RSK_MS_GOAL,
          { "Mergeship: Goal",
            "What beating the unified seed takes.\n"
            "Ganon: Hyrule's own win condition (the Win Condition options: defeat Ganon, or one of their triggers). "
            "Majora is optional.\n"
            "Majora: defeat Majora in Termina, behind Termina's moon and Majora requirements. Ganon is optional.\n"
            "Ganon and Majora: both, in either order; the ending plays in the world where the last one falls. "
            "Beating the first one tells you what is left and lets you go on (Majora sends you back to the dawn of "
            "the first day).\n"
            "Triforce Hunt: find the required Triforce pieces, spread over both worlds; pieces from either world "
            "count in both." } },
        { RSK_MS_TRIFORCE_PIECES_TOTAL,
          { "Mergeship: Triforce Pieces in Pool",
            "Triforce Hunt: how many Triforce pieces the unified item pool holds, placed anywhere in Hyrule or "
            "Termina (the Hyrule Triforce Hunt's own pieces count towards it)." } },
        { RSK_MS_TRIFORCE_PIECES_REQUIRED,
          { "Mergeship: Triforce Pieces Required",
            "Triforce Hunt: how many pieces, found in either world, complete the Triforce (at most the pool's)." } },
        { RSK_MS_REMAINS_AS_MEDALLIONS,
          { "Mergeship: Remains Count as Medallions",
            "Majora's Mask's four boss remains count as medallions (and so as dungeon rewards) wherever Hyrule counts "
            "them: the Rainbow Bridge, Ganon's Boss Key, Ganon's Soul and the win condition. The counts' limits stay "
            "as they are: with 6 Medallions required, any 6 of the 6 medallions and 4 remains will do." } },
        { RSK_MS_MAJORA_FORM_MASKS,
          { "Termina: Majora Transformation Masks",
            "Transformation masks (Deku, Goron, Zora, Fierce Deity's) Majora requires on top of Termina's Majora "
            "Masks count, which counts Majora's Mask's other 20 masks. 20 masks and 4 transformation masks is every "
            "mask." } },
        { RSK_MS_COMBINE_SHARED_ITEMS,
          { "Mergeship: Combine Shared Items",
            "Items both games have are one item: finding either game's gives it in both games, and the pool holds "
            "one copy. That covers the bows, Fire, Ice and Light Arrows, the Lens of Truth, the Hookshots (Ocarina "
            "of Time's Hookshot or Longshot and MM's Hookshot), the Hylian / Hero's and both Mirror Shields, the "
            "Kokiri Swords, the ocarina, bombs, bombchus, sticks, nuts, beans, their bags and quivers, ammo and "
            "bottles.\n"
            "Off (default): every Ocarina of Time item and every Majora's Mask item is separate. The pool holds "
            "each game's own copies, any check in either world can hold either game's, and a copy only ever goes "
            "to its own game: Ocarina of Time's Mirror Shield found in Termina is Hyrule's, Majora's Mask's "
            "Hookshot found in Hyrule is Termina's (Majora's Mask's masks aside)." } },
        { RSK_MS_TERMINA_HINTS,
          { "Mergeship: Hints Cover Termina",
            "Hyrule's hints cover both worlds: Gossip Stones may name Termina's checks and areas (Clock Town, "
            "Woodfall Temple, ...), and Way of the Hero and foolish hints count Termina's areas too. Off, Termina's "
            "checks are never hinted in Hyrule (items from Termina can still be named)." } },
        { RSK_MM_ACCESS_MOON_REMAINS_COUNT,
          { "Termina: Moon Remains",
            "Boss remains needed to reach the moon (Oath to Order on the Clock Tower rooftop). Majora's Mask needs "
            "all 4." } },
        { RSK_MM_ACCESS_MOON_MASKS_COUNT,
          { "Termina: Moon Masks",
            "Masks (of Majora's Mask's 20 masks that are not transformations) needed to reach the moon." } },
        { RSK_MM_ACCESS_TRIALS,
          { "Termina: Moon Trials",
            "What opens the moon's four trials: Vanilla (the masks the moon children ask for), 2-6-12-20 masks, the "
            "trial's boss remains, the trial's transformation, or Open." } },
        { RSK_MM_ACCESS_MAJORA_REMAINS_COUNT,
          { "Termina: Majora Remains",
            "Boss remains needed to face Majora (the moon child under the tree lets you in)." } },
        { RSK_MM_ACCESS_MAJORA_MASKS_COUNT,
          { "Termina: Majora Masks",
            "Masks (of Majora's Mask's 20 masks that are not transformations) needed to face Majora. 20 is all of "
            "them; add the transformation masks with Termina: Majora Transformation Masks." } },
    };
    auto it = kTexts.find(rsk);
    if (it == kTexts.end()) {
        return nullptr;
    }
    return description ? &it->second.second : &it->second.first;
}

// ---- generation ----------------------------------------------------------------------------------------------------

namespace {

// Termina's logic is ported (M4) once Majora's lair is a region of the world graph; until then (M1) every Termina
// location sits in RR_MM_ROOT with no condition.
bool TerminaLogicPorted() {
    const Region* lair = RegionTable(RR_MM_MOON_MAJORAS_LAIR);
    return !lair->locations.empty() || !lair->exits.empty() || !lair->events.empty();
}

// M1: Majora counts once Termina is reached and the remains and masks 2Ship asks for (moon and Majora alike) are
// found. Remains that 2Ship keeps at their bosses (RO_SHUFFLE_BOSS_REMAINS vanilla) come with Termina.
bool MajoraReachableWithoutTerminaLogic(Rando::Logic* logic) {
    if (!RegionTable(RR_MM_ROOT)->HasAccess()) {
        return false;
    }
    const bool vanillaRemains = Opt(RSK_MM_SHUFFLE_BOSS_REMAINS) == 0;
    const int remains = vanillaRemains ? 4 : CountOwned(logic, kRemains);
    return remains >= std::max(MoonRemains(), MajoraRemains()) &&
           CountOwned(logic, kMoonMasks) >= std::max(MoonMasks(), MajoraMasks());
}

bool MajoraLogicMet(Rando::Logic* logic) {
    const bool beaten = TerminaLogicPorted() ? logic->Get(LOGIC_MM_MAJORA_DEFEATED)
                                             : MajoraReachableWithoutTerminaLogic(logic);
    return beaten && CountOwned(logic, kFormMasks) >= Opt(RSK_MS_MAJORA_FORM_MASKS);
}

bool HyruleLogicMet(Rando::Logic* logic) {
    return HyruleWinconIsGanon() ? logic->Get(LOGIC_MS_GANON_DEFEATED) : logic->CanTriggerHyruleWincon();
}

} // namespace

bool MergeShip_Goal_LogicMet(Rando::Logic* logic) {
    switch (Goal()) {
        case RO_MS_GOAL_GANON:
            return HyruleLogicMet(logic);
        case RO_MS_GOAL_MAJORA:
            return MajoraLogicMet(logic);
        case RO_MS_GOAL_BOTH:
            return HyruleLogicMet(logic) && MajoraLogicMet(logic);
        case RO_MS_GOAL_TRIFORCE:
            return logic->GetTriforcePieceCount() >= MergeShip_Goal_TriforceRequired();
        default:
            return false;
    }
}

uint8_t MergeShip_Goal_LogicExtraMedallions(Rando::Logic* logic) {
    if (!Unified() || !Opt(RSK_MS_REMAINS_AS_MEDALLIONS)) {
        return 0;
    }
    return (uint8_t)CountOwned(logic, kRemains);
}

bool MergeShip_Goal_ItemIsAdvancement(RandomizerGet rg) {
    enum { OTHER, REMAINS, MOON_MASK, FORM_MASK } kind = OTHER;
    switch (rg) {
        case RG_MM_ODOLWAS_REMAINS:
        case RG_MM_GOHTS_REMAINS:
        case RG_MM_GYORGS_REMAINS:
        case RG_MM_TWINMOLDS_REMAINS:
            kind = REMAINS;
            break;
        case RG_MM_MASK_OF_TRUTH:
        case RG_MM_KAFEIS_MASK:
        case RG_MM_ALL_NIGHT_MASK:
        case RG_MM_BUNNY_HOOD:
        case RG_MM_KEATON_MASK:
        case RG_MM_GAROS_MASK:
        case RG_MM_ROMANIS_MASK:
        case RG_MM_CIRCUS_LEADERS_MASK:
        case RG_MM_POSTMANS_HAT:
        case RG_MM_COUPLES_MASK:
        case RG_MM_GREAT_FAIRYS_MASK:
        case RG_MM_GIBDO_MASK:
        case RG_MM_DON_GEROS_MASK:
        case RG_MM_KAMAROS_MASK:
        case RG_MM_CAPTAINS_HAT:
        case RG_MM_STONE_MASK:
        case RG_MM_BREMEN_MASK:
        case RG_MM_BLAST_MASK:
        case RG_MM_MASK_OF_SCENTS:
        case RG_MM_GIANTS_MASK:
            kind = MOON_MASK;
            break;
        case RG_MM_DEKU_MASK:
        case RG_MM_GORON_MASK:
        case RG_MM_ZORA_MASK:
        case RG_MM_FIERCE_DEITY_MASK:
            kind = FORM_MASK;
            break;
        default:
            return false;
    }
    if (!Unified()) {
        return false;
    }
    const bool majora = GoalNeedsMajora();
    switch (kind) {
        case REMAINS:
            return Opt(RSK_MS_REMAINS_AS_MEDALLIONS) || (majora && std::max(MoonRemains(), MajoraRemains()) > 0);
        case MOON_MASK:
            return majora && std::max(MoonMasks(), MajoraMasks()) > 0;
        case FORM_MASK:
            return majora && Opt(RSK_MS_MAJORA_FORM_MASKS) > 0;
        default:
            return false;
    }
}

void MergeShip_Goal_AddItemsToPool(std::vector<RandomizerGet>& itemPool) {
    if (!Unified() || Goal() != RO_MS_GOAL_TRIFORCE) {
        return;
    }
    // Hyrule's own Triforce Hunt pieces (RSK_TRIFORCE_HUNT_PIECES_TOTAL) are already in; top up to the unified count.
    const int have = (int)std::count(itemPool.begin(), itemPool.end(), RG_TRIFORCE_PIECE);
    for (int i = have; i < TriforceTotal(); i++) {
        itemPool.push_back(RG_TRIFORCE_PIECE);
    }
    SPDLOG_INFO("MergeShip goal: Triforce Hunt over both worlds, {} pieces in the pool ({} were Hyrule's), {} required",
                std::max(have, TriforceTotal()), have, MergeShip_Goal_TriforceRequired());
}

std::string MergeShip_Goal_Name() {
    if (!Unified()) {
        return "ganon";
    }
    switch (Goal()) {
        case RO_MS_GOAL_MAJORA:
            return "majora";
        case RO_MS_GOAL_BOTH:
            return "ganon+majora";
        case RO_MS_GOAL_TRIFORCE:
            return "triforce";
        default:
            return "ganon";
    }
}

nlohmann::ordered_json MergeShip_Goal_SeedSection() {
    nlohmann::ordered_json goal;
    goal["type"] = MergeShip_Goal_Name();
    goal["msGoalType"] = MergeShip_Goal_SharedType();
    goal["hyrule"] = HyruleWinconIsGanon() ? "ganon" : "trigger";
    goal["hyruleText"] = HyrulePartText();
    goal["majoraRequires"] = { { "moonRemains", MoonRemains() },
                               { "moonMasks", MoonMasks() },
                               { "majoraRemains", MajoraRemains() },
                               { "majoraMasks", MajoraMasks() },
                               { "majoraTransformationMasks", Opt(RSK_MS_MAJORA_FORM_MASKS) } };
    goal["triforceTotal"] = Goal() == RO_MS_GOAL_TRIFORCE ? TriforceTotal() : 0;
    goal["triforceRequired"] = Goal() == RO_MS_GOAL_TRIFORCE ? MergeShip_Goal_TriforceRequired() : 0;
    goal["remainsAsMedallions"] = Opt(RSK_MS_REMAINS_AS_MEDALLIONS) != 0;
    goal["text"] = MergeShip_Goal_Describe();
    return goal;
}

void MergeShip_Goal_PatchMMOptions(nlohmann::ordered_json& mmOptions) {
    // The Triforce Hunt is the unified one: 2Ship never ends the game on its own count (GiveItem would at
    // RO_TRIFORCE_PIECES_REQUIRED) and never locks Majora behind its hunt (Souls.cpp); UnifiedGoal.cpp decides.
    mmOptions["RO_SHUFFLE_TRIFORCE_PIECES"] = 0u;
    mmOptions["RO_TRIFORCE_PIECES_MAX"] = (uint32_t)(Goal() == RO_MS_GOAL_TRIFORCE ? TriforceTotal() : 0);
    mmOptions["RO_TRIFORCE_PIECES_REQUIRED"] = 0xFFFFFFFFu;
}

// ---- in game -------------------------------------------------------------------------------------------------------

uint8_t MergeShip_Goal_TriforcePieces() {
    const uint8_t hyrule = gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected;
    if (!Unified()) {
        return hyrule;
    }
    // The ledger holds every piece found in either world; Hyrule's counter also has those the ledger gave it. Neither
    // counts one piece twice, so the larger is the count.
    return (uint8_t)std::min<int>(255, std::max<int>(hyrule, LedgerTriforcePieces(Shared())));
}

bool MergeShip_Goal_RuntimeMet() {
    if (!Unified()) {
        return false;
    }
    MsSharedSave* shared = Shared();
    WriteHyrulePart(shared);
    const bool met = GoalMet(shared, (shared->goalFlags & MS_GOAL_FLAG_HYRULE) != 0, MergeShip_Goal_TriforcePieces());
    if (met) {
        shared->goalFlags |= MS_GOAL_FLAG_COMPLETE;
    }
    return met;
}

void MergeShip_Goal_OnGanonDefeated() {
    if (!Unified()) {
        return;
    }
    if (MergeShip_Goal_RuntimeMet()) {
        return; // CheckTriggers gives the Triforce: the credits
    }
    std::string message = "Ganon is defeated! ";
    switch (Goal()) {
        case RO_MS_GOAL_BOTH:
            message += HyruleWinconIsGanon() ? "Majora still threatens Termina." : "The goal also needs: " +
                                                                                     HyrulePartText() + ".";
            break;
        case RO_MS_GOAL_MAJORA:
            message += "But the goal is Majora, in Termina.";
            break;
        case RO_MS_GOAL_TRIFORCE:
            message += "But the goal is the Triforce: " + std::to_string(MergeShip_Goal_TriforcePieces()) + " of " +
                       std::to_string(MergeShip_Goal_TriforceRequired()) + " pieces.";
            break;
        default:
            message += "The goal: " + MergeShip_Goal_Describe() + ".";
            break;
    }
    SPDLOG_INFO("MergeShip goal: {}", message);
    Notification::Emit({ .message = message, .remainingTime = 12.0f });
}

std::string MergeShip_Goal_Describe() {
    switch (Unified() ? Goal() : (uint8_t)RO_MS_GOAL_GANON) {
        case RO_MS_GOAL_MAJORA:
            return "defeat Majora in Termina (Majora needs " + MajoraRequirementText() + ")";
        case RO_MS_GOAL_BOTH:
            return HyrulePartText() + ", and Majora is defeated in Termina (Majora needs " + MajoraRequirementText() +
                   ")";
        case RO_MS_GOAL_TRIFORCE:
            return "find " + std::to_string(MergeShip_Goal_TriforceRequired()) + " of the " +
                   std::to_string(TriforceTotal()) + " Triforce pieces hidden in Hyrule and Termina";
        default:
            return HyrulePartText();
    }
}

CustomMessage MergeShip_Goal_AltarText() {
    std::string english;
    std::vector<std::string> colors;
    switch (Goal()) {
        case RO_MS_GOAL_MAJORA:
            english = "$wAnd the #Triforce# will be granted once #Majora# falls in #Termina#.^";
            colors = { QM_YELLOW, QM_RED, QM_BLUE };
            break;
        case RO_MS_GOAL_BOTH:
            if (HyruleWinconIsGanon()) {
                english = "$wAnd the #Triforce# will be granted once #Ganon# falls in Hyrule and #Majora# falls in "
                          "#Termina#.^";
                colors = { QM_YELLOW, QM_RED, QM_RED, QM_BLUE };
            } else {
                english = "$wAnd the #Triforce# will be granted once #" + HyrulePartText() + "# and #Majora# falls "
                          "in #Termina#.^";
                colors = { QM_YELLOW, QM_RED, QM_RED, QM_BLUE };
            }
            break;
        case RO_MS_GOAL_TRIFORCE:
            english = "$wAnd the #Triforce# will be granted once #" +
                      std::to_string(MergeShip_Goal_TriforceRequired()) + " Triforce pieces# are gathered from #Hyrule "
                      "and Termina#.^";
            colors = { QM_YELLOW, QM_YELLOW, QM_BLUE };
            break;
        default:
            english = "$wAnd the #Triforce# will be granted once #" + HyrulePartText() + "#.^";
            colors = { QM_YELLOW, QM_RED };
            break;
    }
    // One text for every language: the names are the same in all three.
    return CustomMessage(english, english, english, colors);
}

void MergeShip_Goal_Capture(MsSharedSave* shared) {
    if (shared == nullptr) {
        return;
    }
    shared->itemsCombined = MergeShip_Goal_CombineItemsInGame() ? 1 : 0;
    // The seed's Bombchu Bag and its mode: Termina's bombchus come only with it (2Ship MergeShip_Own_ChusLocked), and a
    // bag Termina gives counts as SoH's would (a single bag is the whole bag, 2Ship MergeShip_Own_UnlockMMChus).
    const bool chuBag = IS_RANDO && Unified() && (Opt(RSK_BOMBCHU_BAG) != RO_BOMBCHU_BAG_NONE);
    shared->hyruleChuBag = !chuBag                                           ? 0
                           : (Opt(RSK_BOMBCHU_BAG) == RO_BOMBCHU_BAG_SINGLE) ? MS_CHU_BAG_SINGLE
                                                                             : MS_CHU_BAG_PROGRESSIVE;
    // Hyrule's "Gossip Stone Hints", which Termina's gossip stones follow (BUG-0026; 2Ship Rando/ActorBehavior/EnGs.cpp).
    shared->gossipHints = (IS_RANDO && Unified()) ? (uint8_t)(MS_GOSSIP_HINTS_NONE + Opt(RSK_GOSSIP_STONE_HINTS))
                                                  : MS_GOSSIP_HINTS_UNSET;
    if (!IS_RANDO || !Unified()) {
        shared->goalType = MS_GOAL_NONE;
        return;
    }
    WriteHyrulePart(shared);
}

bool MergeShip_Goal_CombineItems() {
    return Unified() && (Opt(RSK_MS_COMBINE_SHARED_ITEMS) != 0);
}

bool MergeShip_Goal_CombineItemsInGame() {
    return IS_RANDO && MergeShip_Goal_CombineItems();
}

extern "C" uint8_t MergeShip_Goal_ExtraMedallions(void) {
    if (!IS_RANDO || !Unified() || !Opt(RSK_MS_REMAINS_AS_MEDALLIONS)) {
        return 0;
    }
    uint8_t count = 0;
    for (int bit = 0; bit < 4; bit++) {
        count += (gMergeShipSave.mmQuestItems >> bit) & 1;
    }
    return count;
}

// ---- test harness --------------------------------------------------------------------------------------------------

std::vector<std::string> MergeShip_Goal_Summary() {
    auto ctx = Rando::Context::GetInstance();
    std::vector<std::string> out;
    char buf[512];
    snprintf(buf, sizeof(buf),
             "settings unified=%d goal=%d(%s) ownsWincon=%d sohWincon=%d triforceTotal=%d triforceRequired=%d "
             "remainsAsMedallions=%d formMasks=%d moonRemains=%d moonMasks=%d majoraRemains=%d majoraMasks=%d",
             (int)Unified(), (int)Goal(), MergeShip_Goal_Name().c_str(), (int)MergeShip_Goal_OwnsWincon(),
             (int)ctx->GetOption(RSK_WINCON).Get(), TriforceTotal(), (int)MergeShip_Goal_TriforceRequired(),
             (int)Opt(RSK_MS_REMAINS_AS_MEDALLIONS), (int)Opt(RSK_MS_MAJORA_FORM_MASKS), MoonRemains(), MoonMasks(),
             MajoraRemains(), MajoraMasks());
    out.push_back(buf);

    const bool winconInPool =
        std::find(ctx->allLocations.begin(), ctx->allLocations.end(), RC_WINCON) != ctx->allLocations.end();
    auto itemName = [&ctx](RandomizerCheck rc) {
        return Rando::StaticData::RetrieveItem(ctx->GetItemLocation(rc)->GetPlacedRandomizerGet())
            .GetName()
            .GetEnglish();
    };
    snprintf(buf, sizeof(buf), "wincon ganon=\"%s\" wincon=\"%s\" winconInPool=%d triforceInPool=%d",
             itemName(RC_GANON).c_str(), itemName(RC_WINCON).c_str(), (int)winconInPool, CountPlaced({ RG_TRIFORCE }));
    out.push_back(buf);

    int piecesHyrule = 0, piecesTermina = 0;
    for (RandomizerCheck rc : ctx->allLocations) {
        if (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() == RG_TRIFORCE_PIECE) {
            (Rando::StaticData::GetLocation(rc)->GetWorld() == RW_TERMINA ? piecesTermina : piecesHyrule)++;
        }
    }
    snprintf(buf, sizeof(buf), "triforce pieces=%d hyrule=%d termina=%d", piecesHyrule + piecesTermina, piecesHyrule,
             piecesTermina);
    out.push_back(buf);

    snprintf(buf, sizeof(buf), "advancement remains=%d moonMask=%d formMask=%d placedRemains=%d placedMoonMasks=%d "
             "placedFormMasks=%d terminaLogicPorted=%d",
             (int)Rando::StaticData::RetrieveItem(RG_MM_ODOLWAS_REMAINS).IsAdvancement(),
             (int)Rando::StaticData::RetrieveItem(RG_MM_KEATON_MASK).IsAdvancement(),
             (int)Rando::StaticData::RetrieveItem(RG_MM_DEKU_MASK).IsAdvancement(), CountPlaced(kRemains),
             CountPlaced(kMoonMasks), CountPlaced(kFormMasks), (int)TerminaLogicPorted());
    out.push_back(buf);

    // The sphere of the playthrough that holds the Triforce (the goal's last step), and the last sphere's checks.
    int triforceSphere = -1;
    for (size_t i = 0; i < ctx->playthroughLocations.size(); i++) {
        for (RandomizerCheck rc : ctx->playthroughLocations[i]) {
            if (ctx->GetItemLocation(rc)->GetPlacedRandomizerGet() == RG_TRIFORCE) {
                triforceSphere = (int)i;
            }
        }
    }
    snprintf(buf, sizeof(buf), "playthrough spheres=%d triforceSphere=%d", (int)ctx->playthroughLocations.size(),
             triforceSphere);
    out.push_back(buf);
    if (!ctx->playthroughLocations.empty()) {
        for (RandomizerCheck rc : ctx->playthroughLocations.back()) {
            snprintf(buf, sizeof(buf), "lastSphere \"%s\" = %s", Rando::StaticData::GetLocation(rc)->GetName().c_str(),
                     Rando::StaticData::RetrieveItem(ctx->GetItemLocation(rc)->GetPlacedRandomizerGet())
                         .GetName()
                         .GetEnglish()
                         .c_str());
            out.push_back(buf);
        }
    }
    out.push_back("text " + MergeShip_Goal_Describe());
    out.push_back("altar " + MergeShip_Goal_AltarText().GetEnglish(MF_RAW));
    return out;
}

std::vector<std::string> MergeShip_Goal_TestCommand(const std::vector<std::string>& args) {
    std::vector<std::string> out;
    const std::string op = args.empty() ? "state" : args[0];
    MsSharedSave* shared = Shared();
    if (Unified()) {
        WriteHyrulePart(shared); // the seed's goal first, so what follows is not cleared as another seed's
    } else {
        shared->goalType = MS_GOAL_NONE;
    }
    if (op == "majora") {
        // What 2Ship writes when Majora falls.
        shared->goalFlags |= MS_GOAL_FLAG_MAJORA;
    } else if (op == "clear") {
        shared->goalFlags = 0;
        Flags_UnsetRandomizerInf(RAND_INF_DUNGEONS_DONE_GANONS_TOWER);
    } else if (op == "ganon") {
        // What VB_SLAY_GANON does when Ganon is not the whole goal.
        Flags_SetRandomizerInf(RAND_INF_DUNGEONS_DONE_GANONS_TOWER);
        MergeShip_Goal_OnGanonDefeated();
    } else if (op == "pieces" && args.size() > 1) {
        gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected = (uint8_t)atoi(args[1].c_str());
    } else if (op == "check") {
        // hook_handlers.cpp's triggers: RC_WINCON is queued (and the Triforce given, the credits) once the goal is met.
        CheckTriggers();
    } else if (op == "capture") {
        MergeShip_Goal_Capture(shared);
    }
    const bool hyruleMet = Unified() && HyruleRuntimeMet();
    const bool met = MergeShip_Goal_RuntimeMet(); // writes Hyrule's part (and COMPLETE when met) first
    char buf[400];
    snprintf(buf, sizeof(buf),
             "%s hosted=%d unified=%d goal=%s ownsWincon=%d sharedType=%d flags=0x%02X seed=%u formMasks=%d "
             "triforceRequired=%d pieces=%d (hyrule counter %d, ledger %d) hyruleMet=%d met=%d",
             op.c_str(), (int)(MergeShipHost_SharedSave() != nullptr), (int)Unified(), MergeShip_Goal_Name().c_str(),
             (int)MergeShip_Goal_OwnsWincon(), (int)shared->goalType, (int)shared->goalFlags, shared->goalSeed,
             (int)shared->majoraFormMasks, (int)shared->triforceRequired, (int)MergeShip_Goal_TriforcePieces(),
             (int)gSaveContext.ship.quest.data.randomizer.triforcePiecesCollected,
             (int)LedgerTriforcePieces(shared), (int)hyruleMet, (int)met);
    out.push_back(buf);
    return out;
}
