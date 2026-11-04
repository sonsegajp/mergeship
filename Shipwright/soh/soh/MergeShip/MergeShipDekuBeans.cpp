// MergeShip2 -- Deku Beans (see MergeShipDekuBeans.h).
//
// Design:
// - Deku Beans are not a new item id. Every table Ocarina of Time indexes by item (icons, age requirements, slots,
//   ammo, item actions) is sized in headers whose change rebuilds the whole game, and an id past them would be read
//   off their ends. So the bean slot keeps ITEM_BEAN, and a per-file flag (the "mergeShipDekuBeans" save section)
//   says whether it holds Magic Beans or Deku Beans. The pause screen switches the flag the way SoH's Mask Select
//   switches the mask slot (z_kaleido_item.c, KaleidoScope_HandleDekuBeanCycle); everything that shows or uses the
//   bean item asks MergeShip_DekuBeans_IsSelected().
// - Count: Deku Beans do not use up Magic Beans. Once the player owns Magic Beans (and the Deku Mask) Deku Beans are
//   unlimited, so Ocarina of Time's bean economy (ten beans from the salesman, one per soft-soil patch) is untouched
//   whichever of the two the slot holds. The C button shows no count for them. Planted flowers are limited instead:
//   at most kMaxFlowers stand at a time; planting another removes the oldest (never the one the Deku is standing
//   in). They belong to no room, so they last until the scene changes (the scene's actors are cleared then).
// - Use: only Deku Link (MergeShip_FormCanUseItem, MergeShipItems.c; the HUD dims the button for every other form
//   and for Link). Majora's Mask has no Deku planting animation (its Magic Beans are human only), so the Deku plants
//   without one; the sprout plays Ocarina of Time's bean-growing sound (ObjBean's NA_SE_PL_PLANT_GROW_UP) while the
//   flower comes up. The flower is MM's Obj_Etcetera (ovl_MM_Obj_Etcetera) of type
//   MM_DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE: MM's flower that appears from nothing with a 30-frame bounce. The
//   Deku burrows into it and launches exactly as from any MM flower (MergeShip_Deku_Action_93/94).
// - Icon and name are made at run time from the player's own oot.o2r: the Magic Bean icon recoloured to browns and
//   earth tones, and the name "Deku Beans" put together from the "Deku" of the Deku Nuts name and the "Beans" of the
//   Magic Beans name (English for every language: Ocarina of Time has no such words to borrow in the others).

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/Texture.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <mergeship/AdultMask.hpp> // MsAdultMask::MemoryArchive: the bean models are mounted as one

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipDekuBeans.h"
#include "soh/MergeShip/MergeShipOwnership.h" // MergeShip_Own_OoTOwns (BUG-0068)
#include "src/overlays/actors/ovl_MM_Obj_Etcetera/z_mm_obj_etcetera.h"
extern PlayState* gPlayState;
void MergeShip_Deku_EnsureActorsRegistered(void);
void MergeShipTest_Log(const char* fmt, ...);
}

#define CVAR_DEKU_BEANS_NAME CVAR_ENHANCEMENT("MergeShip.DekuBeans")
#define CVAR_DEKU_BEANS_VALUE CVarGetInteger(CVAR_DEKU_BEANS_NAME, 1)
// BUG-0012 (the testers' suggestion, 2026-10-07): Deku Beans and Gold Deku Beans are ammo -- at most kBeanAmmoMax each,
// full when first obtained, one per flower planted, and found again in grass. Off: unlimited, as before.
#define CVAR_DEKU_BEAN_AMMO_NAME CVAR_ENHANCEMENT("MergeShip.DekuBeanAmmo")
#define CVAR_DEKU_BEAN_AMMO_VALUE CVarGetInteger(CVAR_DEKU_BEAN_AMMO_NAME, 1)

static bool DekuBeansAvailable();

namespace {

constexpr s32 kMaxFlowers = 3;
constexpr f32 kPlantDistance = 40.0f;  // in front of the Deku; the flower's collision is 21.25 units in radius
constexpr f32 kPlantMaxStep = 30.0f;   // the floor there may be this far above or below the Deku's feet
constexpr u32 kSproutSoundFrames = 20; // the flower's appearing bounce lasts 30 frames

u8 sSelected = false; // saved: the bean slot holds Deku Beans
// Saved: each kind's count, and whether it was ever filled (the first time the kind can be used).
constexpr u8 kBeanAmmoMax = 10;
constexpr s32 kGrassFindOneIn = 8;
u8 sDekuAmmo = 0;
u8 sGoldAmmo = 0;
u8 sDekuAmmoFilled = false;
u8 sGoldAmmoFilled = false;

struct PlantedFlower {
    Actor* actor;
    u32 plantedFrame;
};
PlantedFlower sFlowers[kMaxFlowers];
s32 sFlowerCount = 0;

// ---- Icon and name ----

alignas(8) u8 sIconTex[32 * 32 * 4]; // RGBA32
alignas(8) u8 sNameTex[128 * 16 / 2]; // IA4, the size of Ocarina of Time's item names
s32 sIconState = 0;                   // 0 not tried, 1 made, -1 failed
s32 sNameState = 0;
// Gold Deku Beans (the randomizer's second Progressive Deku Beans): the same, in gold.
alignas(8) u8 sGoldIconTex[32 * 32 * 4];
alignas(8) u8 sGoldNameTex[128 * 16 / 2];
s32 sGoldIconState = 0;
s32 sGoldNameState = 0;

std::shared_ptr<Fast::Texture> LoadOriginalTexture(const char* path) {
    // loadExact: the game's own texture, not a texture pack's replacement (whose size and format could be anything).
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true);
    if ((res == nullptr) || (res->GetInitData() == nullptr) ||
        (res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::Texture))) {
        return nullptr;
    }
    return std::static_pointer_cast<Fast::Texture>(res);
}

