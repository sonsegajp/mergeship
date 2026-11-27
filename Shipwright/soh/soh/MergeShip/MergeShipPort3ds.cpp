#include "MergeShipPort3ds.h"

#include <libultraship/libultraship.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <fast/Fast3dWindow.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <ship/resource/ResourceType.h>
#include <ship/resource/type/Blob.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/window/Window.h>
#include <ship/utils/StrHash64.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include "macros.h"
#include "soh/MergeShip/MergeShipForms.h"
}

#if MERGESHIP_PORT3DS // (switched off for now: MergeShipPort3ds.h; the stubs are at the end of the file)

#define CVAR_PORT3DS_ROOMS_NAME CVAR_ENHANCEMENT("MergeShip.Port3ds.Rooms")
#define CVAR_PORT3DS_MODELS_NAME CVAR_ENHANCEMENT("MergeShip.Port3ds.Models")
#define CVAR_PORT3DS_ANIMATIONS_NAME CVAR_ENHANCEMENT("MergeShip.Port3ds.Animations")
#define CVAR_PORT3DS_OBJECTS_NAME CVAR_ENHANCEMENT("MergeShip.Port3ds.Objects")

extern "C" void MergeShipTest_Log(const char* fmt, ...); // MergeShipTest.cpp (no-op outside the harness)

