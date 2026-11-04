// MergeShip2 -- Ocarina of Time 3D characters: see MergeShip3dsSkin.h, and tools/oot3d/src/Oot3dSkin.h for the
// package layout.

#include "soh/MergeShip/MergeShip3dsSkin.h"
#include "soh/MergeShip/MergeShip3dsSkinCore.h"

#include "soh/ResourceManagerHelpers.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/resource/type/Blob.h>
#include <fast/resource/type/DisplayList.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "z64.h"
#include "functions.h"
#include "macros.h"

#define CVAR_OOT3D_CHARACTERS_NAME CVAR_ENHANCEMENT("MergeShip.Oot3dCharacters")
#define CVAR_OOT3D_CHARACTERS_VALUE CVarGetInteger(CVAR_OOT3D_CHARACTERS_NAME, 1)

extern "C" s32 gMergeShip3dsSuppressLimbs = 0;

namespace ms3ds {

struct Instance {
    SkelAnime* skelAnime = nullptr;
    std::string skeletonPath;
    std::shared_ptr<Skin> skin;
    bool looked = false;
    const void* animPtr = nullptr;
    int anim = -1;
    std::vector<Pose> last, snapshot;
    bool haveLast = false, haveSnapshot = false;
    int32_t clipStart = 0; // gameplay frame the current animation began (for clips on their own clock)
};

// One per skeleton draw in progress (skeleton draws can nest through limb callbacks): the limbs' lists.
// Limb-skinned bodies (ZSK6 blobs, "zzskin_<skeleton>" beside the skeleton; Link): the 3DS mesh in weight clusters of
// the N64 limbs, drawn from the limbs' own matrices as the skeleton draws them, so everything the game does to a limb
// (Player's upper-body aim, head turn, child scaling) moves the 3DS body too. Every cluster matrix goes through the
// matrix stack relative to the actor's, so SoH's frame interpolation keeps the body as smooth as the limbs.
struct Zsk {
    uint32_t eyeMat = 0xFFFFFFFF, eyeBase = 0, eyeCount = 0, mouthMat = 0xFFFFFFFF, mouthBase = 0, mouthCount = 0;
    uint32_t tunicMat = 0xFFFFFFFF, tunicBase = 0, tunicCount = 0; // ZSK7: Kokiri, Goron, Zora
    float drawScale = 1.0f;
    std::vector<uint16_t> slotLimb;
    std::vector<std::array<float, 12>> invBind;   // 3x4 rows
    std::vector<std::array<uint8_t, 8>> clusters; // (slot, weight in 16ths) x 4
    std::vector<Vtx> verts;
    std::vector<uint16_t> cmds;
    std::vector<std::string> mats; // "__OTR__" resource paths
    // zzframes_<skeleton>: per limb, the rotation (3x3 rows) from the 3DS limb frame back to the N64 one, for the game
    // code that places things in N64 limb space (Player's held items, masks, colliders).
    std::vector<std::array<float, 9>> n64Frames;
    // ZFR2: Link's animations are N64-convention (the game's own blending and procedural rotations stay right); per
    // limb, the rotation D (3x3 rows) from the N64 limb frame to the 3DS bone frame the skin is bound in.
    std::vector<std::array<float, 9>> drawFrames;
};
std::unordered_map<const void*, std::shared_ptr<Zsk>> sZsk; // by the skeleton's limb table
std::unordered_map<std::string, std::shared_ptr<Zsk>> sZskByPath;

struct Frame {
    const void* jointTable;
    const Skin* skin = nullptr;
    const Zsk* zsk = nullptr;
    std::vector<const void*> lists; // per limb: what it would have drawn
    std::vector<MtxF> limbMtx;      // limb-skinned bodies: each limb's matrix as drawn
    std::vector<uint8_t> limbSet;
    MtxF actor;                      // the matrix the draw started from
    Gfx* opaStart = nullptr;         // where the traversal began writing (to find the actor's own list calls)
    Gfx* xluStart = nullptr;
};
std::vector<Frame> sFrames;

std::shared_ptr<Zsk> ParseZsk(const uint8_t* d, size_t n) {
    Reader r(d, n);
    const uint32_t magic = r.Get<uint32_t>();
    const int version = magic == 0x374B535A   ? 7
                        : magic == 0x364B535A ? 6
                        : magic == 0x354B535A ? 5
                        : magic == 0x344B535A ? 4
                                              : 0;
    if (version == 0) {
        return nullptr;
    }
    auto z = std::make_shared<Zsk>();
    const uint32_t vertCount = r.Get<uint32_t>(), limbCount = r.Get<uint32_t>(), clusterCount = r.Get<uint32_t>();
    const uint32_t matCount = r.Get<uint32_t>(), cmdCount = r.Get<uint32_t>();
    if (vertCount > 65535 || limbCount > 64 || clusterCount > 4096 || matCount > 4096 || cmdCount > 1000000) {
        return nullptr;
    }
    if (version >= 5) {
        z->eyeMat = r.Get<uint32_t>();
        z->eyeBase = r.Get<uint32_t>();
        z->eyeCount = r.Get<uint32_t>();
        z->mouthMat = r.Get<uint32_t>();
        z->mouthBase = r.Get<uint32_t>();
        z->mouthCount = r.Get<uint32_t>();
    }
    if (version >= 6) {
        z->drawScale = r.Get<float>();
    }
    if (version >= 7) {
        z->tunicMat = r.Get<uint32_t>();
        z->tunicBase = r.Get<uint32_t>();
        z->tunicCount = r.Get<uint32_t>();
    }
    for (uint32_t i = 0; i < limbCount; i++) {
        z->slotLimb.push_back(r.Get<uint16_t>());
    }
    if (limbCount % 2) {
        r.Get<uint16_t>();
    }
    z->invBind.resize(limbCount);
    for (auto& m : z->invBind) {
        for (float& x : m) {
            x = r.Get<float>();
        }
    }
    z->clusters.resize(clusterCount);
    for (auto& c : z->clusters) {
        for (uint8_t& b : c) {
            b = r.Get<uint8_t>();
        }
    }
    z->verts.resize(vertCount);
    for (Vtx& v : z->verts) {
        v.n.ob[0] = r.Get<int16_t>();
        v.n.ob[1] = r.Get<int16_t>();
        v.n.ob[2] = r.Get<int16_t>();
        v.n.flag = 0;
        v.n.n[0] = r.Get<int8_t>();
        v.n.n[1] = r.Get<int8_t>();
        v.n.n[2] = r.Get<int8_t>();
        v.n.a = 255;
        v.n.tc[0] = r.Get<int16_t>();
        v.n.tc[1] = r.Get<int16_t>();
    }
    z->cmds.resize(cmdCount);
    for (uint16_t& c : z->cmds) {
        c = r.Get<uint16_t>();
    }
    for (uint32_t i = 0; i < matCount; i++) {
        const uint16_t len = r.Get<uint16_t>();
        std::string p;
        for (uint16_t k = 0; k < len; k++) {
            p.push_back(static_cast<char>(r.Get<uint8_t>()));
        }
        z->mats.push_back("__OTR__" + p);
    }
    if (r.Bad()) {
        return nullptr;
    }
    return z;
}

void OnSkeletonLoaded(const char* path, void** limbs) {
    if (path == nullptr || limbs == nullptr) {
        return;
    }
    std::string p = path;
    if (p.starts_with("alt/")) {
        p = p.substr(4);
    }
    const size_t slash = p.find_last_of('/');
    if (slash == std::string::npos) {
        return;
    }
    const std::string blobPath = p.substr(0, slash + 1) + "zzskin_" + p.substr(slash + 1);
    auto it = sZskByPath.find(blobPath);
    if (it == sZskByPath.end()) {
        std::shared_ptr<Zsk> z;
        auto file = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(blobPath);
        if (file != nullptr && file->IsLoaded && file->Buffer != nullptr) {
            z = ParseZsk(reinterpret_cast<const uint8_t*>(file->Buffer->data()), file->Buffer->size());
            if (z == nullptr) {
                SPDLOG_ERROR("MergeShip: the OoT3D body {} is damaged", blobPath);
            }
        }
        if (z != nullptr) {
            const std::string framesPath = p.substr(0, slash + 1) + "zzframes_" + p.substr(slash + 1);
            auto ff = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->LoadFile(framesPath);
            if (ff != nullptr && ff->IsLoaded && ff->Buffer != nullptr && ff->Buffer->size() >= 8 &&
                (memcmp(ff->Buffer->data(), "ZFR1", 4) == 0 || memcmp(ff->Buffer->data(), "ZFR2", 4) == 0)) {
                uint32_t n = 0;
                memcpy(&n, ff->Buffer->data() + 4, 4);
                auto& dst = memcmp(ff->Buffer->data(), "ZFR2", 4) == 0 ? z->drawFrames : z->n64Frames;
                if (n < 64 && ff->Buffer->size() >= 8 + size_t(n) * 36) {
                    dst.resize(n);
                    memcpy(dst.data(), ff->Buffer->data() + 8, size_t(n) * 36);
                }
            }
        }
        it = sZskByPath.emplace(blobPath, z).first;
    }
    if (it->second != nullptr) {
        sZsk[limbs] = it->second;
    }
}

// Conventional 3x4 (row-major, translation in column 3) <-> MtxF (mf[column][row]).
void ToRows(const MtxF& m, float out[12]) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            out[r * 4 + c] = m.mf[c][r];
        }
    }
}
void MulRows(const float a[12], const float b[12], float out[12]) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            float v = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c];
            if (c == 3) {
                v += a[r * 4 + 3];
            }
            out[r * 4 + c] = v;
        }
    }
}
void InvertRows(const float m[12], float out[12]) {
    const float a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9], i = m[10];
    const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const float det = a * A + b * B + c * C;
    const float k = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    out[0] = A * k;
    out[1] = -(b * i - c * h) * k;
    out[2] = (b * f - c * e) * k;
    out[4] = B * k;
    out[5] = (a * i - c * g) * k;
    out[6] = -(a * f - c * d) * k;
    out[8] = C * k;
    out[9] = -(a * h - b * g) * k;
    out[10] = (a * e - b * d) * k;
    for (int r = 0; r < 3; r++) {
        out[r * 4 + 3] = -(out[r * 4 + 0] * m[3] + out[r * 4 + 1] * m[7] + out[r * 4 + 2] * m[11]);
    }
}

