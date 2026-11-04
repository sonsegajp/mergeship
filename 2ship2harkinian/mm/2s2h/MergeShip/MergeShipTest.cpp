// MergeShip2 -- scripted in-game test harness for 2 Ship 2 Harkinian (the counterpart of SoH's MergeShipTest.cpp).
//
// Booting the game and reading a log line proves only that something was built. This harness drives the real game
// with scripted controller input and records what the game actually did, frame by frame, so a feature can be shown to
// work rather than assumed to.
//
// It is inert unless MERGESHIP_TEST_SCRIPT names a script file, so nothing it does reaches a normal session or the
// user's saved configuration (it never saves CVars; quit ends the process without the shutdown save).
//
// Script: one command per line, '#' starts a comment.
//
//   boot <entrance> [form]          start the debug save at an MM entrance (hex or decimal); form 0 FD, 1 Goron,
//                                   2 Zora, 3 Deku, 4 human (default)
//   waitplay [frames]               wait until gameplay is running, then N more frames
//   wait <frames>                   neutral input for N frames
//   hold <buttons> <frames>         hold buttons, e.g. "hold A+B 10"
//   stick <x> <y> <frames> [buttons] hold the stick (and optional buttons)
//   press <buttons>                 one frame down, one frame up
//   notes <buttons...>              ocarina notes: each is pressed 6 frames and released 6 frames
//   give <item>                     Item_Give
//   hyrulequest <bit|all>           set an Ocarina of Time quest bit (MERGESHIP_OOT_QUEST_*) in the MergeShip save
//   getitem <item>                  hold up an Ocarina of Time item (ITEM_OOT_* or 0x100 + quest bit)
//   proxyrow <dist> <item...>       randomizer items lying in a row dist in front of the player, 45 apart, drawn as
//                                   2Ship's randomizer draws a check's item (Rando::DrawItem); an item is an RI_ name
//                                   (RI_OOT_BOMB_BAG, RI_MAGIC_JAR_BIG) or a unified key (BOMB_BAG = RI_OOT_BOMB_BAG)
//   proxyaudit                      every Hyrule item 2Ship can hold (RI_OOT_*): its Ocarina of Time model row, and
//                                   FALLBACK for any with none (the Huge Rupee)
//   cbutton <1-3> <item>            put an item on C-left(1)/C-down(2)/C-right(3)
//   questpage <0|1> [point]         show / hide the Hyrule quest sub-page, place its cursor
//   warpreq <ootEntrance>           MergeShip_RequestHyruleWarp
//   savejson                        round-trip ShipSaveInfo through the save's JSON and log the MergeShip key
//   state                           log the ocarina / message / audio state
//   anchor <command ...>            Anchor: enable/disable/state/loop/give/send/dummies (2s2h/Network/Anchor)
//   trace on|off                    per-frame state
//   cvar <name> <int>               set an integer CVar for this run (never saved), as the menu does (ShipInit)
//   time <hh> <mm>                  set the time of day
//   mark <text>                     a marker in the log
//   shot <name>                     screenshot of the game window, <name>.bmp
//   quit                            flush and end the process (no shutdown save)
//
// Film movement (the trailer harness; the same ops as SoH's MergeShipTest.cpp where they exist there):
//   bootweekevent <flag...>         weekEventReg flags set on the debug save at boot, before the scene loads (packed
//                                   (index << 8) | mask, e.g. 0x1402, or woodfall_raised / woodfall_cleared)
//   pos <x> <y> <z> [yaw]           place the player
//   walkto <x> <z> <frames> [r]     walk (the stick steered with the active camera's input yaw, as SoH's
//                                   StickTowardYaw) towards a point until within r (default 12) or frames run out
//   actors <actorId>                log every actor with that id (the n of "infront" / "flowerfly" is this order)
//   infront <actorId> <dist> [n]    stand dist in front of the n-th actor with that id, facing it
//   flowerfly <n|x,z> [tx tz] [charge] [glide]
//                                   Deku Link flies from a Deku flower (Obj_Etcetera) as MM's player does it: walk onto
//                                   the n-th flower's pad (or the flower nearest x,z; -1 the nearest to Link), press and
//                                   hold A to burrow, keep A held `charge` frames once burrowed (default 20; 10 or more
//                                   charges the launch that opens the petals) while the stick turns him toward (tx,tz),
//                                   release to launch, then glide steered toward (tx,tz): braking to stop above it and
//                                   pressing A to drop when it is in reach, or flying on until the propeller gives out
//                                   (600 units from a pink flower, 960 from a gold one) and steering the fall. A target
//                                   on a Deku flower is landed on with A held, so Link burrows straight into it; the
//                                   next line must then be another flowerfly (it carries on from the burrow, A still
//                                   held). "-" for tx: no target (hover where he rose, drop after `glide` frames,
//                                   default 300). Every state change is logged (FLY ...).
//   cineyaw auto|move|shape         the yaw the film camera trails: Link's model (shape), his movement yaw (move), or
//                                   auto (default): move during a flowerfly (his model spins while he burrows)
//
// Buttons: A B Z R L S CU CD CL CR DU DD DL DR, joined with '+'.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32 // MergeShip2 Android: Windows only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#else
#include <strings.h>
#include <unistd.h>
#define _stricmp strcasecmp
#endif

#include <spdlog/spdlog.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <nlohmann/json.hpp>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/MergeShip/MergeShipHyrule.h"
#include "2s2h/Rando/MergeShip/UnifiedGoal.h"
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h"
#include "2s2h/Rando/MergeShip/UnifiedSeed.h"
#include "2s2h/Rando/MergeShip/OoTGiDraw.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/CustomItem/CustomItem.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h" // icetrapgi (BUG-0097)
extern "C" s32 gMergeShipAudioTrace; // audiotrace (playback.c)
#include "2s2h/MergeShip/MergeShipAssets.h"
#include "2s2h/MergeShip/MergeShipHost.h" // MergeShipHost_SharedSave (hyruleextras)
#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip_Own_SetOoTBottle (ootbottle)
#include <mergeship/SharedSave.h>
#include "BenJsonConversions.hpp"

extern "C" {
#include "z64.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h" // horse
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "main.h"
#include "overlays/gamestates/ovl_select/z_select.h"
extern PlayState* gPlayState;
extern GameState* gGameState;
extern AudioContext gAudioCtx;
extern char** gSequenceMap;
extern size_t gSequenceMapSize;
void MergeShipAnchor_TestCommand(const char* args); // 2s2h/Network/Anchor/MergeShipAnchorHost.cpp
}

// MergeShip2: Ocarina of Time's equipment commands (ooteq*, MergeShipOoTEquip.cpp)
bool MergeShipTest_OoTEquipCommand(const std::string& op, const std::vector<std::string>& args);
// MergeShip2: Ocarina of Time's items commands (oot*, MergeShipOoTItems.cpp)
bool MergeShipTest_OoTItemsCommand(const std::string& op, const std::vector<std::string>& args);
// MergeShip2: adult Link commands (adult*, MergeShipAdultLink.cpp)
bool MergeShipTest_AdultLinkCommand(const std::string& op, const std::vector<std::string>& args);
// MergeShip2: Termina Equipment commands (terminaeq*, MergeShipTerminaEquip.cpp)
bool MergeShipTest_TerminaEquipCommand(const std::string& op, const std::vector<std::string>& args);

#include <mergeship/CineCam.hpp> // the film camera ("cinecam")

std::string MergeShipCine_Hud(bool off); // below: "hud off|on", shared with the door harness

#include "MergeShipSingleplayer.h"
static s32 sTatlCallHold = 0;
extern "C" s32 gMergeShipSfxProbe;

namespace {

struct Command {
    std::string op;
    std::vector<std::string> args;
    int line;
};

struct Harness {
    bool active = false;
    std::vector<Command> commands;
    size_t pc = 0;
    int waitFrames = 0;
    bool waitingForPlay = false;
    int afterPlayFrames = 0;
    u16 buttons = 0;
    s8 stickX = 0;
    s8 stickY = 0;
    u16 prevButtons = 0;
    bool pressRelease = false;
    std::vector<u16> noteQueue; // "notes": buttons, one per 12 frames
    int noteFrame = 0;
    bool trace = false;
    u64 frame = 0;
    FILE* log = nullptr;
    std::string dir;
    bool booted = false;
    s32 bootEntrance = -1;
    s32 bootForm = PLAYER_FORM_HUMAN;
    std::vector<s32> bootWeekEvents; // "bootweekevent": set on the debug save before the first scene loads
    int steerFrames = 0;             // "walkto": frames left
    Vec3f steerTarget = {};
    f32 steerRadius = 12.0f;
};

Harness sH;

void Log(const char* fmt, ...) {
    if (sH.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sH.log, "[%06llu] ", (unsigned long long)sH.frame);
    vfprintf(sH.log, fmt, ap);
    fputc('\n', sH.log);
    va_end(ap);
    fflush(sH.log);
}

u16 ParseButtons(const std::string& s) {
    static const struct {
        const char* name;
        u16 bit;
    } kButtons[] = {
        { "A", BTN_A },     { "B", BTN_B },       { "Z", BTN_Z },       { "R", BTN_R },      { "L", BTN_L },
        { "S", BTN_START }, { "CU", BTN_CUP },    { "CD", BTN_CDOWN },  { "CL", BTN_CLEFT }, { "CR", BTN_CRIGHT },
        { "DU", BTN_DUP },  { "DD", BTN_DDOWN },  { "DL", BTN_DLEFT },  { "DR", BTN_DRIGHT },
    };
    u16 mask = 0;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        bool found = false;
        for (auto& b : kButtons) {
            if (_stricmp(tok.c_str(), b.name) == 0) {
                mask |= b.bit;
                found = true;
            }
        }
        if (!found && !tok.empty() && tok != "-") {
            Log("WARN unknown button '%s'", tok.c_str());
        }
    }
    return mask;
}

