// MergeShip2 -- 2 Ship 2 Harkinian's side of the host: Termina's end of the door (M3) and the shared save's
// transfers (M4). See MergeShipHost.h and Shipwright/mergeship2/docs/HOST_DESIGN.md.
//
// 2ship.exe (MERGESHIP_HOSTED off) gets the stubs at the end: never hosted, Majora's Mask untouched.

#include "2s2h/MergeShip/MergeShipHost.h"

#ifdef MERGESHIP_HOSTED

#include "2s2h/MergeShip/MergeShipSharedSave.h"
#include "2s2h/MergeShip/MergeShipEntrances.h"
#include "2s2h/MergeShip/MergeShipSingleplayer.h"
#include "2s2h/Network/Anchor/MergeShipAnchor.h" // MergeShipAnchor_OnHyruleArrival
#include "2s2h/Rando/MergeShip/UnifiedHyrule.h"
#include "2s2h/Rando/MergeShip/UnifiedSeed.h"

#include <mergeship/EngineApi.h>
#include <mergeship/SharedSave.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
}

// ShipUtils.cpp: Majora's Mask's sOwlWarpEntrances (the Song of Soaring's statues, OWL_WARP_*).
extern u16 sOwlWarpEntrancesForMods[];

namespace {

const MsHostServices* sHost = nullptr;

enum class Door {
    Idle,    // nothing going on
    Away,    // the player went back to Hyrule; the transition that took them there is held, faded out
    Arrived, // the player is back: the held transition commits sArrivalEntrance on the next frame
};
Door sDoor = Door::Idle;

// An arrival's entrance in Majora's Mask's numbering: the owl statue's (sOwlWarpEntrances) for the Song of Soaring from
// Hyrule (MS_TRANSFER_FLAG_OWL_WARP, `entrance` = OWL_WARP_*), else the entrance itself.
u16 ArrivalEntrance(const MsTransfer* arrival) {
    if (arrival->flags & MS_TRANSFER_FLAG_OWL_WARP) {
        const s32 owl = arrival->entrance;
        return ((owl >= 0) && (owl < OWL_WARP_MAX - 1)) ? sOwlWarpEntrancesForMods[owl] : (u16)MS_ENTRANCE_2SHIP_CLOCK_TOWN;
    }
    return (u16)arrival->entrance;
}
u16 sArrivalEntrance = 0;
// BUG-0036: Anchor's warp to a teammate in the other world -- going (to Hyrule) or arriving (from it).
struct AnchorWarp {
    bool set = false;
    s32 entrance = 0;
    s32 room = 0;
    f32 pos[3] = {};
    s32 yaw = 0;
};
AnchorWarp sPendingAnchorWarp;
AnchorWarp sArrivalAnchorWarp;
bool sReturnExitTaken = false; // the player walked into the Clock Tower passage's exit (func_808354A4)
bool sTransferRequested = false;
const char* sDoorState = "idle";

// The session came from Hyrule: the Ocarina of Time file it belongs to and the Majora's Mask file it plays.
bool sLinked = false;
int32_t sLinkedSlot = -1;
s32 sLinkedFileNum = -1;

// An arrival that must start (or replace) a game state, done before the next frame (MergeShipHost_BeforeFrame).
bool sBootPending = false;
MsTransfer sBoot = {};

// The body Link arrived in from Hyrule (MsTransfer.playerAge / playerForm), -1 when not given.
s32 sArrivalAge = -1;
s32 sArrivalForm = -1;

// Entrance randomizer (M8, MergeShipEntrances.h): the trip away was a shuffled entrance (sRandoVanilla: where MM's own
// exit went, if the host turns the trip down); the arrival is a Termina key, resolved as it commits.
bool sRandoTrip = false;
u16 sRandoVanilla = 0;
bool sArrivalRando = false;

// Majora's Mask's first cycle (MergeShipSharedSave.h): SoH's menu turned "Share items during Termina's first cycle" off
// for this trip (MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE); an apply held things back until MM's own script is done.
bool sArrivalNoFirstCycleShare = false;
bool sHeldBack = false;
} // namespace

// Adult Link in Termina is another workstream's (a player form of its own in 2Ship). It receives Link's Ocarina of
// Time age here, and tells the age he leaves with; until it lands, these defaults stand in (MSVC's /alternatename is
// a weak symbol: a real MergeShip_Hyrule_SetArrivalAge / _GetDepartureAge anywhere in 2ship.dll replaces them).
extern "C" void MergeShip_Hyrule_SetArrivalAge(u8 ootAge);
extern "C" u8 MergeShip_Hyrule_GetDepartureAge(void);
extern "C" void MergeShipHost_DefaultSetArrivalAge(u8 ootAge) {
    (void)ootAge;
}
extern "C" u8 MergeShipHost_DefaultGetDepartureAge(void) {
    return (sArrivalAge == 0) ? 0 : 1; // LINK_AGE_ADULT / LINK_AGE_CHILD: as he came
}
#ifdef _MSC_VER
#pragma comment(linker, "/alternatename:MergeShip_Hyrule_SetArrivalAge=MergeShipHost_DefaultSetArrivalAge")
#pragma comment(linker, "/alternatename:MergeShip_Hyrule_GetDepartureAge=MergeShipHost_DefaultGetDepartureAge")
#else
// (clang / ELF: the same fallbacks as weak definitions; a real one anywhere in the library wins)
extern "C" __attribute__((weak)) void MergeShip_Hyrule_SetArrivalAge(u8 ootAge) {
    MergeShipHost_DefaultSetArrivalAge(ootAge);
}
extern "C" __attribute__((weak)) u8 MergeShip_Hyrule_GetDepartureAge(void) {
    return MergeShipHost_DefaultGetDepartureAge();
}
#endif