void RgbToHsv(f32 r, f32 g, f32 b, f32* h, f32* s, f32* v) {
    f32 max = std::fmax(r, std::fmax(g, b));
    f32 min = std::fmin(r, std::fmin(g, b));
    f32 d = max - min;

    *v = max;
    *s = (max > 0.0f) ? (d / max) : 0.0f;
    if (d <= 0.0f) {
        *h = 0.0f;
    } else if (max == r) {
        *h = 60.0f * std::fmod((g - b) / d, 6.0f);
    } else if (max == g) {
        *h = 60.0f * (((b - r) / d) + 2.0f);
    } else {
        *h = 60.0f * (((r - g) / d) + 4.0f);
    }
    if (*h < 0.0f) {
        *h += 360.0f;
    }
}

void HsvToRgb(f32 h, f32 s, f32 v, f32* r, f32* g, f32* b) {
    f32 c = v * s;
    f32 x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    f32 m = v - c;
    f32 rr = 0.0f;
    f32 gg = 0.0f;
    f32 bb = 0.0f;

    if (h < 60.0f) {
        rr = c, gg = x;
    } else if (h < 120.0f) {
        rr = x, gg = c;
    } else if (h < 180.0f) {
        gg = c, bb = x;
    } else if (h < 240.0f) {
        gg = x, bb = c;
    } else if (h < 300.0f) {
        rr = x, bb = c;
    } else {
        rr = c, bb = x;
    }
    *r = rr + m;
    *g = gg + m;
    *b = bb + m;
}

u8 ToByte(f32 f) {
    f = std::fmin(std::fmax(f, 0.0f), 1.0f);
    return (u8)(f * 255.0f + 0.5f);
}

// The Magic Bean icon is a green pod holding a red, a yellow, a blue and a green bean. Each hue band takes an earth
// tone: the pod (and the green bean) brown, the red bean terracotta, the yellow one ochre, the blue one moss. Black
// outlines and white highlights (little saturation) are kept. One colour (RGB bytes), shared with the get-item model.
void RecolorDeku(u8* rgb) {
    f32 h, s, v, nh, ns, nv, r, g, b;

    RgbToHsv(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, &h, &s, &v);
    if (s < 0.15f) {
        return;
    }
    if ((h >= 70.0f) && (h < 170.0f)) { // pod: brown
        nh = 26.0f, ns = s * 0.70f, nv = v * 0.85f;
    } else if ((h < 20.0f) || (h >= 330.0f)) { // red bean: terracotta
        nh = 14.0f, ns = s * 0.80f, nv = v * 0.85f;
    } else if (h < 70.0f) { // yellow bean: ochre
        nh = 40.0f, ns = s * 0.60f, nv = v * 0.90f;
    } else { // blue bean: moss
        nh = 75.0f, ns = s * 0.45f, nv = v * 0.70f;
    }
    HsvToRgb(nh, std::fmin(ns, 1.0f), std::fmin(nv, 1.0f), &r, &g, &b);
    rgb[0] = ToByte(r);
    rgb[1] = ToByte(g);
    rgb[2] = ToByte(b);
}

// Gold Deku Beans: every coloured part turned to gold (pod) and bright gold (beans).
void RecolorGold(u8* rgb) {
    f32 h, s, v, nh, ns, nv, r, g, b;

    RgbToHsv(rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f, &h, &s, &v);
    if (s < 0.15f) {
        return;
    }
    if ((h >= 70.0f) && (h < 170.0f)) { // pod: deep gold
        nh = 40.0f, ns = std::fmax(s, 0.75f), nv = v * 0.95f;
    } else { // beans: bright gold
        nh = 48.0f, ns = std::fmax(s * 0.9f, 0.6f), nv = std::fmin(v * 1.15f + 0.1f, 1.0f);
    }
    HsvToRgb(nh, std::fmin(ns, 1.0f), std::fmin(nv, 1.0f), &r, &g, &b);
    rgb[0] = ToByte(r);
    rgb[1] = ToByte(g);
    rgb[2] = ToByte(b);
}