long ParseInt(const std::string& s) {
    return strtol(s.c_str(), nullptr, 0);
}

// The unified key of a proxy RI_OOT_<key> (MsItems.def), MSI_MAX for any other item.
MsItemKey ProxyKeyOf(RandoItemId ri) {
    switch (ri) {
#define MS_ITEM(key, rg, ri, kind, persist, oot, mm, maxLevel, name)
#define MS_ITEM_OOT(key, rg, kind, persist, oot, mm, maxLevel, name)     case RI_OOT_##key:                                                     return MSI_##key;
#include <mergeship/MsItems.def>
#undef MS_ITEM
#undef MS_ITEM_OOT
        default:
            return MSI_MAX;
    }
}

// The game window as the compositor last presented it (PW_RENDERFULLCONTENT reads the DWM surface, which works for
// a Direct3D swap chain).
void Screenshot(const std::string& name) {
#ifdef _WIN32
    struct Find {
        DWORD pid;
        HWND best;
        LONG bestArea;
    } find = { GetCurrentProcessId(), nullptr, 0 };
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            Find* f = (Find*)lp;
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != f->pid || !IsWindowVisible(hwnd)) {
                return TRUE;
            }
            RECT r;
            GetClientRect(hwnd, &r);
            LONG area = (r.right - r.left) * (r.bottom - r.top);
            if (area > f->bestArea) {
                f->bestArea = area;
                f->best = hwnd;
            }
            return TRUE;
        },
        (LPARAM)&find);
    if (find.best == nullptr) {
        Log("SHOT %s failed: no window", name.c_str());
        return;
    }
    RECT rc;
    GetClientRect(find.best, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HDC screen = GetDC(find.best);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    BOOL ok = PrintWindow(find.best, mem, PW_CLIENTONLY | 0x2 /* PW_RENDERFULLCONTENT */);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    int stride = ((w * 3 + 3) & ~3);
    std::vector<unsigned char> pixels((size_t)stride * h);
    GetDIBits(mem, bmp, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(find.best, screen);

    std::string path = sH.dir + "\\" + name + ".bmp";
    FILE* f = fopen(path.c_str(), "wb");
    if (f == nullptr) {
        Log("SHOT %s failed: cannot write %s", name.c_str(), path.c_str());
        return;
    }
    BITMAPFILEHEADER bf = {};
    bf.bfType = 0x4D42;
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bf.bfSize = bf.bfOffBits + (DWORD)pixels.size();
    fwrite(&bf, sizeof(bf), 1, f);
    fwrite(&bi, sizeof(bi), 1, f);
    fwrite(pixels.data(), 1, pixels.size(), f);
    fclose(f);
    Log("SHOT %s %dx%d printwindow=%d", path.c_str(), w, h, ok);
#else
    (void)name; // (window capture: Windows only)
#endif
}

void LogState(const char* tag) {
    if (gPlayState == nullptr) {
        Log("%s no play state", tag);
        return;
    }
    PlayState* play = gPlayState;
    MessageContext* msgCtx = &play->msgCtx;
    SequencePlayer* fanfare = &gAudioCtx.seqPlayers[SEQ_PLAYER_FANFARE];
    Player* player = GET_PLAYER(play);
    Log("%s scene=%d form=%d msgMode=%d textId=0x%X ocMode=%d ocAction=%d songPlayed=%d lastPlayed=%d endType=%d "
        "choice=%d fanfare=%d seq=%d(%s) instrument? quest=0x%06X mmQuest=0x%08X pause=%d page=%d time=0x%04X "
        "sunsSong=%d player=(%.0f,%.0f,%.0f) s1=%08X",
        tag, play->sceneId, player ? player->transformation : -1, msgCtx->msgMode, msgCtx->currentTextId,
        msgCtx->ocarinaMode, msgCtx->ocarinaAction, msgCtx->songPlayed, msgCtx->lastPlayedSong,
        msgCtx->textboxEndType, msgCtx->choiceIndex, fanfare->enabled, fanfare->seqId,
        ((fanfare->seqId < gSequenceMapSize) && (gSequenceMap[fanfare->seqId] != nullptr)) ? gSequenceMap[fanfare->seqId]
                                                                                            : "-",
        MergeShip_Hyrule_GetQuestItems(), gSaveContext.save.saveInfo.inventory.questItems, play->pauseCtx.state,
        play->pauseCtx.pageIndex, gSaveContext.save.time, gSaveContext.sunsSongState,
        player ? player->actor.world.pos.x : 0.0f, player ? player->actor.world.pos.y : 0.0f,
        player ? player->actor.world.pos.z : 0.0f, player ? player->stateFlags1 : 0);
    if (player != nullptr) {
        // what is in his hands and what he plays (a sword where none should be: Ganondorf mode)
        const char* anim = (const char*)player->skelAnime.animation;
        Log("%s held ia=%d item=0x%02X rh=%d lh=%d mwa=%d unkAA5=%d bItem=0x%02X anim=%s", tag, player->heldItemAction,
            player->heldItemId, player->rightHandType, player->leftHandType, player->meleeWeaponAnimation,
            player->unk_AA5, Inventory_GetBtnBItem(play),
            ((anim != nullptr) && ((uintptr_t)anim > 0x10000) && (anim[0] == '_')) ? anim : "?");
    }
}

bool InPlay() {
    return (gPlayState != nullptr) && (GET_PLAYER(gPlayState) != nullptr) &&
           (gPlayState->transitionTrigger == TRANS_TRIGGER_OFF) && (gPlayState->transitionMode == TRANS_MODE_OFF);
}

// ---- film movement: walkto, actors / infront, flowerfly ------------------------------------------------------------

// The stick that moves the player towards a world yaw (SoH's StickTowardYaw): the player turns towards the stick's
// angle plus the active camera's input yaw (Player_ProcessControlStick). Unlike Ocarina of Time, MM takes the stick's
// magnitude from the relative axes (each raw axis less a dead zone of 7, clamped to 60: PadUtils_UpdateRelXY) but its
// angle from the raw axes (Lib_GetControlStickData: Math_Atan2S_XY(cur.stick_y, -cur.stick_x)). So the raw stick here
// points exactly along the angle wanted (a raw stick built from the relative one, as SoH builds it, would bend the angle
// by up to 12 degrees off the axes), and its length is the shortest whose relative axes give the magnitude wanted.
// magnitude 100 is full tilt (relative magnitude 60). MM's walk (SPEED_MODE_CURVED) does not move below a relative
// magnitude of 20, i.e. magnitude 34 (Player_CalcSpeedAndYawFromControlStick); the glide and the air (LINEAR) do.
void StickTowardYaw(s16 worldYaw, f32 magnitude) {
    Camera* camera = (gPlayState != nullptr) ? GET_ACTIVE_CAM(gPlayState) : nullptr;
    const f32 want = std::clamp(magnitude, 0.0f, 100.0f) * 0.6f;
    if ((camera == nullptr) || (want < 0.5f)) {
        sH.stickX = sH.stickY = 0;
        return;
    }
    const s16 angle = (s16)(worldYaw - Camera_GetInputDirYaw(camera));
    const f32 dirX = -Math_SinS(angle);
    const f32 dirY = Math_CosS(angle);
    // The raw axis sent for a value (rounded half away from zero), and the relative magnitude the game makes of the raw
    // stick len long along (dirX, dirY).
    auto rawAxis = [](f32 v) -> s32 { return (v < 0.0f) ? -(s32)floorf(0.5f - v) : (s32)floorf(v + 0.5f); };
    auto relMagnitude = [&](f32 len) {
        auto rel = [&](f32 v) -> f32 {
            const s32 a = std::abs(rawAxis(v));
            return (f32)((a > 7) ? std::min(a - 7, 0x43 - 7) : 0);
        };
        const f32 rx = rel(dirX * len);
        const f32 ry = rel(dirY * len);
        return std::min(sqrtf((rx * rx) + (ry * ry)), 60.0f);
    };
    // Monotonic in len, and 60 at len 80 whatever the angle: the shortest len that reaches the magnitude wanted.
    f32 lo = 0.0f;
    f32 hi = 80.0f;
    for (int i = 0; i < 20; i++) {
        const f32 mid = (lo + hi) * 0.5f;
        if (relMagnitude(mid) < want) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    sH.stickX = (s8)rawAxis(dirX * hi);
    sH.stickY = (s8)rawAxis(dirY * hi);
}

// The n-th live actor with an id, in the order "actors" logs them (category by category, each in spawn order).
Actor* NthActor(s16 id, int want, bool log) {
    Actor* chosen = nullptr;
    int n = 0;
    Player* p = GET_PLAYER(gPlayState);
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->id != id) {
                continue;
            }
            if (log) {
                Log("ACTOR id=0x%03X #%d params=0x%04X pos=(%.1f,%.1f,%.1f) yaw=0x%04X dist=%.1f room=%d", id, n,
                    (u16)a->params, a->world.pos.x, a->world.pos.y, a->world.pos.z, (u16)a->shape.rot.y,
                    Math_Vec3f_DistXYZ(&a->world.pos, &p->actor.world.pos), a->room);
            }
            if (n == want) {
                chosen = a;
            }
            n++;
        }
    }
    return chosen;
}