namespace {

void HostLog(int32_t level, const char* message) {
    if (sHost != nullptr && sHost->Log != nullptr) {
        sHost->Log(sHost->host, level, message);
    }
}

void DoorLog(const char* state, const char* fmt, ...) {
    sDoorState = state;
    char message[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    MergeShipHostTest_Log("DOOR %s: %s", state, message);
    char line[300];
    snprintf(line, sizeof(line), "2Ship door %s: %s", state, message);
    HostLog(0, line);
}

bool DoorEnabled() {
    return (sHost != nullptr) && CVarGetInteger(MS_CVAR_TERMINA_DOOR, 1);
}

// Majora's Mask's file for an Ocarina of Time file slot: the same slot; SoH's unsaved debug file uses the third.
s32 FileNumForSlot(int32_t slot) {
    return ((slot >= 0) && (slot < 3)) ? slot : 2;
}

// MM's file-name characters (sSaveDefaultPlayerData's "LINK" is 0x15 0x12 0x17 0x14): 0-9 0x00.., A-Z 0x0A..,
// a-z 0x24.., space 0x3E.
u8 NameChar(char c) {
    if (c >= '0' && c <= '9') {
        return (u8)(c - '0');
    }
    if (c >= 'A' && c <= 'Z') {
        return (u8)(0x0A + (c - 'A'));
    }
    if (c >= 'a' && c <= 'z') {
        return (u8)(0x24 + (c - 'a'));
    }
    return 0x3E;
}

// The player's share of Termina's progress takes part only once MM's prologue is over (its first Song of Time):
// before that, Majora's Mask is exactly its own opening, with its own starting inventory.
bool PrologueOver() {
    return gSaveContext.save.saveInfo.playerData.threeDayResetCount > 0;
}

// "Share items during Termina's first cycle" (EngineApi.h MS_CVAR_SHARE_FIRST_CYCLE): on in this game's menu and not
// turned off by SoH's for the trip.
bool ShareFirstCycle() {
    return CVarGetInteger(MS_CVAR_SHARE_FIRST_CYCLE, 1) && !sArrivalNoFirstCycleShare;
}

// Majora's Mask's scripted first cycle is done: the first Song of Time was played and the Happy Mask Salesman has
// healed Link (he gives the Deku Mask with the Song of Healing; 2Ship's Skip First Cycle gives both). A randomizer file
// starts after it (Rando::MiscBehavior::OnFileCreate: the Salesman's flag set, the Deku Mask shuffled). Until then MM's
// ocarina, the Song of Time and Healing and the transformation masks are held back (MergeShipSharedSave.h), and Link's
// form is MM's to decide.
bool FirstCycleScriptDone() {
    if (!PrologueOver()) {
        return false;
    }
    return (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) ||
           (INV_CONTENT(ITEM_MASK_DEKU) == ITEM_MASK_DEKU) || CHECK_QUEST_ITEM(QUEST_SONG_HEALING);
}

// MS_FORM_* (SoH's PlayerForm) <-> Majora's Mask's PlayerForm. The Fierce Deity comes as the human: Majora's Mask
// allows that mask only in boss lairs.
s32 MMFormFromMs(s32 form) {
    switch (form) {
        case MS_FORM_DEKU: return PLAYER_FORM_DEKU;
        case MS_FORM_GORON: return PLAYER_FORM_GORON;
        case MS_FORM_ZORA: return PLAYER_FORM_ZORA;
        default: return PLAYER_FORM_HUMAN;
    }
}
s32 MsFormFromMM(s32 mmForm, s32 age) {
    switch (mmForm) {
        case PLAYER_FORM_DEKU: return MS_FORM_DEKU;
        case PLAYER_FORM_GORON: return MS_FORM_GORON;
        case PLAYER_FORM_ZORA: return MS_FORM_ZORA;
        case PLAYER_FORM_FIERCE_DEITY: return MS_FORM_FIERCE_DEITY;
        default: return (age == 0) ? MS_FORM_ADULT : MS_FORM_CHILD;
    }
}

// Link arrives as he left Hyrule: his age goes to adult Link's workstream; a Deku, Goron or Zora stays one (after the
// prologue too: Link is never Majora's Mask's cursed first-cycle Deku, 2026-10-10).
void TakeArrivalAnchorWarp(const MsTransfer* arrival) {
    sArrivalAnchorWarp.set = (arrival->flags & MS_TRANSFER_FLAG_ANCHOR_WARP) && MS_TRANSFER_HAS(arrival, anchorYaw);
    if (sArrivalAnchorWarp.set) {
        sArrivalAnchorWarp.entrance = arrival->entrance;
        sArrivalAnchorWarp.room = arrival->anchorRoom;
        sArrivalAnchorWarp.pos[0] = arrival->anchorPos[0];
        sArrivalAnchorWarp.pos[1] = arrival->anchorPos[1];
        sArrivalAnchorWarp.pos[2] = arrival->anchorPos[2];
        sArrivalAnchorWarp.yaw = arrival->anchorYaw;
    }
}

// Link arrives at the teammate's place (as Anchor's own TELEPORT_TO does, TeleportTo.cpp).
void ApplyArrivalAnchorWarp(PlayState* play) {
    if (!sArrivalAnchorWarp.set || (play == nullptr)) {
        return;
    }
    sArrivalAnchorWarp.set = false;
    Vec3f pos = { sArrivalAnchorWarp.pos[0], sArrivalAnchorWarp.pos[1], sArrivalAnchorWarp.pos[2] };
    Play_SetRespawnData(play, RESPAWN_MODE_DOWN, (u16)sArrivalAnchorWarp.entrance, (s8)sArrivalAnchorWarp.room,
                        PLAYER_PARAMS(0xFF, PLAYER_START_MODE_B), &pos, (s16)sArrivalAnchorWarp.yaw);
    gSaveContext.respawnFlag = 1;
    static HOOK_ID hookId = 0;
    hookId = GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnActorInit>(
        ACTOR_PLAYER, [](Actor* actor) {
            if ((gPlayState != nullptr) && (actor == (Actor*)GET_PLAYER(gPlayState))) {
                ((Player*)actor)->unk_D6A = 0;
                GameInteractor::Instance->UnregisterGameHookForID<GameInteractor::OnActorInit>(hookId);
            }
        });
    DoorLog("body", "Anchor warp: at the teammate's place in room %d", sArrivalAnchorWarp.room);
}

void ApplyArrivalBody(const MsTransfer* arrival) {
    if (!(arrival->flags & MS_TRANSFER_FLAG_PLAYER_BODY) || !MS_TRANSFER_HAS(arrival, playerForm)) {
        return;
    }
    sArrivalAge = arrival->playerAge;
    sArrivalForm = arrival->playerForm;
    if ((sArrivalAge == 0) || (sArrivalAge == 1)) {
        MergeShip_Hyrule_SetArrivalAge((u8)sArrivalAge);
    }
    // Always as he left Hyrule, MM's first cycle too: never its cursed Deku (the user, 2026-10-10).
    gSaveContext.save.playerForm = (u8)MMFormFromMs(sArrivalForm);
    MergeShipHostTest_Log("BODY arrival age=%d form=%d -> MM form %d", sArrivalAge, sArrivalForm,
                          gSaveContext.save.playerForm);
}

void ApplyShared(const MsTransfer* arrival) {
    sArrivalNoFirstCycleShare = (arrival->flags & MS_TRANSFER_FLAG_NO_FIRST_CYCLE_SHARE) != 0;
    if (!(arrival->flags & MS_TRANSFER_FLAG_SHARED_SAVE) || (arrival->sharedSave == nullptr) ||
        (arrival->sharedSaveSize < sizeof(MsSharedSave))) {
        return;
    }
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(arrival->sharedSave);
    if (FirstCycleScriptDone()) {
        MergeShipShared_Apply(shared, 0);
        sHeldBack = false;
        return;
    }
    if (!PrologueOver() && !ShareFirstCycle()) {
        MergeShipHostTest_Log("SHARED apply skipped: Majora's Mask's first cycle (sharing during it is off)");
        return;
    }
    // The user's rule: everything Link owns (items, songs, masks) comes with him from the first arrival, as items and
    // songs survive MM's own Song of Time; nothing is held back for MM's first-cycle script.
    MergeShipShared_Apply(shared, 0);
    sHeldBack = false;
}

// What an apply held back during MM's first cycle is given once MM's own script is done, outside a cutscene or a
// message (the Salesman's scene gives the Deku Mask last).
void DeliverHeldBack() {
    if (!sHeldBack || !sLinked || (gSaveContext.fileNum != sLinkedFileNum) || (gPlayState == nullptr) ||
        (GET_PLAYER(gPlayState) == nullptr) || (gSaveContext.gameMode != GAMEMODE_NORMAL) || !FirstCycleScriptDone() ||
        (gPlayState->csCtx.state != CS_STATE_IDLE) || (gPlayState->msgCtx.msgMode != MSGMODE_NONE)) {
        return;
    }
    sHeldBack = false;
    if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr)) {
        return;
    }
    MergeShipShared_ApplyHeldBack(static_cast<const MsSharedSave*>(sHost->GetSharedSave(sHost->host, sLinkedSlot)));
}

