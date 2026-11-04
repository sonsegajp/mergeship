// MergeShip2 -- a scripted test driver for 2 Ship 2 Harkinian inside mergeship.exe, the counterpart of SoH's
// MergeShipTest.cpp (same script language, the commands the door tests need). It drives the real game with scripted
// controller input and logs what happened, so a trip through the door is shown to work, not assumed to.
//
// Inert unless the environment variable MERGESHIP_TEST_SCRIPT_2SHIP names a script file. Output: trace_2ship.log
// and <name>.bmp screenshots next to the script.
//
//   waitplay [frames]               wait until gameplay is running (no transition), then N more frames
//   wait <frames>                   neutral input for N frames
//   hold <buttons> <frames>         hold buttons, e.g. "hold A+B 10"
//   stick <x> <y> <frames> [buttons] hold the stick (and optional buttons)
//   press <buttons>                 one frame down, one frame up
//   pos <x> <y> <z> [yaw]           place the player
//   exits [n]                       log the scene's exit polygons (exit n, or every exit), grouped into openings
//   walkexit <n> <frames> [k]       walk (steering the stick) into the k-th nearest opening of exit n
//   toexit <n> [dist] [k]           stand dist (default 150) in front of the k-th nearest opening of exit n
//   walkto <x> <z> <frames>         walk (steering the stick) towards a point
//   actors <actorId>                log every actor with that id
//   infront <actorId> <dist> [n]    stand dist in front of the n-th actor with that id, facing it
//   cvar <name> <int>               set an integer CVar (and re-run the features that depend on it)
//   cvarlog <name...>               log CVars as the game reads them (integer and float)
//   settings                        log every setting shared with SoH: SoH's value and 2Ship's (SettingsMap.def)
//   give <item>                     Item_Give (Majora's Mask item id)
//   inventory                       log what the shared save carries: masks, items, life, rupees, and Termina's
//                                   Hyrule views (Items (Hyrule), Hyrule's quest status, Equipment)
//   shared                          log this file's shared save (as it came, or as the last capture left it)
//   ootbutton <btn> <item>          put an Ocarina of Time item (MergeShipOoTItems.h: 0x11 Hyrule's Hookshot / Longshot,
//                                   0x0B Slingshot, 0xE0.. the others) on C-Left (1) / C-Down (2) / C-Right (3)
//   cbutton <btn> <item>            put a Majora's Mask item on a button (0 B, 1-3 C)
//   form <n>                        debug: the saved form becomes MM PlayerForm n (4 human, 3 Deku) -- takes effect with
//                                   the next scene (erwarp); lets a test use the human's gear during MM's first cycle
//   gear                            log Termina's gear: MM's sword, shield, B, ocarina, bow, Hookshot; Hyrule's hookshot,
//                                   swords and shields in Termina's Hyrule views; the magic meter; the file's first cycle
//   anchor <command ...>            Anchor: enable/disable/state/loop/give/send/dummies (2s2h/Network/Anchor)
//   trace on|off                    per-frame player state
//   mark <text>                     a marker in the log
//   shot <name>                     screenshot of the game window, <name>.bmp
//   ertable | erexit [key] | erwarp <entrance>
//                                   entrance shuffle over both worlds (M8, MergeShipEntrances.cpp): the overrides; take
//                                   a shuffled exit of this scene (or of `key`); a plain scene change
//   unified test [seed] | off       the unified randomizer's ledger for a file that is not a unified seed's (M3)
//   ledger                          log the shared save's ledger (every check found in either world)
//   ledgeradd <hyrule|termina> <item> [check] [flags]   an entry as the other engine would write it
//   terminafind <item> [check]      a Termina check holding <item> (MsItems.def key or RI_ name) is found
//   project                         give now what Hyrule found for Termina; then unifiedstate
//   unifiedstate                    log what Termina holds of Hyrule's items
//   hyrulecatalog                   the Hyrule catalog the last trip brought (M6/M7, Rando/MergeShip/UnifiedHyrule.h)
//   hyrulehint <item...> | stones [n]   where each item's hint says it is (RI_ name or MsItems.def key); the first
//                                   n Hyrule hints a Gossip Stone could give
//   hyruletrack                     the check tracker's Hyrule section, from the ledger
//
// Buttons: A B Z R L S CU CD CL CR DU DD DL DR, joined with '+'.

#include "2s2h/MergeShip/MergeShipHost.h"
#include "2s2h/MergeShip/MergeShipEntrances.h"
#include "2s2h/MergeShip/MergeShipSettingsSync.h"
#include "2s2h/MergeShip/MergeShipHyrule.h"
#include "2s2h/MergeShip/MergeShipOoTEquip.h"
#include "2s2h/MergeShip/MergeShipOoTItems.h"
#include "2s2h/MergeShip/MergeShipOwnership.h" // MergeShip_Own_BowIsOoT (the gear log)
#include <mergeship/SharedSave.h>
#include "2s2h/Rando/MergeShip/UnifiedDelivery.h"
#include "2s2h/Rando/MergeShip/UnifiedGoal.h"
#include "2s2h/Rando/MergeShip/UnifiedHyrule.h"
#include "2s2h/Rando/MergeShip/UnifiedSeed.h"

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