// The live actor with an id nearest a point (XZ).
Actor* NearestActor(s16 id, f32 x, f32 z) {
    Actor* best = nullptr;
    f32 bestDist = 1e30f;
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->id != id) {
                continue;
            }
            const f32 dx = a->world.pos.x - x;
            const f32 dz = a->world.pos.z - z;
            const f32 d = dx * dx + dz * dz;
            if (d < bestDist) {
                bestDist = d;
                best = a;
            }
        }
    }
    return best;
}

// "flowerfly": Deku Link's flight from a Deku flower, driven with the controller alone as MM's player code reads it
// (z_player.c): standing on the flower's burrow pad (a floor poly with flag 0x4000, func_800C9DDC), A pressed makes
// Player_Action_93 burrow him (av1 0 sink, 1 spin down, 2 hidden: STATE3_100 every frame below -1500); while hidden,
// A held counts av2 up (10 = charged, the sparkle) and the stick turns his yaw (func_80855F9C); A released launches him
// (av1 3, then Player_Action_94 with STATE3_200 rising, STATE3_2000 if charged). Falling again, the petals open
// (STATE3_200 off, STATE3_1000000 on) and he glides: the stick steers (the yaw turns 0xFA a frame) and sets the speed,
// which steps 0.25 a frame toward the stick's (a stick opposite his yaw brakes, then flies him backward), up to 8
// (12 units a frame: the actor movement scale is 1.5); A drops him (kakkufinish); 600 units (pink) or 960 (gold) from
// the launch point the propeller gives out and he falls. Landing on a flower pad with A held burrows him at once
// (func_80837134).
enum FlyPhase { FLY_OFF, FLY_WALK, FLY_SETTLE, FLY_BURROW, FLY_CHARGE, FLY_LAUNCH, FLY_RISE, FLY_GLIDE, FLY_DROP };

const char* const kFlyPhaseNames[] = { "off", "walk", "settle", "burrow", "charge", "launch", "rise", "glide", "drop" };

struct FlowerFly {
    int phase = FLY_OFF;
    Vec3f flower = {};
    s32 flowerBgId = BGCHECK_SCENE;
    bool gold = false;
    bool hasTarget = false;
    Vec3f target = {};
    bool targetFlower = false; // the target is on a flower's pad: land on it holding A (chain)
    int chargeFrames = 20;
    int glideFrames = 300;
    int phaseFrames = 0;
    int totalFrames = 0;
    int chargedAt = -1;
    u32 prevFlags3 = 0;
    s8 prevAv1 = -100;
    Vec3f launchPos = {};
    f32 launchY = 0.0f;
    f32 peakY = -1e9f;
    int dropPress = 0;    // frames A is still to be held after the drop press (no chain)
    int groundFrames = 0; // frames on the ground waiting for a chain burrow
};

FlowerFly sFly;

bool FlyActive() {
    return sFly.phase != FLY_OFF;
}

void FlySetPhase(int phase, const char* why) {
    Player* p = GET_PLAYER(gPlayState);
    Log("FLY %s -> %s (%s) pos=(%.1f,%.1f,%.1f) yaw=0x%04X spd=%.2f vy=%.2f s3=%08X av1=%d av2=%d", kFlyPhaseNames[sFly.phase],
        kFlyPhaseNames[phase], why, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, (u16)p->yaw,
        p->speedXZ, p->actor.velocity.y, p->stateFlags3, p->av1.actionVar1, p->av2.actionVar2);
    sFly.phase = phase;
    sFly.phaseFrames = 0;
}

void FlyEnd(const char* why, bool keepA) {
    Player* p = GET_PLAYER(gPlayState);
    const f32 d = sFly.hasTarget ? Math_Vec3f_DistXZ(&p->actor.world.pos, &sFly.target) : 0.0f;
    Log("FLY end (%s) pos=(%.1f,%.1f,%.1f) floorY=%.1f toTarget=%.1f frames=%d peak=%.1f (+%.1f) A=%s s3=%08X", why,
        p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, p->actor.floorHeight, d, sFly.totalFrames,
        sFly.peakY, sFly.peakY - sFly.launchY, keepA ? "held" : "up", p->stateFlags3);
    sFly.phase = FLY_OFF;
    sH.buttons = keepA ? BTN_A : 0;
    sH.stickX = sH.stickY = 0;
}