// The user's rule (2026-09-23): nothing is granted on the way into Termina -- Link has there only what he owns in
// Hyrule (the shared save, applied at every arrival) and what he earns in Termina. Majora's Mask's new save
// (sSaveDefaultItemEquips / sSaveDefaultPlayerData) starts Link with the Kokiri Sword on B and the Hero's Shield, and a
// full magic amount (no meter): taken off here, as 2Ship's randomizer does (Rando::MiscBehavior::OnFileCreate). MM's
// first cycle plays without them (MM gives Deku Link the meter at the Clock Town Great Fairy; see
// build/tests/door_no_free_gear/README.txt). The C-Left button keeps pointing at the ocarina's slot (empty: MM's rooftop
// puts the ocarina there). The file is marked (MERGESHIP_SHARED_BARE_START): its Hero's Shield, if it gets one, is
// Termina's own.
void StripNewSaveGear() {
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_NONE);
    SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_NONE);
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B) = ITEM_NONE;
    gSaveContext.save.saveInfo.playerData.isMagicAcquired = false;
    gSaveContext.save.saveInfo.playerData.isDoubleMagicAcquired = false;
    gSaveContext.save.saveInfo.playerData.magicLevel = 0;
    gSaveContext.save.saveInfo.playerData.magic = 0;
    gSaveContext.save.shipSaveInfo.mergeShipOoTEquip.variants |= MERGESHIP_SHARED_BARE_START;
    MergeShipHostTest_Log("NEWFILE bare start: sword=%d shield=%d B=0x%02X ocarina=0x%02X magic=%d/%d",
                          GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD),
                          BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), INV_CONTENT(ITEM_OCARINA_OF_TIME),
                          gSaveContext.save.saveInfo.playerData.isMagicAcquired,
                          gSaveContext.save.saveInfo.playerData.magic);
}

// BUG-0100: Termina never waits on day 0 (MM's moment before the Dawn of the First Day card) once its prologue is over:
// the door starts the First Day at 6:01, on every arrival, without the card. MM's EnTest4 turns day 0 into day 1, but
// it is not in every scene a shuffled entrance leads to: a file left on day 0 there kept its clock running on day 0
// (the user, 2026-10-12: "putting link at the wrong time"), and the randomizer's clock reads day 0 as the end.
void StartFirstDay(const char* when) {
    if ((gSaveContext.save.day != 0) || !PrologueOver() ||
        !CVarGetInteger("gEnhancements.MergeShip.SkipFirstDawnCard", 1)) {
        return;
    }
    gSaveContext.save.day = 1;
    gSaveContext.save.eventDayCount = 1;
    gSaveContext.save.time = CLOCK_TIME(6, 1);
    DoorLog(when, "day 0: the First Day starts at 6:01 (no dawn card)");
}