void MakeIcon() {
    auto tex = LoadOriginalTexture(static_cast<const char*>(gItemIcons[ITEM_BEAN]));

    if ((tex == nullptr) || (tex->Type != Fast::TextureType::RGBA32bpp) || (tex->Width != 32) ||
        (tex->Height != 32) || (tex->ImageData == nullptr) || (tex->ImageDataSize < sizeof(sIconTex))) {
        sIconState = -1;
        return;
    }
    memcpy(sIconTex, tex->ImageData, sizeof(sIconTex));

    for (size_t i = 0; i < sizeof(sIconTex); i += 4) {
        RecolorDeku(&sIconTex[i]);
    }
    sIconState = 1;
}

// The Gold Deku Beans icon: every coloured pixel of the Magic Bean icon turned to gold (pod) and bright gold (beans).
void MakeGoldIcon() {
    auto tex = LoadOriginalTexture(static_cast<const char*>(gItemIcons[ITEM_BEAN]));

    if ((tex == nullptr) || (tex->Type != Fast::TextureType::RGBA32bpp) || (tex->Width != 32) ||
        (tex->Height != 32) || (tex->ImageData == nullptr) || (tex->ImageDataSize < sizeof(sGoldIconTex))) {
        sGoldIconState = -1;
        return;
    }
    memcpy(sGoldIconTex, tex->ImageData, sizeof(sGoldIconTex));

    for (size_t i = 0; i < sizeof(sGoldIconTex); i += 4) {
        RecolorGold(&sGoldIconTex[i]);
    }
    sGoldIconState = 1;
}

// IA4, 128 wide: two pixels a byte, the first in the high nibble.
u8 GetNibble(const u8* tex, s32 x, s32 y) {
    u8 byte = tex[(y * 128 + x) / 2];
    return (x & 1) ? (byte & 0xF) : (byte >> 4);
}

void SetNibble(u8* tex, s32 x, s32 y, u8 value) {
    u8* byte = &tex[(y * 128 + x) / 2];
    *byte = (x & 1) ? ((*byte & 0xF0) | value) : ((*byte & 0x0F) | (value << 4));
}

// The words of an item name: runs of columns holding any pixel.
s32 FindWords(const u8* tex, s32 (*words)[2], s32 maxWords) {
    s32 count = 0;
    s32 start = -1;

    for (s32 x = 0; x <= 128; x++) {
        bool used = false;
        for (s32 y = 0; (x < 128) && (y < 16); y++) {
            if (GetNibble(tex, x, y) != 0) {
                used = true;
                break;
            }
        }
        if (used && (start < 0)) {
            start = x;
        } else if (!used && (start >= 0)) {
            if (count < maxWords) {
                words[count][0] = start;
                words[count][1] = x - 1;
            }
            count++;
            start = -1;
        }
    }
    return count;
}

bool IsItemName(const std::shared_ptr<Fast::Texture>& tex) {
    return (tex != nullptr) && (tex->Type == Fast::TextureType::GrayscaleAlpha4bpp) && (tex->Width == 128) &&
           (tex->Height == 16) && (tex->ImageData != nullptr) && (tex->ImageDataSize >= sizeof(sNameTex));
}

// "Deku" from "Deku Nuts", "Beans" from "Magic Beans", two blank columns apart (the gap Ocarina of Time's names put
// between words), centred as the names are.
void MakeName() {
    auto nuts = LoadOriginalTexture("textures/item_name_static/gDekuNutItemNameENGTex");
    auto beans = LoadOriginalTexture("textures/item_name_static/gMagicBeansItemNameENGTex");
    s32 nutsWords[4][2];
    s32 beansWords[4][2];
    s32 nutsCount;
    s32 beansCount;

    if (!IsItemName(nuts) || !IsItemName(beans)) {
        sNameState = -1;
        return;
    }
    nutsCount = FindWords(nuts->ImageData, nutsWords, 4);
    beansCount = FindWords(beans->ImageData, beansWords, 4);
    if ((nutsCount < 2) || (nutsCount > 4) || (beansCount < 2) || (beansCount > 4)) {
        sNameState = -1;
        return;
    }

    s32 dekuStart = nutsWords[0][0];
    s32 dekuWidth = nutsWords[0][1] - dekuStart + 1;
    s32 beansStart = beansWords[beansCount - 1][0];
    s32 beansWidth = beansWords[beansCount - 1][1] - beansStart + 1;
    s32 width = dekuWidth + 2 + beansWidth;
    s32 x = (128 - width) / 2;

    if (width > 128) {
        sNameState = -1;
        return;
    }
    memset(sNameTex, 0, sizeof(sNameTex));
    for (s32 i = 0; i < dekuWidth; i++, x++) {
        for (s32 y = 0; y < 16; y++) {
            SetNibble(sNameTex, x, y, GetNibble(nuts->ImageData, dekuStart + i, y));
        }
    }
    x += 2;
    for (s32 i = 0; i < beansWidth; i++, x++) {
        for (s32 y = 0; y < 16; y++) {
            SetNibble(sNameTex, x, y, GetNibble(beans->ImageData, beansStart + i, y));
        }
    }
    sNameState = 1;
}