bool OnFlowerPad(Player* p) {
    return (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && (p->actor.floorPoly != nullptr) &&
           (p->actor.floorBgId != BGCHECK_SCENE) && (p->actor.floorBgId == sFly.flowerBgId) &&
           func_800C9DDC(&gPlayState->colCtx, p->actor.floorPoly, p->actor.floorBgId);
}

// Burrowing or hidden in a flower (Player_Action_93 sets unk_ABC below zero from its first frame).
bool InFlower(Player* p) {
    return (p->stateFlags3 & PLAYER_STATE3_100) || (p->unk_ABC < 0.0f);
}

// One frame of a flowerfly; returns true while it holds the script.
bool FlyFrame() {
    if (!FlyActive()) {
        return false;
    }
    if (!InPlay()) {
        Log("FLY stopped: no play / transition");
        sFly.phase = FLY_OFF;
        sH.buttons = 0;
        sH.stickX = sH.stickY = 0;
        return false;
    }
    Player* p = GET_PLAYER(gPlayState);
    sFly.phaseFrames++;
    sFly.totalFrames++;

    // Every change of the flight's state bits / burrow sub-state, as the game set them.
    const u32 kFlyBits = PLAYER_STATE3_100 | PLAYER_STATE3_200 | PLAYER_STATE3_2000 | PLAYER_STATE3_40000 |
                         PLAYER_STATE3_1000000;
    const u32 bits = p->stateFlags3 & kFlyBits;
    if ((bits != sFly.prevFlags3) || (p->av1.actionVar1 != sFly.prevAv1)) {
        Log("FLY state s3 %08X->%08X av1 %d->%d av2=%d abc=%.0f pos=(%.1f,%.1f,%.1f) vy=%.2f spd=%.2f ground=%d",
            sFly.prevFlags3, bits, sFly.prevAv1, p->av1.actionVar1, p->av2.actionVar2, p->unk_ABC,
            p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, p->actor.velocity.y, p->speedXZ,
            (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ? 1 : 0);
        sFly.prevFlags3 = bits;
        sFly.prevAv1 = p->av1.actionVar1;
    }
    if (p->actor.world.pos.y > sFly.peakY) {
        sFly.peakY = p->actor.world.pos.y;
    }
    if (sFly.totalFrames > 2000) {
        FlyEnd("FAIL: timeout", false);
        return false;
    }

    const bool grounded = (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) != 0;
    const s16 yawToTarget = sFly.hasTarget ? Math_Vec3f_Yaw(&p->actor.world.pos, &sFly.target) : p->yaw;
    const f32 dTarget = sFly.hasTarget ? Math_Vec3f_DistXZ(&p->actor.world.pos, &sFly.target) : 0.0f;

    switch (sFly.phase) {
        case FLY_WALK: {
            sH.buttons = 0;
            const f32 d = Math_Vec3f_DistXZ(&p->actor.world.pos, &sFly.flower);
            if (OnFlowerPad(p) && (d < 9.0f)) {
                sH.stickX = sH.stickY = 0;
                FlySetPhase(FLY_SETTLE, "on the pad");
                break;
            }
            if (sFly.phaseFrames > 450) {
                FlyEnd("FAIL: could not walk onto the flower", false);
                return false;
            }
            // Full tilt until close, then slower so he stops on the pad (his speed follows the stick). Never below 36
            // (relative 21.6): MM's walk does not move at all under a relative magnitude of 20 (SPEED_MODE_CURVED),
            // and just above it creeps at ~1.5 units a frame, so he cannot stall short of the pad.
            const f32 mag = (d > 60.0f) ? 100.0f : std::max(36.0f, d * 1.6f);
            StickTowardYaw(Math_Vec3f_Yaw(&p->actor.world.pos, &sFly.flower), mag);
            if ((sFly.phaseFrames % 20) == 1) {
                Log("FLY walk d=%.1f pos=(%.1f,%.1f,%.1f) spd=%.2f floorBg=%d pad=%d", d, p->actor.world.pos.x,
                    p->actor.world.pos.y, p->actor.world.pos.z, p->speedXZ, p->actor.floorBgId, OnFlowerPad(p) ? 1 : 0);
            }
            break;
        }
        case FLY_SETTLE:
            sH.buttons = 0;
            sH.stickX = sH.stickY = 0;
            if (!OnFlowerPad(p)) {
                FlySetPhase(FLY_WALK, "slid off the pad");
                break;
            }
            if ((p->speedXZ < 0.3f) || (sFly.phaseFrames >= 8)) {
                sH.buttons = BTN_A; // the press: Player_ActionHandler_6 -> func_80839A84 -> func_80836DC0 burrows him
                FlySetPhase(FLY_BURROW, "A pressed on the pad");
            }
            break;
        case FLY_BURROW:
            sH.buttons = BTN_A;
            sH.stickX = sH.stickY = 0;
            if ((p->stateFlags3 & PLAYER_STATE3_100) && (p->av1.actionVar1 == 2)) {
                FlySetPhase(FLY_CHARGE, "hidden in the flower");
                break;
            }
            if ((sFly.phaseFrames > 8) && !InFlower(p)) {
                FlyEnd("FAIL: A did not burrow him (not on the pad?)", false);
                return false;
            }
            if (sFly.phaseFrames > 90) {
                FlyEnd("FAIL: burrow did not finish", false);
                return false;
            }
            break;
        case FLY_CHARGE: {
            sH.buttons = BTN_A;
            if ((sFly.chargedAt < 0) && (p->av2.actionVar2 >= 10)) {
                sFly.chargedAt = sFly.phaseFrames;
                Log("FLY charged (av2=%d, the sparkle) after %d frames", p->av2.actionVar2, sFly.phaseFrames);
            }
            bool aligned = true;
            if (sFly.hasTarget) {
                StickTowardYaw(yawToTarget, 100.0f); // func_80855F9C turns his yaw 0x258 a frame toward the stick
                aligned = ABS((s16)(p->yaw - yawToTarget)) < 0x0C00;
            } else {
                sH.stickX = sH.stickY = 0;
            }
            if ((sFly.phaseFrames >= sFly.chargeFrames) && (aligned || (sFly.phaseFrames >= sFly.chargeFrames + 90))) {
                sH.buttons = 0; // released: av1 3, the launch
                sH.stickX = sH.stickY = 0;
                Log("FLY release A after %d frames hidden av2=%d yaw=0x%04X toTarget=0x%04X aligned=%d",
                    sFly.phaseFrames, p->av2.actionVar2, (u16)p->yaw, (u16)yawToTarget, aligned ? 1 : 0);
                FlySetPhase(FLY_LAUNCH, "A released");
            }
            break;
        }
        case FLY_LAUNCH:
            sH.buttons = 0;
            sH.stickX = sH.stickY = 0;
            if (p->stateFlags3 & PLAYER_STATE3_200) {
                sFly.launchPos = p->unk_AF0[0]; // Player_Action_93 copies the launch point there
                sFly.launchY = p->actor.floorHeight;
                sFly.peakY = p->actor.world.pos.y;
                Log("FLY launch charged=%d from (%.1f,%.1f,%.1f) vy=%.2f gold=%d", (p->stateFlags3 & PLAYER_STATE3_2000) ? 1 : 0,
                    sFly.launchPos.x, sFly.launchPos.y, sFly.launchPos.z, p->actor.velocity.y, sFly.gold ? 1 : 0);
                FlySetPhase(FLY_RISE, "launched");
            } else if (sFly.phaseFrames > 30) {
                FlyEnd("FAIL: no launch (a ceiling over the flower?)", false);
                return false;
            }
            break;
        case FLY_RISE:
            sH.buttons = 0;
            sH.stickX = sH.stickY = 0;
            if (!(p->stateFlags3 & PLAYER_STATE3_2000)) {
                FlySetPhase(FLY_DROP, grounded ? "landed (no glide: not charged)" : "a hop, no glide (not charged)");
            } else if (!(p->stateFlags3 & PLAYER_STATE3_200) && (p->stateFlags3 & PLAYER_STATE3_1000000)) {
                FlySetPhase(FLY_GLIDE, "petals open");
            }
            break;
        case FLY_GLIDE: {
            sH.buttons = 0;
            if (grounded) {
                FlySetPhase(FLY_DROP, "touched down gliding");
                break;
            }
            if (!(p->stateFlags3 & PLAYER_STATE3_2000)) {
                FlySetPhase(FLY_DROP, "the glide ended (propeller out)");
                break;
            }
            const f32 range = sFly.gold ? 960.0f : 600.0f;
            const f32 fromLaunch = Math_Vec3f_DistXZ(&p->actor.world.pos, &sFly.launchPos);
            bool drop = false;
            if (!sFly.hasTarget) {
                sH.stickX = sH.stickY = 0;
                drop = (sFly.phaseFrames >= sFly.glideFrames);
            } else {
                // Closing speed toward the target, braking so he stops over it (decel 0.25 a frame in speed, 1.5x in
                // distance: stopping distance 3 v^2). Out of reach (the propeller gives out first): full tilt.
                const f32 closing = p->speedXZ * Math_CosS((s16)(p->yaw - yawToTarget));
                const bool reachable = (fromLaunch + dTarget) < (range - 5.0f);
                if (!reachable) {
                    StickTowardYaw(yawToTarget, 100.0f);
                } else {
                    const f32 want = std::min(8.0f, sqrtf(std::max(dTarget - 3.0f, 0.0f) / 3.0f));
                    if (closing > want + 0.25f) {
                        StickTowardYaw((s16)(yawToTarget + 0x8000), 100.0f);
                    } else if (closing < want - 0.25f) {
                        StickTowardYaw(yawToTarget, 100.0f);
                    } else {
                        sH.stickX = sH.stickY = 0;
                    }
                    drop = (dTarget < 8.0f) && (fabsf(p->speedXZ) < 1.0f);
                }
                drop = drop || (sFly.phaseFrames >= sFly.glideFrames);
            }
            if ((sFly.phaseFrames % 10) == 1) {
                Log("FLY glide f=%d pos=(%.1f,%.1f,%.1f) toTarget=%.1f fromLaunch=%.1f spd=%.2f vy=%.2f prop=%d yaw=0x%04X "
                    "stick=(%d,%d)",
                    sFly.phaseFrames, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, dTarget,
                    fromLaunch, p->speedXZ, p->actor.velocity.y, p->unk_B86[1], (u16)p->yaw, sH.stickX, sH.stickY);
            }
            if (drop) {
                sH.buttons = BTN_A; // kakkufinish: the petals close and he drops
                sH.stickX = sH.stickY = 0;
                sFly.dropPress = sFly.targetFlower ? 0 : 1;
                FlySetPhase(FLY_DROP, sFly.targetFlower ? "A: drop onto the flower, A held" : "A: drop");
            }
            break;
        }
        case FLY_DROP: {
            // A held through the landing for a flower target (func_80837134 burrows him); else just the press.
            if (sFly.targetFlower) {
                sH.buttons = BTN_A;
            } else if (sFly.dropPress > 0) {
                sFly.dropPress--;
            } else {
                sH.buttons = 0;
            }
            if (grounded) {
                sH.stickX = sH.stickY = 0;
                if (sFly.targetFlower) {
                    // The landing burrows him the frame he touches the pad; a few frames' grace before giving up.
                    if (InFlower(p)) {
                        FlyEnd("landed in the flower, burrowing (chain: A held)", true);
                        return false;
                    }
                    if (++sFly.groundFrames > 6) {
                        FlyEnd("landed beside the flower (no burrow)", false);
                        return false;
                    }
                    break;
                }
                FlyEnd("landed", false);
                return false;
            }
            if (sFly.hasTarget) {
                // Deku air control (Player_Action_25, func_8083CBC4 at half the stick's speed): ease onto the point.
                if (dTarget > 3.0f) {
                    StickTowardYaw(yawToTarget, std::min(100.0f, dTarget * 2.5f));
                } else {
                    sH.stickX = sH.stickY = 0;
                }
            } else {
                sH.stickX = sH.stickY = 0;
            }
            if (sFly.phaseFrames > 240) {
                FlyEnd("FAIL: still airborne", false);
                return false;
            }
            break;
        }
        default:
            break;
    }
    return true;
}

// "flowerfly <n|x,z> [tx tz] [charge] [glide]": set up the flight; false (logged) when no flower is found.
bool FlyStart(const std::vector<std::string>& args) {
    auto arg = [&](size_t i) -> std::string { return i < args.size() ? args[i] : std::string(); };
    Player* p = GET_PLAYER(gPlayState);
    sFly = FlowerFly();
    const std::string sel = arg(0);
    Actor* flower = nullptr;
    const size_t comma = sel.find(',');
    if (comma != std::string::npos) {
        Vec3f point = { (f32)atof(sel.substr(0, comma).c_str()), 0.0f, (f32)atof(sel.substr(comma + 1).c_str()) };
        flower = NearestActor(ACTOR_OBJ_ETCETERA, point.x, point.z);
        if ((flower != nullptr) && (Math_Vec3f_DistXZ(&flower->world.pos, &point) > 60.0f)) {
            Log("FLY the flower nearest %s is %.0f away: not the one meant (another scene layer?)", sel.c_str(),
                Math_Vec3f_DistXZ(&flower->world.pos, &point));
            flower = nullptr;
        }
    } else if (ParseInt(sel) < 0) {
        flower = NearestActor(ACTOR_OBJ_ETCETERA, p->actor.world.pos.x, p->actor.world.pos.z);
    } else {
        flower = NthActor(ACTOR_OBJ_ETCETERA, (int)ParseInt(sel), false);
    }
    NthActor(ACTOR_OBJ_ETCETERA, -1, true); // every flower, for the log
    if (flower == nullptr) {
        Log("FLY no Deku flower '%s'", sel.c_str());
        return false;
    }
    sFly.flower = flower->world.pos;
    sFly.flowerBgId = ((DynaPolyActor*)flower)->bgId;
    sFly.gold = (flower->params & 0x100) != 0; // DEKU_FLOWER_TYPE_GOLD(_WITH_INITIAL_BOUNCE)
    if (!arg(1).empty() && (arg(1) != "-")) {
        sFly.hasTarget = true;
        sFly.target = { (f32)atof(arg(1).c_str()), p->actor.world.pos.y, (f32)atof(arg(2).c_str()) };
        Actor* at = NearestActor(ACTOR_OBJ_ETCETERA, sFly.target.x, sFly.target.z);
        if ((at != nullptr) && (at != flower) && (Math_Vec3f_DistXZ(&at->world.pos, &sFly.target) < 20.0f)) {
            sFly.targetFlower = true;
            sFly.target.x = at->world.pos.x;
            sFly.target.z = at->world.pos.z;
        }
    }
    if (!arg(3).empty()) {
        sFly.chargeFrames = std::max(1, (int)ParseInt(arg(3)));
    }
    if (!arg(4).empty()) {
        sFly.glideFrames = std::max(1, (int)ParseInt(arg(4)));
    }
    sFly.prevFlags3 = p->stateFlags3 & (PLAYER_STATE3_100 | PLAYER_STATE3_200 | PLAYER_STATE3_2000 |
                                        PLAYER_STATE3_40000 | PLAYER_STATE3_1000000);
    sFly.prevAv1 = p->av1.actionVar1;
    const f32 d = Math_Vec3f_DistXZ(&p->actor.world.pos, &sFly.flower);
    Log("FLY start flower=(%.1f,%.1f,%.1f) gold=%d bg=%d target=%s(%.1f,%.1f)%s charge=%d glide=%d link=(%.1f,%.1f,%.1f) "
        "d=%.1f form=%d",
        sFly.flower.x, sFly.flower.y, sFly.flower.z, sFly.gold ? 1 : 0, sFly.flowerBgId, sFly.hasTarget ? "" : "none ",
        sFly.target.x, sFly.target.z, sFly.targetFlower ? " (a flower: chain)" : "", sFly.chargeFrames,
        sFly.glideFrames, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, d, p->transformation);
    if (p->transformation != PLAYER_FORM_DEKU) {
        Log("FLY WARN Link is not Deku (form %d): only Deku Link burrows", p->transformation);
    }
    sFly.phase = FLY_WALK;
    if (InFlower(p) && (d < 40.0f) && (p->actor.floorBgId == sFly.flowerBgId)) {
        // Carried on from a chain landing: still burrowing in this flower with A held.
        sH.buttons = BTN_A;
        sFly.phase = FLY_BURROW;
        Log("FLY already in the flower (chain), A held");
    }
    return true;
}

// Runs commands until one consumes frames.
// "cinecam" (include/mergeship/CineCam.hpp): the film camera, a sub camera of the harness's own placed each frame
// (MM's own cameras swing behind Link, e.g. while he guards). Shared with the door harness (MergeShipHostTest.cpp).
MsCineCam sCine;
s16 sCineCamId = CAM_ID_NONE;
bool sHudOff = false; // "hud off": the HUD hidden (as the game hides it for cutscenes), every frame until "hud on"
enum { CINEYAW_AUTO, CINEYAW_MOVE, CINEYAW_SHAPE };
int sCineYaw = CINEYAW_AUTO; // "cineyaw": the yaw the film camera trails

void UpdateCineCam() {
    PlayState* play = gPlayState;
    if (play == nullptr) {
        sCineCamId = CAM_ID_NONE;
        return;
    }
    if (sHudOff) {
        Interface_SetHudVisibility(HUD_VISIBILITY_NONE);
    }
    const bool alive = (sCineCamId != CAM_ID_NONE) && (play->cameraPtrs[sCineCamId] != nullptr);
    if (sCine.mode == MsCineCam::OFF) {
        if (alive) {
            Play_ClearCamera(play, sCineCamId);
            Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_ACTIVE);
        }
        sCineCamId = CAM_ID_NONE;
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return;
    }
    if (!alive) {
        sCineCamId = Play_CreateSubCamera(play);
        if (sCineCamId == CAM_ID_NONE) {
            return;
        }
        Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STATUS_WAIT);
        Play_ChangeCameraStatus(play, sCineCamId, CAM_STATUS_ACTIVE);
        sCine.primed = false;
    }
    float at[3];
    float eye[3];
    // Burrowing in a Deku flower spins his model (Player_Action_93), so a flight is trailed by his movement yaw.
    const bool moveYaw = (sCineYaw == CINEYAW_MOVE) || ((sCineYaw == CINEYAW_AUTO) && FlyActive());
    sCine.Update(player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z,
                 moveYaw ? player->yaw : player->actor.shape.rot.y, at, eye);
    Vec3f vAt = { at[0], at[1], at[2] };
    Vec3f vEye = { eye[0], eye[1], eye[2] };
    Play_SetCameraAtEye(play, sCineCamId, &vAt, &vEye);
}