// The user (2026-11-03): "The dark sky day 4 thing in termina is back" -- a teammate's first arrival from the Lost
// Woods had a dark sky until a grotto took them out of the scene and back. Majora's Mask builds a scene's sky from the
// raw day (Environment_Init: from day 4 on, the moon's day, a dark overcast sky at every hour), while everything else
// reads the day modulo five (CURRENT_DAY): a raw day 5 is day 0 to EnTest4, which starts the First Day on the scene's
// first frame -- after the sky was built dark. The clock looked right; the sky stayed the moon's until the next scene,
// and the randomizer's clock reads any raw day past 3 as the end. A raw day past 4 is folded into the day the game
// reads (day 9, the ending's "Dawn of a New Day", into the First Day, as DayTelop leaves it); a day 0 that comes of it
// starts the First Day as the door does (StartFirstDay, which reads the raw day).
void FoldCalendar(const char* when) {
    const s32 raw = gSaveContext.save.day;

    if ((raw >= 0) && (raw <= 4)) {
        return;
    }
    gSaveContext.save.day = (raw >= 9) ? 1 : (raw % 5);
    gSaveContext.save.eventDayCount = gSaveContext.save.day;
    DoorLog(when, "calendar: raw day %d is day %d (time 0x%04X)", raw, gSaveContext.save.day, gSaveContext.save.time);
    StartFirstDay(when);
}

// BUG-0157: Majora's Mask keeps the weather through an exit (Player's scene exits set retainWeatherMode, and a pending
// respawn keeps it too, Environment_Init): Ikana's gloom (WEATHER_MODE_2: its dark skybox and light setting 5), the
// rain or the snow. Through the door Link leaves one region of Termina for Hyrule and comes back to another, so the
// weather he left with came along -- a dark sky and lighting over the arrival, until a grotto (a scene without a
// pending respawn) cleared it. An arrival starts clear; the scene's own weather tags set it again.
void ClearWeather(const char* when) {
    if ((gWeatherMode != WEATHER_MODE_CLEAR) || gSaveContext.retainWeatherMode) {
        DoorLog(when, "weather: mode %d (retained %d) cleared", gWeatherMode, gSaveContext.retainWeatherMode);
    }
    gWeatherMode = WEATHER_MODE_CLEAR;
    gSaveContext.retainWeatherMode = false;
}

// Creates Majora's Mask's file `fileNum` as the file select does (Sram_InitSave), named after the Hyrule file.
void CreateFile(s32 fileNum, const char* name, std::vector<u8>& buffer) {
    Rand_Seed(osGetTime()); // the title demo would have seeded it (SkipToFileSelect does the same)
    Sram_InitNewSave();
    StripNewSaveGear(); // before OnSaveInit: a randomizer file's own starting items (options) still come after
    if (fileNum == 0) {
        gSaveContext.save.cutsceneIndex = 0xFFF0; // as Sram_InitSave for the first file
    }
    size_t length = strlen(name);
    if (length == 0) {
        name = "Link";
        length = 4;
    }
    for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.save.saveInfo.playerData.playerName); i++) {
        gSaveContext.save.saveInfo.playerData.playerName[i] = (i < length) ? NameChar(name[i]) : 0x3E;
    }
    const char newf[] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    memcpy(gSaveContext.save.saveInfo.playerData.newf, newf, sizeof(newf));
    gSaveContext.save.saveInfo.checksum = 0;
    gSaveContext.save.saveInfo.checksum = Sram_CalcChecksum(&gSaveContext.save, sizeof(Save));
    GameInteractor_ExecuteOnSaveInit(fileNum);

    memset(buffer.data(), 0, buffer.size());
    memcpy(buffer.data(), &gSaveContext.save, sizeof(Save));
    memcpy(&buffer[SAVE_BUFFER_SIZE_HALF], &gSaveContext.save, sizeof(Save));
    SysFlashrom_WriteDataSync(buffer.data(), gFlashSaveStartPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
                              gFlashSpecialSaveNumPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER]);
}