namespace {
// Material animations run on the 3DS game's 30 frames a second; Ocarina of Time's play state ticks 20 a second.
constexpr f32 kFramesPerTick = 30.0f / 20.0f;
constexpr u8 kOpNoop = 0x00;
constexpr u8 kOpDisplayList = 0xDE;
constexpr u8 kOpPushShader = 0x43;
constexpr u8 kOpPopShader = 0x44;
constexpr u8 kOpSetTileSizeLerp = 0x4A;

// Vertices one material lights by their normals: `count` of them from `first` in a vertex array, the material's
// ambient and diffuse factors, and each vertex's unlit colour.
struct LitRange {
    Vtx* vertices;
    u32 count;
    f32 ambient[3];
    f32 diffuse[3];
    const u8* base;
};

struct Scroll {
    u32 instruction; // a block of five commands, rewritten into G_SETTILESIZE_LERP
    u32 tile;
    f32 lrs, lrt;    // tile extent, quarter texels
    f32 gain[2];     // track value -> quarter texels
    f32 period[2];   // quarter texels after which the texture repeats (0: clamped)
    s32 track[2];    // -1: that axis does not move
};

struct Colour {
    u32 instruction; // a G_SETPRIMCOLOR or G_SETENVCOLOR whose colour is rewritten
    u8 base[4];
    s32 track[4];
};

// What drives a material animation, or decides whether a part of a room is drawn (tools/port3ds/cmab.py). Most
// animations run on their own clock; the 3DS game holds the others to its state: by day or by night only (the
// daylight in a dungeon's entrance, lit windows), read at the hour of the day (the windows of Lon Lon Ranch), or
// for one of Link's two eras only. While a driver does not hold, a material keeps its own values.
enum Driver : u32 { kAlways = 0, kDay = 1, kNight = 2, kHour = 3, kAdult = 4, kChild = 5 };

struct Track {
    const f32* values;
    u32 count; // samples: one per 3DS frame of its animation, or ten an hour from 0 h to 24 h (kHour)
    u32 driver;
};

// A part of a room the 3DS game shows in one state only (the Shooting Gallery has the Market's backdrop and
// Kakariko's on the same wall): a no-op command, rewritten into a jump to `to` while the driver does not hold.
struct Switch {
    u32 at;
    u32 to;
    u32 driver;
};

// The game's state the drivers look at
struct Clock {
    f32 hour; // 0 .. 24
    bool night;
    bool adult;
};

// What "<list>/anim" holds for one list (tools/port3ds/roomconv.py AnimTable)
struct ListAnims {
    std::shared_ptr<Ship::IResource> blob;
    std::vector<std::string> shaders; // "__OTR__port3ds/shaders/..." (the push commands point at these)
    std::vector<Scroll> scrolls;
    std::vector<Colour> colours;
    std::vector<Switch> switches;
    std::vector<Track> tracks;
    bool loaded = false;
};

struct RoomLists {
    std::shared_ptr<Fast::DisplayList> opa;
    std::shared_ptr<Fast::DisplayList> xlu;
    ListAnims opaAnims;
    ListAnims xluAnims;
    std::vector<std::shared_ptr<Ship::IResource>> litResources; // (kept alive: the ranges point into them)
    std::vector<LitRange> lit;
    EnvLightSettings lastLight;
    bool lastLightValid = false;
};

s32 sRoomDrawn = false;

// The scene's 3DS light of this frame (the room draw blends it), for the characters; or none.
EnvLightSettings sSceneLight;
bool sSceneLightValid = false;
// The scene is one the 3DS game lights by the time of day while this game shows it as a painted background
// (SceneLights::byTime), as of the last room draw.
bool sSceneByTime = false;

template <typename T> bool Read(const u8* data, size_t size, size_t& at, T& out) {
    if (at + sizeof(T) > size) {
        return false;
    }
    memcpy(&out, data + at, sizeof(T));
    at += sizeof(T);
    return true;
}

// Reads "<list>/anim" and turns its shader placeholders into G_PUSH_SHADER / G_POP_SHADER. A list whose table is
// missing or damaged is left with no-ops there (drawn with the default shader) and no animations.
void LoadAnims(ListAnims& anims, const std::shared_ptr<Fast::DisplayList>& list, const std::string& listPath) {
    if (list == nullptr) {
        return;
    }
    std::string path = listPath + "/anim";
    if (!ResourceMgr_FileExists(path.c_str())) {
        return;
    }
    auto blob = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
    if (blob == nullptr || blob->GetInitData()->Type != static_cast<uint32_t>(Ship::ResourceType::Blob)) {
        return;
    }
    const u8* data = (const u8*)blob->GetRawPointer();
    const size_t size = blob->GetPointerSize();
    size_t at = 0;
    char magic[4];
    // (version 1: every track runs on its own clock and has duration + 1 samples; version 2 adds the switches
    // and gives each track its own length and driver)
    u32 version, duration, shaderCount, pushCount, scrollCount, colourCount, trackCount, switchCount = 0;
    if (data == nullptr || !Read(data, size, at, magic) || memcmp(magic, "P3AN", 4) != 0 ||
        !Read(data, size, at, version) || (version != 1 && version != 2) || !Read(data, size, at, duration) ||
        !Read(data, size, at, shaderCount) || !Read(data, size, at, pushCount) || !Read(data, size, at, scrollCount) ||
        !Read(data, size, at, colourCount) || !Read(data, size, at, trackCount) ||
        (version >= 2 && !Read(data, size, at, switchCount))) {
        return;
    }
    std::vector<std::string> shaders;
    for (u32 i = 0; i < shaderCount; i++) {
        u16 length;
        if (!Read(data, size, at, length) || at + length > size) {
            return;
        }
        shaders.emplace_back((const char*)data + at, length);
        at += length;
    }
    at = (at + 3) & ~(size_t)3;
    std::vector<u32> pushes(pushCount * 3);
    for (u32& v : pushes) {
        if (!Read(data, size, at, v)) {
            return;
        }
    }
    std::vector<Scroll> scrolls(scrollCount);
    for (Scroll& s : scrolls) {
        if (!Read(data, size, at, s.instruction) || !Read(data, size, at, s.tile) || !Read(data, size, at, s.lrs) ||
            !Read(data, size, at, s.lrt) || !Read(data, size, at, s.gain[0]) || !Read(data, size, at, s.gain[1]) ||
            !Read(data, size, at, s.period[0]) || !Read(data, size, at, s.period[1]) ||
            !Read(data, size, at, s.track[0]) || !Read(data, size, at, s.track[1])) {
            return;
        }
    }
    std::vector<Colour> colours(colourCount);
    for (Colour& c : colours) {
        if (!Read(data, size, at, c.instruction) || !Read(data, size, at, c.base)) {
            return;
        }
        for (int k = 0; k < 4; k++) {
            if (!Read(data, size, at, c.track[k])) {
                return;
            }
        }
    }
    std::vector<Switch> switches(switchCount);
    for (Switch& s : switches) {
        if (!Read(data, size, at, s.at) || !Read(data, size, at, s.to) || !Read(data, size, at, s.driver)) {
            return;
        }
    }
    std::vector<Track> tracks(trackCount);
    if (version == 1) {
        if (at + (size_t)trackCount * (duration + 1) * sizeof(f32) > size) {
            return;
        }
        for (u32 i = 0; i < trackCount; i++) {
            tracks[i] = { (const f32*)(data + at) + (size_t)i * (duration + 1), duration + 1, kAlways };
        }
    } else {
        size_t total = 0;
        for (Track& t : tracks) {
            if (!Read(data, size, at, t.count) || !Read(data, size, at, t.driver) || t.count == 0 ||
                t.driver > kChild) {
                return;
            }
            total += t.count;
        }
        if (at + total * sizeof(f32) > size) {
            return;
        }
        const f32* values = (const f32*)(data + at);
        for (Track& t : tracks) {
            t.values = values;
            values += t.count;
        }
    }
    // every instruction the table names must lie inside the list, and every track reference inside the table
    const size_t count = list->Instructions.size();
    for (const Switch& s : switches) {
        if (s.at >= count || s.to >= count || s.driver > kChild) {
            return;
        }
    }
    for (u32 i = 0; i < pushCount; i++) {
        if (pushes[i * 3] >= count || pushes[i * 3 + 1] >= count || pushes[i * 3 + 2] >= shaderCount) {
            return;
        }
    }
    for (const Scroll& s : scrolls) {
        if (s.instruction + 5 > count || s.tile > 7 || s.track[0] >= (s32)trackCount || s.track[1] >= (s32)trackCount) {
            return;
        }
    }
    for (const Colour& c : colours) {
        if (c.instruction >= count) {
            return;
        }
        for (int k = 0; k < 4; k++) {
            if (c.track[k] >= (s32)trackCount) {
                return;
            }
        }
    }

    anims.blob = blob;
    anims.shaders = std::move(shaders);
    anims.scrolls = std::move(scrolls);
    anims.colours = std::move(colours);
    anims.switches = std::move(switches);
    anims.tracks = std::move(tracks);
    anims.loaded = true;
    for (u32 i = 0; i < pushCount; i++) {
        Gfx& push = (Gfx&)list->Instructions[pushes[i * 3]];
        Gfx& pop = (Gfx&)list->Instructions[pushes[i * 3 + 1]];
        // (std::string keeps its characters at an even heap address, as the renderer needs for a pointer)
        push.words.w0 = (uintptr_t)kOpPushShader << 24;
        push.words.w1 = (uintptr_t)anims.shaders[pushes[i * 3 + 2]].c_str();
        pop.words.w0 = (uintptr_t)kOpPopShader << 24;
        pop.words.w1 = 0;
    }
}

bool Holds(u32 driver, const Clock& clock) {
    switch (driver) {
        case kDay:
            return !clock.night;
        case kNight:
            return clock.night;
        case kAdult:
            return clock.adult;
        case kChild:
            return !clock.adult;
        default:
            return true;
    }
}

// A track's value at `frame` of its own clock (or at the hour of the day). Returns false while the track's driver
// does not hold: the material keeps its own value then.
bool Sample(const ListAnims& anims, s32 index, const Clock& clock, f32 frame, f32& out) {
    const Track& track = anims.tracks[index];
    if (!Holds(track.driver, clock)) {
        return false;
    }
    const u32 last = track.count - 1;
    if (last == 0) {
        out = track.values[0];
        return true;
    }
    f32 f;
    if (track.driver == kHour) {
        f = clock.hour / 24.0f * (f32)last;
    } else {
        f = fmodf(frame, (f32)last);
        if (f < 0.0f) {
            f += last;
        }
    }
    if (f < 0.0f) {
        f = 0.0f;
    }
    u32 i = (u32)f;
    if (i >= last) {
        out = track.values[last];
        return true;
    }
    out = track.values[i] + (track.values[i + 1] - track.values[i]) * (f - (f32)i);
    return true;
}

u32 FloatBits(f32 value) {
    u32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Writes this tick's scrolls (interpolated from the previous tick), constant colours and part switches into the
// list.
void Animate(ListAnims& anims, const std::shared_ptr<Fast::DisplayList>& list, u32 ticks, const Clock& clock) {
    if (list == nullptr || !anims.loaded) {
        return;
    }
    const f32 now = ticks * kFramesPerTick;
    const f32 before = (ticks > 0 ? ticks - 1 : 0) * kFramesPerTick;
    for (const Scroll& s : anims.scrolls) {
        f32 at[2][2]; // [previous, current][s, t]
        for (int axis = 0; axis < 2; axis++) {
            f32 a = 0.0f;
            f32 b = 0.0f;
            if (s.track[axis] >= 0 && Sample(anims, s.track[axis], clock, before, a) &&
                Sample(anims, s.track[axis], clock, now, b)) {
                a *= s.gain[axis];
                b *= s.gain[axis];
            } else {
                a = b = 0.0f;
            }
            if (s.period[axis] > 0.0f) {
                // the same whole number of repeats off both ends, so the in-between frames glide forwards
                f32 wrap = floorf(b / s.period[axis]) * s.period[axis];
                a -= wrap;
                b -= wrap;
                if (fabsf(a - b) > s.period[axis] * 0.5f) {
                    a = b; // the animation looped back to its start: no glide across the jump
                }
            }
            at[0][axis] = a;
            at[1][axis] = b;
        }
        Gfx* g = (Gfx*)&list->Instructions[s.instruction];
        g[0].words.w0 = (uintptr_t)kOpSetTileSizeLerp << 24;
        g[0].words.w1 = (uintptr_t)(s.tile & 7) << 24;
        g[1].words.w0 = FloatBits(at[0][0]);
        g[1].words.w1 = FloatBits(at[0][1]);
        g[2].words.w0 = FloatBits(at[0][0] + s.lrs);
        g[2].words.w1 = FloatBits(at[0][1] + s.lrt);
        g[3].words.w0 = FloatBits(at[1][0]);
        g[3].words.w1 = FloatBits(at[1][1]);
        g[4].words.w0 = FloatBits(at[1][0] + s.lrs);
        g[4].words.w1 = FloatBits(at[1][1] + s.lrt);
    }
    for (const Colour& c : anims.colours) {
        u8 rgba[4];
        for (int k = 0; k < 4; k++) {
            f32 v;
            if (c.track[k] < 0 || !Sample(anims, c.track[k], clock, now, v)) {
                rgba[k] = c.base[k];
            } else {
                v = v * 255.0f + 0.5f;
                rgba[k] = v <= 0.0f ? 0 : v >= 255.0f ? 255 : (u8)v;
            }
        }
        Gfx& g = (Gfx&)list->Instructions[c.instruction];
        g.words.w1 = ((uintptr_t)rgba[0] << 24) | ((uintptr_t)rgba[1] << 16) | ((uintptr_t)rgba[2] << 8) | rgba[3];
    }
    for (const Switch& s : anims.switches) {
        Gfx& g = (Gfx&)list->Instructions[s.at];
        if (Holds(s.driver, clock)) {
            g.words.w0 = (uintptr_t)kOpNoop << 24;
            g.words.w1 = 0;
        } else {
            // (a branch: G_DL that does not come back)
            g.words.w0 = ((uintptr_t)kOpDisplayList << 24) | ((uintptr_t)1 << 16);
            g.words.w1 = (uintptr_t)&list->Instructions[s.to];
        }
    }
}

// "<list path>/lit": 'P3LT', range count, then per range { u16 array, u16 pad, u32 first, u32 count, u8 ambient[3],
// u8 diffuse[3] }, then every range's base colours (3 bytes a vertex). The arrays are "<list path>/vtx_<array>".
void LoadLit(RoomLists& room, const std::string& listPath) {
    std::string path = listPath + "/lit";
    if (!ResourceMgr_FileExists(path.c_str())) {
        return;
    }
    auto resourceManager = Ship::Context::GetRawInstance()->GetResourceManager();
    auto blob = resourceManager->LoadResource(path);
    if (blob == nullptr || blob->GetInitData()->Type != static_cast<uint32_t>(Ship::ResourceType::Blob)) {
        return;
    }
    const u8* data = (const u8*)blob->GetRawPointer();
    size_t size = blob->GetPointerSize();
    if (data == nullptr || size < 8 || memcmp(data, "P3LT", 4) != 0) {
        return;
    }
    u32 rangeCount;
    memcpy(&rangeCount, data + 4, 4);
    const size_t recordSize = 18;
    if (size < 8 + (size_t)rangeCount * recordSize) {
        return;
    }
    const u8* base = data + 8 + (size_t)rangeCount * recordSize;
    room.litResources.push_back(blob);
    for (u32 i = 0; i < rangeCount; i++) {
        const u8* rec = data + 8 + (size_t)i * recordSize;
        u16 array;
        u32 first;
        u32 count;
        memcpy(&array, rec, 2);
        memcpy(&first, rec + 4, 4);
        memcpy(&count, rec + 8, 4);
        auto vertices = resourceManager->LoadResource(listPath + "/vtx_" + std::to_string(array));
        if (vertices == nullptr || vertices->GetRawPointer() == nullptr ||
            (size_t)(base - data) + (size_t)count * 3 > size ||
            ((size_t)first + count) * sizeof(Vtx) > vertices->GetPointerSize()) {
            return; // a damaged table: leave the rest lit as shipped
        }
        LitRange range;
        range.vertices = (Vtx*)vertices->GetRawPointer() + first;
        range.count = count;
        for (int c = 0; c < 3; c++) {
            range.ambient[c] = rec[12 + c] / 255.0f;
            range.diffuse[c] = rec[15 + c] / 255.0f;
        }
        range.base = base;
        base += (size_t)count * 3;
        room.litResources.push_back(vertices);
        room.lit.push_back(range);
    }
}

// The pack's two-byte octahedral normal (tools/port3ds/meshout.py oct_encode).
void DecodeNormal(u16 packed, f32* n) {
    f32 x = (packed >> 8) / 255.0f * 2.0f - 1.0f;
    f32 y = (packed & 0xFF) / 255.0f * 2.0f - 1.0f;
    f32 z = 1.0f - fabsf(x) - fabsf(y);
    f32 t = z < 0.0f ? -z : 0.0f;
    x += x >= 0.0f ? -t : t;
    y += y >= 0.0f ? -t : t;
    f32 length = sqrtf(x * x + y * y + z * z);
    n[0] = x / length;
    n[1] = y / length;
    n[2] = z / length;
}

// One scene setup's light settings: a count byte, that many EnvLightSettings records, then a flags byte (packs
// before it have none). Flag 1: the 3DS game lights the scene by the time of day while Ocarina of Time lights it
// with one fixed setting (the Market, the Temple of Time's grounds, Link's house); the list is then the 3DS
// scene's own, in its own order (dawn, day, dusk, night first).
struct SceneLights {
    std::shared_ptr<Ship::IResource> blob;
    EnvLightSettings* list = nullptr;
    s32 count = 0;
    bool byTime = false;
};
std::unordered_map<std::string, SceneLights> sLights;

const SceneLights& LoadLights(const std::string& path) {
    auto it = sLights.find(path);
    if (it != sLights.end()) {
        return it->second;
    }
    SceneLights lights;
    if (ResourceMgr_FileExists(path.c_str())) {
        auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
        if (res != nullptr && res->GetInitData()->Type == static_cast<uint32_t>(Ship::ResourceType::Blob)) {
            const u8* data = (const u8*)res->GetRawPointer();
            size_t size = res->GetPointerSize();
            if (data != nullptr && size >= 1 && size >= 1 + (size_t)data[0] * sizeof(EnvLightSettings)) {
                lights.blob = res;
                lights.list = (EnvLightSettings*)(data + 1);
                lights.count = data[0];
                const size_t flags = 1 + (size_t)data[0] * sizeof(EnvLightSettings);
                lights.byTime = size > flags && (data[flags] & 1) != 0;
            }
        }
    }
    return sLights.emplace(path, std::move(lights)).first->second;
}

// Which 3DS setting the floor under Link has (tools/port3ds/lightzones.py). The 3DS game cut the scenes' floors up
// anew: Dodongo's Cavern has five settings here and eight there, and what is one light index here is four
// settings there, a blue one among them. So an indoor scene's setting is looked up by the floor itself:
// "<scene folder>/lightfloors" is a u32 count and a byte a polygon of the scene's collision, the 3DS setting of
// that floor (0x1F: the floor leaves the light as it is; 0xFF: unknown), and "<scene folder>/lights3ds_<setup>"
// the 3DS scene's own settings in its own order (a list as LoadLights reads).
struct LightFloors {
    std::shared_ptr<Ship::IResource> blob;
    const u8* floors = nullptr;
    u32 count = 0;
};
std::unordered_map<std::string, LightFloors> sLightFloors;

const LightFloors& LoadLightFloors(const std::string& path) {
    auto it = sLightFloors.find(path);
    if (it != sLightFloors.end()) {
        return it->second;
    }
    LightFloors out;
    if (ResourceMgr_FileExists(path.c_str())) {
        auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
        if (res != nullptr && res->GetInitData()->Type == static_cast<uint32_t>(Ship::ResourceType::Blob)) {
            const u8* data = (const u8*)res->GetRawPointer();
            const size_t size = res->GetPointerSize();
            u32 count = 0;
            if (data != nullptr && size >= 4) {
                memcpy(&count, data, sizeof(count));
                if (count > 0 && size >= 4 + (size_t)count) {
                    out.blob = res;
                    out.floors = data + 4;
                    out.count = count;
                }
            }
        }
    }
    return sLightFloors.emplace(path, std::move(out)).first->second;
}

// The light by the floor: from the setting it was to the floor's, a twentieth a frame (the game's own pace from
// one setting into the next).
struct FloorLight {
    std::string scene; // the list this is of ("<scene folder>/lights3ds_<setup>")
    s32 index = -1;    // the setting the light is going to (-1: no floor seen yet)
    f32 from[3][3] = {};
    f32 fromDir[2][3] = {};
    f32 weight = 1.0f;
    u32 frame = 0;
};
FloorLight sFloorLight;

bool FloorLightOf(PlayState* play, const std::string& folder, EnvLightSettings* out) {
    EnvironmentContext* env = &play->envCtx;
    // (not where the hour lights the scene, nor while the game forces a setting of its own: a cutscene's, an
    // actor's. Those are the game's indices, and the list by those answers.)
    if (!env->indoors || env->unk_BF != 0xFF) {
        return false;
    }
    const std::string listPath = folder + "lights3ds_" + std::to_string((int)gSaveContext.sceneLayer);
    const SceneLights* list = &LoadLights(listPath);
    if (list->list == nullptr) {
        list = &LoadLights(folder + "lights3ds_0");
    }
    const LightFloors& floors = LoadLightFloors(folder + "lightfloors");
    if (list->list == nullptr || list->count <= 0 || floors.floors == nullptr) {
        return false;
    }
    FloorLight& state = sFloorLight;
    if (state.scene != listPath) {
        state = FloorLight();
        state.scene = listPath;
    }
    s32 floor = -1;
    Player* player = GET_PLAYER(play);
    if (player != nullptr && player->actor.floorPoly != nullptr && player->actor.floorBgId == BGCHECK_SCENE &&
        play->colCtx.colHeader != nullptr && play->colCtx.colHeader->polyList != nullptr) {
        const ptrdiff_t n = player->actor.floorPoly - play->colCtx.colHeader->polyList;
        if (n >= 0 && (u32)n < floors.count && n < (ptrdiff_t)play->colCtx.colHeader->numPolygons) {
            const u8 setting = floors.floors[n];
            if (setting != 0x1F && setting < list->count) {
                floor = setting;
            }
        }
    }
    const auto colours = [](const EnvLightSettings& s, int which) -> const u8* {
        return which == 0 ? s.ambientColor : (which == 1 ? s.light1Color : s.light2Color);
    };
    const auto dirs = [](const EnvLightSettings& s, int which) -> const s8* {
        return which == 0 ? s.light1Dir : s.light2Dir;
    };
    if (state.index < 0) {
        if (floor < 0) {
            return false; // (no floor seen in this scene yet: the game's index says the light meanwhile)
        }
        state.index = floor;
        state.weight = 1.0f;
    } else if (floor >= 0 && floor != state.index) {
        // from what the light is now
        const EnvLightSettings& to = list->list[state.index];
        for (int which = 0; which < 3; which++) {
            for (int c = 0; c < 3; c++) {
                state.from[which][c] += ((f32)colours(to, which)[c] - state.from[which][c]) * state.weight;
            }
        }
        for (int which = 0; which < 2; which++) {
            for (int c = 0; c < 3; c++) {
                state.fromDir[which][c] += ((f32)dirs(to, which)[c] - state.fromDir[which][c]) * state.weight;
            }
        }
        state.index = floor;
        state.weight = 0.0f;
        state.frame = play->gameplayFrames;
    }
    if (state.weight < 1.0f && play->gameplayFrames != state.frame) {
        state.weight += 0.05f * (f32)(play->gameplayFrames - state.frame);
        state.frame = play->gameplayFrames;
        if (state.weight > 1.0f) {
            state.weight = 1.0f;
        }
    }
    const EnvLightSettings& to = list->list[state.index];
    if (state.weight >= 1.0f) {
        for (int which = 0; which < 3; which++) {
            for (int c = 0; c < 3; c++) {
                state.from[which][c] = (f32)colours(to, which)[c];
            }
        }
        for (int which = 0; which < 2; which++) {
            for (int c = 0; c < 3; c++) {
                state.fromDir[which][c] = (f32)dirs(to, which)[c];
            }
        }
    }
    const s16* adjust[3] = { env->adjAmbientColor, env->adjLight1Color, env->adjLight1Color };
    u8* outColours[3] = { out->ambientColor, out->light1Color, out->light2Color };
    for (int which = 0; which < 3; which++) {
        for (int c = 0; c < 3; c++) {
            f32 v = state.from[which][c] + ((f32)colours(to, which)[c] - state.from[which][c]) * state.weight +
                    (f32)adjust[which][c];
            outColours[which][c] = (u8)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
        }
    }
    s8* outDirs[2] = { out->light1Dir, out->light2Dir };
    for (int which = 0; which < 2; which++) {
        for (int c = 0; c < 3; c++) {
            outDirs[which][c] = (s8)(state.fromDir[which][c] +
                                     ((f32)dirs(to, which)[c] - state.fromDir[which][c]) * state.weight);
        }
    }
    return true;
}

// Keeps the lists of the rooms seen so far alive; a room is looked up once.
std::unordered_map<std::string, RoomLists> sRooms;

std::shared_ptr<Fast::DisplayList> LoadList(const std::string& path) {
    if (!ResourceMgr_FileExists(path.c_str())) {
        return nullptr;
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
    if (res == nullptr || res->GetInitData()->Type != static_cast<uint32_t>(Fast::ResourceType::DisplayList)) {
        return nullptr;
    }
    auto list = std::static_pointer_cast<Fast::DisplayList>(res);
    return list->Instructions.empty() ? nullptr : list;
}

// The generated material shaders exist for Direct3D and OpenGL; Metal draws them untested, so the rooms stay
// Ocarina of Time's there.
bool BackendSupported() {
    static int sSupported = -1;
    if (sSupported < 0) {
        auto window = Ship::Context::GetRawInstance()->GetWindow();
        sSupported = window != nullptr && window->GetWindowBackend() != Fast::WindowBackend::FAST3D_SDL_METAL;
    }
    return sSupported != 0;
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// Characters: a skeleton whose 3DS model is in the pack ("port3ds/skel/<skeleton resource path>/...",
// tools/port3ds/modelconv.py) is drawn from that model. The game still walks its own skeleton with its own
// animation and limb callbacks; the walk records each limb's matrix instead of drawing the limb's list, and the
// model's bones are then posed from those matrices and the rig.

namespace {
constexpr u8 kBoneMapped = 1;
constexpr u8 kBoneRoot = 2;
constexpr s32 kMaxLimbs = 100;
constexpr s32 kMaxSkeletonDepth = 4;

constexpr s32 kMaxWanted = 24;

#pragma pack(push, 1)
struct RigHeader {
    char magic[4];
    u32 version;    // 3
    u32 boneCount;
    u32 groupCount;
    f32 rootScale;  // the root's distance from the model's origin, of the game's root limb's
    f32 rootLift;   // ... plus this much height (model units): the model's ankles are not as high as the game's
    u32 nameCount;
    u32 tunicCount; // textures "<model>/tunic_<n>", put in segment 0x0A by the player's tunic
    u32 keepCount;  // limbs (one byte each, after the names) whose own lists the game goes on drawing
};
struct RigBone {
    s8 parent;
    s8 limb;
    u8 flags;
    u8 pad;
    f32 offset[3];
    f32 rot[3][3]; // rows as the game's matrices: a bone-frame vector times this is in the limb's (or parent's) frame
};
struct RigGroup {
    u16 id;
    // 1: drawn without being asked for; 2: drawn in the gauntlets' colour; 4: the group has its own light factors
    // (its material's diffuse factor in the high byte here, 0..255, and its ambient factor in `ambient`)
    u16 flags;
    s8 anchor;  // drawn only while the game draws this limb (-1: whatever it draws)
    u8 wear;    // drawn only while the player wears all of these (PORT3DS_WEAR_*)
    u8 without; // ... and none of these
    u8 ambient;
    s8 regLimb; // the limb whose colour registers the group's bound constants take (-1: it has none)
    // Of the segments that limb's list calls (8..15), the one after whose list the registers are taken; 0: as they
    // are after all of them. (A list may call two, for two parts of itself: an old man's robe and its hem.)
    u8 regSeg;
};
struct RigName {
    u64 hash;    // of a display list's resource path (0: any list the rig has no entry for, on this limb)
    s8 limb;     // when the game draws that list on this limb ...
    u8 count;    // ... the model draws these groups
    u8 segments; // bit n: that list calls the display list the actor's code put in segment 8 + n
    // 1: written with no group (the model has what the list draws in its own parts or textures: markings laid over
    // the game's model as decals, eyes drawn after the head): drawn by the actor's own code, the list is left out
    u8 flags;
    u16 groups[6];
};
// Version 4, after the kept limbs: u32 count, then one of these per limb that draws something and has a bone with
// a part of its own. How far a part reaches in 14 directions (the six axes and the eight diagonals): what the
// game's limb draws, in the limb's frame, and what the model's bone carries, in the bone's.
constexpr int kGroundPoints = 14;
struct RigGround {
    s8 limb;
    s8 bone;
    u8 pad[2];
    f32 limbPoints[kGroundPoints][3];
    f32 bonePoints[kGroundPoints][3];
};
#pragma pack(pop)

// One of the game's animations as the 3DS game has it ("<skeleton>/anims", tools/port3ds/actoranim.py): a frame
// of the game's is every bone's turn as a quaternion (s16 w, x, y, z; 32767 = 1), then the translations and the
// scales of the bones that have a track for them (f32 x, y, z each), in the bones' order.
struct NativeAnim {
    u32 frames = 0;
    // 1: the frames are the 3DS animation's own and run by the game's clock (the game has a still pose where the
    // 3DS game has a living one: a Kokiri who sits breathes)
    u16 flags = 0;
    // the game's frames it stands for (the whole animation: 0 and 0xFFFF). A file of the game's that the 3DS game
    // has as several (a Kokiri's poses, one a frame; four files there) has one of these a stretch of its frames,
    // each in its track's own frames, read from the stretch's first frame on.
    u16 first = 0;
    u16 last = 0xFFFF;
    const u8* moved = nullptr; // per bone: bit 0 its translation has a track, bit 1 its scale
    const u8* data = nullptr;
    u32 stride = 0;
    u32 withT = 0;
    u32 withS = 0;
};
constexpr size_t kMaxBones = 96;

struct SkeletonModel {
    std::vector<std::shared_ptr<Ship::IResource>> keep;
    std::vector<RigBone> bones;
    f32 rootScale = 1.0f;
    f32 rootLift = 0.0f;
    // the 3DS animations paired with the game's (PoseNative)
    std::shared_ptr<Ship::IResource> nativeBlob;
    std::vector<f32> nativeRest;                                         // per bone: translation, scale
    // hash of the game's animation's path -> its 3DS one (several: one a stretch of its frames)
    std::unordered_map<u64, std::vector<NativeAnim>> nativeAnims;
    // the game's animation -> its 3DS ones, or none
    std::unordered_map<const void*, const std::vector<NativeAnim>*> nativeByPointer;
    std::unordered_map<void**, std::vector<s8>> limbParents;             // the game's skeleton -> each limb's parent
    struct Group {
        u16 id;
        u16 flags;
        s8 anchor;
        u8 wear;
        u8 without;
        u8 ambient; // the material's light factors, 0..255 (CharacterLights)
        u8 diffuse;
        s8 regLimb;
        u8 regSeg;
        Gfx* opa;
        Gfx* xlu;
    };
    std::vector<Group> groups;
    std::vector<RigName> names;
    std::unordered_map<u64, size_t> nameByHash; // (path hash, limb) -> index into names
    std::unordered_map<u64, size_t> nameByList; // (loaded list, limb) -> index into names (see FindName)
    std::unordered_set<u64> unknownLists;       // (loaded list, limb) the rig has no entry for
    std::unordered_map<uintptr_t, size_t> nameByListAny; // loaded list -> index into names, whatever the limb
    std::unordered_set<uintptr_t> unknownAny;            // loaded lists the rig has no entry for on any limb
    u32 anyRefreshed = 0;                                // the frame the lists were last looked up for FindDrawn
    bool nameLimb[kMaxLimbs] = {};
    std::vector<RigGround> ground; // (empty: the model follows the game's root by the rig's scale and lift alone)
    bool keepLimb[kMaxLimbs] = {}; // the game draws these limbs' own lists
    bool regLimb[kMaxLimbs] = {};  // a group takes its colour registers from these limbs
    u8 regSegs[kMaxLimbs] = {};    // ... as they are after these segments' lists (bit s: segment 8 + s)
    bool usesRegs = false;
    std::vector<std::string> tunics; // "__OTR__<model>/tunic_<n>"
    Gfx* begin = nullptr;
    Gfx* end = nullptr;
    bool anyXlu = false;
};

u64 NameKey(u64 hash, s32 limb) {
    return hash ^ ((u64)(u8)limb * 0x9E3779B97F4A7C15ULL);
}

// limb table -> the skeleton's resource path (taken when the game loads a skeleton), and path -> model
std::unordered_map<void**, std::string> sSkeletonPaths;
std::unordered_map<std::string, std::unique_ptr<SkeletonModel>> sSkeletonModels;

// A model's list that has "<list>/anim" (tools/port3ds/modelconv.py: a texture the 3DS game scrolls, a colour it
// changes) is written every frame as a room's lists are (MergeShipPort3ds_SceneView).
struct AnimatedList {
    std::shared_ptr<Fast::DisplayList> list;
    ListAnims anims;
};
std::vector<AnimatedList> sAnimatedLists;
std::unordered_set<std::string> sAnimatedSeen;

void RegisterAnimated(const std::string& path) {
    if (!sAnimatedSeen.insert(path).second || !ResourceMgr_FileExists(path.c_str()) ||
        !ResourceMgr_FileExists((path + "/anim").c_str())) {
        return;
    }
    AnimatedList entry;
    entry.list = LoadList(path);
    if (entry.list == nullptr) {
        return;
    }
    LoadAnims(entry.anims, entry.list, path);
    if (entry.anims.loaded) {
        sAnimatedLists.push_back(std::move(entry));
    }
}

Gfx* LoadModelList(SkeletonModel& model, const std::string& path) {
    auto list = LoadList(path);
    if (list == nullptr) {
        return nullptr;
    }
    model.keep.push_back(list);
    RegisterAnimated(path);
    return (Gfx*)&list->Instructions[0];
}

// "<skeleton>/anims": 'P3AA', u32 version (1), u32 bone count, u32 animation count; per bone its rest translation
// and scale (6 f32); per animation { u64 hash of the game's animation's path; u32 offset; u16 frames; u16 flags };
// at each offset a byte a bone (NativeAnim::moved), padded to 4, then the frames. A table that does not fit the
// model's bones, or runs past the file, is not taken: the model is then posed from the limbs. flags: 1, the frames
// are the 3DS animation's own and run by the game's clock (a still pose's living one); 2, they are its own too, a
// track of another length than the game's animation (PoseNative lays them over the game's); else a frame is the
// game's frame.
void LoadNative(SkeletonModel& model, const std::string& prefix) {
    const std::string path = prefix + "/anims";
    if (!ResourceMgr_FileExists(path.c_str())) {
        return;
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
    const u8* data = res != nullptr ? (const u8*)res->GetRawPointer() : nullptr;
    const size_t size = res != nullptr ? res->GetPointerSize() : 0;
    u32 head[3];
    if (data == nullptr || size < 16 || memcmp(data, "P3AA", 4) != 0) {
        return;
    }
    memcpy(head, data + 4, sizeof(head));
    const u32 bones = head[1];
    const u32 count = head[2];
    size_t at = 16;
    // (version 2: each animation's entry is followed by the stretch of the game's frames it stands for)
    const size_t entry = head[0] == 2 ? 20 : 16;
    if ((head[0] != 1 && head[0] != 2) || bones == 0 || bones > kMaxBones || bones != model.bones.size() ||
        size < at + (size_t)bones * 24 + (size_t)count * entry) {
        return;
    }
    model.nativeRest.resize((size_t)bones * 6);
    memcpy(model.nativeRest.data(), data + at, (size_t)bones * 24);
    at += (size_t)bones * 24;
    for (u32 i = 0; i < count; i++, at += entry) {
        u64 hash;
        u32 offset;
        u16 frames;
        u16 flags;
        memcpy(&hash, data + at, sizeof(hash));
        memcpy(&offset, data + at + 8, sizeof(offset));
        memcpy(&frames, data + at + 12, sizeof(frames));
        memcpy(&flags, data + at + 14, sizeof(flags));
        if (frames == 0 || (size_t)offset + bones > size) {
            continue;
        }
        NativeAnim anim;
        anim.frames = frames;
        anim.flags = flags;
        if (entry == 20) {
            memcpy(&anim.first, data + at + 16, sizeof(anim.first));
            memcpy(&anim.last, data + at + 18, sizeof(anim.last));
        }
        anim.moved = data + offset;
        for (u32 b = 0; b < bones; b++) {
            anim.withT += anim.moved[b] & 1;
            anim.withS += (anim.moved[b] >> 1) & 1;
        }
        const size_t start = ((size_t)offset + bones + 3) & ~(size_t)3;
        anim.stride = bones * 8 + (anim.withT + anim.withS) * 12;
        if (start + (size_t)anim.stride * frames > size) {
            continue;
        }
        anim.data = data + start;
        model.nativeAnims[hash].push_back(anim);
    }
    if (!model.nativeAnims.empty()) {
        model.nativeBlob = res; // (kept: the tables point into it)
    } else {
        model.nativeRest.clear();
    }
}

SkeletonModel* LoadSkeletonModel(const std::string& skeletonPath) {
    auto it = sSkeletonModels.find(skeletonPath);
    if (it != sSkeletonModels.end()) {
        return it->second.get();
    }
    std::unique_ptr<SkeletonModel> model;
    const std::string prefix = "port3ds/skel/" + skeletonPath;
    const std::string rigPath = prefix + "/rig";
    if (ResourceMgr_FileExists(rigPath.c_str())) {
        auto rig = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(rigPath);
        const u8* data = rig != nullptr ? (const u8*)rig->GetRawPointer() : nullptr;
        const size_t size = rig != nullptr ? rig->GetPointerSize() : 0;
        RigHeader header;
        if (data != nullptr && size >= sizeof(header)) {
            memcpy(&header, data, sizeof(header));
            const size_t need = sizeof(header) + (size_t)header.boneCount * sizeof(RigBone) +
                                (size_t)header.groupCount * sizeof(RigGroup);
            if (memcmp(header.magic, "P3RG", 4) == 0 && (header.version == 3 || header.version == 4) &&
                header.boneCount > 0 &&
                header.boneCount <= 96 && header.nameCount <= 4096 && header.tunicCount <= 8 &&
                header.keepCount <= (u32)kMaxLimbs &&
                size >= need + (size_t)header.nameCount * sizeof(RigName) + header.keepCount) {
                model = std::make_unique<SkeletonModel>();
                model->keep.push_back(rig);
                model->rootScale = header.rootScale;
                model->rootLift = header.rootLift;
                model->bones.resize(header.boneCount);
                memcpy(model->bones.data(), data + sizeof(header), header.boneCount * sizeof(RigBone));
                const u8* groups = data + sizeof(header) + (size_t)header.boneCount * sizeof(RigBone);
                for (u32 i = 0; i < header.groupCount; i++) {
                    RigGroup g;
                    memcpy(&g, groups + i * sizeof(RigGroup), sizeof(g));
                    SkeletonModel::Group group;
                    group.id = g.id;
                    group.flags = g.flags;
                    group.anchor = g.anchor < kMaxLimbs ? g.anchor : -1;
                    group.wear = g.wear;
                    group.without = g.without;
                    // (Link's materials, and a rig that names none: 0.4 and 0.5)
                    group.ambient = (g.flags & 4) ? g.ambient : 102;
                    group.diffuse = (g.flags & 4) ? (u8)(g.flags >> 8) : 127;
                    group.regLimb = g.regLimb < kMaxLimbs ? g.regLimb : -1;
                    group.regSeg = g.regSeg >= 8 && g.regSeg < 16 ? g.regSeg : 0;
                    if (group.regLimb >= 0) {
                        model->regLimb[group.regLimb] = true;
                        model->usesRegs = true;
                        if (group.regSeg != 0) {
                            model->regSegs[group.regLimb] |= (u8)(1u << (group.regSeg - 8));
                        }
                    }
                    group.opa = LoadModelList(*model, prefix + "/g" + std::to_string(g.id) + "_opa");
                    group.xlu = LoadModelList(*model, prefix + "/g" + std::to_string(g.id) + "_xlu");
                    model->anyXlu = model->anyXlu || group.xlu != nullptr;
                    model->groups.push_back(group);
                }
                const u8* names = groups + (size_t)header.groupCount * sizeof(RigGroup);
                model->names.resize(header.nameCount);
                if (header.nameCount > 0) {
                    memcpy(model->names.data(), names, header.nameCount * sizeof(RigName));
                }
                for (size_t i = 0; i < model->names.size(); i++) {
                    RigName& name = model->names[i];
                    if (name.limb < 0 || name.limb >= kMaxLimbs || name.count > 6) {
                        name.count = 0;
                        name.limb = -1; // (a damaged entry: never looked up)
                        continue;
                    }
                    model->nameByHash[NameKey(name.hash, name.limb)] = i;
                    model->nameLimb[name.limb] = true;
                }
                const u8* keep = names + (size_t)header.nameCount * sizeof(RigName);
                for (u32 i = 0; i < header.keepCount; i++) {
                    if (keep[i] < kMaxLimbs) {
                        model->keepLimb[keep[i]] = true;
                    }
                }
                if (header.version >= 4) {
                    const u8* more = keep + header.keepCount;
                    const size_t left = size - (size_t)(more - data);
                    u32 groundCount = 0;
                    if (left >= sizeof(u32)) {
                        memcpy(&groundCount, more, sizeof(u32));
                    }
                    if (groundCount <= (u32)kMaxLimbs && left >= sizeof(u32) + (size_t)groundCount * sizeof(RigGround)) {
                        model->ground.resize(groundCount);
                        if (groundCount > 0) {
                            memcpy(model->ground.data(), more + sizeof(u32), groundCount * sizeof(RigGround));
                        }
                        for (const RigGround& g : model->ground) {
                            if (g.limb < 0 || g.limb >= kMaxLimbs || g.bone < 0 || (u32)g.bone >= header.boneCount) {
                                model->ground.clear(); // (a damaged table: the model follows the root alone)
                                break;
                            }
                        }
                    }
                }
                for (u32 i = 0; i < header.tunicCount; i++) {
                    model->tunics.push_back("__OTR__" + prefix + "/tunic_" + std::to_string(i));
                }
                model->begin = LoadModelList(*model, prefix + "/begin");
                model->end = LoadModelList(*model, prefix + "/end");
                for (const RigBone& bone : model->bones) {
                    if (bone.parent >= (s32)header.boneCount || bone.limb >= kMaxLimbs) {
                        model.reset(); // a damaged rig
                        break;
                    }
                }
                if (model != nullptr && (model->begin == nullptr || model->end == nullptr)) {
                    model.reset();
                }
                if (model != nullptr) {
                    LoadNative(*model, prefix);
                }
            }
        }
    }
    return sSkeletonModels.emplace(skeletonPath, std::move(model)).first->second.get();
}

// One skeleton being walked (they can nest: a limb callback may draw another skeleton)
struct SkeletonWalk {
    SkeletonModel* model;
    MtxF base;
    MtxF limb[kMaxLimbs];
    bool seen[kMaxLimbs];
    bool drawn[kMaxLimbs];  // the game drew a list on the limb
    Gfx* noted[kMaxLimbs];  // MergeShipPort3ds_NoteLimbList
    u16 wanted[kMaxWanted]; // groups the limbs' lists asked for
    s32 wantedCount;
    s32 regSlot[kMaxLimbs]; // where the renderer keeps the colour registers as they were at the limb (-1: nowhere)
    s32 regSlotSeg[kMaxLimbs][8]; // ... and as they were after the list in segment 8 + s (-1: nowhere)
    PlayState* play;
    bool refreshed;         // the model's loaded lists were looked up again during this walk
    // Where the display buffers stood when the walk last looked (ScanDrawn): what the limb callbacks have drawn
    // since lies between these and where the buffers stand now.
    Gfx* markOpa;
    Gfx* markXlu;
    Gfx* markSingle;
    Gfx* markOwn;           // a kept limb's own list, which the game draws next (not a list drawn by a callback)
    // the player's look (MergeShipPort3ds_SetPlayerLook), taken by the walk that follows it
    bool player;
    s32 tunic;
    u32 wear;
    u8 gauntlets[3];
    // the 3DS animations (PoseNative): the game's skeleton, and the joint table this draw poses its limbs from
    void** skeleton;
    Vec3s* joints;
};
SkeletonWalk sWalks[kMaxSkeletonDepth];
// G_REGS_EX (libultraship, interpreter.cpp): 0 keeps the colour registers in a slot, 1 chooses the slot the model's
// lists read. Slots are handed out in turn; one is read back within the frame that wrote it, long before the
// counter comes round again.
constexpr u32 kRegSlots = 4096;
constexpr u32 kRegsKeep = 0;
constexpr u32 kRegsSelect = 1;
u32 sNextRegSlot = 0;
// (`fades`, with kRegsSelect: the model is drawn where the game draws what fades)
void EmitRegs(Gfx*& gfx, u32 what, u32 slot, bool fades = false) {
    Gfx* g = gfx++;
    g->words.w0 = (0x4Fu << 24) | (what << 16) | (fades ? 1u : 0u);
    g->words.w1 = slot;
}
s32 sWalkDepth = 0;   // skeleton draws entered (with or without a model)
s32 sWalkActive = -1; // index into sWalks of the innermost one that has a model, or -1
bool sLastDrawn = false;

struct PlayerLook {
    bool set = false;
    s32 tunic = 0;
    u32 wear = 0;
    u8 gauntlets[3] = { 255, 255, 255 };
};
PlayerLook sPlayerLook;

// Where the rig's lists are loaded now (a list not seen before was asked about: the resource manager may have
// loaded lists anew). At most once a walk.
void RefreshLists(SkeletonWalk& walk) {
    SkeletonModel& model = *walk.model;
    walk.refreshed = true;
    model.nameByList.clear();
    model.unknownLists.clear();
    model.nameByListAny.clear();
    model.unknownAny.clear();
    auto archive = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager();
    for (size_t i = 0; i < model.names.size(); i++) {
        // (an entry may name no group: a list that only calls segment lists)
        if (model.names[i].limb < 0 || model.names[i].hash == 0) {
            continue;
        }
        const char* path = archive->HashToCString(model.names[i].hash);
        Gfx* loaded = path != nullptr ? ResourceMgr_LoadGfxByName(path) : nullptr;
        if (loaded != nullptr) {
            model.nameByList[NameKey((u64)(uintptr_t)loaded, model.names[i].limb)] = i;
            // (one list named on two limbs: the entry that has groups)
            auto any = model.nameByListAny.find((uintptr_t)loaded);
            if (any == model.nameByListAny.end() || model.names[any->second].count == 0) {
                model.nameByListAny[(uintptr_t)loaded] = i;
            }
        }
    }
}

// The rig's entry for a list the game draws on a limb: by its path when the game hands a path (the equipment
// screen, the first-person arms), else by the loaded list. Loaded lists are looked up by name once and again when
// one is not known (the resource manager may have loaded a list anew), at most once a walk.
const RigName* FindName(SkeletonWalk& walk, s32 limb, Gfx* list) {
    SkeletonModel& model = *walk.model;
    if (list == nullptr || !model.nameLimb[limb]) {
        return nullptr;
    }
    if (ResourceMgr_OTRSigCheck((char*)list)) {
        auto it = model.nameByHash.find(NameKey(CRC64((const char*)list + 7), limb));
        return it != model.nameByHash.end() ? &model.names[it->second] : nullptr;
    }
    const u64 key = NameKey((u64)(uintptr_t)list, limb);
    auto it = model.nameByList.find(key);
    if (it != model.nameByList.end()) {
        return &model.names[it->second];
    }
    if (model.unknownLists.count(key)) {
        return nullptr;
    }
    if (!walk.refreshed) {
        // a list not seen before: where the rig's lists are loaded now
        RefreshLists(walk);
        it = model.nameByList.find(key);
        if (it != model.nameByList.end()) {
            return &model.names[it->second];
        }
    }
    model.unknownLists.insert(key);
    return nullptr;
}

// The rig's entry for a loaded list whatever limb it names: for the lists an actor's code draws itself while its
// skeleton is walked (a guard's hair after her head, a shovel after a hand), which it may do from any limb's callback.
const RigName* FindDrawn(SkeletonWalk& walk, Gfx* list) {
    SkeletonModel& model = *walk.model;
    if (list == nullptr || model.names.empty()) {
        return nullptr;
    }
    const uintptr_t key = (uintptr_t)list;
    auto it = model.nameByListAny.find(key);
    if (it != model.nameByListAny.end()) {
        return &model.names[it->second];
    }
    if (model.unknownAny.count(key)) {
        return nullptr;
    }
    // A list not seen before. Most are simply not the rig's (an actor's code draws many lists, some made anew each
    // frame); one may be the rig's, loaded anew. The rig's lists are looked up again for it, but not more often
    // than once a second, and the lists known not to be the rig's are forgotten before they pile up.
    const u32 now = (u32)walk.play->state.frames;
    if (!walk.refreshed && (model.anyRefreshed == 0 || now - model.anyRefreshed >= 20)) {
        RefreshLists(walk);
        model.anyRefreshed = now != 0 ? now : 1;
        it = model.nameByListAny.find(key);
        if (it != model.nameByListAny.end()) {
            return &model.names[it->second];
        }
    }
    if (model.unknownAny.size() >= 1024) {
        model.unknownAny.clear();
    }
    model.unknownAny.insert(key);
    return nullptr;
}

// What the limb callbacks drew themselves since the walk last looked: a list the rig has groups for is one the
// model holds (the hair the code draws after the head limb, the shovel after the hand), so those groups are shown
// and the game's own list is taken out of the buffer. Anything else stays as the game drew it.
void ScanDrawn(SkeletonWalk& walk, Gfx** single) {
    GraphicsContext* gfxCtx = walk.play->state.gfxCtx;
    Gfx* ends[3] = { gfxCtx->polyOpa.p, gfxCtx->polyXlu.p, single != nullptr ? *single : nullptr };
    Gfx** marks[3] = { &walk.markOpa, &walk.markXlu, &walk.markSingle };
    for (int b = 0; b < 3; b++) {
        Gfx* from = *marks[b];
        Gfx* to = ends[b];
        *marks[b] = to;
        // (a buffer that was not being written, or that the game has pointed elsewhere meanwhile)
        if (from == nullptr || to == nullptr || to < from || to - from > 4096) {
            continue;
        }
        for (Gfx* g = from; g < to; g++) {
            if (((g->words.w0 >> 24) & 0xFF) != G_DL) {
                continue;
            }
            Gfx* list = (Gfx*)g->words.w1;
            if (list == walk.markOwn) {
                walk.markOwn = nullptr; // (the limb's own list, drawn by the game: the limb is kept)
                continue;
            }
            const RigName* name = FindDrawn(walk, list);
            if (name == nullptr || (name->count == 0 && !(name->flags & 1))) {
                continue;
            }
            for (int i = 0; i < name->count && walk.wantedCount < kMaxWanted; i++) {
                walk.wanted[walk.wantedCount++] = name->groups[i];
            }
            gSPNoOp(g);
        }
    }
    walk.markOwn = nullptr;
}

// (a, b as the game's matrices: a vector times a, then times b)
void MtxMul(const MtxF& a, const MtxF& b, MtxF& out) {
    MtxF r;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r.mf[i][j] = a.mf[i][0] * b.mf[0][j] + a.mf[i][1] * b.mf[1][j] + a.mf[i][2] * b.mf[2][j] +
                         a.mf[i][3] * b.mf[3][j];
        }
    }
    out = r;
}

// The game's light for a character, re-bound the way the 3DS game lights one. Its vertex shader gives
// ambient factor * scene ambient + diffuse factor * (each light's colour by how far the surface faces it), the
// factors being the material's own (`ambientFactor`, `diffuseFactor`). Link's materials have 0.4 and 0.5 and a
// texture combiner that doubles the result: a surface turned away from every light shows its texture at 0.8 of the
// scene's ambient, and one facing the sun gains the sun's full colour on top. About half of the other actors'
// materials have 1 and 1 with a combiner that does not double. The scene's 3DS light settings are used when the pack has them (they are retuned: higher ambients,
// other colours), else the game's own; point lights (torches) come from the game either way.
//
// The scene ambient counts once here. (Rooms take it twice, MergeShipPort3ds_RelightRoom. Measured against the
// game's own hint videos, which are captures of the 3DS game: in Kokiri Forest by day, ambient 181 of 255, the
// shaded side of Link's arm is at 0.75 of his skin texture and the sunlit top at 1.4; at Hyrule Castle, ambient
// 130, 0.5 and 1.2. Counted twice he would never be darker than 1.14 times his texture in Kokiri.)
//
// CharacterLights makes the binding; EmitLights writes it into one display buffer (the game's Lights_Draw writes
// into both the opaque and the translucent buffer at once, which a skeleton drawn into a buffer of the caller's
// cannot use).
Lights* CharacterLights(PlayState* play, const Vec3f& where, bool forCharacter, f32 ambientFactor = 0.4f,
                        f32 diffuseFactor = 0.5f) {
    Vec3f pos = where;
    Lights* lights = LightContext_NewLights(&play->lightCtx, play->state.gfxCtx);
    Lights_BindAll(lights, play->lightCtx.listHead, &pos);
    if (forCharacter) {
        EnvLightSettings ds;
        const bool have = sSceneLightValid;
        if (have) {
            ds = sSceneLight;
        }
        for (int c = 0; c < 3; c++) {
            // (without 3DS settings: the game's own ambient and lights by the diffuse factor, which a doubling
            // combiner turns back into the game's own light)
            f32 ambient = have ? ds.ambientColor[c] * ambientFactor : lights->l.a.l.col[c] * diffuseFactor;
            lights->l.a.l.col[c] = lights->l.a.l.colc[c] = (u8)(ambient > 255.0f ? 255.0f : ambient);
        }
        const LightParams* env[2] = { &play->envCtx.dirLight1.params, &play->envCtx.dirLight2.params };
        for (int i = 0; i < lights->numLights; i++) {
            Light* light = &lights->l.l[i];
            int which = -1;
            for (int k = 0; k < 2; k++) {
                if (light->l.dir[0] == env[k]->dir.x && light->l.dir[1] == env[k]->dir.y &&
                    light->l.dir[2] == env[k]->dir.z && light->l.col[0] == env[k]->dir.color[0] &&
                    light->l.col[1] == env[k]->dir.color[1] && light->l.col[2] == env[k]->dir.color[2]) {
                    which = k;
                    break;
                }
            }
            for (int c = 0; c < 3; c++) {
                f32 color = light->l.col[c];
                if (have && which >= 0) {
                    // (the 3DS setting's colour, and its direction: the sun's where the 3DS game lights by the time
                    // of day a scene this game lights with a fixed setting)
                    color = which == 0 ? ds.light1Color[c] : ds.light2Color[c];
                    light->l.dir[c] = which == 0 ? ds.light1Dir[c] : ds.light2Dir[c];
                }
                color *= diffuseFactor;
                light->l.col[c] = light->l.colc[c] = (u8)(color > 255.0f ? 255.0f : color);
            }
        }
    }
    return lights;
}

void EmitLights(Lights* lights, Gfx*& gfx) {
    gSPNumLights(gfx++, lights->numLights);
    for (int i = 0; i < lights->numLights; i++) {
        gSPLight(gfx++, &lights->l.l[i], i + 1);
    }
    gSPLight(gfx++, &lights->l.a, lights->numLights + 1); // (the ambient light comes after the others)
}
} // namespace

extern "C" void MergeShipPort3ds_OnSkeletonLoaded(const char* path, void** skeleton) {
    if (path != nullptr && skeleton != nullptr) {
        sSkeletonPaths[skeleton] = path;
    }
}

// The pack's path for a room ("scenes/.../<room>"; the Master Quest one for a Master Quest scene), or "" for none.
static std::string RoomPath(PlayState* play, Room* room) {
    if (room->num < 0 || room->num >= play->numRooms) {
        return "";
    }
    const char* file = play->roomList[room->num].fileName;
    if (file == nullptr) {
        return "";
    }
    std::string path = file;
    if (path.compare(0, 7, "__OTR__") == 0) {
        path = path.substr(7);
    }
    if (ResourceMgr_IsSceneMasterQuest(play->sceneNum)) {
        size_t pos = path.find("/nonmq/");
        if (pos != std::string::npos) {
            path.replace(pos, 7, "/mq/");
        }
    }
    return path;
}

// Objects: the game's list path -> the pack's list for it (null: the pack has none). The option is read once a
// frame (the lists are asked for by name many times a frame).
namespace {
bool sObjectsOn = false;
std::unordered_map<std::string, std::shared_ptr<Fast::DisplayList>> sObjectLists;
// The scene's 3DS light settings where the renderer reads them (G_REGS_EX 7 and 6, interpreter.cpp): a model that
// stands in for a plain list is lit by them and its material's factors, as CharacterLights lights a character.
struct SceneLightsEx {
    u8 valid;
    u8 ambient[3];
    u8 color1[3];
    s8 dir1[3];
    u8 color2[3];
    s8 dir2[3];
};
SceneLightsEx sSceneLightsEx;
} // namespace

extern "C" Gfx* MergeShipPort3ds_ObjectList(const char* path) {
    if (!sObjectsOn || path == nullptr) {
        return nullptr;
    }
    const char* plain = strncmp(path, "__OTR__", 7) == 0 ? path + 7 : path;
    auto it = sObjectLists.find(plain);
    if (it == sObjectLists.end()) {
        const std::string answer = std::string("port3ds/dl/") + plain;
        it = sObjectLists.emplace(plain, LoadList(answer)).first;
        // (the lists of its model that are written every frame: "<answer>/animated" names them, a line each)
        const std::string index = answer + "/animated";
        if (it->second != nullptr && ResourceMgr_FileExists(index.c_str())) {
            auto blob = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(index);
            const char* text = blob != nullptr ? (const char*)blob->GetRawPointer() : nullptr;
            const size_t size = blob != nullptr ? blob->GetPointerSize() : 0;
            // (a line each; the blob may be padded with zeros after the text)
            size_t start = 0;
            for (size_t i = 0; text != nullptr && i <= size; i++) {
                const bool end = i == size || text[i] == '\0';
                if (end || text[i] == '\n' || text[i] == '\r') {
                    if (i > start) {
                        RegisterAnimated(std::string(text + start, i - start));
                    }
                    start = i + 1;
                    if (end) {
                        break;
                    }
                }
            }
        }
    }
    return it->second != nullptr ? (Gfx*)&it->second->Instructions[0] : nullptr;
}

extern "C" void MergeShipPort3ds_SceneView(PlayState* play) {
    sObjectsOn = CVarGetInteger(CVAR_PORT3DS_OBJECTS_NAME, 0) != 0 && BackendSupported();
    if (!sAnimatedLists.empty()) {
        // the models' own material animations, a step a game frame (as the rooms': MergeShipPort3ds_RoomLists)
        Clock clock;
        clock.hour = gSaveContext.dayTime * (24.0f / 65536.0f);
        clock.night = gSaveContext.nightFlag != 0;
        clock.adult = LINK_IS_ADULT;
        for (AnimatedList& entry : sAnimatedLists) {
            Animate(entry.anims, entry.list, play->gameplayFrames, clock);
        }
    }
    // (the room draw of the frame before says whether a 3DS room of such a scene is on screen)
    if (!sRoomDrawn || !sSceneByTime || !CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0)) {
        return;
    }
    if (play->lightCtx.fogFar < 12800) {
        play->lightCtx.fogFar = 12800;
    }
    if (play->lightCtx.fogNear < 996) {
        play->lightCtx.fogNear = 996;
    }
}

extern "C" void MergeShipPort3ds_UpdateSceneLight(PlayState* play, Room* room) {
    sSceneLightValid = false;
    sSceneByTime = false;
    sSceneLightsEx.valid = 0;
    if ((!CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0) && !CVarGetInteger(CVAR_PORT3DS_MODELS_NAME, 0) &&
         !CVarGetInteger(CVAR_PORT3DS_OBJECTS_NAME, 0)) ||
        !BackendSupported()) {
        return;
    }
    {
        // (the renderer is told where the settings are before anything of this frame's scene is drawn; what they
        // are is filled in below)
        Gfx* g = play->state.gfxCtx->polyOpa.p++;
        g->words.w0 = (0x4Fu << 24) | (7u << 16);
        g->words.w1 = (uintptr_t)&sSceneLightsEx;
    }
    const std::string path = RoomPath(play, room);
    if (path.empty() || path.rfind('/') == std::string::npos) {
        return;
    }
    // (a scene setup without a header of its own runs the scene's first header)
    const std::string folder = "port3ds/" + path.substr(0, path.rfind('/')) + "/lights_";
    const SceneLights* lights = &LoadLights(folder + std::to_string((int)gSaveContext.sceneLayer));
    if (lights->list == nullptr) {
        lights = &LoadLights(folder + "0");
    }
    // (indoors the floor under Link says the setting where the pack has the floors, else the game's light index)
    const bool byFloor = lights->list != nullptr && !lights->byTime &&
                         FloorLightOf(play, "port3ds/" + path.substr(0, path.rfind('/')) + "/", &sSceneLight);
    if (lights->list != nullptr &&
        (byFloor || (lights->byTime ? Environment_BlendLightsByTime(play, lights->list, lights->count, &sSceneLight)
                                    : Environment_BlendLightsOf(play, lights->list, lights->count, &sSceneLight)))) {
        sSceneLightValid = true;
        sSceneLightsEx.valid = 1;
        for (int c = 0; c < 3; c++) {
            sSceneLightsEx.ambient[c] = sSceneLight.ambientColor[c];
            sSceneLightsEx.color1[c] = sSceneLight.light1Color[c];
            sSceneLightsEx.dir1[c] = sSceneLight.light1Dir[c];
            sSceneLightsEx.color2[c] = sSceneLight.light2Color[c];
            sSceneLightsEx.dir2[c] = sSceneLight.light2Dir[c];
        }
    }
    sSceneByTime = lights->list != nullptr && lights->byTime;
    {
        // (the test harness's log: what the scene light was made from, whenever that changes; a no-op in play)
        static u32 sLogged = 0xFFFFFFFF;
        const EnvironmentContext* env = &play->envCtx;
        const u32 signature =
            (u32)(gSaveContext.sceneLayer & 0xF) | ((u32)env->unk_BF << 4) | ((u32)(env->unk_1F & 0xF) << 12) |
            ((u32)(env->unk_20 & 0xF) << 16) | ((u32)(env->unk_21 != 0) << 20) | ((u32)sSceneLightValid << 21) |
            ((u32)(sSceneLight.ambientColor[0] >> 5) << 22) | ((u32)(sSceneLight.ambientColor[1] >> 5) << 25) |
            ((u32)(sSceneLight.ambientColor[2] >> 5) << 28);
        if (signature != sLogged) {
            sLogged = signature;
            MergeShipTest_Log("P3DLIGHT layer=%d count=%d BF=0x%02X BD=%d BE=%d config=%d->%d blending=%d %d/%d "
                              "indoors=%d sky=0x%04X day=0x%04X valid=%d ambient=(%d,%d,%d) light1=(%d,%d,%d) "
                              "light2=(%d,%d,%d) game ambient=(%d,%d,%d) adjust=(%d,%d,%d)",
                              (int)gSaveContext.sceneLayer, (int)lights->count, env->unk_BF, env->unk_BD, env->unk_BE,
                              env->unk_1F, env->unk_20, env->unk_21, env->unk_22, env->unk_24, env->indoors,
                              gSaveContext.skyboxTime, gSaveContext.dayTime, (int)sSceneLightValid,
                              sSceneLight.ambientColor[0], sSceneLight.ambientColor[1], sSceneLight.ambientColor[2],
                              sSceneLight.light1Color[0], sSceneLight.light1Color[1], sSceneLight.light1Color[2],
                              sSceneLight.light2Color[0], sSceneLight.light2Color[1], sSceneLight.light2Color[2],
                              env->lightSettings.ambientColor[0], env->lightSettings.ambientColor[1],
                              env->lightSettings.ambientColor[2], env->adjAmbientColor[0], env->adjAmbientColor[1],
                              env->adjAmbientColor[2]);
        }
    }
}

extern "C" s32 MergeShipPort3ds_SceneLight(EnvLightSettings* out) {
    if (!sSceneLightValid) {
        return false;
    }
    *out = sSceneLight;
    return true;
}

// "port3ds/<scene folder>/sky_<setup>": the 3DS scene's sky id, sky setting and light mode (three bytes). The 3DS
// ids that name a sky this game has: 1 the sky of the time of day, 4 the overcast sunset.
namespace {
void ForgetSkelAnimes(); // (the 3DS animations, below)
} // namespace

extern "C" s16 MergeShipPort3ds_SceneSkybox(PlayState* play, s16 skyboxId) {
    // (a scene is being set up: every actor of the last one is gone, and with it whose joint table was whose)
    ForgetSkelAnimes();
    if (!CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0) || !BackendSupported() || play->numRooms <= 0 ||
        play->roomList == nullptr || play->roomList[0].fileName == nullptr) {
        return skyboxId;
    }
    std::string path = play->roomList[0].fileName;
    if (path.compare(0, 7, "__OTR__") == 0) {
        path = path.substr(7);
    }
    if (ResourceMgr_IsSceneMasterQuest(play->sceneNum)) {
        size_t pos = path.find("/nonmq/");
        if (pos != std::string::npos) {
            path.replace(pos, 7, "/mq/");
        }
    }
    if (path.rfind('/') == std::string::npos) {
        return skyboxId;
    }
    const std::string folder = "port3ds/" + path.substr(0, path.rfind('/')) + "/sky_";
    std::string name = folder + std::to_string((int)gSaveContext.sceneLayer);
    if (!ResourceMgr_FileExists(name.c_str())) {
        name = folder + "0";
        if (!ResourceMgr_FileExists(name.c_str())) {
            return skyboxId;
        }
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(name);
    if (res == nullptr || res->GetInitData()->Type != static_cast<uint32_t>(Ship::ResourceType::Blob) ||
        res->GetRawPointer() == nullptr || res->GetPointerSize() < 3) {
        return skyboxId;
    }
    const u8 sky = ((const u8*)res->GetRawPointer())[0];
    if (sky == 1) {
        return SKYBOX_NORMAL_SKY;
    }
    if (sky == 4) {
        return SKYBOX_OVERCAST_SUNSET;
    }
    return skyboxId;
}

// "port3ds/<room path>/sky_<setup>": the 3DS room's two switches (sky off, sun and moon off).
extern "C" void MergeShipPort3ds_RoomSkyboxDisables(PlayState* play, u8* skyboxDisabled, u8* sunMoonDisabled) {
    if (!CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0) || !BackendSupported()) {
        return;
    }
    const std::string path = RoomPath(play, &play->roomCtx.curRoom);
    if (path.empty()) {
        return;
    }
    const std::string base = "port3ds/" + path + "/sky_";
    std::string name = base + std::to_string((int)gSaveContext.sceneLayer);
    if (!ResourceMgr_FileExists(name.c_str())) {
        name = base + "0";
        if (!ResourceMgr_FileExists(name.c_str())) {
            return;
        }
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(name);
    if (res == nullptr || res->GetInitData()->Type != static_cast<uint32_t>(Ship::ResourceType::Blob) ||
        res->GetRawPointer() == nullptr || res->GetPointerSize() < 2) {
        return;
    }
    const u8* data = (const u8*)res->GetRawPointer();
    *skyboxDisabled = data[0] != 0;
    *sunMoonDisabled = data[1] != 0;
}

// Called where a skeleton draw starts (the model matrix is the current matrix). Always paired with SkeletonEnd.
extern "C" void MergeShipPort3ds_SkeletonBegin(PlayState* play, void** skeleton) {
    const s32 depth = sWalkDepth++;
    if (depth >= kMaxSkeletonDepth) {
        return;
    }
    sWalks[depth].model = nullptr;
    if (!CVarGetInteger(CVAR_PORT3DS_MODELS_NAME, 0) || !BackendSupported()) {
        return;
    }
    auto it = sSkeletonPaths.find(skeleton);
    if (it == sSkeletonPaths.end()) {
        return;
    }
    SkeletonModel* model = LoadSkeletonModel(it->second);
    if (model == nullptr) {
        return;
    }
    SkeletonWalk& walk = sWalks[depth];
    walk.model = model;
    walk.skeleton = skeleton;
    walk.joints = nullptr;
    Matrix_Get(&walk.base);
    memset(walk.seen, 0, sizeof(walk.seen));
    memset(walk.drawn, 0, sizeof(walk.drawn));
    memset(walk.noted, 0, sizeof(walk.noted));
    walk.wantedCount = 0;
    memset(walk.regSlot, 0xFF, sizeof(walk.regSlot));
    memset(walk.regSlotSeg, 0xFF, sizeof(walk.regSlotSeg));
    walk.play = play;
    walk.refreshed = false;
    walk.markOpa = play->state.gfxCtx->polyOpa.p;
    walk.markXlu = play->state.gfxCtx->polyXlu.p;
    walk.markSingle = nullptr;
    walk.markOwn = nullptr;
    walk.player = sPlayerLook.set;
    walk.tunic = sPlayerLook.tunic;
    walk.wear = sPlayerLook.set ? sPlayerLook.wear : 0;
    memcpy(walk.gauntlets, sPlayerLook.gauntlets, sizeof(walk.gauntlets));
    sPlayerLook.set = false;
    sWalkActive = depth;
}

// Called for each limb once its matrix is the current matrix. Returns 1 when a model draws this skeleton (the
// limb's own list is then left out). `single`: the display buffer the skeleton is drawn into, when it is not the
// frame's opaque buffer.
extern "C" void MergeShipTest_SkeletonLimb(void); // (test harness: where the joints of the actor being drawn are)
extern "C" void SkelAnime_GetFrameData(AnimationHeader* animation, s32 frame, s32 limbCount, Vec3s* frameTable);

extern "C" s32 MergeShipPort3ds_SkeletonLimbGfx(s32 limb, Gfx* dList, Gfx** single) {
    MergeShipTest_SkeletonLimb();
    if (sWalkActive < 0 || sWalkActive != sWalkDepth - 1) {
        return 0;
    }
    SkeletonWalk& walk = sWalks[sWalkActive];
    bool kept = false;
    // (what the callbacks of the limbs before this one drew themselves)
    if (single != nullptr && walk.markSingle == nullptr) {
        walk.markSingle = *single;
    }
    ScanDrawn(walk, single);
    if (limb >= 0 && limb < kMaxLimbs) {
        Matrix_Get(&walk.limb[limb]);
        walk.seen[limb] = true;
        walk.drawn[limb] = dList != nullptr;
        kept = walk.model->keepLimb[limb];
        u32 segments = 0;
        if (dList != nullptr) {
            const RigName* name = FindName(walk, limb, walk.noted[limb] != nullptr ? walk.noted[limb] : dList);
            if (name == nullptr && walk.noted[limb] != nullptr) {
                name = FindName(walk, limb, dList);
            }
            if (name != nullptr) {
                segments = name->segments; // (of the list the game draws: its own entry, not the stand-in's)
            }
            if (name == nullptr && walk.model->nameLimb[limb]) {
                auto it = walk.model->nameByHash.find(NameKey(0, limb));
                if (it != walk.model->nameByHash.end()) {
                    name = &walk.model->names[it->second];
                }
            }
            for (int i = 0; name != nullptr && i < name->count && walk.wantedCount < kMaxWanted; i++) {
                walk.wanted[walk.wantedCount++] = name->groups[i];
            }
        }
        if (walk.model->regLimb[limb]) {
            Gfx*& disp = single != nullptr ? *single : walk.play->state.gfxCtx->polyOpa.p;
            // The limb's own list would have run the lists the actor's code put in these segments (this person's
            // colours); the model draws without the limb's list, so they run here. Only the segments the list the
            // game is drawing calls: nothing says what is in any other.
            for (u32 s = 0; s < 8; s++) {
                if (segments & (1u << s)) {
                    gSPDisplayList(disp++, (Gfx*)(uintptr_t)(((8 + s) << 24) | 1));
                    if (walk.model->regSegs[limb] & (1u << s)) {
                        // (a part of the model takes the registers as this segment's list leaves them)
                        walk.regSlotSeg[limb][s] = (s32)(sNextRegSlot++ % (kRegSlots - 1));
                        EmitRegs(disp, kRegsKeep, (u32)walk.regSlotSeg[limb][s]);
                    }
                }
            }
            // the colour registers as the actor's code has them for this limb (the last slot is the objects' own:
            // a list that stands for one of the game's keeps the code's colours there, tools/port3ds/objects.py)
            walk.regSlot[limb] = (s32)(sNextRegSlot++ % (kRegSlots - 1));
            EmitRegs(disp, kRegsKeep, (u32)walk.regSlot[limb]);
        }
    }
    {
        Gfx*& disp = single != nullptr ? *single : walk.play->state.gfxCtx->polyOpa.p;
        if (!kept && dList != nullptr) {
            // The game loads a limb's matrix before the limb's list, and what the limb's callback then draws
            // itself is drawn with that matrix. The list is left out; the matrix is loaded all the same.
            gSPMatrix(disp++, Matrix_NewMtx(walk.play->state.gfxCtx, (char*)__FILE__, __LINE__), G_MTX_LOAD);
        }
        // from here on the buffers hold what the game and this limb's callback draw
        walk.markOpa = walk.play->state.gfxCtx->polyOpa.p;
        walk.markXlu = walk.play->state.gfxCtx->polyXlu.p;
        if (single != nullptr) {
            walk.markSingle = *single;
        }
        walk.markOwn = nullptr;
        if (kept && dList != nullptr) {
            walk.markOwn = ResourceMgr_OTRSigCheck((char*)dList) ? ResourceMgr_LoadGfxByName((const char*)dList) : dList;
        }
    }
    return kept ? 0 : 1;
}

extern "C" s32 MergeShipPort3ds_SkeletonLimb(s32 limb, Gfx* dList) {
    return MergeShipPort3ds_SkeletonLimbGfx(limb, dList, nullptr);
}

extern "C" void MergeShipPort3ds_NoteLimbList(s32 limb, Gfx* dList) {
    if (sWalkActive < 0 || sWalkActive != sWalkDepth - 1 || limb < 0 || limb >= kMaxLimbs) {
        return;
    }
    sWalks[sWalkActive].noted[limb] = dList;
}

extern "C" s32 MergeShipPort3ds_SkeletonDrawn(void) {
    return sLastDrawn;
}

extern "C" void MergeShipPort3ds_SetPlayerLook(s32 tunic, u32 wear, u8 r, u8 g, u8 b) {
    sPlayerLook.set = true;
    sPlayerLook.tunic = tunic;
    sPlayerLook.wear = wear;
    sPlayerLook.gauntlets[0] = r;
    sPlayerLook.gauntlets[1] = g;
    sPlayerLook.gauntlets[2] = b;
}

// ---------------------------------------------------------------------------------------------------------------
// The 3DS game's own animations (MergeShipPort3ds.h; tools/port3ds/actoranim.py).
namespace {
// Which joint table is whose: the game hands a skeleton draw the table, not the SkelAnime that fills it. An entry
// may outlive its actor (most never free their SkelAnime); it is believed only while the SkelAnime it names still
// has that table and the skeleton being drawn.
std::unordered_map<const Vec3s*, SkelAnime*> sSkelAnimes;

struct NativePose {
    f32 q[kMaxBones][4]; // w, x, y, z
    f32 t[kMaxBones][3];
    f32 s[kMaxBones][3];
};
// What a SkelAnime's model showed last, for the blend from one animation into the next.
struct NativeState {
    const void* animation = nullptr;
    f32 weight = 0.0f;
    u32 frame = 0;
    bool valid = false;
    NativePose pose;
};
std::unordered_map<const SkelAnime*, NativeState> sNativeStates;
u32 sNativePruned = 0;

void ForgetSkelAnimes() {
    sSkelAnimes.clear();
    sNativeStates.clear();
    sNativePruned = 0;
}

void SampleNative(const SkeletonModel& model, const NativeAnim& anim, u32 frame, NativePose& out) {
    const size_t bones = model.bones.size();
    const u8* q = anim.data + (size_t)anim.stride * frame;
    const u8* t = q + bones * 8;
    const u8* s = t + (size_t)anim.withT * 12;
    for (size_t b = 0; b < bones; b++) {
        s16 raw[4];
        memcpy(raw, q + b * 8, sizeof(raw));
        for (int k = 0; k < 4; k++) {
            out.q[b][k] = raw[k] * (1.0f / 32767.0f);
        }
        if (anim.moved[b] & 1) {
            memcpy(out.t[b], t, 12);
            t += 12;
        } else {
            memcpy(out.t[b], &model.nativeRest[b * 6], 12);
        }
        if (anim.moved[b] & 2) {
            memcpy(out.s[b], s, 12);
            s += 12;
        } else {
            memcpy(out.s[b], &model.nativeRest[b * 6 + 3], 12);
        }
    }
}

// (k of the way from a to b; out may be either)
void BlendNative(const NativePose& a, const NativePose& b, f32 k, size_t bones, NativePose& out) {
    for (size_t i = 0; i < bones; i++) {
        const f32 dot = a.q[i][0] * b.q[i][0] + a.q[i][1] * b.q[i][1] + a.q[i][2] * b.q[i][2] + a.q[i][3] * b.q[i][3];
        const f32 sign = dot < 0.0f ? -1.0f : 1.0f; // (the short way round)
        f32 q[4];
        f32 length = 0.0f;
        for (int c = 0; c < 4; c++) {
            q[c] = a.q[i][c] + (sign * b.q[i][c] - a.q[i][c]) * k;
            length += q[c] * q[c];
        }
        length = length > 1e-12f ? 1.0f / sqrtf(length) : 0.0f;
        for (int c = 0; c < 4; c++) {
            out.q[i][c] = length > 0.0f ? q[c] * length : (c == 0 ? 1.0f : 0.0f);
        }
        for (int c = 0; c < 3; c++) {
            out.t[i][c] = a.t[i][c] + (b.t[i][c] - a.t[i][c]) * k;
            out.s[i][c] = a.s[i][c] + (b.s[i][c] - a.s[i][c]) * k;
        }
    }
}

// 3 x 3 turns as m[row][column], for column vectors (a point turned is m times the point).
struct Turn {
    f32 m[3][3];
};

Turn TurnMul(const Turn& a, const Turn& b) {
    Turn r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
        }
    }
    return r;
}

