// MergeShip2 -- Ocarina of Time's items in 2Ship: the test harness's commands for them (MergeShipTest.cpp) and a
// per-frame trace of the item state. See MergeShipOoTItems.h.

#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>

#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "2s2h/MergeShip/MergeShipOoTSpells.h"
extern PlayState* gPlayState;
int MergeShipTest_IsActive(void);
}

namespace {

bool sTrace = false;

long Arg(const std::vector<std::string>& args, size_t i) {
    return (i < args.size()) ? strtol(args[i].c_str(), nullptr, 0) : 0;
}

void LogState(const char* tag) {
    const MergeShipOoTItemsSave& s = gSaveContext.save.shipSaveInfo.mergeShipOoTItems;
    PlayState* play = gPlayState;
    Player* player = (play != nullptr) ? GET_PLAYER(play) : nullptr;
    const RespawnData* top = MergeShip_OoTItems_FaroresTop();
    std::string cells;

    for (s32 slot = 0; slot < MS_OOT_SLOT_MAX; slot++) {
        u8 item = MergeShip_OoTItems_GetSlotItem(slot);
        if (item != ITEM_NONE) {
            char buf[48];
            snprintf(buf, sizeof(buf), " %d:%s", slot, MergeShip_OoTItems_Name(item));
            cells += buf;
        }
    }
    MergeShipTest_Log("%s enabled=%d owned=0x%08X seeds=%d/%d bag=%d mask=0x%02X fw=%d top.data=%d nayru=%d "
                      "magic=%d state=%d cells:%s",
                      tag, MergeShip_OoTItems_Enabled(), s.owned, s.seeds, MergeShip_OoTItems_SeedCapacity(),
                      s.bulletBag, s.wornMask, s.faroresWindSet, top->data, gMergeShipNayrusLoveTimer,
                      gSaveContext.save.saveInfo.playerData.magic, gSaveContext.magicState, cells.c_str());
    MergeShipTest_Log("%s buttons C=%02X/%02X/%02X slots=%02X/%02X/%02X D=%02X/%02X/%02X/%02X bottles=%02X %02X %02X "
                      "%02X %02X %02X",
                      tag, BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN),
                      BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT), C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT),
                      C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN), C_SLOT_EQUIP(0, EQUIP_SLOT_C_RIGHT),
                      DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_RIGHT), DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_LEFT),
                      DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_DOWN), DPAD_BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_D_UP),
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1],
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_2],
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_3],
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_4],
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_5],
                      gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_6]);
    if (player != nullptr) {
        MergeShipTest_Log("%s player form=%d pos=(%.1f,%.1f,%.1f) yaw=%d held=0x%02X ia=%d heldIa=%d exch=%d "
                          "s1=%08X s2=%08X s3=%08X mwa=%d mws=%d mask=%d boom=%p hp=%d",
                          tag, player->transformation, player->actor.world.pos.x, player->actor.world.pos.y,
                          player->actor.world.pos.z, player->actor.shape.rot.y, player->heldItemId, player->itemAction,
                          player->heldItemAction, player->exchangeItemAction, player->stateFlags1, player->stateFlags2,
                          player->stateFlags3, player->meleeWeaponAnimation, player->meleeWeaponState,
                          player->currentMask, (void*)player->zoraBoomerangActor,
                          gSaveContext.save.saveInfo.playerData.health);
    }
}

