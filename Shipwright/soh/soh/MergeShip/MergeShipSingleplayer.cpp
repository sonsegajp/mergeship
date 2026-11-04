// MergeShip2 -- singleplayer mode: the traveling Happy Mask Salesman (MergeShipSingleplayer.h, docs/SINGLEPLAYER.md).
//
// Where he is: the day is cut into quarters (gSaveContext.totalDays * 4 + dayTime / 0x4000); each quarter rolls one
// place from a per-file seed, so the same file, day and quarter always find him in the same place and a reload does
// not move him. A child can find him in his shop (open as Ocarina of Time has it), roaming the Market or the five
// other places; an adult only in the five (the Market lies in ruins). Wherever he is not, nothing spawns; while he is
// not in his shop its Market door reads closed.
//
// What he sells: Majora's Mask's 23 masks (the 20 regular ones and Deku, Goron, Zora), three at a time. The rotation
// is a per-file shuffle of the 23; each new quarter takes the next three masks Link does not own from where the last
// offer ended, so every unowned mask is offered before any comes round again. The Adult Mask he offers only to an
// adult, once; the Fierce Deity's Mask is his gift when every mask is bought.

#include "soh/MergeShip/MergeShipSingleplayer.h"

#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ActorDB.h"
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstring>
#include <string>

extern "C" {
#include "z64.h"
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "soh/MergeShip/MergeShipForms.h"
#include "soh/MergeShip/MergeShipAdultMask.h"
#include "soh/MergeShip/MergeShipMMAudio.h"
#include "src/overlays/actors/ovl_En_Door/z_en_door.h"
#include "src/overlays/actors/ovl_MM_En_Osn_Travel/z_mm_en_osn_travel.h"
extern PlayState* gPlayState;
void MergeShipTest_Log(const char* fmt, ...);
}