Turn TurnTranspose(const Turn& a) {
    Turn r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = a.m[j][i];
        }
    }
    return r;
}

// How one of the game's matrices is turned (its three axes, each made one long); false: an axis has no length or
// the matrix mirrors.
bool TurnOf(const MtxF& mf, Turn& out) {
    for (int c = 0; c < 3; c++) {
        const f32 length = sqrtf(mf.mf[c][0] * mf.mf[c][0] + mf.mf[c][1] * mf.mf[c][1] + mf.mf[c][2] * mf.mf[c][2]);
        if (length < 1e-9f) {
            return false;
        }
        for (int r = 0; r < 3; r++) {
            out.m[r][c] = mf.mf[c][r] / length;
        }
    }
    const f32 det = out.m[0][0] * (out.m[1][1] * out.m[2][2] - out.m[1][2] * out.m[2][1]) -
                    out.m[0][1] * (out.m[1][0] * out.m[2][2] - out.m[1][2] * out.m[2][0]) +
                    out.m[0][2] * (out.m[1][0] * out.m[2][1] - out.m[1][1] * out.m[2][0]);
    return det > 0.0f;
}

// A limb's turn in a joint table: binary angles about X, then Y, then Z (Matrix_TranslateRotateZYX).
Turn TurnOfJoint(const Vec3s& rot) {
    const f32 x = rot.x * (f32)(M_PI / 32768.0);
    const f32 y = rot.y * (f32)(M_PI / 32768.0);
    const f32 z = rot.z * (f32)(M_PI / 32768.0);
    const f32 cx = cosf(x), sx = sinf(x), cy = cosf(y), sy = sinf(y), cz = cosf(z), sz = sinf(z);
    Turn r = { { { cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz },
                 { cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz },
                 { -sy, sx * cy, cx * cy } } };
    return r;
}