// "Gold Deku Beans": "Gold" from the Gold Skulltula name before the Deku Beans words. Falls back to "Deku Beans".
void MakeGoldName() {
    auto gold = LoadOriginalTexture("textures/item_name_static/gGoldSkulltulaItemNameENGTex");
    s32 goldWords[4][2];
    s32 goldCount;

    if (sNameState == 0) {
        MakeName();
    }
    if (!IsItemName(gold) || (sNameState <= 0)) {
        sGoldNameState = -1;
        return;
    }
    goldCount = FindWords(gold->ImageData, goldWords, 4);
    if (goldCount < 1) {
        sGoldNameState = -1;
        return;
    }
    // The Deku Beans name's own extent, to re-centre the three words.
    s32 dekuLeft = 128, dekuRight = -1;
    for (s32 x = 0; x < 128; x++) {
        for (s32 y = 0; y < 16; y++) {
            if (GetNibble(sNameTex, x, y) != 0) {
                dekuLeft = std::min(dekuLeft, x);
                dekuRight = std::max(dekuRight, x);
            }
        }
    }
    s32 goldStart = goldWords[0][0];
    s32 goldWidth = goldWords[0][1] - goldStart + 1;
    s32 dekuWidth = dekuRight - dekuLeft + 1;
    s32 width = goldWidth + 2 + dekuWidth;
    if ((dekuRight < 0) || (width > 128)) {
        sGoldNameState = -1;
        return;
    }
    s32 x = (128 - width) / 2;
    memset(sGoldNameTex, 0, sizeof(sGoldNameTex));
    for (s32 i = 0; i < goldWidth; i++, x++) {
        for (s32 y = 0; y < 16; y++) {
            SetNibble(sGoldNameTex, x, y, GetNibble(gold->ImageData, goldStart + i, y));
        }
    }
    x += 2;
    for (s32 i = 0; i < dekuWidth; i++, x++) {
        for (s32 y = 0; y < 16; y++) {
            SetNibble(sGoldNameTex, x, y, GetNibble(sNameTex, dekuLeft + i, y));
        }
    }
    sGoldNameState = 1;
}

// The randomizer's Progressive Deku Beans (RSK_SHUFFLE_DEKU_BEANS): locked until the first is found, gold after the
// second. Outside such a seed Deku Beans are always the ordinary (pink flower) kind.
bool BeansShuffled() {
    return IS_RANDO && RAND_GET_OPTION(RSK_SHUFFLE_DEKU_BEANS);
}

bool BeansGold() {
    return BeansShuffled() && Flags_GetRandomizerInf(RAND_INF_OBTAINED_GOLD_DEKU_BEANS);
}

// ---- Planting ----

bool IsFlowerAlive(PlayState* play, Actor* flower) {
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].head; actor != nullptr; actor = actor->next) {
        if ((actor == flower) && (actor->id == gMergeShipActorId_MM_Obj_Etcetera) && (actor->update != nullptr)) {
            return true;
        }
    }
    return false;
}

void PruneFlowers(PlayState* play) {
    s32 kept = 0;

    for (s32 i = 0; i < sFlowerCount; i++) {
        if (IsFlowerAlive(play, sFlowers[i].actor)) {
            sFlowers[kept++] = sFlowers[i];
        }
    }
    sFlowerCount = kept;
}

bool PlayerIsOnFlower(Player* player, Actor* flower) {
    return (player->actor.floorBgId != BGCHECK_SCENE) && (player->actor.floorBgId == ((DynaPolyActor*)flower)->bgId);
}

// Makes room for one more flower: the oldest one the Deku is not standing in goes.
void RemoveOldestFlower(PlayState* play, Player* player) {
    for (s32 i = 0; i < sFlowerCount; i++) {
        if (!PlayerIsOnFlower(player, sFlowers[i].actor)) {
            Actor_Kill(sFlowers[i].actor);
            for (s32 j = i; j < sFlowerCount - 1; j++) {
                sFlowers[j] = sFlowers[j + 1];
            }
            sFlowerCount--;
            return;
        }
    }
}

// A floor within a step of the Deku's feet at (x, z), not under water.
bool FindPlantingFloor(PlayState* play, Player* player, f32 x, f32 z, Vec3f* out) {
    Vec3f check = { x, player->actor.world.pos.y + kPlantMaxStep, z };
    CollisionPoly* poly = nullptr;
    s32 bgId = BGCHECK_SCENE;
    f32 floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &player->actor, &check);
    f32 waterY;
    WaterBox* waterBox;

    if ((poly == nullptr) || (floorY <= BGCHECK_Y_MIN) ||
        (std::fabs(floorY - player->actor.world.pos.y) > kPlantMaxStep)) {
        return false;
    }
    waterY = floorY;
    if (WaterBox_GetSurface1(play, &play->colCtx, x, z, &waterY, &waterBox) && (waterY > floorY)) {
        return false;
    }
    out->x = x;
    out->y = floorY;
    out->z = z;
    return true;
}