// The film camera (MergeShipTest.cpp, include/mergeship/CineCam.hpp).
std::string MergeShipCine_Command(const std::vector<std::string>& args);
void MergeShipCine_Frame();
std::string MergeShipCine_Hud(bool off);

#ifdef _WIN32 // MergeShip2 Android: Windows only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <strings.h>
#define _stricmp strcasecmp
#endif

#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
void MergeShipAnchor_TestCommand(const char* args); // 2s2h/Network/Anchor/MergeShipAnchorHost.cpp
}

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
    bool pressRelease = false;
    bool trace = false;
    int steerFrames = 0;
    Vec3f steerTarget = {};
    u64 frame = 0;
    FILE* log = nullptr;
    std::string dir;
};

Harness sT;

void Log(const char* fmt, ...) {
    if (sT.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sT.log, "[%06llu] ", (unsigned long long)sT.frame);
    vfprintf(sT.log, fmt, ap);
    fputc('\n', sT.log);
    va_end(ap);
    fflush(sT.log);
}

u16 ParseButtons(const std::string& s) {
    static const struct {
        const char* name;
        u16 bit;
    } kButtons[] = {
        { "A", BTN_A },     { "B", BTN_B },       { "Z", BTN_Z },       { "R", BTN_R },        { "L", BTN_L },
        { "S", BTN_START }, { "CU", BTN_CUP },    { "CD", BTN_CDOWN },  { "CL", BTN_CLEFT },   { "CR", BTN_CRIGHT },
        { "DU", BTN_DUP },  { "DD", BTN_DDOWN },  { "DL", BTN_DLEFT },  { "DR", BTN_DRIGHT },
    };
    u16 mask = 0;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        for (auto& b : kButtons) {
            if (_stricmp(tok.c_str(), b.name) == 0) {
                mask |= b.bit;
            }
        }
    }
    return mask;
}

long ParseInt(const std::string& s) {
    return strtol(s.c_str(), nullptr, 0);
}

// PrintWindow with PW_RENDERFULLCONTENT reads the compositor's copy, which works for a D3D swap chain.
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
    std::string path = sT.dir + "\\" + name + ".bmp";
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

void TracePlayer() {
    if (gPlayState == nullptr) {
        return;
    }
    Player* p = GET_PLAYER(gPlayState);
    if (p == nullptr) {
        return;
    }
    Log("P scene=%d room=%d entrance=0x%X form=%d pos=(%.1f,%.1f,%.1f) yaw=%d spd=%.2f s1=%08X s2=%08X hp=%d/%d "
        "rupees=%d day=%d time=0x%04X door=%s",
        gPlayState->sceneId, gPlayState->roomCtx.curRoom.num, gSaveContext.save.entrance, gSaveContext.save.playerForm,
        p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z, p->actor.shape.rot.y, p->speedXZ,
        p->stateFlags1, p->stateFlags2, gSaveContext.save.saveInfo.playerData.health,
        gSaveContext.save.saveInfo.playerData.healthCapacity, gSaveContext.save.saveInfo.playerData.rupees,
        gSaveContext.save.day, gSaveContext.save.time, MergeShipHost_DoorState());
}