void QuaternionOf(const Turn& r, f32 q[4]) {
    const f32 trace = r.m[0][0] + r.m[1][1] + r.m[2][2];
    if (trace > 0.0f) {
        const f32 s = sqrtf(trace + 1.0f) * 2.0f;
        q[0] = 0.25f * s;
        q[1] = (r.m[2][1] - r.m[1][2]) / s;
        q[2] = (r.m[0][2] - r.m[2][0]) / s;
        q[3] = (r.m[1][0] - r.m[0][1]) / s;
    } else if (r.m[0][0] > r.m[1][1] && r.m[0][0] > r.m[2][2]) {
        const f32 s = sqrtf(1.0f + r.m[0][0] - r.m[1][1] - r.m[2][2]) * 2.0f;
        q[0] = (r.m[2][1] - r.m[1][2]) / s;
        q[1] = 0.25f * s;
        q[2] = (r.m[0][1] + r.m[1][0]) / s;
        q[3] = (r.m[0][2] + r.m[2][0]) / s;
    } else if (r.m[1][1] > r.m[2][2]) {
        const f32 s = sqrtf(1.0f + r.m[1][1] - r.m[0][0] - r.m[2][2]) * 2.0f;
        q[0] = (r.m[0][2] - r.m[2][0]) / s;
        q[1] = (r.m[0][1] + r.m[1][0]) / s;
        q[2] = 0.25f * s;
        q[3] = (r.m[1][2] + r.m[2][1]) / s;
    } else {
        const f32 s = sqrtf(1.0f + r.m[2][2] - r.m[0][0] - r.m[1][1]) * 2.0f;
        q[0] = (r.m[1][0] - r.m[0][1]) / s;
        q[1] = (r.m[0][2] + r.m[2][0]) / s;
        q[2] = (r.m[1][2] + r.m[2][1]) / s;
        q[3] = 0.25f * s;
    }
}