bool DekuCanPlant(Player* player) {
    return (player->mergeShipForm == PLAYER_FORM_DEKU) && (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
           !(player->stateFlags1 & (PLAYER_STATE1_JUMPING | PLAYER_STATE1_FREEFALL | PLAYER_STATE1_IN_WATER |
                                    PLAYER_STATE1_IN_CUTSCENE | PLAYER_STATE1_DAMAGED |
                                    PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_ON_HORSE)) &&
           // In or launching from a flower, hovering on its petals, flying, hopping on water.
           !(player->stateFlags3 & (PLAYER_STATE3_100 | PLAYER_STATE3_200 | PLAYER_STATE3_2000 |
                                    PLAYER_STATE3_200000 | PLAYER_STATE3_1000000));
}

void PlantDekuBean(PlayState* play, Player* player) {
    s16 yaw = player->actor.shape.rot.y;
    Vec3f from = player->actor.world.pos;
    Vec3f to = { player->actor.world.pos.x + Math_SinS(yaw) * kPlantDistance, player->actor.world.pos.y,
                 player->actor.world.pos.z + Math_CosS(yaw) * kPlantDistance };
    Vec3f hit;
    Vec3f pos;
    CollisionPoly* wallPoly = nullptr;
    s32 wallBgId = BGCHECK_SCENE;
    Actor* flower;

    MergeShip_Deku_EnsureActorsRegistered();
    if (!DekuCanPlant(player) || (gMergeShipActorId_MM_Obj_Etcetera < 0)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        MergeShipTest_Log("DEKUBEANS refused (form=%d bg=%04X s1=%08X s3=%08X)", player->mergeShipForm,
                          player->actor.bgCheckFlags, player->stateFlags1, player->stateFlags3);
        return;
    }

    // In front of the Deku; where a wall or a drop is in the way, at his feet.
    from.y += 10.0f;
    to.y += 10.0f;
    if (BgCheck_EntityLineTest1(&play->colCtx, &from, &to, &hit, &wallPoly, true, false, false, true, &wallBgId) ||
        !FindPlantingFloor(play, player, to.x, to.z, &pos)) {
        if (!FindPlantingFloor(play, player, player->actor.world.pos.x, player->actor.world.pos.z, &pos)) {
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            MergeShipTest_Log("DEKUBEANS no floor to plant on");
            return;
        }
    }

    // BUG-0012: one bean of the kind in use per flower.
    u8* ammo = BeansGold() ? &sGoldAmmo : &sDekuAmmo;
    if (CVAR_DEKU_BEAN_AMMO_VALUE && (*ammo == 0)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        MergeShipTest_Log("DEKUBEANS none left (%s)", BeansGold() ? "gold" : "deku");
        return;
    }

    PruneFlowers(play);
    if (sFlowerCount >= kMaxFlowers) {
        RemoveOldestFlower(play, player);
        if (sFlowerCount >= kMaxFlowers) {
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            return;
        }
    }

    flower = Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_Obj_Etcetera, pos.x, pos.y, pos.z, 0, yaw, 0,
                         MM_DEKU_FLOWER_PARAMS(BeansGold() ? MM_DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE
                                                           : MM_DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE));
    if (flower == nullptr) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return;
    }
    // No room: it stays when the Deku walks into another room, and goes with the scene.
    flower->room = -1;
    sFlowers[sFlowerCount].actor = flower;
    sFlowers[sFlowerCount].plantedFrame = play->gameplayFrames;
    sFlowerCount++;

    if (CVAR_DEKU_BEAN_AMMO_VALUE) {
        (*ammo)--;
    }
    Actor_SpawnFloorDustRing(play, flower, &pos, 20.0f, 7, 4.0f, 100, 15, false);
    MergeShipTest_Log("DEKUBEANS planted flower %p at (%.1f,%.1f,%.1f) params=0x%X count=%d", (void*)flower, pos.x,
                      pos.y, pos.z, (u16)flower->params, sFlowerCount);
}

// ---- Save ----

void InitDekuBeansSave(bool isDebug) {
    (void)isDebug;
    sSelected = false;
    sDekuAmmo = sGoldAmmo = 0;
    sDekuAmmoFilled = sGoldAmmoFilled = false;
}

void LoadDekuBeansVersion1() {
    SaveManager::Instance->LoadData("selected", sSelected, (u8) false);
    SaveManager::Instance->LoadData("dekuAmmo", sDekuAmmo, (u8)0);
    SaveManager::Instance->LoadData("goldAmmo", sGoldAmmo, (u8)0);
    SaveManager::Instance->LoadData("dekuAmmoFilled", sDekuAmmoFilled, (u8) false);
    SaveManager::Instance->LoadData("goldAmmoFilled", sGoldAmmoFilled, (u8) false);
}

void SaveDekuBeans(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("selected", sSelected);
    SaveManager::Instance->SaveData("dekuAmmo", sDekuAmmo);
    SaveManager::Instance->SaveData("goldAmmo", sGoldAmmo);
    SaveManager::Instance->SaveData("dekuAmmoFilled", sDekuAmmoFilled);
    SaveManager::Instance->SaveData("goldAmmoFilled", sGoldAmmoFilled);
}

void RegisterDekuBeansSave() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    SaveManager::Instance->AddLoadFunction("mergeShipDekuBeans", 1, LoadDekuBeansVersion1);
    SaveManager::Instance->AddSaveFunction("mergeShipDekuBeans", 1, SaveDekuBeans, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(InitDekuBeansSave);
}