namespace {

// ---- message text ------------------------------------------------------------------------------------------------

#define MS_NEWLINE "\x01"
#define MS_END "\x02"
#define MS_BOX_BREAK "\x04"
#define MS_COLOR_DEFAULT "\x05\x40"
#define MS_COLOR_RED "\x05\x41"
#define MS_COLOR_GREEN "\x05\x42"
#define MS_COLOR_BLUE "\x05\x43"
#define MS_EVENT "\x0B"
#define MS_ITEM_ICON "\x13"
#define MS_TWO_CHOICE "\x1B"
#define MS_THREE_CHOICE "\x1C"

// ---- the stock -----------------------------------------------------------------------------------------------------

struct StockEntry {
    u8 item;     // the SoH item id (ITEM_MM_MASK_*)
    s16 price;   // rupees
    const char* name;
};

// Majora's Mask's masks, as Ocarina of Time names them here (" (MM)" on all but the Deku and Fierce Deity's Masks).
const std::array<StockEntry, 23> kStock = { {
    { ITEM_MM_MASK_DEKU, 100, "Deku Mask" },
    { ITEM_MM_MASK_GORON, 200, "Goron Mask (MM)" },
    { ITEM_MM_MASK_ZORA, 200, "Zora Mask (MM)" },
    { ITEM_MM_MASK_TRUTH, 150, "Mask of Truth (MM)" },
    { ITEM_MM_MASK_KAFEIS_MASK, 60, "Kafei's Mask (MM)" },
    { ITEM_MM_MASK_ALL_NIGHT, 500, "All-Night Mask (MM)" },
    { ITEM_MM_MASK_BUNNY, 120, "Bunny Hood (MM)" },
    { ITEM_MM_MASK_KEATON, 80, "Keaton Mask (MM)" },
    { ITEM_MM_MASK_GARO, 100, "Garo's Mask (MM)" },
    { ITEM_MM_MASK_ROMANI, 90, "Romani's Mask (MM)" },
    { ITEM_MM_MASK_CIRCUS_LEADER, 90, "Circus Leader's Mask (MM)" },
    { ITEM_MM_MASK_POSTMAN, 60, "Postman's Hat (MM)" },
    { ITEM_MM_MASK_COUPLE, 250, "Couple's Mask (MM)" },
    { ITEM_MM_MASK_GREAT_FAIRY, 150, "Great Fairy's Mask (MM)" },
    { ITEM_MM_MASK_GIBDO, 120, "Gibdo Mask (MM)" },
    { ITEM_MM_MASK_DON_GERO, 70, "Don Gero's Mask (MM)" },
    { ITEM_MM_MASK_KAMARO, 80, "Kamaro's Mask (MM)" },
    { ITEM_MM_MASK_CAPTAIN, 120, "Captain's Hat (MM)" },
    { ITEM_MM_MASK_STONE, 100, "Stone Mask (MM)" },
    { ITEM_MM_MASK_BREMEN, 90, "Bremen Mask (MM)" },
    { ITEM_MM_MASK_BLAST, 100, "Blast Mask (MM)" },
    { ITEM_MM_MASK_SCENTS, 140, "Mask of Scents (MM)" },
    { ITEM_MM_MASK_GIANT, 300, "Giant's Mask (MM)" },
} };
constexpr s32 kStockCount = (s32)kStock.size();
constexpr s16 kAdultMaskPrice = 250;
constexpr u8 kNoOffer = 0xFF;

// ---- save data -------------------------------------------------------------------------------------------------------

struct SpSave {
    u32 seed = 0;             // the file's own random seed (places and the rotation's shuffle); 0: not drawn yet
    u8 adultMaskBought = 0;
    u8 fierceDeityGiven = 0;
    s32 offerSlot = -1;       // the time slot the current offer belongs to
    u8 rotation = 0;          // where the next offer starts in the shuffle
    u8 offer[3] = { kNoOffer, kNoOffer, kNoOffer };
    u32 clock = 0; // his schedule's clock: frames of play on this file
    // The post-game (phases 2 and 3).
    u8 ootBeaten = 0;     // Zelda's farewell is over: no credits, child Link in the Temple of Time
    u8 endingArrival = 0; // ... and Navi has yet to speak there (the file is saved as she does)
    u8 doorDone = 0;      // he met Link in the Lost Woods and walked away: Termina lies beyond
};
SpSave sSave;
s32 sSoldFor = 0; // what he paid for the masks at the Lost Woods (the text names it)
s32 sArrivalWait = 0;
bool sNaviCall = false; // Navi has yet to speak her first words after the ending

// Conversation state (not saved).
u8 sChosen = kNoOffer;       // stock index the confirm/bought texts name (kNoOffer: the Adult Mask)
bool sChoseAdult = false;
u8 sShown[3] = { kNoOffer, kNoOffer, kNoOffer }; // the offer as the menu lists it (owned ones left out)
s32 sShownCount = 0;

u32 Hash(u32 a, u32 b) {
    u32 h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

// The file's own seed: from when the file was made and its name, so the same file always rolls the same places (also
// before its first save) and a reload never moves him.
u32 Seed() {
    if (sSave.seed == 0) {
        u32 created = (u32)gSaveContext.ship.stats.fileCreatedAt ^ (u32)(gSaveContext.ship.stats.fileCreatedAt >> 32);
        u32 name = 0;
        for (s32 i = 0; i < 8; i++) {
            name = name * 31 + gSaveContext.playerName[i];
        }
        sSave.seed = Hash(created, name ^ 0x5A1E5u) | 1u;
    }
    return sSave.seed;
}

bool Owned(s32 stock) {
    return MergeShip_HasMMMaskItem(kStock[stock].item) != 0;
}

// The file's shuffle of the stock (Fisher-Yates from the seed).
std::array<u8, kStockCount> Shuffle() {
    std::array<u8, kStockCount> order;
    for (s32 i = 0; i < kStockCount; i++) {
        order[i] = (u8)i;
    }
    for (s32 i = kStockCount - 1; i > 0; i--) {
        s32 j = (s32)(Hash(Seed(), 0x5A1E5u + i) % (u32)(i + 1));
        std::swap(order[i], order[j]);
    }
    return order;
}

// A new quarter: the next three unowned masks of the rotation, from where the last offer ended.
s32 CurrentStay();

void RollOffer() {
    const auto order = Shuffle();
    s32 taken = 0;
    s32 pos = sSave.rotation % kStockCount;

    for (s32 k = 0; k < 3; k++) {
        sSave.offer[k] = kNoOffer;
    }
    for (s32 step = 0; (step < kStockCount) && (taken < 3); step++) {
        u8 stock = order[(pos + step) % kStockCount];
        if (!Owned(stock)) {
            sSave.offer[taken++] = stock;
            sSave.rotation = (u8)((pos + step + 1) % kStockCount);
        }
    }
    sSave.offerSlot = CurrentStay();
    MergeShipTest_Log("SPSHOP offer slot=%d: %d %d %d (rotation %d)", sSave.offerSlot, sSave.offer[0], sSave.offer[1],
                      sSave.offer[2], sSave.rotation);
}

// ---- the schedule ----------------------------------------------------------------------------------------------------
//
// His own clock (sSave.clock: frames of play on this file) runs a timeline: he stays at a place kStayFrames, then walks
// to the next (a random place of the file's seed, never the same one twice running) along the real route -- the areas
// between, each crossed on paths planned from the scene's own collision (tools/sp_navplan.py: walkable floor, no water
// over it, small steps, no wall between) -- at his walking pace. Where the timeline puts him now is where he is: an
// area Link enters finds him roaming it, walking through it, arriving or leaving. Where the ground cannot be walked
// (Zora's River to Zora's Domain) he does as MM's salesman does when he leaves: he fades away, and fades in at the
// far side.

enum SpScene { SC_FIELD, SC_MARKET_ENT, SC_MARKET, SC_KAK, SC_DMT, SC_ZR, SC_ZD, SC_SHOP, SC_COUNT };

struct SpPortal {
    s8 to;                   // the scene the portal leads to
    u8 fade;                 // he fades out here (and in, coming the other way)
    std::vector<Vec3f> path; // from the scene's hub to the portal
};

struct SpSceneInfo {
    s16 scene;
    s16 sceneAlt; // the same place at night (or -1)
    Vec3f hub;    // where he roams when he stays here
    f32 radius;
    std::vector<SpPortal> portals;
};

constexpr u32 kStayFrames = 4800; // 4 minutes of play at a place
constexpr f32 kWalkSpeed = 2.72f; // ovl_MM_En_Osn_Travel OSNT_WALK_SPEED

const std::vector<SpSceneInfo>& Scenes() {
    // Paths from tools/sp_navplan.py on each scene's collision (oot.o2r).
    static const std::vector<SpSceneInfo> sScenes = {
        /* SC_FIELD */
        { SCENE_HYRULE_FIELD,
          -1,
          { 1900, 0, 850 },
          400,
          { { SC_MARKET_ENT, 0, { { 1900, 0, 850 }, { 150, 0, 1400 } } },
            { SC_KAK, 0, { { 1900, 0, 850 }, { 2150, -14, 700 }, { 3350, -5, 550 }, { 3700, 207, 300 } } },
            { SC_ZR,
              0,
              { { 1900, 0, 850 },
                { 2100, -20, 800 },
                { 3650, -33, 1600 },
                { 4150, -58, 1800 },
                { 4950, -50, 2600 },
                { 5500, -26, 3250 },
                { 5950, -20, 3700 } } } } },
        /* SC_MARKET_ENT */
        { SCENE_MARKET_ENTRANCE_DAY,
          SCENE_MARKET_ENTRANCE_NIGHT,
          { 8, 0, 2530 },
          0,
          { { SC_FIELD, 0, { { 8, 0, 2530 }, { 5, 0, 2906 } } }, { SC_MARKET, 0, { { 8, 0, 2530 }, { 11, 0, 2158 } } } } },
        /* SC_MARKET */
        { SCENE_MARKET_DAY,
          SCENE_MARKET_NIGHT,
          { 140, 0, 120 },
          220,
          { { SC_MARKET_ENT, 0, { { 140, 0, 120 }, { 0, 0, 720 } } },
            { SC_SHOP, 0, { { 140, 0, 120 }, { 280, 0, -480 }, { 320, 0, -560 } } } } },
        /* SC_KAK */
        { SCENE_KAKARIKO_VILLAGE,
          -1,
          { -1020, 6, 390 },
          260,
          { { SC_FIELD, 0, { { -1020, 6, 390 }, { -2130, 137, 990 }, { -2640, 138, 1050 } } },
            { SC_DMT,
              0,
              { { -1020, 6, 390 },
                { -300, 0, 60 },
                { -300, 12, 30 },
                { -420, 72, -30 },
                { -630, 177, -30 },
                { -660, 192, -60 },
                { -630, 200, -120 },
                { -210, 200, -300 },
                { -150, 251, -420 },
                { -120, 298, -510 },
                { 0, 407, -1410 },
                { -60, 493, -1740 } } } } },
        /* SC_DMT */
        { SCENE_DEATH_MOUNTAIN_TRAIL,
          -1,
          { -1530, 174, 2130 },
          150,
          { { SC_KAK, 0, { { -1530, 174, 2130 }, { -1350, 120, 2220 } } } } },
        /* SC_ZR */
        { SCENE_ZORAS_RIVER,
          -1,
          { -1290, 100, -30 },
          140,
          { { SC_FIELD, 0, { { -1290, 100, -30 }, { -1410, 100, 120 }, { -1500, -20, 1500 } } },
            { SC_ZD, 1, { { -1290, 100, -30 }, { -1200, 100, -120 }, { -720, 100, -330 }, { -690, 100, -360 } } } } },
        /* SC_ZD */
        { SCENE_ZORAS_DOMAIN, -1, { -875, 210, -150 }, 110, { { SC_ZR, 1, { { -875, 210, -150 }, { -1100, 210, -150 } } } } },
        /* SC_SHOP: inside his shop, not seen */
        { SCENE_HAPPY_MASK_SHOP, -1, { 0, 0, 0 }, 0, { { SC_MARKET, 0, {} } } },
    };
    return sScenes;
}

const s8 kPlaceScene[MS_SALESMAN_PLACE_MAX] = { SC_SHOP, SC_MARKET, SC_KAK, SC_DMT, SC_ZR, SC_ZD, SC_FIELD };

s32 SceneOf(s16 sceneNum) {
    const auto& sc = Scenes();
    for (s32 i = 0; i < SC_COUNT; i++) {
        if ((sc[i].scene == sceneNum) || (sc[i].sceneAlt == sceneNum)) {
            return i;
        }
    }
    return -1;
}

s32 PortalTo(s32 from, s32 to) {
    const auto& p = Scenes()[from].portals;
    for (s32 i = 0; i < (s32)p.size(); i++) {
        if (p[i].to == to) {
            return i;
        }
    }
    return -1;
}

struct SpLeg {
    s8 scene = 0;
    std::vector<Vec3f> pts;
    f32 length = 0;
    u8 fadeIn = 0;
    u8 fadeOut = 0;
    u8 endAtHub = 0;
};

f32 SegLength(const Vec3f& a, const Vec3f& b) {
    return sqrtf(SQ(b.x - a.x) + SQ(b.y - a.y) + SQ(b.z - a.z));
}

f32 PathLength(const std::vector<Vec3f>& pts) {
    f32 len = 0;
    for (size_t i = 1; i < pts.size(); i++) {
        len += SegLength(pts[i - 1], pts[i]);
    }
    return len;
}

// The areas from one place's scene to another's (breadth first over the portals) and the walk through each.
std::vector<SpLeg> BuildRoute(s32 fromPlace, s32 toPlace) {
    const auto& sc = Scenes();
    s32 a = kPlaceScene[fromPlace];
    s32 b = kPlaceScene[toPlace];
    std::vector<s32> prev(SC_COUNT, -2);
    std::vector<s32> queue = { a };
    prev[a] = -1;
    for (size_t qi = 0; qi < queue.size(); qi++) {
        for (const auto& portal : sc[queue[qi]].portals) {
            if (prev[portal.to] == -2) {
                prev[portal.to] = queue[qi];
                queue.push_back(portal.to);
            }
        }
    }
    std::vector<s32> chain;
    for (s32 s = b; s != -1; s = prev[s]) {
        chain.insert(chain.begin(), s);
    }
    std::vector<SpLeg> legs;
    for (size_t i = 0; i < chain.size(); i++) {
        SpLeg leg;
        leg.scene = (s8)chain[i];
        const SpSceneInfo& info = sc[chain[i]];
        // In: from the portal he came through to the hub (or from the hub, where he stayed).
        if (i > 0) {
            const SpPortal& in = info.portals[PortalTo(chain[i], chain[i - 1])];
            leg.pts.assign(in.path.rbegin(), in.path.rend());
            leg.fadeIn = in.fade;
        } else {
            leg.pts.push_back(info.hub);
        }
        // Out: from the hub to the portal he leaves by (or he stays at the hub).
        if (i + 1 < chain.size()) {
            const SpPortal& out = info.portals[PortalTo(chain[i], chain[i + 1])];
            for (size_t k = 1; k < out.path.size(); k++) {
                leg.pts.push_back(out.path[k]);
            }
            leg.fadeOut = out.fade;
        } else {
            leg.endAtHub = 1;
        }
        if (chain[i] == SC_SHOP) {
            leg.pts.clear();
        }
        leg.length = PathLength(leg.pts);
        legs.push_back(leg);
    }
    return legs;
}

// The places, in the file's order: a random one each time, never the one he just left. A child can find him in his
// shop and anywhere; an adult only beyond the Market (it lies in ruins).
s32 PlaceAt(s32 k) {
    static const s32 kChild[] = { MS_SALESMAN_SHOP,          MS_SALESMAN_MARKET,       MS_SALESMAN_KAKARIKO,
                                  MS_SALESMAN_DEATH_MOUNTAIN, MS_SALESMAN_ZORAS_RIVER, MS_SALESMAN_ZORAS_DOMAIN,
                                  MS_SALESMAN_HYRULE_FIELD };
    static const s32 kAdult[] = { MS_SALESMAN_KAKARIKO, MS_SALESMAN_DEATH_MOUNTAIN, MS_SALESMAN_ZORAS_RIVER,
                                  MS_SALESMAN_ZORAS_DOMAIN, MS_SALESMAN_HYRULE_FIELD };
    const s32* list = LINK_IS_ADULT ? kAdult : kChild;
    u32 count = LINK_IS_ADULT ? ARRAY_COUNT(kAdult) : ARRAY_COUNT(kChild);
    // Each place depends on the one before (never the same twice running), so k is reached from 0 -- cached.
    static u32 sSeed = 0;
    static s32 sAge = -1;
    static std::vector<s32> sPlaces;
    if ((sSeed != Seed()) || (sAge != gSaveContext.linkAge)) {
        sSeed = Seed();
        sAge = gSaveContext.linkAge;
        sPlaces.clear();
    }
    while ((s32)sPlaces.size() <= k) {
        s32 i = (s32)sPlaces.size();
        s32 prev = sPlaces.empty() ? -1 : sPlaces.back();
        u32 h = Hash(Seed(), (u32)i * 7919u + 13u);
        s32 place = list[h % count];
        if (place == prev) {
            place = list[(h % count + 1 + (h / count) % (count - 1)) % count];
        }
        // A child's Hyrule: every third stay he keeps his own shop in the Market (its door open); the others are
        // anywhere else. (The adult's Market lies in ruins: no shop then.)
        if (!LINK_IS_ADULT) {
            if ((i % 3) == 0) {
                place = MS_SALESMAN_SHOP;
            } else if (place == MS_SALESMAN_SHOP) {
                place = list[1 + (h / count) % (count - 1)];
                if (place == prev) {
                    place = list[1 + ((h / count) + 1) % (count - 1)];
                }
            }
        }
        sPlaces.push_back(place);
    }
    return sPlaces[k];
}

struct SpNow {
    s32 stay = 0;  // the stay he is at, or has left
    s32 place = 0; // that stay's place
    s32 next = 0;  // the next stay's place
    bool traveling = false;
    std::vector<SpLeg> legs;
    s32 leg = 0;    // traveling: the leg he is on
    f32 offset = 0; // traveling: how far along it
    u32 stayEnd = 0;
};

s32 DebugPlace() {
    s32 forced = CVarGetInteger("gEnhancements.MergeShip.SPDebugPlace", 0);
    return ((forced > 0) && (forced <= MS_SALESMAN_PLACE_MAX)) ? forced - 1 : -1;
}

SpNow Evaluate(u32 clock) {
    SpNow now;
    if (DebugPlace() >= 0) { // tests: he stays there
        now.place = DebugPlace();
        now.stayEnd = 0xFFFFFFFFu;
        return now;
    }
    // Where the last evaluation got to (the timeline only moves on).
    static u32 sCacheSeed = 0;
    static s32 sCacheAge = -1;
    static s32 sCacheStay = 0;
    static u32 sCacheStart = 0;
    s32 k = 0;
    u32 t = 0;
    if ((sCacheSeed == Seed()) && (sCacheAge == gSaveContext.linkAge) && (clock >= sCacheStart)) {
        k = sCacheStay;
        t = sCacheStart;
    }
    for (s32 guard = 0; guard < 200000; guard++) {
        s32 place = PlaceAt(k);
        u32 stayEnd = t + kStayFrames;
        if (clock < stayEnd) {
            now.stay = k;
            now.place = place;
            now.stayEnd = stayEnd;
            break;
        }
        s32 next = PlaceAt(k + 1);
        std::vector<SpLeg> legs = BuildRoute(place, next);
        f32 total = 0;
        for (const auto& leg : legs) {
            total += leg.length;
        }
        u32 travelEnd = stayEnd + (u32)(total / kWalkSpeed);
        if (clock < travelEnd) {
            f32 walked = (clock - stayEnd) * kWalkSpeed;
            now.stay = k;
            now.place = place;
            now.next = next;
            now.traveling = true;
            now.legs = legs;
            for (s32 i = 0; i < (s32)legs.size(); i++) {
                if ((walked < legs[i].length) || (i == (s32)legs.size() - 1)) {
                    now.leg = i;
                    now.offset = walked;
                    break;
                }
                walked -= legs[i].length;
            }
            break;
        }
        k++;
        t = travelEnd;
        sCacheSeed = Seed();
        sCacheAge = gSaveContext.linkAge;
        sCacheStay = k;
        sCacheStart = t;
    }
    return now;
}

s32 CurrentStay() {
    return Evaluate(sSave.clock).stay;
}

// A point `offset` along a path, and the index of the next point after it.
Vec3f PointAlong(const std::vector<Vec3f>& pts, f32 offset, size_t* nextIndex) {
    for (size_t i = 1; i < pts.size(); i++) {
        f32 seg = SegLength(pts[i - 1], pts[i]);
        if ((offset <= seg) || (i == pts.size() - 1)) {
            f32 t = (seg > 0) ? CLAMP(offset / seg, 0.0f, 1.0f) : 1.0f;
            *nextIndex = i;
            return { pts[i - 1].x + (pts[i].x - pts[i - 1].x) * t, pts[i - 1].y + (pts[i].y - pts[i - 1].y) * t,
                     pts[i - 1].z + (pts[i].z - pts[i - 1].z) * t };
        }
        offset -= seg;
    }
    *nextIndex = pts.size();
    return pts.empty() ? Vec3f{ 0, 0, 0 } : pts.back();
}

void FillHub(MsSalesmanPlan* plan, s32 scene) {
    const SpSceneInfo& info = Scenes()[scene];
    plan->hub[0] = info.hub.x;
    plan->hub[1] = info.hub.y;
    plan->hub[2] = info.hub.z;
    plan->radius = info.radius;
}

void FillTravelPlan(MsSalesmanPlan* plan, const SpLeg& leg, f32 offset, s32 stay, s32 endStay) {
    size_t next = 0;
    Vec3f at = PointAlong(leg.pts, offset, &next);
    plan->mode = MS_SALESMAN_PLAN_TRAVEL;
    plan->wayCount = 0;
    plan->start[0] = at.x;
    plan->start[1] = at.y;
    plan->start[2] = at.z;
    for (size_t i = next; (i < leg.pts.size()) && (plan->wayCount < MS_SALESMAN_PLAN_MAX_WAY); i++) {
        plan->way[plan->wayCount][0] = leg.pts[i].x;
        plan->way[plan->wayCount][1] = leg.pts[i].y;
        plan->way[plan->wayCount][2] = leg.pts[i].z;
        plan->wayCount++;
    }
    plan->fadeIn = leg.fadeIn && (offset < 30.0f);
    plan->fadeOut = leg.fadeOut;
    plan->endAtHub = leg.endAtHub;
    FillHub(plan, leg.scene);
    plan->stay = leg.endAtHub ? endStay : stay;
}

void FillStayPlan(MsSalesmanPlan* plan, s32 scene, s32 stay) {
    const SpSceneInfo& info = Scenes()[scene];
    f32 angle = Rand_ZeroOne() * 2.0f * M_PI;
    f32 dist = info.radius * Rand_ZeroOne() * 0.6f;
    plan->mode = MS_SALESMAN_PLAN_STAY;
    plan->wayCount = 0;
    plan->start[0] = info.hub.x + sinf(angle) * dist;
    plan->start[1] = info.hub.y;
    plan->start[2] = info.hub.z + cosf(angle) * dist;
    FillHub(plan, scene);
    plan->fadeIn = plan->fadeOut = plan->endAtHub = 0;
    plan->stay = stay;
}

// ---- texts -----------------------------------------------------------------------------------------------------------

std::string Icon(u8 item) {
    return std::string(MS_ITEM_ICON) + (char)item;
}

// The Adult Mask is the Child Mask while Link is the adult (MergeShipAdultMask.h) -- which he always is when he buys it.
const char* AdultMaskName() {
    return MergeShip_AdultMask_IsChildMask() ? "Child Mask" : "Adult Mask";
}

std::string ChosenName() {
    return sChoseAdult ? AdultMaskName() : ((sChosen < kStockCount) ? kStock[sChosen].name : "?");
}

s16 ChosenPrice() {
    return sChoseAdult ? kAdultMaskPrice : ((sChosen < kStockCount) ? kStock[sChosen].price : 0);
}

u8 ChosenItem() {
    return sChoseAdult ? ITEM_MM_MASK_ADULT : ((sChosen < kStockCount) ? kStock[sChosen].item : ITEM_NONE);
}

void LoadText(const std::string& text, bool* loadFromMessageTable, TextBoxType type = TEXTBOX_TYPE_BLACK) {
    CustomMessage msg(text, text, text, type, TEXTBOX_POS_BOTTOM);
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void BuildText(uint16_t* textId, bool* loadFromMessageTable) {
    std::string t;

    switch (*textId) {
        case MERGESHIP_TEXT_SP_GREET:
            t = "Oh! Ohhh... A customer!" MS_NEWLINE "I travel the land, gathering masks" MS_NEWLINE
                "from a world not your own..." MS_BOX_BREAK "Perhaps one of them will catch your" MS_NEWLINE
                "eye? Hee hee..." MS_EVENT MS_END;
            break;
        case MERGESHIP_TEXT_SP_MENU:
            t = "These are the masks I carry today." MS_NEWLINE;
            if (sShownCount >= 3) {
                t += MS_THREE_CHOICE;
            } else if (sShownCount == 2) {
                // A two-choice box puts its choices on the third and fourth lines.
                t += "Which one catches your eye?" MS_NEWLINE MS_TWO_CHOICE;
            }
            for (s32 i = 0; i < sShownCount; i++) {
                t += std::string(MS_COLOR_GREEN) + kStock[sShown[i]].name + MS_COLOR_DEFAULT;
                if (i + 1 < sShownCount) {
                    t += MS_NEWLINE;
                }
            }
            t += MS_END;
            break;
        case MERGESHIP_TEXT_SP_CONFIRM:
            t = std::string("The ") + MS_COLOR_RED + ChosenName() + MS_COLOR_DEFAULT "..." MS_NEWLINE "For " +
                MS_COLOR_RED + std::to_string(ChosenPrice()) + " Rupees" MS_COLOR_DEFAULT ", it is yours." MS_NEWLINE
                MS_TWO_CHOICE MS_COLOR_GREEN "Buy it" MS_NEWLINE "No thanks" MS_COLOR_DEFAULT MS_END;
            break;
        case MERGESHIP_TEXT_SP_BOUGHT:
            t = Icon(ChosenItem()) + "You got the " MS_COLOR_RED + ChosenName() + MS_COLOR_DEFAULT "!" MS_NEWLINE
                "Treat it well... Masks have" MS_NEWLINE "feelings too, you know." MS_EVENT MS_END;
            break;
        case MERGESHIP_TEXT_SP_NO_RUPEES:
            t = "Oh... It seems you haven't" MS_NEWLINE "enough Rupees." MS_NEWLINE
                "What a pity... What a pity..." MS_EVENT MS_END;
            break;
        case MERGESHIP_TEXT_SP_FAREWELL:
            t = "Do come again... I shall be" MS_NEWLINE "wandering, here and there," MS_NEWLINE
                "as the days go by." MS_END;
            break;
        case MERGESHIP_TEXT_SP_SOLD_OUT:
            t = "You have every mask I carry..." MS_NEWLINE "I have nothing more to sell you." MS_NEWLINE
                "Hee hee..." MS_END;
            break;
        case MERGESHIP_TEXT_SP_ADULT:
            t = std::string("My, my... You have grown up,") + MS_NEWLINE "haven't you? Children grow so" MS_NEWLINE
                "quickly..." MS_BOX_BREAK "This mask changes as you do." MS_NEWLINE "Grown up, it is the " MS_COLOR_RED
                "Child Mask" MS_COLOR_DEFAULT ";" MS_NEWLINE "as a child, the " MS_COLOR_RED "Adult Mask" MS_COLOR_DEFAULT
                "." MS_NEWLINE "Wear it to become the other." MS_BOX_BREAK
                "For one who wishes to be young" MS_NEWLINE "again: the " MS_COLOR_RED +
                AdultMaskName() + MS_COLOR_DEFAULT ", " MS_COLOR_RED + std::to_string(kAdultMaskPrice) +
                " Rupees" MS_COLOR_DEFAULT "." MS_NEWLINE MS_TWO_CHOICE MS_COLOR_GREEN "Buy it" MS_NEWLINE
                "No thanks" MS_COLOR_DEFAULT MS_END;
            break;
        case MERGESHIP_TEXT_SP_FD_GIFT:
            t = "You have gathered every one of my" MS_NEWLINE "masks... Then this one is yours." MS_BOX_BREAK +
                Icon(ITEM_MM_MASK_FIERCE_DEITY) + "You got the " MS_COLOR_RED "Fierce Deity's Mask" MS_COLOR_DEFAULT
                "!" MS_NEWLINE "Its power is... immense." MS_EVENT MS_END;
            break;
        case MERGESHIP_TEXT_SP_DOOR_ASK:
            t = "Oh! It's you... The boy who saved" MS_NEWLINE "this land. I've been waiting." MS_BOX_BREAK
                "Deep in these woods lies a path to" MS_NEWLINE "another world... the world my masks" MS_NEWLINE
                "came from." MS_BOX_BREAK "Will you bring the masks you" MS_NEWLINE "bought from me along with you?"
                MS_NEWLINE MS_TWO_CHOICE MS_COLOR_GREEN "Bring them" MS_NEWLINE "Sell them back" MS_COLOR_DEFAULT
                MS_END;
            break;
        case MERGESHIP_TEXT_SP_DOOR_BRING:
            t = "Then take them... and take good" MS_NEWLINE "care of them." MS_BOX_BREAK
                "I wonder what kind of fate awaits" MS_NEWLINE "you there... Hee hee..." MS_NEWLINE
                "We shall meet again, I am sure." MS_END;
            break;
        case MERGESHIP_TEXT_SP_DOOR_SOLD:
            t = "Very well... I shall take them back." MS_NEWLINE "Here, for your trouble: " MS_COLOR_RED +
                std::to_string(sSoldFor) + " Rupees" MS_COLOR_DEFAULT "." MS_BOX_BREAK
                "That world is no place for one" MS_NEWLINE "with empty hands, though..." MS_NEWLINE
                "We shall meet again, I am sure." MS_END;
            break;
        case MERGESHIP_TEXT_SP_DOOR_NONE:
            t = "Oh! It's you... The boy who saved" MS_NEWLINE "this land." MS_BOX_BREAK
                "Deep in these woods lies a path to" MS_NEWLINE "another world... the world my masks" MS_NEWLINE
                "came from. You carry none of them..." MS_BOX_BREAK "No matter. I wonder what kind of" MS_NEWLINE
                "fate awaits you there... Hee hee..." MS_NEWLINE "We shall meet again, I am sure." MS_END;
            break;
        case MERGESHIP_TEXT_SP_NAVI_END:
            t = "Link... We're back. Back in our own" MS_NEWLINE "time, and Hyrule is at peace." MS_BOX_BREAK
                "But... I'm not going anywhere!" MS_NEWLINE "Our adventure isn't over yet!" MS_BOX_BREAK
                "Hey, why don't we go to the " MS_COLOR_GREEN "Lost" MS_NEWLINE "Woods" MS_COLOR_DEFAULT
                "? I have a feeling something" MS_NEWLINE "is waiting for us there..." MS_END;
            LoadText(t, loadFromMessageTable, TEXTBOX_TYPE_BLUE);
            return;
        case MERGESHIP_TEXT_SP_NAVI_HINT:
            t = "Let's go to the " MS_COLOR_GREEN "Lost Woods" MS_COLOR_DEFAULT "!" MS_NEWLINE
                "I have a feeling something is" MS_NEWLINE "waiting for us there..." MS_END;
            LoadText(t, loadFromMessageTable, TEXTBOX_TYPE_BLUE);
            return;
        case MERGESHIP_TEXT_SP_SHOP_CLOSED:
            if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_MARKET_DAY)) {
                return;
            }
            t = "Happy Mask Shop" MS_NEWLINE "Closed -- the owner is out" MS_NEWLINE "traveling. Back in time..." MS_END;
            break;
        default:
            return;
    }
    LoadText(t, loadFromMessageTable);
}

// ---- spawning and the shop door --------------------------------------------------------------------------------------

// He left this scene through a portal on this stay (or arrived and moved on): not spawned again for it.
s32 sLeftStay = -1;
s32 sLeftScene = -1;

const char* const kPlaceNames[MS_SALESMAN_PLACE_MAX] = { "his shop (Castle Town Market)", "Castle Town Market",
                                                         "Kakariko Village", "Death Mountain Trail", "Zora's River",
                                                         "Zora's Domain", "Hyrule Field" };

Actor* FindSalesman(PlayState* play) {
    if (gMergeShipActorId_MM_En_Osn_Travel < 0) {
        return nullptr;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_NPC].head; a != nullptr; a = a->next) {
        if (a->id == gMergeShipActorId_MM_En_Osn_Travel) {
            return a;
        }
    }
    return nullptr;
}