void Step() {
    if (FlyActive() && FlyFrame()) {
        return; // "flowerfly" holds the script until he has landed
    }
    if (sH.steerFrames > 0) {
        // "walkto": steer until within the radius (the stick is left for the next line, so legs join up) or timeout.
        Player* player = InPlay() ? GET_PLAYER(gPlayState) : nullptr;
        if (player == nullptr) {
            Log("WALKTO stopped: transition");
            sH.steerFrames = 0;
            sH.stickX = sH.stickY = 0;
        } else {
            const f32 d = Math_Vec3f_DistXZ(&player->actor.world.pos, &sH.steerTarget);
            if (d <= sH.steerRadius) {
                Log("WALKTO arrived (%.1f,%.1f) d=%.1f pos=(%.1f,%.1f,%.1f) spd=%.2f", sH.steerTarget.x,
                    sH.steerTarget.z, d, player->actor.world.pos.x, player->actor.world.pos.y,
                    player->actor.world.pos.z, player->speedXZ);
                sH.steerFrames = 0;
            } else if (--sH.steerFrames > 0) {
                StickTowardYaw(Math_Vec3f_Yaw(&player->actor.world.pos, &sH.steerTarget), 100.0f);
                return;
            } else {
                sH.stickX = sH.stickY = 0;
                Log("WALKTO timeout (%.1f,%.1f) d=%.1f pos=(%.1f,%.1f,%.1f)", sH.steerTarget.x, sH.steerTarget.z, d,
                    player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z);
            }
        }
    }
    if (!sH.noteQueue.empty()) {
        // "notes": 6 frames down, 6 frames up per note.
        int phase = sH.noteFrame++ % 12;
        sH.buttons = (phase < 6) ? sH.noteQueue.front() : 0;
        if (phase == 11) {
            sH.noteQueue.erase(sH.noteQueue.begin());
        }
        return;
    }
    if (sH.pressRelease) {
        sH.pressRelease = false;
        sH.buttons = 0;
        return;
    }
    if (sH.waitingForPlay) {
        if (!InPlay()) {
            return;
        }
        if (sH.afterPlayFrames-- > 0) {
            return;
        }
        sH.waitingForPlay = false;
        Log("PLAY ready scene=%d entrance=0x%X form=%d layer=%d", gPlayState->sceneId, gSaveContext.save.entrance,
            GET_PLAYER(gPlayState)->transformation, gSaveContext.sceneLayer);
    }
    // tatlcallhold: the fairy keeps calling (her C-Up label shows) whatever she would do herself.
    if ((sTatlCallHold > 0) && InPlay()) {
        sTatlCallHold--;
        // As a quest hint does (EnElf: QuestHint_GetTatlTextId): she has something to say (one of her hints, 0x0216).
        if (GET_PLAYER(gPlayState)->tatlTextId == 0) {
            GET_PLAYER(gPlayState)->tatlTextId = 0x0216;
        }
    }
    if (sH.waitFrames > 0) {
        sH.waitFrames--;
        if (sH.waitFrames > 0) {
            return;
        }
        sH.buttons = 0;
        sH.stickX = sH.stickY = 0;
    }

    while (sH.pc < sH.commands.size()) {
        const Command& c = sH.commands[sH.pc++];
        auto arg = [&](size_t i) -> std::string { return i < c.args.size() ? c.args[i] : std::string(); };

        if (c.op == "boot") {
            continue; // done at the console logo
        } else if (c.op == "waitplay") {
            sH.waitingForPlay = true;
            sH.afterPlayFrames = c.args.empty() ? 0 : (int)ParseInt(arg(0));
            return;
        } else if (c.op == "wait") {
            sH.buttons = 0;
            sH.stickX = sH.stickY = 0;
            sH.waitFrames = (int)ParseInt(arg(0));
            return;
        } else if (c.op == "hold") {
            sH.buttons = ParseButtons(arg(0));
            sH.stickX = sH.stickY = 0;
            sH.waitFrames = (int)ParseInt(arg(1));
            return;
        } else if (c.op == "hud") {
            // hud off|on: the HUD hidden for filming (the game's own cutscene visibility), kept off every frame.
            Log("%s", MergeShipCine_Hud(arg(0) != "on").c_str());
        } else if (c.op == "cinecam") {
            Log("%s", sCine.Command(c.args).c_str()); // include/mergeship/CineCam.hpp
        } else if (c.op == "frontcam") {
            // frontcam <dist> [height] [angle] | off: a rigid "cinecam front"
            std::vector<std::string> cine = { (arg(0) == "off") ? "off" : "front", arg(0).empty() ? "160" : arg(0),
                                              arg(1).empty() ? "10" : arg(1), arg(2).empty() ? "0" : arg(2), "1" };
            Log("%s", sCine.Command(cine).c_str());
        } else if (c.op == "cineyaw") {
            sCineYaw = (arg(0) == "move") ? CINEYAW_MOVE : (arg(0) == "shape") ? CINEYAW_SHAPE : CINEYAW_AUTO;
            Log("CINEYAW %s", (sCineYaw == CINEYAW_MOVE) ? "move" : (sCineYaw == CINEYAW_SHAPE) ? "shape" : "auto");
        } else if (c.op == "bootweekevent") {
            continue; // applied at boot (Boot)
        } else if (c.op == "hyruleextras") {
            // hyruleextras <MS_HYRULE_EXTRA_* bits> [deku] [gold]: what Hyrule's capture would write (BUG-0014).
            MsSharedSave* shared = static_cast<MsSharedSave*>(MergeShipHost_SharedSave());
            if (shared != nullptr) {
                shared->hyruleExtras = (u8)(MS_HYRULE_EXTRA_VALID | ParseInt(arg(0)));
            }
            // Termina's own bean counts (BUG-0105), stored as count + 1
            MergeShipOoTItemsSave& own = gSaveContext.save.shipSaveInfo.mergeShipOoTItems;
            own.dekuBeans = (u8)((arg(1).empty() ? 10 : ParseInt(arg(1))) + 1);
            own.goldBeans = (u8)((arg(2).empty() ? 10 : ParseInt(arg(2))) + 1);
            Log("HYRULEEXTRAS 0x%02X deku=%d gold=%d (shared %s)", shared ? shared->hyruleExtras : 0, own.dekuBeans - 1,
                own.goldBeans - 1, shared ? "ok" : "none");
        } else if (c.op == "ootbottle") {
            // ootbottle <0-3> <MM item>: what one of Hyrule's bottles holds, as Majora's Mask's item (BUG-0024).
            MergeShip_Own_SetOoTBottle((s32)ParseInt(arg(0)), (u8)ParseInt(arg(1)));
            Log("OOTBOTTLE %s = 0x%02X", arg(0).c_str(), MergeShip_Own_OoTBottle((s32)ParseInt(arg(0))));
        } else if (c.op == "buttons") {
            // buttons [tag]: the human's B and C items and each button's status (BUG-0029: BTN_ENABLED 0, DISABLED 0xFF).
            Log("BUTTONS %s B=0x%02X C=0x%02X/0x%02X/0x%02X status=%02X/%02X/%02X/%02X form=%d", arg(0).c_str(),
                BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT),
                BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN), BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_RIGHT),
                gSaveContext.buttonStatus[EQUIP_SLOT_B], gSaveContext.buttonStatus[EQUIP_SLOT_C_LEFT],
                gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN], gSaveContext.buttonStatus[EQUIP_SLOT_C_RIGHT],
                (gPlayState != nullptr) ? GET_PLAYER(gPlayState)->transformation : -1);
        } else if (c.op == "hastatl") {
            // hastatl <0|1>: the file has its fairy (gSaveContext.save.hasTatl); she appears on the next scene.
            gSaveContext.save.hasTatl = (s32)ParseInt(arg(0)) != 0;
            Log("HASTATL %d", gSaveContext.save.hasTatl);
        } else if (c.op == "warp") {
            // warp <entrance>: change scene (follow with waitplay).
            if (InPlay()) {
                gPlayState->nextEntrance = (u16)ParseInt(arg(0));
                gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                gPlayState->transitionType = TRANS_TYPE_INSTANT;
            }
            Log("WARP 0x%X", (u32)ParseInt(arg(0)));
        } else if (c.op == "tatlinfo") {
            if (InPlay()) {
                Player* p = GET_PLAYER(gPlayState);
                Log("TATLINFO hasTatl=%d actor=%p textId=%d timer=%d calling=%d s2=%08X gameMode=%d scene=%d",
                    gSaveContext.save.hasTatl, (void*)p->tatlActor, p->tatlTextId,
                    gSaveContext.save.saveInfo.playerData.tatlTimer, gPlayState->interfaceCtx.tatlCalling,
                    p->stateFlags2, gSaveContext.gameMode, gPlayState->sceneId);
            }
        } else if (c.op == "tatlcallhold") {
            // tatlcallhold <frames>: hold the fairy's call (interfaceCtx.tatlCalling) on every frame.
            sTatlCallHold = (s32)ParseInt(arg(0));
        } else if (c.op == "tatlcall") {
            // tatlcall <0x2A|0x2B|0x2C>: the fairy calls Link (Interface_SetTatlCall: her voice and the C-Up label).
            if (InPlay()) {
                gMergeShipSfxProbe = 4;
                Interface_SetTatlCall(gPlayState, (u16)ParseInt(arg(0)));
            }
            Log("TATLCALL %s", arg(0).c_str());
        } else if (c.op == "spnavi") {
            // spnavi <active> [fdFromMoon]: singleplayer's Navi without a trip from Hyrule.
            MergeShip_Singleplayer_DebugSet((s32)ParseInt(arg(0)), arg(1).empty() ? 0 : (s32)ParseInt(arg(1)));
        } else if (c.op == "text") {
            // text <id>: open a message box (the Navi lines).
            if (InPlay()) {
                Message_StartTextbox(gPlayState, (u16)ParseInt(arg(0)), NULL);
            }
            Log("TEXT 0x%X", (u32)ParseInt(arg(0)));
        } else if (c.op == "sfx") {
            // sfx <id>: play a sound effect; the next notes log the font entry they play (SFXNOTE).
            gMergeShipSfxProbe = 4;
            Audio_PlaySfx((u16)ParseInt(arg(0)));
            Log("SFX 0x%X", (u32)ParseInt(arg(0)));
        } else if (c.op == "bank") {
            // bank [rupees]: set the Clock Town bank's balance; logs it (BUG-0030: it travels with the shared save).
            if (!arg(0).empty()) {
                HS_SET_BANK_RUPEES((u32)ParseInt(arg(0)));
            }
            Log("BANK %u rupees=%d", (u32)HS_GET_BANK_RUPEES(), gSaveContext.save.saveInfo.playerData.rupees);
        } else if (c.op == "pos") {
            if (InPlay()) {
                Player* p = GET_PLAYER(gPlayState);
                p->actor.world.pos.x = p->actor.home.pos.x = (f32)atof(arg(0).c_str());
                p->actor.world.pos.y = p->actor.home.pos.y = (f32)atof(arg(1).c_str());
                p->actor.world.pos.z = p->actor.home.pos.z = (f32)atof(arg(2).c_str());
                if (!arg(3).empty()) {
                    p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)ParseInt(arg(3));
                }
            }
            Log("POS %s %s %s %s", arg(0).c_str(), arg(1).c_str(), arg(2).c_str(), arg(3).c_str());
        } else if (c.op == "walkto") {
            if (!InPlay()) {
                Log("WALKTO skipped: not in play");
                continue;
            }
            Player* p = GET_PLAYER(gPlayState);
            sH.steerTarget = { (f32)atof(arg(0).c_str()), p->actor.world.pos.y, (f32)atof(arg(1).c_str()) };
            sH.steerFrames = (int)ParseInt(arg(2));
            sH.steerRadius = arg(3).empty() ? 12.0f : (f32)atof(arg(3).c_str());
            sH.buttons = 0;
            Log("WALKTO %s %s %d from (%.1f,%.1f,%.1f) d=%.1f", arg(0).c_str(), arg(1).c_str(), sH.steerFrames,
                p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z,
                Math_Vec3f_DistXZ(&p->actor.world.pos, &sH.steerTarget));
            StickTowardYaw(Math_Vec3f_Yaw(&p->actor.world.pos, &sH.steerTarget), 100.0f);
            return;
        } else if ((c.op == "actors") || (c.op == "infront")) {
            if (!InPlay()) {
                continue;
            }
            const s16 id = (s16)ParseInt(arg(0));
            const int want = ((c.op == "infront") && !arg(2).empty()) ? (int)ParseInt(arg(2)) : 0;
            Actor* chosen = NthActor(id, want, true);
            if ((c.op == "infront") && (chosen != nullptr)) {
                Player* p = GET_PLAYER(gPlayState);
                const f32 dist = arg(1).empty() ? 120.0f : (f32)atof(arg(1).c_str());
                const s16 yaw = chosen->shape.rot.y;
                p->actor.world.pos.x = p->actor.home.pos.x = chosen->world.pos.x + Math_SinS(yaw) * dist;
                p->actor.world.pos.y = p->actor.home.pos.y = chosen->world.pos.y;
                p->actor.world.pos.z = p->actor.home.pos.z = chosen->world.pos.z + Math_CosS(yaw) * dist;
                p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)(yaw + 0x8000);
                Log("INFRONT 0x%03X #%d -> pos=(%.1f,%.1f,%.1f)", id, want, p->actor.world.pos.x,
                    p->actor.world.pos.y, p->actor.world.pos.z);
            } else if (c.op == "infront") {
                Log("INFRONT 0x%03X #%d: none", id, want);
            }
        } else if (c.op == "flowerfly") {
            if (!InPlay() || !FlyStart(c.args)) {
                Log("FLY skipped (line %d)", c.line);
                continue;
            }
            FlyFrame();
            return;
        } else if (c.op == "stick") {
            sH.stickX = (s8)ParseInt(arg(0));
            sH.stickY = (s8)ParseInt(arg(1));
            sH.waitFrames = (int)ParseInt(arg(2));
            sH.buttons = ParseButtons(arg(3));
            return;
        } else if (c.op == "press") {
            sH.buttons = ParseButtons(arg(0));
            sH.stickX = sH.stickY = 0;
            sH.pressRelease = true;
            return;
        } else if (c.op == "notes") {
            sH.noteQueue.clear();
            for (auto& a : c.args) {
                sH.noteQueue.push_back(ParseButtons(a));
            }
            sH.noteFrame = 0;
            Log("NOTES %d", (int)sH.noteQueue.size());
            Step();
            return;
        } else if (c.op == "give") {
            u8 item = (u8)ParseInt(arg(0));
            u8 r = Item_Give(gPlayState, item);
            Log("GIVE 0x%02X -> 0x%02X quest=0x%06X mmQuest=0x%08X", item, r, MergeShip_Hyrule_GetQuestItems(),
                gSaveContext.save.saveInfo.inventory.questItems);
        } else if (c.op == "hyrulequest") {
            if (arg(0) == "all") {
                for (u8 q = 0; q < MERGESHIP_OOT_QUEST_MAX; q++) {
                    MergeShip_Hyrule_SetQuestItem(q);
                }
            } else {
                MergeShip_Hyrule_SetQuestItem((u8)ParseInt(arg(0)));
            }
            Log("HYRULEQUEST %s -> 0x%06X", arg(0).c_str(), MergeShip_Hyrule_GetQuestItems());
        } else if (c.op == "audiotrace") {
            // audiotrace on|off: log the font instruments and sound effects notes ask for (playback.c).
            gMergeShipAudioTrace = (arg(0) == "on") ? 1 : 0;
            Log("AUDIOTRACE %s", arg(0).c_str());
        } else if (c.op == "icetrapgi") {
            // icetrapgi [fake RandoItemId name]: an ice trap held up as a randomizer check's (BUG-0097).
            static RandoItemId sFake = RI_BOMBCHU_10;
            RandoItemId fake = arg(0).empty() ? RI_BOMBCHU_10 : Rando::StaticData::GetItemIdFromName(arg(0).c_str());
            sFake = (fake == RI_UNKNOWN) ? RI_BOMBCHU_10 : fake;
            GameInteractor::Instance->events.emplace_back(GIEventGiveItem{
                .showGetItemCutscene = true,
                .param = 0,
                .giveItem = [](Actor* actor, PlayState* play) { CUSTOM_ITEM_PARAM = RI_TRAP; },
                .drawItem =
                    [](Actor* actor, PlayState* play) {
                        if (CUSTOM_ITEM_FLAGS & CustomItem::CALLED_ACTION) {
                            Rando::MiscBehavior::DrawIceTrapGetItem(actor, play, sFake);
                        } else {
                            Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                            Rando::DrawItem(sFake, RC_UNKNOWN, actor);
                        }
                    } });
            Log("ICETRAPGI fake=%d", (int)sFake);
        } else if (c.op == "getitem") {
            MergeShip_Hyrule_QueueGetItem((u16)ParseInt(arg(0)));
        } else if (c.op == "proxyrow") {
            // proxyrow <dist> <item...>: each item lying dist in front of the player, drawn by Rando::DrawItem.
            s32 n = (s32)c.args.size() - 1;
            s32 spawned = 0;
            if ((gPlayState != nullptr) && (n > 0)) {
                Player* player = GET_PLAYER(gPlayState);
                f32 dist = (f32)atof(arg(0).c_str());
                s16 yaw = player->actor.shape.rot.y;
                for (s32 i = 0; i < n; i++) {
                    std::string name = c.args[i + 1];
                    RandoItemId ri = Rando::StaticData::GetItemIdFromName(name.c_str());
                    if (ri == RI_UNKNOWN) {
                        ri = Rando::StaticData::GetItemIdFromName(("RI_OOT_" + name).c_str());
                    }
                    f32 side = (i - (n - 1) / 2.0f) * 45.0f;
                    f32 x = player->actor.world.pos.x + Math_SinS(yaw) * dist + Math_CosS(yaw) * side;
                    f32 z = player->actor.world.pos.z + Math_CosS(yaw) * dist - Math_SinS(yaw) * side;
                    EnItem00* item = nullptr;
                    if (ri != RI_UNKNOWN) {
                        item = CustomItem::Spawn(x, player->actor.world.pos.y + 10.0f, z, 0, CustomItem::STOP_BOBBING,
                                                 (s16)ri, nullptr, [](Actor* actor, PlayState* play) {
                                                     Matrix_Scale(30.0f, 30.0f, 30.0f, MTXMODE_APPLY);
                                                     Rando::DrawItem((RandoItemId)CUSTOM_ITEM_PARAM, RC_UNKNOWN, actor);
                                                 });
                    }
                    spawned += (item != nullptr) ? 1 : 0;
                    const MsItemKey key = ProxyKeyOf(ri);
                    Log("PROXYROW %s ri=%d proxy=%d ootRow=%d", name.c_str(), (int)ri,
                        Rando::MergeShip::IsProxy(ri) ? 1 : 0, (key < MSI_MAX) && MergeShip_OoTGi_HasKey(key) ? 1 : 0);
                }
            }
            Log("PROXYROW spawned %d of %d (oot.o2r mounted=%d)", spawned, n, MergeShip_IsOcarinaMounted());
        } else if (c.op == "proxyaudit") {
            // proxyaudit: every proxy RI_OOT_* and its Ocarina of Time model (OoTGiDraw.cpp); FALLBACK for none.
            s32 total = 0;
            s32 fallbacks = 0;
            for (auto& [ri, item] : Rando::StaticData::Items) {
                if (!Rando::MergeShip::IsProxy(ri)) {
                    continue;
                }
                total++;
                const MsItemKey key = ProxyKeyOf(ri);
                const bool row = (key < MSI_MAX) && MergeShip_OoTGi_HasKey(key);
                fallbacks += row ? 0 : 1;
                Log("PROXYAUDIT %s key=%d ootRow=%d%s", item.spoilerName, (int)key, row ? 1 : 0, row ? "" : " FALLBACK");
            }
            Log("PROXYAUDIT %d proxies, %d falling back (oot.o2r mounted=%d)", total, fallbacks,
                MergeShip_IsOcarinaMounted());
        } else if (c.op == "cbutton") {
            s32 btn = (s32)ParseInt(arg(0));
            u8 item = (u8)ParseInt(arg(1));
            s32 slot = SLOT(item);
            // MM keeps the C buttons in row 0 for every form (C_BTN_ITEM); only B is per form.
            s32 row = (btn == EQUIP_SLOT_B) ? CUR_FORM : 0;
            BUTTON_ITEM_EQUIP(row, btn) = item;
            C_SLOT_EQUIP(row, btn) = slot;
            gSaveContext.buttonStatus[btn] = BTN_ENABLED;
            if (gPlayState != nullptr) {
                Interface_LoadItemIconImpl(gPlayState, btn);
            }
            Log("CBUTTON %d item=0x%02X slot=%d form=%d curForm=%d status=%d", btn, item, slot,
                gSaveContext.save.playerForm, CUR_FORM, gSaveContext.buttonStatus[btn]);
        } else if (c.op == "questpage") {
            MergeShip_QuestHyrule_Set((s32)ParseInt(arg(0)), c.args.size() > 1 ? (s16)ParseInt(arg(1)) : -1);
            Log("QUESTPAGE set %s %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "warpreq") {
            s32 r = MergeShip_RequestHyruleWarp((u16)ParseInt(arg(0)));
            Log("WARPREQ 0x%X -> %d", (u16)ParseInt(arg(0)), r);
        } else if (c.op == "savejson") {
            nlohmann::json j = gSaveContext.save.shipSaveInfo;
            bool hasKey = j.contains("mergeShipOoTQuest");
            ShipSaveInfo copy = {};
            j.get_to(copy);
            Log("SAVEJSON key=%d json=%s roundtrip=0x%06X original=0x%06X", hasKey,
                hasKey ? j["mergeShipOoTQuest"].dump().c_str() : "-", copy.mergeShipOoTQuest.questItems,
                gSaveContext.save.shipSaveInfo.mergeShipOoTQuest.questItems);
            nlohmann::json old = j;
            old.erase("mergeShipOoTQuest");
            ShipSaveInfo fromOld = {};
            fromOld.mergeShipOoTQuest.questItems = 0xDEAD;
            old.get_to(fromOld);
            Log("SAVEJSON old save without the key loads as 0x%X", fromOld.mergeShipOoTQuest.questItems);
        } else if (c.op == "state") {
            LogState("STATE");
        } else if (c.op == "anchor") {
            std::string line;
            for (const std::string& a : c.args) {
                line += (line.empty() ? "" : " ") + a;
            }
            MergeShipAnchor_TestCommand(line.c_str());
        } else if (c.op == "trace") {
            sH.trace = (arg(0) == "on");
        } else if (c.op == "horse") {
            // horse [near|ride]: Link's Epona as Majora's Mask spawns her (z_horse.c): standing 150 in front of Link, or
            // with Link on her. horse side [dist]: Link stands at the nearest En_Horse's left, facing her, to mount
            // with A; horse log: every En_Horse and the rider.
            if (InPlay() && ((arg(0) == "side") || (arg(0) == "log"))) {
                Player* player = GET_PLAYER(gPlayState);
                Actor* nearest = nullptr;
                for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_BG].first; a != nullptr; a = a->next) {
                    if ((a->id == ACTOR_EN_HORSE) && !(((EnHorse*)a)->stateFlags & ENHORSE_INACTIVE) &&
                        ((nearest == nullptr) || (Math_Vec3f_DistXYZ(&a->world.pos, &player->actor.world.pos) <
                                                  Math_Vec3f_DistXYZ(&nearest->world.pos, &player->actor.world.pos)))) {
                        nearest = a;
                    }
                }
                for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_BG].first; a != nullptr; a = a->next) {
                    if (a->id != ACTOR_EN_HORSE) {
                        continue;
                    }
                    if ((arg(0) == "side") && (a == nearest)) {
                        const f32 dist = arg(1).empty() ? 45.0f : (f32)atof(arg(1).c_str());
                        const s16 yaw = (s16)(a->shape.rot.y + 0x4000);
                        player->actor.world.pos.x = player->actor.home.pos.x = a->world.pos.x + Math_SinS(yaw) * dist;
                        player->actor.world.pos.y = player->actor.home.pos.y = a->world.pos.y;
                        player->actor.world.pos.z = player->actor.home.pos.z = a->world.pos.z + Math_CosS(yaw) * dist;
                        player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)(yaw + 0x8000);
                    }
                    Log("HORSE adult=%d type=%d action=%d anim=%d scale=%.5f dist=%.1f ridden=%d rideActor=%d "
                        "onHorse=%d",
                        ((EnHorse*)a)->mergeShipAdult, ((EnHorse*)a)->type, ((EnHorse*)a)->action,
                        ((EnHorse*)a)->animIndex, a->scale.x, Math_Vec3f_DistXYZ(&a->world.pos, &player->actor.world.pos),
                        a->child != nullptr, player->rideActor == a,
                        (player->stateFlags1 & PLAYER_STATE1_800000) != 0);
                }
            } else if (InPlay()) {
                Player* player = GET_PLAYER(gPlayState);
                const bool ride = (arg(0) == "ride");
                const s16 yaw = player->actor.shape.rot.y;
                const f32 dist = ride ? 0.0f : 150.0f;
                Actor* horse = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_HORSE,
                                           player->actor.world.pos.x + Math_SinS(yaw) * dist, player->actor.world.pos.y,
                                           player->actor.world.pos.z + Math_CosS(yaw) * dist, 0, yaw, 0,
                                           ENHORSE_PARAMS(ENHORSE_PARAM_4000, ride ? ENHORSE_11 : ENHORSE_1));
                if (ride && (horse != nullptr)) {
                    player->rideActor = horse;
                    Player_MountHorse(gPlayState, player, horse);
                    Player_SetCameraHorseSetting(gPlayState, player);
                }
                Log("HORSE %s spawned=%d adult=%d", ride ? "ride" : "near", (horse != nullptr) ? 1 : 0,
                    (horse != nullptr) ? ((EnHorse*)horse)->mergeShipAdult : -1);
            }
        } else if (c.op == "mapsvisible") {
            // mapsvisible: every scene's minimap shown (as all of Tingle's maps bought)
            for (size_t i = 0; i < ARRAY_COUNT(gSaveContext.save.saveInfo.scenesVisible); i++) {
                gSaveContext.save.saveInfo.scenesVisible[i] = 0xFFFFFFFF;
            }
            Log("MAPSVISIBLE");
        } else if (c.op == "mmshield") {
            // mmshield <1 Hero's|2 Mirror>: Majora's Mask's shield equipped (follow with "ooteq shield 0" to show it).
            SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, (u8)ParseInt(arg(0)));
            Log("MMSHIELD %d", GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD));
        } else if (c.op == "cvarf") {
            CVarSetFloat(arg(0).c_str(), (float)atof(arg(1).c_str()));
            Log("CVARF %s=%s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvar") {
            CVarSetInteger(arg(0).c_str(), (s32)ParseInt(arg(1)));
            // As the menu does (UIWidgets.cpp): hooks registered for this CVar re-register with the new value, so a
            // script can show an option taking effect mid-run.
            ShipInit::Init(arg(0));
            Log("CVAR %s=%d", arg(0).c_str(), (int)ParseInt(arg(1)));
        } else if (c.op == "time") {
            gSaveContext.save.time = CLOCK_TIME(ParseInt(arg(0)), ParseInt(arg(1)));
            Log("TIME 0x%04X", gSaveContext.save.time);
        } else if (c.op == "mark") {
            std::string text;
            for (auto& a : c.args) {
                text += a + " ";
            }
            Log("MARK %s", text.c_str());
        } else if (c.op == "shot") {
            Screenshot(arg(0));
        } else if (c.op == "quit") {
            Log("QUIT");
            if (sH.log != nullptr) {
                fclose(sH.log);
                sH.log = nullptr;
            }
#ifdef _WIN32
            TerminateProcess(GetCurrentProcess(), 0);
#else
            _exit(0);
#endif
        } else if (MergeShipTest_OoTEquipCommand(c.op, c.args)) {
            // MergeShip2: handled by Ocarina of Time's equipment (MergeShipOoTEquip.cpp)
        } else if (MergeShipTest_OoTItemsCommand(c.op, c.args)) {
            // MergeShip2: handled by Ocarina of Time's items (MergeShipOoTItems.cpp)
        } else if (MergeShipTest_AdultLinkCommand(c.op, c.args)) {
            // MergeShip2: handled by adult Link (MergeShipAdultLink.cpp)
        } else if (MergeShipTest_TerminaEquipCommand(c.op, c.args)) {
            // MergeShip2: handled by Termina Equipment (MergeShipTerminaEquip.cpp)
        } else if (Rando::MergeShip::GoalTestCommand(c.op, c.args)) {
            // MergeShip2: goal state|fake|hyrule|majora|pieces, the unified randomizer's goal (UnifiedGoal.h)
        } else {
            Log("WARN line %d: unknown command '%s'", c.line, c.op.c_str());
        }
    }
}