// Loads (or first creates) this Hyrule file's Majora's Mask file and starts gameplay, as FileSelect_LoadGame does.
void Boot() {
    const s32 fileNum = FileNumForSlot(sBoot.saveSlot);
    std::vector<u8> buffer(SAVE_BUFFER_SIZE);

    // A unified seed's Termina file that is not that seed's randomizer file (made before the seed reached Termina, or
    // another seed's) is set aside and made again (Rando/MergeShip/UnifiedSeed.h).
    // A slot the Hyrule file started anew (MsSharedSave.terminaFileStale): the earlier playthrough's file is replaced.
    MsSharedSave* bootShared = ((sBoot.flags & MS_TRANSFER_FLAG_SHARED_SAVE) && (sBoot.sharedSave != nullptr) &&
                                (sBoot.sharedSaveSize >= sizeof(MsSharedSave)))
                                   ? static_cast<MsSharedSave*>(sBoot.sharedSave)
                                   : nullptr;
    const bool stale = (bootShared != nullptr) && bootShared->terminaFileStale;
    const bool exists = !stale &&
                        (SysFlashrom_ReadData(buffer.data(), gFlashSaveStartPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
                                              gFlashSaveNumPages[fileNum * FLASH_SAVE_MAIN_MULTIPLIER]) == 0) &&
                        !Rando::MergeShip::RetireStaleTerminaFile(fileNum, buffer.data());
    if (stale) {
        bootShared->terminaFileStale = 0;
        DoorLog("booting", "Hyrule file slot %d started anew: Termina file %d made again", sBoot.saveSlot, fileNum + 1);
    }
    const s32 owlIndex = (fileNum + FILE_NUM_OWL_SAVE_OFFSET) * FLASH_SAVE_MAIN_MULTIPLIER;
    const bool owl = exists && (SysFlashrom_ReadData(buffer.data(), gFlashSaveStartPages[owlIndex],
                                                     gFlashSaveNumPages[owlIndex]) == 0);
    if (!exists) {
        CreateFile(fileNum, sBoot.playerName, buffer);
    }

    // Sram_OpenSave reads the file select's owl-save flags; a blank file select with the one it needs.
    FileSelectState* fileSelect = static_cast<FileSelectState*>(calloc(1, sizeof(FileSelectState)));
    fileSelect->isOwlSave[fileNum + FILE_NUM_OWL_SAVE_OFFSET] = owl;
    SramContext sram = {};
    sram.saveBuf = buffer.data();
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.fileNum = fileNum;
    Sram_OpenSave(fileSelect, &sram);
    free(fileSelect);
    FoldCalendar("booting");  // a raw day past 4: the moon's dark sky over the first scene
    ClearWeather("booting");  // BUG-0157
    StartFirstDay("booting"); // BUG-0100: before the file's load hooks (the randomizer's clock) read the day

    // FileSelect_LoadGame.
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.respawnFlag = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance = ENTR_LOAD_OPENING;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.ambienceId = AMBIENCE_ID_DISABLED;
    gSaveContext.showTitleCard = true;
    gSaveContext.dogParams = 0;
    for (s32 i = 0; i < TIMER_ID_MAX; i++) {
        gSaveContext.timerStates[i] = TIMER_STATE_OFF;
    }
    gSaveContext.prevHudVisibility = HUD_VISIBILITY_ALL;
    gSaveContext.nayrusLoveTimer = 0;
    gSaveContext.healthAccumulator = 0;
    gSaveContext.magicFlag = 0;
    gSaveContext.forcedSeqId = 0;
    gSaveContext.skyboxTime = CLOCK_TIME(0, 0);
    gSaveContext.nextTransitionType = TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.chamberCutsceneNum = 0;
    gSaveContext.nextDayTime = NEXT_TIME_NONE;
    gSaveContext.retainWeatherMode = false;
    gSaveContext.buttonStatus[EQUIP_SLOT_B] = BTN_ENABLED;
    gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT] = BTN_ENABLED;
    gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN] = BTN_ENABLED;
    gSaveContext.buttonStatus[EQUIP_SLOT_C_RIGHT] = BTN_ENABLED;
    gSaveContext.shipSaveContext.dpad.status[EQUIP_SLOT_D_RIGHT] = BTN_ENABLED;
    gSaveContext.shipSaveContext.dpad.status[EQUIP_SLOT_D_LEFT] = BTN_ENABLED;
    gSaveContext.shipSaveContext.dpad.status[EQUIP_SLOT_D_DOWN] = BTN_ENABLED;
    gSaveContext.shipSaveContext.dpad.status[EQUIP_SLOT_D_UP] = BTN_ENABLED;
    memset(gSaveContext.masksGivenOnMoon, 0, sizeof(gSaveContext.masksGivenOnMoon));
    gSaveContext.buttonStatus[EQUIP_SLOT_A] = BTN_ENABLED;
    gSaveContext.hudVisibilityForceButtonAlphasByStatus = false;
    gSaveContext.nextHudVisibility = HUD_VISIBILITY_IDLE;
    gSaveContext.hudVisibility = HUD_VISIBILITY_IDLE;
    gSaveContext.hudVisibilityTimer = 0;
    gSaveContext.save.saveInfo.playerData.tatlTimer = 0;
    GameInteractor_ExecuteOnSaveLoad(gSaveContext.fileNum);

    ApplyShared(&sBoot);
    ApplyArrivalBody(&sBoot);
    if (sBoot.flags & MS_TRANSFER_FLAG_RANDO_ENTRANCE) {
        // M8: a shuffled entrance of Hyrule leads to Termina: the file starts there, not where it was saved.
        gSaveContext.save.entrance = MergeShipEntrances_Arrive((s16)sBoot.entrance, true);
        gSaveContext.save.cutsceneIndex = 0;
    } else if (sBoot.flags & MS_TRANSFER_FLAG_OWL_WARP) {
        // The Song of Soaring from Hyrule: the file starts at the owl statue.
        gSaveContext.save.entrance = ArrivalEntrance(&sBoot);
        gSaveContext.save.cutsceneIndex = 0;
    } else if (sBoot.flags & MS_TRANSFER_FLAG_ANCHOR_WARP) {
        // BUG-0036: Anchor's warp to a teammate: the file starts at the teammate's entrance (the exact place needs a
        // play state, which a booting file has not got yet).
        gSaveContext.save.entrance = (u16)sBoot.entrance;
        gSaveContext.save.cutsceneIndex = 0;
        sArrivalAnchorWarp.set = false;
    }

    sLinked = true;
    sLinkedSlot = sBoot.saveSlot;
    sLinkedFileNum = fileNum;
    sDoor = Door::Idle;
    sReturnExitTaken = false;
    DoorLog("arrived", "%s file %d (\"%s\") %s: entrance 0x%X, first cycle %s, day %d", exists ? "loaded" : "created",
            fileNum + 1, sBoot.playerName, owl ? "(owl save)" : "", gSaveContext.save.entrance,
            PrologueOver() ? "over" : "not over", gSaveContext.save.day);
    DoorLog("arrived", "file %d is %s, finalSeed %u (Hyrule's unified seed: %u)", fileNum + 1,
            (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) ? "a randomizer file" : "not a randomizer file",
            (gSaveContext.save.shipSaveInfo.saveType == SAVETYPE_RANDO) ? gSaveContext.save.shipSaveInfo.rando.finalSeed
                                                                        : 0u,
            (sBoot.flags & MS_TRANSFER_FLAG_UNIFIED_SEED) ? sBoot.unifiedSeed : 0u);

    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState, Play_Init, sizeof(PlayState));
}