// Each limb's parent in the game's skeleton (-1: it hangs from the actor itself, as the root limb and its
// siblings do).
void LimbParents(void** skeleton, s32 count, std::vector<s8>& out) {
    out.assign((size_t)count, -1);
    // (child first, then sibling: a stack of (limb, parent) in place of the game's recursion)
    std::vector<std::pair<s32, s32>> stack;
    stack.emplace_back(0, -1);
    s32 guard = 0;
    while (!stack.empty() && guard++ < 4 * kMaxLimbs) {
        const auto [limb, parent] = stack.back();
        stack.pop_back();
        if (limb < 0 || limb >= count || skeleton[limb] == nullptr) {
            continue;
        }
        out[(size_t)limb] = (s8)parent;
        const StandardLimb* data = (const StandardLimb*)skeleton[limb];
        if (data->sibling != LIMB_DONE) {
            stack.emplace_back(data->sibling, parent);
        }
        if (data->child != LIMB_DONE) {
            stack.emplace_back(data->child, limb);
        }
    }
}

// Poses the model's bones from the 3DS animation paired with what the walk's SkelAnime is playing. False: there is
// none (the option is off, the table is nobody's, the animation has no 3DS one), and the limbs pose the model.
bool PoseNative(SkeletonWalk& walk, SkeletonModel& model, MtxF* bone) {
    if (model.nativeAnims.empty() || walk.player || walk.joints == nullptr ||
        !CVarGetInteger(CVAR_PORT3DS_ANIMATIONS_NAME, 0)) {
        return false;
    }
    auto owner = sSkelAnimes.find(walk.joints);
    if (owner == sSkelAnimes.end()) {
        return false;
    }
    SkelAnime* skelAnime = owner->second;
    if (skelAnime->jointTable != walk.joints || skelAnime->skeleton != walk.skeleton || skelAnime->animation == nullptr ||
        skelAnime->limbCount < 2 || skelAnime->limbCount > kMaxLimbs) {
        return false;
    }
    const std::vector<NativeAnim>* paired = nullptr;
    auto known = model.nativeByPointer.find(skelAnime->animation);
    if (known != model.nativeByPointer.end()) {
        paired = known->second;
    } else {
        if (ResourceMgr_OTRSigCheck((char*)skelAnime->animation)) {
            auto found = model.nativeAnims.find(CRC64((const char*)skelAnime->animation + 7));
            if (found != model.nativeAnims.end()) {
                paired = &found->second;
            }
        }
        model.nativeByPointer.emplace(skelAnime->animation, paired);
    }
    if (paired == nullptr) {
        return false;
    }
    f32 at = skelAnime->curFrame;
    if (!(at > 0.0f)) {
        at = 0.0f;
    }
    // (the one that stands for the game's frame: a file that is several of the 3DS game's has one a stretch)
    const NativeAnim* anim = nullptr;
    for (const NativeAnim& one : *paired) {
        if ((u32)at >= one.first && (u32)at <= one.last) {
            anim = &one;
            break;
        }
    }
    if (anim == nullptr) {
        return false;
    }
    const size_t bones = model.bones.size();
    const s32 limbs = skelAnime->limbCount - 1;

    // the animation at the game's frame (between two frames: between the two; a loop's last frame leads to its first)
    const u32 now = walk.play->gameplayFrames;
    const bool ownClock = (anim->flags & 1) != 0;
    const bool stretch = anim->first != 0 || anim->last != 0xFFFF; // (a stretch of the game's frames, in its own)
    NativePose target;
    const u32 last = anim->frames - 1;
    const f32 gameAt = at;
    if (stretch) {
        at -= (f32)anim->first;
    }
    u32 first = (u32)at;
    if (first > last) {
        first = last;
    }
    f32 part = at - (f32)first;
    u32 next = first < last ? first + 1 : (skelAnime->mode <= ANIMMODE_LOOP_INTERP ? 0 : last);
    // (the game's own frame, for what its code does to the limbs below: counted in the game's animation, which a
    // track in its own frames is not as long as)
    const s32 gameLast = (s32)Animation_GetLastFrame(skelAnime->animation);
    u32 gameFirst = (u32)gameAt;
    if (gameLast >= 0 && gameFirst > (u32)gameLast) {
        gameFirst = (u32)gameLast;
    }
    const f32 gamePart = gameAt - (f32)gameFirst;
    const u32 gameNext = (s32)gameFirst < gameLast ? gameFirst + 1
                                                    : (skelAnime->mode <= ANIMMODE_LOOP_INTERP ? 0 : gameFirst);
    // (flag 2: a track of another length than the game's animation, kept in its own frames. The 3DS game retimed
    // these: a Tektite's wait is 120 frames for the game's 20, an old man's walk 60 for 20. Where the game loops
    // the animation the track runs at its own pace, as the 3DS game plays it; anything else, a motion played once
    // or a frame the code holds, is laid over the game's length, so that it ends where the game's does.)
    const bool ownFrames = (anim->flags & 2) != 0;
    if (ownClock || (ownFrames && skelAnime->mode <= ANIMMODE_LOOP_INTERP)) {
        // a living pose: round and round by the game's clock, each actor from a frame of its own
        first = (now + (u32)(((uintptr_t)skelAnime >> 4) & 0xFFFF)) % anim->frames;
        next = first;
        part = 0.0f;
    } else if (ownFrames) {
        const f32 span = skelAnime->animLength > 1.0f ? skelAnime->animLength - 1.0f : 1.0f;
        f32 own = at * (f32)last / span;
        if (own > (f32)last) {
            own = (f32)last;
        }
        first = (u32)own;
        part = own - (f32)first;
        next = first < last ? first + 1 : last;
    }
    SampleNative(model, *anim, first, target);
    if (part > 0.001f && part < 1.0f && next != first) {
        NativePose after;
        SampleNative(model, *anim, next, after);
        BlendNative(target, after, part, bones, target);
    }

    // from one animation into the next, as the game blends its own joint table: each frame the pose shown goes on
    // towards the new animation by the share of the blend that has run since the frame before
    NativeState& state = sNativeStates[skelAnime];
    const f32 weight = skelAnime->morphWeight > 0.0f ? skelAnime->morphWeight : 0.0f;
    if (state.valid && weight > 0.0f && now - state.frame <= 3) {
        const f32 before =
            (state.animation != skelAnime->animation || weight > state.weight || state.weight <= 0.0f) ? 1.0f : state.weight;
        f32 k = (before - weight) / before;
        k = k < 0.0f ? 0.0f : (k > 1.0f ? 1.0f : k);
        BlendNative(state.pose, target, k, bones, state.pose);
    } else {
        state.pose = target;
    }
    state.valid = true;
    state.frame = now;
    state.weight = weight;
    state.animation = skelAnime->animation;
    NativePose pose = state.pose;

    // What the game's code turns by hand on a limb (a head that follows Link, a chest that sways, a pose the code
    // writes into the joint table itself): the limb as it was drawn against the limb as the game's animation has
    // it at this frame, turned over on the limb's bone. (The rig says which bone, and how the bone's axes lie in
    // the limb's; of several bones on one limb the first takes it.) While the game blends into the animation its
    // joint table is neither the old pose nor the new: then the limb is held against the table as it is, which
    // leaves what the limbs' callbacks turn.
    static Vec3s authored[kMaxLimbs + 1];
    const Vec3s* reference = walk.joints;
    const s32 gameFrames = (s32)Animation_GetLastFrame(skelAnime->animation) + 1;
    if (weight <= 0.0f && gameFrames > 0 && (s32)gameFirst < gameFrames && (s32)gameNext < gameFrames) {
        SkelAnime_GetFrameData((AnimationHeader*)skelAnime->animation, (s32)gameFirst, skelAnime->limbCount, authored);
        if ((skelAnime->mode & 1) && gamePart > 0.001f && gamePart < 1.0f && gameNext != gameFirst) {
            static Vec3s after[kMaxLimbs + 1];
            SkelAnime_GetFrameData((AnimationHeader*)skelAnime->animation, (s32)gameNext, skelAnime->limbCount, after);
            SkelAnime_InterpFrameTable(skelAnime->limbCount, authored, authored, after, gamePart);
        }
        reference = authored;
    }
    std::vector<s8>& parents = model.limbParents[walk.skeleton];
    if ((s32)parents.size() != limbs) {
        LimbParents(walk.skeleton, limbs, parents);
    }
    bool taken[kMaxLimbs] = {};
    Turn baseTurn;
    const bool baseKnown = TurnOf(walk.base, baseTurn);
    for (size_t b = 0; baseKnown && b < bones; b++) {
        const RigBone& rig = model.bones[b];
        const s32 limb = rig.limb;
        if (!(rig.flags & kBoneMapped) || limb < 0 || limb >= limbs || !walk.seen[limb] || taken[limb]) {
            continue;
        }
        taken[limb] = true;
        const s32 parent = parents[(size_t)limb];
        Turn drawn;
        Turn above = baseTurn;
        if (!TurnOf(walk.limb[limb], drawn) || (parent >= 0 && walk.seen[parent] && !TurnOf(walk.limb[parent], above))) {
            continue;
        }
        const Turn local = TurnMul(TurnTranspose(above), drawn);
        const Turn byHand = TurnMul(TurnTranspose(TurnOfJoint(reference[limb + 1])), local);
        if (byHand.m[0][0] + byHand.m[1][1] + byHand.m[2][2] > 2.9997f) {
            continue; // (under a degree: the table's own rounding)
        }
        Turn axes; // the bone's axes in the limb's frame (the rig's rows are this, transposed)
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                axes.m[i][j] = rig.rot[j][i];
            }
        }
        f32 d[4];
        QuaternionOf(TurnMul(TurnTranspose(axes), TurnMul(byHand, axes)), d);
        // the bone's turn, then the hand's (in the bone's own frame)
        const f32* q = pose.q[b];
        const f32 w = q[0] * d[0] - q[1] * d[1] - q[2] * d[2] - q[3] * d[3];
        const f32 x = q[0] * d[1] + q[1] * d[0] + q[2] * d[3] - q[3] * d[2];
        const f32 y = q[0] * d[2] - q[1] * d[3] + q[2] * d[0] + q[3] * d[1];
        const f32 z = q[0] * d[3] + q[1] * d[2] - q[2] * d[1] + q[3] * d[0];
        pose.q[b][0] = w;
        pose.q[b][1] = x;
        pose.q[b][2] = y;
        pose.q[b][3] = z;
    }

    // the bones, each from its parent's
    for (size_t b = 0; b < bones; b++) {
        const RigBone& rig = model.bones[b];
        const MtxF& parent = rig.parent >= 0 && (size_t)rig.parent < b ? bone[rig.parent] : walk.base;
        const f32 w = pose.q[b][0], x = pose.q[b][1], y = pose.q[b][2], z = pose.q[b][3];
        const f32 turn[3][3] = { { 1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - z * w), 2.0f * (x * z + y * w) },
                                 { 2.0f * (x * y + z * w), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z - x * w) },
                                 { 2.0f * (x * z - y * w), 2.0f * (y * z + x * w), 1.0f - 2.0f * (x * x + y * y) } };
        MtxF local;
        memset(&local, 0, sizeof(local));
        for (int c = 0; c < 3; c++) {
            for (int r = 0; r < 3; r++) {
                local.mf[c][r] = turn[r][c] * pose.s[b][c];
            }
            local.mf[3][c] = pose.t[b][c];
        }
        local.mf[3][3] = 1.0f;
        MtxMul(local, parent, bone[b]);
    }

    // Where the game's code moved the root limb by hand (against where the animation has it), the model goes too.
    if (walk.seen[0]) {
        const Vec3s& root = reference[0];
        f32 shift[3];
        f32 moved = 0.0f;
        f32 unit = 0.0f;
        for (int j = 0; j < 3; j++) {
            const f32 table = root.x * walk.base.mf[0][j] + root.y * walk.base.mf[1][j] + root.z * walk.base.mf[2][j] +
                              walk.base.mf[3][j];
            shift[j] = walk.limb[0].mf[3][j] - table;
            moved += shift[j] * shift[j];
            unit += walk.base.mf[1][j] * walk.base.mf[1][j];
        }
        if (moved > 4.0f * unit) { // (more than two of the model's units)
            for (size_t b = 0; b < bones; b++) {
                for (int j = 0; j < 3; j++) {
                    bone[b].mf[3][j] += shift[j];
                }
            }
        }
    }

    // (the states of SkelAnimes long gone)
    if (now - sNativePruned > 2000) {
        sNativePruned = now;
        for (auto it = sNativeStates.begin(); it != sNativeStates.end();) {
            it = now - it->second.frame > 600 ? sNativeStates.erase(it) : std::next(it);
        }
    }
    return true;
}
} // namespace