void LoadScript(const char* path) {
    std::ifstream in(path);
    if (!in) {
        SPDLOG_ERROR("MergeShip test: cannot open script {}", path);
        return;
    }
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        n++;
        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::stringstream ss(line);
        Command c;
        c.line = n;
        if (!(ss >> c.op)) {
            continue;
        }
        std::string a;
        while (ss >> a) {
            c.args.push_back(a);
        }
        if (c.op == "boot") {
            sH.bootEntrance = (s32)ParseInt(c.args.empty() ? "0" : c.args[0]);
            if (c.args.size() > 1) {
                sH.bootForm = (s32)ParseInt(c.args[1]);
            }
        } else if (c.op == "bootweekevent") {
            for (const std::string& f : c.args) {
                if (f == "woodfall_raised") {
                    sH.bootWeekEvents.push_back(WEEKEVENTREG_20_01); // Woodfall Temple raised (Dm_Char01 types 2, 3)
                } else if (f == "woodfall_cleared") {
                    sH.bootWeekEvents.push_back(WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE);
                } else {
                    sH.bootWeekEvents.push_back((s32)ParseInt(f));
                }
            }
        }
        sH.commands.push_back(c);
    }
    std::string p(path);
    auto slash = p.find_last_of("\\/");
    sH.dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
    std::string logPath = sH.dir + "\\trace.log";
    sH.log = fopen(logPath.c_str(), "w");
    sH.active = true;
    // Keep the display on while the script runs (a sleeping display throttles presentation); test runs are silent.
