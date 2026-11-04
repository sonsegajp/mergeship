// MergeShip2 -- Ocarina of Time's equipment in Majora's Mask: display list surgery, the tunic colour, the get-item
// cutscene and texts, the hooks and the test harness commands. See MergeShipOoTEquip.h.

#include "2s2h/MergeShip/MergeShipOwnership.h"
#include "MergeShipOoTEquip.h"
#include "MergeShipAssets.h"
#include "MergeShipOoTItems.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/BenPort.h"

#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <fast/resource/type/DisplayList.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, int index, Gfx instruction);
void ResourceMgr_UnpatchGfxByName(const char* path, const char* patchName);
}

#define OOT_EQUIP (gSaveContext.save.shipSaveInfo.mergeShipOoTEquip)

// ---- display list surgery ---------------------------------------------------------------------------------------------
//
// Ocarina of Time's adult lists draw the item and the adult's gloved fist together; a list is copied once, and the
// triangles drawn while one of the hand's (or a scabbard's) textures is loaded become no-ops, so MM's own bare fist can
// hold the item. Lists that load a limb matrix from segment 0x0D (boots, gauntlets, bracelet: OoT draws them after the
// skeleton, with the skeleton's matrices still in 0x0D) load it from segment 0x0E instead, which the caller points at
// the fitted matrix of the limb it is drawing on.

namespace {

std::unordered_map<std::string, std::vector<Gfx>> sRewrittenDLs;

bool IsHandTexture(const std::string& name) {
    return (name.find("Gauntlet") != std::string::npos) || (name.find("ClosedHand") != std::string::npos) ||
           (name.find("gLinkAdultHandTex") != std::string::npos) || (name.find("UnusedHand") != std::string::npos);
}

bool IsSheathTexture(const std::string& name) {
    return name.find("Sheath") != std::string::npos;
}

bool IsExpanded(u8 opcode) {
    // LUS ResourceFactoryBinaryDisplayListV0: these are 128-bit commands (the second word is a hash).
    return (opcode == G_SETTIMG_OTR_HASH) || (opcode == G_DL_OTR_HASH) || (opcode == G_VTX_OTR_HASH) ||
           (opcode == G_BRANCH_Z_OTR) || (opcode == G_MARKER) || (opcode == G_MTX_OTR) || (opcode == G_MOVEMEM_OTR);
}

} // namespace

extern "C" Gfx* MergeShip_OoTEquip_RewriteDL(const char* path, u32 flags) {
    std::string key = std::string(path) + "#" + std::to_string(flags);
    auto found = sRewrittenDLs.find(key);
    if (found != sRewrittenDLs.end()) {
        return found->second.data();
    }

    const char* resPath = (strncmp(path, "__OTR__", 7) == 0) ? path + 7 : path;
    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto dl = std::dynamic_pointer_cast<Fast::DisplayList>(resourceManager->LoadResource(resPath));
    if ((dl == nullptr) || dl->Instructions.empty()) {
        SPDLOG_ERROR("MergeShip: OoT equipment list {} failed to load", resPath);
        return (Gfx*)path;
    }

    std::vector<Gfx> out(dl->Instructions.begin(), dl->Instructions.end());
    auto archiveManager = resourceManager->GetArchiveManager();
    bool dropping = false;
    size_t dropped = 0;
    size_t remapped = 0;

    for (size_t i = 0; i < out.size(); i++) {
        u8 opcode = (u8)(out[i].words.w0 >> 24);

        if (opcode == G_SETTIMG_OTR_HASH && (i + 1 < out.size())) {
            uint64_t hash = ((uint64_t)out[i + 1].words.w0 << 32) | (uint64_t)out[i + 1].words.w1;
            const std::string* name = archiveManager->HashToString(hash);
            // A palette load keeps the texture's state; any other texture decides whether what follows is dropped.
            if ((name != nullptr) && (name->find("TLUT") == std::string::npos)) {
                dropping = ((flags & MS_OOTEQ_DL_STRIP_HAND) && IsHandTexture(*name)) ||
                           ((flags & MS_OOTEQ_DL_STRIP_SHEATH) && IsSheathTexture(*name));
            }
        } else if (dropping && ((opcode == 0x05) || (opcode == 0x06) || (opcode == 0x07) || (opcode == G_TRI1_OTR))) {
            // F3DEX2 G_TRI1 / G_TRI2 / G_QUAD, LUS G_TRI1_OTR
            out[i].words.w0 = 0; // F3DEX2 G_NOOP
            out[i].words.w1 = 0;
            dropped++;
        } else if ((flags & MS_OOTEQ_DL_REMAP_0D) && (opcode == 0xDA) && ((out[i].words.w1 >> 24) == 0x0D) &&
                   (out[i].words.w1 & 1)) {
            out[i].words.w1 = 0x0E000001; // F3DEX2 G_MTX from segment 0x0E, offset 0
            remapped++;
        }
        if (IsExpanded(opcode)) {
            i++;
        }
    }

    SPDLOG_INFO("MergeShip: {} rewritten (flags {}): {} triangle commands dropped, {} matrices remapped", resPath,
                flags, dropped, remapped);
    MergeShipTest_Log("OOTEQ DL %s flags=%u dropped=%zu remapped=%zu size=%zu", resPath, flags, dropped, remapped,
                      out.size());
    auto& stored = sRewrittenDLs[key];
    stored = std::move(out);
    return stored.data();
}