// Hand the player back to Hyrule: the Clock Tower passage's fade-out has finished (the door), or a warp song
// to one of Ocarina of Time's warp pads was played (MergeShip_RequestHyruleWarp).
void RequestHyruleTransfer(PlayState* play, s16 entrance, u32 flags) {
    MsTransfer transfer = {};
    transfer.size = sizeof(MsTransfer);
    transfer.from = MS_ENGINE_2SHIP;
    transfer.to = MS_ENGINE_SOH;
    transfer.entrance = entrance;
    transfer.flags = flags;
    transfer.saveSlot = sLinkedSlot;
    if (sPendingAnchorWarp.set) {
        transfer.anchorRoom = sPendingAnchorWarp.room;
        transfer.anchorPos[0] = sPendingAnchorWarp.pos[0];
        transfer.anchorPos[1] = sPendingAnchorWarp.pos[1];
        transfer.anchorPos[2] = sPendingAnchorWarp.pos[2];
        transfer.anchorYaw = sPendingAnchorWarp.yaw;
        sPendingAnchorWarp.set = false;
    }
    // Link goes back as Termina has him: a Deku, Goron, Zora or Fierce Deity, or the human of his age.
    transfer.flags |= MS_TRANSFER_FLAG_PLAYER_BODY;
    transfer.playerAge = MergeShip_Hyrule_GetDepartureAge();
    transfer.playerForm = MsFormFromMM(gSaveContext.save.playerForm, transfer.playerAge);

    // MM's first cycle takes part too when sharing during it is on: what it holds back was never taken from the shared
    // save, and a capture only adds or sets what Termina holds.
    if ((sHost->GetSharedSave != nullptr) && (PrologueOver() || ShareFirstCycle())) {
        MsSharedSave* shared = static_cast<MsSharedSave*>(sHost->GetSharedSave(sHost->host, sLinkedSlot));
        MergeShipShared_Capture(shared);
        sHost->CommitSharedSave(sHost->host, sLinkedSlot);
        transfer.flags |= MS_TRANSFER_FLAG_SHARED_SAVE;
    }
    DoorLog("away", "%s -> Hyrule entrance 0x%X (file slot %d, scene %d)",
            (flags & MS_TRANSFER_FLAG_TERMINA_DOOR)      ? "Clock Tower passage"
            : (flags & MS_TRANSFER_FLAG_RANDO_ENTRANCE) ? "shuffled entrance"
                                                         : "warp song",
            entrance, sLinkedSlot, play->sceneId);
    sHost->RequestTransfer(sHost->host, &transfer);
    sTransferRequested = true;
}

} // namespace

extern "C" {

s32 MergeShipHost_IsHosted(void) {
    return sHost != nullptr;
}

s32 MergeShipHost_TerminaDoorActive(void) {
    return DoorEnabled() && sLinked && (gSaveContext.fileNum == sLinkedFileNum);
}

void* MergeShipHost_SharedSave(void) {
    if ((sHost == nullptr) || (sHost->GetSharedSave == nullptr) || !sLinked ||
        (gSaveContext.fileNum != sLinkedFileNum)) {
        return nullptr;
    }
    return sHost->GetSharedSave(sHost->host, sLinkedSlot);
}

void MergeShipHost_CommitSharedSave(void) {
    if ((sHost != nullptr) && (sHost->CommitSharedSave != nullptr) && sLinked) {
        sHost->CommitSharedSave(sHost->host, sLinkedSlot);
    }
}

s32 MergeShipHost_SharingAllowed(void) {
    return PrologueOver() || ShareFirstCycle();
}

void MergeShipHost_OnMoonCrashReset(void) {
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());

    if ((shared == nullptr) || (shared->lastWriter == 0)) {
        return;
    }
    MergeShipShared_Apply(shared, 0);
    sHeldBack = false;
    MergeShipHostTest_Log("SHARED re-applied after the moon crash reload");
}

void MergeShipHost_OnSceneExit(PlayState* play) {
    if (MergeShipHost_TerminaDoorActive() && (play->sceneId == SCENE_INSIDETOWER) &&
        (play->nextEntrance == ENTRANCE(OPENING_DUNGEON, 1))) {
        sReturnExitTaken = true;
        DoorLog("exit", "the Clock Tower's passage (room %d)", play->roomCtx.curRoom.num);
    }
}

s32 MergeShipHost_HoldTransition(PlayState* play) {
    if (sDoor == Door::Arrived) {
        play->nextEntrance = sArrivalRando ? MergeShipEntrances_Arrive((s16)sArrivalEntrance, false) : sArrivalEntrance;
        ApplyArrivalAnchorWarp(play);
        sDoor = Door::Idle;
        sReturnExitTaken = false;
        sRandoTrip = sArrivalRando = false;
        DoorLog("idle", "arrival committed: entrance 0x%X", play->nextEntrance);
        return false;
    }
    if ((sDoor == Door::Away) && sRandoTrip) {
        return true;
    }
    // M8: a shuffled entrance (MergeShipEntrances.h) stays in Termina (nextEntrance replaced) or leads to Hyrule.
    if ((sDoor == Door::Idle) && !sReturnExitTaken) {
        const s32 hyrule = MergeShipEntrances_OnCommit(play);
        if (hyrule >= 0) {
            if ((sHost != nullptr) && (sHost->RequestTransfer != nullptr) && sLinked &&
                (gSaveContext.fileNum == sLinkedFileNum)) {
                sDoor = Door::Away;
                sRandoTrip = true;
                sRandoVanilla = play->nextEntrance;
                RequestHyruleTransfer(play, (s16)hyrule, MS_TRANSFER_FLAG_RANDO_ENTRANCE);
                return true;
            }
            DoorLog("idle", "entrance 0x%X leads to Hyrule 0x%X, but this Termina has no Hyrule file: MM's own exit",
                    play->nextEntrance, hyrule);
        }
    }
    if (!sReturnExitTaken) {
        return false;
    }
    if (sDoor == Door::Away) {
        return true;
    }
    if (!MergeShipHost_TerminaDoorActive()) {
        sReturnExitTaken = false; // Majora's Mask's own exit
        return false;
    }
    sDoor = Door::Away;
    RequestHyruleTransfer(play, MS_ENTRANCE_SOH_LOST_WOODS, MS_TRANSFER_FLAG_TERMINA_DOOR);
    return true;
}