// Where the timeline has him now, if that is this scene: the plan the actor starts with. False when he is elsewhere.
bool PlanHere(PlayState* play, MsSalesmanPlan* plan, SpNow* outNow) {
    s32 here = SceneOf(play->sceneNum);
    SpNow now = Evaluate(sSave.clock);
    if (outNow != nullptr) {
        *outNow = now;
    }
    if (here < 0) {
        return false;
    }
    if (!now.traveling) {
        if ((kPlaceScene[now.place] != here) || (here == SC_SHOP)) {
            return false;
        }
        FillStayPlan(plan, here, now.stay);
        return true;
    }
    const SpLeg& leg = now.legs[now.leg];
    if ((leg.scene != here) || leg.pts.empty()) {
        return false;
    }
    FillTravelPlan(plan, leg, now.offset, now.stay, now.stay + 1);
    return true;
}

void SpawnFromPlan(PlayState* play, const MsSalesmanPlan& plan) {
    MergeShip_SingleplayerActors_Register();
    if (gMergeShipActorId_MM_En_Osn_Travel < 0) {
        return;
    }
    gMsSalesmanPlan = plan;
    s16 yaw = 0;
    if (plan.wayCount > 0) {
        yaw = (s16)(Math_FAtan2F(plan.way[0][0] - plan.start[0], plan.way[0][2] - plan.start[2]) * (0x8000 / M_PI));
    }
    Actor* osn = Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Osn_Travel, plan.start[0],
                             plan.start[1] + 40.0f, plan.start[2], 0, yaw, 0, 0);
    if (osn != nullptr) {
        osn->room = -1;
    }
    MergeShipTest_Log("SPSALESMAN spawned in scene %d: %s (stay %d) at (%.0f,%.0f,%.0f), %d waypoints%s%s -> %p",
                      play->sceneNum, (plan.mode == MS_SALESMAN_PLAN_STAY) ? "staying" : "traveling", plan.stay,
                      plan.start[0], plan.start[1], plan.start[2], plan.wayCount, plan.fadeIn ? ", fading in" : "",
                      plan.endAtHub ? ", arriving" : "", (void*)osn);
}