// The resource path last put in each segment (nullptr: not a path).
extern const char* sSegmentPaths[16];

// Link's eye and mouth textures (z_player_lib.c sEyeTextures / sMouthTextures order) -> the body's face variant.
int FaceIndex(int segment, const char* const* names, int count) {
    const char* set = sSegmentPaths[segment];
    if (set == nullptr) {
        return -1;
    }
    const size_t n = strlen(set);
    for (int i = 0; i < count; i++) {
        const size_t k = strlen(names[i]);
        if (n >= k && strcmp(set + n - k, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

void DrawZsk(PlayState* play, const Frame& f, void* gfxArg) {
    const Zsk& z = *f.zsk;
    // First person and crawling hide Link's head (limb 11): the body stays out of the camera too.
    constexpr size_t kHead = 10;
    if (f.limbSet.size() > kHead && f.limbSet[kHead] && (f.lists.size() <= kHead || f.lists[kHead] == nullptr)) {
        return;
    }
    float actor[12], actorInv[12];
    ToRows(f.actor, actor);
    InvertRows(actor, actorInv);
    std::vector<std::array<float, 12>> skinM(z.slotLimb.size());
    for (size_t s = 0; s < z.slotLimb.size(); s++) {
        const size_t l = z.slotLimb[s];
        float limb[12], rel[12];
        if (l < f.limbSet.size() && f.limbSet[l]) {
            ToRows(f.limbMtx[l], limb);
            if (l < z.drawFrames.size()) {
                // N64 limb frame -> the 3DS bone frame: rotation columns times D, the joint stays.
                const auto& d = z.drawFrames[l];
                float rot[3][3];
                for (int r = 0; r < 3; r++) {
                    for (int c = 0; c < 3; c++) {
                        rot[r][c] = limb[r * 4 + 0] * d[0 * 3 + c] + limb[r * 4 + 1] * d[1 * 3 + c] +
                                    limb[r * 4 + 2] * d[2 * 3 + c];
                    }
                }
                for (int r = 0; r < 3; r++) {
                    for (int c = 0; c < 3; c++) {
                        limb[r * 4 + c] = rot[r][c];
                    }
                }
            }
        } else {
            memcpy(limb, actor, sizeof(limb));
        }
        MulRows(actorInv, limb, rel);
        MulRows(rel, z.invBind[s].data(), skinM[s].data());
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    std::vector<Mtx*> mtx(z.clusters.size());
    for (size_t c = 0; c < z.clusters.size(); c++) {
        float m[12] = {};
        for (int k = 0; k < 4; k++) {
            const uint8_t slot = z.clusters[c][k * 2], w = z.clusters[c][k * 2 + 1];
            if (w == 0 || slot >= skinM.size()) {
                continue;
            }
            for (int i = 0; i < 12; i++) {
                m[i] += skinM[slot][i] * (w / 16.0f);
            }
        }
        MtxF mf;
        for (int r = 0; r < 4; r++) {
            for (int col = 0; col < 4; col++) {
                // drawScale (ZSK6) was the old engine's child correction for its own raw limb walk; these matrices are the
                // game's own (Player already scales the child's root), so it does not apply.
                mf.mf[col][r] = r < 3 ? m[r * 4 + col] : (col == 3 ? 1.0f : 0.0f);
            }
        }
        Matrix_Push();
        Matrix_Mult(&mf, MTXMODE_APPLY);
        mtx[c] = MATRIX_NEWMTX(gfxCtx);
        Matrix_Pop();
    }
    static const char* const kEyes[] = { "EyesOpenTex",      "EyesHalfTex",  "EyesClosedfTex", "EyesRollLeftTex",
                                         "EyesRollRightTex", "EyesShockTex", "EyesUnk1Tex",    "EyesUnk2Tex" };
    static const char* const kMouths[] = { "Mouth1Tex", "Mouth2Tex", "Mouth3Tex", "Mouth4Tex" };
    const int eye = FaceIndex(8, kEyes, 8), mouth = FaceIndex(9, kMouths, 4);

    Gfx** opa = gfxArg != nullptr ? static_cast<Gfx**>(gfxArg) : &gfxCtx->polyOpa.p;
    const std::vector<uint16_t>& c = z.cmds;
    for (size_t i = 0; i < c.size();) {
        const uint16_t t = c[i];
        if (t == 0xFFFE && i + 1 < c.size()) {
            uint32_t m = c[i + 1];
            if (m == z.eyeMat && eye >= 0 && uint32_t(eye) < z.eyeCount) {
                m = z.eyeBase + eye;
            } else if (m == z.mouthMat && mouth >= 0 && uint32_t(mouth) < z.mouthCount) {
                m = z.mouthBase + mouth;
            } else if (m == z.tunicMat && gMergeShip3dsTunic >= 0 && uint32_t(gMergeShip3dsTunic) < z.tunicCount) {
                m = z.tunicBase + gMergeShip3dsTunic;
            }
            if (m < z.mats.size()) {
                gSPDisplayList((*opa)++, (Gfx*)z.mats[m].c_str());
            }
            i += 2;
        } else if (t == 0xFFFB && i + 1 < c.size()) {
            if (c[i + 1] < mtx.size()) {
                gSPMatrix((*opa)++, mtx[c[i + 1]], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            }
            i += 2;
        } else if (t == 0xFFFF && i + 3 < c.size()) {
            if (size_t(c[i + 1]) + c[i + 2] <= z.verts.size()) {
                gSPVertex((*opa)++, (uintptr_t)&z.verts[c[i + 1]], c[i + 2], c[i + 3]);
            }
            i += 4;
        } else if (i + 2 < c.size()) {
            gSP1Triangle((*opa)++, t, c[i + 1], c[i + 2], 0);
            i += 3;
        } else {
            break;
        }
    }
    gDPPipeSync((*opa)++);
}

std::unordered_map<std::string, std::shared_ptr<Skin>> sSkins;
// The resource path last put in each segment (nullptr: not a path).
const char* sSegmentPaths[16] = {};
std::unordered_map<const void*, Instance> sInstances; // by jointTable
std::set<std::string> sWarned;

std::shared_ptr<Skin> SkinFor(const std::string& skeletonPath) {
    auto it = sSkins.find(skeletonPath);
    if (it != sSkins.end()) {
        return it->second;
    }
    std::shared_ptr<Skin> skin;
    auto res = std::dynamic_pointer_cast<Ship::Blob>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource("oot3d/skin/" + skeletonPath));
    if (res != nullptr) {
        skin = Parse(reinterpret_cast<const uint8_t*>(res->Data.data()), res->Data.size());
        if (skin == nullptr) {
            SPDLOG_ERROR("MergeShip: the OoT3D package for {} is damaged", skeletonPath);
        } else {
            for (Group& g : skin->groups) {
                g.statePath = "__OTR__" + g.statePath;
                if (!g.envStatePath.empty()) {
                    g.envStatePath = "__OTR__" + g.envStatePath;
                }
                for (std::string& p : g.palettePaths) {
                    p = "__OTR__" + p;
                }
            }
        }
    }
    sSkins[skeletonPath] = skin;
    return skin;
}

std::string StripOtr(const char* path) {
    if (path == nullptr) {
        return {};
    }
    return strncmp(path, "__OTR__", 7) == 0 ? std::string(path + 7) : std::string(path);
}

// The N64 animation's root translation at `frame` (to carry the game's own changes to it, root motion, over).
bool N64Root(const void* animation, float frame, float out[3]) {
    if (animation == nullptr) {
        return false;
    }
    const AnimationHeader* h =
        (ResourceMgr_OTRSigCheck((char*)animation) != 0)
            ? reinterpret_cast<const AnimationHeader*>(ResourceMgr_LoadAnimByName((const char*)animation))
            : reinterpret_cast<const AnimationHeader*>(animation);
    if (h == nullptr || h->jointIndices == nullptr || h->frameData == nullptr) {
        return false;
    }
    const s32 f = static_cast<s32>(frame);
    const JointIndex& j = h->jointIndices[0];
    const u16 idx[3] = { j.x, j.y, j.z };
    for (int k = 0; k < 3; k++) {
        out[k] = static_cast<float>(idx[k] < h->staticIndexMax ? h->frameData[idx[k]] : h->frameData[idx[k] + f]);
    }
    return true;
}

void OnInit(SkelAnime* skelAnime, const char* skeletonPath) {
    if (skelAnime == nullptr || skelAnime->jointTable == nullptr || skeletonPath == nullptr ||
        strncmp(skeletonPath, "__OTR__", 7) != 0) {
        return;
    }
    Instance in;
    in.skelAnime = skelAnime;
    in.skeletonPath = StripOtr(skeletonPath);
    sInstances[skelAnime->jointTable] = std::move(in);
}

void OnFree(SkelAnime* skelAnime) {
    if (skelAnime != nullptr && skelAnime->jointTable != nullptr) {
        auto it = sInstances.find(skelAnime->jointTable);
        if (it != sInstances.end() && it->second.skelAnime == skelAnime) {
            sInstances.erase(it);
        }
    }
}

Instance* Lookup(void** skeleton, void* jointTable) {
    if (!CVAR_OOT3D_CHARACTERS_VALUE ||
        !Ship::Context::GetRawInstance()->GetResourceManager()->IsAltAssetsEnabled()) {
        return nullptr;
    }
    auto it = sInstances.find(jointTable);
    if (it == sInstances.end()) {
        return nullptr;
    }
    Instance& in = it->second;
    SkelAnime* sa = in.skelAnime;
    if (sa == nullptr || sa->jointTable != jointTable || sa->skeleton != skeleton) {
        return nullptr;
    }
    if (!in.looked) {
        in.skin = SkinFor(in.skeletonPath);
        in.looked = true;
    }
    return in.skin != nullptr ? &in : nullptr;
}

s32 Begin(PlayState* play, void** skeleton, void* jointTable, void* gfxArg) {
    if (play == nullptr) {
        return 0;
    }
    Instance* in = Lookup(skeleton, jointTable);
    const Zsk* zsk = nullptr;
    if (in == nullptr) {
        auto zit = sZsk.find(skeleton);
        if (!CVAR_OOT3D_CHARACTERS_VALUE || zit == sZsk.end()) {
            return 0;
        }
        zsk = zit->second.get();
    }
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Frame f;
    f.jointTable = jointTable;
    f.skin = in != nullptr ? in->skin.get() : nullptr;
    f.zsk = zsk;
    Matrix_Get(&f.actor);
    f.opaStart = gfxArg != nullptr ? *static_cast<Gfx**>(gfxArg) : gfxCtx->polyOpa.p;
    f.xluStart = gfxCtx->polyXlu.p;
    sFrames.push_back(std::move(f));
    return 1;
}

// Lists the actor drew itself during the traversal (gSPDisplayList from a limb callback) that the package has in 3DS
// form: their calls become no-ops, and they count as drawn for the groups that show with them.
std::set<std::string> TakeActorLists(const Skin& skin, const Frame& f, Gfx* opaEnd, Gfx* xluEnd) {
    std::set<std::string> drawn;
    std::unordered_map<const Gfx*, const std::string*> wanted;
    for (const Group& g : skin.groups) {
        if (g.limb == -2 && !g.list.empty()) {
            const std::string otr = "__OTR__" + g.list;
            if (Gfx* loaded = ResourceMgr_LoadGfxByName(otr.c_str())) {
                wanted[loaded] = &g.list;
            }
        }
    }
    if (wanted.empty()) {
        return drawn;
    }
    for (auto [begin, end] : { std::pair<Gfx*, Gfx*>{ f.opaStart, opaEnd }, { f.xluStart, xluEnd } }) {
        if (begin == nullptr || end == nullptr || end <= begin || end - begin > 65536) {
            continue;
        }
        for (Gfx* g = begin; g < end; g++) {
            if ((g->words.w0 >> 24) != G_DL) {
                continue;
            }
            auto it = wanted.find(reinterpret_cast<const Gfx*>(g->words.w1));
            if (it != wanted.end()) {
                drawn.insert(*it->second);
                g->words.w0 = static_cast<uintptr_t>(G_NOOP) << 24;
                g->words.w1 = 0;
            }
        }
    }
    return drawn;
}

void RecordLimb(s32 limb, const void* dList) {
    if (!gMergeShip3dsSuppressLimbs || sFrames.empty() || limb < 0 || limb > 255) {
        return;
    }
    Frame& f = sFrames.back();
    if (f.lists.size() <= size_t(limb)) {
        f.lists.resize(size_t(limb) + 1, nullptr);
    }
    f.lists[limb] = dList;
    if (f.zsk != nullptr) {
        if (f.limbMtx.size() <= size_t(limb)) {
            f.limbMtx.resize(size_t(limb) + 1);
            f.limbSet.resize(size_t(limb) + 1, 0);
        }
        Matrix_Get(&f.limbMtx[limb]);
        f.limbSet[limb] = 1;
    }
}

// Whether a variant group shows: its limb draws its list this frame, or the actor drew it itself.
bool Shows(const Group& g, const Frame& f, const std::set<std::string>& actorLists) {
    if (g.limb == -2) {
        return actorLists.count(g.list) != 0;
    }
    if (g.limb < 0) {
        return true;
    }
    if (size_t(g.limb) >= f.lists.size() || f.lists[g.limb] == nullptr) {
        return false;
    }
    const char* dl = static_cast<const char*>(f.lists[g.limb]);
    return ResourceMgr_OTRSigCheck(const_cast<char*>(dl)) != 0 && strcmp(dl + 7, g.list.c_str()) == 0;
}

void End(PlayState* play, void** skeleton, void* jointTable, void* gfxArg) {
    if (sFrames.empty()) {
        return;
    }
    const Frame frame = std::move(sFrames.back());
    sFrames.pop_back();
    if (frame.zsk != nullptr) {
        if (play != nullptr && frame.jointTable == jointTable) {
            DrawZsk(play, frame, gfxArg);
        }
        return;
    }
    Instance* inp = Lookup(skeleton, jointTable);
    if (play == nullptr || inp == nullptr || frame.jointTable != jointTable) {
        return;
    }
    Instance& in = *inp;
    SkelAnime* sa = in.skelAnime;
    const Skin& skin = *in.skin;
    GraphicsContext* gfxCtx0 = play->state.gfxCtx;
    const std::set<std::string> actorLists =
        TakeActorLists(skin, frame, gfxArg != nullptr ? *static_cast<Gfx**>(gfxArg) : gfxCtx0->polyOpa.p,
                       gfxCtx0->polyXlu.p);

    // The pose: the paired 3DS animation at the N64 animation's frame.
    if (sa->animation != in.animPtr) {
        if (in.haveLast) {
            in.snapshot = in.last;
            in.haveSnapshot = true;
        }
        in.animPtr = sa->animation;
        in.clipStart = play->gameplayFrames;
        const std::string path = StripOtr(static_cast<const char*>(sa->animation));
        auto ai = skin.animByPath.find(path);
        in.anim = ai == skin.animByPath.end() ? -1 : ai->second;
        if (in.anim < 0 && !path.empty() && sWarned.insert(in.skeletonPath + "|" + path).second) {
            // Every animation must have its 3DS pair; one without is a converter bug to fix.
            SPDLOG_WARN("MergeShip: no 3DS animation paired with {} for {}", path, in.skeletonPath);
        }
    }
    std::vector<Pose> pose;
    if (in.anim >= 0) {
        SamplePose(skin, skin.anims[in.anim], sa->curFrame, pose, float(play->gameplayFrames - in.clipStart));
        // Game-side changes of the root translation (root motion taken out, heights adjusted) carry over.
        float root[3];
        if (N64Root(sa->animation, sa->curFrame, root)) {
            const Vec3s* jt = static_cast<const Vec3s*>(jointTable);
            pose[0].v[0] += jt[0].x - root[0];
            pose[0].v[1] += jt[0].y - root[1];
            pose[0].v[2] += jt[0].z - root[2];
        }
    } else if (in.haveLast) {
        pose = in.last;
    } else {
        BindPose(skin, pose);
    }
    // Animation changes blend from the pose held when the change happened, as the N64 morphs.
    if (sa->morphWeight > 0.0f && in.haveSnapshot) {
        BlendPose(pose, in.snapshot, std::min(1.0f, sa->morphWeight));
    }
    in.last = pose;
    in.haveLast = true;

    std::vector<float> pos, nrm;
    SkinVertices(skin, pose, pos, nrm);

    // Display lists: the actor's matrix (the draw has not pushed yet), then each material group.
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Gfx** opa = gfxArg != nullptr ? static_cast<Gfx**>(gfxArg) : &gfxCtx->polyOpa.p;
    Gfx** xlu = &gfxCtx->polyXlu.p;
    const bool offset = skin.offset[0] != 0.0f || skin.offset[1] != 0.0f || skin.offset[2] != 0.0f;
    if (offset) {
        Matrix_Push();
        Matrix_Translate(skin.offset[0], skin.offset[1], skin.offset[2], MTXMODE_APPLY);
    }
    Mtx* mtx = MATRIX_NEWMTX(gfxCtx);
    if (offset) {
        Matrix_Pop();
    }
    gSPMatrix((*opa)++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (skin.anyTranslucent) {
        gSPMatrix((*xlu)++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    }
    for (const Group& g : skin.groups) {
        if (!Shows(g, frame, actorLists)) {
            continue;
        }
        Gfx** dst = g.translucent ? xlu : opa;
        // Faces: the 3DS texture paired with the N64 eye/mouth texture the actor set, else Grezzo's own track.
        const char* state = g.statePath.c_str();
        if (!g.palettePaths.empty()) {
            int idx = -1;
            for (const auto& [path, i] : g.paletteN64) {
                for (int seg = 8; seg < 14 && idx < 0; seg++) {
                    const char* set = sSegmentPaths[seg];
                    if (set != nullptr && strcmp(set + 7, path.c_str()) == 0) {
                        idx = i;
                    }
                }
                if (idx >= 0) {
                    break;
                }
            }
            if (idx < 0) {
                // Each character on its own phase, so a crowd does not blink as one.
                const uint32_t phase = static_cast<uint32_t>((reinterpret_cast<uintptr_t>(jointTable) >> 4) % 97);
                idx = PaletteIndexAt(g, float(play->gameplayFrames + phase));
            }
            if (idx >= 0 && size_t(idx) < g.palettePaths.size()) {
                state = g.palettePaths[idx].c_str();
            }
        }
        gSPDisplayList((*dst)++, (Gfx*)state);
        // `env`: the reflection pass (v5): normals for G_TEXTURE_GEN, its own SHADE alpha.
        auto drawBatches = [&](bool env) {
            for (const Batch& b : g.batches) {
                const size_t n = b.vertices.size();
                if (n == 0) {
                    continue;
                }
                Vtx* vtx = static_cast<Vtx*>(Graph_Alloc(gfxCtx, n * sizeof(Vtx)));
                for (size_t i = 0; i < n; i++) {
                    const BatchVertex& bv = b.vertices[i];
                    const float* p = &pos[size_t(bv.vertex) * 3];
                    Vtx& v = vtx[i];
                    v.v.ob[0] = static_cast<s16>(std::lround(p[0]));
                    v.v.ob[1] = static_cast<s16>(std::lround(p[1]));
                    v.v.ob[2] = static_cast<s16>(std::lround(p[2]));
                    v.v.flag = 0;
                    v.v.tc[0] = env ? 0 : bv.s;
                    v.v.tc[1] = env ? 0 : bv.t;
                    if (g.lit || env) {
                        const float* nn = &nrm[size_t(bv.vertex) * 3];
                        v.n.n[0] = static_cast<s8>(std::lround(nn[0] * 127.0f));
                        v.n.n[1] = static_cast<s8>(std::lround(nn[1] * 127.0f));
                        v.n.n[2] = static_cast<s8>(std::lround(nn[2] * 127.0f));
                        v.n.a = env ? bv.envAlpha : bv.rgba[3];
                    } else {
                        for (int c = 0; c < 4; c++) {
                            v.v.cn[c] = bv.rgba[c];
                        }
                    }
                }
                gSPVertex((*dst)++, reinterpret_cast<uintptr_t>(vtx), static_cast<s32>(n), 0);
                const size_t tris = b.tris.size() / 3;
                for (size_t t = 0; t < tris; t += 2) {
                    const uint8_t* a = &b.tris[t * 3];
                    if (t + 1 < tris) {
                        const uint8_t* c = &b.tris[(t + 1) * 3];
                        gSP2Triangles((*dst)++, a[0], a[1], a[2], 0, c[0], c[1], c[2], 0);
                    } else {
                        gSP1Triangle((*dst)++, a[0], a[1], a[2], 0);
                    }
                }
            }
        };
        drawBatches(false);
        if (!g.envStatePath.empty()) {
            gSPDisplayList((*dst)++, (Gfx*)g.envStatePath.c_str());
            drawBatches(true);
            gDPPipeSync((*dst)++);
            gSPClearGeometryMode((*dst)++, G_TEXTURE_GEN);
        }
    }
    gDPPipeSync((*opa)++);
}

} // namespace ms3ds

extern "C" s32 gMergeShip3dsTunic = -1;

extern "C" const void* MergeShip3dsSkin_AgeAnimation(const char* path, s32 isChild) {
    if (!isChild || path == nullptr || !ResourceMgr_IsAltAssetsEnabled()) {
        return path;
    }
    // path -> its "_child" path (kept alive here), or nullptr when the pack has none
    static std::unordered_map<std::string, std::unique_ptr<std::string>> sChildPaths;
    const std::string key(path);
    auto it = sChildPaths.find(key);
    if (it == sChildPaths.end()) {
        std::unique_ptr<std::string> child;
        if (ResourceMgr_FileExists((key.substr(7) + "_child").c_str())) {
            child = std::make_unique<std::string>(key + "_child");
        }
        it = sChildPaths.emplace(key, std::move(child)).first;
    }
    return it->second != nullptr ? it->second->c_str() : path;
}

extern "C" void MergeShip3dsSkin_OnSkeletonLoaded(const char* path, void** limbs) {
    ms3ds::OnSkeletonLoaded(path, limbs);
}

extern "C" void MergeShip3dsSkin_NoteSegment(int segment, uintptr_t target) {
    if (segment >= 0 && segment < 16) {
        const char* p = reinterpret_cast<const char*>(target);
        ms3ds::sSegmentPaths[segment] = (p != nullptr && ResourceMgr_OTRSigCheck(const_cast<char*>(p)) == 1) ? p : nullptr;
    }
}

extern "C" void MergeShip3dsSkin_OnInit(SkelAnime* skelAnime, const char* skeletonPath) {
    ms3ds::OnInit(skelAnime, skeletonPath);
}

extern "C" void MergeShip3dsSkin_OnFree(SkelAnime* skelAnime) {
    ms3ds::OnFree(skelAnime);
}

extern "C" s32 MergeShip3dsSkin_Begin(PlayState* play, void** skeleton, void* jointTable, void* gfx) {
    return ms3ds::Begin(play, skeleton, jointTable, gfx);
}

extern "C" s32 MergeShip3dsSkin_PushN64Frame(s32 limb) {
    if (ms3ds::sFrames.empty() || ms3ds::sFrames.back().zsk == nullptr || limb < 0) {
        return 0;
    }
    const auto& frames = ms3ds::sFrames.back().zsk->n64Frames;
    if (size_t(limb) >= frames.size()) {
        return 0;
    }
    const auto& d = frames[limb];
    MtxF mf;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            mf.mf[c][r] = (r < 3 && c < 3) ? d[r * 3 + c] : (r == c ? 1.0f : 0.0f);
        }
    }
    Matrix_Push();
    Matrix_Mult(&mf, MTXMODE_APPLY);
    return 1;
}

extern "C" s32 MergeShip3dsSkin_SuppressLimb(s32 limb) {
    if (!gMergeShip3dsSuppressLimbs) {
        return 0;
    }
    if (!ms3ds::sFrames.empty() && ms3ds::sFrames.back().zsk != nullptr) {
        return 0; // a limb-skinned body: the pack empties the body's own lists, equipment keeps drawing
    }
    if (!ms3ds::sFrames.empty() && ms3ds::sFrames.back().skin != nullptr) {
        for (int16_t k : ms3ds::sFrames.back().skin->keepLimbs) {
            if (k == limb) {
                return 0;
            }
        }
    }
    return 1;
}

extern "C" void MergeShip3dsSkin_RecordLimb(s32 limb, void* dList) {
    ms3ds::RecordLimb(limb, dList);
}

extern "C" void MergeShip3dsSkin_End(PlayState* play, void** skeleton, void* jointTable, void* gfx) {
    ms3ds::End(play, skeleton, jointTable, gfx);
}

// -- room texture scrolls ----------------------------------------------------------------------------------------------
// OoT3D rooms scroll water, lava and waterfalls with CMAB translation tracks. The converter gives each scrolling material
// a small list of its tiles' SetTileSize, called by the room geometry, and a table of the tiles' rates
// ("oot3d/scroll/table"); every frame the tile origins of the lists in the resource cache are moved along.
namespace ms3dsScroll {

struct Tile {
    uint8_t tile;
    uint16_t width, height, uls, ult;
    float rateS, rateT; // 10.2 texels per 3DS frame (30 per second)
};
struct Entry {
    std::string path;
    std::vector<Tile> tiles;
    std::shared_ptr<Fast::DisplayList> dl;
};

static std::vector<Entry> sEntries;
static bool sLoaded = false;
static u32 sLastFrame = 0xFFFFFFFF;

static void LoadTable(const char* path);

// The rooms' table and the objects' (bodies converted ring by ring: Morpha's tentacle, the Like Like).
static void Load() {
    sLoaded = true;
    LoadTable("oot3d/scroll/table");
    LoadTable("oot3d/scroll/table_obj");
    SPDLOG_INFO("MergeShip: {} scrolling OoT3D materials", sEntries.size());
}

static void LoadTable(const char* path) {
    if (!ResourceMgr_FileExists(path)) {
        return;
    }
    auto res = std::dynamic_pointer_cast<Ship::Blob>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
    if (res == nullptr || res->Data.size() < 8 || memcmp(res->Data.data(), "SCR1", 4) != 0) {
        return;
    }
    const uint8_t* p = reinterpret_cast<const uint8_t*>(res->Data.data());
    const uint8_t* end = p + res->Data.size();
    auto rd8 = [&]() { return p < end ? *p++ : uint8_t(0); };
    auto rd16 = [&]() { uint16_t v = 0; if (end - p >= 2) { memcpy(&v, p, 2); p += 2; } return v; };
    auto rd32 = [&]() { uint32_t v = 0; if (end - p >= 4) { memcpy(&v, p, 4); p += 4; } return v; };
    auto rdf = [&]() { float v = 0; if (end - p >= 4) { memcpy(&v, p, 4); p += 4; } return v; };
    p += 4;
    const uint32_t count = rd32();
    for (uint32_t i = 0; i < count && p < end; i++) {
        Entry e;
        const uint16_t len = rd16();
        if (end - p < len) {
            break;
        }
        e.path.assign(reinterpret_cast<const char*>(p), len);
        p += len;
        const uint8_t tiles = rd8();
        for (uint8_t t = 0; t < tiles; t++) {
            Tile tl;
            tl.tile = rd8();
            tl.width = rd16();
            tl.height = rd16();
            tl.uls = rd16();
            tl.ult = rd16();
            tl.rateS = rdf();
            tl.rateT = rdf();
            e.tiles.push_back(tl);
        }
        sEntries.push_back(std::move(e));
    }
}

// The origin after `frames3ds` frames, wrapped to two texture widths (mirrored textures repeat every two).
static uint32_t Origin(uint16_t base, float rate, float frames3ds, uint16_t size) {
    const float period = float(size) * 8.0f;
    float o = std::fmod(float(base) - rate * frames3ds, period);
    if (o < 0) {
        o += period;
    }
    return static_cast<uint32_t>(o) & 0xFFF;
}

} // namespace ms3dsScroll

extern "C" void MergeShip3dsScroll_Update(u32 gameplayFrames) {
    using namespace ms3dsScroll;
    if (gameplayFrames == sLastFrame) {
        return;
    }
    sLastFrame = gameplayFrames;
    if (!sLoaded) {
        Load();
    }
    if (sEntries.empty()) {
        return;
    }
    auto* rm = Ship::Context::GetRawInstance()->GetResourceManager().get();
    const float frames3ds = float(gameplayFrames) * 1.5f; // game logic runs at 20 per second
    for (Entry& e : sEntries) {
        if (e.dl == nullptr || e.dl->IsDirty()) {
            e.dl = std::dynamic_pointer_cast<Fast::DisplayList>(rm->GetCachedResource(e.path));
            if (e.dl == nullptr) {
                continue; // its room was never drawn
            }
        }
        size_t k = 0;
        for (Gfx& g : e.dl->Instructions) {
            const uint32_t op = (uint32_t(g.words.w0) >> 24) & 0xFF;
            if (op == 0xDF || k >= e.tiles.size()) {
                break;
            }
            if (op != 0xF2) {
                continue;
            }
            const Tile& t = e.tiles[k++];
            const uint32_t uls = Origin(t.uls, t.rateS, frames3ds, t.width);
            const uint32_t ult = Origin(t.ult, t.rateT, frames3ds, t.height);
            const uint32_t lrs = (uls + ((uint32_t(t.width) - 1) << 2)) & 0xFFF;
            const uint32_t lrt = (ult + ((uint32_t(t.height) - 1) << 2)) & 0xFFF;
            g.words.w0 = (0xF2u << 24) | (uls << 12) | ult;
            g.words.w1 = (uint32_t(t.tile & 7) << 24) | (lrs << 12) | lrt;
        }
    }
}