void LogInventory() {
    std::string masks;
    for (s32 slot = ITEM_NUM_SLOTS; slot < ITEM_NUM_SLOTS + MASK_NUM_SLOTS; slot++) {
        char item[8];
        snprintf(item, sizeof(item), "%02X ", gSaveContext.save.saveInfo.inventory.items[slot]);
        masks += item;
    }
    std::string items;
    for (s32 slot = 0; slot < ITEM_NUM_SLOTS; slot++) {
        char item[8];
        snprintf(item, sizeof(item), "%02X ", gSaveContext.save.saveInfo.inventory.items[slot]);
        items += item;
    }
    Log("INVENTORY file=%d hp=%d/%d rupees=%d magic=%d sword/shield=0x%04X songs/quest=0x%08X resets=%d", gSaveContext.fileNum,
        gSaveContext.save.saveInfo.playerData.health, gSaveContext.save.saveInfo.playerData.healthCapacity,
        gSaveContext.save.saveInfo.playerData.rupees, gSaveContext.save.saveInfo.playerData.magic,
        gSaveContext.save.saveInfo.equips.equipment, gSaveContext.save.saveInfo.inventory.questItems,
        gSaveContext.save.saveInfo.playerData.threeDayResetCount);
    Log("INVENTORY items: %s", items.c_str());
    Log("INVENTORY masks: %s", masks.c_str());

    // Termina's Hyrule views (MergeShipOoTItems.h, MergeShipHyrule.h, MergeShipOoTEquip.h).
    const MergeShipOoTItemsSave& ootItems = gSaveContext.save.shipSaveInfo.mergeShipOoTItems;
    std::string owned;
    const u8 kOoTItems[] = { ITEM_SLINGSHOT, ITEM_LONGSHOT };
    for (u8 item : kOoTItems) {
        if (MergeShip_OoTItems_Owns(item)) {
            owned += std::string(MergeShip_OoTItems_Name(item)) + ", ";
        }
    }
    for (u32 item = ITEM_OOT_DINS_FIRE; item <= ITEM_OOT_DEKU_SEEDS; item++) {
        if (ITEM_IS_OOT_ITEM(item) && (item != ITEM_OOT_DEKU_SEEDS) && MergeShip_OoTItems_Owns((u8)item)) {
            owned += std::string(MergeShip_OoTItems_Name((u8)item)) + ", ";
        }
    }
    Log("INVENTORY hyrule items: owned=0x%08X seeds=%d bulletBag=%d tradeChild=0x%02X tradeAdult=0x%02X: %s",
        ootItems.owned, ootItems.seeds, ootItems.bulletBag, ootItems.tradeChild, ootItems.tradeAdult, owned.c_str());
    std::string songs;
    static const char* kQuestNames[MERGESHIP_OOT_QUEST_MAX] = {
        "Forest", "Fire",   "Water",  "Spirit", "Shadow",   "Light",   "Minuet",  "Bolero",
        "Serenade", "Requiem", "Nocturne", "Prelude", "Lullaby", "Epona", "Saria", "Sun",
        "Time", "Storms", "Emerald", "Ruby", "Sapphire", "Agony", "Gerudo Card",
    };
    for (u8 q = 0; q < MERGESHIP_OOT_QUEST_MAX; q++) {
        if (MergeShip_Hyrule_HasQuestItem(q)) {
            songs += std::string(kQuestNames[q]) + ", ";
        }
    }
    Log("INVENTORY hyrule quest: bits=0x%06X has: %s", MergeShip_Hyrule_GetQuestItems(), songs.c_str());
    std::string gear;
    for (u32 item = ITEM_OOT_SWORD_MASTER; item <= ITEM_OOT_BULLET_BAG; item++) {
        if (MergeShip_OoTEquip_Owns((u8)item)) {
            gear += std::string(MergeShip_OoTEquip_Name((u8)item)) + ", ";
        }
    }
    const MergeShipOoTEquipSave& ootEquip = gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
    Log("INVENTORY hyrule equipment: owned=0x%08X strength=%d scale=%d bgs=%d knife=%d sword=%d shield=%d tunic=%d "
        "boots=%d: %s",
        ootEquip.owned, ootEquip.strength, ootEquip.scale, ootEquip.bgsFlag, ootEquip.swordHealth, ootEquip.sword,
        ootEquip.shield, ootEquip.tunic, ootEquip.boots, gear.c_str());
    Log("INVENTORY termina songs: time=%d healing=%d epona=%d storms=%d soaring=%d sonata=%d lullaby=%d bossa=%d "
        "elegy=%d oath=%d saria=%d sun=%d ocarina=0x%02X form=%d",
        CHECK_QUEST_ITEM(QUEST_SONG_TIME) != 0, CHECK_QUEST_ITEM(QUEST_SONG_HEALING) != 0,
        CHECK_QUEST_ITEM(QUEST_SONG_EPONA) != 0, CHECK_QUEST_ITEM(QUEST_SONG_STORMS) != 0,
        CHECK_QUEST_ITEM(QUEST_SONG_SOARING) != 0, CHECK_QUEST_ITEM(QUEST_SONG_SONATA) != 0,
        CHECK_QUEST_ITEM(QUEST_SONG_LULLABY) != 0, CHECK_QUEST_ITEM(QUEST_SONG_BOSSA_NOVA) != 0,
        CHECK_QUEST_ITEM(QUEST_SONG_ELEGY) != 0, CHECK_QUEST_ITEM(QUEST_SONG_OATH) != 0,
        CHECK_QUEST_ITEM(QUEST_SONG_SARIA) != 0, CHECK_QUEST_ITEM(QUEST_SONG_SUN) != 0,
        INV_CONTENT(ITEM_OCARINA_OF_TIME), gSaveContext.save.playerForm);
}

void LogShared() {
    const MsSharedSave* shared = static_cast<const MsSharedSave*>(MergeShipHost_SharedSave());
    if (shared == nullptr) {
        Log("SHARED none (not a Termina linked to a Hyrule file)");
        return;
    }
    Log("SHARED gen=%u writer=%d slot=%d hp=%d/%d rupees=%d masks=0x%X/0x%08X items=0x%08X songs=0x%08X",
        shared->generation, shared->lastWriter, shared->saveSlot, shared->health, shared->healthCapacity,
        shared->rupees, shared->transformationMasks, shared->mmMasks, shared->items, shared->songs);
    Log("SHARED hyrule block=0x%X quest=0x%06X items=0x%06X trade=%d/%d seeds=%d tokens=%d swords=0x%02X "
        "shields=0x%02X equipment=0x%04X upgrades=0x%06X",
        shared->hyruleBlock, shared->hyruleQuest, shared->hyruleItems, shared->hyruleTradeChild,
        shared->hyruleTradeAdult, shared->hyruleSeeds, shared->gsTokens, shared->swords, shared->shields,
        shared->ootOwnedEquipment, shared->ootUpgrades);
    Log("SHARED own hyrule=0x%02X termina=0x%02X valid=0x%02X combined=%d hyruleBeans=%d/%d magicBeans=%d",
        shared->hyruleOwn, shared->terminaOwn, shared->ownValid, shared->itemsCombined, shared->hyruleBeans,
        shared->hyruleBeansWriter, shared->magicBeans);
    Log("SHARED extras=0x%02X dekuBeans=%d goldBeans=%d bank=%d/%u gossip=%d", shared->hyruleExtras,
        shared->dekuBeanAmmo, shared->goldBeanAmmo, shared->bankValid, shared->bankRupees[0] | (shared->bankRupees[1] << 8),
        shared->gossipHints);
}