void LogWhere() {
    SpNow now = Evaluate(sSave.clock);
    if (now.traveling) {
        SPDLOG_INFO("MergeShip singleplayer: the Happy Mask Salesman is walking from {} to {} (scene {} of the way, "
                    "clock {})",
                    kPlaceNames[now.place], kPlaceNames[now.next], Scenes()[now.legs[now.leg].scene].scene,
                    sSave.clock);
    } else {
        SPDLOG_INFO("MergeShip singleplayer: the Happy Mask Salesman is in {} (stay {}, {} s left, clock {})",
                    kPlaceNames[now.place], now.stay, (now.stayEnd - sSave.clock) / 20, sSave.clock);
    }
}

// The post-game Lost Woods: he waits ahead of where Link comes in, until he has spoken and walked away.
void SpawnDoorSalesman(PlayState* play) {
    if ((play->sceneNum != SCENE_LOST_WOODS) || !MergeShip_Singleplayer_DoorPending() ||
        (play->linkActorEntry == nullptr)) {
        return;
    }
    const ActorEntry* entry = play->linkActorEntry;
    const s16 yaw = entry->rot.y;
    MsSalesmanPlan plan = {};
    plan.mode = MS_SALESMAN_PLAN_DOOR;
    plan.start[0] = entry->pos.x + Math_SinS(yaw) * 220.0f;
    plan.start[1] = entry->pos.y;
    plan.start[2] = entry->pos.z + Math_CosS(yaw) * 220.0f;
    plan.hub[0] = plan.start[0];
    plan.hub[1] = plan.start[1];
    plan.hub[2] = plan.start[2];
    plan.radius = 40.0f;
    plan.stay = -1;
    MergeShip_SingleplayerActors_Register();
    if (gMergeShipActorId_MM_En_Osn_Travel < 0) {
        return;
    }
    gMsSalesmanPlan = plan;
    Actor* osn = Actor_Spawn(&play->actorCtx, play, gMergeShipActorId_MM_En_Osn_Travel, plan.start[0],
                             plan.start[1] + 40.0f, plan.start[2], 0, (s16)(yaw + 0x8000), 0, 0);
    if (osn != nullptr) {
        osn->room = -1;
    }
    MergeShipTest_Log("SPDOOR salesman waits in the Lost Woods (spawn %d) at (%.0f,%.0f,%.0f) -> %p", play->curSpawn,
                      plan.start[0], plan.start[1], plan.start[2], (void*)osn);
}