extern "C" void MergeShipPort3ds_OnSkelAnime(SkelAnime* skelAnime, s32 alive) {
    if (skelAnime == nullptr || skelAnime->jointTable == nullptr) {
        return;
    }
    if (alive) {
        sSkelAnimes[skelAnime->jointTable] = skelAnime;
        sNativeStates.erase(skelAnime); // (a new actor where another was: nothing of the old one's pose is its)
    } else {
        auto it = sSkelAnimes.find(skelAnime->jointTable);
        if (it != sSkelAnimes.end() && it->second == skelAnime) {
            sSkelAnimes.erase(it);
        }
        sNativeStates.erase(skelAnime);
    }
}

extern "C" void MergeShipPort3ds_SkeletonJoints(Vec3s* jointTable) {
    const s32 depth = sWalkDepth - 1;
    if (depth >= 0 && depth < kMaxSkeletonDepth && sWalks[depth].model != nullptr) {
        sWalks[depth].joints = jointTable;
    }
}

// `single`: the display buffer the caller of the skeleton draw is writing (SkelAnime_Draw, SkelAnime_DrawFlex), which
// then takes the whole model, its opaque groups and after them its translucent ones; or null, and the opaque groups
// go to the frame's opaque buffer and the translucent ones to its translucent buffer.
// (C linkage: the display buffer macros declare the functions they call inside the function that uses them)
extern "C" void MergeShipPort3ds_SkeletonEndTo(PlayState* play, Gfx** single) {
    const s32 depth = --sWalkDepth;
    if (depth < 0) {
        sWalkDepth = 0;
        return;
    }
    if (depth == 0) {
        sLastDrawn = false;
        sPlayerLook.set = false; // (a look set for a skeleton that has no model is not kept for the next one)
    }
    if (depth >= kMaxSkeletonDepth || sWalks[depth].model == nullptr) {
        return;
    }
    if (depth == 0) {
        sLastDrawn = true;
    }
    SkeletonWalk& walk = sWalks[depth];
    SkeletonModel& model = *walk.model;
    ScanDrawn(walk, single); // (what the last limbs' callbacks drew themselves)
    // the innermost walk with a model, among those still open
    sWalkActive = -1;
    for (s32 k = depth - 1; k >= 0; k--) {
        if (k < kMaxSkeletonDepth && sWalks[k].model != nullptr) {
            sWalkActive = k;
            break;
        }
    }

    // An actor the game has squashed flat shows nothing (a Stalfos waiting in the ground has no height: its own
    // model is then a sheet lying in the floor, which is how the game hides it).
    {
        f32 smallest = 0.0f;
        f32 largest = 0.0f;
        for (int i = 0; i < 3; i++) {
            const f32 length = sqrtf(walk.base.mf[i][0] * walk.base.mf[i][0] + walk.base.mf[i][1] * walk.base.mf[i][1] +
                                     walk.base.mf[i][2] * walk.base.mf[i][2]);
            smallest = i == 0 || length < smallest ? length : smallest;
            largest = length > largest ? length : largest;
        }
        if (largest <= 0.0f || smallest < largest * 0.02f) {
            return;
        }
    }

    // Pose the bones: a mapped bone turns with its limb and sits at its parent's matrix times its own offset (so the
    // model keeps its own proportions); the root sits where the game's root limb is, its height scaled by the
    // models' leg lengths; any other bone hangs from its parent as in the bind pose.
    const size_t count = model.bones.size();
    static MtxF bone[96];
    // (the 3DS animation of what the game is playing, where the pack has it: the model on its own skeleton)
    const bool native = PoseNative(walk, model, bone);
    for (size_t b = 0; !native && b < count; b++) {
        const RigBone& rig = model.bones[b];
        const MtxF& parent = rig.parent >= 0 && (size_t)rig.parent < b ? bone[rig.parent] : walk.base;
        MtxF local;
        memset(&local, 0, sizeof(local));
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                local.mf[i][j] = rig.rot[i][j];
            }
            local.mf[3][i] = rig.offset[i];
        }
        local.mf[3][3] = 1.0f;
        const bool mapped = (rig.flags & kBoneMapped) && rig.limb >= 0 && walk.seen[rig.limb];
        if (!mapped) {
            MtxMul(local, parent, bone[b]);
            if ((rig.flags & kBoneRoot) && rig.limb >= 0 && walk.seen[rig.limb]) {
                // (a root with no limb of its own, above the bone of the game's root limb: it sits where that limb
                // is and keeps its bind rotation)
                const MtxF& limb = walk.limb[rig.limb];
                for (int i = 0; i < 3; i++) {
                    bone[b].mf[3][i] = walk.base.mf[3][i] + model.rootScale * (limb.mf[3][i] - walk.base.mf[3][i]) +
                                       model.rootLift * walk.base.mf[1][i];
                }
            }
            continue;
        }
        const MtxF& limb = walk.limb[rig.limb];
        MtxF placed;
        MtxMul(local, parent, placed); // (for where it sits)
        MtxF turned = limb;            // (for how it is turned: the limb's axes, through the rig's rotation)
        turned.mf[3][0] = turned.mf[3][1] = turned.mf[3][2] = 0.0f;
        local.mf[3][0] = local.mf[3][1] = local.mf[3][2] = 0.0f;
        MtxMul(local, turned, bone[b]);
        if (rig.flags & kBoneRoot) {
            // (the lift is along the model's up axis, in the model's units: the base matrix's second row)
            for (int i = 0; i < 3; i++) {
                bone[b].mf[3][i] = walk.base.mf[3][i] + model.rootScale * (limb.mf[3][i] - walk.base.mf[3][i]) +
                                   model.rootLift * walk.base.mf[1][i];
            }
        } else {
            for (int i = 0; i < 3; i++) {
                bone[b].mf[3][i] = placed.mf[3][i];
            }
        }
        bone[b].mf[3][3] = 1.0f;
    }

    // A model built anew has other measures than the game's skeleton (shorter legs, a root higher in the body), and
    // the game's animations move the root for the game's measures. Whatever the pose, the model stays down where the
    // game's model is down: of the limbs the game is drawing, the one whose own part reaches lowest (along the
    // actor's up) carries the pose (a foot when it stands, the head or the back when a Goron lies curled), and the
    // whole model is moved along the actor's up until the model's part on that limb's bone reaches as low.
    // (Posed by its own animation it stands where the 3DS game stands it.)
    if (!native && !model.ground.empty()) {
        f32 up[3] = { walk.base.mf[1][0], walk.base.mf[1][1], walk.base.mf[1][2] };
        const f32 length = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
        const auto lowest = [&](const MtxF& m, const f32 points[kGroundPoints][3]) {
            f32 low = 0.0f;
            for (int k = 0; k < kGroundPoints; k++) {
                f32 h = 0.0f;
                for (int j = 0; j < 3; j++) {
                    const f32 world = points[k][0] * m.mf[0][j] + points[k][1] * m.mf[1][j] + points[k][2] * m.mf[2][j] +
                                      m.mf[3][j];
                    h += world * up[j];
                }
                low = k == 0 || h < low ? h : low;
            }
            return low;
        };
        if (length > 0.0f) {
            for (int j = 0; j < 3; j++) {
                up[j] /= length;
            }
            const RigGround* carrier = nullptr;
            f32 carrierLow = 0.0f;
            for (const RigGround& g : model.ground) {
                if (!walk.seen[g.limb] || !walk.drawn[g.limb]) {
                    continue;
                }
                const f32 low = lowest(walk.limb[g.limb], g.limbPoints);
                if (carrier == nullptr || low < carrierLow) {
                    carrier = &g;
                    carrierLow = low;
                }
            }
            if (carrier != nullptr) {
                const f32 lift = carrierLow - lowest(bone[carrier->bone], carrier->bonePoints);
                for (size_t b = 0; b < count; b++) {
                    for (int j = 0; j < 3; j++) {
                        bone[b].mf[3][j] += lift * up[j];
                    }
                }
            }
        }
    }

    // Seen through his own eyes, Link's model is held by its head. The game's camera is at its own head limb, and
    // the 3DS skeleton's head joint is elsewhere (its neck reaches some two units further forward and one higher):
    // left where the rig puts it, the arms and the item in them are seen from behind the model's eyes. Every bone
    // moves by what brings the model's head joint onto the game's.
    if (walk.player && (walk.wear & PORT3DS_LOOK_FIRST_PERSON)) {
        constexpr s32 kHeadLimb = 10; // (PLAYER_LIMB_HEAD - 1)
        for (size_t b = 0; walk.seen[kHeadLimb] && b < count; b++) {
            if ((model.bones[b].flags & kBoneMapped) && model.bones[b].limb == kHeadLimb) {
                f32 shift[3];
                for (int i = 0; i < 3; i++) {
                    shift[i] = walk.limb[kHeadLimb].mf[3][i] - bone[b].mf[3][i];
                }
                for (size_t k = 0; k < count; k++) {
                    for (int i = 0; i < 3; i++) {
                        bone[k].mf[3][i] += shift[i];
                    }
                }
                break;
            }
        }
    }

    Vec3f where = { walk.base.mf[3][0], walk.base.mf[3][1], walk.base.mf[3][2] };
    Mtx* array = (Mtx*)Graph_Alloc(play->state.gfxCtx, count * sizeof(Mtx));
    Matrix_Push();
    for (size_t b = 0; b < count; b++) {
        Matrix_Put(&bone[b]);
        Matrix_ToMtx(&array[b], (char*)__FILE__, __LINE__);
    }
    Matrix_Pop();

    // What is drawn: a group that is always there or that a limb's list asked for, while the game draws the limb it
    // belongs to and the player wears what it needs.
    const auto shown = [&](const SkeletonModel::Group& group) {
        if (group.anchor >= 0 && !walk.drawn[group.anchor]) {
            return false;
        }
        if ((group.wear != 0 && (walk.wear & group.wear) != group.wear) || (walk.wear & group.without) != 0) {
            return false;
        }
        if (group.flags & 1) {
            return true;
        }
        for (s32 i = 0; i < walk.wantedCount; i++) {
            if (walk.wanted[i] == group.id) {
                return true;
            }
        }
        return false;
    };
    bool anyXlu = false;
    for (const SkeletonModel::Group& group : model.groups) {
        anyXlu = anyXlu || (group.xlu != nullptr && shown(group));
    }
    const char* tunic = nullptr;
    if (!model.tunics.empty()) {
        const s32 index = walk.player && walk.tunic >= 0 && walk.tunic < (s32)model.tunics.size() ? walk.tunic : 0;
        tunic = model.tunics[index].c_str();
    }

    OPEN_DISPS(play->state.gfxCtx);
    // An actor that fades draws its skeleton into the translucent buffer with the alpha in a colour register; a
    // material bound to that register then blends (its lists ask, G_REGS_EX 3).
    bool fades = false;
    if (single != nullptr) {
        const TwoHeadGfxArena& xlu = play->state.gfxCtx->polyXlu;
        fades = (uintptr_t)*single >= (uintptr_t)xlu.bufp && (uintptr_t)*single < (uintptr_t)xlu.bufp + xlu.size;
    }
    for (int layer = 0; layer < 2; layer++) {
        if (layer == 1 && !anyXlu) {
            break;
        }
        Gfx*& disp = single != nullptr ? *single : (layer == 0 ? POLY_OPA_DISP : POLY_XLU_DISP);
        gSPSegment(disp++, 0x0D, (uintptr_t)array);
        if (tunic != nullptr) {
            gSPSegment(disp++, 0x0A, (uintptr_t)tunic);
        }
        gSPMatrix(disp++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPDisplayList(disp++, model.begin);
        s32 boundAmbient = -1;
        s32 boundDiffuse = -1;
        for (const SkeletonModel::Group& group : model.groups) {
            Gfx* list = layer == 0 ? group.opa : group.xlu;
            if (list == nullptr || !shown(group)) {
                continue;
            }
            if (group.ambient != boundAmbient || group.diffuse != boundDiffuse) {
                // the scene's light by this material's factors
                boundAmbient = group.ambient;
                boundDiffuse = group.diffuse;
                EmitLights(CharacterLights(play, where, true, group.ambient / 255.0f, group.diffuse / 255.0f), disp);
            }
            if (group.flags & 2) {
                // (the material leaves its colour constant to the game: both registers carry the gauntlets' colour)
                gDPPipeSync(disp++);
                gDPSetPrimColor(disp++, 0, 0, walk.gauntlets[0], walk.gauntlets[1], walk.gauntlets[2], 255);
                gDPSetEnvColor(disp++, walk.gauntlets[0], walk.gauntlets[1], walk.gauntlets[2], 255);
            }
            if (model.usesRegs) {
                // (the registers the group's bound constants read: its limb's, or none, and they keep the model's)
                s32 slot = group.regLimb >= 0 ? walk.regSlot[group.regLimb] : -1;
                if (slot >= 0 && group.regSeg != 0 && walk.regSlotSeg[group.regLimb][group.regSeg - 8] >= 0) {
                    slot = walk.regSlotSeg[group.regLimb][group.regSeg - 8];
                }
                EmitRegs(disp, kRegsSelect, slot >= 0 ? (u32)slot : 0xFFFFFFFFu, fades);
            }
            gSPDisplayList(disp++, list);
        }
        if (model.usesRegs) {
            EmitRegs(disp, kRegsSelect, 0xFFFFFFFFu);
        }
        gSPDisplayList(disp++, model.end);
        // the game's own light binding again, for whatever this actor draws next into this buffer
        EmitLights(CharacterLights(play, where, false), disp);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

extern "C" void MergeShipPort3ds_SkeletonEnd(PlayState* play) {
    MergeShipPort3ds_SkeletonEndTo(play, nullptr);
}

extern "C" Gfx* MergeShipPort3ds_SkeletonEndGfx(PlayState* play, Gfx* gfx) {
    MergeShipPort3ds_SkeletonEndTo(play, &gfx);
    return gfx;
}

// ---------------------------------------------------------------------------------------------------------------
// 3DS animations (tools/port3ds/linkanim.py). "port3ds/anim/<adult|child>/<name>" for Ocarina of Time's
// gPlayerAnim_<name>, and ".../<age>/mm/<name>" for Majora's Mask's, which this build's young Link plays.
namespace {
#pragma pack(push, 1)
struct AnimHeader {
    char magic[4]; // "P3LA"
    u32 version;   // 1
    u32 frames;
    u32 values;    // s16 per frame: 67
};
#pragma pack(pop)
std::unordered_map<std::string, std::shared_ptr<Ship::IResource>> sPlayerAnims;
} // namespace

extern "C" s32 MergeShipPort3ds_PlayerAnimFrame(PlayState* play, const char* animation, s32 frame, s32 limbCount,
                                                Vec3s* frameTable) {
    if (animation == nullptr || frameTable == nullptr || limbCount <= 0 ||
        !CVarGetInteger(CVAR_PORT3DS_ANIMATIONS_NAME, 0)) {
        return 0;
    }
    // Link himself, not a transformation (those keep Majora's Mask's own animations)
    Player* player = play != nullptr ? GET_PLAYER(play) : nullptr;
    if (player == nullptr || MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return 0;
    }
    const char* path = strncmp(animation, "__OTR__", 7) == 0 ? animation + 7 : animation;
    static const char kOot[] = "objects/gameplay_keep/gPlayerAnim_";
    static const char kMm[] = "mm/objects/gameplay_keep/gPlayerAnim_";
    std::string key = LINK_IS_ADULT ? "port3ds/anim/adult/" : "port3ds/anim/child/";
    if (strncmp(path, kOot, sizeof(kOot) - 1) == 0) {
        key += path + sizeof(kOot) - 1;
    } else if (strncmp(path, kMm, sizeof(kMm) - 1) == 0) {
        key += "mm/";
        key += path + sizeof(kMm) - 1;
    } else {
        return 0;
    }
    auto it = sPlayerAnims.find(key);
    if (it == sPlayerAnims.end()) {
        std::shared_ptr<Ship::IResource> res;
        if (ResourceMgr_FileExists(key.c_str())) {
            res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(key);
        }
        it = sPlayerAnims.emplace(key, res).first;
    }
    if (it->second == nullptr) {
        return 0;
    }
    const u8* data = (const u8*)it->second->GetRawPointer();
    const size_t size = it->second->GetPointerSize();
    AnimHeader header;
    if (data == nullptr || size < sizeof(header)) {
        return 0;
    }
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.magic, "P3LA", 4) != 0 || header.version != 1 || header.frames == 0 || header.values == 0 ||
        size < sizeof(header) + (size_t)header.frames * header.values * sizeof(s16)) {
        return 0;
    }
    const u32 at = frame < 0 ? 0 : ((u32)frame >= header.frames ? header.frames - 1 : (u32)frame);
    const size_t want = sizeof(Vec3s) * (size_t)limbCount + 2;
    const size_t have = (size_t)header.values * sizeof(s16);
    memcpy(frameTable, data + sizeof(header) + (size_t)at * have, want < have ? want : have);
    return 1;
}