static RegisterShipInitFunc sSaveInit(RegisterDekuBeansSave, {});

// ---- Hooks ----

void RegisterDekuBeans() {
    bool enabled = CVAR_DEKU_BEANS_VALUE != 0;

    // Deku Link uses Deku Beans (every other form is stopped before this by MergeShip_FormCanUseItem).
    COND_VB_SHOULD(VB_CHANGE_HELD_ITEM_AND_USE_ITEM, enabled, {
        int32_t item = va_arg(args, int32_t);

        if ((item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected() && (gPlayState != nullptr)) {
            *should = false;
            PlantDekuBean(gPlayState, GET_PLAYER(gPlayState));
        }
    });

    // Deku Beans are the Deku's at either age, unlike Ocarina of Time's child-only Magic Beans in the same slot: the
    // adult equips them too (BUG-0009).
    COND_VB_SHOULD(VB_ITEM_MEETS_AGE_REQ, enabled, {
        int item = va_arg(args, int);

        if ((item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected()) {
            *should = true;
        }
    });
    COND_VB_SHOULD(VB_SLOT_MEETS_AGE_REQ, enabled, {
        int slot = va_arg(args, int);

        if ((slot == SLOT_BEAN) && MergeShip_DekuBeans_IsSelected()) {
            *should = true;
        }
    });

    // Unlimited Deku Beans have no count on the C button or in the pause screen; as ammo (BUG-0012) they show theirs
    // (MergeShip_DekuBeans_Ammo, z_parameter.c / z_kaleido_item.c), green when full.
    COND_VB_SHOULD(VB_DRAW_AMMO_COUNT, enabled, {
        int16_t* item = va_arg(args, int16_t*);

        if ((item != nullptr) && (*item == ITEM_BEAN) && MergeShip_DekuBeans_IsSelected() &&
            !CVAR_DEKU_BEAN_AMMO_VALUE) {
            *should = false;
        }
    });
    COND_VB_SHOULD(VB_COLOR_AMMO_GREEN, enabled, {
        int item = va_arg(args, int);
        s16 ammo;

        if ((item == ITEM_BEAN) && MergeShip_DekuBeans_Ammo(&ammo) && (ammo == kBeanAmmoMax)) {
            *should = true;
        }
    });
    // Grass hides beans of the kinds owned and not full (one in kGrassFindOneIn tufts), as it hides other ammo.
    COND_VB_SHOULD(VB_GRASS_DROP_ITEM, enabled, {
        if (!CVAR_DEKU_BEAN_AMMO_VALUE || !MergeShip_DekuBeans_CanSelect() || (Rand_ZeroOne() * kGrassFindOneIn >= 1.0f)) {
            return;
        }
        u8* ammo = nullptr;
        if (sGoldAmmoFilled && (sGoldAmmo < kBeanAmmoMax) && ((sDekuAmmo >= kBeanAmmoMax) || (Rand_ZeroOne() < 0.5f))) {
            ammo = &sGoldAmmo;
        } else if (sDekuAmmoFilled && (sDekuAmmo < kBeanAmmoMax)) {
            ammo = &sDekuAmmo;
        }
        if (ammo != nullptr) {
            (*ammo)++;
            Sfx_PlaySfxCentered(NA_SE_SY_GET_ITEM);
            MergeShipTest_Log("DEKUBEANS found in grass: deku=%d gold=%d", sDekuAmmo, sGoldAmmo);
        }
    });
    // The bean slot holds the Deku Beans once they are there, Magic Beans or not (a seed shuffling them gives them
    // apart from the Magic Beans): an empty slot takes them, selected, with no Magic Beans in it (0 -- what Ocarina
    // of Time's own Magic Beans give adds to that, as to any count).
    COND_HOOK(OnPlayerUpdate, enabled, []() {
        if (DekuBeansAvailable() && (INV_CONTENT(ITEM_BEAN) == ITEM_NONE)) {
            gSaveContext.inventory.items[SLOT_BEAN] = ITEM_BEAN;
            AMMO(ITEM_BEAN) = 0;
            sSelected = true;
            MergeShipTest_Log("DEKUBEANS the empty bean slot takes the Deku Beans");
        }
    });

    // A save from when the first copy was the Magic Beans (until 2026-10-09): the copies found are Deku Beans, then
    // gold. The Magic Beans that copy gave are kept.
    COND_HOOK(OnPlayerUpdate, enabled, []() {
        if (!Flags_GetRandomizerInf(RAND_INF_OBTAINED_PROGRESSIVE_MAGIC_BEANS)) {
            return;
        }
        Flags_UnsetRandomizerInf(RAND_INF_OBTAINED_PROGRESSIVE_MAGIC_BEANS);
        if (Flags_GetRandomizerInf(RAND_INF_OBTAINED_DEKU_BEANS)) {
            Flags_SetRandomizerInf(RAND_INF_OBTAINED_GOLD_DEKU_BEANS);
        }
        Flags_SetRandomizerInf(RAND_INF_OBTAINED_DEKU_BEANS);
        MergeShipTest_Log("DEKUBEANS old three-step save: deku=1 gold=%d",
                          Flags_GetRandomizerInf(RAND_INF_OBTAINED_GOLD_DEKU_BEANS));
    });

    // The first time a kind can be used, its count is full.
    COND_HOOK(OnPlayerUpdate, enabled, []() {
        if (!CVAR_DEKU_BEAN_AMMO_VALUE || !MergeShip_DekuBeans_CanSelect()) {
            return;
        }
        if (!sDekuAmmoFilled) {
            sDekuAmmoFilled = true;
            sDekuAmmo = kBeanAmmoMax;
        }
        if (!sGoldAmmoFilled && BeansGold()) {
            sGoldAmmoFilled = true;
            sGoldAmmo = kBeanAmmoMax;
        }
    });

    // The sprouting flower sounds as a Magic Bean sprout does.
    COND_HOOK(OnPlayerUpdate, enabled, []() {
        if ((gPlayState == nullptr) || (sFlowerCount == 0)) {
            return;
        }
        PruneFlowers(gPlayState);
        for (s32 i = 0; i < sFlowerCount; i++) {
            if ((u32)(gPlayState->gameplayFrames - sFlowers[i].plantedFrame) < kSproutSoundFrames) {
                Actor_PlaySfx_Flagged(sFlowers[i].actor, NA_SE_PL_PLANT_GROW_UP - SFX_FLAG);
            }
        }
    });

    // A new scene has none of the flowers planted in the last one.
    COND_HOOK(OnSceneInit, true, [](int16_t sceneNum) {
        (void)sceneNum;
        sFlowerCount = 0;
    });
}

static RegisterShipInitFunc sHooksInit(RegisterDekuBeans, { CVAR_DEKU_BEANS_NAME });

} // namespace

extern "C" s32 MergeShip_DekuBeans_IsEnabled(void) {
    return CVAR_DEKU_BEANS_VALUE != 0;
}

// The Deku Beans themselves are there (the Deku Mask; on a seed shuffling them, the first found), whether or not
// Ocarina of Time's Magic Beans are.
// A seed shuffling them: once found, with the Deku Mask or not (only Deku Link plants them; the user, 2026-10-08:
// found, and still not selectable).
static bool DekuBeansAvailable() {
    if (!MergeShip_DekuBeans_IsEnabled()) {
        return false;
    }
    return BeansShuffled() ? Flags_GetRandomizerInf(RAND_INF_OBTAINED_DEKU_BEANS)
                           : MergeShip_HasTransformationMask(MM_FORM_DEKU);
}

extern "C" s32 MergeShip_DekuBeans_CanSelect(void) {
    return DekuBeansAvailable() && (INV_CONTENT(ITEM_BEAN) == ITEM_BEAN);
}

extern "C" s32 MergeShip_DekuBeans_Ammo(s16* out) {
    if (!CVAR_DEKU_BEAN_AMMO_VALUE || !MergeShip_DekuBeans_IsSelected()) {
        return false;
    }
    *out = BeansGold() ? sGoldAmmo : sDekuAmmo;
    return true;
}

extern "C" void MergeShip_DekuBeans_Refill(s32 gold, s32 count) {
    u8* ammo = gold ? &sGoldAmmo : &sDekuAmmo;
    *ammo = (u8)std::min<s32>(kBeanAmmoMax, std::max<s32>(0, count));
    (gold ? sGoldAmmoFilled : sDekuAmmoFilled) = true;
}

extern "C" s32 MergeShip_DekuBeans_GoldOwned(void) {
    return MergeShip_DekuBeans_CanSelect() && BeansGold();
}

extern "C" s32 MergeShip_DekuBeans_Count(s32 gold) {
    return gold ? sGoldAmmo : sDekuAmmo;
}

extern "C" s32 MergeShip_DekuBeans_AmmoMode(void) {
    return CVAR_DEKU_BEAN_AMMO_VALUE != 0;
}

// Hyrule's own Magic Beans are in the bean slot (it can hold Termina's, the shared slot's -- MergeShipOwnership.h).
static bool MagicBeansHere() {
    return (INV_CONTENT(ITEM_BEAN) == ITEM_BEAN) && MergeShip_Own_OoTOwns(ITEM_BEAN);
}

// BUG-0068: without Hyrule's Magic Beans the slot holds the Deku Beans, nothing to switch to.
extern "C" s32 MergeShip_DekuBeans_IsSelected(void) {
    return MergeShip_DekuBeans_CanSelect() && (sSelected || !MagicBeansHere());
}

extern "C" s32 MergeShip_DekuBeans_CanSwitch(void) {
    return MergeShip_DekuBeans_CanSelect() && MagicBeansHere();
}

extern "C" void MergeShip_DekuBeans_SetSelected(s32 selected) {
    sSelected = (selected != 0);
    MergeShipTest_Log("DEKUBEANS selected=%d", sSelected);
}

extern "C" void* MergeShip_DekuBeans_GetIcon(void) {
    if (BeansGold()) {
        if (sGoldIconState == 0) {
            MakeGoldIcon();
        }
        if (sGoldIconState > 0) {
            return (void*)sGoldIconTex;
        }
    }
    if (sIconState == 0) {
        MakeIcon();
    }
    return (sIconState > 0) ? (void*)sIconTex : gItemIcons[ITEM_BEAN];
}

// ---- Progressive Deku Beans: its steps and their get-item models ----

// The Magic Bean get-item model (object_gi_bean gGiBeanDL) with its colours recoloured as the icons are: each part of
// it sets its own primitive and environment colours over one shared texture. Made once, from the player's oot.o2r,
// mounted in memory beside it.
constexpr char kDekuBeanDL[] = "mergeship/dekubeans/gGiDekuBeanDL";
constexpr char kGoldBeanDL[] = "mergeship/dekubeans/gGiGoldDekuBeanDL";
alignas(8) char sDekuBeanDLPath[] = "__OTR__mergeship/dekubeans/gGiDekuBeanDL";
alignas(8) char sGoldBeanDLPath[] = "__OTR__mergeship/dekubeans/gGiGoldDekuBeanDL";
s32 sModelState = 0; // 0 not tried, 1 made, -1 failed

std::vector<char> RecolorBeanList(const std::vector<char>& src, void (*recolor)(u8*)) {
    std::vector<char> out = src;
    // Commands are 8 bytes after the 0x48-byte header; a resource hash (and G_MARKER's name) takes the next 8.
    for (size_t i = 0x48; i + 8 <= out.size(); i += 8) {
        u32 w0, w1;
        memcpy(&w0, out.data() + i, 4);
        memcpy(&w1, out.data() + i + 4, 4);
        const u32 op = w0 >> 24;
        if ((op == 0x20) || (op == 0x31) || (op == 0x32) || (op == 0x33) || (op == 0x35) || (op == 0x36)) {
            i += 8;
            continue;
        }
        if ((op == 0xFA) || (op == 0xFB)) { // G_SETPRIMCOLOR / G_SETENVCOLOR: RRGGBBAA
            u8 rgb[3] = { (u8)(w1 >> 24), (u8)(w1 >> 16), (u8)(w1 >> 8) };
            recolor(rgb);
            w1 = ((u32)rgb[0] << 24) | ((u32)rgb[1] << 16) | ((u32)rgb[2] << 8) | (w1 & 0xFF);
            memcpy(out.data() + i + 4, &w1, 4);
        }
    }
    return out;
}

bool MakeBeanModels() {
    if (sModelState != 0) {
        return sModelState > 0;
    }
    sModelState = -1;
    auto archives = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    auto file = archives->LoadFile("objects/object_gi_bean/gGiBeanDL");
    if ((file == nullptr) || !file->IsLoaded || (file->Buffer == nullptr) || (file->Buffer->size() < 0x50)) {
        return false;
    }
    std::map<std::string, std::vector<char>> files;
    files[kDekuBeanDL] = RecolorBeanList(*file->Buffer, RecolorDeku);
    files[kGoldBeanDL] = RecolorBeanList(*file->Buffer, RecolorGold);
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
        return false;
    }
    sModelState = 1;
    return true;
}