bool InPlay() {
    return gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr &&
           gPlayState->transitionTrigger == TRANS_TRIGGER_OFF && gPlayState->transitionMode == TRANS_MODE_OFF;
}

struct ExitOpening {
    Vec3f center;
    Vec3f normal;
    int polys;
    f32 dist;
};

std::vector<ExitOpening> FindExitOpenings(s32 exitIndex) {
    std::vector<ExitOpening> openings;
    if (gPlayState == nullptr || gPlayState->colCtx.colHeader == nullptr) {
        return openings;
    }
    CollisionHeader* header = gPlayState->colCtx.colHeader;
    Player* player = GET_PLAYER(gPlayState);
    for (s32 i = 0; i < header->numPolygons; i++) {
        CollisionPoly* poly = &header->polyList[i];
        if ((s32)SurfaceType_GetSceneExitIndex(&gPlayState->colCtx, poly, BGCHECK_SCENE) != exitIndex) {
            continue;
        }
        Vec3f c = { 0.0f, 0.0f, 0.0f };
        for (s32 v = 0; v < 3; v++) {
            Vec3s* vtx = &header->vtxList[poly->vtxData[v] & 0x1FFF];
            c.x += vtx->x / 3.0f;
            c.y += vtx->y / 3.0f;
            c.z += vtx->z / 3.0f;
        }
        ExitOpening* found = nullptr;
        for (auto& o : openings) {
            if (Math_Vec3f_DistXZ(&o.center, &c) < 250.0f) {
                found = &o;
            }
        }
        if (found == nullptr) {
            openings.push_back({ c, { 0, 0, 0 }, 0, 0.0f });
            found = &openings.back();
        } else {
            f32 n = (f32)found->polys;
            found->center.x = (found->center.x * n + c.x) / (n + 1);
            found->center.y = (found->center.y * n + c.y) / (n + 1);
            found->center.z = (found->center.z * n + c.z) / (n + 1);
        }
        found->normal.x += COLPOLY_GET_NORMAL(poly->normal.x);
        found->normal.y += COLPOLY_GET_NORMAL(poly->normal.y);
        found->normal.z += COLPOLY_GET_NORMAL(poly->normal.z);
        found->polys++;
    }
    for (auto& o : openings) {
        o.dist = (player != nullptr) ? Math_Vec3f_DistXZ(&o.center, &player->actor.world.pos) : 0.0f;
    }
    std::sort(openings.begin(), openings.end(),
              [](const ExitOpening& a, const ExitOpening& b) { return a.dist < b.dist; });
    return openings;
}

void LogExitOpenings(s32 exitIndex) {
    auto openings = FindExitOpenings(exitIndex);
    for (size_t k = 0; k < openings.size(); k++) {
        const auto& o = openings[k];
        Log("EXIT %d (entrance 0x%X) opening %zu: center=(%.1f,%.1f,%.1f) polys=%d normal=(%.2f,%.2f,%.2f) dist=%.1f",
            exitIndex, (gPlayState->setupExitList != nullptr) ? gPlayState->setupExitList[exitIndex - 1] : 0xFFFF, k,
            o.center.x, o.center.y, o.center.z, o.polys, o.normal.x, o.normal.y, o.normal.z, o.dist);
    }
}

// The player turns towards the stick's angle, atan2(-stick_x, stick_y) (0 = up, Lib_GetControlStickData), plus the
// camera's input yaw.
void Steer() {
    Player* player = GET_PLAYER(gPlayState);
    Camera* camera = GET_ACTIVE_CAM(gPlayState);
    if (player == nullptr || camera == nullptr) {
        sT.stickX = sT.stickY = 0;
        return;
    }
    s16 yaw = Math_Vec3f_Yaw(&player->actor.world.pos, &sT.steerTarget);
    s16 angle = (s16)(yaw - Camera_GetInputDirYaw(camera));
    sT.stickX = (s8)(-Math_SinS(angle) * 100.0f);
    sT.stickY = (s8)(Math_CosS(angle) * 100.0f);
}