void EquipButton(s32 btn, u8 item) {
    s32 slot = MergeShip_OoTItems_SlotOfItem(item);
    u8 buttonSlot = (slot >= 0) ? MS_OOT_BUTTON_SLOT(slot) : SLOT_NONE;

    // Ruto's Letter is in one of MM's bottles: that bottle's slot.
    for (s32 i = SLOT_BOTTLE_1; (slot < 0) && (i <= SLOT_BOTTLE_6); i++) {
        if (gSaveContext.save.saveInfo.inventory.items[i] == item) {
            buttonSlot = i;
            break;
        }
    }

    if (btn <= EQUIP_SLOT_C_RIGHT) {
        BUTTON_ITEM_EQUIP(0, btn) = item;
        C_SLOT_EQUIP(0, btn) = buttonSlot;
        if (gPlayState != nullptr) {
            Interface_LoadItemIconImpl(gPlayState, btn);
        }
    } else {
        DPAD_BUTTON_ITEM_EQUIP(0, btn) = item;
        DPAD_SLOT_EQUIP(0, btn) = buttonSlot;
        if (gPlayState != nullptr) {
            Interface_Dpad_LoadItemIconImpl(gPlayState, btn);
        }
    }
    MergeShipTest_Log("OOTBUTTON %d item=0x%02X (%s) slot=0x%02X", btn, item, MergeShip_OoTItems_Name(item),
                      buttonSlot);
}

} // namespace

// Harness commands (MergeShipTest.cpp's command chain):
//   ootgive <item>          MergeShip_OoTItems_Give (ITEM_OOT_*, 0x0B Slingshot, 0x11 Longshot, 0xF9 seeds)
//   ootgetitem <item>       MergeShip_OoTItems_GiveWithCutscene (Link holds it up, OoT's text)
//   ootbutton <btn> <item>  put an OoT item on C-left(1)/C-down(2)/C-right(3) or D-pad right(4)..up(7), with its
//                           Items (Hyrule) slot
//   ootmagic <n>            magic meter on (single) and filled to n
//   ootstate [tag]          log the OoT items' state, the buttons and the player
//   oottrace on|off         ootstate every frame
//   ootspawn <id> <dist> [params]  spawn an actor dist in front of the player
//   ootcycle <slot>         A on an Items (Hyrule) trade cell
//   ootcvar <int>           gEnhancements.MergeShip.OoTItems for this run
bool MergeShipTest_OoTItemsCommand(const std::string& op, const std::vector<std::string>& args) {
    PlayState* play = gPlayState;

    if (op == "ootgive") {
        u8 item = (u8)Arg(args, 0);
        u8 r = (play != nullptr) ? ((item == ITEM_LONGSHOT) ? MergeShip_OoTItems_Give(play, item) : Item_Give(play, item))
                                 : ITEM_NONE;
        MergeShipTest_Log("OOTGIVE 0x%02X (%s) -> 0x%02X", item, MergeShip_OoTItems_Name(item), r);
        return true;
    }
    if (op == "ootgetitem") {
        MergeShip_OoTItems_GiveWithCutscene((u8)Arg(args, 0));
        MergeShipTest_Log("OOTGETITEM 0x%02X queued", (u8)Arg(args, 0));
        return true;
    }
    if (op == "ootbutton") {
        EquipButton((s32)Arg(args, 0), (u8)Arg(args, 1));
        return true;
    }
    if (op == "ootmagic") {
        gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
        gSaveContext.save.saveInfo.playerData.magicLevel = 1;
        gSaveContext.magicCapacity = 0x30;
        gSaveContext.save.saveInfo.playerData.magic = (s8)Arg(args, 0);
        MergeShipTest_Log("OOTMAGIC %d", gSaveContext.save.saveInfo.playerData.magic);
        return true;
    }
    if (op == "ootstate") {
        LogState(args.empty() ? "OOTSTATE" : args[0].c_str());
        return true;
    }
    if (op == "oottrace") {
        sTrace = !args.empty() && (args[0] == "on");
        return true;
    }
    if (op == "ootspawn") {
        if (play != nullptr) {
            Player* player = GET_PLAYER(play);
            f32 dist = (f32)Arg(args, 1);
            Actor* a = Actor_Spawn(&play->actorCtx, play, (s16)Arg(args, 0),
                                   player->actor.world.pos.x + Math_SinS(player->actor.shape.rot.y) * dist,
                                   player->actor.world.pos.y,
                                   player->actor.world.pos.z + Math_CosS(player->actor.shape.rot.y) * dist, 0,
                                   player->actor.shape.rot.y + 0x8000, 0, (s16)Arg(args, 2));
            MergeShipTest_Log("OOTSPAWN 0x%X -> %p", (s32)Arg(args, 0), (void*)a);
        }
        return true;
    }
    if (op == "ootcycle") {
        MergeShip_OoTItems_CycleTradeSlot((s32)Arg(args, 0));
        LogState("OOTCYCLE");
        return true;
    }
    if (op == "ootcvar") {
        CVarSetInteger(MERGESHIP_OOT_ITEMS_CVAR, (s32)Arg(args, 0));
        MergeShipTest_Log("OOTCVAR %d", (s32)Arg(args, 0));
        return true;
    }
    return false;
}