#ifdef _WIN32
    SetThreadExecutionState(0x80000003); // ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
#endif
    CVarSetFloat("gSettings.Audio.MasterVolume", 0.0f);
    SPDLOG_INFO("MergeShip test: running {} ({} commands), trace to {}", path, sH.commands.size(), logPath);
}

// The debug save at an entrance, as 2S2H's boot-to-warp-point does (MapSelect_LoadGame with the debug file).
void Boot() {
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.fileNum = 0xFF;
    MapSelect_LoadGame((MapSelectState*)gGameState, sH.bootEntrance, 0);
    gSaveContext.save.playerForm = sH.bootForm;
    gSaveContext.save.time = CLOCK_TIME(10, 0);
    Log("BOOT entrance=0x%X form=%d", sH.bootEntrance, sH.bootForm);
    // The debug save is made by now (MapSelect_LoadGame -> Sram_InitDebugSave) and Play_Init has not run, so the scene
    // loads with these (e.g. Woodfall's layer after Odolwa: Play_Init picks it from CLEARED_WOODFALL_TEMPLE).
    for (s32 flag : sH.bootWeekEvents) {
        SET_WEEKEVENTREG(flag);
        Log("BOOT weekEventReg 0x%04X (%d & 0x%02X) set", flag, flag >> 8, flag & 0xFF);
    }
}