s32 MergeShipHost_RequestHyruleWarp(u16 ootEntrance) {
    // Only a Termina that came from a Hyrule file has a Hyrule to warp back to.
    if ((sHost == nullptr) || (sHost->RequestTransfer == nullptr) || !sLinked ||
        (gSaveContext.fileNum != sLinkedFileNum) || (gPlayState == nullptr)) {
        return false;
    }
    // Termina stays where the song was played; coming back later resumes it there (MergeShipHost_OnResume).
    RequestHyruleTransfer(gPlayState, (s16)ootEntrance, MS_TRANSFER_FLAG_NONE);
    return true;
}

s32 MergeShipHost_CanReachHyrule(void) {
    return (sHost != nullptr) && (sHost->RequestTransfer != nullptr) && sLinked && (gSaveContext.fileNum == sLinkedFileNum) &&
           (gPlayState != nullptr);
}

s32 MergeShipHost_RequestHyruleAnchorWarp(s32 ootEntrance, s32 room, const f32 pos[3], s32 yaw) {
    if (!MergeShipHost_CanReachHyrule()) {
        return false;
    }
    sPendingAnchorWarp.set = true;
    sPendingAnchorWarp.entrance = ootEntrance;
    sPendingAnchorWarp.room = room;
    sPendingAnchorWarp.pos[0] = pos[0];
    sPendingAnchorWarp.pos[1] = pos[1];
    sPendingAnchorWarp.pos[2] = pos[2];
    sPendingAnchorWarp.yaw = yaw;
    // Termina stays where the player was, as with a warp song to Hyrule (MergeShipHost_RequestHyruleWarp).
    RequestHyruleTransfer(gPlayState, (s16)ootEntrance, MS_TRANSFER_FLAG_ANCHOR_WARP);
    return true;
}

s32 MergeShipHost_StoneDoorsStayOpen(void) {
    return MergeShipHost_TerminaDoorActive();
}

const char* MergeShipHost_DoorState(void) {
    return sDoorState;
}

} // extern "C"

void MergeShipHost_SetServices(const MsHostServices* services) {
    sHost = services;
}

// BenPort.cpp, once 2Ship's config is loaded and before ShipInit::InitAll turns the options' hooks on (the defaults
// set in MergeShipHost_SetServices came before the config existed, and were lost: the user, 2026-09-25, "cutscenes
// are still playing"). Only settings the player has not set; saved at once.
extern "C" void MergeShipHost_ApplyDefaultSettings() {
    if (sHost == nullptr) {
        return;
    }
    // In mergeship.exe, pickups do not stop Link but for major items (the user, 2026-09-24: "don't get drop spammed,
    // and turn on the Skip junk pickups thing and only make item stuff pop up above link for Major items"): 2Ship's
    // "Skip Item Get Cutscene" is "Everything But Major" unless the player set it in the menu.
    if (CVarGetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", -1) == -1) {
        CVarSetInteger("gEnhancements.Cutscenes.SkipGetItemCutscenes", 2);
    }
    // Termina's cutscenes are skipped (the user, 2026-09-24: "put any skip cutscene and entrance stuff on for mm as
    // well we dont need those entrance cutscenes or story cutscenes to be watched"), unless the player set them.
    for (const char* cvar : { "gEnhancements.Cutscenes.SkipEntranceCutscenes", "gEnhancements.Cutscenes.SkipStoryCutscenes",
                              "gEnhancements.Cutscenes.SkipOnePointCutscenes", "gEnhancements.Cutscenes.SkipMiscInteractions",
                              "gEnhancements.Cutscenes.SkipEnemyCutscenes", "gEnhancements.Cutscenes.SkipIntroSequence",
                              "gEnhancements.Cutscenes.SkipFirstCycle" }) {
        if (CVarGetInteger(cvar, -1) == -1) {
            CVarSetInteger(cvar, 1);
        }
    }
    // The Razor Sword does not break and is not taken by the Song of Time (the user, 2026-09-24: "it shouldn't break").
    if (CVarGetInteger("gCheats.UnbreakableRazorSword", -1) == -1) {
        CVarSetInteger("gCheats.UnbreakableRazorSword", 1);
    }
    if (CVarGetInteger("gEnhancements.Cycle.DoNotResetRazorSword", -1) == -1) {
        CVarSetInteger("gEnhancements.Cycle.DoNotResetRazorSword", 1);
    }
    CVarSave();
}

void MergeShipHost_BeforeFrame() {
    // The first frame ever creates the console logo's game state; the arrival replaces it on the one after.
    if (sBootPending && (gGameState != nullptr)) {
        sBootPending = false;
        Boot();
    }
    DeliverHeldBack();
}

bool MergeShipHost_TakeTransferRequested() {
    const bool requested = sTransferRequested;
    sTransferRequested = false;
    return requested;
}