static void RegisterMergeShipOoTItems() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateUpdate>([]() {
        MergeShip_OoTItems_UpdateOcarinaLook(gPlayState); // BUG-0031
        if (sTrace && MergeShipTest_IsActive() && (gPlayState != nullptr)) {
            LogState("T");
        }
    });
}

static RegisterShipInitFunc sMergeShipOoTItemsInit(RegisterMergeShipOoTItems, {});

// ---- SoH's Roc's Feather (BUG-0014) ---------------------------------------------------------------------------------------
//
// Its icon and name are SoH's own (soh.o2r, textures/icon_item_static/gRocsFeatherTex and
// textures/item_name_static/gRocsFeatherItemNameENGTex): read from that archive file (Termina does not mount it, as the
// transformation scream reads mergeship2.o2r, MergeShipAdultLink.cpp) and mounted under mergeship/rocs/.
#include <mergeship/AdultMask.hpp> // MsAdultMask::MemoryArchive
#include <ship/resource/archive/O2rArchive.h>
#include <filesystem>

// BUG-0161: and its jump, SoH's own animation (objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump and its _Data,
// which the header names): mounted at its own paths, which Majora's Mask's gameplay_keep does not have.
extern "C" s32 gMergeShipRocsJumpMounted;
s32 gMergeShipRocsJumpMounted = false;

extern "C" void MergeShip_OoTItems_MountRocsFeather(void) {
    static const struct {
        const char* from;
        const char* to;
        bool jump;
    } kFiles[] = {
        { "textures/icon_item_static/gRocsFeatherTex", "mergeship/rocs/gRocsFeatherTex", false },
        { "textures/item_name_static/gRocsFeatherItemNameENGTex", "mergeship/rocs/gRocsFeatherItemNameENGTex", false },
        { "objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump",
          "objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump", true },
        { "objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump_Data",
          "objects/gameplay_keep/gPlayerAnim_link_rocs_feather_jump_Data", true },
    };
    const std::string path = Ship::Context::LocateFileAcrossAppDirs("soh.o2r", "2ship");
    if (!std::filesystem::exists(path)) {
        SPDLOG_WARN("MergeShip: soh.o2r not found; the Roc's Feather has no icon in Termina");
        return;
    }
    auto soh = std::make_shared<Ship::O2rArchive>(path);
    soh->Load();
    if (!soh->IsLoaded()) {
        SPDLOG_ERROR("MergeShip: could not open {}", path);
        return;
    }
    std::map<std::string, std::vector<char>> files;
    s32 jumpFiles = 0;
    for (const auto& f : kFiles) {
        std::shared_ptr<Ship::File> file = soh->LoadFile(std::string(f.from));
        if ((file == nullptr) || (file->Buffer == nullptr)) {
            SPDLOG_ERROR("MergeShip: {} is missing from soh.o2r", f.from);
            if (f.jump) {
                continue; // (the hop falls back to Majora's Mask's jump)
            }
            return;
        }
        files[f.to] = std::vector<char>(file->Buffer->begin(), file->Buffer->end());
        jumpFiles += f.jump ? 1 : 0;
    }
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    uint32_t gameVersion = 0;
    for (const auto& mounted : *archives->GetArchives()) {
        if (mounted->HasGameVersion()) {
            gameVersion = mounted->GetGameVersion();
            break;
        }
    }
    auto archive = std::make_shared<MsAdultMask::MemoryArchive>(std::move(files), gameVersion);
    archive->Load();
    if (!archive->IsLoaded() || (archives->AddArchive(archive) == nullptr)) {
        SPDLOG_ERROR("MergeShip: the Roc's Feather's icon could not be mounted");
        return;
    }
    gMergeShipRocsJumpMounted = (jumpFiles == 2);
    MergeShipTest_Log("OOTITEMS Roc's Feather icon and name mounted from %s (jump %d)", path.c_str(),
                      gMergeShipRocsJumpMounted);
}