void RegisterMergeShipTest() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    // MergeShip2 host (mergeship.exe): both games share one environment, so this harness's script there is
    // MERGESHIP_TEST_SCRIPT_MM (SoH's is MERGESHIP_TEST_SCRIPT_SOH, the host door test's MERGESHIP_TEST_SCRIPT_2SHIP).
    const char* script = getenv("MERGESHIP_TEST_SCRIPT_MM");
#ifndef MERGESHIP_HOSTED
    if ((script == nullptr) || (*script == '\0')) {
        script = getenv("MERGESHIP_TEST_SCRIPT");
    }
#endif
    if ((script == nullptr) || (*script == '\0')) {
        return;
    }
    LoadScript(script);
    if (!sH.active) {
        return;
    }

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnConsoleLogoUpdate>([]() {
        if (sH.booted || (sH.bootEntrance < 0)) {
            return;
        }
        sH.booted = true;
        Boot();
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>([]() {
        sH.frame++;
        Step();
        UpdateCineCam();
        if (sH.trace) {
            LogState("T");
        }
    });
}

static RegisterShipInitFunc sInitFunc(RegisterMergeShipTest, {});

} // namespace

// The film camera for the door harness (MergeShipHostTest.cpp): one "cinecam" line, and once a frame.
std::string MergeShipCine_Command(const std::vector<std::string>& args) {
    return sCine.Command(args);
}

void MergeShipCine_Frame() {
    UpdateCineCam();
}

std::string MergeShipCine_Hud(bool off) {
    sHudOff = off;
    if (!off && (gPlayState != nullptr)) {
        Interface_SetHudVisibility(HUD_VISIBILITY_ALL);
    }
    return off ? "HUD off" : "HUD on";
}

// Diagnostics from C code; a no-op unless a script is running.
extern "C" void MergeShipTest_Log(const char* fmt, ...) {
    if (sH.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sH.log, "[%06llu] ", (unsigned long long)sH.frame);
    vfprintf(sH.log, fmt, ap);
    fputc('\n', sH.log);
    va_end(ap);
    fflush(sH.log);
}

// padmgr.c (PadMgr_HandleRetrace): while a script runs, the script is controller 1, for the game state and for the
// ocarina alike (AudioOcarina_ReadControllerInput reads padmgr's polled input before the game state's frame).
extern "C" void MergeShipTest_OverrideInput(OSContPad* pad) {
    if (!sH.active) {
        return;
    }
    pad->button = sH.buttons;
    pad->stick_x = sH.stickX;
    pad->stick_y = sH.stickY;
    pad->err_no = 0;
}

extern "C" int MergeShipTest_IsActive(void) {
    return sH.active ? 1 : 0;
}