// MergeShip2: a copy of a list cut where a texture starts being loaded (include/mergeship/DlCut.hpp).
#include <mergeship/DlCut.hpp>

extern "C" Gfx* MergeShip_OoTEquip_CutDLBeforeTexture(const char* path, const char* texture) {
    size_t cut = 0;
    Gfx* dl = (Gfx*)MsDl_CutBeforeTexture(path, texture, &cut);
    static bool sLogged = false;
    if (!sLogged) {
        sLogged = true;
        MergeShipTest_Log("DLCUT %s before %s: %s at %zu", path, texture, (dl != nullptr) ? "cut" : "not found", cut);
    }
    return dl;
}

// ---- the tunic's colour --------------------------------------------------------------------------------------------------
//
// MM's human sets its tunic colour inline (gsDPSetPrimColor) in eleven lists; 2S2H's cosmetic editor patches those same
// instructions for its "Human Tunic" colour (CosmeticEditor.cpp). The Goron and Zora Tunics patch them the same way, under
// their own patch name, with Ocarina of Time's colours (OoT sTunicColors).

namespace {

Gfx sTunicColorDL[] = {
    gsDPSetPrimColor(0, 0, 30, 105, 27, 255),
    gsDPPipeSync(),
    gsSPEndDisplayList(),
};

struct TunicPatch {
    const char* path;
    const char* name;
    int index;
};

const TunicPatch sTunicPatches[] = {
    { "objects/object_link_child/gLinkHumanWaistDL", "msOoTTunic", 5 },
    { "objects/object_link_child/gLinkHumanRightThighDL", "msOoTTunic", 10 },
    { "objects/object_link_child/gLinkHumanLeftThighDL", "msOoTTunic", 10 },
    { "objects/object_link_child/gLinkHumanHeadDL", "msOoTTunic", 92 },
    { "objects/object_link_child/gLinkHumanHatDL", "msOoTTunic", 10 },
    { "objects/object_link_child/gLinkHumanCollarDL", "msOoTTunic", 5 },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", "msOoTTunic1", 10 },
    { "objects/object_link_child/gLinkHumanLeftShoulderDL", "msOoTTunic2", 65 },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", "msOoTTunic1", 10 },
    { "objects/object_link_child/gLinkHumanRightShoulderDL", "msOoTTunic2", 65 },
    { "objects/object_link_child/gLinkHumanTorsoDL", "msOoTTunic", 5 },
};

s32 sPatchedTunic = 0;

} // namespace