// After the ending: once child Link stands in the Temple of Time, Navi flies to him and speaks (and the file is saved,
// so the post-game is kept).
void EndingArrival(PlayState* play) {
    if (!sSave.endingArrival || (play->sceneNum != SCENE_TEMPLE_OF_TIME)) {
        sArrivalWait = 0;
        return;
    }
    Player* player = GET_PLAYER(play);
    if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || (play->transitionMode != TRANS_MODE_OFF) ||
        (play->msgCtx.msgMode != MSGMODE_NONE) || Player_InCsMode(play) || (player->naviActor == nullptr)) {
        return;
    }
    if (++sArrivalWait < 30) {
        return;
    }
    if (sArrivalWait == 30) {
        sSave.endingArrival = 0;
        sNaviCall = true;
        Play_PerformSave(play);
        MergeShipTest_Log("SPENDING child Link in the Temple of Time: saved, Navi speaks");
    }
}

void OnSceneSpawn() {
    PlayState* play = gPlayState;
    MsSalesmanPlan plan;

    if ((play == nullptr) || !MergeShip_Singleplayer_Active()) {
        return;
    }
    SpawnDoorSalesman(play);
    sLeftStay = sLeftScene = -1;
    LogWhere();
    if (PlanHere(play, &plan, nullptr)) {
        SpawnFromPlan(play, plan);
    }
}