// 2 Deku Beans, 3 Gold Deku Beans (1, the Magic Beans, was a step until 2026-10-09).
extern "C" s32 MergeShip_DekuBeans_NextStep(void) {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_DEKU_BEANS) ? 3 : 2;
}

extern "C" void MergeShip_DekuBeans_GiveMagicBeans(void) {
    INV_CONTENT(ITEM_BEAN) = ITEM_BEAN;
    if (AMMO(ITEM_BEAN) < 10) {
        AMMO(ITEM_BEAN) = 10;
    }
}

extern "C" void MergeShip_DekuBeans_DrawGi(PlayState* play, GetItemEntry* getItemEntry) {
    const s32 step = MergeShip_DekuBeans_NextStep();

    if ((step == 1) || !MakeBeanModels()) {
        GetItem_Draw(play, GID_BEAN);
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_MODELVIEW | G_MTX_LOAD);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)((step == 3) ? sGoldBeanDLPath : sDekuBeanDLPath));
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void* MergeShip_DekuBeans_SwapIcon(void* texture) {
    if ((texture == gItemIcons[ITEM_BEAN]) && MergeShip_DekuBeans_IsSelected()) {
        return MergeShip_DekuBeans_GetIcon();
    }
    return texture;
}

extern "C" void* MergeShip_DekuBeans_NameTexture(PlayState* play, void* nameSegment) {
    if ((play->pauseCtx.pageIndex != PAUSE_ITEM) || (play->pauseCtx.namedItem != ITEM_BEAN) ||
        !MergeShip_DekuBeans_IsSelected()) {
        return nameSegment;
    }
    if (BeansGold()) {
        if (sGoldNameState == 0) {
            MakeGoldName();
        }
        if (sGoldNameState > 0) {
            return (void*)sGoldNameTex;
        }
    }
    if (sNameState == 0) {
        MakeName();
    }
    return (sNameState > 0) ? (void*)sNameTex : nameSegment;
}