// BUG-0031: Hyrule's form instrument icons (mergeship2.o2r, which Termina does not mount), at their own paths.
extern "C" s32 gMergeShipInstrumentIconsMounted;
extern "C" void MergeShip_OoTItems_MountInstrumentIcons(void) {
    static const char* kFiles[] = { "mergeship2/textures/gItemIconDekuPipesTex",
                                    "mergeship2/textures/gItemIconGoronDrumsTex",
                                    "mergeship2/textures/gItemIconZoraGuitarTex" };
    const std::string path = Ship::Context::LocateFileAcrossAppDirs("mergeship2.o2r", "2ship");
    if (!std::filesystem::exists(path)) {
        SPDLOG_WARN("MergeShip: mergeship2.o2r not found; no form instrument icons in Termina");
        return;
    }
    auto ms2 = std::make_shared<Ship::O2rArchive>(path);
    ms2->Load();
    if (!ms2->IsLoaded()) {
        return;
    }
    std::map<std::string, std::vector<char>> files;
    for (const char* f : kFiles) {
        std::shared_ptr<Ship::File> file = ms2->LoadFile(std::string(f));
        if ((file == nullptr) || (file->Buffer == nullptr)) {
            SPDLOG_ERROR("MergeShip: {} is missing from mergeship2.o2r", f);
            return;
        }
        files[f] = std::vector<char>(file->Buffer->begin(), file->Buffer->end());
    }
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    uint32_t gameVersion = 0;
    for (const auto& mounted : *archives->GetArchives()) {
        if (mounted->HasGameVersion()) {
            gameVersion = mounted->GetGameVersion();
            break;
        }
    }
    auto archive = std::make_shared<MsAdultMask::MemoryArchive>(std::move(files), gameVersion);
    archive->Load();
    if (!archive->IsLoaded() || (archives->AddArchive(archive) == nullptr)) {
        return;
    }
    gMergeShipInstrumentIconsMounted = true;
    MergeShipTest_Log("OOTITEMS form instrument icons mounted from %s", path.c_str());
}

// ---- Ocarina of Time's HUD textures in Termina (BUG-0013, include/mergeship/HudSwap.hpp) --------------------------------
//
// The C/B button backgrounds, the hearts, the rupee counter's icon and Link's head on the map (Majora's Mask's
// gQuestIconLinkHumanFaceTex, which the dungeon map and the quest page use), each an option ("Enhancements" >
// "Mergeship"). Hyrule's side is Shipwright's MergeShipHudSwap.cpp. 2Ship asks for its own textures without the "mm/"
// its archive keeps them under, so each is mounted at both paths.
#include <mergeship/HudSwap.hpp>
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"