// Every frame of play: his clock runs; every second, if the timeline brings him into this scene (arriving through a
// portal, or the scene loaded before he got here), he appears.
void OnFrame() {
    PlayState* play = gPlayState;

    if ((play == nullptr) || !MergeShip_Singleplayer_Active() || (GET_PLAYER(play) == nullptr)) {
        return;
    }
    if (play->pauseCtx.state == 0) {
        sSave.clock++;
    }
    // Navi's first words after the ending: a negative id makes her fly in and speak at once (z_player.c), set again
    // every frame until she does (Player_UpdateCommon clears it).
    EndingArrival(play);
    if (sNaviCall) {
        Player* player = GET_PLAYER(play);
        if (play->sceneNum != SCENE_TEMPLE_OF_TIME) {
            sNaviCall = false;
        } else if (play->msgCtx.msgMode != MSGMODE_NONE) {
            if ((player->talkActor != nullptr) && (player->talkActor == player->naviActor)) {
                sNaviCall = false;
            }
        } else {
            player->naviTextId = (s16)-MERGESHIP_TEXT_SP_NAVI_END;
        }
    }
    if ((sSave.clock % 20) != 0) {
        return;
    }
    MsSalesmanPlan plan;
    SpNow now;
    if (FindSalesman(play) != nullptr) {
        return;
    }
    if (!PlanHere(play, &plan, &now)) {
        return;
    }
    if ((sLeftScene == SceneOf(play->sceneNum)) && (sLeftStay == now.stay)) {
        return;
    }
    SpawnFromPlan(play, plan);
}

// Navi's C-Up hint in the post-game: the Lost Woods, until he has met Link there.
void NaviHint(uint16_t* textId, bool* loadFromMessageTable) {
    PlayState* play = gPlayState;
    if ((play == nullptr) || !MergeShip_Singleplayer_DoorPending()) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if ((player == nullptr) || (player->naviActor == nullptr) || (player->talkActor != player->naviActor)) {
        return;
    }
    if ((*textId != ElfMessage_GetCUpText(play)) && (*textId != 0x015F)) {
        return;
    }
    *textId = MERGESHIP_TEXT_SP_NAVI_HINT;
    BuildText(textId, loadFromMessageTable);
}

// The Market's Happy Mask Shop door (EnDoor params 0x0DBF, a scene exit at (320, 0, -579)) while he is not in: a
// checkable door whose text is MERGESHIP_TEXT_SP_SHOP_CLOSED.
void CloseShopDoor(Actor* actor) {
    if ((gPlayState == nullptr) || (gPlayState->sceneNum != SCENE_MARKET_DAY) || (actor->id != ACTOR_EN_DOOR) ||
        (actor->params != 0x0DBF) || !MergeShip_Singleplayer_Active() ||
        (MergeShip_Singleplayer_SalesmanPlace() == MS_SALESMAN_SHOP)) {
        return;
    }
    actor->params = (s16)((actor->params & ~0x3BF) | (DOOR_CHECKABLE << 7) | (MERGESHIP_TEXT_SP_SHOP_CLOSED - 0x0200));
    MergeShipTest_Log("SPSALESMAN shop door closed (params 0x%04X)", (u16)actor->params);
}

// ---- save --------------------------------------------------------------------------------------------------------------

void InitSave(bool isDebug) {
    (void)isDebug;
    sSave = SpSave();
}

void LoadSaveV1() {
    SaveManager::Instance->LoadData("seed", sSave.seed, (u32)0);
    SaveManager::Instance->LoadData("adultMaskBought", sSave.adultMaskBought, (u8)0);
    SaveManager::Instance->LoadData("fierceDeityGiven", sSave.fierceDeityGiven, (u8)0);
    SaveManager::Instance->LoadData("offerSlot", sSave.offerSlot, (s32)-1);
    SaveManager::Instance->LoadData("rotation", sSave.rotation, (u8)0);
    SaveManager::Instance->LoadData("clock", sSave.clock, (u32)0);
    SaveManager::Instance->LoadData("ootBeaten", sSave.ootBeaten, (u8)0);
    SaveManager::Instance->LoadData("endingArrival", sSave.endingArrival, (u8)0);
    SaveManager::Instance->LoadData("doorDone", sSave.doorDone, (u8)0);
    SaveManager::Instance->LoadArray("offer", 3, [](size_t i) { SaveManager::Instance->LoadData("", sSave.offer[i], kNoOffer); });
}