extern "C" void MergeShip_OoTEquip_UpdateTunicColor(s32 tunic) {
    // OoT sTunicColors: Goron { 100, 20, 0 }, Zora { 0, 60, 100 }.
    static const u8 sColors[3][3] = { { 30, 105, 27 }, { 100, 20, 0 }, { 0, 60, 100 } };

    if ((tunic < 0) || (tunic > 2) || (tunic == sPatchedTunic)) {
        return;
    }
    if (tunic == 0) {
        for (const auto& patch : sTunicPatches) {
            ResourceMgr_UnpatchGfxByName(patch.path, patch.name);
        }
    } else {
        sTunicColorDL[0] = gsDPSetPrimColor(0, 0, sColors[tunic][0], sColors[tunic][1], sColors[tunic][2], 255);
        if (sPatchedTunic == 0) {
            for (const auto& patch : sTunicPatches) {
                ResourceMgr_PatchGfxByName(patch.path, patch.name, patch.index, gsSPDisplayList(sTunicColorDL));
            }
        }
    }
    sPatchedTunic = tunic;
    MergeShipTest_Log("OOTEQ tunic colour %d", tunic);
}

// ---- texts ----------------------------------------------------------------------------------------------------------------
//
// Ocarina of Time's English get-item texts (nes_message_data_static), converted to MM's message codes: OoT's colours
// (0x05 X) to MM's, its instant-text markers (0x08 / 0x09) to MM's 0x17 / 0x18, its line and box breaks (0x01 / 0x04) to
// MM's 0x11 / 0x10, its button glyphs (0x9F..) to MM's (0xB0..) and its end (0x02) to MM's 0xBF; its item icon code
// becomes the header's icon. The Master Sword, the Kokiri Tunic and Boots and the broken knife have no get-item text in
// Ocarina of Time (the first is pulled in a cutscene, the others are never given); theirs are written in its style.