Actor* FindActor(s16 id, int want, bool log) {
    Actor* chosen = nullptr;
    int n = 0;
    Player* p = GET_PLAYER(gPlayState);
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->id != id) {
                continue;
            }
            if (log) {
                Log("ACTOR id=0x%03X #%d params=0x%04X pos=(%.1f,%.1f,%.1f) yaw=%d dist=%.1f room=%d", id, n,
                    (u16)a->params, a->world.pos.x, a->world.pos.y, a->world.pos.z, a->shape.rot.y,
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

void Step() {
    if (sT.steerFrames > 0) {
        if (gPlayState == nullptr || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
            Log("STEER stopped: transition started (door %s)", MergeShipHost_DoorState());
            sT.steerFrames = 0;
            sT.stickX = sT.stickY = 0;
        } else {
            Steer();
            if (--sT.steerFrames > 0) {
                return;
            }
            sT.stickX = sT.stickY = 0;
            Log("STEER done");
            TracePlayer();
        }
    }
    if (sT.pressRelease) {
        sT.pressRelease = false;
        sT.buttons = 0;
        return;
    }
    if (sT.waitingForPlay) {
        if (!InPlay()) {
            return;
        }
        if (sT.afterPlayFrames-- > 0) {
            return;
        }
        sT.waitingForPlay = false;
        Log("PLAY ready scene=%d room=%d entrance=0x%X door=%s", gPlayState->sceneId, gPlayState->roomCtx.curRoom.num,
            gSaveContext.save.entrance, MergeShipHost_DoorState());
    }
    if (sT.waitFrames > 0) {
        if (--sT.waitFrames > 0) {
            return;
        }
        sT.buttons = 0;
        sT.stickX = sT.stickY = 0;
    }

    while (sT.pc < sT.commands.size()) {
        const Command& c = sT.commands[sT.pc++];
        auto arg = [&](size_t i) -> std::string { return i < c.args.size() ? c.args[i] : std::string(); };
        const bool play = (gPlayState != nullptr) && (GET_PLAYER(gPlayState) != nullptr);

        if (c.op == "waitplay") {
            sT.waitingForPlay = true;
            sT.afterPlayFrames = c.args.empty() ? 0 : (int)ParseInt(arg(0));
            return;
        } else if (c.op == "wait") {
            sT.buttons = 0;
            sT.stickX = sT.stickY = 0;
            sT.waitFrames = (int)ParseInt(arg(0));
            return;
        } else if (c.op == "hold") {
            sT.buttons = ParseButtons(arg(0));
            sT.stickX = sT.stickY = 0;
            sT.waitFrames = (int)ParseInt(arg(1));
            return;
        } else if (c.op == "stick") {
            sT.stickX = (s8)ParseInt(arg(0));
            sT.stickY = (s8)ParseInt(arg(1));
            sT.waitFrames = (int)ParseInt(arg(2));
            sT.buttons = ParseButtons(arg(3));
            return;
        } else if (c.op == "press") {
            sT.buttons = ParseButtons(arg(0));
            sT.stickX = sT.stickY = 0;
            sT.pressRelease = true;
            return;
        } else if (c.op == "warpreq") {
            // warpreq <OoT entrance>: back to Hyrule as a warp song does (MergeShip_RequestHyruleWarp).
            Log("WARPREQ 0x%X -> %d", (u16)ParseInt(arg(0)), MergeShip_RequestHyruleWarp((u16)ParseInt(arg(0))));
        } else if (c.op == "mmwarp") {
            // mmwarp <MM entrance>: change scene within Termina (BUG-0129's soft soil; follow with waitplay).
            if (play) {
                gPlayState->nextEntrance = (u16)ParseInt(arg(0));
                gPlayState->transitionTrigger = TRANS_TRIGGER_START;
                gPlayState->transitionType = TRANS_TYPE_INSTANT;
            }
            Log("MMWARP 0x%X", (u32)ParseInt(arg(0)));
        } else if (c.op == "bank") {
            // bank [rupees]: set the Clock Town bank's balance; logs it (BUG-0030: it travels with the shared save).
            if (!arg(0).empty()) {
                HS_SET_BANK_RUPEES((u32)ParseInt(arg(0)));
            }
            Log("BANK %u rupees=%d", (u32)HS_GET_BANK_RUPEES(), gSaveContext.save.saveInfo.playerData.rupees);
        } else if (c.op == "pos") {
            if (play) {
                Player* p = GET_PLAYER(gPlayState);
                p->actor.world.pos.x = p->actor.home.pos.x = (f32)atof(arg(0).c_str());
                p->actor.world.pos.y = p->actor.home.pos.y = (f32)atof(arg(1).c_str());
                p->actor.world.pos.z = p->actor.home.pos.z = (f32)atof(arg(2).c_str());
                if (!arg(3).empty()) {
                    p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)ParseInt(arg(3));
                }
            }
            Log("POS %s %s %s", arg(0).c_str(), arg(1).c_str(), arg(2).c_str());
        } else if (c.op == "exits") {
            if (play) {
                if (!c.args.empty()) {
                    LogExitOpenings((s32)ParseInt(arg(0)));
                } else {
                    for (s32 e = 1; e < 32; e++) {
                        LogExitOpenings(e);
                    }
                }
                Log("EXITS scene=%d room=%d", gPlayState->sceneId, gPlayState->roomCtx.curRoom.num);
            }
        } else if ((c.op == "walkexit") || (c.op == "toexit")) {
            if (!play) {
                continue;
            }
            s32 exitIndex = (s32)ParseInt(arg(0));
            size_t nth = arg(2).empty() ? 0 : (size_t)ParseInt(arg(2));
            auto openings = FindExitOpenings(exitIndex);
            if (nth >= openings.size()) {
                Log("%s %d: no opening %zu (%zu found)", c.op.c_str(), exitIndex, nth, openings.size());
                continue;
            }
            const ExitOpening& o = openings[nth];
            if (c.op == "toexit") {
                f32 dist = arg(1).empty() ? 150.0f : (f32)atof(arg(1).c_str());
                Player* p = GET_PLAYER(gPlayState);
                f32 dx = p->actor.world.pos.x - o.center.x;
                f32 dz = p->actor.world.pos.z - o.center.z;
                f32 len = sqrtf(dx * dx + dz * dz);
                if (len < 1.0f) {
                    dx = o.normal.x;
                    dz = o.normal.z;
                    len = sqrtf(dx * dx + dz * dz) + 0.001f;
                }
                Vec3f center = o.center;
                p->actor.world.pos.x = p->actor.home.pos.x = o.center.x + dx / len * dist;
                p->actor.world.pos.z = p->actor.home.pos.z = o.center.z + dz / len * dist;
                p->actor.world.pos.y = p->actor.home.pos.y = p->actor.world.pos.y + 20.0f;
                p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = Math_Vec3f_Yaw(&p->actor.world.pos, &center);
                Log("TOEXIT %d opening %zu center=(%.1f,%.1f,%.1f) -> pos=(%.1f,%.1f,%.1f)", exitIndex, nth, o.center.x,
                    o.center.y, o.center.z, p->actor.world.pos.x, p->actor.world.pos.y, p->actor.world.pos.z);
            } else {
                sT.steerTarget = o.center;
                sT.steerFrames = (int)ParseInt(arg(1));
                sT.buttons = 0;
                Log("WALKEXIT %d opening %zu center=(%.1f,%.1f,%.1f) dist=%.1f room=%d", exitIndex, nth, o.center.x,
                    o.center.y, o.center.z, o.dist, gPlayState->roomCtx.curRoom.num);
                Steer();
                return;
            }
        } else if (c.op == "walkto") {
            if (!play) {
                continue;
            }
            sT.steerTarget = { (f32)atof(arg(0).c_str()), GET_PLAYER(gPlayState)->actor.world.pos.y,
                               (f32)atof(arg(1).c_str()) };
            sT.steerFrames = (int)ParseInt(arg(2));
            sT.buttons = 0;
            Steer();
            Log("WALKTO %s %s", arg(0).c_str(), arg(1).c_str());
            return;
        } else if ((c.op == "actors") || (c.op == "infront")) {
            if (!play) {
                continue;
            }
            s16 id = (s16)ParseInt(arg(0));
            int want = (c.op == "infront" && !arg(2).empty()) ? (int)ParseInt(arg(2)) : 0;
            Actor* chosen = FindActor(id, want, true);
            if ((c.op == "infront") && (chosen != nullptr)) {
                Player* p = GET_PLAYER(gPlayState);
                f32 dist = arg(1).empty() ? 120.0f : (f32)atof(arg(1).c_str());
                s16 yaw = chosen->shape.rot.y;
                p->actor.world.pos.x = p->actor.home.pos.x = chosen->world.pos.x + Math_SinS(yaw) * dist;
                p->actor.world.pos.y = p->actor.home.pos.y = chosen->world.pos.y;
                p->actor.world.pos.z = p->actor.home.pos.z = chosen->world.pos.z + Math_CosS(yaw) * dist;
                p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = (s16)(yaw + 0x8000);
                Log("INFRONT 0x%03X -> pos=(%.1f,%.1f,%.1f)", id, p->actor.world.pos.x, p->actor.world.pos.y,
                    p->actor.world.pos.z);
            }
        } else if (c.op == "cvar") {
            CVarSetInteger(arg(0).c_str(), (int32_t)ParseInt(arg(1)));
            ShipInit::Init(arg(0));
            Log("CVAR %s = %s", arg(0).c_str(), arg(1).c_str());
        } else if (c.op == "cvarlog") {
            // MergeShip2 settings sharing (MergeShipSettingsSync.h): what 2Ship reads for each named CVar.
            for (const std::string& name : c.args) {
                Log("CVARLOG %s int=%d float=%g", name.c_str(), CVarGetInteger(name.c_str(), -1),
                    CVarGetFloat(name.c_str(), -1.0f));
            }
        } else if (c.op == "settings") {
            MergeShipSettings_LogAll(); // every shared setting: SoH's value and 2Ship's
        } else if (c.op == "give") {
            u8 result = 0;
            if (play) {
                result = Item_Give(gPlayState, (u8)ParseInt(arg(0)));
            }
            Log("GIVE %s -> 0x%02X", arg(0).c_str(), result);
        } else if (c.op == "cbutton") {
            // cbutton <button> <MM item>: as MergeShipTest.cpp's (C buttons in row 0 for every form, B per form).
            const s32 btn = (s32)ParseInt(arg(0));
            const u8 item = (u8)ParseInt(arg(1));
            const s32 row = (btn == EQUIP_SLOT_B) ? CUR_FORM : 0;
            BUTTON_ITEM_EQUIP(row, btn) = item;
            C_SLOT_EQUIP(row, btn) = SLOT(item);
            gSaveContext.buttonStatus[btn] = BTN_ENABLED;
            if (gPlayState != nullptr) {
                Interface_LoadItemIconImpl(gPlayState, btn);
            }
            Log("CBUTTON %d item=0x%02X slot=%d form=%d", btn, item, SLOT(item), gSaveContext.save.playerForm);
        } else if (c.op == "ootbottle") {
            // ootbottle <0-3> <MM item>: what one of Hyrule's bottles holds, as Majora's Mask's item (BUG-0024).
            MergeShip_Own_SetOoTBottle((s32)ParseInt(arg(0)), (u8)ParseInt(arg(1)));
            Log("OOTBOTTLE %s = 0x%02X", arg(0).c_str(), MergeShip_Own_OoTBottle((s32)ParseInt(arg(0))));
        } else if (c.op == "inventory") {
            LogInventory();
        } else if (c.op == "ootbutton") {
            // As the Items (Hyrule) page equips one (MergeShipOoTItems.h MS_OOT_BUTTON_SLOT).
            const s32 btn = (s32)ParseInt(arg(0));
            const u8 item = (u8)ParseInt(arg(1));
            const s32 slot = MergeShip_OoTItems_SlotOfItem(item);
            if ((btn >= EQUIP_SLOT_C_LEFT) && (btn <= EQUIP_SLOT_C_RIGHT) && (slot >= 0) &&
                MergeShip_OoTItems_Owns(item)) {
                BUTTON_ITEM_EQUIP(0, btn) = item;
                C_SLOT_EQUIP(0, btn) = MS_OOT_BUTTON_SLOT(slot);
                if (gPlayState != nullptr) {
                    Interface_LoadItemIconImpl(gPlayState, btn);
                }
            }
            Log("OOTBUTTON %d = 0x%02X (%s) slot=%d owned=%d", btn, item, MergeShip_OoTItems_Name(item), slot,
                MergeShip_OoTItems_Owns(item) ? 1 : 0);
        } else if (c.op == "hud") {
            // hud off|on: the HUD hidden for filming (MergeShipTest.cpp).
            Log("%s", MergeShipCine_Hud(arg(0) != "on").c_str());
        } else if (c.op == "cinecam") {
            // The film camera (include/mergeship/CineCam.hpp, shared with MergeShipTest.cpp).
            Log("%s", MergeShipCine_Command(c.args).c_str());
        } else if (c.op == "pausepage") {
            // The pause menu opens on the page it last showed: this one next time (0 Select Item, 1 Map, 2 Quest, 3 Masks).
            if (gPlayState != nullptr) {
                gPlayState->pauseCtx.pageIndex = (u16)ParseInt(arg(0));
            }
            Log("PAUSEPAGE %s", arg(0).c_str());
        } else if (c.op == "form") {
            gSaveContext.save.playerForm = (u8)ParseInt(arg(0));
            Log("FORM saved form = %d (next scene)", gSaveContext.save.playerForm);
        } else if (c.op == "ammo") {
            // ammo <MM item> <count>: set Termina's count of that item (BUG-0105: each game's own); "gear" logs them.
            AMMO((u8)ParseInt(arg(0))) = (s8)ParseInt(arg(1));
            Log("AMMO item 0x%02X = %d", (u8)ParseInt(arg(0)), AMMO((u8)ParseInt(arg(0))));
        } else if (c.op == "gear") {
            // build/tests/door_no_free_gear: nothing granted, each game's version its own.
            const MergeShipOoTEquipSave& ootEquip = gSaveContext.save.shipSaveInfo.mergeShipOoTEquip;
            const auto& player = gSaveContext.save.saveInfo.playerData;
            Log("GEAR mmSword=%d mmShield=%d B=0x%02X ocarina=0x%02X bow=0x%02X mmHookshot=0x%02X "
                "ootHookshot=%d(%s) ootEquip=0x%08X ootSword=%d ootShield=%d ootMirror=%d ootHylian=%d magic=%d/%d "
                "level=%d amount=%d bareStart=%d resets=%d form=%d dekuMask=0x%02X songTime=%d songHealing=%d",
                GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD), GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD),
                BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B), INV_CONTENT(ITEM_OCARINA_OF_TIME), INV_CONTENT(ITEM_BOW),
                INV_CONTENT(ITEM_HOOKSHOT), MergeShip_OoTItems_HookshotLevel(),
                MergeShip_OoTItems_Owns(ITEM_LONGSHOT) ? MergeShip_OoTItems_Name(ITEM_LONGSHOT) : "none", ootEquip.owned,
                ootEquip.sword, ootEquip.shield, MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_MIRROR) ? 1 : 0,
                MergeShip_OoTEquip_Owns(ITEM_OOT_SHIELD_HYLIAN) ? 1 : 0, player.isMagicAcquired ? 1 : 0,
                player.isDoubleMagicAcquired ? 1 : 0, player.magicLevel, player.magic,
                (ootEquip.variants & 0x80) ? 1 : 0, player.threeDayResetCount, gSaveContext.save.playerForm,
                INV_CONTENT(ITEM_MASK_DEKU), CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? 1 : 0,
                CHECK_QUEST_ITEM(QUEST_SONG_HEALING) ? 1 : 0);
            // The one ammo pool (SharedSave.h MsSharedSave.ammo) as Termina reads it, and whose bow the slot shows.
            Log("GEAR termina swords=0x%X shields=0x%X (MS_TEQ_SWORD_BIT / MS_TEQ_SHIELD_BIT: Termina Equipment)",
                ootEquip.terminaSwords, ootEquip.terminaShields);
            Log("GEAR ammo bombs=%d chus=%d sticks=%d nuts=%d arrows=%d bowIsOoT=%d hp=%d/%d", AMMO(ITEM_BOMB),
                AMMO(ITEM_BOMBCHU), AMMO(ITEM_DEKU_STICK), AMMO(ITEM_DEKU_NUT), AMMO(ITEM_BOW),
                MergeShip_Own_BowIsOoT(), player.health, player.healthCapacity);
        } else if (c.op == "shared") {
            LogShared();
        } else if (c.op == "anchor") {
            std::string line;
            for (const std::string& a : c.args) {
                line += (line.empty() ? "" : " ") + a;
            }
            MergeShipAnchor_TestCommand(line.c_str());
        } else if (c.op == "trace") {
            sT.trace = (arg(0) == "on");
        } else if (c.op == "mark") {
            std::string text;
            for (auto& a : c.args) {
                text += a + " ";
            }
            Log("MARK %s", text.c_str());
            TracePlayer();
        } else if (c.op == "shot") {
            Screenshot(arg(0));
        } else if (MergeShipEntrances_TestCommand(c.op, c.args)) {
            // MergeShip2 M8: ertable, erexit, erwarp (MergeShipEntrances.cpp)
        } else if (Rando::MergeShip::TestCommand(c.op, c.args)) {
            // MergeShip2 M3: unified, ledger, ledgeradd, terminafind, project, unifiedstate (UnifiedDelivery.h)
        } else if (Rando::MergeShip::GoalTestCommand(c.op, c.args)) {
            // MergeShip2: goal state|fake|hyrule|majora|pieces, the unified randomizer's goal (UnifiedGoal.h)
        } else if (Rando::MergeShip::HyruleTestCommand(c.op, c.args)) {
            // MergeShip2 M6/M7: hyrulecatalog, hyrulehint, hyruletrack (UnifiedHyrule.h)
        } else if (Rando::MergeShip::SeedTestCommand(c.op, c.args)) {
            // MergeShip2: unifiedseed [check <RandoCheckId>], the seed Hyrule handed over (UnifiedSeed.h)
        } else {
            Log("WARN line %d: unknown command '%s'", c.line, c.op.c_str());
        }
    }
}

} // namespace