// A drawn bow under a 3DS animation ("port3ds/anim/<age>/aim": 'P3AM', u32 version (1), then 4 x 4 floats by rows:
// the arrow against the bow hand, and for the third person's list and the first person's each: how the 3DS bow
// sits, the upper body's turn, and that turn for the game's own model).
namespace {
struct AimNumbers {
    bool loaded = false;
    bool valid = false;
    MtxF atBow;
    MtxF sits[2];
    MtxF turn[2];
    MtxF turnPlain[2];
};
AimNumbers sAim[2]; // adult, child

void RowsToMtxF(const u8* rows, MtxF* out) {
    f32 values[16];
    memcpy(values, rows, sizeof(values));
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out->mf[c][r] = values[r * 4 + c];
        }
    }
}
} // namespace

extern "C" s32 MergeShipPort3ds_PlayerAim(PlayState* play, s32 firstPerson, MtxF* atBow, MtxF* sits, MtxF* turn) {
    if (play == nullptr || !CVarGetInteger(CVAR_PORT3DS_ANIMATIONS_NAME, 0)) {
        return 0;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr || MERGESHIP_FORM_IS_MM(player->mergeShipForm)) {
        return 0;
    }
    AimNumbers& aim = sAim[LINK_IS_ADULT ? 0 : 1];
    if (!aim.loaded) {
        aim.loaded = true;
        const std::string folder = LINK_IS_ADULT ? "port3ds/anim/adult/" : "port3ds/anim/child/";
        const std::string path = folder + "aim";
        // (the numbers are for the pack's own animation of the drawn bow)
        if (ResourceMgr_FileExists(path.c_str()) && ResourceMgr_FileExists((folder + "link_bow_bow_wait").c_str())) {
            auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path);
            const u8* data = res != nullptr ? (const u8*)res->GetRawPointer() : nullptr;
            const size_t size = res != nullptr ? res->GetPointerSize() : 0;
            u32 version = 0;
            const size_t one = 16 * sizeof(f32);
            if (data != nullptr && size >= 8 + 7 * one && memcmp(data, "P3AM", 4) == 0) {
                memcpy(&version, data + 4, sizeof(version));
            }
            if (version == 1) {
                RowsToMtxF(data + 8, &aim.atBow);
                for (int k = 0; k < 2; k++) {
                    RowsToMtxF(data + 8 + one * (1 + 3 * k), &aim.sits[k]);
                    RowsToMtxF(data + 8 + one * (2 + 3 * k), &aim.turn[k]);
                    RowsToMtxF(data + 8 + one * (3 + 3 * k), &aim.turnPlain[k]);
                }
                aim.valid = true;
            }
        }
    }
    if (!aim.valid) {
        return 0;
    }
    // (called from Link's limb callbacks: his skeleton's walk says whether the 3DS model is drawn)
    const bool model = sWalkActive >= 0 && sWalkActive == sWalkDepth - 1 && sWalks[sWalkActive].model != nullptr;
    const int k = firstPerson ? 1 : 0;
    if (atBow != nullptr) {
        *atBow = aim.atBow;
    }
    if (sits != nullptr) {
        if (model) {
            *sits = aim.sits[k];
        } else {
            SkinMatrix_Clear(sits);
        }
    }
    if (turn != nullptr) {
        *turn = model ? aim.turn[k] : aim.turnPlain[k];
    }
    return 1;
}

extern "C" s32 MergeShipPort3ds_RoomDrawn(void) {
    return sRoomDrawn;
}

// ---------------------------------------------------------------------------------------------------------------
// More grass (the "More Grass" option). Tufts of the 3DS game's own grass (the pack's "port3ds/grass/blades", which
// tools/port3ds/review/grass_pack.py names) stand on every floor the game itself calls grass -- its collision says
// so, by the footstep it gives a floor -- around the camera, in thicker and thinner patches, fewer and smaller with
// distance. They are part of the world: lit by the scene's light (worked out once a frame, for a thing that faces
// up as the ground does), in its fog, under the real-time shadows, the near ones casting their own; and they stand
// in the wind (libultraship's, G_REGS_EX 10: the top of a tuft moves, on every drawn frame).
namespace {
constexpr f32 kGrassCell = 30.0f;   // a square of ground, which grows up to kGrassPerCell tufts
constexpr int kGrassPerCell = 6;
constexpr f32 kGrassFar = 640.0f;   // from the eye
constexpr f32 kGrassNear = 270.0f;  // nearer than this a tuft is two crossed cards and casts a shadow
constexpr int kGrassMaxTufts = 7000;
constexpr int kGrassNewCells = 120; // squares looked up a frame at most (a floor check a tuft)

struct GrassTuft {
    s16 x, y, z;
    u8 size; // 0..255: small to large
    u8 look; // bits 0-1: which of the texture's tufts; bits 2-7: how it is turned
};
struct GrassCell {
    s16 fromY; // the height the floors were looked for under
    u8 count;
    GrassTuft tufts[kGrassPerCell];
};
// (the wind's measures as libultraship reads them, SwayEx: how far a tuft's top moves; the weight is the vertex's)
struct GrassWind {
    f32 amp;
    f32 byHeight;
    f32 base;
    f32 pad;
};

std::unordered_map<u64, GrassCell> sGrassCells;
s32 sGrassScene = -1;
int sGrassHave = -1; // the pack has the texture
std::vector<Vtx> sGrassVtx[2];
std::vector<Vtx> sGrassFarVtx;
std::vector<Gfx> sGrassGfx[2];
int sGrassFlip = 0;
GrassWind sGrassWind = { 4.2f, 0.0f, 0.0f, 0.0f };
alignas(2) const char kGrassTexture[] = "__OTR__port3ds/grass/blades";

// The texture's four tufts, in the texture as the list loads it (64 by 64, in 1/32 texel): left, top, right, bottom.
const s16 kGrassRects[4][4] = {
    { 32, 16, 800, 1016 },
    { 608, 16, 1376, 1016 },
    { 1312, 16, 2032, 1016 },
    { 0, 1056, 864, 2040 },
};

inline u32 GrassHash(s32 cx, s32 cz, u32 k) {
    u32 h = (u32)cx * 73856093u ^ (u32)cz * 19349663u ^ (k + 1u) * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// How thick the grass grows about a square, 0 to 1: a slow rise and fall across the land, so that it stands in
// patches and not in rows.
inline f32 GrassPatch(s32 cx, s32 cz) {
    const f32 px = (f32)cx / 5.0f;
    const f32 pz = (f32)cz / 5.0f;
    const s32 ix = (s32)floorf(px);
    const s32 iz = (s32)floorf(pz);
    f32 tx = px - (f32)ix;
    f32 tz = pz - (f32)iz;
    tx = tx * tx * (3.0f - 2.0f * tx);
    tz = tz * tz * (3.0f - 2.0f * tz);
    auto at = [](s32 x, s32 z) { return (f32)(GrassHash(x, z, 977u) & 0xFFFF) / 65535.0f; };
    return (at(ix, iz) * (1.0f - tx) + at(ix + 1, iz) * tx) * (1.0f - tz) +
           (at(ix, iz + 1) * (1.0f - tx) + at(ix + 1, iz + 1) * tx) * tz;
}

// What grows in a square: a tuft wherever one of its places has a grass floor under it (not too steep, not under
// water).
void GrassGrow(PlayState* play, s32 cx, s32 cz, f32 fromY, GrassCell& cell) {
    cell.fromY = (s16)fromY;
    cell.count = 0;
    for (u32 k = 0; k < (u32)kGrassPerCell; k++) {
        const u32 h = GrassHash(cx, cz, k);
        Vec3f pos = { ((f32)cx + (f32)(h & 0xFF) / 256.0f) * kGrassCell, fromY,
                      ((f32)cz + (f32)((h >> 8) & 0xFF) / 256.0f) * kGrassCell };
        CollisionPoly* poly = nullptr;
        s32 bgId = BGCHECK_SCENE;
        const f32 y = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &pos);
        if (y <= BGCHECK_Y_MIN + 1.0f || poly == nullptr || bgId != BGCHECK_SCENE) {
            continue;
        }
        if (func_80041F10(&play->colCtx, poly, bgId) != 8) {
            continue; // (8: the floor sounds of grass underfoot)
        }
        if (COLPOLY_GET_NORMAL(poly->normal.y) < 0.85f) {
            continue;
        }
        f32 water = 0.0f;
        WaterBox* box = nullptr;
        if (WaterBox_GetSurface1(play, &play->colCtx, pos.x, pos.z, &water, &box) && water > y) {
            continue;
        }
        GrassTuft& tuft = cell.tufts[cell.count++];
        tuft.x = (s16)pos.x;
        tuft.y = (s16)y;
        tuft.z = (s16)pos.z;
        tuft.size = (u8)(h >> 16);
        tuft.look = (u8)(h >> 24);
    }
}
} // namespace