namespace {

struct OoTEquipText {
    u8 item;
    const char* text;
    size_t length;
};

#define MS_TEXT(s) s, sizeof(s) - 1

const OoTEquipText sGetItemTexts[] = {
    // OoT message 0x00A4 (the Kokiri Sword in the Kokiri Forest's treasure chest)
    { ITEM_OOT_SWORD_KOKIRI, MS_TEXT("\x17""You got the \x01""Kokiri Sword\x00""!\x18""\x11""This is a hidden treasure of the "
                                     "\x11""Kokiri, but you can borrow it for a\x11""while.\xBF") },
    { ITEM_OOT_SWORD_MASTER, MS_TEXT("\x17""You got the \x01""Master Sword\x00""!\x18""\x11""The Master Sword, the evil-sealing "
                                     "\x11""blade, is yours to wield.\xBF") },
    // OoT message 0x0C
    { ITEM_OOT_SWORD_BIGGORON,
      MS_TEXT("\x17""You handed in the Claim Check\x11""and got the \x01""Biggoron's Sword\x00""!\x18""\x11""This blade was forged "
              "by a \x11""master smith and won't break!\xBF") },
    // OoT message 0x4B
    { ITEM_OOT_SWORD_GIANTS_KNIFE,
      MS_TEXT("\x17""You got the \x02""Giant's Knife\x00""!\x18""\x11""Hold it with both hands and use \x11""\x02""\xB1"" \x00""to "
              "attack! It's so long, you\x11""can't use it with a \x05""shield\x00"".\xBF") },
    { ITEM_OOT_SWORD_BROKEN, MS_TEXT("\x17""You got the \x01""Broken Giant's Knife\x00""!\x18""\xBF") },
    // OoT message 0x4C
    { ITEM_OOT_SHIELD_DEKU,
      MS_TEXT("\x17""You got a \x05""Deku Shield\x00""!\x18""\x11""Switch to the \x03""Equipment \x11""Subscreen\x01"" \x00""and select "
              "the\x11""shield. Press \x03""\xB0"" \x00""to equip it.\x10""Press \x05""\xB4""\x00"" to crouch and\x11""defend. If you "
              "press \x05""\xB4""\x00"" while \x11""\x05""\xB5"" Targeting\x00"", you can move\x11""while defending.\xBF") },
    // OoT message 0x4D
    { ITEM_OOT_SHIELD_HYLIAN,
      MS_TEXT("\x17""You got a \x05""Hylian Shield\x00""!\x18""\x11""Switch to the \x03""Equipment \x11""Subscreen\x00"" and select "
              "this\x11""shield, then equip it with \x03""\xB0""\x00"".\xBF") },
    // OoT message 0x4E
    { ITEM_OOT_SHIELD_MIRROR,
      MS_TEXT("\x17""You found the \x05""Mirror Shield\x00""!\x18""\x11""The shield's polished surface can\x11""reflect light or "
              "energy. Press \x05""\xB4""\x11""\x00""to use it.\xBF") },
    { ITEM_OOT_TUNIC_KOKIRI, MS_TEXT("\x17""You got a \x02""Kokiri Tunic\x00""!\x18""\xBF") },
    // OoT message 0x50
    { ITEM_OOT_TUNIC_GORON,
      MS_TEXT("\x17""You got a \x01""Goron Tunic\x00""!\x18""\x11""This heat-resistant tunic is\x11""adult size, so it won't fit a "
              "kid...\x11""Going to a hot place? No worry!\xBF") },
    // OoT message 0x51
    { ITEM_OOT_TUNIC_ZORA,
      MS_TEXT("\x17""You got a \x03""Zora Tunic\x00""!\x18""\x11""This diving suit is adult size,\x11""so it won't fit a kid. Wear "
              "it,\x11""and you won't drown underwater.\xBF") },
    { ITEM_OOT_BOOTS_KOKIRI, MS_TEXT("\x17""You got the \x01""Kokiri Boots\x00""!\x18""\xBF") },
    // OoT message 0x53
    { ITEM_OOT_BOOTS_IRON,
      MS_TEXT("\x17""You got the \x01""Iron Boots\x00""!\x18""\x11""So heavy, you can't run.\x11""So heavy, you can't float.\xBF") },
    // OoT message 0x54
    { ITEM_OOT_BOOTS_HOVER,
      MS_TEXT("\x17""You got the \x01""Hover Boots\x00""!\x18""\x11""With these mysterious boots\x11""you can hover above the "
              "ground.\x11""The downside? No traction!\x10""You can even use these to\x11""walk for short periods where "
              "\x11""there is no solid ground. Be brave\x11""and trust in the boots!\xBF") },
    // OoT message 0x79
    { ITEM_OOT_BRACELET,
      MS_TEXT("\x17""You got the \x01""Goron's Bracelet\x00""!\x18""\x11""Now you can pull up Bomb \x11""Flowers. Stand next to one "
              "and\x11""use \x03""\xB0""\x00"" to pull it up!\xBF") },
    // OoT message 0x5B
    { ITEM_OOT_GAUNTLETS_SILVER,
      MS_TEXT("\x17""You found the \x03""Silver Gauntlets\x00""!\x18""\x11""If you wore them, you would\x11""feel power in your arms, "
              "the \x11""power to lift big things with \x03""\xB0""\x00""!\x10""But, these gauntlets won't fit\x11""a kid... Plus, "
              "you promised to give\x11""them to \x01""Nabooru\x00"". You should keep\x11""your word...\xBF") },
    // OoT message 0x5C
    { ITEM_OOT_GAUNTLETS_GOLD,
      MS_TEXT("\x17""You found the \x03""Golden Gauntlets\x00""!\x18""\x11""You can feel even more power \x11""coursing through your "
              "arms!\x11""Grab with \x03""\xB0"" \x00""and lift stuff up!\xBF") },
    // OoT message 0xCD
    { ITEM_OOT_SCALE_SILVER,
      MS_TEXT("\x17""You got the \x03""Silver Scale\x00""!\x18""\x11""Jump into the water and press \x11""\x03""\xB0""\x00"". You can dive "
              "deeper than you\x11""could before.\xBF") },
    // OoT message 0xCE
    { ITEM_OOT_SCALE_GOLDEN,
      MS_TEXT("\x17""You got the \x03""Golden Scale\x00""!\x18""\x11""Jump into the water and press \x11""\x03""\xB0""\x00"". Now you can "
              "dive much\x11""deeper than you could before!\xBF") },
};

// OoT message 0x07 (the 40-seed bag; the first bag's count says 30) and 0x6C (the 50-seed bag).
const char sBulletBagText30[] = "\x17""You got a \x11""\x01"
                                "Deku Seeds Bullet Bag\x00"".\x18""\x11""This bag can hold up to \x04"
                                "30\x00""\x11""slingshot bullets.\xBF";
const char sBulletBagText40[] = "\x17""You got a \x11""\x01"
                                "Deku Seeds Bullet Bag\x00"".\x18""\x11""This bag can hold up to \x04"
                                "40\x00""\x11""slingshot bullets.\xBF";
const char sBulletBagText50[] = "\x17""Your \x01"
                                "Deku Seeds Bullet Bag \x11""\x00""has become bigger!\x18""\x11""Now you can carry more "
                                "bullets!\x11""This bag can hold \x04"
                                "50\x01"" \x00"
                                "bullets!\xBF";

std::string GetItemText(u8 item) {
    if (item == ITEM_OOT_BULLET_BAG) {
        // Read after Item_Give: the bag just received.
        u8 level = gSaveContext.save.shipSaveInfo.mergeShipOoTItems.bulletBag;
        if (level >= 3) {
            return std::string(sBulletBagText50, sizeof(sBulletBagText50) - 1);
        }
        if (level == 2) {
            return std::string(sBulletBagText40, sizeof(sBulletBagText40) - 1);
        }
        return std::string(sBulletBagText30, sizeof(sBulletBagText30) - 1);
    }
    for (const auto& t : sGetItemTexts) {
        if (t.item == item) {
            return std::string(t.text, t.length);
        }
    }
    return std::string("\x17You got it!\x18\xBF");
}

CustomMessage::Entry MakeEntry(u8 item, std::string text) {
    CustomMessage::Entry entry;
    entry.textboxType = 2; // MM's get-item textbox (MM message 0x32 and the others)
    entry.textboxYPos = 0;
    entry.icon = item;
    entry.autoFormat = false; // already in MM's codes
    entry.msg = std::move(text);
    return entry;
}

} // namespace