void SaveSave(SaveContext* saveContext, int sectionID, bool fullSave) {
    (void)saveContext;
    (void)sectionID;
    (void)fullSave;
    SaveManager::Instance->SaveData("seed", sSave.seed);
    SaveManager::Instance->SaveData("adultMaskBought", sSave.adultMaskBought);
    SaveManager::Instance->SaveData("fierceDeityGiven", sSave.fierceDeityGiven);
    SaveManager::Instance->SaveData("offerSlot", sSave.offerSlot);
    SaveManager::Instance->SaveData("rotation", sSave.rotation);
    SaveManager::Instance->SaveData("clock", sSave.clock);
    SaveManager::Instance->SaveData("ootBeaten", sSave.ootBeaten);
    SaveManager::Instance->SaveData("endingArrival", sSave.endingArrival);
    SaveManager::Instance->SaveData("doorDone", sSave.doorDone);
    SaveManager::Instance->SaveArray("offer", 3, [](size_t i) { SaveManager::Instance->SaveData("", sSave.offer[i]); });
}

void RegisterSave() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    SaveManager::Instance->AddLoadFunction("mergeShipSingleplayer", 1, LoadSaveV1);
    SaveManager::Instance->AddSaveFunction("mergeShipSingleplayer", 1, SaveSave, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(InitSave);
}

static RegisterShipInitFunc sSaveInit(RegisterSave, {});

// ---- hooks -------------------------------------------------------------------------------------------------------------

void RegisterSingleplayer() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    for (u16 id = MERGESHIP_TEXT_SP_GREET; id <= MERGESHIP_TEXT_SP_NAVI_HINT; id++) {
        GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(id, BuildText);
    }
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_SP_SHOP_CLOSED,
                                                                                BuildText);
    GameInteractor::Instance->RegisterGameHookForID<GameInteractor::OnOpenText>(MERGESHIP_TEXT_SP_NAVI_END, BuildText);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(NaviHint);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneSpawnActors>(OnSceneSpawn);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPlayerUpdate>(OnFrame);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnActorInit>(
        [](void* actor) { CloseShopDoor((Actor*)actor); });
}

static RegisterShipInitFunc sInit(RegisterSingleplayer, {});

} // namespace

// ---- the actor -----------------------------------------------------------------------------------------------------------

// MM En_Osn_Profile: ACTORCAT_NPC, attention + friendly; GAMEPLAY_KEEP (his object is read from mm.o2r by path).
extern "C" void MergeShip_SingleplayerActors_Register(void) {
    static bool sRegistered = false;

    if (sRegistered || (ActorDB::Instance == nullptr)) {
        return;
    }
    sRegistered = true;
    ActorDBInit osn = {
        "MM_En_Osn_Travel",
        "The traveling Happy Mask Salesman (MergeShip singleplayer)",
        -1,
        ACTORCAT_NPC,
        (ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED),
        OBJECT_GAMEPLAY_KEEP,
        sizeof(MMEnOsnTravel),
        (ActorFunc)MMEnOsnTravel_Init,
        (ActorFunc)MMEnOsnTravel_Destroy,
        (ActorFunc)MMEnOsnTravel_Update,
        (ActorFunc)MMEnOsnTravel_Draw,
        nullptr,
    };
    ActorDB::Instance->AddEntry(osn);
    gMergeShipActorId_MM_En_Osn_Travel = (s16)ActorDB::Instance->RetrieveId("MM_En_Osn_Travel");
}

static RegisterShipInitFunc sActorInit(MergeShip_SingleplayerActors_Register);

// ---- C API ---------------------------------------------------------------------------------------------------------------

extern "C" s32 MergeShip_Singleplayer_Active(void) {
    return CVarGetInteger(MERGESHIP_SINGLEPLAYER_CVAR, 0) && !IS_RANDO && (gSaveContext.fileNum >= 0) &&
           (gSaveContext.fileNum <= 2);
}

extern "C" s32 MergeShip_Singleplayer_OotBeaten(void) {
    return sSave.ootBeaten;
}

extern "C" s32 MergeShip_Singleplayer_TerminaLocked(void) {
    return MergeShip_Singleplayer_Active() && !sSave.ootBeaten;
}

extern "C" s32 MergeShip_Singleplayer_OnEnding(PlayState* play) {
    if (!MergeShip_Singleplayer_Active()) {
        return false;
    }
    // Terminator 117 would start the credits (GAMEMODE_END_CREDITS, adult Link riding into Hyrule Field). Instead:
    // Link is a child again, in the Temple of Time before the Master Sword's pedestal (spawn 4, where the Chamber of
    // Sages returns him), and Navi stays with him (no 0xFFF1 scene of her flying off).
    sSave.ootBeaten = 1;
    sSave.endingArrival = 1;
    sArrivalWait = 0;
    play->linkAgeOnLoad = LINK_AGE_CHILD;
    play->nextEntranceIndex = ENTR_TEMPLE_OF_TIME_4;
    gSaveContext.cutsceneIndex = 0;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_WHITE;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_WHITE;
    MergeShipTest_Log("SPENDING Zelda's farewell is over: no credits, child Link to the Temple of Time");
    return true;
}

extern "C" s32 MergeShip_Singleplayer_DoorPending(void) {
    return MergeShip_Singleplayer_Active() && sSave.ootBeaten && !sSave.doorDone;
}

extern "C" s32 MergeShip_Singleplayer_HasAnyMask(void) {
    for (s32 i = 0; i < kStockCount; i++) {
        if (Owned(i)) {
            return true;
        }
    }
    return sSave.fierceDeityGiven && MergeShip_HasTransformationMask(MM_FORM_FIERCE_DEITY);
}

extern "C" s32 MergeShip_Singleplayer_SellAllMasks(void) {
    const u8 wornForm = MergeShip_FormToMM(MergeShip_GetPlayerForm());
    s32 paid = 0;

    for (s32 i = 0; i < kStockCount; i++) {
        const StockEntry& e = kStock[i];
        if (!Owned(i)) {
            continue;
        }
        if (MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(e.item)) {
            const u8 form = (u8)(e.item - ITEM_MM_MASK_FIERCE_DEITY);
            if (form == wornForm) {
                continue; // the mask he is transformed by stays on his face
            }
            gMergeShipSave.ownedMasks &= ~(1 << form);
        } else {
            gMergeShipSave.ownedMMMasks &= ~(1u << (e.item - ITEM_MM_MASK_TRUTH + MM_MASK_TRUTH));
        }
        paid += e.price;
    }
    if (sSave.fierceDeityGiven && MergeShip_HasTransformationMask(MM_FORM_FIERCE_DEITY) &&
        (wornForm != MM_FORM_FIERCE_DEITY)) {
        gMergeShipSave.ownedMasks &= ~(1 << MM_FORM_FIERCE_DEITY);
    }
    // Nothing he took back stays on a C button, nor on Link's face.
    for (s32 b = 1; b < 4; b++) {
        const u8 item = gSaveContext.equips.buttonItems[b];
        if ((MERGESHIP_ITEM_IS_TRANSFORMATION_MASK(item) || MERGESHIP_ITEM_IS_MM_REGULAR_MASK(item)) &&
            !MergeShip_HasMMMaskItem(item)) {
            gSaveContext.equips.buttonItems[b] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[b - 1] = SLOT_NONE;
        }
    }
    if ((gPlayState != nullptr) && (GET_PLAYER(gPlayState) != nullptr)) {
        Player* player = GET_PLAYER(gPlayState);
        if (PLAYER_MASK_IS_MM_REGULAR(player->currentMask) &&
            !MergeShip_HasMMMask(PLAYER_MASK_TO_MM_MASK(player->currentMask))) {
            player->currentMask = PLAYER_MASK_NONE;
        }
    }
    if (paid > 0) {
        Rupees_ChangeBy(paid);
    }
    sSoldFor = paid;
    MergeShipTest_Log("SPDOOR sold every mask back for %d rupees", paid);
    return paid;
}