namespace {

#define MS_HUD_TEX(name)                                                         \
    { "parameter_static/" name, "textures/parameter_static/" name },             \
    {                                                                            \
        "mm/parameter_static/" name, "textures/parameter_static/" name           \
    }

const MsHudSwap::Swap kMsHudButtons[] = { MS_HUD_TEX("gButtonBackgroundTex") };
const MsHudSwap::Swap kMsHudHearts[] = {
    MS_HUD_TEX("gHeartFullTex"),         MS_HUD_TEX("gHeartEmptyTex"),        MS_HUD_TEX("gHeartHalfTex"),
    MS_HUD_TEX("gHeartQuarterTex"),      MS_HUD_TEX("gHeartThreeQuarterTex"), MS_HUD_TEX("gDefenseHeartFullTex"),
    MS_HUD_TEX("gDefenseHeartEmptyTex"), MS_HUD_TEX("gDefenseHeartHalfTex"),  MS_HUD_TEX("gDefenseHeartQuarterTex"),
    MS_HUD_TEX("gDefenseHeartThreeQuarterTex"),
};
const MsHudSwap::Swap kMsHudRupees[] = { MS_HUD_TEX("gRupeeCounterIconTex") };
const MsHudSwap::Swap kMsHudMapLink[] = {
    { "icon_item_24_static_yar/gQuestIconLinkHumanFaceTex", "textures/icon_item_dungeon_static/gDungeonMapLinkHeadTex" },
    { "mm/icon_item_24_static_yar/gQuestIconLinkHumanFaceTex",
      "textures/icon_item_dungeon_static/gDungeonMapLinkHeadTex" },
};
const MsHudSwap::Group kMsHudGroups[] = {
    { kMsHudButtons, sizeof(kMsHudButtons) / sizeof(kMsHudButtons[0]) },
    { kMsHudHearts, sizeof(kMsHudHearts) / sizeof(kMsHudHearts[0]) },
    { kMsHudRupees, sizeof(kMsHudRupees) / sizeof(kMsHudRupees[0]) },
    { kMsHudMapLink, sizeof(kMsHudMapLink) / sizeof(kMsHudMapLink[0]) },
};
const char* const kMsHudCVars[] = {
    "gEnhancements.MergeShip.OoTHud.Buttons",
    "gEnhancements.MergeShip.OoTHud.Hearts",
    "gEnhancements.MergeShip.OoTHud.Rupees",
    "gEnhancements.MergeShip.OoTHud.MapLink",
};
std::shared_ptr<MsAdultMask::MemoryArchive> sMsHudMounted = nullptr;
uint32_t sMsHudApplied = 0;

void RegisterOoTHud() {
    COND_HOOK(OnGameStateUpdate, true, []() {
        uint32_t mask = 0;
        for (size_t i = 0; i < sizeof(kMsHudCVars) / sizeof(kMsHudCVars[0]); i++) {
            if (CVarGetInteger(kMsHudCVars[i], 0)) {
                mask |= 1u << i;
            }
        }
        if (mask == sMsHudApplied) {
            return;
        }
        sMsHudMounted = MsHudSwap::Apply(kMsHudGroups, sizeof(kMsHudGroups) / sizeof(kMsHudGroups[0]), mask,
                                         "oot.o2r", "2ship", sMsHudMounted, "mergeship-hud (Ocarina of Time)");
        sMsHudApplied = mask;
        MergeShipTest_Log("HUDSWAP Ocarina of Time textures mask=0x%X mounted=%d", mask,
                          (sMsHudMounted != nullptr) ? 1 : 0);
    });
}

RegisterShipInitFunc sMsOoTHudInit(RegisterOoTHud, {});

} // namespace

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
using namespace UIWidgets;

static void RegisterOoTHudMenu() {
    WidgetPath path = { "Enhancements", "Mergeship", SECTION_COLUMN_1 };
    mBenMenu->AddWidget(path, "OoT Button Textures", WIDGET_CVAR_CHECKBOX)
        .CVar(kMsHudCVars[0])
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The B and C buttons are drawn with Ocarina of Time's button texture."));
    mBenMenu->AddWidget(path, "OoT Heart Textures", WIDGET_CVAR_CHECKBOX)
        .CVar(kMsHudCVars[1])
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The life meter's hearts (and Double Defense's) are Ocarina of Time's."));
    mBenMenu->AddWidget(path, "OoT Rupee Icon", WIDGET_CVAR_CHECKBOX)
        .CVar(kMsHudCVars[2])
        .Options(CheckboxOptions().DefaultValue(false).Tooltip("The rupee counter's icon is Ocarina of Time's."));
    mBenMenu->AddWidget(path, "OoT Link Icon On The Map", WIDGET_CVAR_CHECKBOX)
        .CVar(kMsHudCVars[3])
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "The pause screen's dungeon map (and the quest page) mark Link with Ocarina of Time's head icon."));
}

static RegisterMenuInitFunc sOoTHudMenuInit(RegisterOoTHudMenu);
} // namespace BenGui