extern "C" void MergeShip_OoTEquip_ShowText(const char* mmCodedText) {
    // OoT message 0x305F "Your shield is gone!": MM's own mid-gameplay textbox (MM 0xF9, the Razor Sword's).
    std::string text(mmCodedText);
    // The literal holds MM's colour code 0x00 (white): take it up to the end code instead of the first zero.
    size_t len = 0;
    while ((u8)mmCodedText[len] != 0xBF) {
        len++;
    }
    text.assign(mmCodedText, len + 1);
    CustomMessage::Entry entry;
    entry.textboxType = 6;
    entry.autoFormat = false;
    if ((gPlayState != nullptr) && (Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_NONE)) {
        CustomMessage::StartTextbox(text, entry);
    }
}

extern "C" void MergeShip_OoTEquip_GiveWithCutscene(u8 item) {
    GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
        .showGetItemCutscene = true,
        .param = (s16)item,
        .giveItem =
            [](Actor* actor, PlayState* play) {
                u8 item = (u8)CUSTOM_ITEM_PARAM;
                Item_Give(play, item);
                CustomMessage::Entry entry = MakeEntry(item, GetItemText(item));
                // The textbox's icon is the item itself (z_message.c Message_DecodeHeader, MergeShipOoTItems.h).
                MergeShip_OoTItems_SetTextIconPending(item);
                if (CUSTOM_ITEM_FLAGS & CustomItem::GIVE_ITEM_CUTSCENE) {
                    CustomMessage::SetActiveCustomMessage(entry.msg, entry);
                } else {
                    entry.msg.pop_back(); // MM's timed close before the end code (SkipCouplesMaskCutscene)
                    CustomMessage::StartTextbox(entry.msg + "\x1C\x02\x10\xBF", entry);
                }
                MergeShipTest_Log("OOTEQ getitem 0x%02X shown", item);
            },
        .drawItem =
            [](Actor* actor, PlayState* play) {
                Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                MergeShip_OoTEquip_DrawGetItem(play, (u8)CUSTOM_ITEM_PARAM);
            },
    });
    MergeShipTest_Log("OOTEQ getitem 0x%02X queued", item);
}