void MergeShipHost_OnResume(const MsTransfer* arrival) {
    if (arrival == nullptr) {
        // A manual switch (Ctrl+F12) while the player is in Hyrule: Termina carries on, from Majora's Mask's own exit.
        if (sDoor == Door::Away) {
            sArrivalEntrance = sRandoTrip ? sRandoVanilla : ENTRANCE(OPENING_DUNGEON, 1);
            sArrivalRando = false;
            sDoor = Door::Arrived;
            DoorLog("arrived", "switched back without a transfer: Majora's Mask's own exit");
        }
        return;
    }
    // The unified seed the Hyrule file plays, if any: Termina's file of this slot is a randomizer file of it
    // (Rando/MergeShip/UnifiedSeed.h; Boot creates or remakes the file).
    Rando::MergeShip::OnSeedTransfer(arrival);
    // M8: the seed's overrides of Termina's entrances come with every trip from Hyrule.
    MergeShipEntrances_OnTransfer(arrival);
    // M6/M7: a unified seed's Hyrule checks, for Termina's hints and check tracker (Rando/MergeShip/UnifiedHyrule.h).
    Rando::MergeShip::OnWorldCatalog(arrival);
    // Singleplayer: Termina as Ocarina of Time's New Game Plus, Navi in Tatl's place (MergeShipSingleplayer.h).
    MergeShip_Singleplayer_OnTransfer(arrival);
    if (arrival->entrance < 0) {
        // The host turned the trip down: carry on in Termina from Majora's Mask's own exit.
        if (sDoor == Door::Away) {
            sArrivalEntrance = sRandoTrip ? sRandoVanilla : ENTRANCE(OPENING_DUNGEON, 1);
            sArrivalRando = false;
            sDoor = Door::Arrived;
            DoorLog("arrived", "the host refused the trip");
        }
        return;
    }
    const s32 fileNum = FileNumForSlot(arrival->saveSlot);
    // (A loaded file that is not the Hyrule file's unified seed's is booted again, which remakes it: UnifiedSeed.h.)
    // The Hyrule file started its slot anew (a new file or seed, SoH MergeShipHost_ClaimSharedSave): Termina's file of
    // the slot is the earlier playthrough's, made again below.
    MsSharedSave* arrivalShared = ((arrival->flags & MS_TRANSFER_FLAG_SHARED_SAVE) && (arrival->sharedSave != nullptr) &&
                                   (arrival->sharedSaveSize >= sizeof(MsSharedSave)))
                                      ? static_cast<MsSharedSave*>(arrival->sharedSave)
                                      : nullptr;
    const bool staleTermina = (arrivalShared != nullptr) && arrivalShared->terminaFileStale;
    // Anchor: Termina's file of this slot belongs to the Hyrule file the player comes from, and to its playthrough, until
    // the next arrival -- even when a manual switch later brings Termina back with SoH on another file or seed.
    {
        const MsSharedSave* linkedShared =
            (arrivalShared != nullptr)
                ? arrivalShared
                : ((sHost->GetSharedSave != nullptr)
                       ? static_cast<const MsSharedSave*>(sHost->GetSharedSave(sHost->host, arrival->saveSlot))
                       : nullptr);
        if (linkedShared != nullptr) {
            MergeShipAnchor_OnHyruleArrival(linkedShared->hyruleFileId);
        }
    }
    const bool sameFile = sLinked && (sLinkedSlot == arrival->saveSlot) && (gSaveContext.fileNum == fileNum) &&
                          !Rando::MergeShip::LoadedFileNeedsSeed() && !staleTermina;
    const bool rando = (arrival->flags & MS_TRANSFER_FLAG_RANDO_ENTRANCE) != 0;
    TakeArrivalAnchorWarp(arrival);
    if (sameFile && (sDoor == Door::Away)) {
        // Back through the door this session: the held transition takes the player out of the Clock Tower (or to the
        // owl statue the Song of Soaring chose in Hyrule).
        ApplyShared(arrival);
        ApplyArrivalBody(arrival);
        sArrivalEntrance = ArrivalEntrance(arrival);
        sArrivalRando = rando;
        if (!rando) {
            gSaveContext.respawnFlag = 0; // a shuffled grotto's way out may have left MM's grotto respawn pending
        }
        sDoor = Door::Arrived;
        FoldCalendar("arrived");
        ClearWeather("arrived"); // BUG-0157
        StartFirstDay("arrived"); // BUG-0100: a file still on day 0 from an earlier trip
        DoorLog("arrived", "back from Hyrule: entrance 0x%X, day %d", sArrivalEntrance, gSaveContext.save.day);
        return;
    }
    if (sameFile && (gPlayState != nullptr) && (gSaveContext.gameMode == GAMEMODE_NORMAL)) {
        // The player switched here by hand in between and is somewhere in Termina: take them to the door anyway.
        ApplyShared(arrival);
        ApplyArrivalBody(arrival);
        FoldCalendar("arrived");
        ClearWeather("arrived");  // BUG-0157
        StartFirstDay("arrived"); // (the arrival's scene builds its sky from the day: before the transition below)
        gPlayState->nextEntrance =
            rando ? MergeShipEntrances_Arrive((s16)arrival->entrance, false) : ArrivalEntrance(arrival);
        ApplyArrivalAnchorWarp(gPlayState);
        gPlayState->transitionTrigger = TRANS_TRIGGER_START;
        gPlayState->transitionType =
            (arrival->flags & MS_TRANSFER_FLAG_OWL_WARP) ? TRANS_TYPE_FADE_WHITE : TRANS_TYPE_FADE_BLACK;
        sDoor = Door::Idle;
        DoorLog("arrived", "back from Hyrule while playing: entrance 0x%X", arrival->entrance);
        return;
    }
    // A new session (or another file): load this file's Termina before the next frame.
    sBoot = *arrival;
    sBoot.playerName[sizeof(sBoot.playerName) - 1] = '\0';
    sBootPending = true;
    sDoor = Door::Idle;
    DoorLog("booting", "Hyrule file slot %d -> Majora's Mask file %d", arrival->saveSlot, fileNum + 1);
}

#else // !MERGESHIP_HOSTED: 2ship.exe

extern "C" {
s32 MergeShipHost_IsHosted(void) {
    return 0;
}
s32 MergeShipHost_TerminaDoorActive(void) {
    return 0;
}
void* MergeShipHost_SharedSave(void) {
    return nullptr;
}
void MergeShipHost_CommitSharedSave(void) {
}
s32 MergeShipHost_SharingAllowed(void) {
    return 0;
}
void MergeShipHost_OnMoonCrashReset(void) {
}
void MergeShipHost_OnSceneExit(PlayState* play) {
    (void)play;
}
s32 MergeShipHost_HoldTransition(PlayState* play) {
    (void)play;
    return 0;
}
s32 MergeShipHost_StoneDoorsStayOpen(void) {
    return 0;
}
s32 MergeShipHost_CanReachHyrule(void) {
    return 0;
}

s32 MergeShipHost_RequestHyruleAnchorWarp(s32 ootEntrance, s32 room, const f32 pos[3], s32 yaw) {
    return 0;
}

s32 MergeShipHost_RequestHyruleWarp(u16 ootEntrance) {
    (void)ootEntrance;
    return 0;
}
const char* MergeShipHost_DoorState(void) {
    return "not hosted";
}
}

#endif // MERGESHIP_HOSTED