extern "C" void MergeShip_Singleplayer_DebugSetState(s32 ootBeaten, s32 doorDone) {
    sSave.ootBeaten = (u8)ootBeaten;
    sSave.doorDone = (u8)doorDone;
}

extern "C" void MergeShip_Singleplayer_DoorDone(void) {
    sSave.doorDone = 1;
    MergeShipTest_Log("SPDOOR the salesman walked away: the Lost Woods lead to Termina");
}

extern "C" s32 MergeShip_Singleplayer_SalesmanPlace(void) {
    SpNow now = Evaluate(sSave.clock);
    return now.traveling ? -1 : now.place;
}

MsSalesmanPlan gMsSalesmanPlan;

extern "C" s32 MergeShip_Singleplayer_StayOver(s32 stay) {
    SpNow now = Evaluate(sSave.clock);
    return (now.stay > stay) || ((now.stay == stay) && now.traveling);
}

extern "C" s32 MergeShip_Singleplayer_DeparturePlan(MsSalesmanPlan* plan) {
    PlayState* play = gPlayState;
    SpNow now = Evaluate(sSave.clock);
    s32 here = (play != nullptr) ? SceneOf(play->sceneNum) : -1;
    if ((here < 0) || (DebugPlace() >= 0)) {
        return false;
    }
    // The route out of this stay's place, from its first leg (the one in this scene).
    std::vector<SpLeg> legs = BuildRoute(now.place, PlaceAt(now.stay + 1));
    for (const SpLeg& leg : legs) {
        if ((leg.scene == here) && !leg.pts.empty()) {
            FillTravelPlan(plan, leg, 0.0f, now.stay, now.stay + 1);
            plan->fadeIn = 0;
            return true;
        }
    }
    return false;
}

extern "C" void MergeShip_Singleplayer_LeftScene(s32 stay) {
    sLeftStay = stay;
    sLeftScene = (gPlayState != nullptr) ? SceneOf(gPlayState->sceneNum) : -1;
    MergeShipTest_Log("SPSALESMAN left the scene (stay %d)", stay);
}

extern "C" void MergeShip_Singleplayer_LogWhere(void) {
    SpNow now = Evaluate(sSave.clock);
    LogWhere();
    MergeShipTest_Log("SPWHERE clock %u stay %d place %d next %d traveling %d leg %d/%d scene %d offset %.0f", sSave.clock,
                      now.stay, now.place, now.next, now.traveling ? 1 : 0, now.leg, (s32)now.legs.size(),
                      now.traveling ? Scenes()[now.legs[now.leg].scene].scene : -1, now.offset);
}

// Tests: the next clock (from now) at which he walks through `sceneNum` (offset at least `minOffset` along it, or 0
// at the start of that leg); 0 when none within a long while. `arriving`: only a leg that ends at that scene's hub.
extern "C" u32 MergeShip_Singleplayer_FindTravel(s16 sceneNum, f32 minOffset, s32 arriving) {
    s32 target = SceneOf(sceneNum);
    for (u32 c = sSave.clock; c < sSave.clock + 2000000; c += 10) {
        SpNow now = Evaluate(c);
        if (now.traveling && (now.legs[now.leg].scene == target) && (now.offset >= minOffset) &&
            (!arriving || now.legs[now.leg].endAtHub)) {
            return c;
        }
    }
    return 0;
}

extern "C" u32 MergeShip_Singleplayer_Clock(void) {
    return sSave.clock;
}

extern "C" void MergeShip_Singleplayer_SetClock(u32 clock) {
    sSave.clock = clock;
}

extern "C" void MergeShip_Singleplayer_ShopBegin(void) {
    if (sSave.offerSlot != CurrentStay()) {
        RollOffer();
    }
    sShownCount = 0;
    for (s32 k = 0; k < 3; k++) {
        u8 stock = sSave.offer[k];
        if ((stock < kStockCount) && !Owned(stock)) {
            sShown[sShownCount++] = stock;
        }
    }
    // Everything this quarter offered is bought, yet more is left: the rotation moves on at once.
    if (sShownCount == 0) {
        RollOffer();
        for (s32 k = 0; k < 3; k++) {
            if (sSave.offer[k] < kStockCount) {
                sShown[sShownCount++] = sSave.offer[k];
            }
        }
    }
    sChosen = kNoOffer;
    sChoseAdult = false;
}

extern "C" s32 MergeShip_Singleplayer_OfferCount(void) {
    return sShownCount;
}

extern "C" s32 MergeShip_Singleplayer_ChooseOffer(s32 choice) {
    if ((choice < 0) || (choice >= sShownCount)) {
        return false;
    }
    sChosen = sShown[choice];
    sChoseAdult = false;
    return true;
}

extern "C" s32 MergeShip_Singleplayer_BuyChosen(void) {
    if (sChosen >= kStockCount) {
        return false;
    }
    const StockEntry& e = kStock[sChosen];
    if (gSaveContext.rupees < e.price) {
        return false;
    }
    Rupees_ChangeBy(-e.price);
    if ((e.item >= ITEM_MM_MASK_FIERCE_DEITY) && (e.item <= ITEM_MM_MASK_DEKU)) {
        MergeShip_GiveTransformationMask((u8)(e.item - ITEM_MM_MASK_FIERCE_DEITY));
    } else {
        MergeShip_GiveMMMask((u8)(e.item - ITEM_MM_MASK_TRUTH + MM_MASK_TRUTH));
    }
    MergeShip_MMAudio_PlayMMFanfare(2); // MM NA_BGM_GET_NEW_MASK
    MergeShipTest_Log("SPSHOP bought %s for %d (rupees now %d)", e.name, e.price, gSaveContext.rupees - e.price);
    return true;
}

extern "C" s32 MergeShip_Singleplayer_AdultMaskOffered(void) {
    return MergeShip_Singleplayer_Active() && LINK_IS_ADULT && !sSave.adultMaskBought;
}

extern "C" s32 MergeShip_Singleplayer_AdultMaskBought(void) {
    return sSave.adultMaskBought;
}

extern "C" s32 MergeShip_Singleplayer_BuyAdultMask(void) {
    sChoseAdult = true;
    if (gSaveContext.rupees < kAdultMaskPrice) {
        return false;
    }
    Rupees_ChangeBy(-kAdultMaskPrice);
    sSave.adultMaskBought = 1;
    MergeShip_MMAudio_PlayMMFanfare(2);
    MergeShipTest_Log("SPSHOP bought the Adult Mask for %d", kAdultMaskPrice);
    return true;
}

extern "C" s32 MergeShip_Singleplayer_GiveFierceDeityIfDone(void) {
    if (sSave.fierceDeityGiven) {
        return false;
    }
    for (s32 i = 0; i < kStockCount; i++) {
        if (!Owned(i)) {
            return false;
        }
    }
    sSave.fierceDeityGiven = 1;
    MergeShip_GiveTransformationMask(MM_FORM_FIERCE_DEITY);
    MergeShip_MMAudio_PlayMMFanfare(2);
    MergeShipTest_Log("SPSHOP every mask bought: the Fierce Deity's Mask given");
    return true;
}

extern "C" s32 MergeShip_Singleplayer_SoldOut(void) {
    for (s32 i = 0; i < kStockCount; i++) {
        if (!Owned(i)) {
            return false;
        }
    }
    return sSave.fierceDeityGiven;
}