// ---- hooks ---------------------------------------------------------------------------------------------------------------

static bool sOoTEquipTrace = false; // the harness's "ooteqtrace on"
void MergeShipTest_OoTEquipLogState(const char* tag);

static void RegisterOoTEquipment() {
    bool enabled = CVarGetInteger(MERGESHIP_OOT_EQUIP_CVAR, 1) != 0;

    // Every frame after the player's update: the B button, the tunic and the boots follow the save.
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, [](Actor* actor) {
        if ((gPlayState != nullptr) && (actor == &GET_PLAYER(gPlayState)->actor)) {
            MergeShip_OoTEquip_Update(gPlayState);
            MergeShip_OoTEquip_HoverSfx((Player*)actor);
            if (sOoTEquipTrace) {
                MergeShipTest_OoTEquipLogState("OOTEQTRACE");
            }
        }
    });
    COND_ID_HOOK(OnActorDraw, ACTOR_PLAYER, enabled, [](Actor* actor) {
        if (gPlayState != nullptr) {
            MergeShip_OoTEquip_DrawHoverEffect(gPlayState, (Player*)actor);
        }
    });
    COND_HOOK(OnPlayerPostLimbDraw, enabled, [](Player* player, s32 limbIndex) {
        if (gPlayState != nullptr) {
            MergeShip_OoTEquip_PostLimbDraw(gPlayState, player, limbIndex);
        }
    });
    COND_HOOK(OnItemGive, enabled, [](u8 item) { MergeShip_OoTEquip_OnMMItemGiven(item); });
    // Each game's own items (MergeShipOwnership.h), always: before MM's Item_Give its copy is put in the slot, after it
    // the ownership is written.
    COND_HOOK(ShouldItemGive, true, [](u8 item, bool* should) {
        if (*should) {
            MergeShip_Own_BeforeMMItemGiven(item);
        }
    });
    COND_HOOK(OnItemGive, true, [](u8 item) { MergeShip_Own_OnMMItemGiven(item); });
    // The one shared ammo count (MergeShipOwnership.h) is Hyrule's too: the cycle's emptied slots take it back after.
    COND_HOOK(BeforeEndOfCycleSave, true, []() { MergeShip_Own_BeforeEndOfCycle(); });
    COND_HOOK(AfterEndOfCycleSave, true, []() { MergeShip_Own_AfterEndOfCycle(); });
    COND_HOOK(OnKaleidoUpdate, true, [](PauseContext* pauseCtx) {
        // Leaving the pause screen closes the equipment page (the Quest face keeps it while other faces are turned to,
        // as it keeps Hyrule's quest status; z_kaleido_mergeship.c).
        if (MergeShip_OoTEquipPause_IsOpen() &&
            ((pauseCtx->state == PAUSE_STATE_OFF) || (pauseCtx->state >= PAUSE_STATE_UNPAUSE_SETUP) ||
             !MergeShip_OoTEquip_Enabled())) {
            MergeShip_OoTEquipPause_Close();
        }
    });
}

static RegisterShipInitFunc sOoTEquipInit(RegisterOoTEquipment, { MERGESHIP_OOT_EQUIP_CVAR });