// Drawn after the rooms, into the opaque buffer.
extern "C" void MergeShipPort3ds_DrawGrass(PlayState* play) {
    if (play == nullptr || !CVarGetInteger("gMergeShip.Grass", 0)) {
        return;
    }
    if (sGrassHave < 0) {
        sGrassHave = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HasFile(
                         "port3ds/grass/blades")
                         ? 1
                         : 0;
    }
    if (!sGrassHave) {
        return;
    }
    if (sGrassScene != play->sceneNum || sGrassCells.size() > 90000) {
        sGrassCells.clear();
        sGrassScene = play->sceneNum;
    }

    const Vec3f eye = play->view.eye;
    f32 fx = play->view.lookAt.x - eye.x;
    f32 fz = play->view.lookAt.z - eye.z;
    const f32 flat = sqrtf(fx * fx + fz * fz);
    if (flat > 0.001f) {
        fx /= flat;
        fz /= flat;
    } else {
        fx = 0.0f;
        fz = 1.0f;
    }
    const int density = CLAMP(CVarGetInteger("gMergeShip.GrassDensity", 4), 1, kGrassPerCell);
    // (Link parts the grass he stands and walks in)
    const Player* walker = GET_PLAYER(play);
    const Vec3f feet = walker != nullptr ? walker->actor.world.pos : eye;
    // (floors are looked for from well over the camera's subject: the slopes about him are found, a roof is not
    // gone through)
    const f32 fromY = play->view.lookAt.y + 500.0f;
    const f32 midX = eye.x + fx * kGrassFar * 0.45f;
    const f32 midZ = eye.z + fz * kGrassFar * 0.45f;
    const f32 half = kGrassFar * 0.8f;
    const s32 x0 = (s32)floorf((midX - half) / kGrassCell), x1 = (s32)floorf((midX + half) / kGrassCell);
    const s32 z0 = (s32)floorf((midZ - half) / kGrassCell), z1 = (s32)floorf((midZ + half) / kGrassCell);

    // The light, once for all of them: what the scene's light gives a thing that faces up (as the 3DS characters
    // and rooms have that light where the pack draws the rooms; else the game's own).
    const Vec3f at = play->view.lookAt;
    const bool roomsOn = CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0) != 0;
    f32 lit[3];
    EnvLightSettings sceneLight;
    if (roomsOn && MergeShipPort3ds_SceneLight(&sceneLight)) {
        // (beside the pack's rooms: lit as their ground is, MergeShipPort3ds_RelightRoom -- the ambient twice and each
        // of the scene's two lights by how far it stands over the ground -- so the grass keeps the ground's own
        // brightness and colour through the day, at dusk too)
        for (int c = 0; c < 3; c++) {
            lit[c] = (f32)sceneLight.ambientColor[c] * 2.0f;
        }
        const s8* dirs[2] = { sceneLight.light1Dir, sceneLight.light2Dir };
        const u8* colors[2] = { sceneLight.light1Color, sceneLight.light2Color };
        for (int k = 0; k < 2; k++) {
            const f32 length = sqrtf((f32)dirs[k][0] * dirs[k][0] + (f32)dirs[k][1] * dirs[k][1] +
                                     (f32)dirs[k][2] * dirs[k][2]);
            const f32 up = length > 0.0f ? (f32)dirs[k][1] / length : 0.0f;
            if (up > 0.0f) {
                for (int c = 0; c < 3; c++) {
                    lit[c] += up * (f32)colors[k][c];
                }
            }
        }
        for (int c = 0; c < 3; c++) {
            lit[c] = lit[c] > 255.0f ? 255.0f : lit[c];
        }
    } else {
        Lights* lights = CharacterLights(play, at, roomsOn, 0.8f, 1.0f);
        for (int c = 0; c < 3; c++) {
            lit[c] = lights->l.a.l.col[c];
        }
        for (int i = 0; i < lights->numLights; i++) {
            const f32 up = (f32)lights->l.l[i].l.dir[1] / 127.0f;
            if (up > 0.0f) {
                for (int c = 0; c < 3; c++) {
                    lit[c] += up * (f32)lights->l.l[i].l.col[c];
                }
            }
        }
        for (int c = 0; c < 3; c++) {
            lit[c] = lit[c] > 255.0f ? 255.0f : lit[c];
        }
    }

    std::vector<Vtx>& vtx = sGrassVtx[sGrassFlip];
    std::vector<Vtx>& farVtx = sGrassFarVtx;
    std::vector<Gfx>& gfx = sGrassGfx[sGrassFlip];
    sGrassFlip ^= 1;
    vtx.clear();
    farVtx.clear();
    vtx.reserve((size_t)kGrassMaxTufts * 8);
    farVtx.reserve((size_t)kGrassMaxTufts * 4);

    auto corner = [&lit](std::vector<Vtx>& into, f32 x, f32 y, f32 z, s16 s, s16 t, u16 weight, f32 shade) {
        Vtx v;
        v.v.ob[0] = (s16)x;
        v.v.ob[1] = (s16)y;
        v.v.ob[2] = (s16)z;
        v.v.flag = weight; // (how much of the wind moves it)
        v.v.tc[0] = s;
        v.v.tc[1] = t;
        v.v.cn[0] = (u8)(lit[0] * shade);
        v.v.cn[1] = (u8)(lit[1] * shade);
        v.v.cn[2] = (u8)(lit[2] * shade);
        v.v.cn[3] = 255;
        into.push_back(v);
    };

    int grown = 0;
    int tufts = 0;
    for (s32 cz = z0; cz <= z1 && tufts < kGrassMaxTufts; cz++) {
        for (s32 cx = x0; cx <= x1 && tufts < kGrassMaxTufts; cx++) {
            const f32 dx = ((f32)cx + 0.5f) * kGrassCell - eye.x;
            const f32 dz = ((f32)cz + 0.5f) * kGrassCell - eye.z;
            const f32 d2 = dx * dx + dz * dz;
            if (d2 > kGrassFar * kGrassFar) {
                continue;
            }
            // (not behind the eye, nor far off to the sides of what it sees)
            const f32 ahead = dx * fx + dz * fz;
            if (ahead < -70.0f || (d2 > 170.0f * 170.0f && ahead * ahead < d2 * 0.27f)) {
                continue;
            }
            const u64 key = ((u64)(u32)cx << 32) | (u32)cz;
            auto it = sGrassCells.find(key);
            if (it == sGrassCells.end() || fabsf((f32)it->second.fromY - fromY) > 420.0f) {
                if (grown >= kGrassNewCells) {
                    continue;
                }
                grown++;
                GrassCell cell;
                GrassGrow(play, cx, cz, fromY, cell);
                it = sGrassCells.insert_or_assign(key, cell).first;
            }
            const GrassCell& cell = it->second;
            // (how many of the square's tufts: by the patch it lies in, and fewer the farther it is)
            const f32 d = sqrtf(d2);
            f32 want = (f32)density * (0.2f + 1.25f * GrassPatch(cx, cz));
            want *= d < kGrassFar * 0.45f ? 1.0f : (d < kGrassFar * 0.72f ? 0.75f : 0.5f);
            const int most = (int)(want + 0.5f);
            for (int k = 0; k < cell.count && k < most && tufts < kGrassMaxTufts; k++) {
                const GrassTuft& tuft = cell.tufts[k];
                const f32 tx = (f32)tuft.x - eye.x;
                const f32 ty = (f32)tuft.y - eye.y;
                const f32 tz = (f32)tuft.z - eye.z;
                const f32 away = sqrtf(tx * tx + tz * tz);
                // (a tuft grows in over the last stretch of the distance, so none pops up; and none stands right
                // before the lens)
                const f32 close = sqrtf(away * away + ty * ty);
                const f32 grow = CLAMP((kGrassFar - away) / 110.0f, 0.0f, 1.0f) * CLAMP((close - 50.0f) / 50.0f, 0.0f, 1.0f);
                if (grow < 0.05f) {
                    continue;
                }
                const f32 r = (f32)tuft.size / 255.0f;
                const f32 size = (0.55f + 0.85f * r * r) * grow;
                const f32 wide = 6.4f * size;
                const f32 tall = 13.0f * size;
                const s16* rect = kGrassRects[tuft.look & 3];
                const f32 x = tuft.x, y = (f32)tuft.y - 1.5f, z = tuft.z;
                // (each a little lighter or darker than the next; its foot in its own shade)
                const f32 tone = 0.9f + 0.1f * (f32)((tuft.look >> 2) & 7) / 7.0f;
                const f32 foot = tone * 0.74f;
                // (a tuft at Link's feet leans away from him, the more the nearer, and is pressed down a little)
                f32 leanX = 0.0f;
                f32 leanZ = 0.0f;
                f32 top = y + tall;
                {
                    const f32 ax = x - feet.x;
                    const f32 az = z - feet.z;
                    const f32 beside = sqrtf(ax * ax + az * az);
                    if (beside < 24.0f && fabsf((f32)tuft.y - feet.y) < 28.0f) {
                        const f32 k = 1.0f - beside / 24.0f;
                        if (beside > 0.05f) {
                            leanX = ax / beside * tall * 0.62f * k;
                            leanZ = az / beside * tall * 0.62f * k;
                        }
                        top = y + tall * (1.0f - 0.5f * k);
                    }
                }
                if (away < kGrassNear) {
                    // two cards, crossed
                    const f32 turn = (f32)(tuft.look >> 2) * (3.14159265f / 64.0f);
                    const f32 c = cosf(turn) * wide;
                    const f32 s = sinf(turn) * wide;
                    corner(vtx, x - c, y, z - s, rect[0], rect[3], 0, foot);
                    corner(vtx, x + c, y, z + s, rect[2], rect[3], 0, foot);
                    corner(vtx, x + c + leanX, top, z + s + leanZ, rect[2], rect[1], 255, tone);
                    corner(vtx, x - c + leanX, top, z - s + leanZ, rect[0], rect[1], 255, tone);
                    corner(vtx, x + s, y, z - c, rect[0], rect[3], 0, foot);
                    corner(vtx, x - s, y, z + c, rect[2], rect[3], 0, foot);
                    corner(vtx, x - s + leanX, top, z + c + leanZ, rect[2], rect[1], 255, tone);
                    corner(vtx, x + s + leanX, top, z - c + leanZ, rect[0], rect[1], 255, tone);
                } else {
                    // one card, turned to the eye
                    const f32 c = -tz / away * wide;
                    const f32 s = tx / away * wide;
                    corner(farVtx, x - c, y, z - s, rect[0], rect[3], 0, foot);
                    corner(farVtx, x + c, y, z + s, rect[2], rect[3], 0, foot);
                    corner(farVtx, x + c + leanX, top, z + s + leanZ, rect[2], rect[1], 255, tone);
                    corner(farVtx, x - c + leanX, top, z - s + leanZ, rect[0], rect[1], 255, tone);
                }
                tufts++;
            }
        }
    }
    if (tufts == 0) {
        return;
    }
    const int nearCorners = (int)vtx.size();
    vtx.insert(vtx.end(), farVtx.begin(), farVtx.end());
    const int corners = (int)vtx.size();

    // the list: the state, then the cards thirty-two corners at a time
    gfx.assign((size_t)64 + (size_t)(corners / 32 + 2) * 9, Gfx{});
    Gfx* g = gfx.data();
    const bool wind = CVarGetInteger("gMergeShip.GrassWind", 0) != 0;
    gDPPipeSync(g++);
    gDPSetOtherMode(g++,
                    G_AD_DISABLE | G_CD_MAGICSQ | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                        G_TD_CLAMP | G_TP_PERSP | G_CYC_2CYCLE | G_PM_NPRIMITIVE,
                    G_AC_NONE | G_ZS_PIXEL | G_RM_FOG_SHADE_A | G_RM_AA_ZB_TEX_EDGE2);
    // (the texture mixed towards the ground's own green by the primitive colour's alpha, then times the light)
    gDPSetCombineLERP(g++, PRIMITIVE, TEXEL0, PRIMITIVE_ALPHA, TEXEL0, 0, 0, 0, TEXEL0, COMBINED, 0, SHADE, 0, 0, 0,
                      0, COMBINED);
    if (roomsOn) {
        gDPSetPrimColor(g++, 0, 0, 170, 198, 62, 92);
    } else {
        gDPSetPrimColor(g++, 0, 0, 122, 136, 64, 150);
    }
    gSPLoadGeometryMode(g++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH | G_FOG);
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(g++, kGrassTexture, G_IM_FMT_RGBA, G_IM_SIZ_16b, 64, 64, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gSPMatrix(g++, &gMtxClear, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    if (wind) {
        g->words.w0 = (0x4Fu << 24) | (10u << 16); // G_REGS_EX 10: the wind
        g->words.w1 = (uintptr_t)&sGrassWind;
        g++;
    }
    for (int first = 0; first < corners; first += 32) {
        if (first >= nearCorners && first < nearCorners + 32) {
            // (the far tufts follow: they cast no shadow -- G_REGS_EX 11 keeps them out of the light's pass)
            g->words.w0 = (0x4Fu << 24) | (11u << 16);
            g->words.w1 = 1;
            g++;
        }
        const int count = corners - first < 32 ? corners - first : 32;
        gSPVertex(g++, (uintptr_t)&vtx[first], count, 0);
        for (int q = 0; q + 3 < count; q += 8) {
            gSP2Triangles(g++, q, q + 1, q + 2, 0, q, q + 2, q + 3, 0);
            if (q + 7 < count) {
                gSP2Triangles(g++, q + 4, q + 5, q + 6, 0, q + 4, q + 6, q + 7, 0);
            }
        }
    }
    g->words.w0 = (0x4Fu << 24) | (11u << 16);
    g->words.w1 = 0;
    g++;
    if (wind) {
        g->words.w0 = (0x4Fu << 24) | (10u << 16);
        g->words.w1 = 0;
        g++;
    }
    gSPEndDisplayList(g++);

    OPEN_DISPS(play->state.gfxCtx);
    gSPDisplayList(POLY_OPA_DISP++, gfx.data());
    CLOSE_DISPS(play->state.gfxCtx);
}

// Real-time shadows are the Direct3D 11 backend's so far (GfxRenderingAPI::ApplyShadows); with another the game
// keeps its own round shadows.
extern "C" s32 MergeShipPort3ds_ShadowsSupported(void) {
    static int sSupported = -1;
    if (sSupported < 0) {
        auto window = Ship::Context::GetRawInstance()->GetWindow();
        sSupported = window != nullptr && window->GetWindowBackend() == Fast::WindowBackend::FAST3D_DXGI_DX11;
    }
    return sSupported != 0;
}

extern "C" void MergeShipPort3ds_SetRoomDrawn(s32 drawn) {
    sRoomDrawn = drawn;
}

extern "C" void MergeShipPort3ds_RelightRoom(void* handle, EnvLightSettings* light) {
    RoomLists* room = (RoomLists*)handle;
    if (room == nullptr || room->lit.empty()) {
        return;
    }
    if (room->lastLightValid && memcmp(room->lastLight.ambientColor, light->ambientColor, 3) == 0 &&
        memcmp(room->lastLight.light1Dir, light->light1Dir, 3) == 0 &&
        memcmp(room->lastLight.light1Color, light->light1Color, 3) == 0 &&
        memcmp(room->lastLight.light2Dir, light->light2Dir, 3) == 0 &&
        memcmp(room->lastLight.light2Color, light->light2Color, 3) == 0) {
        return;
    }
    room->lastLight = *light;
    room->lastLightValid = true;

    f32 dir[2][3];
    f32 color[2][3];
    f32 ambient[3];
    for (int c = 0; c < 3; c++) {
        ambient[c] = light->ambientColor[c] / 255.0f;
        color[0][c] = light->light1Color[c] / 255.0f;
        color[1][c] = light->light2Color[c] / 255.0f;
        dir[0][c] = light->light1Dir[c];
        dir[1][c] = light->light2Dir[c];
    }
    for (int k = 0; k < 2; k++) {
        f32 length = sqrtf(dir[k][0] * dir[k][0] + dir[k][1] * dir[k][1] + dir[k][2] * dir[k][2]);
        for (int c = 0; c < 3; c++) {
            dir[k][c] = length > 0.0f ? dir[k][c] / length : 0.0f;
        }
    }
    // The 3DS vertex shader's primary colour: the ambient term once per light (both environment lights carry the
    // scene's ambient) plus each light's diffuse term, times the vertex colour (tools/port3ds/meshout.py).
    for (LitRange& range : room->lit) {
        for (u32 i = 0; i < range.count; i++) {
            Vtx& vtx = range.vertices[i];
            f32 n[3];
            DecodeNormal(vtx.v.flag, n);
            f32 facing[2];
            for (int k = 0; k < 2; k++) {
                f32 d = n[0] * dir[k][0] + n[1] * dir[k][1] + n[2] * dir[k][2];
                facing[k] = d > 0.0f ? d : 0.0f;
            }
            for (int c = 0; c < 3; c++) {
                f32 lit = range.ambient[c] * ambient[c] * 2.0f +
                          range.diffuse[c] * (color[0][c] * facing[0] + color[1][c] * facing[1]);
                f32 value = lit * range.base[i * 3 + c] + 0.5f;
                vtx.v.cn[c] = value > 255.0f ? 255 : (u8)value;
            }
        }
    }
}

extern "C" s32 MergeShipPort3ds_RoomLists(PlayState* play, Room* room, Gfx** opa, Gfx** xlu, void** handle) {
    if (!CVarGetInteger(CVAR_PORT3DS_ROOMS_NAME, 0) || !BackendSupported()) {
        return false;
    }
    const std::string path = RoomPath(play, room);
    if (path.empty()) {
        return false;
    }
    auto it = sRooms.find(path);
    if (it == sRooms.end()) {
        RoomLists lists;
        lists.opa = LoadList("port3ds/" + path + "/opa");
        lists.xlu = LoadList("port3ds/" + path + "/xlu");
        LoadAnims(lists.opaAnims, lists.opa, "port3ds/" + path + "/opa");
        LoadAnims(lists.xluAnims, lists.xlu, "port3ds/" + path + "/xlu");
        LoadLit(lists, "port3ds/" + path + "/opa");
        LoadLit(lists, "port3ds/" + path + "/xlu");
        it = sRooms.emplace(path, std::move(lists)).first;
    }
    RoomLists& lists = it->second;
    if (lists.opa == nullptr && lists.xlu == nullptr) {
        return false;
    }
    Clock clock;
    clock.hour = gSaveContext.dayTime * (24.0f / 65536.0f);
    clock.night = gSaveContext.nightFlag != 0;
    clock.adult = LINK_IS_ADULT;
    Animate(lists.opaAnims, lists.opa, play->gameplayFrames, clock);
    Animate(lists.xluAnims, lists.xlu, play->gameplayFrames, clock);
    *opa = lists.opa ? (Gfx*)&lists.opa->Instructions[0] : nullptr;
    *xlu = lists.xlu ? (Gfx*)&lists.xlu->Instructions[0] : nullptr;
    *handle = &lists;
    return true;
}

#else // MERGESHIP_PORT3DS

// The port switched off: every answer is "the pack has nothing here", so each caller takes the path it takes
// without mods/port3ds.o2r (rooms, skeletons, animations, objects, skies and grass draw as the game's own).
extern "C" s32 MergeShipPort3ds_RoomLists(PlayState*, Room*, Gfx**, Gfx**, void**) {
    return false;
}
extern "C" void MergeShipPort3ds_UpdateSceneLight(PlayState*, Room*) {
}
extern "C" s32 MergeShipPort3ds_SceneLight(EnvLightSettings*) {
    return false;
}
extern "C" void MergeShipPort3ds_RelightRoom(void*, EnvLightSettings*) {
}
extern "C" s32 MergeShipPort3ds_RoomDrawn(void) {
    return false;
}
extern "C" void MergeShipPort3ds_SetRoomDrawn(s32) {
}
extern "C" void MergeShipPort3ds_OnSkeletonLoaded(const char*, void**) {
}
extern "C" void MergeShipPort3ds_SkeletonBegin(PlayState*, void**) {
}
extern "C" s32 MergeShipPort3ds_SkeletonLimb(s32, Gfx*) {
    return false;
}
extern "C" void MergeShipPort3ds_SkeletonEnd(PlayState*) {
}
extern "C" s32 MergeShipPort3ds_SkeletonLimbGfx(s32, Gfx*, Gfx**) {
    return false;
}
extern "C" Gfx* MergeShipPort3ds_SkeletonEndGfx(PlayState*, Gfx* gfx) {
    return gfx;
}
extern "C" void MergeShipPort3ds_NoteLimbList(s32, Gfx*) {
}
extern "C" s32 MergeShipPort3ds_SkeletonDrawn(void) {
    return false;
}
extern "C" void MergeShipPort3ds_OnSkelAnime(SkelAnime*, s32) {
}
extern "C" void MergeShipPort3ds_SkeletonJoints(Vec3s*) {
}
extern "C" void MergeShipPort3ds_SetPlayerLook(s32, u32, u8, u8, u8) {
}
extern "C" s32 MergeShipPort3ds_PlayerAnimFrame(PlayState*, const char*, s32, s32, Vec3s*) {
    return false;
}
extern "C" s32 MergeShipPort3ds_PlayerAim(PlayState*, s32, MtxF*, MtxF*, MtxF*) {
    return false;
}
extern "C" s16 MergeShipPort3ds_SceneSkybox(PlayState*, s16 skyboxId) {
    return skyboxId;
}
extern "C" void MergeShipPort3ds_RoomSkyboxDisables(PlayState*, u8*, u8*) {
}
extern "C" void MergeShipPort3ds_SceneView(PlayState*) {
}
extern "C" Gfx* MergeShipPort3ds_ObjectList(const char*) {
    return nullptr;
}
extern "C" void MergeShipPort3ds_DrawGrass(PlayState*) {
}

// Real-time shadows are not part of the port (z_play.c): the Direct3D 11 backend's, as with the port on.
extern "C" s32 MergeShipPort3ds_ShadowsSupported(void) {
    static int sSupported = -1;
    if (sSupported < 0) {
        auto window = Ship::Context::GetRawInstance()->GetWindow();
        sSupported = window != nullptr && window->GetWindowBackend() == Fast::WindowBackend::FAST3D_DXGI_DX11;
    }
    return sSupported != 0;
}

#endif // MERGESHIP_PORT3DS