void MergeShipHostTest_Init() {
    const char* script = getenv("MERGESHIP_TEST_SCRIPT_2SHIP");
    if (script == nullptr || *script == '\0') {
        return;
    }
    std::ifstream in(script);
    if (!in) {
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
        sT.commands.push_back(c);
    }
    std::string p(script);
    auto slash = p.find_last_of("\\/");
    sT.dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
    sT.log = fopen((sT.dir + "\\trace_2ship.log").c_str(), "w");
    sT.active = true;
    Log("SCRIPT %s: %zu commands", script, sT.commands.size());
}

// Once per frame, before the frame runs (EngineExport.cpp).
void MergeShipHostTest_Frame() {
    if (!sT.active) {
        return;
    }
    sT.frame++;
    Step();
    MergeShipCine_Frame();
    if (sT.trace) {
        TracePlayer();
    }
}

extern "C" void MergeShipHostTest_OverrideInput(OSContPad* pad) {
    if (!sT.active) {
        return;
    }
    pad->button = sT.buttons;
    pad->stick_x = sT.stickX;
    pad->stick_y = sT.stickY;
    pad->err_no = 0;
}

extern "C" void MergeShipHostTest_Log(const char* fmt, ...) {
    if (sT.log == nullptr) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    fprintf(sT.log, "[%06llu] ", (unsigned long long)sT.frame);
    vfprintf(sT.log, fmt, ap);
    fputc('\n', sT.log);
    va_end(ap);
    fflush(sT.log);
}