// ---- test harness commands (MergeShipTest.cpp) ---------------------------------------------------------------------------

namespace {
long ParseArg(const std::vector<std::string>& args, size_t i, long def = 0) {
    return (i < args.size()) ? strtol(args[i].c_str(), nullptr, 0) : def;
}
} // namespace

void MergeShipTest_OoTEquipLogState(const char* tag) {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);
    MergeShipTest_Log("%s owned=%08X sword=%d shield=%d tunic=%d(native %d) boots=%d variants=%X strength=%d(get %d) "
                      "scale=%d bgs=%d health=%d B=0x%02X heldItem=0x%02X ia=%d curShield=%d curBoots=%d envHazard=%d "
                      "hazardTimer=%d pos=(%.1f,%.1f,%.1f) vy=%.2f depth=%.1f s1=%08X s2=%08X bg=%04X quad=%d/%d",
                      tag, OOT_EQUIP.owned, OOT_EQUIP.sword, OOT_EQUIP.shield, OOT_EQUIP.tunic,
                      GET_CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC), OOT_EQUIP.boots, OOT_EQUIP.variants, OOT_EQUIP.strength,
                      Player_GetStrength(), OOT_EQUIP.scale, OOT_EQUIP.bgsFlag, OOT_EQUIP.swordHealth,
                      BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), player->heldItemId, player->heldItemAction,
                      player->currentShield, player->currentBoots, Player_GetEnvironmentalHazard(gPlayState),
                      gSaveContext.timerStates[TIMER_ID_ENV_HAZARD], player->actor.world.pos.x,
                      player->actor.world.pos.y, player->actor.world.pos.z, player->actor.velocity.y,
                      player->actor.depthInWater, player->stateFlags1, player->stateFlags2, player->actor.bgCheckFlags,
                      player->meleeWeaponQuads[0].elem.atDmgInfo.damage, player->meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags);
}

// Returns true when `op` is one of these commands:
//   ooteq <sword|shield|tunic|boots> <n>   equip (MsOoTEqSword / MsOoTEqShield / 0-2 / 0-2)
//   ooteqgi <item>                         give with Link holding it up (MergeShip_OoTEquip_GiveWithCutscene)
//   ooteqstate [tag]                       log the equipment and the player's state
//   ooteqtrace on|off                      log that state every frame (after the player's update)
//   ooteqquestpoint <point>                put the Quest page's cursor on a point (QUEST_SWORD 5, QUEST_SHIELD 4)
//   ooteqpos <x> <y> <z> [yaw]             place the player
//   ooteqhot <0|1>                         make the room hot (ROOM_ENV_HOT) for the Goron Tunic
//   ooteqburn                              set the player on fire (the Deku Shield burns)
//   ooteqspawn <actorId> <dist> [params]   spawn an actor in front of the player
bool MergeShipTest_OoTEquipCommand(const std::string& op, const std::vector<std::string>& args) {
    PlayState* play = gPlayState;

    if (op == "ooteq") {
        s32 value = (s32)ParseArg(args, 1);
        s32 ok = false;
        if (play == nullptr) {
            return true;
        }
        if (args.empty()) {
            MergeShipTest_Log("OOTEQ usage");
        } else if (args[0] == "sword") {
            ok = MergeShip_OoTEquip_EquipSword(play, value);
        } else if (args[0] == "shield") {
            ok = MergeShip_OoTEquip_EquipShield(play, value);
        } else if (args[0] == "tunic") {
            ok = MergeShip_OoTEquip_EquipTunic(value);
        } else if (args[0] == "boots") {
            ok = MergeShip_OoTEquip_EquipBoots(play, value);
        }
        MergeShipTest_Log("OOTEQ equip %s %d -> %d", args.empty() ? "" : args[0].c_str(), value, ok);
        return true;
    }
    if (op == "ooteqcursor") {
        MergeShip_OoTEquipPause_DebugCursor(gPlayState, args.empty() ? -1 : (s32)ParseArg(args, 0));
        return true;
    }
    if (op == "ooteqgi") {
        MergeShip_OoTEquip_GiveWithCutscene((u8)ParseArg(args, 0));
        return true;
    }
    if (op == "ooteqtrace") {
        sOoTEquipTrace = !args.empty() && (args[0] == "on");
        return true;
    }
    if (op == "ooteqstate") {
        MergeShipTest_OoTEquipLogState(args.empty() ? "OOTEQSTATE" : args[0].c_str());
        return true;
    }
    if (op == "ooteqpos") {
        if (play != nullptr) {
            Player* player = GET_PLAYER(play);
            player->actor.world.pos.x = player->actor.home.pos.x = (f32)ParseArg(args, 0);
            player->actor.world.pos.y = player->actor.home.pos.y = (f32)ParseArg(args, 1);
            player->actor.world.pos.z = player->actor.home.pos.z = (f32)ParseArg(args, 2);
            player->actor.prevPos = player->actor.world.pos;
            if (args.size() > 3) {
                player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)ParseArg(args, 3);
            }
            MergeShipTest_Log("OOTEQ pos %.0f %.0f %.0f", player->actor.world.pos.x, player->actor.world.pos.y,
                              player->actor.world.pos.z);
        }
        return true;
    }
    if (op == "ooteqlift") {
        // Raise the player by dy (a ledge without looking for one: the Hover Boots' timer was set on the ground).
        if (play != nullptr) {
            Player* player = GET_PLAYER(play);
            player->actor.world.pos.y += (f32)ParseArg(args, 0);
            player->actor.prevPos = player->actor.world.pos;
            player->actor.velocity.y = 0.0f;
            MergeShipTest_Log("OOTEQ lift to y=%.1f", player->actor.world.pos.y);
        }
        return true;
    }
    if (op == "ooteqquestpoint") {
        // Put the Quest page's cursor on a point (QUEST_SWORD 5, QUEST_SHIELD 4) before pressing A there.
        if (play != nullptr) {
            play->pauseCtx.cursorPoint[PAUSE_QUEST] = (s16)ParseArg(args, 0);
            // After a page turn the cursor sits on the page arrow (cursorSpecialPos); a point on the page is not.
            play->pauseCtx.cursorSpecialPos = 0;
            MergeShipTest_Log("OOTEQ quest point %d", play->pauseCtx.cursorPoint[PAUSE_QUEST]);
        }
        return true;
    }
    if (op == "ooteqhot") {
        if (play != nullptr) {
            play->roomCtx.curRoom.environmentType = ParseArg(args, 0) ? ROOM_ENV_HOT : 0;
            MergeShipTest_Log("OOTEQ room env=%d", play->roomCtx.curRoom.environmentType);
        }
        return true;
    }
    if (op == "ooteqburn") {
        if (play != nullptr) {
            Player* player = GET_PLAYER(play);
            for (s32 i = 0; i < PLAYER_BODYPART_MAX; i++) {
                player->bodyFlameTimers[i] = Rand_S16Offset(0, 200);
            }
            player->bodyIsBurning = true;
            MergeShipTest_Log("OOTEQ burning");
        }
        return true;
    }
    if (op == "ooteqspawn") {
        if (play != nullptr) {
            Player* player = GET_PLAYER(play);
            f32 dist = (f32)ParseArg(args, 1, 80);
            s16 yaw = player->actor.shape.rot.y;
            Actor* spawned = Actor_Spawn(&play->actorCtx, play, (s16)ParseArg(args, 0),
                                         player->actor.world.pos.x + Math_SinS(yaw) * dist, player->actor.world.pos.y,
                                         player->actor.world.pos.z + Math_CosS(yaw) * dist, 0, yaw + 0x8000, 0,
                                         (s16)ParseArg(args, 2));
            MergeShipTest_Log("OOTEQ spawn 0x%X -> %p", (int)ParseArg(args, 0), (void*)spawned);
        }
        return true;
    }
    return false;
}
