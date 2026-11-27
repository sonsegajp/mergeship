#define NOMINMAX

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif

#include <algorithm>
#include <any>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <list>
#include <stack>
#include <chrono>
#include <intrin.h>
#include "fast/resource/type/Light.h"

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include "fast/debug/GfxDebugger.h"
#include "fast/types.h"
#include <string>

#include "fast/interpreter.h"
#include "fast/lus_gbi.h"
#include "fast/backends/gfx_window_manager_api.h"
#include "fast/backends/gfx_rendering_api.h"

#include "ship/window/gui/Gui.h"
#include "ship/resource/ResourceManager.h"
#include "ship/utils/Utils.h"
#include "ship/utils/StrHash64.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"

#include "libultraship/libultra/os.h"

#include <spdlog/fmt/fmt.h>

#ifdef _WIN32
#include <windows.h>
#endif

std::stack<std::string> currentDir;

#define SEG_ADDR(seg, addr) (addr | (seg << 24) | 1)
#define SUPPORT_CHECK(x) assert(x)

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_5_8(VAL_) (((VAL_)*0xFF) / 0x1F)
#define SCALE_8_5(VAL_) ((((VAL_) + 4) * 0x1F) / 0xFF)
#define SCALE_4_8(VAL_) ((VAL_)*0x11)
#define SCALE_8_4(VAL_) ((VAL_) / 0x11)
#define SCALE_3_8(VAL_) ((VAL_)*0x24)
#define SCALE_8_3(VAL_) ((VAL_) / 0x24)

// Based off the current set native dimensions or active framebuffer
#define HALF_SCREEN_WIDTH(activeFb) ((mFbActive ? activeFb->second.orig_width : mNativeDimensions.width) / 2)
#define HALF_SCREEN_HEIGHT(activeFb) ((mFbActive ? activeFb->second.orig_height : mNativeDimensions.height) / 2)

// Ratios for current window dimensions or active framebuffer scaled size
#define RATIO_X(activeFb, dims) \
    ((mFbActive ? activeFb->second.applied_width : dims.width) / (2.0f * HALF_SCREEN_WIDTH(activeFb)))
#define RATIO_Y(activeFb, dims) \
    ((mFbActive ? activeFb->second.applied_height : dims.height) / (2.0f * HALF_SCREEN_HEIGHT(activeFb)))

#define TEXTURE_CACHE_MAX_SIZE 1024

namespace Fast {

static UcodeHandlers ucode_handler_index = ucode_f3dex2;

const static uint32_t f3dex2AttrHandler[] = {
    F3DEX2_G_MTX_PROJECTION, F3DEX2_G_MTX_LOAD,  F3DEX2_G_MTX_PUSH,  F3DEX_G_MTX_NOPUSH,
    F3DEX2_G_CULL_FRONT,     F3DEX2_G_CULL_BACK, F3DEX2_G_CULL_BOTH,
};

const static uint32_t f3dexAttrHandler[] = { F3DEX_G_MTX_PROJECTION, F3DEX_G_MTX_LOAD,   F3DEX_G_MTX_PUSH,
                                             F3DEX_G_MTX_NOPUSH,     F3DEX_G_CULL_FRONT, F3DEX_G_CULL_BACK,
                                             F3DEX_G_CULL_BOTH };

static constexpr std::array ucode_attr_handlers = {
    &f3dexAttrHandler,  // ucode_f3db
    &f3dexAttrHandler,  // ucode_f3d
    &f3dexAttrHandler,  // ucode_f3dex
    &f3dexAttrHandler,  // ucode_f3exb
    &f3dex2AttrHandler, // ucode_f3ex2
    &f3dex2AttrHandler, // ucode_s2dex
};

static uint32_t get_attr(Attribute attr) {
    const auto ucode_map = ucode_attr_handlers[ucode_handler_index];
    // assert(ucode_map->contains(attr) && "Attribute not found in the current ucode handler");
    return (*ucode_map)[attr];
}

static std::string GetPathWithoutFileName(char* filePath) {
    size_t len = strlen(filePath);

    for (size_t i = len - 1; (long)i >= 0; i--) {
        if (filePath[i] == '/' || filePath[i] == '\\') {
            return std::string(filePath).substr(0, i);
        }
    }

    return filePath;
}

constexpr size_t MAX_TRI_BUFFER = 256;

Interpreter::Interpreter() {
    // MergeShip2: the ids of pushed shaders count from zero. (The counter had no initial value: a session that
    // happened to start it on junk drew every pushed shader with junk in its other shader options, among them
    // the render style, which showed as whole scenes tinted or dark now and then, or as a shader that would
    // not compile.)
    mShadersIndex = 0;
    mRsp = new RSP();
    mRdp = new RDP();
    mBufVbo = new float[MAX_TRI_BUFFER * (32 * 3)];
}

Interpreter::~Interpreter() {
    delete mRsp;
    delete mRdp;
    delete[] mBufVbo;
}

static std::weak_ptr<Interpreter> mInstance;
// Set a cached pointer to the instance so we don't need to go through the window every time
void GfxSetInstance(std::shared_ptr<Interpreter> gfx) {
    mInstance = gfx;
}

// N64 prim_depth is 15-bit (0 near, 0x7FFF far).
static constexpr float N64_PRIM_DEPTH_MAX = 32767.0f;

// ---------------------------------------------------------------------------------------------------------------
// MergeShip2 render styles: ink outlines (the toon styles). Every lit vertex keeps the direction its normal points
// on screen; a back face of a lit, opaque, depth-tested model is not thrown away but drawn again in ink, each
// corner pushed out along that direction by a pixel or two, so it shows as a rim around the model's silhouette.
// The ink triangles are gathered here and drawn after each batch of the frame's own triangles.
static constexpr uint32_t kExNoOutline = 0x10; // extra geometry mode bit (the pack sets it on cut-out materials)
// The toon styles on a surface with painted light, as the shaders are told (a style number of its own, after the
// ones in RenderStyle: "o_style == 14" in the shader templates).
static constexpr uint32_t kStyleToonPainted = 14;
static float sOutlineNormal[MAX_VERTICES + 4][2];
static bool sOutlineHasNormal[MAX_VERTICES + 4];
static std::vector<float> sOutlineVbo; // x y z w r g b for each corner
static size_t sOutlineTris = 0;

static bool OutlineStyle(const Interpreter* gfx) {
    return gfx->mRenderStyle == RENDER_STYLE_TOON || gfx->mRenderStyle == RENDER_STYLE_TOON_FLAT;
}

static bool PixelStyle(uint32_t style) {
    return style == RENDER_STYLE_16BIT || style == RENDER_STYLE_8BIT;
}

static void EmitOutlineTriangle(Interpreter* gfx, uint8_t i1, uint8_t i2, uint8_t i3) {
    const uint8_t idx[3] = { i1, i2, i3 };
    RSP* rsp = gfx->mRsp;
    RDP* rdp = gfx->mRdp;
    bool vertexNormals = true;
    for (int k = 0; k < 3; k++) {
        if (idx[k] >= MAX_VERTICES || rsp->loaded_vertices[idx[k]].w <= 0.0f) {
            return;
        }
        vertexNormals = vertexNormals && sOutlineHasNormal[idx[k]];
    }
    const uint32_t modeL = rdp->other_mode_l;
    const bool depthTested = (rsp->geometry_mode & G_ZBUFFER) == G_ZBUFFER && (modeL & Z_CMP) == Z_CMP &&
                             (modeL & Z_UPD) == Z_UPD && (modeL & ZMODE_DEC) != ZMODE_DEC;
    const bool blended = ((modeL & (3 << 20)) == (G_BL_CLR_MEM << 20) && (modeL & (3 << 16)) == (G_BL_1MA << 16)) ||
                         ((modeL & (3 << 22)) == (G_BL_CLR_MEM << 22) && (modeL & (3 << 18)) == (G_BL_1MA << 18));
    const bool cutOut = (modeL & CVG_X_ALPHA) == CVG_X_ALPHA || (modeL & (3U << G_MDSFT_ALPHACOMPARE)) != 0 ||
                        (rsp->extra_geometry_mode & kExNoOutline) != 0;
    const bool invisible = (modeL & (3 << 24)) == (G_BL_0 << 24) && (modeL & (3 << 20)) == (G_BL_CLR_MEM << 20);
    const bool plainBlendEx = rdp->blend_ex_mode == 0 || (((rdp->blend_ex_mode >> 8) & 0xF) == G_BLENDEX_ONE &&
                                                          ((rdp->blend_ex_mode >> 4) & 0xF) == G_BLENDEX_ZERO);
    if (!depthTested || blended || cutOut || invisible || !plainBlendEx) {
        return;
    }

    const float width = (float)std::max<uint32_t>(gfx->mCurDimensions.width, 1);
    const float height = (float)std::max<uint32_t>(gfx->mCurDimensions.height, 1);
    const float thickness = std::max(1.0f, height / 420.0f); // in pixels, for what is near

    // A surface without normals (a room: its light is painted into its vertex colours) is pushed out whole, the way
    // its own plane leans on screen: a face seen edge-on, at a silhouette, moves the most, and one that faces
    // straight away from the eye not at all. The plane's normal comes from the corners in clip space (as in
    // FacetLight below), pointing away from the eye.
    float faceDir[2] = { 0.0f, 0.0f };
    float faceLean = 1.0f;
    if (!vertexNormals) {
        const float(*p)[4] = rsp->P_matrix;
        const float sx = sqrtf(p[0][0] * p[0][0] + p[1][0] * p[1][0] + p[2][0] * p[2][0]);
        const float sy = sqrtf(p[0][1] * p[0][1] + p[1][1] * p[1][1] + p[2][1] * p[2][1]);
        const float sw = sqrtf(p[0][3] * p[0][3] + p[1][3] * p[1][3] + p[2][3] * p[2][3]);
        const float adjust = gfx->AdjXForAspectRatio(1.0f);
        if (sx < 1e-6f || sy < 1e-6f || sw < 0.5f || fabsf(adjust) < 1e-6f) {
            return; // (a view without perspective)
        }
        float pos[3][3];
        for (int k = 0; k < 3; k++) {
            const LoadedVertex* v = &rsp->loaded_vertices[idx[k]];
            pos[k][0] = v->x / (adjust * sx);
            pos[k][1] = v->y / sy;
            pos[k][2] = -v->w / sw;
        }
        const float e1[3] = { pos[1][0] - pos[0][0], pos[1][1] - pos[0][1], pos[1][2] - pos[0][2] };
        const float e2[3] = { pos[2][0] - pos[0][0], pos[2][1] - pos[0][1], pos[2][2] - pos[0][2] };
        float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        const float length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (length < 1e-12f) {
            return;
        }
        const float away = (n[0] * pos[0][0] + n[1] * pos[0][1] + n[2] * pos[0][2]) > 0.0f ? 1.0f : -1.0f;
        for (int k = 0; k < 3; k++) {
            n[k] *= away / length;
        }
        const float dx = n[0] * adjust * sx * width;
        const float dy = n[1] * sy * height;
        const float lean = sqrtf(dx * dx + dy * dy);
        faceLean = std::min(1.0f, sqrtf(n[0] * n[0] + n[1] * n[1]) * 2.5f);
        if (lean < 1e-6f || faceLean < 0.05f) {
            return;
        }
        faceDir[0] = dx / lean;
        faceDir[1] = dy / lean;
    }

    // Ink sits a little behind the faces it was made from (0.15% of their distance), so that the two sides of a
    // thin sheet do not fight. The projection's depth offset, out of the (view and) projection matrix:
    const float(*proj)[4] = rsp->P_matrix;
    float push = 0.0f;
    int axis = 0;
    for (int i = 1; i < 3; i++) {
        if (fabsf(proj[i][3]) > fabsf(proj[axis][3])) {
            axis = i;
        }
    }
    if (fabsf(proj[axis][3]) > 1e-6f) {
        const float scale = -proj[axis][2] / proj[axis][3];
        push = 0.0015f * fabsf(proj[3][2] + proj[3][3] * scale);
    }

    // in fog, the ink fades with what it outlines
    const bool fog = (modeL >> 30) == G_BL_CLR_FOG && (rsp->geometry_mode & G_FOG) != 0;

    const GfxClipParameters clip = gfx->mRapi->GetClipParameters();
    for (int k = 0; k < 3; k++) {
        const LoadedVertex* v = &rsp->loaded_vertices[idx[k]];
        float nx = faceDir[0];
        float ny = faceDir[1];
        if (vertexNormals) {
            nx = sOutlineNormal[idx[k]][0] * width;
            ny = sOutlineNormal[idx[k]][1] * height;
            const float length = sqrtf(nx * nx + ny * ny);
            if (length > 1e-6f) {
                nx /= length;
                ny /= length;
            } else {
                nx = ny = 0.0f;
            }
        }
        const float pixels = std::max(0.9f, thickness * std::clamp(350.0f / v->w, 0.4f, 1.0f)) * faceLean;
        const float x = v->x + nx * (2.0f * pixels / width) * v->w;
        const float y = v->y + ny * (2.0f * pixels / height) * v->w;
        float z = v->z + push;
        if (clip.z_is_from_0_to_1) {
            z = (z + v->w) / 2.0f;
        }
        float ink[3] = { 0.07f, 0.05f, 0.06f };
        if (fog) {
            const float f = v->color.a / 255.0f;
            ink[0] += (rdp->fog_color.r / 255.0f - ink[0]) * f;
            ink[1] += (rdp->fog_color.g / 255.0f - ink[1]) * f;
            ink[2] += (rdp->fog_color.b / 255.0f - ink[2]) * f;
        }
        const float corner[7] = { x, clip.invertY ? -y : y, z, v->w, ink[0], ink[1], ink[2] };
        sOutlineVbo.insert(sOutlineVbo.end(), corner, corner + 7);
    }
    if (++sOutlineTris >= 2048) {
        gfx->Flush();
    }
}

static void FlushOutlines(Interpreter* gfx) {
    static bool sBusy = false;
    if (sOutlineTris == 0 || sBusy) {
        return;
    }
    sBusy = true;
    ShaderProgram* previous = gfx->mRenderingState.mShaderProgram;
    // the plainest program there is: the vertex colour, no texture, no fog, opaque
    ColorCombinerKey key{};
    key.combine_mode = ((uint64_t)G_CCMUX_SHADE << 13) | ((uint64_t)G_ACMUX_1 << 25);
    key.options = (uint64_t)0xFFFF << SHADER_ID_SHIFT;
    ColorCombiner* comb = gfx->LookupOrCreateColorCombiner(key);
    ShaderProgram* prg = comb->prg[0];
    if (prg == nullptr) {
        // (a new program is loaded as it is made, and is then the current one)
        comb->prg[0] = prg = gfx->LookupOrCreateShaderProgram(comb->shader_id0, comb->shader_id1);
    }
    if (prg != nullptr) {
        if (gfx->mRenderingState.mShaderProgram != prg) {
            gfx->mRapi->UnloadShader(gfx->mRenderingState.mShaderProgram);
            gfx->mRapi->LoadShader(prg);
        }
        uint8_t numInputs = 0;
        bool usedTextures[2] = { false, false };
        gfx->mRapi->ShaderGetInfo(prg, &numInputs, usedTextures);
        if (numInputs == 1 && !usedTextures[0] && !usedTextures[1]) {
            const uint8_t none[4] = { 0, 0, 0, 0 };
            gfx->mRapi->SetDepthTestAndMask(true, true);
            gfx->mRapi->SetZmodeDecal(false);
            gfx->mRapi->SetUseAlpha(false);
            gfx->mRapi->SetBlendMode(0, none);
            gfx->mRapi->DrawTriangles(sOutlineVbo.data(), sOutlineVbo.size(), sOutlineTris);
            // back to what the frame's own triangles had
            gfx->mRapi->SetDepthTestAndMask((gfx->mRenderingState.depth_test_and_mask & 1) != 0,
                                            (gfx->mRenderingState.depth_test_and_mask & 2) != 0);
            gfx->mRapi->SetZmodeDecal(gfx->mRenderingState.decal_mode);
            gfx->mRapi->SetUseAlpha(gfx->mRenderingState.alpha_blend);
            const uint8_t constant[4] = { gfx->mBlendExColorActive.r, gfx->mBlendExColorActive.g,
                                          gfx->mBlendExColorActive.b, gfx->mBlendExColorActive.a };
            gfx->mRapi->SetBlendMode(gfx->mBlendExModeActive, constant);
        }
        gfx->mRapi->UnloadShader(prg);
        if (previous != nullptr) {
            gfx->mRapi->LoadShader(previous);
        }
    }
    gfx->mRenderingState.mShaderProgram = previous;
    sOutlineVbo.clear();
    sOutlineTris = 0;
    sBusy = false;
}

// MergeShip2, the low poly style: how bright a triangle is for where it faces, under a fixed light from above and
// one side. The triangle's normal comes from its corners in clip space, and is turned into the world's frame through
// the view the projection matrix carries (Ocarina of Time and Majora's Mask put the camera there), so the shading
// stays on the world when the camera turns.
static float FacetLight(Interpreter* gfx, const LoadedVertex* a, const LoadedVertex* b, const LoadedVertex* c) {
    const float(*p)[4] = gfx->mRsp->P_matrix;
    const float sx = sqrtf(p[0][0] * p[0][0] + p[1][0] * p[1][0] + p[2][0] * p[2][0]);
    const float sy = sqrtf(p[0][1] * p[0][1] + p[1][1] * p[1][1] + p[2][1] * p[2][1]);
    const float sw = sqrtf(p[0][3] * p[0][3] + p[1][3] * p[1][3] + p[2][3] * p[2][3]);
    const float adjust = gfx->AdjXForAspectRatio(1.0f);
    if (sx < 1e-6f || sy < 1e-6f || sw < 0.5f || fabsf(adjust) < 1e-6f || a->w <= 0.0f || b->w <= 0.0f ||
        c->w <= 0.0f) {
        return 1.0f; // (a view without perspective, or a triangle through the eye)
    }
    const LoadedVertex* v[3] = { a, b, c };
    float pos[3][3];
    for (int i = 0; i < 3; i++) {
        pos[i][0] = v[i]->x / (adjust * sx);
        pos[i][1] = v[i]->y / sy;
        pos[i][2] = -v[i]->w / sw;
    }
    const float e1[3] = { pos[1][0] - pos[0][0], pos[1][1] - pos[0][1], pos[1][2] - pos[0][2] };
    const float e2[3] = { pos[2][0] - pos[0][0], pos[2][1] - pos[0][1], pos[2][2] - pos[0][2] };
    float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
    const float length = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (length < 1e-12f) {
        return 1.0f;
    }
    // towards the eye, which is at the origin
    const float side = (n[0] * pos[0][0] + n[1] * pos[0][1] + n[2] * pos[0][2]) > 0.0f ? -1.0f : 1.0f;
    for (int i = 0; i < 3; i++) {
        n[i] *= side / length;
    }
    float world[3];
    for (int i = 0; i < 3; i++) {
        world[i] = n[0] * p[i][0] / sx + n[1] * p[i][1] / sy - n[2] * p[i][3] / sw;
    }
    const float light = world[0] * 0.342f + world[1] * 0.831f + world[2] * 0.439f;
    return 0.66f + 0.5f * std::max(light, 0.0f) + 0.16f * std::min(light, 0.0f);
}

// MergeShip2, 3DS characters: the pack's texture for one the game put in a segment (extra geometry mode bit
// kExTextureRedirect; tools/port3ds/characters.py writes "port3ds/alt/<the game's texture path>"). Null: there is none.
static constexpr uint32_t kExTextureRedirect = 0x20;
// MergeShip2, the 3DS interface option ("gEnhancements.MergeShip.Port3ds.Interface", read once a frame): the
// textures the game loads from "textures/" (its icons) are the pack's "port3ds/alt/textures/..." where it has them.
static bool sInterfaceRedirect = false;
// The stand-ins the pack marks. The game's own HUD pictures are shades that its code colours (a heart between two
// reds, a double-defence heart the other way round, a counter's digit as one flat colour cut out by the texture's
// alpha); the 3DS pictures have their colours, or are white with a black edge. A triangle whose texture is a
// marked stand-in, and whose combiner reads the texture at all, is drawn with another colour combiner (GfxSpTri1;
// the alpha combiner stays, so what the game fades still fades):
//   "port3ds/own/<the game's texture path>" (1): the picture's own colours: a heart, a rupee, a key;
//   "port3ds/tint/<the game's texture path>" (2): the picture by the primitive colour: a digit, green in a full
//   wallet, with its edge still black.
static std::unordered_map<const void*, int> sStandInKinds;

// MergeShip2, the 3DS sharper textures option ("gEnhancements.MergeShip.Port3ds.HdTextures", read once a frame): a
// texture of the pack, "port3ds/<...>", is its upscaled make "port3ds/hd/<...>" where the pack has one
// (tools/port3ds/hdtex.py). The larger picture carries its own scales against the size the list loads, so nothing
// else changes. Returns the texture itself where there is none or the option is off.
static bool sHdTextures = false;
// (real-time shadows, the light's pass: what faces away from the light covers too; ShadowLightFromGame)
static bool sShadowBothSides = false;
// (and the shadow map's size, a side: "gMergeShip.ShadowDetail" 0, 1, 2; read once a frame)
static int sShadowMapSize = 4096;
// (and whether the light starts at the map's near side: what is nearer is cut away, GfxRenderingAPI::StartShadowMap)
static bool sShadowClip = false;
// (and what GfxRenderingAPI::ApplyShadows's params [4] to [11] are: the soft edges' and the fade's measures,
// ShadowLightFromGame)
static float sShadowSoft[8] = { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
// (and the rest of the pass over the screen: ambient shading, clouds, the colour of shade; whether the game's light
// is known this frame, and whether it casts)
static float sShadowAir[8] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
static float sShadowSun[3] = { 1.0f, 1.0f, 1.0f };
static bool sShadowLit = false;
static bool sShadowCasts = false;
// (and a measuring aid: "gMergeShip.ShadowTiming" writes what the light's pass costs to the log)
static bool sShadowTiming = false;

// MergeShip2 swaying grass. G_REGS_EX 10 (w1: a SwayEx of the game's, or 0 for none) puts what is drawn after it in
// the wind: GfxSpVertex moves each vertex sideways by the wind at its place in the world, times the vertex's own
// weight -- the low byte of its flag (0 holds still, 255 moves all the way), or, where `byHeight` is given, how high
// it stands in its model over `base` (a bush: its foot holds, its top moves). The wind is the renderer's, by its
// own clock (Interpreter::Run), so the grass moves on every drawn frame and not only on the game's own; and in the
// light's pass alike, so its shadow moves with it.
struct SwayEx {
    float amp;      // how far a weight of 1 moves, in the world's units
    float byHeight; // 0: the weight is the vertex's flag; over 0: the weight a unit of height over `base` gives;
                    // under 0: the weights are the vertex array's own table (SwayTableFor)
    float base;
    float pad;
};
static const SwayEx* sSway = nullptr;
static float sSwayTime = 0.0f; // seconds
// (real-time shadows: G_REGS_EX 11 with 1 leaves what follows out of the light's pass altogether -- it casts no
// shadow and costs that pass nothing; with 0 what follows casts again)
static bool sShadowNoCast = false;
// (the weights of the vertices being loaded, where their array has a table; set around GfxSpVertex)
static const uint8_t* sSwayTable = nullptr;

static const uint8_t* ExResource(uint64_t hash, size_t* size);

// The weights of `count` vertices of the array `hash` names, from `first` on: the pack's "<array>_sway", a byte a
// vertex (tools/port3ds/review/sway_pack.py marks the plants of a room so). nullptr: the array has none.
static const uint8_t* SwayTableFor(uint64_t hash, const void* first, size_t count) {
    struct Table {
        const uint8_t* base;    // the array's first vertex
        const uint8_t* weights; // nullptr: none
        size_t vertices;
    };
    static std::unordered_map<uint64_t, Table> sTables;
    auto it = sTables.find(hash);
    if (it == sTables.end()) {
        Table table{};
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        table.base = (const uint8_t*)manager->GetResourceRawPointer(hash);
        const std::string* name = manager->GetArchiveManager()->HashToString(hash);
        if (table.base != nullptr && name != nullptr && name->compare(0, 8, "port3ds/") == 0) {
            size_t size = 0;
            const uint8_t* data = ExResource(CRC64((*name + "_sway").c_str()), &size);
            if (data != nullptr && size > 0) {
                table.weights = data;
                table.vertices = size;
            }
        }
        it = sTables.emplace(hash, table).first;
    }
    const Table& table = it->second;
    if (table.weights == nullptr || (const uint8_t*)first < table.base) {
        return nullptr;
    }
    const size_t index = (size_t)((const uint8_t*)first - table.base) / sizeof(F3DVtx);
    return index + count <= table.vertices ? table.weights + index : nullptr;
}

// The wind at a place: (x, z) it pushes by, for a weight of 1 and an amp of 1. It blows one way, in gusts: waves
// some three hundred units long that cross the land at a walking pace and lay the grass over as they pass (one sees
// them travel through a field), rising and dying down over longer stretches; and a quick flutter of every tuft's
// own on top. The grass leans with the wind and swings back past upright only a little.
static inline void SwayAt(float wx, float wz, float* sx, float* sz) {
    const float along = wx * 0.80f + wz * 0.60f; // (the way the wind blows)
    const float gust = sinf(sSwayTime * 2.3f - along * 0.021f + sinf(wx * 0.0061f - wz * 0.0083f) * 1.7f);
    const float swell = 0.62f + 0.38f * sinf(sSwayTime * 0.41f - along * 0.0037f + wz * 0.0021f);
    const float flutter = sinf(sSwayTime * 6.9f + wx * 0.31f + wz * 0.27f);
    const float cross = sinf(sSwayTime * 3.3f - wx * 0.047f + wz * 0.074f);
    const float push = swell * (0.30f + 0.62f * gust + 0.16f * flutter);
    *sx = 0.80f * push - 0.10f * cross * swell;
    *sz = 0.60f * push + 0.13f * cross * swell;
}

static std::shared_ptr<Fast::Texture> HdTexture(const std::shared_ptr<Fast::Texture>& texture) {
    struct Known {
        std::string path;
        std::shared_ptr<Fast::Texture> hd;
    };
    static std::unordered_map<const Fast::Texture*, Known> sKnown;
    if (!sHdTextures || texture == nullptr || texture->GetInitData() == nullptr) {
        return texture;
    }
    const std::string& path = texture->GetInitData()->Path;
    auto it = sKnown.find(texture.get());
    if (it == sKnown.end() || it->second.path != path) {
        Known known;
        known.path = path;
        if (path.compare(0, 8, "port3ds/") == 0 && path.compare(0, 11, "port3ds/hd/") != 0) {
            const std::string hd = "port3ds/hd/" + path.substr(8);
            auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
            if (manager->GetArchiveManager()->HasFile(hd)) {
                known.hd = std::static_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(hd));
            }
        }
        it = sKnown.insert_or_assign(texture.get(), std::move(known)).first;
    }
    return it->second.hd != nullptr ? it->second.hd : texture;
}

static std::shared_ptr<Fast::Texture> RedirectedTexture(const char* name) {
    static std::unordered_map<std::string, std::shared_ptr<Fast::Texture>> sKnown;
    const std::string plain = strncmp(name, "__OTR__", 7) == 0 ? name + 7 : name;
    auto it = sKnown.find(plain);
    if (it == sKnown.end()) {
        std::shared_ptr<Fast::Texture> texture;
        const std::string alt = "port3ds/alt/" + plain;
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        if (manager->GetArchiveManager()->HasFile(alt)) {
            texture = std::static_pointer_cast<Fast::Texture>(manager->LoadResourceProcess(alt));
            if (texture != nullptr && manager->GetArchiveManager()->HasFile("port3ds/own/" + plain)) {
                sStandInKinds[texture.get()] = 1;
            } else if (texture != nullptr && manager->GetArchiveManager()->HasFile("port3ds/tint/" + plain)) {
                sStandInKinds[texture.get()] = 2;
            }
        }
        it = sKnown.emplace(plain, texture).first;
    }
    return it->second;
}

void Interpreter::Flush() {
    if (mBufVboLen > 0) {
        mRapi->SetCurrentPrimDepth((float)mRdp->prim_depth / N64_PRIM_DEPTH_MAX);
        mRapi->DrawTriangles(mBufVbo, mBufVboLen, mBufVboNumTris);
        mBufVboLen = 0;
        mBufVboNumTris = 0;
    }
    FlushOutlines(this);
}

ShaderProgram* Interpreter::LookupOrCreateShaderProgram(uint64_t id0, uint64_t id1) {
    ShaderProgram* prg = mRapi->LookupShader(id0, id1);
    if (prg == nullptr) {
        mRapi->UnloadShader(mRenderingState.mShaderProgram);
        prg = mRapi->CreateAndLoadNewShader(id0, id1);
        mRenderingState.mShaderProgram = prg;
    }
    return prg;
}

const char* Interpreter::CCMUXtoStr(uint32_t ccmux) {
    static constexpr std::array tbl = {
        "G_CCMUX_COMBINED",
        "G_CCMUX_TEXEL0",
        "G_CCMUX_TEXEL1",
        "G_CCMUX_PRIMITIVE",
        "G_CCMUX_SHADE",
        "G_CCMUX_ENVIRONMENT",
        "G_CCMUX_1",
        "G_CCMUX_COMBINED_ALPHA",
        "G_CCMUX_TEXEL0_ALPHA",
        "G_CCMUX_TEXEL1_ALPHA",
        "G_CCMUX_PRIMITIVE_ALPHA",
        "G_CCMUX_SHADE_ALPHA",
        "G_CCMUX_ENV_ALPHA",
        "G_CCMUX_LOD_FRACTION",
        "G_CCMUX_PRIM_LOD_FRAC",
        "G_CCMUX_K5",
    };
    if (ccmux > tbl.size()) {
        return "G_CCMUX_0";
    }
    return tbl[ccmux];
}

// Seems unused
const char* Interpreter::ACMUXtoStr(uint32_t acmux) {
    static constexpr std::array tbl = {
        "G_ACMUX_COMBINED or G_ACMUX_LOD_FRACTION",
        "G_ACMUX_TEXEL0",
        "G_ACMUX_TEXEL1",
        "G_ACMUX_PRIMITIVE",
        "G_ACMUX_SHADE",
        "G_ACMUX_ENVIRONMENT",
        "G_ACMUX_1 or G_ACMUX_PRIM_LOD_FRAC",
        "G_ACMUX_0",
    };
    return tbl[acmux];
}

void Interpreter::GenerateCC(ColorCombiner* comb, const ColorCombinerKey& key) {
    const bool is2Cyc = (key.options & SHADER_OPT(_2CYC)) != 0;

    uint8_t c[2][2][4];
    uint64_t shaderId0 = 0;
    uint64_t shaderId1 = key.options;
    uint8_t shaderInputMapping[2][7] = { { 0 } };
    bool usedTextures[2]{};
    for (uint32_t i = 0; i < 2 && (i == 0 || is2Cyc); i++) {
        uint32_t rgbA = (key.combine_mode >> (i * 28)) & 0xf;
        uint32_t rgbB = (key.combine_mode >> (i * 28 + 4)) & 0xf;
        uint32_t rgbC = (key.combine_mode >> (i * 28 + 8)) & 0x1f;
        uint32_t rgbD = (key.combine_mode >> (i * 28 + 13)) & 7;
        uint32_t alphaA = (key.combine_mode >> (i * 28 + 16)) & 7;
        uint32_t alphaB = (key.combine_mode >> (i * 28 + 16 + 3)) & 7;
        uint32_t alphaC = (key.combine_mode >> (i * 28 + 16 + 6)) & 7;
        uint32_t alphaD = (key.combine_mode >> (i * 28 + 16 + 9)) & 7;

        if (rgbA >= 8) {
            rgbA = G_CCMUX_0;
        }
        if (rgbB >= 8) {
            rgbB = G_CCMUX_0;
        }
        if (rgbC >= 16) {
            rgbC = G_CCMUX_0;
        }
        if (rgbD == 7) {
            rgbD = G_CCMUX_0;
        }

        if (rgbA == rgbB || rgbC == G_CCMUX_0) {
            // Normalize
            rgbA = G_CCMUX_0;
            rgbB = G_CCMUX_0;
            rgbC = G_CCMUX_0;
        }
        if (alphaA == alphaB || alphaC == G_ACMUX_0) {
            // Normalize
            alphaA = G_ACMUX_0;
            alphaB = G_ACMUX_0;
            alphaC = G_ACMUX_0;
        }
        if (i == 1) {
            if (rgbA != G_CCMUX_COMBINED && rgbB != G_CCMUX_COMBINED && rgbC != G_CCMUX_COMBINED &&
                rgbD != G_CCMUX_COMBINED) {
                // First cycle RGB not used, so clear it away
                c[0][0][0] = c[0][0][1] = c[0][0][2] = c[0][0][3] = G_CCMUX_0;
            }
            if (rgbC != G_CCMUX_COMBINED_ALPHA && alphaA != G_ACMUX_COMBINED && alphaB != G_ACMUX_COMBINED &&
                alphaD != G_ACMUX_COMBINED) {
                // First cycle ALPHA not used, so clear it away
                c[0][1][0] = c[0][1][1] = c[0][1][2] = c[0][1][3] = G_ACMUX_0;
            }
        }

        c[i][0][0] = rgbA;
        c[i][0][1] = rgbB;
        c[i][0][2] = rgbC;
        c[i][0][3] = rgbD;
        c[i][1][0] = alphaA;
        c[i][1][1] = alphaB;
        c[i][1][2] = alphaC;
        c[i][1][3] = alphaD;
    }
    if (!is2Cyc) {
        for (uint32_t i = 0; i < 2; i++) {
            for (uint32_t k = 0; k < 4; k++) {
                c[1][i][k] = i == 0 ? G_CCMUX_0 : G_ACMUX_0;
            }
        }

        // In 1-cycle mode, TEXEL1 returns the same value as TEXEL0.
        // Remap combiner inputs so the shader samples from the correct slot.
        // Ex: TEXEL1/TEXEL1_ALPHA → TEXEL0/TEXEL0_ALPHA
        for (uint32_t k = 0; k < 4; k++) {
            if (c[0][0][k] == G_CCMUX_TEXEL1)
                c[0][0][k] = G_CCMUX_TEXEL0;
            if (c[0][0][k] == G_CCMUX_TEXEL1_ALPHA)
                c[0][0][k] = G_CCMUX_TEXEL0_ALPHA;
            if (c[0][1][k] == G_ACMUX_TEXEL1)
                c[0][1][k] = G_ACMUX_TEXEL0;
        }
    }
    {
        uint8_t inputNumber[32] = { 0 };
        uint32_t nextInputNumber = SHADER_INPUT_1;
        for (uint32_t i = 0; i < 2 && (i == 0 || is2Cyc); i++) {
            for (uint32_t j = 0; j < 4; j++) {
                // Mux values 6/7/15 are overloaded by slot. Only B (value 6 = CENTER, 7 = K4)
                // and C (value 6 = SCALE, 15 = K5) carry chroma-key/convert inputs; A and D
                // reuse those values for unrelated constants.
                if (j == 1 && c[i][0][j] == G_CCMUX_CENTER) {
                    c[i][0][j] = G_CCMUX_KEY_CENTER;
                } else if (j == 1 && c[i][0][j] == G_CCMUX_K4) {
                    c[i][0][j] = G_CCMUX_CONVERT_K4;
                } else if (j == 2 && c[i][0][j] == G_CCMUX_SCALE) {
                    c[i][0][j] = G_CCMUX_KEY_SCALE;
                } else if (j == 2 && c[i][0][j] == G_CCMUX_K5) {
                    c[i][0][j] = G_CCMUX_CONVERT_K5;
                }
                uint32_t val = 0;
                switch (c[i][0][j]) {
                    case G_CCMUX_0:
                        val = SHADER_0;
                        break;
                    case G_CCMUX_1:
                        val = SHADER_1;
                        break;
                    case G_CCMUX_TEXEL0:
                        val = SHADER_TEXEL0;
                        // Set the opposite texture when reading from the second cycle color options
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL1:
                        val = SHADER_TEXEL1;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL0_ALPHA:
                        val = SHADER_TEXEL0A;
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_CCMUX_TEXEL1_ALPHA:
                        val = SHADER_TEXEL1A;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_CCMUX_NOISE:
                        val = SHADER_NOISE;
                        break;
                    case G_CCMUX_PRIMITIVE:
                    case G_CCMUX_PRIMITIVE_ALPHA:
                    case G_CCMUX_PRIM_LOD_FRAC:
                    case G_CCMUX_SHADE:
                    case G_CCMUX_ENVIRONMENT:
                    case G_CCMUX_ENV_ALPHA:
                    case G_CCMUX_LOD_FRACTION:
                    case G_CCMUX_KEY_CENTER:
                    case G_CCMUX_KEY_SCALE:
                    case G_CCMUX_CONVERT_K4:
                    case G_CCMUX_CONVERT_K5:
                        if (inputNumber[c[i][0][j]] == 0) {
                            shaderInputMapping[0][nextInputNumber - 1] = c[i][0][j];
                            inputNumber[c[i][0][j]] = nextInputNumber++;
                        }
                        val = inputNumber[c[i][0][j]];
                        break;
                    case G_CCMUX_COMBINED:
                        val = SHADER_COMBINED;
                        break;
                    default:
                        fprintf(stderr, "Unsupported ccmux: %d\n", c[i][0][j]);
                        break;
                }
                shaderId0 |= (uint64_t)val << (i * 32 + j * 4);
            }
        }
        // MergeShip2: the toon styles band the vertex shade, so their shaders are told which colour input it is
        const uint32_t renderStyle = (uint32_t)((key.options >> RENDER_STYLE_SHIFT) & 0xF);
        if (renderStyle == RENDER_STYLE_TOON || renderStyle == RENDER_STYLE_TOON_FLAT ||
            renderStyle == kStyleToonPainted) {
            shaderId1 |= (uint64_t)(inputNumber[G_CCMUX_SHADE] & 7) << RENDER_STYLE_SHADE_SHIFT;
        }
    }
    {
        uint8_t inputNumber[16] = { 0 };
        uint32_t nextInputNumber = SHADER_INPUT_1;
        for (uint32_t i = 0; i < 2; i++) {
            for (uint32_t j = 0; j < 4; j++) {
                uint32_t val = 0;
                switch (c[i][1][j]) {
                    case G_ACMUX_0:
                        val = SHADER_0;
                        break;
                    case G_ACMUX_TEXEL0:
                        val = SHADER_TEXEL0;
                        // Set the opposite texture when reading from the second cycle color options
                        if (i == 0) {
                            usedTextures[0] = true;
                        } else {
                            usedTextures[1] = true;
                        }
                        break;
                    case G_ACMUX_TEXEL1:
                        val = SHADER_TEXEL1;
                        if (i == 0) {
                            usedTextures[1] = true;
                        } else {
                            usedTextures[0] = true;
                        }
                        break;
                    case G_ACMUX_LOD_FRACTION:
                        // case G_ACMUX_COMBINED: same numerical value
                        if (j != 2) {
                            val = SHADER_COMBINED;
                            break;
                        }
                        c[i][1][j] = G_CCMUX_LOD_FRACTION;
                        [[fallthrough]]; // for G_ACMUX_LOD_FRACTION
                    case G_ACMUX_1:
                        // case G_ACMUX_PRIM_LOD_FRAC: same numerical value
                        if (j != 2) {
                            val = SHADER_1;
                            break;
                        }
                        [[fallthrough]]; // for G_ACMUX_PRIM_LOD_FRAC
                    case G_ACMUX_PRIMITIVE:
                    case G_ACMUX_SHADE:
                    case G_ACMUX_ENVIRONMENT:
                        if (inputNumber[c[i][1][j]] == 0) {
                            shaderInputMapping[1][nextInputNumber - 1] = c[i][1][j];
                            inputNumber[c[i][1][j]] = nextInputNumber++;
                        }
                        val = inputNumber[c[i][1][j]];
                        break;
                }
                shaderId0 |= (uint64_t)val << (i * 32 + 16 + j * 4);
            }
        }
    }
    comb->shader_id0 = shaderId0;
    comb->shader_id1 = shaderId1;
    comb->usedTextures[0] = usedTextures[0];
    comb->usedTextures[1] = usedTextures[1];
    // comb->prg = gfx_lookup_or_create_mShaderProgram(shader_id0, shader_id1);
    memcpy(comb->shader_input_mapping, shaderInputMapping, sizeof(shaderInputMapping));
}

ColorCombiner* Interpreter::LookupOrCreateColorCombiner(const ColorCombinerKey& key) {
    if (mPrevCombiner != mColorCombinerPool.end() && mPrevCombiner->first == key) {
        return &mPrevCombiner->second;
    }
    mPrevCombiner = mColorCombinerPool.find(key);
    if (mPrevCombiner != mColorCombinerPool.end()) {
        return &mPrevCombiner->second;
    }
    Flush();
    mPrevCombiner = mColorCombinerPool.insert(std::make_pair(key, ColorCombiner())).first;
    GenerateCC(&mPrevCombiner->second, key);
    return &mPrevCombiner->second;
}

void Interpreter::SetResolvedResourceCacheEnabled(bool enabled) {
    mResolvedResourceCacheEnabled = enabled;
    if (!enabled) {
        mResolvedResourceCache.clear();
    }
}

// Texture binds resolve the same paths every frame; skip the resource
// manager's string/hash/mutex work by memoizing on the pointer.
std::shared_ptr<Ship::IResource> Interpreter::ResolveResourceCached(const char* path) {
    if (path == nullptr) {
        return nullptr;
    }
    if (!mResolvedResourceCacheEnabled) {
        return Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(path);
    }
    auto it = mResolvedResourceCache.find(path);
    if (it != mResolvedResourceCache.end()) {
        return it->second;
    }
    auto res = Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(path);
    // Only memoize a hit. The resource manager caches its own misses, so re-asking
    // for one is cheap, and CacheExternalResource can turn a path that missed into a
    // valid resource at runtime. A memoized null would outlive the resource itself.
    if (res != nullptr) {
        mResolvedResourceCache[path] = res;
    }
    return res;
}

void Interpreter::TextureCacheClear() {
    for (const auto& entry : mTextureCache.map) {
        mTextureCache.free_texture_ids.push_back(entry.second.texture_id);
    }
    mTextureCache.map.clear();
    mTextureCache.lru.clear();
    mResolvedResourceCache.clear();
    // Pre-allocate buckets so the map never rehashes during normal operation.
    // Rehashing invalidates all iterators, including those stored in LRU entries.
    mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
    // Null rendering-state pointers — they pointed into map nodes that are now freed.
    std::fill(std::begin(mRenderingState.mTextures), std::end(mRenderingState.mTextures), nullptr);
}

void Interpreter::ShaderCacheClear() {
    mRapi->ClearShaderCache();
}

bool Interpreter::TextureCacheLookup(int i, const TextureCacheKey& key) {
    TextureCacheMap::iterator it = mTextureCache.map.find(key);
    TextureCacheNode** n = &mRenderingState.mTextures[i];

    if (it != mTextureCache.map.end()) {
        mRapi->SelectTexture(i, it->second.texture_id);
        *n = &*it;
        mTextureCache.lru.splice(mTextureCache.lru.end(), mTextureCache.lru,
                                 it->second.lru_location); // move to back
        return true;
    }

    if (mTextureCache.map.size() >= TEXTURE_CACHE_MAX_SIZE) {
        // Remove the texture that was least recently used
        it = mTextureCache.lru.front().it;
        mTextureCache.free_texture_ids.push_back(it->second.texture_id);
        for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
            if (mRenderingState.mTextures[j] == &*it)
                mRenderingState.mTextures[j] = nullptr;
        }
        mTextureCache.map.erase(it);
        mTextureCache.lru.pop_front();
    }

    uint32_t texture_id;
    if (!mTextureCache.free_texture_ids.empty()) {
        texture_id = mTextureCache.free_texture_ids.back();
        mTextureCache.free_texture_ids.pop_back();
    } else {
        texture_id = mRapi->NewTexture();
    }

    it = mTextureCache.map.insert(std::make_pair(key, TextureCacheValue())).first;
    TextureCacheNode* node = &*it;
    node->second.texture_id = texture_id;
    node->second.lru_location = mTextureCache.lru.insert(mTextureCache.lru.end(), { it });

    mRapi->SelectTexture(i, texture_id);
    mRapi->SetSamplerParameters(i, false, 0, 0);
    *n = node;
    return false;
}

std::string_view Interpreter::GetBaseTexturePath(std::string_view path) {
    if (path.starts_with(Ship::IResource::gAltAssetPrefix)) {
        return path.substr(Ship::IResource::gAltAssetPrefix.length());
    }

    return path;
}

void Interpreter::TextureCacheDelete(const uint8_t* origAddr) {
    while (mTextureCache.map.bucket_count() > 0) {
        TextureCacheKey key = { origAddr, { 0 }, 0, 0, 0 }; // bucket index only depends on the address
        size_t bucket = mTextureCache.map.bucket(key);
        bool again = false;
        for (auto it = mTextureCache.map.begin(bucket); it != mTextureCache.map.end(bucket); ++it) {
            if (it->first.texture_addr == origAddr) {
                for (int j = 0; j < SHADER_MAX_TEXTURES; j++) {
                    if (mRenderingState.mTextures[j] == &*it)
                        mRenderingState.mTextures[j] = nullptr;
                }
                mTextureCache.lru.erase(it->second.lru_location);
                mTextureCache.free_texture_ids.push_back(it->second.texture_id);
                mTextureCache.map.erase(it->first);
                again = true;
                break;
            }
        }
        if (!again) {
            break;
        }
    }
}

// Pick the per-line byte width for texture decode. Prefer the DRAM stride from
// loaded_texture when it looks like real per-line info (differs from total size).
// Fall back to the TMEM tile stride when loaded sizes match total (LoadBlock with
// width=1, where line_size == full_image_line_size == size).
static uint32_t GetEffectiveLineSize(uint32_t lineSizeBytes, uint32_t fullImageLineSizeBytes, uint32_t sizeBytes,
                                     uint32_t tileLineSizeBytes) {
    if ((lineSizeBytes != sizeBytes || fullImageLineSizeBytes != sizeBytes) && lineSizeBytes > 0) {
        return lineSizeBytes;
    }
    return tileLineSizeBytes;
}

static uint32_t GetTileSizeFromCoordinates(float low, float high) {
    // An unset tile (high <= low) defines no region; return 0 so callers skip the tile-region clamp
    // instead of collapsing the texture to the phantom 1-texel size the +4 formula would yield.
    if (high <= low) {
        return 0;
    }
    return static_cast<uint32_t>(lroundf((high - low + 4.0f) / 4.0f));
}

// Loaded size is HD-scaled but the tile region is raw N64 texels; normalize before comparing.
static bool IsPyramidLike(uint32_t width, uint32_t height, uint32_t tileW, uint32_t tileH, float hByteScale,
                          float vPixelScale) {
    const float hs = hByteScale > 0.0f ? hByteScale : 1.0f;
    const float vs = vPixelScale > 0.0f ? vPixelScale : 1.0f;
    const uint32_t loadedPixels = static_cast<uint32_t>(width / hs) * static_cast<uint32_t>(height / vs);
    const uint32_t renderedPixels = tileW * tileH;
    return renderedPixels > 0 && loadedPixels > renderedPixels && loadedPixels * 8 < renderedPixels * 13; // < 1.625x
}

// MergeShip2, the flat styles: a texture as flat colours that keep its drawing. Its texels are sorted into at most
// six colours (the ones it mostly has); each texel then takes the colour most of its neighbourhood has, so specks go
// and patches stay; and a small texture is enlarged, the borders of its patches running straight between texels
// instead of in stairs, so that the patches have clean edges seen from close. Every texel keeps its alpha (enlarged
// the same way), so cut-outs keep their shape. A face keeps its eyes and mouth, grass its blades, a wall its stones.
void Interpreter::UploadTextureStyled(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) {
    if (!mFlattenUpload || rgba32Buf == nullptr || width == 0 || height == 0 ||
        (uint64_t)width * height > 2048ull * 2048ull) {
        mRapi->UploadTexture(rgba32Buf, width, height);
        return;
    }
    const size_t count = (size_t)width * height;
    uint64_t sum[3] = { 0, 0, 0 };
    uint64_t weight = 0;
    for (size_t k = 0; k < count; k++) {
        const uint8_t* p = rgba32Buf + k * 4;
        sum[0] += (uint64_t)p[0] * p[3];
        sum[1] += (uint64_t)p[1] * p[3];
        sum[2] += (uint64_t)p[2] * p[3];
        weight += p[3];
    }
    if (weight == 0) {
        mRapi->UploadTexture(rgba32Buf, width, height);
        return;
    }
    constexpr int kMax = 6;
    float centre[kMax][3] = { { (float)(sum[0] / weight), (float)(sum[1] / weight), (float)(sum[2] / weight) } };
    int centres = 1;
    const auto distance = [](const uint8_t* p, const float* c) {
        const float dr = p[0] - c[0], dg = p[1] - c[1], db = p[2] - c[2];
        return dr * dr + dg * dg + db * db;
    };
    const auto nearest = [&](const uint8_t* p) {
        int best = 0;
        float bestD = distance(p, centre[0]);
        for (int c = 1; c < centres; c++) {
            const float d = distance(p, centre[c]);
            if (d < bestD) {
                bestD = d;
                best = c;
            }
        }
        return best;
    };

    // The colours, from a sample of the opaque texels: each further one is the sample farthest from those there
    // are (while one is far enough to matter), then a few rounds of moving every colour to the middle of the texels
    // nearest to it.
    static std::vector<uint32_t> sample;
    sample.clear();
    const size_t step = std::max<size_t>(1, count / 4096);
    for (size_t k = 0; k < count; k += step) {
        if (rgba32Buf[k * 4 + 3] >= 128) {
            sample.push_back((uint32_t)k);
        }
    }
    if (sample.size() >= 16) {
        while (centres < kMax) {
            float farthest = 0.0f;
            uint32_t pick = sample[0];
            for (uint32_t k : sample) {
                const uint8_t* p = rgba32Buf + (size_t)k * 4;
                float d = distance(p, centre[0]);
                for (int c = 1; c < centres; c++) {
                    d = std::min(d, distance(p, centre[c]));
                }
                if (d > farthest) {
                    farthest = d;
                    pick = k;
                }
            }
            if (farthest < 30.0f * 30.0f) {
                break;
            }
            const uint8_t* p = rgba32Buf + (size_t)pick * 4;
            centre[centres][0] = p[0];
            centre[centres][1] = p[1];
            centre[centres][2] = p[2];
            centres++;
        }
        uint32_t members[kMax] = {};
        for (int pass = 0; pass < 6 && centres > 1; pass++) {
            double total[kMax][3] = {};
            memset(members, 0, sizeof(members));
            for (uint32_t k : sample) {
                const uint8_t* p = rgba32Buf + (size_t)k * 4;
                const int c = nearest(p);
                total[c][0] += p[0];
                total[c][1] += p[1];
                total[c][2] += p[2];
                members[c]++;
            }
            for (int c = 0; c < centres; c++) {
                if (members[c] > 0) {
                    centre[c][0] = (float)(total[c][0] / members[c]);
                    centre[c][1] = (float)(total[c][1] / members[c]);
                    centre[c][2] = (float)(total[c][2] / members[c]);
                }
            }
        }
        // a colour of a few specks goes, and of two that are alike the smaller goes into the larger
        for (bool again = centres > 1; again;) {
            again = false;
            for (int b = 0; b < centres && !again; b++) {
                bool drop = (size_t)members[b] * 100 < sample.size();
                int into = -1;
                for (int a = 0; a < centres && into < 0; a++) {
                    const float dr = centre[a][0] - centre[b][0], dg = centre[a][1] - centre[b][1],
                                db = centre[a][2] - centre[b][2];
                    if (a != b && members[b] <= members[a] && dr * dr + dg * dg + db * db < 34.0f * 34.0f) {
                        into = a;
                    }
                }
                if (into >= 0 && members[into] + members[b] > 0) {
                    const float share = (float)members[into] / (float)(members[into] + members[b]);
                    for (int i = 0; i < 3; i++) {
                        centre[into][i] = centre[into][i] * share + centre[b][i] * (1.0f - share);
                    }
                    members[into] += members[b];
                    drop = true;
                }
                if (drop) {
                    centres--; // (the last one takes the place of the one that goes)
                    if (b != centres) {
                        for (int i = 0; i < 3; i++) {
                            centre[b][i] = centre[centres][i];
                        }
                        members[b] = members[centres];
                    }
                    again = centres > 1;
                }
            }
        }
    }

    // Each texel's colour, then the colour most of it and its eight neighbours have (the texture taken as tiling)
    static std::vector<uint8_t> label, smooth;
    label.resize(count);
    for (size_t k = 0; k < count; k++) {
        label[k] = centres > 1 ? (uint8_t)nearest(rgba32Buf + k * 4) : 0;
    }
    if (centres > 1 && width >= 4 && height >= 4) {
        smooth.resize(count);
        for (uint32_t y = 0; y < height; y++) {
            const uint32_t rows[3] = { (y + height - 1) % height, y, (y + 1) % height };
            for (uint32_t x = 0; x < width; x++) {
                const uint32_t cols[3] = { (x + width - 1) % width, x, (x + 1) % width };
                uint8_t votes[kMax] = {};
                for (uint32_t yy : rows) {
                    for (uint32_t xx : cols) {
                        votes[label[(size_t)yy * width + xx]]++;
                    }
                }
                uint8_t best = label[(size_t)y * width + x];
                for (int c = 0; c < centres; c++) {
                    if (votes[c] > votes[best]) {
                        best = (uint8_t)c;
                    }
                }
                smooth[(size_t)y * width + x] = best;
            }
        }
        label.swap(smooth);
    }

    // Enlarged two or four times while it is small: every new texel takes the colour that weighs most among the four
    // texels around it, which cuts the corners of the patches.
    uint32_t scale = 1;
    while (std::min(width, height) * scale < 128 && scale < 4 &&
           (uint64_t)width * height * (scale * 2) * (scale * 2) <= 512ull * 512ull) {
        scale *= 2;
    }
    const uint32_t outWidth = width * scale, outHeight = height * scale;
    static std::vector<uint8_t> flat;
    flat.resize((size_t)outWidth * outHeight * 4);
    if (scale == 1) {
        for (size_t k = 0; k < count; k++) {
            const float* colour = centre[label[k]];
            flat[k * 4 + 0] = (uint8_t)colour[0];
            flat[k * 4 + 1] = (uint8_t)colour[1];
            flat[k * 4 + 2] = (uint8_t)colour[2];
            flat[k * 4 + 3] = rgba32Buf[k * 4 + 3];
        }
    } else {
        for (uint32_t y = 0; y < outHeight; y++) {
            const float fy = ((float)y + 0.5f) / (float)scale - 0.5f;
            const int y0 = (int)floorf(fy);
            const float ty = fy - (float)y0;
            const uint32_t rows[2] = { (uint32_t)((y0 + (int)height) % (int)height),
                                       (uint32_t)((y0 + 1 + (int)height) % (int)height) };
            for (uint32_t x = 0; x < outWidth; x++) {
                const float fx = ((float)x + 0.5f) / (float)scale - 0.5f;
                const int x0 = (int)floorf(fx);
                const float tx = fx - (float)x0;
                const uint32_t cols[2] = { (uint32_t)((x0 + (int)width) % (int)width),
                                           (uint32_t)((x0 + 1 + (int)width) % (int)width) };
                const float weights[4] = { (1.0f - tx) * (1.0f - ty), tx * (1.0f - ty), (1.0f - tx) * ty, tx * ty };
                float votes[kMax] = {};
                float alpha = 0.0f;
                for (int n = 0; n < 4; n++) {
                    const size_t at = (size_t)rows[n >> 1] * width + cols[n & 1];
                    votes[label[at]] += weights[n];
                    alpha += weights[n] * rgba32Buf[at * 4 + 3];
                }
                int best = 0;
                for (int c = 1; c < centres; c++) {
                    if (votes[c] > votes[best]) {
                        best = c;
                    }
                }
                uint8_t* out = &flat[((size_t)y * outWidth + x) * 4];
                out[0] = (uint8_t)centre[best][0];
                out[1] = (uint8_t)centre[best][1];
                out[2] = (uint8_t)centre[best][2];
                out[3] = (uint8_t)(alpha + 0.5f);
            }
        }
    }
    mRapi->UploadTexture(flat.data(), outWidth, outHeight);
}

void Interpreter::ImportTextureRgba16(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureRgba16: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes / 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].uls, mRdp->texture_tile[tile].lrs);
    uint32_t tile_h = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].ult, mRdp->texture_tile[tile].lrt);
    bool pyramidLike = IsPyramidLike(width, height, tile_w, tile_h, metadata->h_byte_scale, metadata->v_pixel_scale);
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    // A masked axis wraps every 2^mask texels, so trim an over-loaded texture back to that.
    // Skip a mask smaller than the tile region though - that's stale tile state, not a real load.
    uint32_t maskW = mRdp->texture_tile[tile].masks;
    uint32_t maskH = mRdp->texture_tile[tile].maskt;
    if (maskW != 0 && (1u << maskW) >= tile_w && (1u << maskW) < width) {
        width = 1u << maskW;
    }
    if (maskH != 0 && (1u << maskH) >= tile_h && (1u << maskH) < height) {
        height = 1u << maskH;
    }
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width * 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (fullImageLineSizeBytes / 2)) + (x);

            uint16_t col16 = (addr[2 * clrIdx] << 8) | addr[2 * clrIdx + 1];
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;

            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureRgba32(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureRgba32: null texture address for tile {}", tile);
        return;
    }

    uint32_t size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t full_image_line_size_bytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, full_image_line_size_bytes, size_bytes,
                                               mRdp->texture_tile[tile].line_size_bytes * 2);
    uint32_t width = widthBytes / 4;
    uint32_t height = widthBytes > 0 ? size_bytes / widthBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].uls, mRdp->texture_tile[tile].lrs);
    uint32_t tile_h = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].ult, mRdp->texture_tile[tile].lrt);
    bool pyramidLike = IsPyramidLike(width, height, tile_w, tile_h, metadata->h_byte_scale, metadata->v_pixel_scale);
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    // A masked axis wraps every 2^mask texels, so trim an over-loaded texture back to that.
    // Skip a mask smaller than the tile region though - that's stale tile state, not a real load.
    uint32_t maskW = mRdp->texture_tile[tile].masks;
    uint32_t maskH = mRdp->texture_tile[tile].maskt;
    if (maskW != 0 && (1u << maskW) >= tile_w && (1u << maskW) < width) {
        width = 1u << maskW;
    }
    if (maskH != 0 && (1u << maskH) >= tile_h && (1u << maskH) < height) {
        height = 1u << maskH;
    }
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    if (full_image_line_size_bytes == size_bytes) {
        full_image_line_size_bytes = width * 4;
    }

    // Copy pixel by pixel, respecting full image stride (handles sub-tile loads)
    uint32_t fullImageStridePixels = full_image_line_size_bytes / 4;
    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcIdx = y * fullImageStridePixels + x;
            mTexUploadBuffer[4 * i + 0] = addr[4 * srcIdx + 0];
            mTexUploadBuffer[4 * i + 1] = addr[4 * srcIdx + 1];
            mTexUploadBuffer[4 * i + 2] = addr[4 * srcIdx + 2];
            mTexUploadBuffer[4 * i + 3] = addr[4 * srcIdx + 3];
            i++;
        }
    }
    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA4(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureIA4: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes * 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = widthBytes;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcPixelIdx = y * (fullImageLineSizeBytes * 2) + x;
            uint8_t byte = addr[srcPixelIdx / 2];
            uint8_t part = (byte >> (4 - (srcPixelIdx % 2) * 4)) & 0xf;
            uint8_t intensity = part >> 1;
            uint8_t alpha = part & 1;
            mTexUploadBuffer[4 * i + 0] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 1] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 2] = SCALE_3_8(intensity);
            mTexUploadBuffer[4 * i + 3] = alpha ? 255 : 0;
            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureIA8: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t width = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                          mRdp->texture_tile[tile].line_size_bytes);
    uint32_t height = width > 0 ? sizeBytes / width : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcIdx = y * fullImageLineSizeBytes + x;
            uint8_t intensity = addr[srcIdx] >> 4;
            uint8_t alpha = addr[srcIdx] & 0xf;
            mTexUploadBuffer[4 * i + 0] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 1] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 2] = SCALE_4_8(intensity);
            mTexUploadBuffer[4 * i + 3] = SCALE_4_8(alpha);
            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureIA16(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureIA16: null texture address for tile {}", tile);
        return;
    }

    uint32_t size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t full_image_line_size_bytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(line_size_bytes, full_image_line_size_bytes, size_bytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes / 2;
    uint32_t height = widthBytes > 0 ? size_bytes / widthBytes : 0;

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (full_image_line_size_bytes == size_bytes) {
        full_image_line_size_bytes = width * 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (full_image_line_size_bytes / 2)) + (x);

            uint8_t intensity = addr[2 * clrIdx];
            uint8_t alpha = addr[2 * clrIdx + 1];
            uint8_t r = intensity;
            uint8_t g = intensity;
            uint8_t b = intensity;
            mTexUploadBuffer[4 * i + 0] = r;
            mTexUploadBuffer[4 * i + 1] = g;
            mTexUploadBuffer[4 * i + 2] = b;
            mTexUploadBuffer[4 * i + 3] = alpha;

            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureI4(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureI4: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t widthBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                               mRdp->texture_tile[tile].line_size_bytes);
    uint32_t width = widthBytes * 2;
    uint32_t height = widthBytes > 0 ? sizeBytes / widthBytes : 0;

    // A single line of pixels should not equal the entire image (height == 1 non-withstanding)
    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width / 2;
    }

    uint32_t i = 0;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t clrIdx = (y * (fullImageLineSizeBytes * 2)) + (x);

            uint8_t byte = addr[clrIdx / 2];
            uint8_t part = (byte >> (4 - (clrIdx % 2) * 4)) & 0xf;
            uint8_t intensity = part;
            uint8_t r = intensity;
            uint8_t g = intensity;
            uint8_t b = intensity;
            uint8_t a = intensity;
            mTexUploadBuffer[4 * i + 0] = SCALE_4_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_4_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_4_8(b);
            mTexUploadBuffer[4 * i + 3] = SCALE_4_8(a);

            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureI8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureI8: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    uint32_t width = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                          mRdp->texture_tile[tile].line_size_bytes);
    uint32_t height = width > 0 ? sizeBytes / width : 0;

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = width;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint8_t intensity = addr[y * fullImageLineSizeBytes + x];
            mTexUploadBuffer[4 * i + 0] = intensity;
            mTexUploadBuffer[4 * i + 1] = intensity;
            mTexUploadBuffer[4 * i + 2] = intensity;
            mTexUploadBuffer[4 * i + 3] = intensity;
            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureCi4(int tile, bool importReplacement) {
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureCi4: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;
    uint32_t palIdx = mRdp->texture_tile[tile].palette; // 0-15

    const uint8_t* palette;

    if (mRdp->palettes[palIdx / 8] == nullptr) {
        SPDLOG_WARN("CI4: null palette slot {} for palIdx={}", palIdx / 8, palIdx);
        return;
    }
    palette = mRdp->palettes[palIdx / 8] + (palIdx % 8) * 16 * 2;

    uint32_t baseLineSizeBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                                      mRdp->texture_tile[tile].line_size_bytes);
    uint32_t resultLineSizeBytes = baseLineSizeBytes;

    if (metadata->h_byte_scale != 1) {
        resultLineSizeBytes *= metadata->h_byte_scale;
    }

    // CI4: 2 pixels per byte
    uint32_t width = resultLineSizeBytes * 2;
    uint32_t height = resultLineSizeBytes > 0 ? sizeBytes / resultLineSizeBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].uls, mRdp->texture_tile[tile].lrs);
    uint32_t tile_h = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].ult, mRdp->texture_tile[tile].lrt);
    bool pyramidLike = IsPyramidLike(width, height, tile_w, tile_h, metadata->h_byte_scale, metadata->v_pixel_scale);
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    // A masked axis wraps every 2^mask texels, so trim an over-loaded texture back to that.
    // Skip a mask smaller than the tile region though - that's stale tile state, not a real load.
    uint32_t maskW = mRdp->texture_tile[tile].masks;
    uint32_t maskH = mRdp->texture_tile[tile].maskt;
    if (maskW != 0 && (1u << maskW) >= tile_w && (1u << maskW) < width) {
        width = 1u << maskW;
    }
    if (maskH != 0 && (1u << maskH) >= tile_h && (1u << maskH) < height) {
        height = 1u << maskH;
    }
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    if (fullImageLineSizeBytes == sizeBytes) {
        fullImageLineSizeBytes = resultLineSizeBytes;
    }

    uint32_t i = 0;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t srcPixelIdx = y * (fullImageLineSizeBytes * 2) + x;
            uint8_t byte = addr[srcPixelIdx / 2];
            uint8_t idx = (byte >> (4 - (srcPixelIdx % 2) * 4)) & 0xf;
            uint16_t col16 = (palette[idx * 2] << 8) | palette[idx * 2 + 1]; // Big endian load
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;
            i++;
        }
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureCi8(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureCi8: null texture address for tile {}", tile);
        return;
    }

    uint32_t sizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t lineSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    if (mRdp->palettes[0] == nullptr || mRdp->palettes[1] == nullptr) {
        SPDLOG_WARN("CI8: null palette (pal0={}, pal1={})", static_cast<const void*>(mRdp->palettes[0]),
                    static_cast<const void*>(mRdp->palettes[1]));
        return;
    }

    for (uint32_t i = 0, j = 0; i < sizeBytes; j += fullImageLineSizeBytes - lineSizeBytes) {
        for (uint32_t k = 0; k < lineSizeBytes; i++, k++, j++) {
            uint8_t idx = addr[j];
            uint16_t col16 = (mRdp->palettes[idx / 128][(idx % 128) * 2] << 8) |
                             mRdp->palettes[idx / 128][(idx % 128) * 2 + 1]; // Big endian load
            uint8_t a = col16 & 1;
            uint8_t r = col16 >> 11;
            uint8_t g = (col16 >> 6) & 0x1f;
            uint8_t b = (col16 >> 1) & 0x1f;
            mTexUploadBuffer[4 * i + 0] = SCALE_5_8(r);
            mTexUploadBuffer[4 * i + 1] = SCALE_5_8(g);
            mTexUploadBuffer[4 * i + 2] = SCALE_5_8(b);
            mTexUploadBuffer[4 * i + 3] = a ? 255 : 0;
        }
    }

    uint32_t baseLineSizeBytes = GetEffectiveLineSize(lineSizeBytes, fullImageLineSizeBytes, sizeBytes,
                                                      mRdp->texture_tile[tile].line_size_bytes);
    uint32_t resultLineSizeBytes = baseLineSizeBytes;
    if (metadata->h_byte_scale != 1) {
        resultLineSizeBytes *= metadata->h_byte_scale;
    }

    uint32_t width = resultLineSizeBytes;
    uint32_t height = resultLineSizeBytes > 0 ? sizeBytes / resultLineSizeBytes : 0;

    // Clamp to the rendered region only when the loaded buffer is ~1.33x of it (mipmap
    // pyramid signature). Window-scrolling tiles have loaded ≈ rendered or loaded >> rendered;
    // skip both. CLAMP wrap mode always opts in.
    uint32_t tile_w = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].uls, mRdp->texture_tile[tile].lrs);
    uint32_t tile_h = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].ult, mRdp->texture_tile[tile].lrt);
    bool pyramidLike = IsPyramidLike(width, height, tile_w, tile_h, metadata->h_byte_scale, metadata->v_pixel_scale);
    bool clampS = (mRdp->texture_tile[tile].cms & G_TX_CLAMP) != 0;
    bool clampT = (mRdp->texture_tile[tile].cmt & G_TX_CLAMP) != 0;
    // A masked axis wraps every 2^mask texels, so trim an over-loaded texture back to that.
    // Skip a mask smaller than the tile region though - that's stale tile state, not a real load.
    uint32_t maskW = mRdp->texture_tile[tile].masks;
    uint32_t maskH = mRdp->texture_tile[tile].maskt;
    if (maskW != 0 && (1u << maskW) >= tile_w && (1u << maskW) < width) {
        width = 1u << maskW;
    }
    if (maskH != 0 && (1u << maskH) >= tile_h && (1u << maskH) < height) {
        height = 1u << maskH;
    }
    if ((pyramidLike || clampS) && tile_w > 0 && tile_w < width) {
        width = tile_w;
    }
    if ((pyramidLike || clampT) && tile_h > 0 && tile_h < height) {
        height = tile_h;
    }

    UploadTextureStyled(mTexUploadBuffer, width, height);
}

void Interpreter::ImportTextureImg(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureImg: null texture address for tile {}", tile);
        return;
    }

    uint16_t width = metadata->width;
    uint16_t height = metadata->height;
    UploadTextureStyled(addr, width, height);
}

void Interpreter::ImportTextureRaw(int tile, bool importReplacement) {
    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* addr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr;

    if (addr == nullptr) {
        SPDLOG_ERROR("ImportTextureRaw: null texture address for tile {}", tile);
        return;
    }

    uint16_t width = metadata->width;
    uint16_t height = metadata->height;
    Fast::TextureType type = metadata->type;
    std::shared_ptr<Fast::Texture> resource = metadata->resource;

    // if texture type is CI4 or CI8 we need to apply tlut to it
    switch (type) {
        case Fast::TextureType::Palette4bpp:
            ImportTextureCi4(tile, importReplacement);
            return;
        case Fast::TextureType::Palette8bpp:
            ImportTextureCi8(tile, importReplacement);
            return;
        default:
            break;
    }

    uint32_t numLoadedBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
    uint32_t numOriginallyLoadedBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;

    uint32_t resultOrigLineSize = mRdp->texture_tile[tile].line_size_bytes;
    switch (mRdp->texture_tile[tile].siz) {
        case G_IM_SIZ_32b:
            resultOrigLineSize *= 2;
            break;
    }
    uint32_t resultOrigHeight = numOriginallyLoadedBytes / resultOrigLineSize;
    uint32_t resultNewLineSize = resultOrigLineSize * metadata->h_byte_scale;
    uint32_t resultNewHeight = resultOrigHeight * metadata->v_pixel_scale;

    if (resultNewLineSize == 4 * width && resultNewHeight == height) {
        // Can use the texture directly since it has the correct dimensions
        UploadTextureStyled(addr, width, height);
        return;
    }

    uint32_t fullImageLineSizeBytes =
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
    uint32_t line_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;

    // Get the resource's true image size
    uint32_t resourceImageSizeBytes = resource->ImageDataSize;
    uint32_t safeFullImageLineSizeBytes = fullImageLineSizeBytes;
    uint32_t safeLineSizeBytes = line_size_bytes;
    uint32_t safeLoadedBytes = numLoadedBytes;

    // Sometimes the texture load commands will specify a size larger than the authentic texture
    // Normally the OOB info is read as garbage, but will cause a crash on some platforms
    // Restrict the bytes to a safe amount
    if (numLoadedBytes > resourceImageSizeBytes) {
        safeLoadedBytes = resourceImageSizeBytes;
        safeLineSizeBytes = resourceImageSizeBytes;
        safeFullImageLineSizeBytes = resourceImageSizeBytes;
    }

    // Safely only copy the amount of bytes the resource can allow
    for (uint32_t i = 0, j = 0; i < safeLoadedBytes; i += safeLineSizeBytes, j += safeFullImageLineSizeBytes) {
        memcpy(mTexUploadBuffer + i, addr + j, safeLineSizeBytes);
    }

    // Set the remaining bytes to load as 0
    if (numLoadedBytes > resourceImageSizeBytes) {
        memset(mTexUploadBuffer + resourceImageSizeBytes, 0, numLoadedBytes - resourceImageSizeBytes);
    }

    // Describe the buffer by what was actually packed (the loaded HD stride)
    uint32_t uploadWidth = safeLineSizeBytes / 4;
    uint32_t uploadHeight = safeLineSizeBytes > 0 ? safeLoadedBytes / safeLineSizeBytes : 0;
    const bool singleLineLoad = uploadHeight <= 1 && resultNewLineSize != 0 && resultNewLineSize < safeLineSizeBytes;
    if (uploadWidth > (uint32_t)mRapi->GetMaxTextureSize() || singleLineLoad) {
        if (safeLoadedBytes == (uint64_t)width * height * 4) {
            // buffer holds the full image, use its real dimensions
            uploadWidth = width;
            uploadHeight = height;
        } else {
            // partial load, fall back to the tile-derived dimensions
            uploadWidth = resultNewLineSize / 4;
            uploadHeight = resultNewHeight;
        }
    }
    UploadTextureStyled(mTexUploadBuffer, uploadWidth, uploadHeight);
}

void Interpreter::ImportTexture(int i, int tile, bool importReplacement) {
    uint8_t fmt = mRdp->texture_tile[tile].fmt;
    uint8_t siz = mRdp->texture_tile[tile].siz;
    uint32_t texFlags = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags;
    uint32_t tmemIdex = mRdp->texture_tile[tile].tmem_index;
    uint8_t paletteIndex = mRdp->texture_tile[tile].palette;
    uint32_t origSizeBytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;

    const RawTexMetadata* metadata = &mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
    const uint8_t* origAddr =
        importReplacement && (metadata->resource != nullptr)
            ? mMaskedTextures.find(GetBaseTexturePath(metadata->resource->GetInitData()->Path))->second.replacementData
            : mRdp->loaded_texture[tmemIdex].addr;

    // Check if this texture address is a registered GPU framebuffer mirror.
    // If so, bind the GPU FB directly — full resolution, no CPU readback needed.
    if (origAddr != nullptr && !importReplacement) {
        auto fbIt = mFbTextures.find((uintptr_t)origAddr);
        if (fbIt != mFbTextures.end()) {
            Flush();
            mRapi->SelectTextureFb(fbIt->second);
            mRdp->textures_changed[i] = false;
            return;
        }
    }

    if (origAddr == nullptr) {
        // Try the other TMEM slot -- some multi-tile setups only load one slot
        // and expect both tiles to reference it.
        uint32_t otherTmem = tmemIdex ^ 1;
        origAddr = mRdp->loaded_texture[otherTmem].addr;
        if (origAddr == nullptr) {
            SPDLOG_WARN("ImportTexture: null texture address for tile {} (both TMEM slots empty)", tile);
            return;
        }
        SPDLOG_WARN("ImportTexture: tile {} TMEM slot {} empty, falling back to slot {}", tile, tmemIdex, otherTmem);
        tmemIdex = otherTmem;
        origSizeBytes = mRdp->loaded_texture[otherTmem].orig_size_bytes;
        texFlags = mRdp->loaded_texture[otherTmem].tex_flags;
    }

    // Use palette_dram_addr (the original DRAM source) instead of palettes[]
    // (which always points to the staging buffer) so the same texture drawn
    // with different palettes gets distinct cache entries.
    TextureCacheKey key;
    if (fmt == G_IM_FMT_CI) {
        if (siz == G_IM_SIZ_4b) {
            uint8_t palSlot = paletteIndex / 8;
            key = { origAddr,
                    { palSlot == 0 ? mRdp->palette_dram_addr[0] : nullptr,
                      palSlot == 1 ? mRdp->palette_dram_addr[1] : nullptr },
                    fmt,
                    siz,
                    paletteIndex,
                    origSizeBytes };
        } else {
            // CI8 uses both palette halves
            key = { origAddr,     { mRdp->palette_dram_addr[0], mRdp->palette_dram_addr[1] }, fmt, siz, paletteIndex,
                    origSizeBytes };
        }
    } else {
        key = { origAddr, {}, fmt, siz, paletteIndex, origSizeBytes };
    }

    if (mFlattenUpload) {
        key.palette_index |= 0x80; // MergeShip2: the flat-colour copy of a texture is cached beside the real one
    }

    if (TextureCacheLookup(i, key)) {
        return;
    }

    // Guard against zero-sized textures that would cause divide-by-zero
    // or GPU API errors in UploadTexture.
    if (mRdp->texture_tile[tile].line_size_bytes == 0 || mRdp->loaded_texture[tmemIdex].size_bytes == 0 ||
        origAddr == nullptr) {
        return;
    }

    if ((texFlags & TEX_FLAG_LOAD_AS_IMG) != 0) {
        ImportTextureImg(tile, importReplacement);
        return;
    }

    // if load as raw is set then we load_raw();
    if ((texFlags & TEX_FLAG_LOAD_AS_RAW) != 0) {
        ImportTextureRaw(tile, importReplacement);
        return;
    }

    switch (fmt) {
        case G_IM_FMT_RGBA:
            if (siz == G_IM_SIZ_16b) {
                ImportTextureRgba16(tile, importReplacement);
            } else if (siz == G_IM_SIZ_32b) {
                ImportTextureRgba32(tile, importReplacement);
            } else {
                SPDLOG_ERROR("RGBA Texture that isn't 16 or 32 bit. Size = {}", siz);
                // OTRTODO: Sometimes, seemingly randomly, we end up here. Could be a bad dlist, could be
                // something F3D does not have supported. Further investigation is needed.
            }
            break;
        case G_IM_FMT_IA:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureIA4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureIA8(tile, importReplacement);
            } else if (siz == G_IM_SIZ_16b) {
                ImportTextureIA16(tile, importReplacement);
            } else {
                SPDLOG_ERROR("IA Texture that isn't 4, 8, or 16 bit. Size = {}", siz);
                ;
            }
            break;
        case G_IM_FMT_CI:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureCi4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureCi8(tile, importReplacement);
            } else if (siz == G_IM_SIZ_16b) {
                // CI+16b is hardware-invalid on N64. The tile's fmt is likely
                // stale from a prior draw. Decode as RGBA16 instead.
                ImportTextureRgba16(tile, importReplacement);
            } else if (siz == G_IM_SIZ_32b) {
                ImportTextureRgba32(tile, importReplacement);
            } else {
                SPDLOG_ERROR("CI Texture with unexpected size = {}", siz);
            }
            break;
        case G_IM_FMT_I:
            if (siz == G_IM_SIZ_4b) {
                ImportTextureI4(tile, importReplacement);
            } else if (siz == G_IM_SIZ_8b) {
                ImportTextureI8(tile, importReplacement);
            } else {
                SPDLOG_ERROR("I Texture that isn't 4 or 8 bit. Size = {}", siz);
            }
            break;
        case G_IM_FMT_YUV:
            SPDLOG_ERROR("YUV Textures not supported");
            break;
        default:
            SPDLOG_ERROR("Invalid texture format. Fmt = {}", fmt);
            break;
    }
}

void Interpreter::ImportTextureMask(int i, int tile) {
    uint32_t tmemIndex = mRdp->texture_tile[tile].tmem_index;
    RawTexMetadata metadata = mRdp->loaded_texture[tmemIndex].raw_tex_metadata;

    if (metadata.resource == nullptr) {
        return;
    }

    auto maskIter = mMaskedTextures.find(GetBaseTexturePath(metadata.resource->GetInitData()->Path));
    if (maskIter == mMaskedTextures.end()) {
        return;
    }

    const uint8_t* orig_addr = maskIter->second.mask;

    if (orig_addr == nullptr) {
        return;
    }

    TextureCacheKey key = { orig_addr, {}, 0, 0, 0, 0 };

    if (TextureCacheLookup(i, key)) {
        return;
    }

    uint32_t width = mRdp->texture_tile[tile].line_size_bytes;
    uint32_t height = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes /
                      mRdp->texture_tile[tile].line_size_bytes;
    switch (mRdp->texture_tile[tile].siz) {
        case G_IM_SIZ_4b:
            width *= 2;
            break;
        case G_IM_SIZ_8b:
        default:
            break;
        case G_IM_SIZ_16b:
            width /= 2;
            break;
        case G_IM_SIZ_32b:
            width /= 4;
            break;
    }

    for (uint32_t texIndex = 0; texIndex < width * height; texIndex++) {
        uint8_t masked = orig_addr[texIndex];
        if (masked) {
            mTexUploadBuffer[4 * texIndex + 0] = 0;
            mTexUploadBuffer[4 * texIndex + 1] = 0;
            mTexUploadBuffer[4 * texIndex + 2] = 0;
            mTexUploadBuffer[4 * texIndex + 3] = 0xFF;
        } else {
            mTexUploadBuffer[4 * texIndex + 0] = 0;
            mTexUploadBuffer[4 * texIndex + 1] = 0;
            mTexUploadBuffer[4 * texIndex + 2] = 0;
            mTexUploadBuffer[4 * texIndex + 3] = 0;
        }
    }

    mRapi->UploadTexture(mTexUploadBuffer, width, height);
}

void Interpreter::NormalizeVector(float v[3]) {
    float s = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= s;
    v[1] /= s;
    v[2] /= s;
}

void Interpreter::TransposedMatrixMul(float res[3], const float a[3], const float b[4][4]) {
    res[0] = a[0] * b[0][0] + a[1] * b[0][1] + a[2] * b[0][2];
    res[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2];
    res[2] = a[0] * b[2][0] + a[1] * b[2][1] + a[2] * b[2][2];
}

void Interpreter::MatrixMul(float res[4][4], const float a[4][4], const float b[4][4]) {
    float tmp[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(res, tmp, sizeof(tmp));
}

void Interpreter::CalculateNormalDir(const F3DLight_t* light, float coeffs[3]) {
    float light_dir[3] = { light->dir[0] / 127.0f, light->dir[1] / 127.0f, light->dir[2] / 127.0f };

    Interpreter::TransposedMatrixMul(coeffs, light_dir,
                                     mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);
    Interpreter::NormalizeVector(coeffs);
}

void Interpreter::GfxSpMatrix(uint8_t parameters, const int32_t* addr) {
    float matrix[4][4];

    if (auto it = mCurMtxReplacements->find((Mtx*)addr); it != mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = it->second.mf[i][j];
                int as_int = (int)(v * 65536.0f);
                matrix[i][j] = as_int * (1.0f / 65536.0f);
            }
        }
    } else {
#ifndef GBI_FLOATS
        // Original GBI where fixed point matrices are used
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j += 2) {
                int32_t int_part = addr[i * 2 + j / 2];
                uint32_t frac_part = addr[8 + i * 2 + j / 2];
                matrix[i][j] = (int32_t)((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
                matrix[i][j + 1] = (int32_t)((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
            }
        }
#else
        // For a modified GBI where fixed point values are replaced with floats
        memcpy(matrix, addr, sizeof(matrix));
#endif
    }

    const int8_t mtx_projection = get_attr(MTX_PROJECTION);
    const int8_t mtx_load = get_attr(MTX_LOAD);
    const int8_t mtx_push = get_attr(MTX_PUSH);

    if (parameters & mtx_projection) {
        if (parameters & mtx_load) {
            memcpy(mRsp->P_matrix, matrix, sizeof(matrix));
        } else {
            MatrixMul(mRsp->P_matrix, matrix, mRsp->P_matrix);
        }
    } else { // G_MTX_MODELVIEW
        if ((parameters & mtx_push) && mRsp->modelview_matrix_stack_size < 11) {
            ++mRsp->modelview_matrix_stack_size;
            memcpy(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1],
                   mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 2], sizeof(matrix));
        }
        if (parameters & mtx_load) {
            if (mRsp->modelview_matrix_stack_size == 0)
                ++mRsp->modelview_matrix_stack_size;
            memcpy(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], matrix, sizeof(matrix));
        } else {
            MatrixMul(mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], matrix,
                      mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);
        }
        mRsp->lights_changed = 1;
    }
    MatrixMul(mRsp->MP_matrix, mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], mRsp->P_matrix);
    if (mShadowPass) {
        ShadowMatrix();
    }
}

void Interpreter::GfxSpPopMatrix(uint32_t count) {
    while (count--) {
        if (mRsp->modelview_matrix_stack_size > 0) {
            --mRsp->modelview_matrix_stack_size;
            if (mRsp->modelview_matrix_stack_size > 0) {
                MatrixMul(mRsp->MP_matrix, mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1],
                          mRsp->P_matrix);
                if (mShadowPass) {
                    ShadowMatrix();
                }
            }
        }
    }
    mRsp->lights_changed = true;
}

float Interpreter::AdjXForAspectRatio(float x) const {
    // Skip widescreen adjustment for fixed-size off-screen FBs (HUD elements,
    // small capture buffers), or those which specify a fixed aspect ratio.
    if (mFbActive && mActiveFrameBuffer != mFrameBuffers.end() &&
        (!mActiveFrameBuffer->second.resize || mActiveFrameBuffer->second.forceFixedAspect)) {
        return x;
    } else {
        return x * (4.0f / 3.0f) / ((float)mCurDimensions.width / (float)mCurDimensions.height);
    }
}

// Scale the width and height value based on the ratio of the viewport to the native size
void Interpreter::AdjustWidthHeightForScale(uint32_t& width, uint32_t& height, uint32_t nativeWidth,
                                            uint32_t nativeHeight) const {
    width = round(width * (mCurDimensions.width / (2.0f * (nativeWidth / 2))));
    height = round(height * (mCurDimensions.height / (2.0f * (nativeHeight / 2))));

    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }
}

void Interpreter::GfxSpVertex(size_t n_vertices, size_t dest_index, const F3DVtx* vertices) {
    if (mShadowPass && sShadowNoCast) {
        return; // (MergeShip2 real-time shadows: these cast none, G_REGS_EX 11)
    }
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const F3DVtx_t* v = &vertices[i].v;
        const F3DVtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &mRsp->loaded_vertices[dest_index];

        if (v == nullptr) {
            return;
        }

        float x = v->ob[0] * mRsp->MP_matrix[0][0] + v->ob[1] * mRsp->MP_matrix[1][0] +
                  v->ob[2] * mRsp->MP_matrix[2][0] + mRsp->MP_matrix[3][0];
        float y = v->ob[0] * mRsp->MP_matrix[0][1] + v->ob[1] * mRsp->MP_matrix[1][1] +
                  v->ob[2] * mRsp->MP_matrix[2][1] + mRsp->MP_matrix[3][1];
        float z = v->ob[0] * mRsp->MP_matrix[0][2] + v->ob[1] * mRsp->MP_matrix[1][2] +
                  v->ob[2] * mRsp->MP_matrix[2][2] + mRsp->MP_matrix[3][2];
        float w = v->ob[0] * mRsp->MP_matrix[0][3] + v->ob[1] * mRsp->MP_matrix[1][3] +
                  v->ob[2] * mRsp->MP_matrix[2][3] + mRsp->MP_matrix[3][3];

        float world_pos[3] = { 0.0 };
        if (mRsp->geometry_mode & G_LIGHTING_POSITIONAL) {
            float(*mtx)[4] = mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1];
            world_pos[0] = v->ob[0] * mtx[0][0] + v->ob[1] * mtx[1][0] + v->ob[2] * mtx[2][0] + mtx[3][0];
            world_pos[1] = v->ob[0] * mtx[0][1] + v->ob[1] * mtx[1][1] + v->ob[2] * mtx[2][1] + mtx[3][1];
            world_pos[2] = v->ob[0] * mtx[0][2] + v->ob[1] * mtx[1][2] + v->ob[2] * mtx[2][2] + mtx[3][2];
        }

        if (sSway != nullptr) {
            // MergeShip2 swaying grass (SwayEx): the vertex's place moves sideways in the world, which is through
            // the view the projection carries (or the light's, in its pass)
            float weight;
            if (sSway->byHeight > 0.0f) {
                weight = (v->ob[1] - sSway->base) * sSway->byHeight;
            } else if (sSway->byHeight < 0.0f) {
                weight = sSwayTable != nullptr ? (float)sSwayTable[i] * (1.0f / 255.0f) : 0.0f;
            } else {
                weight = (float)(v->flag & 0xFF) * (1.0f / 255.0f);
            }
            if (weight > 0.0f) {
                weight = weight > 1.0f ? 1.0f : weight;
                const float(*m)[4] = mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1];
                const float wx = v->ob[0] * m[0][0] + v->ob[1] * m[1][0] + v->ob[2] * m[2][0] + m[3][0];
                const float wz = v->ob[0] * m[0][2] + v->ob[1] * m[1][2] + v->ob[2] * m[2][2] + m[3][2];
                float sx, sz;
                SwayAt(wx, wz, &sx, &sz);
                const float reach = sSway->amp * weight * weight;
                const float(*c)[4] = mShadowPass ? mShadowLight : mRsp->P_matrix;
                sx *= reach;
                sz *= reach;
                x += sx * c[0][0] + sz * c[2][0];
                y += sx * c[0][1] + sz * c[2][1];
                z += sx * c[0][2] + sz * c[2][2];
                w += sx * c[0][3] + sz * c[2][3];
            }
        }

        if (!mShadowPass) { // (MergeShip2 real-time shadows: the light's view is square)
            x = AdjXForAspectRatio(x);
        }

        short U = v->tc[0] * mRsp->texture_scaling_factor.s >> 16;
        short V = v->tc[1] * mRsp->texture_scaling_factor.t >> 16;

        if ((mRsp->geometry_mode & G_LIGHTING) && !mShadowPass) { // (the light's pass is shapes alone)
            if (mRsp->lights_changed) {
                for (int i = 0; i < mRsp->current_num_lights - 1; i++) {
                    CalculateNormalDir(&mRsp->current_lights[i].l, mRsp->current_lights_coeffs[i]);
                }
                /*static const Light_t lookat_x = {{0, 0, 0}, 0, {0, 0, 0}, 0, {127, 0, 0}, 0};
                static const Light_t lookat_y = {{0, 0, 0}, 0, {0, 0, 0}, 0, {0, 127, 0}, 0};*/
                CalculateNormalDir(&mRsp->lookat[0], mRsp->current_lookat_coeffs[0]);
                CalculateNormalDir(&mRsp->lookat[1], mRsp->current_lookat_coeffs[1]);
                mRsp->lights_changed = false;
            }

            int r = mRsp->current_lights[mRsp->current_num_lights - 1].l.col[0];
            int g = mRsp->current_lights[mRsp->current_num_lights - 1].l.col[1];
            int b = mRsp->current_lights[mRsp->current_num_lights - 1].l.col[2];

            for (int i = 0; i < mRsp->current_num_lights - 1; i++) {
                float intensity = 0;
                if ((mRsp->geometry_mode & G_LIGHTING_POSITIONAL) && (mRsp->current_lights[i].p.unk3 != 0)) {
                    // Calculate distance from the light to the vertex
                    float dist_vec[3] = { mRsp->current_lights[i].p.pos[0] - world_pos[0],
                                          mRsp->current_lights[i].p.pos[1] - world_pos[1],
                                          mRsp->current_lights[i].p.pos[2] - world_pos[2] };
                    float dist_sq =
                        dist_vec[0] * dist_vec[0] + dist_vec[1] * dist_vec[1] +
                        dist_vec[2] * dist_vec[2] * 2; // The *2 comes from GLideN64, unsure of why it does it
                    float dist = sqrt(dist_sq);

                    // Transform distance vector (which acts as a direction light vector) into model's space
                    float light_model[3];
                    TransposedMatrixMul(light_model, dist_vec,
                                        mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1]);

                    // Calculate intensity for each axis using standard formula for intensity
                    float light_intensity[3];
                    for (int light_i = 0; light_i < 3; light_i++) {
                        light_intensity[light_i] = 4.0f * light_model[light_i] / dist_sq;
                        light_intensity[light_i] = std::clamp(light_intensity[light_i], -1.0f, 1.0f);
                    }

                    // Adjust intensity based on surface normal and sum up total
                    float total_intensity =
                        light_intensity[0] * vn->n[0] + light_intensity[1] * vn->n[1] + light_intensity[2] * vn->n[2];
                    total_intensity = std::clamp(total_intensity, -1.0f, 1.0f);

                    // Attenuate intensity based on attenuation values.
                    // Example formula found at https://ogldev.org/www/tutorial20/tutorial20.html
                    // Specific coefficients for MM's microcode sourced from GLideN64
                    // https://github.com/gonetz/GLideN64/blob/3b43a13a80dfc2eb6357673440b335e54eaa3896/src/gSP.cpp#L636
                    float distf = floorf(dist);
                    float attenuation = (distf * mRsp->current_lights[i].p.unk7 * 2.0f +
                                         distf * distf * mRsp->current_lights[i].p.unkE / 8.0f) /
                                            (float)0xFFFF +
                                        1.0f;
                    intensity = total_intensity / attenuation;
                } else {
                    intensity += vn->n[0] * mRsp->current_lights_coeffs[i][0];
                    intensity += vn->n[1] * mRsp->current_lights_coeffs[i][1];
                    intensity += vn->n[2] * mRsp->current_lights_coeffs[i][2];
                    intensity /= 127.0f;
                }
                if (intensity > 0.0f) {
                    r += intensity * mRsp->current_lights[i].l.col[0];
                    g += intensity * mRsp->current_lights[i].l.col[1];
                    b += intensity * mRsp->current_lights[i].l.col[2];
                }
            }

            d->color.r = r > 255 ? 255 : r;
            d->color.g = g > 255 ? 255 : g;
            d->color.b = b > 255 ? 255 : b;

            if (dest_index < MAX_VERTICES + 4) { // MergeShip2 (toon outlines): where the normal points on screen
                sOutlineNormal[dest_index][0] =
                    AdjXForAspectRatio(vn->n[0] * mRsp->MP_matrix[0][0] + vn->n[1] * mRsp->MP_matrix[1][0] +
                                       vn->n[2] * mRsp->MP_matrix[2][0]);
                sOutlineNormal[dest_index][1] = vn->n[0] * mRsp->MP_matrix[0][1] + vn->n[1] * mRsp->MP_matrix[1][1] +
                                                vn->n[2] * mRsp->MP_matrix[2][1];
                sOutlineHasNormal[dest_index] = true;
            }

            if (mRsp->geometry_mode & G_TEXTURE_GEN) {
                float dotx = 0, doty = 0;
                dotx += vn->n[0] * mRsp->current_lookat_coeffs[0][0];
                dotx += vn->n[1] * mRsp->current_lookat_coeffs[0][1];
                dotx += vn->n[2] * mRsp->current_lookat_coeffs[0][2];
                doty += vn->n[0] * mRsp->current_lookat_coeffs[1][0];
                doty += vn->n[1] * mRsp->current_lookat_coeffs[1][1];
                doty += vn->n[2] * mRsp->current_lookat_coeffs[1][2];

                dotx /= 127.0f;
                doty /= 127.0f;

                dotx = Ship::Math::clamp(dotx, -1.0f, 1.0f);
                doty = Ship::Math::clamp(doty, -1.0f, 1.0f);

                if (mRsp->geometry_mode & G_TEXTURE_GEN_LINEAR) {
                    // Not sure exactly what formula we should use to get accurate values
                    /*dotx = (2.906921f * dotx * dotx + 1.36114f) * dotx;
                    doty = (2.906921f * doty * doty + 1.36114f) * doty;
                    dotx = (dotx + 1.0f) / 4.0f;
                    doty = (doty + 1.0f) / 4.0f;*/
                    dotx = acosf(-dotx) /* M_PI */ * 0.159155f;
                    doty = acosf(-doty) /* M_PI */ * 0.159155f;
                } else {
                    dotx = (dotx + 1.0f) / 4.0f;
                    doty = (doty + 1.0f) / 4.0f;
                }

                U = (int32_t)(dotx * mRsp->texture_scaling_factor.s);
                V = (int32_t)(doty * mRsp->texture_scaling_factor.t);
            }
        } else {
            d->color.r = v->cn[0];
            d->color.g = v->cn[1];
            d->color.b = v->cn[2];
            if (dest_index < MAX_VERTICES + 4) {
                sOutlineHasNormal[dest_index] = false;
            }
        }

        d->u = U;
        d->v = V;

        // trivial clip rejection
        d->clip_rej = 0;
        if (x < -w) {
            d->clip_rej |= 1; // CLIP_LEFT
        }
        if (x > w) {
            d->clip_rej |= 2; // CLIP_RIGHT
        }
        if (y < -w) {
            d->clip_rej |= 4; // CLIP_BOTTOM
        }
        if (y > w) {
            d->clip_rej |= 8; // CLIP_TOP
        }
        // if (z < -w) d->clip_rej |= 16; // CLIP_NEAR
        if (z > w) {
            d->clip_rej |= 32; // CLIP_FAR
        }

        d->x = x;
        d->y = y;
        d->z = z;
        d->w = w;

        if (mRsp->geometry_mode & G_FOG) {
            if (fabsf(w) < 0.001f) {
                // To avoid division by zero
                w = 0.001f;
            }

            float winv = 1.0f / w;
            if (winv < 0.0f) {
                winv = std::numeric_limits<int16_t>::max();
            }

            float fog_z = z * winv * mRsp->fog_mul + mRsp->fog_offset;
            fog_z = Ship::Math::clamp(fog_z, 0.0f, 255.0f);
            d->color.a = fog_z; // Use alpha variable to store fog factor
        } else {
            d->color.a = v->cn[3];
        }
        d->alpha = v->cn[3];
    }
}

void Interpreter::GfxSpModifyVertex(uint16_t vtx_idx, uint8_t where, uint32_t val) {
    SUPPORT_CHECK(where == G_MWO_POINT_ST);

    int16_t s = (int16_t)(val >> 16);
    int16_t t = (int16_t)val;

    LoadedVertex* v = &mRsp->loaded_vertices[vtx_idx];
    v->u = s;
    v->v = t;
}

void Interpreter::GfxSpTri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, bool is_rect) {
    struct LoadedVertex* v1 = &mRsp->loaded_vertices[vtx1_idx];
    struct LoadedVertex* v2 = &mRsp->loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &mRsp->loaded_vertices[vtx3_idx];
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };

    if (mShadowPass && sShadowNoCast) {
        return; // (MergeShip2 real-time shadows: these cast none, G_REGS_EX 11)
    }
    // if (rand()%2) return;

    if (v1->clip_rej & v2->clip_rej & v3->clip_rej) {
        // The whole triangle lies outside the visible area
        return;
    }

    const uint32_t cull_both = get_attr(CULL_BOTH);
    const uint32_t cull_front = get_attr(CULL_FRONT);
    const uint32_t cull_back = get_attr(CULL_BACK);
    bool outline_only = false; // MergeShip2 (toon styles): a back face, drawn as ink

    // (MergeShip2 real-time shadows: out of doors a surface covers what is behind it from the light whichever way it
    // faces: a cliff drawn from one side only still stands between the sun and the ground)
    const bool shadowBothSides =
        mShadowPass && sShadowBothSides && (mRsp->geometry_mode & cull_both) != cull_both;
    if ((mRsp->geometry_mode & cull_both) != 0 && !shadowBothSides) {
        float dx1 = v1->x / (v1->w) - v2->x / (v2->w);
        float dy1 = v1->y / (v1->w) - v2->y / (v2->w);
        float dx2 = v3->x / (v3->w) - v2->x / (v2->w);
        float dy2 = v3->y / (v3->w) - v2->y / (v2->w);
        float cross = dx1 * dy2 - dy1 * dx2;

        if ((v1->w < 0) ^ (v2->w < 0) ^ (v3->w < 0)) {
            // If one vertex lies behind the eye, negating cross will give the correct result.
            // If all vertices lie behind the eye, the triangle will be rejected anyway.
            cross = -cross;
        }

        // G_EX_INVERT_CULLING is a LUS extension, not tied to a specific ucode,
        // so apply it regardless of the active microcode handler.
        if ((mRsp->extra_geometry_mode & G_EX_INVERT_CULLING) != 0) {
            cross = -cross;
        }

        auto cull_type = mRsp->geometry_mode & cull_both;

        if (cull_type == cull_front) {
            if (cross <= 0) {
                return;
            }
        } else if (cull_type == cull_back) {
            if (cross >= 0) {
                if (is_rect || cross == 0 || !OutlineStyle(this) || mShadowPass) {
                    return;
                }
                outline_only = true;
            }
        } else if (cull_type == cull_both) {
            // Why is this even an option?
            return;
        }
    }

    // depth_test is set when the fragment has a depth value to compare (either from vertex Z via
    // RSP G_ZBUFFER, or from the prim-depth register via G_ZS_PRIM) and Z_CMP is requested.
    bool zbuffer_enabled = (mRsp->geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    bool prim_depth_enabled = (mRdp->other_mode_l & G_ZS_PRIM) != 0;
    bool depth_test = (zbuffer_enabled || prim_depth_enabled) && (mRdp->other_mode_l & Z_CMP) == Z_CMP;
    bool depth_mask = (mRdp->other_mode_l & Z_UPD) == Z_UPD;
    // (in the light's pass: a solid surface, drawn as its shape alone -- see below)
    bool shadowPlain = false;
    if (mShadowPass) {
        // MergeShip2 real-time shadows, the light's pass: only what stands in the world and covers what is behind
        // it. Not the screen's rectangles, not what is drawn into another framebuffer or through a view that is
        // not the world's, nothing that leaves the depth alone (the sky, glows, the game's own round shadows), no
        // decals, and nothing that blends (water, glass): a cut-out's holes are its shader's doing and stay holes.
        const bool blends = ((mRdp->other_mode_l & (3 << 20)) == (G_BL_CLR_MEM << 20) &&
                             (mRdp->other_mode_l & (3 << 16)) == (G_BL_1MA << 16)) ||
                            ((mRdp->other_mode_l & (3 << 22)) == (G_BL_CLR_MEM << 22) &&
                             (mRdp->other_mode_l & (3 << 18)) == (G_BL_1MA << 18));
        const bool cutout = (mRdp->other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA ||
                            (mRdp->other_mode_l & (3U << G_MDSFT_ALPHACOMPARE)) == G_AC_THRESHOLD;
        // (explicit blend factors, G_SETBLENDMODE_EX: one and zero is a surface that covers; and so is the plain
        // mix by alpha where the surface writes its depth, which is how the 3DS characters' materials are drawn --
        // they blend so that they can fade. Water and glass leave the depth alone and are out by that already.)
        const uint32_t ex = mRdp->blend_ex_mode;
        const uint32_t exSrc = (ex >> 8) & 0xF;
        const uint32_t exDst = (ex >> 4) & 0xF;
        const bool exCovers = (exSrc == G_BLENDEX_ONE && exDst == G_BLENDEX_ZERO) ||
                              (exSrc == G_BLENDEX_SRC_ALPHA && exDst == G_BLENDEX_ONE_MINUS_SRC_ALPHA);
        const bool exBlends = ex != 0 && !exCovers;
        if (is_rect || mFbActive || !mShadowWorld || !depth_test || !depth_mask ||
            (mRdp->other_mode_l & ZMODE_DEC) == ZMODE_DEC || (blends && !cutout && ex == 0) || exBlends) {
            return;
        }
        // (a cut-out of the pack's is marked so for the toon styles' ink: its holes are its own shader's doing)
        shadowPlain = !cutout && (mRsp->extra_geometry_mode & kExNoOutline) == 0;
    }
    uint8_t depth_test_and_mask = (depth_test ? 1 : 0) | (depth_mask ? 2 : 0);
    if (depth_test_and_mask != mRenderingState.depth_test_and_mask) {
        Flush();
        mRapi->SetDepthTestAndMask(depth_test, depth_mask);
        mRenderingState.depth_test_and_mask = depth_test_and_mask;
    }

    bool zmode_decal = (mRdp->other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if (zmode_decal != mRenderingState.decal_mode) {
        Flush();
        mRapi->SetZmodeDecal(zmode_decal);
        mRenderingState.decal_mode = zmode_decal;
    }

    if (mRdp->viewport_or_scissor_changed && !mShadowPass) { // (the light's pass has its own, the whole map)
        if (memcmp(&mRdp->viewport, &mRenderingState.viewport, sizeof(mRdp->viewport)) != 0) {
            Flush();
            mRapi->SetViewport(mRdp->viewport.x, mRdp->viewport.y, mRdp->viewport.width, mRdp->viewport.height);
            mRenderingState.viewport = mRdp->viewport;
        }
        if (memcmp(&mRdp->scissor, &mRenderingState.scissor, sizeof(mRdp->scissor)) != 0) {
            Flush();
            mRapi->SetScissor(mRdp->scissor.x, mRdp->scissor.y, mRdp->scissor.width, mRdp->scissor.height);
            mRenderingState.scissor = mRdp->scissor;
        }
        mRdp->viewport_or_scissor_changed = false;
    }

    if (outline_only) {
        EmitOutlineTriangle(this, vtx1_idx, vtx2_idx, vtx3_idx);
        return;
    }

    uint64_t cc_id = mRdp->combine_mode;
    uint64_t cc_options = 0;
    bool use_alpha = ((mRdp->other_mode_l & (3 << 20)) == (G_BL_CLR_MEM << 20) &&
                      (mRdp->other_mode_l & (3 << 16)) == (G_BL_1MA << 16)) ||
                     ((mRdp->other_mode_l & (3 << 22)) == (G_BL_CLR_MEM << 22) &&
                      (mRdp->other_mode_l & (3 << 18)) == (G_BL_1MA << 18));
    uint8_t blend_src = mRdp->other_mode_l >> 30;
    bool use_blend_color = blend_src == G_BL_CLR_BL;
    bool use_fog = blend_src == G_BL_CLR_FOG || use_blend_color;
    bool texture_edge = (mRdp->other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
    bool use_noise = (mRdp->other_mode_l & (3U << G_MDSFT_ALPHACOMPARE)) == G_AC_DITHER;
    bool use_2cyc = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE;
    bool alpha_threshold = (mRdp->other_mode_l & (3U << G_MDSFT_ALPHACOMPARE)) == G_AC_THRESHOLD;
    bool invisible =
        (mRdp->other_mode_l & (3 << 24)) == (G_BL_0 << 24) && (mRdp->other_mode_l & (3 << 20)) == (G_BL_CLR_MEM << 20);
    bool use_grayscale = mRdp->grayscale;
    bool use_prim_depth = (mRdp->other_mode_l & G_ZS_PRIM) != 0;

    if (texture_edge) {
        if (use_alpha) {
            alpha_threshold = true;
            texture_edge = false;
        }
        use_alpha = true;
    }

    if (shadowPlain) {
        // MergeShip2 real-time shadows, the light's pass: a solid surface is only its shape there. All of them are
        // drawn with one plain shader, without texture or blending, so nothing changes from one material to the
        // next and they go to the card in a few large lots.
        use_alpha = use_fog = texture_edge = use_noise = use_2cyc = alpha_threshold = invisible = use_grayscale =
            false;
    }

    // MergeShip2: explicit blending (G_SETBLENDMODE_EX) needs the shader's alpha and blending on
    const uint32_t blend_ex_mode = shadowPlain ? 0 : mRdp->blend_ex_mode;
    if (blend_ex_mode != 0) {
        use_alpha = true;
    }

    // MergeShip2 render styles (RenderStyle in interpreter.h). The ones that reshade the world stay off screen
    // rectangles and everything drawn without a depth test, which is the text, the HUD and the menus.
    uint32_t render_style = mRenderStyle;
    if (render_style >= RENDER_STYLE_TOON && render_style <= RENDER_STYLE_FACETED && (is_rect || !depth_test)) {
        render_style = RENDER_STYLE_NONE;
    }
    if (mShadowPass) {
        render_style = RENDER_STYLE_NONE; // (real-time shadows: the light's pass is depth alone)
    }
    const bool flat_textures = render_style == RENDER_STYLE_FLAT || render_style == RENDER_STYLE_TOON_FLAT ||
                               render_style == RENDER_STYLE_FACETED;
    // The toon styles on a surface whose light is painted into its vertex colours (the rooms: nothing is computed
    // from normals there): finer bands with soft edges, kStyleToonPainted to the shaders. Hard bands cut such a
    // surface along its triangles. Surfaces drawn with a blend (water, glows, shadows) keep their smooth shade.
    if ((render_style == RENDER_STYLE_TOON || render_style == RENDER_STYLE_TOON_FLAT) &&
        (mRsp->geometry_mode & G_LIGHTING) == 0) {
        const uint32_t src = (blend_ex_mode >> 8) & 0xF;
        const uint32_t dst = (blend_ex_mode >> 4) & 0xF;
        const bool opaque =
            blend_ex_mode != 0 ? (src == G_BLENDEX_ONE && dst == G_BLENDEX_ZERO) : (!use_alpha || texture_edge);
        render_style = opaque ? kStyleToonPainted : (uint32_t)RENDER_STYLE_NONE;
    }
    if (flat_textures != mFlattenUpload) {
        mFlattenUpload = flat_textures;
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    const bool faceted = render_style == RENDER_STYLE_FACETED;
    cc_options |= (uint64_t)render_style << RENDER_STYLE_SHIFT;

    if (use_alpha) {
        cc_options |= SHADER_OPT(ALPHA);
    }
    if (use_fog) {
        cc_options |= SHADER_OPT(FOG);
    }
    if (texture_edge) {
        cc_options |= SHADER_OPT(TEXTURE_EDGE);
    }
    if (use_noise) {
        cc_options |= SHADER_OPT(NOISE);
    }
    if (use_2cyc) {
        cc_options |= SHADER_OPT(_2CYC);
    }
    if (alpha_threshold) {
        cc_options |= SHADER_OPT(ALPHA_THRESHOLD);
    }
    if (invisible) {
        cc_options |= SHADER_OPT(INVISIBLE);
    }
    if (use_grayscale) {
        cc_options |= SHADER_OPT(GRAYSCALE);
    }
    if (use_prim_depth) {
        cc_options |= SHADER_OPT(PRIM_DEPTH);
    }

    if (!mShaderStack.empty() && !shadowPlain) {
        cc_options |= ((uint64_t)(mShaderStack.top() & 0xFFFF) << SHADER_ID_SHIFT);
    } else {
        cc_options |= (uint64_t)0xFFFF << SHADER_ID_SHIFT;
    }

    if (!shadowPlain) {
        if (mRdp->loaded_texture[0].masked) {
            cc_options |= SHADER_OPT(TEXEL0_MASK);
        }
        if (mRdp->loaded_texture[1].masked) {
            cc_options |= SHADER_OPT(TEXEL1_MASK);
        }
        if (mRdp->loaded_texture[0].blended) {
            cc_options |= SHADER_OPT(TEXEL0_BLEND);
        }
        if (mRdp->loaded_texture[1].blended) {
            cc_options |= SHADER_OPT(TEXEL1_BLEND);
        }
    }

    ColorCombinerKey key;
    key.combine_mode = mRdp->combine_mode;
    if (shadowPlain) {
        // (the shade alone, colour and alpha: (0 - 0) * 0 + SHADE in both cycles)
        const uint64_t rgb = 15u | (15u << 4) | (31u << 8) | ((uint64_t)G_CCMUX_SHADE << 13);
        const uint64_t alpha = 7u | (7u << 3) | (7u << 6) | ((uint64_t)G_ACMUX_SHADE << 9);
        key.combine_mode = rgb | (alpha << 16) | (rgb << 28) | (alpha << 44);
    }
    if (!sStandInKinds.empty() && !shadowPlain) {
        // MergeShip2, the 3DS interface option: a marked stand-in (sStandInKinds)
        const RawTexMetadata& standIn =
            mRdp->loaded_texture[mRdp->texture_tile[mRdp->first_tile_index].tmem_index].raw_tex_metadata;
        const auto kind = standIn.resource != nullptr ? sStandInKinds.find(standIn.resource.get()) : sStandInKinds.end();
        if (kind != sStandInKinds.end()) {
            const uint32_t rgb = (uint32_t)(key.combine_mode & 0xFFFF);
            const uint32_t alpha = (uint32_t)((key.combine_mode >> 16) & 0xFFF);
            const bool reads = (rgb & 0xF) == G_CCMUX_TEXEL0 || ((rgb >> 4) & 0xF) == G_CCMUX_TEXEL0 ||
                               ((rgb >> 8) & 0x1F) == G_CCMUX_TEXEL0 || ((rgb >> 13) & 7) == G_CCMUX_TEXEL0 ||
                               (alpha & 7) == G_ACMUX_TEXEL0 || ((alpha >> 3) & 7) == G_ACMUX_TEXEL0 ||
                               ((alpha >> 6) & 7) == G_ACMUX_TEXEL0 || ((alpha >> 9) & 7) == G_ACMUX_TEXEL0;
            if (reads) {
                // (a - b) * c + d: the texture alone, or the texture by the primitive colour
                const uint32_t own = (uint32_t)G_CCMUX_TEXEL0 << 13;
                const uint32_t tint = G_CCMUX_TEXEL0 | ((G_CCMUX_0 & 0xF) << 4) | (G_CCMUX_PRIMITIVE << 8) |
                                      ((G_CCMUX_0 & 7) << 13);
                key.combine_mode = (key.combine_mode & ~(uint64_t)0xFFFF) | (kind->second == 1 ? own : tint);
            }
        }
    }
    key.options = cc_options;

    ColorCombiner* comb = LookupOrCreateColorCombiner(key);

    uint32_t tm = 0;
    uint32_t tex_width[2], tex_height[2], tex_width2[2], tex_height2[2];
    uint32_t effective_tile[2];

    for (int i = 0; i < 2; i++) {
        uint32_t tile = mRdp->first_tile_index + i;

        // No LOD support: force both slots to the base mip level.
        if (i == 1 && mRdp->first_tile_index >= 2) {
            tile = mRdp->first_tile_index;
        }
        effective_tile[i] = tile;

        if (comb->usedTextures[i]) {
            if (mRdp->textures_changed[i]) {
                Flush();
                ImportTexture(i, tile, false);
                if (mRdp->loaded_texture[i].masked) {
                    ImportTextureMask(SHADER_FIRST_MASK_TEXTURE + i, tile);
                }
                if (mRdp->loaded_texture[i].blended) {
                    ImportTexture(SHADER_FIRST_REPLACEMENT_TEXTURE + i, tile, true);
                }
                mRdp->textures_changed[i] = false;
            }

            uint8_t cms = mRdp->texture_tile[tile].cms;
            uint8_t cmt = mRdp->texture_tile[tile].cmt;

            uint32_t loaded_line_size = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes;
            uint32_t loaded_size = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes;
            uint32_t loaded_full_line =
                mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes;
            uint32_t tex_size_bytes;
            uint32_t line_size;
            if ((loaded_line_size != loaded_size || loaded_full_line != loaded_size) && loaded_line_size > 0) {
                line_size = loaded_line_size;
                tex_size_bytes = loaded_size;
            } else {
                line_size = mRdp->texture_tile[tile].line_size_bytes;
                tex_size_bytes = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes;
                // RGBA32: texture_tile stores TMEM-interleaved stride (half of actual DRAM stride).
                if (mRdp->texture_tile[tile].siz == G_IM_SIZ_32b) {
                    line_size *= 2;
                }
            }

            if (line_size == 0) {
                line_size = 1;
            }

            tex_height[i] = tex_size_bytes / line_size;
            switch (mRdp->texture_tile[tile].siz) {
                case G_IM_SIZ_4b:
                    line_size <<= 1;
                    break;
                case G_IM_SIZ_8b:
                    break;
                case G_IM_SIZ_16b:
                    line_size /= G_IM_SIZ_16b_LINE_BYTES;
                    break;
                case G_IM_SIZ_32b:
                    line_size /= 4; // RGBA32: 4 bytes per pixel (line_size is now actual DRAM stride)
                    break;
            }
            tex_width[i] = line_size;

            tex_width2[i] = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].uls, mRdp->texture_tile[tile].lrs);
            tex_height2[i] = GetTileSizeFromCoordinates(mRdp->texture_tile[tile].ult, mRdp->texture_tile[tile].lrt);

            // Same pyramid-like ratio gate as ImportTexture: only clamp when loaded pixels
            // are close to rendered pixels (mipmap), not when much bigger (window scroll).
            const RawTexMetadata& triMeta = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata;
            bool pyrLike = IsPyramidLike(tex_width[i], tex_height[i], tex_width2[i], tex_height2[i],
                                         triMeta.h_byte_scale, triMeta.v_pixel_scale);
            // Same wrap-period trim as the import paths. The >= tex_width2 guard skips a stale
            // mask left by an FB blit (the pause background), which would otherwise tile the FB.
            uint32_t maskW = mRdp->texture_tile[tile].masks;
            uint32_t maskH = mRdp->texture_tile[tile].maskt;
            if (maskW != 0 && (1u << maskW) >= tex_width2[i] && (1u << maskW) < tex_width[i]) {
                tex_width[i] = 1u << maskW;
            }
            if (maskH != 0 && (1u << maskH) >= tex_height2[i] && (1u << maskH) < tex_height[i]) {
                tex_height[i] = 1u << maskH;
            }
            if ((pyrLike || (cms & G_TX_CLAMP)) && tex_width2[i] > 0 && tex_width2[i] < tex_width[i]) {
                tex_width[i] = tex_width2[i];
            }
            if ((pyrLike || (cmt & G_TX_CLAMP)) && tex_height2[i] > 0 && tex_height2[i] < tex_height[i]) {
                tex_height[i] = tex_height2[i];
            }

            uint32_t tex_width1 = tex_width[i] << (cms & G_TX_MIRROR);
            uint32_t tex_height1 = tex_height[i] << (cmt & G_TX_MIRROR);

            if ((cms & G_TX_CLAMP) && ((cms & G_TX_MIRROR) || tex_width1 != tex_width2[i])) {
                tm |= 1 << 2 * i;
                cms &= ~G_TX_CLAMP;
            }
            if ((cmt & G_TX_CLAMP) && ((cmt & G_TX_MIRROR) || tex_height1 != tex_height2[i])) {
                tm |= 1 << 2 * i + 1;
                cmt &= ~G_TX_CLAMP;
            }

            if (mRenderingState.mTextures[i] == nullptr) {
                continue;
            }

            bool linear_filter = (mRdp->other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
            if (PixelStyle(mRenderStyle)) {
                linear_filter = false; // MergeShip2: hard texels for the 16-bit and 8-bit styles
            }
            if (linear_filter != mRenderingState.mTextures[i]->second.linear_filter ||
                cms != mRenderingState.mTextures[i]->second.cms || cmt != mRenderingState.mTextures[i]->second.cmt) {
                Flush();

                // Set the same sampler params on the blended texture. Needed for opengl.
                if (mRdp->loaded_texture[i].blended) {
                    mRapi->SetSamplerParameters(SHADER_FIRST_REPLACEMENT_TEXTURE + i, linear_filter, cms, cmt);
                }

                mRapi->SetSamplerParameters(i, linear_filter, cms, cmt);
                mRenderingState.mTextures[i]->second.linear_filter = linear_filter;
                mRenderingState.mTextures[i]->second.cms = cms;
                mRenderingState.mTextures[i]->second.cmt = cmt;
            }
        }
    }

    struct ShaderProgram* prg = comb->prg[tm];
    if (prg == NULL) {
        comb->prg[tm] = prg =
            LookupOrCreateShaderProgram(comb->shader_id0, comb->shader_id1 | tm * SHADER_OPT(TEXEL0_CLAMP_S));
    }
    if (prg != mRenderingState.mShaderProgram) {
        Flush();
        mRapi->UnloadShader(mRenderingState.mShaderProgram);
        mRapi->LoadShader(prg);
        mRenderingState.mShaderProgram = prg;
    }
    if (use_alpha != mRenderingState.alpha_blend) {
        Flush();
        mRapi->SetUseAlpha(use_alpha);
        mRenderingState.alpha_blend = use_alpha;
    }
    if (blend_ex_mode != mBlendExModeActive ||
        (blend_ex_mode != 0 && memcmp(&mRdp->blend_ex_color, &mBlendExColorActive, sizeof(RGBA)) != 0)) {
        Flush();
        const uint8_t constant[4] = { mRdp->blend_ex_color.r, mRdp->blend_ex_color.g, mRdp->blend_ex_color.b,
                                      mRdp->blend_ex_color.a };
        mRapi->SetBlendMode(blend_ex_mode, constant);
        mBlendExModeActive = blend_ex_mode;
        mBlendExColorActive = mRdp->blend_ex_color;
    }
    uint8_t numInputs;
    bool usedTextures[2];

    mRapi->ShaderGetInfo(prg, &numInputs, usedTextures);


    struct GfxClipParameters clip_parameters = mRapi->GetClipParameters();

    // MergeShip2, faceted style: the three vertex shades averaged into one
    RGBA facet_color[3];
    if (faceted) {
        RGBA mean;
        const float facet = FacetLight(this, v1, v2, v3) / 3.0f;
        mean.r = (uint8_t)std::min((v1->color.r + v2->color.r + v3->color.r) * facet, 255.0f);
        mean.g = (uint8_t)std::min((v1->color.g + v2->color.g + v3->color.g) * facet, 255.0f);
        mean.b = (uint8_t)std::min((v1->color.b + v2->color.b + v3->color.b) * facet, 255.0f);
        for (int i = 0; i < 3; i++) {
            facet_color[i] = mean;
            facet_color[i].a = v_arr[i]->color.a;
        }
    }

    for (int i = 0; i < 3; i++) {
        float z = v_arr[i]->z, w = v_arr[i]->w;
        if (clip_parameters.z_is_from_0_to_1) {
            z = (z + w) / 2.0f;
        }

        mBufVbo[mBufVboLen++] = v_arr[i]->x;
        mBufVbo[mBufVboLen++] = clip_parameters.invertY ? -v_arr[i]->y : v_arr[i]->y;
        mBufVbo[mBufVboLen++] = z;
        mBufVbo[mBufVboLen++] = w;

        for (int t = 0; t < 2; t++) {
            if (!usedTextures[t]) {
                continue;
            }
            float u = v_arr[i]->u / 32.0f;
            float v = v_arr[i]->v / 32.0f;
            if (t == 1 && (mRsp->extra_geometry_mode & G_EX_TEXEL1_UPPER_VERTEX)) {
                // MergeShip2: a second UV set, loaded 32 slots above the triangle's vertices
                const ptrdiff_t slot = v_arr[i] - mRsp->loaded_vertices;
                if (slot >= 0 && slot < 32) {
                    u = mRsp->loaded_vertices[slot + 32].u / 32.0f;
                    v = mRsp->loaded_vertices[slot + 32].v / 32.0f;
                }
            }

            uint32_t uv_tile = effective_tile[t];
            int shifts = mRdp->texture_tile[uv_tile].shifts;
            int shiftt = mRdp->texture_tile[uv_tile].shiftt;
            if (shifts != 0) {
                if (shifts <= 10) {
                    u /= 1 << shifts;
                } else {
                    u *= 1 << (16 - shifts);
                }
            }
            if (shiftt != 0) {
                if (shiftt <= 10) {
                    v /= 1 << shiftt;
                } else {
                    v *= 1 << (16 - shiftt);
                }
            }

            u -= mRdp->texture_tile[uv_tile].uls / 4.0f;
            v -= mRdp->texture_tile[uv_tile].ult / 4.0f;

            if ((mRdp->other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
                // Linear filter adds 0.5f to the coordinates
                if (!is_rect) {
                    u += 0.5f;
                    v += 0.5f;
                }
            }

            mBufVbo[mBufVboLen++] = u / tex_width[t];
            mBufVbo[mBufVboLen++] = v / tex_height[t];

            bool clampS = tm & (1 << 2 * t);
            bool clampT = tm & (1 << 2 * t + 1);

            if (clampS) {
                mBufVbo[mBufVboLen++] = (tex_width2[t] - 0.5f) / tex_width[t];
            }

            if (clampT) {
                mBufVbo[mBufVboLen++] = (tex_height2[t] - 0.5f) / tex_height[t];
            }
        }

        if (use_fog) {
            if (use_blend_color) {
                // Shroud/blend mode: blend toward blend_color using fog alpha as factor
                mBufVbo[mBufVboLen++] = mRdp->blend_color.r / 255.0f;
                mBufVbo[mBufVboLen++] = mRdp->blend_color.g / 255.0f;
                mBufVbo[mBufVboLen++] = mRdp->blend_color.b / 255.0f;
                mBufVbo[mBufVboLen++] = mRdp->fog_color.a / 255.0f;
            } else {
                mBufVbo[mBufVboLen++] = mRdp->fog_color.r / 255.0f;
                mBufVbo[mBufVboLen++] = mRdp->fog_color.g / 255.0f;
                mBufVbo[mBufVboLen++] = mRdp->fog_color.b / 255.0f;
                mBufVbo[mBufVboLen++] = v_arr[i]->color.a / 255.0f; // fog factor (not alpha)
            }
        }

        if (use_grayscale) {
            mBufVbo[mBufVboLen++] = mRdp->grayscale_color.r / 255.0f;
            mBufVbo[mBufVboLen++] = mRdp->grayscale_color.g / 255.0f;
            mBufVbo[mBufVboLen++] = mRdp->grayscale_color.b / 255.0f;
            mBufVbo[mBufVboLen++] = mRdp->grayscale_color.a / 255.0f; // lerp interpolation factor (not alpha)
        }

        for (int j = 0; j < numInputs; j++) {
            RGBA* color;
            RGBA tmp;
            for (int k = 0; k < 1 + (use_alpha ? 1 : 0); k++) {
                bool is_shade = false;
                switch (comb->shader_input_mapping[k][j]) {
                        // Note: CCMUX constants and ACMUX constants used here have same value, which is why this works
                        // (except LOD fraction).
                    case G_CCMUX_PRIMITIVE:
                        color = &mRdp->prim_color;
                        break;
                    case G_CCMUX_SHADE:
                        color = faceted ? &facet_color[i] : &v_arr[i]->color;
                        is_shade = true;
                        break;
                    case G_CCMUX_ENVIRONMENT:
                        color = &mRdp->env_color;
                        break;
                    case G_CCMUX_PRIMITIVE_ALPHA: {
                        tmp.r = tmp.g = tmp.b = mRdp->prim_color.a;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_ENV_ALPHA: {
                        tmp.r = tmp.g = tmp.b = mRdp->env_color.a;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_PRIM_LOD_FRAC: {
                        tmp.r = tmp.g = tmp.b = mRdp->prim_lod_fraction;
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_LOD_FRACTION: {
                        if (mRdp->other_mode_l & G_TL_LOD) {
                            // "Hack" that works for Bowser - Peach painting
                            float distance_frac = (v1->w - 3000.0f) / 3000.0f;
                            if (distance_frac < 0.0f) {
                                distance_frac = 0.0f;
                            }
                            if (distance_frac > 1.0f) {
                                distance_frac = 1.0f;
                            }
                            tmp.r = tmp.g = tmp.b = tmp.a = distance_frac * 255.0f;
                        } else {
                            tmp.r = tmp.g = tmp.b = tmp.a = 255.0f;
                        }
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_KEY_CENTER:
                        color = &mRdp->key_center;
                        break;
                    case G_CCMUX_KEY_SCALE:
                        color = &mRdp->key_scale;
                        break;
                    case G_CCMUX_CONVERT_K4: {
                        tmp.r = tmp.g = tmp.b = mRdp->convert_k[4];
                        color = &tmp;
                        break;
                    }
                    case G_CCMUX_CONVERT_K5: {
                        tmp.r = tmp.g = tmp.b = mRdp->convert_k[5];
                        color = &tmp;
                        break;
                    }
                    case G_ACMUX_PRIM_LOD_FRAC:
                        tmp.a = mRdp->prim_lod_fraction;
                        color = &tmp;
                        break;
                    default:
                        memset(&tmp, 0, sizeof(tmp));
                        color = &tmp;
                        break;
                }
                if (k == 0) {
                    mBufVbo[mBufVboLen++] = color->r / 255.0f;
                    mBufVbo[mBufVboLen++] = color->g / 255.0f;
                    mBufVbo[mBufVboLen++] = color->b / 255.0f;
                } else {
                    if (use_fog && !use_blend_color && is_shade) {
                        // Shade alpha is 100% for standard fog, blend color mode preserves
                        // it since fog alpha is the blend factor. (MergeShip2: G_EX_KEEP_VERTEX_ALPHA keeps the
                        // vertex's own alpha.)
                        mBufVbo[mBufVboLen++] = (mRsp->extra_geometry_mode & G_EX_KEEP_VERTEX_ALPHA)
                                                    ? v_arr[i]->alpha / 255.0f
                                                    : 1.0f;
                    } else {
                        mBufVbo[mBufVboLen++] = color->a / 255.0f;
                    }
                }
            }
        }

        // struct RGBA *color = &v_arr[i]->color;
        // mBufVbo[mBufVboLen++] = color->r / 255.0f;
        // mBufVbo[mBufVboLen++] = color->g / 255.0f;
        // mBufVbo[mBufVboLen++] = color->b / 255.0f;
        // mBufVbo[mBufVboLen++] = color->a / 255.0f;
    }

    if (++mBufVboNumTris == MAX_TRI_BUFFER) {
        // if (++mBufVbo_num_tris == 1) {
        Flush();
    }
}

void Interpreter::GfxSpGeometryMode(uint32_t clear, uint32_t set) {
    mRsp->geometry_mode &= ~clear;
    mRsp->geometry_mode |= set;
}

void Interpreter::GfxSpExtraGeometryMode(uint32_t clear, uint32_t set) {
    mRsp->extra_geometry_mode &= ~clear;
    mRsp->extra_geometry_mode |= set;
}

void Interpreter::AdjustVIewportOrScissor(XYWidthHeight* area) {
    if (!mFbActive) {
        // Adjust the y origin based on the y-inversion for the active framebuffer
        GfxClipParameters clipParameters = mRapi->GetClipParameters();
        if (clipParameters.invertY) {
            area->y -= area->height;
        } else {
            area->y = mNativeDimensions.height - area->y;
        }

        area->width *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
        area->height *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
        area->x *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
        area->y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);

        if (!mRendersToFb || (mMsaaLevel > 1 && mCurDimensions.width == mGameWindowViewport.width &&
                              mCurDimensions.height == mGameWindowViewport.height)) {
            area->x += mGameWindowViewport.x;
            area->y += mGfxCurrentWindowDimensions.height - (mGameWindowViewport.y + mGameWindowViewport.height);
        }
    } else {
        area->y = mActiveFrameBuffer->second.orig_height - area->y;

        if (mActiveFrameBuffer->second.resize) {
            area->width *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
            area->height *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
            area->x *= RATIO_X(mActiveFrameBuffer, mCurDimensions);
            area->y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
        }
    }
}

void Interpreter::CalcAndSetViewport(const F3DVp_t* viewport) {
    // 2 bits fraction
    float width = 2.0f * viewport->vscale[0] / 4.0f;
    float height = 2.0f * viewport->vscale[1] / 4.0f;
    float x = (viewport->vtrans[0] / 4.0f) - width / 2.0f;
    float y = ((viewport->vtrans[1] / 4.0f) + height / 2.0f);

    mRdp->viewport.x = x;
    mRdp->viewport.y = y;
    mRdp->viewport.width = width;
    mRdp->viewport.height = height;

    AdjustVIewportOrScissor(&mRdp->viewport);

    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::GfxSpMovememF3dex2(uint8_t index, uint8_t offset, const void* data) {
    switch (index) {
        case F3DEX2_G_MV_VIEWPORT:
            CalcAndSetViewport((const F3DVp_t*)data);
            break;
        case F3DEX2_G_MV_LIGHT: {
            int lightidx = offset / 24 - 2;
            if (lightidx >= 0 && lightidx <= MAX_LIGHTS) { // skip lookat
                // NOTE: reads out of bounds if it is an ambient light
                memcpy(mRsp->current_lights + lightidx, data, sizeof(F3DLight));
            } else if (lightidx < 0) {
                memcpy(mRsp->lookat + offset / 24, data, sizeof(F3DLight_t)); // TODO Light?
            }
            break;
        }
    }
}

void Interpreter::GfxSpMovememF3d(uint8_t index, uint8_t offset, const void* data) {
    switch (index) {
        case F3DEX_G_MV_VIEWPORT:
            CalcAndSetViewport((const F3DVp_t*)data);
            break;
        case F3DEX_G_MV_LOOKATY:
        case F3DEX_G_MV_LOOKATX:
            memcpy(mRsp->lookat + (index - F3DEX_G_MV_LOOKATY) / 2, data, sizeof(F3DLight_t));
            break;
        case F3DEX_G_MV_L0:
        case F3DEX_G_MV_L1:
        case F3DEX_G_MV_L2:
        case F3DEX_G_MV_L3:
        case F3DEX_G_MV_L4:
        case F3DEX_G_MV_L5:
        case F3DEX_G_MV_L6:
        case F3DEX_G_MV_L7:
            // NOTE: reads out of bounds if it is an ambient light
            memcpy(mRsp->current_lights + (index - F3DEX_G_MV_L0) / 2, data, sizeof(F3DLight_t));
            break;
    }
}

void Interpreter::GfxSpMovewordF3dex2(uint8_t index, uint16_t offset, uintptr_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
            mRsp->current_num_lights = data / 24 + 1; // add ambient light
            mRsp->lights_changed = true;
            break;
        case G_MW_FOG:
            mRsp->fog_mul = (int16_t)(data >> 16);
            mRsp->fog_offset = (int16_t)data;
            break;
        case G_MW_SEGMENT: {
            int segNumber = offset / 4;
            mSegmentPointers[segNumber] = data;
        } break;
        case G_MW_SEGMENT_INTERP: {
            int segNumber = offset % 16;
            int segIndex = offset / 16;

            if (segIndex == mInterpolationIndex)
                mSegmentPointers[segNumber] = data;
        } break;
    }
}

void Interpreter::GfxSpMovewordF3d(uint8_t index, uint16_t offset, uintptr_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
            // Ambient light is included
            // The 31st bit is a flag that lights should be recalculated
            mRsp->current_num_lights = (data - 0x80000000U) / 32;
            mRsp->lights_changed = true;
            break;
        case G_MW_FOG:
            mRsp->fog_mul = (int16_t)(data >> 16);
            mRsp->fog_offset = (int16_t)data;
            break;
        case G_MW_SEGMENT: {
            int segNumber = offset / 4;
            mSegmentPointers[segNumber] = data;
        } break;
        case G_MW_SEGMENT_INTERP: {
            int segNumber = offset % 16;
            int segIndex = offset / 16;

            if (segIndex == mInterpolationIndex)
                mSegmentPointers[segNumber] = data;
        } break;
    }
}

void Interpreter::GfxSpTexture(uint16_t sc, uint16_t tc, uint8_t level, uint8_t tile, uint8_t on) {
    mRsp->texture_scaling_factor.s = sc;
    mRsp->texture_scaling_factor.t = tc;
    if (mRdp->first_tile_index != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }

    mRdp->first_tile_index = tile;
}

void Interpreter::GfxDpSetScissor(uint32_t mode, uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry) {
    float x = ulx / 4.0f;
    float y = lry / 4.0f;
    float width = (lrx - ulx) / 4.0f;
    float height = (lry - uly) / 4.0f;

    mRdp->scissor.x = x;
    mRdp->scissor.y = y;
    mRdp->scissor.width = width;
    mRdp->scissor.height = height;

    AdjustVIewportOrScissor(&mRdp->scissor);

    mRdp->viewport_or_scissor_changed = true;
}

void Interpreter::GfxDpSetTextureImage(uint32_t format, uint32_t size, uint32_t width, const char* texPath,
                                       uint32_t texFlags, RawTexMetadata rawTexMetdata, const void* addr) {
    // fprintf(stderr, "GfxDpSetTextureImage: %s (width=%d; size=0x%X)\n",
    //         rawTexMetdata.resource ? rawTexMetdata.resource->GetInitData()->Path.c_str() : nullptr, width, size);
    mRdp->texture_to_load.addr = (const uint8_t*)addr;
    mRdp->texture_to_load.siz = size;
    mRdp->texture_to_load.width = width;
    mRdp->texture_to_load.tex_flags = texFlags;
    mRdp->texture_to_load.raw_tex_metadata = rawTexMetdata;
}

void Interpreter::GfxDpSetTile(uint8_t fmt, uint32_t siz, uint32_t line, uint32_t tmem, uint8_t tile, uint32_t palette,
                               uint32_t cmt, uint32_t maskt, uint32_t shiftt, uint32_t cms, uint32_t masks,
                               uint32_t shifts) {
    // OTRTODO:
    // SUPPORT_CHECK(tmem == 0 || tmem == 256);

    if (cms == G_TX_WRAP && masks == G_TX_NOMASK) {
        cms = G_TX_CLAMP;
    }
    if (cmt == G_TX_WRAP && maskt == G_TX_NOMASK) {
        cmt = G_TX_CLAMP;
    }

    mRdp->texture_tile[tile].palette = palette; // palette should set upper 4 bits of color index in 4b mode
    mRdp->texture_tile[tile].fmt = fmt;
    mRdp->texture_tile[tile].siz = siz;
    mRdp->texture_tile[tile].cms = cms;
    mRdp->texture_tile[tile].cmt = cmt;
    mRdp->texture_tile[tile].masks = masks;
    mRdp->texture_tile[tile].maskt = maskt;
    mRdp->texture_tile[tile].shifts = shifts;
    mRdp->texture_tile[tile].shiftt = shiftt;
    mRdp->texture_tile[tile].line_size_bytes = line * 8;

    mRdp->texture_tile[tile].tmem = tmem;
    // mRdp->texture_tile[tile].tmem_index = tmem / 256; // tmem is the 64-bit word offset, so 256 words means 2 kB

    mRdp->texture_tile[tile].tmem_index =
        tmem != 0; // assume one texture is loaded at address 0 and another texture at any other address

    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

void Interpreter::GfxDpSetTileSize(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
    mRdp->texture_tile[tile].uls = uls;
    mRdp->texture_tile[tile].ult = ult;
    mRdp->texture_tile[tile].lrs = lrs;
    mRdp->texture_tile[tile].lrt = lrt;
    mRdp->textures_changed[0] = true;
    mRdp->textures_changed[1] = true;
}

void Interpreter::GfxDpLoadTlut(uint8_t tile, uint32_t high_index) {
    SUPPORT_CHECK(mRdp->texture_to_load.siz == G_IM_SIZ_16b);

    uint16_t tmem = mRdp->texture_tile[tile].tmem;
    const uint8_t* src = mRdp->texture_to_load.addr;
    uint32_t entryCount = high_index + 1;
    uint32_t byteCount = entryCount * 2;

    // MergeShip2: a palette resource is exactly as long as its colours, however many entries the display list loads
    // (child Malon's eyes load 256 from a shorter one; the N64 read on into whatever followed). Reading past the
    // resource can run off the end of mapped memory, so only what the resource holds is copied and the rest is zero.
    uint32_t available = UINT32_MAX;
    const auto& meta = mRdp->texture_to_load.raw_tex_metadata;
    if (meta.resource != nullptr && meta.resource->ImageData != nullptr) {
        const uint8_t* begin = meta.resource->ImageData;
        const uint8_t* end = begin + meta.resource->ImageDataSize;
        if (src >= begin && src <= end) {
            available = (uint32_t)(end - src);
        }
    }
    auto copyTlut = [&](uint8_t* dst, uint32_t srcOffset, uint32_t length) {
        uint32_t readable = srcOffset >= available ? 0 : std::min(length, available - srcOffset);
        if (readable > 0) {
            memcpy(dst, src + srcOffset, readable);
        }
        if (readable < length) {
            memset(dst + readable, 0, length - readable);
        }
    };

    if (tmem >= 256) {
        // N64 TMEM palette area starts at tmem word 256. Each CI4 palette = 16 entries = 16 tmem words.
        uint32_t paletteByteOffset = (tmem - 256) * 2;

        if (high_index == 255 && paletteByteOffset == 0) {
            // CI8: full 256-entry palette spanning both halves
            copyTlut(mRdp->palette_staging[0], 0, 256);
            copyTlut(mRdp->palette_staging[1], 256, 256);
            mRdp->palettes[0] = mRdp->palette_staging[0];
            mRdp->palettes[1] = mRdp->palette_staging[1];
            mRdp->palette_dram_addr[0] = src;
            mRdp->palette_dram_addr[1] = src + 256;
        } else if (paletteByteOffset < 256) {
            // Palettes 0-7 range
            uint32_t copyLen = (paletteByteOffset + byteCount <= 256) ? byteCount : (256 - paletteByteOffset);
            copyTlut(mRdp->palette_staging[0] + paletteByteOffset, 0, copyLen);
            mRdp->palettes[0] = mRdp->palette_staging[0];
            mRdp->palette_dram_addr[0] = src;
        } else {
            // Palettes 8-15 range
            uint32_t offset = paletteByteOffset - 256;
            uint32_t copyLen = (offset + byteCount <= 256) ? byteCount : (256 - offset);
            copyTlut(mRdp->palette_staging[1] + offset, 0, copyLen);
            mRdp->palettes[1] = mRdp->palette_staging[1];
            mRdp->palette_dram_addr[1] = src;
        }
    } else {
        // tmem < 256: non-standard location, fall back to direct pointer
        mRdp->palettes[1] = src;
        mRdp->palette_dram_addr[1] = src;
    }
}

void Interpreter::GfxDpLoadBlock(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t dxt) {
    SUPPORT_CHECK(uls == 0);
    SUPPORT_CHECK(ult == 0);

    // The lrs field rather seems to be number of pixels to load
    uint32_t word_size_shift = 0;
    switch (mRdp->texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = -1;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }
    uint32_t orig_size_bytes =
        word_size_shift > 0 ? (lrs + 1) << word_size_shift : (lrs + 1) >> (-(int64_t)word_size_shift);
    uint32_t size_bytes = orig_size_bytes;
    if (mRdp->texture_to_load.raw_tex_metadata.h_byte_scale != 1 ||
        mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale != 1) {
        size_bytes *= mRdp->texture_to_load.raw_tex_metadata.h_byte_scale;
        size_bytes *= mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale;
    }
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes = orig_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes = size_bytes;
    // Compute actual per-line DRAM stride from SetTextureImage width when available.
    // The standard gDPLoadTextureBlock macro sets width=1, but manually-built DL
    // commands may set the real pixel width.
    uint32_t actual_line_bytes = size_bytes;
    const RawTexMetadata& blkMeta = mRdp->texture_to_load.raw_tex_metadata;
    bool blkHd = blkMeta.h_byte_scale != 1 || blkMeta.v_pixel_scale != 1;
    if (mRdp->texture_to_load.width > 1 && blkHd && blkMeta.height > 0 && size_bytes % blkMeta.height == 0) {
        // HD-upscaled textures report a sentinel SetTextureImage width, so the per-line stride
        // derived from it is bogus. The resource's stored height is authoritative.
        actual_line_bytes = size_bytes / blkMeta.height;
    } else if (mRdp->texture_to_load.width > 1) {
        uint32_t candidate;
        switch (mRdp->texture_to_load.siz) {
            case G_IM_SIZ_4b:
                candidate = (mRdp->texture_to_load.width + 1) / 2;
                break;
            case G_IM_SIZ_8b:
                candidate = mRdp->texture_to_load.width;
                break;
            case G_IM_SIZ_16b:
                candidate = mRdp->texture_to_load.width * 2;
                break;
            case G_IM_SIZ_32b:
                candidate = mRdp->texture_to_load.width * 4;
                break;
            default:
                candidate = mRdp->texture_to_load.width;
                break;
        }
        if (candidate > 0 && candidate < size_bytes && size_bytes % candidate == 0) {
            actual_line_bytes = candidate;
        }
    }
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes = actual_line_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes = actual_line_bytes;
    // assert(size_bytes <= 4096 && "bug: too big texture");
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags = mRdp->texture_to_load.tex_flags;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata = mRdp->texture_to_load.raw_tex_metadata;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr = mRdp->texture_to_load.addr;
    // fprintf(stderr, "GfxDpLoadBlock: line_size = 0x%x; orig = 0x%x; bpp=%d; lrs=%d\n", size_bytes,
    // orig_size_bytes,
    //         mRdp->texture_to_load.siz, lrs);

    const std::string_view texPath =
        mRdp->texture_to_load.raw_tex_metadata.resource != nullptr
            ? GetBaseTexturePath(mRdp->texture_to_load.raw_tex_metadata.resource->GetInitData()->Path)
            : std::string_view{};
    auto maskedTextureIter = mMaskedTextures.find(texPath);
    if (maskedTextureIter != mMaskedTextures.end()) {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = true;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended =
            maskedTextureIter->second.replacementData != nullptr;
    } else {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = false;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended = false;
    }

    mRdp->textures_changed[mRdp->texture_tile[tile].tmem_index] = true;
}

void Interpreter::GfxDpLoadTile(uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
    SUPPORT_CHECK(tile == G_TX_LOADTILE);

    uint32_t word_size_shift = 0;
    switch (mRdp->texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }

    uint32_t offset_x = uls >> G_TEXTURE_IMAGE_FRAC;
    uint32_t offset_y = ult >> G_TEXTURE_IMAGE_FRAC;
    uint32_t tile_width = ((lrs - uls) >> G_TEXTURE_IMAGE_FRAC) + 1;
    uint32_t tile_height = ((lrt - ult) >> G_TEXTURE_IMAGE_FRAC) + 1;
    uint32_t full_image_width = mRdp->texture_to_load.width;

    uint32_t offset_x_in_bytes = offset_x << word_size_shift;
    uint32_t tile_line_size_bytes = tile_width << word_size_shift;
    uint32_t full_image_line_size_bytes = full_image_width << word_size_shift;

    uint32_t orig_size_bytes = tile_line_size_bytes * tile_height;
    uint32_t size_bytes = orig_size_bytes;
    uint32_t start_offset_bytes = full_image_line_size_bytes * offset_y + offset_x_in_bytes;

    float h_byte_scale = mRdp->texture_to_load.raw_tex_metadata.h_byte_scale;
    float v_pixel_scale = mRdp->texture_to_load.raw_tex_metadata.v_pixel_scale;

    if (h_byte_scale != 1 || v_pixel_scale != 1) {
        start_offset_bytes = h_byte_scale * (v_pixel_scale * offset_y * full_image_line_size_bytes + offset_x_in_bytes);
        size_bytes *= h_byte_scale * v_pixel_scale;
        full_image_line_size_bytes *= h_byte_scale;
        tile_line_size_bytes *= h_byte_scale;
    }

    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].orig_size_bytes = orig_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].size_bytes = size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].full_image_line_size_bytes = full_image_line_size_bytes;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].line_size_bytes = tile_line_size_bytes;

    //    assert(size_bytes <= 4096 && "bug: too big texture");
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].tex_flags = mRdp->texture_to_load.tex_flags;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].raw_tex_metadata = mRdp->texture_to_load.raw_tex_metadata;
    mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].addr = mRdp->texture_to_load.addr + start_offset_bytes;

    const std::string_view texPath =
        mRdp->texture_to_load.raw_tex_metadata.resource != nullptr
            ? GetBaseTexturePath(mRdp->texture_to_load.raw_tex_metadata.resource->GetInitData()->Path)
            : std::string_view{};
    auto maskedTextureIter = mMaskedTextures.find(texPath);
    if (maskedTextureIter != mMaskedTextures.end()) {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = true;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended =
            maskedTextureIter->second.replacementData != nullptr;
    } else {
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].masked = false;
        mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index].blended = false;
    }

    mRdp->texture_tile[tile].uls = uls;
    mRdp->texture_tile[tile].ult = ult;
    mRdp->texture_tile[tile].lrs = lrs;
    mRdp->texture_tile[tile].lrt = lrt;

    mRdp->textures_changed[mRdp->texture_tile[tile].tmem_index] = true;
}

/*static uint8_t color_comb_component(uint32_t v) {
    switch (v) {
        case G_CCMUX_TEXEL0:
            return CC_TEXEL0;
        case G_CCMUX_TEXEL1:
            return CC_TEXEL1;
        case G_CCMUX_PRIMITIVE:
            return CC_PRIM;
        case G_CCMUX_SHADE:
            return CC_SHADE;
        case G_CCMUX_ENVIRONMENT:
            return CC_ENV;
        case G_CCMUX_TEXEL0_ALPHA:
            return CC_TEXEL0A;
        case G_CCMUX_LOD_FRACTION:
            return CC_LOD;
        default:
            return CC_0;
    }
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return color_comb_component(a) |
           (color_comb_component(b) << 3) |
           (color_comb_component(c) << 6) |
           (color_comb_component(d) << 9);
}

static void GfxDpSetCombineMode(uint32_t rgb, uint32_t alpha) {
    mRdp->combine_mode = rgb | (alpha << 12);
}*/

void Interpreter::GfxDpSetCombineMode(uint32_t rgb, uint32_t alpha, uint32_t rgb_cyc2, uint32_t alpha_cyc2) {
    mRdp->combine_mode = rgb | (alpha << 16) | ((uint64_t)rgb_cyc2 << 28) | ((uint64_t)alpha_cyc2 << 44);
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a & 0xf) | ((b & 0xf) << 4) | ((c & 0x1f) << 8) | ((d & 7) << 13);
}

static inline uint32_t alpha_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return (a & 7) | ((b & 7) << 3) | ((c & 7) << 6) | ((d & 7) << 9);
}

// Sign-extend a 9-bit value (used for G_SETCONVERT K0..K5).
static inline int16_t sign_extend_9(uint32_t v) {
    return (int16_t)((v & 0x100) ? (int32_t)(v | 0xFFFFFE00u) : (int32_t)v);
}

void Interpreter::GfxDpSetGrayscaleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->grayscale_color.r = r;
    mRdp->grayscale_color.g = g;
    mRdp->grayscale_color.b = b;
    mRdp->grayscale_color.a = a;
}

void Interpreter::GfxDpSetEnvColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->env_color.r = r;
    mRdp->env_color.g = g;
    mRdp->env_color.b = b;
    mRdp->env_color.a = a;
}

void Interpreter::GfxDpSetPrimColor(uint8_t m, uint8_t l, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->prim_lod_fraction = l;
    mRdp->prim_color.r = r;
    mRdp->prim_color.g = g;
    mRdp->prim_color.b = b;
    mRdp->prim_color.a = a;
}

void Interpreter::GfxDpSetFogColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->fog_color.r = r;
    mRdp->fog_color.g = g;
    mRdp->fog_color.b = b;
    mRdp->fog_color.a = a;
}

void Interpreter::GfxDpSetBlendColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    mRdp->blend_color.r = r;
    mRdp->blend_color.g = g;
    mRdp->blend_color.b = b;
    mRdp->blend_color.a = a;
}

void Interpreter::GfxDpSetFillColor(uint32_t packed_color) {
    uint16_t col16 = (uint16_t)packed_color;
    uint32_t r = col16 >> 11;
    uint32_t g = (col16 >> 6) & 0x1f;
    uint32_t b = (col16 >> 1) & 0x1f;
    uint32_t a = col16 & 1;
    mRdp->fill_color.r = SCALE_5_8(r);
    mRdp->fill_color.g = SCALE_5_8(g);
    mRdp->fill_color.b = SCALE_5_8(b);
    mRdp->fill_color.a = a * 255;
}

void Interpreter::GfxDrawRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    uint32_t saved_other_mode_h = mRdp->other_mode_h;
    uint32_t cycle_type = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    if (cycle_type == G_CYC_COPY) {
        mRdp->other_mode_h = (mRdp->other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT;
    }

    // U10.2 coordinates
    float ulxf = ulx;
    float ulyf = uly;
    float lrxf = lrx;
    float lryf = lry;

    ulxf = ulxf / (4.0f * HALF_SCREEN_WIDTH(mActiveFrameBuffer)) - 1.0f;
    ulyf = -(ulyf / (4.0f * HALF_SCREEN_HEIGHT(mActiveFrameBuffer))) + 1.0f;
    lrxf = lrxf / (4.0f * HALF_SCREEN_WIDTH(mActiveFrameBuffer)) - 1.0f;
    lryf = -(lryf / (4.0f * HALF_SCREEN_HEIGHT(mActiveFrameBuffer))) + 1.0f;

    ulxf = AdjXForAspectRatio(ulxf);
    lrxf = AdjXForAspectRatio(lrxf);

    struct LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    struct LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    struct LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    struct LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];

    ul->x = ulxf;
    ul->y = ulyf;
    ul->z = -1.0f;
    ul->w = 1.0f;

    ll->x = ulxf;
    ll->y = lryf;
    ll->z = -1.0f;
    ll->w = 1.0f;

    lr->x = lrxf;
    lr->y = lryf;
    lr->z = -1.0f;
    lr->w = 1.0f;

    ur->x = lrxf;
    ur->y = ulyf;
    ur->z = -1.0f;
    ur->w = 1.0f;

    // The coordinates for texture rectangle shall bypass the viewport setting
    struct XYWidthHeight default_viewport;
    if (!mFbActive) {
        default_viewport = { 0, (int16_t)mNativeDimensions.height, mNativeDimensions.width, mNativeDimensions.height };
    } else {
        default_viewport = { 0, (int16_t)mActiveFrameBuffer->second.orig_height, mActiveFrameBuffer->second.orig_width,
                             mActiveFrameBuffer->second.orig_height };
    }

    struct XYWidthHeight viewport_saved = mRdp->viewport;
    uint32_t geometry_mode_saved = mRsp->geometry_mode;

    AdjustVIewportOrScissor(&default_viewport);

    mRdp->viewport = default_viewport;
    mRdp->viewport_or_scissor_changed = true;
    mRsp->geometry_mode = 0;

    GfxSpTri1(MAX_VERTICES + 0, MAX_VERTICES + 1, MAX_VERTICES + 3, true);
    GfxSpTri1(MAX_VERTICES + 1, MAX_VERTICES + 2, MAX_VERTICES + 3, true);

    mRsp->geometry_mode = geometry_mode_saved;
    mRdp->viewport = viewport_saved;
    mRdp->viewport_or_scissor_changed = true;

    if (cycle_type == G_CYC_COPY) {
        mRdp->other_mode_h = saved_other_mode_h;
    }
}

void Interpreter::GfxDpTextureRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, uint8_t tile, int16_t uls,
                                        int16_t ult, int16_t dsdx, int16_t dtdy, bool flip) {
    // printf("render %d at %d\n", tile, lrx);
    uint64_t saved_combine_mode = mRdp->combine_mode;
    if ((mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_COPY) {
        // Per RDP Command Summary Set Tile's shift s and this dsdx should be set to 4 texels
        // Divide by 4 to get 1 instead
        dsdx >>= 2;

        // Color combiner is turned off in copy mode
        GfxDpSetCombineMode(color_comb(0, 0, 0, G_CCMUX_TEXEL0), alpha_comb(0, 0, 0, G_ACMUX_TEXEL0), 0, 0);

        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    // uls and ult are S10.5
    // dsdx and dtdy are S5.10
    // lrx, lry, ulx, uly are U10.2
    // lrs, lrt are S10.5
    if (flip) {
        dsdx = -dsdx;
        dtdy = -dtdy;
    }
    int16_t width = !flip ? lrx - ulx : lry - uly;
    int16_t height = !flip ? lry - uly : lrx - ulx;
    float lrs = ((uls << 7) + dsdx * width) >> 7;
    float lrt = ((ult << 7) + dtdy * height) >> 7;

    LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];
    ul->u = uls;
    ul->v = ult;
    lr->u = lrs;
    lr->v = lrt;
    if (!flip) {
        ll->u = uls;
        ll->v = lrt;
        ur->u = lrs;
        ur->v = ult;
    } else {
        ll->u = lrs;
        ll->v = ult;
        ur->u = uls;
        ur->v = lrt;
    }

    uint8_t saved_tile = mRdp->first_tile_index;
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = tile;

    GfxDrawRectangle(ulx, uly, lrx, lry);
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = saved_tile;
    mRdp->combine_mode = saved_combine_mode;
}

void Interpreter::GfxDpImageRectangle(int32_t tile, int32_t w, int32_t h, int32_t ulx, int32_t uly, int16_t uls,
                                      int16_t ult, int32_t lrx, int32_t lry, int16_t lrs, int16_t lrt) {

    LoadedVertex* ul = &mRsp->loaded_vertices[MAX_VERTICES + 0];
    LoadedVertex* ll = &mRsp->loaded_vertices[MAX_VERTICES + 1];
    LoadedVertex* lr = &mRsp->loaded_vertices[MAX_VERTICES + 2];
    LoadedVertex* ur = &mRsp->loaded_vertices[MAX_VERTICES + 3];
    ul->u = uls * 32;
    ul->v = ult * 32;
    lr->u = lrs * 32;
    lr->v = lrt * 32;
    ll->u = uls * 32;
    ll->v = lrt * 32;
    ur->u = lrs * 32;
    ur->v = ult * 32;

    // ensure we have the correct texture size, format and starting position
    mRdp->texture_tile[tile].siz = G_IM_SIZ_8b;
    mRdp->texture_tile[tile].fmt = G_IM_FMT_RGBA;
    mRdp->texture_tile[tile].cms = 0;
    mRdp->texture_tile[tile].cmt = 0;
    mRdp->texture_tile[tile].shifts = 0;
    mRdp->texture_tile[tile].shiftt = 0;
    mRdp->texture_tile[tile].uls = 0 * 4;
    mRdp->texture_tile[tile].ult = 0 * 4;
    mRdp->texture_tile[tile].lrs = w * 4;
    mRdp->texture_tile[tile].lrt = h * 4;
    mRdp->texture_tile[tile].line_size_bytes = w << (mRdp->texture_tile[tile].siz >> 1);

    auto& loadtex = mRdp->loaded_texture[mRdp->texture_tile[tile].tmem_index];
    loadtex.full_image_line_size_bytes = loadtex.line_size_bytes = mRdp->texture_tile[tile].line_size_bytes;
    loadtex.size_bytes = loadtex.orig_size_bytes = loadtex.line_size_bytes * h;

    uint8_t saved_tile = mRdp->first_tile_index;
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = tile;

    GfxDrawRectangle(ulx, uly, lrx, lry);
    if (saved_tile != tile) {
        mRdp->textures_changed[0] = true;
        mRdp->textures_changed[1] = true;
    }
    mRdp->first_tile_index = saved_tile;
}

void Interpreter::GfxDpFillRectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    if (mRdp->color_image_address == mRdp->z_buf_address) {
        // Fullscreen Z clears are redundant — already done by glClear at frame start.
        bool isFullScreen = (ulx <= 0 && uly <= 0 && lrx >= (int32_t)(mNativeDimensions.width - 1) * 4 &&
                             lry >= (int32_t)(mNativeDimensions.height - 1) * 4);
        if (isFullScreen) {
            return;
        }

        // Partial depth clear (e.g. HUD model regions): clear the actual depth buffer
        // via a scissored depth clear instead of drawing a colored rect to the color buffer.
        Flush();

        // Convert U10.2 coords to pixel coords and add +1 pixel for fill mode
        int32_t expanded_lrx = lrx + (1 << 2);
        int32_t expanded_lry = lry + (1 << 2);
        float x = ulx / 4.0f;
        float y = expanded_lry / 4.0f;
        float w = (expanded_lrx - ulx) / 4.0f;
        float h = (expanded_lry - uly) / 4.0f;
        float halfNativeWidth = (float)HALF_SCREEN_WIDTH(mActiveFrameBuffer);
        x = halfNativeWidth + AdjXForAspectRatio(x - halfNativeWidth);
        w = AdjXForAspectRatio(w);

        struct XYWidthHeight area;
        area.x = (int16_t)x;
        area.y = (int16_t)y;
        area.width = (uint32_t)w;
        area.height = (uint32_t)h;
        AdjustVIewportOrScissor(&area);

        mRapi->ClearDepthRegion(area.x, area.y, area.width, area.height);
        return;
    }
    uint32_t mode = (mRdp->other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    // Expand fullscreen fill rects to cover widescreen viewports.
    // Without this, screen clears and fades only cover the native 4:3 area.
    if (ulx == 0 && uly == 0) {
        bool isFullScreen = (lrx == ((int32_t)(mNativeDimensions.width - 1) * 4) &&
                             lry == ((int32_t)(mNativeDimensions.height - 1) * 4));
        if (isFullScreen) {
            ulx = -1024;
            uly = -1024;
            lrx = 2048;
            lry = 2048;
        }
    }

    if (mode == G_CYC_COPY || mode == G_CYC_FILL) {
        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    for (int i = MAX_VERTICES; i < MAX_VERTICES + 4; i++) {
        LoadedVertex* v = &mRsp->loaded_vertices[i];
        v->color = mRdp->fill_color;
    }

    uint64_t saved_combine_mode = mRdp->combine_mode;

    if (mode == G_CYC_FILL) {
        GfxDpSetCombineMode(color_comb(0, 0, 0, G_CCMUX_SHADE), alpha_comb(0, 0, 0, G_ACMUX_SHADE), 0, 0);
    }

    GfxDrawRectangle(ulx, uly, lrx, lry);
    mRdp->combine_mode = saved_combine_mode;
}

void Interpreter::GfxDpSetZImage(void* zBufAddr) {
    mRdp->z_buf_address = zBufAddr;
}

void Interpreter::GfxDpSetColorImage(uint32_t format, uint32_t size, uint32_t width, void* address) {
    mRdp->color_image_address = address;
}

void Interpreter::GfxSpSetOtherMode(uint32_t shift, uint32_t num_bits, uint64_t mode) {
    uint64_t mask = (((uint64_t)1 << num_bits) - 1) << shift;
    uint64_t om = mRdp->other_mode_l | ((uint64_t)mRdp->other_mode_h << 32);
    om = (om & ~mask) | mode;
    mRdp->other_mode_l = (uint32_t)om;
    mRdp->other_mode_h = (uint32_t)(om >> 32);
}

void Interpreter::GfxDpSetOtherMode(uint32_t h, uint32_t l) {
    mRdp->other_mode_h = h;
    mRdp->other_mode_l = l;
}

void Interpreter::Gfxs2dexBgCopy(F3DuObjBg* bg) {
    /*
    bg->b.imageX = 0;
    bg->b.imageW = width * 4;
    bg->b.frameX = frameX * 4;
    bg->b.imageY = 0;
    bg->b.imageH = height * 4;
    bg->b.frameY = frameY * 4;
    bg->b.imagePtr = source;
    bg->b.imageLoad = G_BGLT_LOADTILE;
    bg->b.imageFmt = fmt;
    bg->b.imageSiz = siz;
    bg->b.imagePal = 0;
    bg->b.imageFlip = 0;
    */

    uintptr_t data = (uintptr_t)bg->b.imagePtr;

    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    if ((bool)gfx_check_image_signature((char*)data)) {
        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        texFlags = tex->Flags;
        rawTexMetadata.width = tex->Width;
        rawTexMetadata.height = tex->Height;
        rawTexMetadata.h_byte_scale = tex->HByteScale;
        rawTexMetadata.v_pixel_scale = tex->VPixelScale;
        rawTexMetadata.type = tex->Type;
        rawTexMetadata.resource = tex;
        data = (uintptr_t) reinterpret_cast<char*>(tex->ImageData);
    }

    s16 dsdx = 4 << 10;
    s16 uls = bg->b.imageX << 3;
    // Flip flag only flips horizontally
    if (bg->b.imageFlip == G_BG_FLAG_FLIPS) {
        dsdx = -dsdx;
        uls = (bg->b.imageW - bg->b.imageX) << 3;
    }

    SUPPORT_CHECK(bg->b.imageSiz == G_IM_SIZ_16b);
    GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, nullptr, texFlags, rawTexMetadata, (void*)data);
    GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, (bg->b.imageW * bg->b.imageH >> 4) - 1, 0);
    GfxDpSetTile(bg->b.imageFmt, G_IM_SIZ_16b, bg->b.imageW >> 4, 0, G_TX_RENDERTILE, bg->b.imagePal, 0, 0, 0, 0, 0, 0);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, bg->b.imageW, bg->b.imageH);
    GfxDpTextureRectangle(bg->b.frameX, bg->b.frameY, bg->b.frameX + bg->b.imageW - 4, bg->b.frameY + bg->b.imageH - 4,
                          G_TX_RENDERTILE, uls, bg->b.imageY << 3, dsdx, 1 << 10, false);
}

void Interpreter::Gfxs2dexBg1cyc(F3DuObjBg* bg) {
    uintptr_t data = (uintptr_t)bg->b.imagePtr;

    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    if ((bool)gfx_check_image_signature((char*)data)) {
        std::shared_ptr<Fast::Texture> tex = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess((char*)data));
        texFlags = tex->Flags;
        rawTexMetadata.width = tex->Width;
        rawTexMetadata.height = tex->Height;
        rawTexMetadata.h_byte_scale = tex->HByteScale;
        rawTexMetadata.v_pixel_scale = tex->VPixelScale;
        rawTexMetadata.type = tex->Type;
        rawTexMetadata.resource = tex;
        data = (uintptr_t) reinterpret_cast<char*>(tex->ImageData);
    }

    // TODO: Implement bg scaling correctly
    s16 uls = bg->b.imageX >> 2;
    s16 lrs = bg->b.imageW >> 2;

    s16 dsdxRect = 1 << 10;
    s16 ulsRect = bg->b.imageX << 3;
    // Flip flag only flips horizontally
    if (bg->b.imageFlip == G_BG_FLAG_FLIPS) {
        dsdxRect = -dsdxRect;
        ulsRect = (bg->b.imageW - bg->b.imageX) << 3;
    }

    GfxDpSetTextureImage(bg->b.imageFmt, bg->b.imageSiz, bg->b.imageW >> 2, nullptr, texFlags, rawTexMetadata,
                         (void*)data);
    GfxDpSetTile(bg->b.imageFmt, bg->b.imageSiz, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, (bg->b.imageW * bg->b.imageH >> 4) - 1, 0);
    GfxDpSetTile(bg->b.imageFmt, bg->b.imageSiz, (((lrs - uls) * bg->b.imageSiz) + 7) >> 3, 0, G_TX_RENDERTILE,
                 bg->b.imagePal, 0, 0, 0, 0, 0, 0);
    GfxDpSetTileSize(G_TX_RENDERTILE, 0, 0, bg->b.imageW, bg->b.imageH);

    GfxDpTextureRectangle(bg->b.frameX, bg->b.frameY, bg->b.frameW, bg->b.frameH, G_TX_RENDERTILE, ulsRect,
                          bg->b.imageY << 3, dsdxRect, 1 << 10, false);
}

void Interpreter::Gfxs2dexRecyCopy(F3DuObjSprite* spr) {
    s16 dsdx = 4 << 10;
    [[maybe_unused]] s16 uls = spr->s.objX << 3;
    // Flip flag only flips horizontally
    if (spr->s.imageFlags == G_BG_FLAG_FLIPS) {
        dsdx = -dsdx;
        uls = (spr->s.imageW - spr->s.objX) << 3;
    }

    int realX = spr->s.objX >> 2;
    int realY = spr->s.objY >> 2;
    int realW = (((spr->s.imageW)) >> 5);
    int realH = (((spr->s.imageH)) >> 5);
    float realSW = spr->s.scaleW / 1024.0f;
    float realSH = spr->s.scaleH / 1024.0f;

    int testX = (realX + (realW / realSW));
    int testY = (realY + (realH / realSH));

    GfxDpTextureRectangle(realX << 2, realY << 2, testX << 2, testY << 2, G_TX_RENDERTILE,
                          (s32)mRdp->texture_tile[0].uls << 3, (s32)mRdp->texture_tile[0].ult << 3,
                          (float)(1 << 10) * realSW, (float)(1 << 10) * realSH, false);
}

void* Interpreter::SegAddr(uintptr_t w1) {
    // Segmented?
    if (w1 & 1) {
        uint32_t segNum = (uint32_t)(w1 >> 24);

        uint32_t offset = w1 & 0x00FFFFFE;

        if (mSegmentPointers[segNum] != 0) {
            return (void*)(mSegmentPointers[segNum] + offset);
        } else {
            return (void*)w1;
        }
    } else {
        return (void*)w1;
    }
}

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))

void GfxExecStack::start(F3DGfx* dlist) {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
    cmd_stack.push(dlist);
    disp_stack.clear();
}

void GfxExecStack::stop() {
    while (!cmd_stack.empty())
        cmd_stack.pop();
    gfx_path.clear();
}

F3DGfx*& GfxExecStack::currCmd() {
    return cmd_stack.top();
}

void GfxExecStack::openDisp(const char* file, int line) {
    disp_stack.push_back({ file, line });
}
void GfxExecStack::closeDisp() {
    disp_stack.pop_back();
}
const std::vector<GfxExecStack::CodeDisp>& GfxExecStack::getDisp() const {
    return disp_stack;
}

void GfxExecStack::branch(F3DGfx* caller) {
    F3DGfx* old = cmd_stack.top();
    cmd_stack.pop();
    cmd_stack.push(nullptr);
    cmd_stack.push(old);

    gfx_path.push_back(caller);
}

void GfxExecStack::call(F3DGfx* caller, F3DGfx* callee) {
    cmd_stack.push(callee);
    gfx_path.push_back(caller);
}

F3DGfx* GfxExecStack::ret() {
    F3DGfx* cmd = cmd_stack.top();

    cmd_stack.pop();
    if (!gfx_path.empty()) {
        gfx_path.pop_back();
    }

    while (cmd_stack.size() > 0 && cmd_stack.top() == nullptr) {
        cmd_stack.pop();
        if (!gfx_path.empty()) {
            gfx_path.pop_back();
        }
    }
    return cmd;
}

void gfx_set_framebuffer(int fb, float noise_scale);
void gfx_reset_framebuffer();
void gfx_copy_framebuffer(int fb_dst_id, int fb_src_id, bool copyOnce, bool* hasCopiedPtr);

// The main type of the handler function. These function will take a pointer to a pointer to a Gfx. It needs to be a
// double pointer because we sometimes need to increment and decrement the underlying pointer Returns false if the
// current opcode should be incremented after the handler ends.
typedef bool (*GfxOpcodeHandlerFunc)(F3DGfx** gfx);

bool gfx_load_ucode_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = mInstance.lock().get();
    gfx->mRsp->fog_mul = 0;
    gfx->mRsp->fog_offset = 0;
    return false;
}

bool gfx_cull_dl_handler_f3dex2(F3DGfx** cmd) {
    // TODO:
    return false;
}

bool gfx_marker_handler_otr(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    (*cmd0)++;
    F3DGfx* cmd = (*cmd0);
    gfx->mMarkerOn = true;
    return false;
}

bool gfx_invalidate_tex_cache_handler_f3dex2(F3DGfx** cmd) {
    Interpreter* gfx = mInstance.lock().get();
    const uintptr_t texAddr = (*cmd)->words.w1;

    if (texAddr == 0) {
        gfx->TextureCacheClear();
    } else {
        gfx->TextureCacheDelete((const uint8_t*)texAddr);
    }
    return false;
}

bool gfx_noop_handler_f3dex2(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    const char* filename = (const char*)(cmd)->words.w1;
    uint32_t p = C0(16, 8);
    uint32_t l = C0(0, 16);
    if (p == 7) {
        g_exec_stack.openDisp(filename, l);
    } else if (p == 8) {
        if (g_exec_stack.disp_stack.size() == 0) {
            SPDLOG_WARN("CLOSE_DISPS without matching open {}:{}", p, l);
        } else {
            g_exec_stack.closeDisp();
        }
    }
    return false;
}

bool gfx_mtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    uintptr_t mtxAddr = cmd->words.w1;

    gfx->GfxSpMatrix(C0(0, 8) ^ F3DEX2_G_MTX_PUSH, (const int32_t*)gfx->SegAddr(mtxAddr));
    return false;
}
// Seems to be the same for all other non F3DEX2 microcodes...
bool gfx_mtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    uintptr_t mtxAddr = cmd->words.w1;

    gfx->GfxSpMatrix(C0(16, 8), (const int32_t*)gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_mtx_otr_filepath_handler_custom_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    const char* fileName = (const char*)cmd->words.w1;
    const int32_t* mtx = (const int32_t*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(
        (const char*)fileName);

    if (mtx != NULL) {
        gfx->GfxSpMatrix(C0(0, 8) ^ F3DEX2_G_MTX_PUSH, mtx);
    }

    return false;
}

bool gfx_mtx_otr_filepath_handler_custom_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    const char* fileName = (const char*)cmd->words.w1;
    const int32_t* mtx = (const int32_t*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(
        (const char*)fileName);

    if (mtx != NULL) {
        gfx->GfxSpMatrix(C0(16, 8), mtx);
    }

    return false;
}

bool gfx_mtx_otr_filepath_handler_custom(F3DGfx** cmd0) {
    if (ucode_handler_index == ucode_f3dex2) {
        return gfx_mtx_otr_filepath_handler_custom_f3dex2(cmd0);
    } else {
        return gfx_mtx_otr_filepath_handler_custom_f3d(cmd0);
    }
}

// MergeShip2: a room of the Ocarina of Time 3D pack loads a matrix by name for every cluster of its vertices, well
// over a thousand a frame, and through the resource manager each is a search by name: about two microseconds, which
// came to near half of a frame's whole drawing (measured: "gMergeShip.ShadowTiming"). The pack's matrices never
// change, so they are remembered by their hash, and kept loaded. nullptr: not one of the pack's (as always, then).
static const int32_t* PackMatrix(uint64_t hash) {
    struct Kept {
        std::shared_ptr<Ship::IResource> resource;
        const int32_t* mtx;
    };
    static std::unordered_map<uint64_t, Kept> sKept;
    auto it = sKept.find(hash);
    if (it == sKept.end()) {
        Kept kept{};
        auto manager = Ship::Context::GetRawInstance()->GetResourceManager();
        const std::string* name = manager->GetArchiveManager()->HashToString(hash);
        if (name != nullptr && name->compare(0, 12, "port3ds/mtx/") == 0) {
            kept.resource = manager->LoadResource(hash);
            kept.mtx = kept.resource != nullptr ? (const int32_t*)kept.resource->GetRawPointer() : nullptr;
        }
        it = sKept.emplace(hash, std::move(kept)).first;
    }
    return it->second.mtx;
}

bool gfx_mtx_otr_handler_custom_f3dex2(F3DGfx** cmd0) {
    (*cmd0)++;
    F3DGfx* cmd = *cmd0;

    const uint64_t hash = ((uint64_t)cmd->words.w0 << 32) + cmd->words.w1;
    const int32_t* mtx = PackMatrix(hash);
    if (mtx == nullptr) {
        mtx = (const int32_t*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
    }

    if (mtx != NULL) {
        Interpreter* gfx = mInstance.lock().get();
        cmd--;
        gfx->GfxSpMatrix(C0(0, 8) ^ F3DEX2_G_MTX_PUSH, mtx);
        cmd++;
    }

    return false;
}

bool gfx_mtx_otr_handler_custom_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    (*cmd0)++;
    F3DGfx* cmd = *cmd0;

    const uint64_t hash = ((uint64_t)cmd->words.w0 << 32) + cmd->words.w1;
    const int32_t* mtx =
        (const int32_t*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
    if (mtx != nullptr) {
        cmd--;
        gfx->GfxSpMatrix(C0(16, 8), mtx);
        cmd++;
    }
    return false;
}

bool gfx_mtx_otr_handler_custom(F3DGfx** cmd0) {
    if (ucode_handler_index == ucode_f3dex2) {
        return gfx_mtx_otr_handler_custom_f3dex2(cmd0);
    } else {
        return gfx_mtx_otr_handler_custom_f3d(cmd0);
    }
}

bool gfx_pop_mtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpPopMatrix((uint32_t)(cmd->words.w1 / 64));

    return false;
}

bool gfx_pop_mtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpPopMatrix(1);

    return false;
}

bool gfx_movemem_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovememF3dex2(C0(0, 8), C0(8, 8) * 8, gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_movemem_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovememF3d(C0(16, 8), 0, gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_movemem_handler_otr(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    const uint8_t index = C1(24, 8);
    const uint8_t offset = C1(16, 8);
    const uint8_t hasOffset = C1(8, 8);

    (*cmd0)++;

    const uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

    if (ucode_handler_index == ucode_f3dex2) {
        gfx->GfxSpMovememF3dex2(index, offset,
                                Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash));
    } else {
        auto light =
            (Fast::LightEntry*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);
        uintptr_t data = (uintptr_t)&light->Ambient;
        gfx->GfxSpMovememF3d(index, offset, (void*)(data + (hasOffset == 1 ? 0x8 : 0)));
    }
    return false;
}

bool gfx_push_shader(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    const char* path = (const char*)gfx->SegAddr(cmd->words.w1);

    if (!gfx_check_image_signature(path)) {
        SPDLOG_ERROR("G_PUSH_SHADER: Shader is not a valid OTR resource name, unable to register push shader");
        return false;
    }

    path = &path[7];

    size_t shaderId = static_cast<size_t>(-1);
    for (const auto& shader : gfx->mShaders) {
        if (strcmp(shader.second, path) == 0) {
            shaderId = shader.first;
            break;
        }
    }

    if (shaderId == static_cast<size_t>(-1)) {
        shaderId = gfx->mShadersIndex++;
        gfx->mShaders[shaderId] = path;
    }

    gfx->mShaderStack.push(shaderId);

    return false;
}

bool gfx_pop_shader(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    if (!gfx->mShaderStack.empty()) {
        gfx->mShaderStack.pop();
    }

    return false;
}

const char* gfx_get_shader(int16_t id) {
    Interpreter* gfx = mInstance.lock().get();

    for (const std::pair<size_t, const char*>& shader : gfx->mShaders) {
        if (shader.first == id) {
            return shader.second;
        }
    }

    return nullptr; // Use no shader
}

bool gfx_moveword_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovewordF3dex2(C0(16, 8), C0(0, 16), cmd->words.w1);

    return false;
}

bool gfx_moveword_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpMovewordF3d(C0(0, 8), C0(8, 16), cmd->words.w1);

    return false;
}

bool gfx_texture_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTexture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(1, 7));

    return false;
}

// Seems to be the same for all other non F3DEX2 microcodes...
bool gfx_texture_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTexture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(0, 8));

    return false;
}

// Almost all versions of the microcode have their own version of this opcode
bool gfx_vtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpVertex(C0(12, 8), C0(1, 7) - C0(12, 8), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_vtx_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    gfx->GfxSpVertex(C0(10, 6), C0(17, 7), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_vtx_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpVertex((C0(0, 16)) / sizeof(F3DVtx), C0(16, 4), (const F3DVtx*)gfx->SegAddr(cmd->words.w1));

    return false;
}

bool gfx_vtx_hash_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    // Offset added to the start of the vertices
    const uintptr_t offset = (*cmd0)->words.w1;
    // This is a two-part display list command, so increment the instruction pointer so we can get the CRC64
    // hash from the second
    (*cmd0)++;
    const uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

    // We need to know if the offset is a cached pointer or not. An offset greater than one million is not a
    // real offset, so it must be a real pointer
    if (offset > 0xFFFFF) {
        (*cmd0)--;
        F3DGfx* cmd = *cmd0;
        if (sSway != nullptr && sSway->byHeight < 0.0f) { // (MergeShip2 swaying grass: a room's own plants)
            sSwayTable = SwayTableFor(hash, (const void*)offset, C0(12, 8));
        }
        gfx->GfxSpVertex(C0(12, 8), C0(1, 7) - C0(12, 8), (F3DVtx*)offset);
        sSwayTable = nullptr;
        (*cmd0)++;
    } else {
        F3DVtx* vtx = (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);

        if (vtx != NULL) {
            vtx = (F3DVtx*)((char*)vtx + offset);

            (*cmd0)--;
            F3DGfx* cmd = *cmd0;

            // TODO: WTF??
            cmd->words.w1 = (uintptr_t)vtx;

            if (sSway != nullptr && sSway->byHeight < 0.0f) { // (MergeShip2 swaying grass: a room's own plants)
                sSwayTable = SwayTableFor(hash, vtx, C0(12, 8));
            }
            gfx->GfxSpVertex(C0(12, 8), C0(1, 7) - C0(12, 8), vtx);
            sSwayTable = nullptr;
            (*cmd0)++;
        }
    }
    return false;
}

bool gfx_vtx_otr_filepath_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    char* fileName = (char*)cmd->words.w1;
    (*cmd0)++;
    cmd = *cmd0;
    size_t vtxCnt = cmd->words.w0;
    size_t vtxIdxOff = cmd->words.w1 >> 16;
    size_t vtxDataOff = cmd->words.w1 & 0xFFFF;
    F3DVtx* vtx =
        (F3DVtx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);
    vtx += vtxDataOff;

    gfx->GfxSpVertex(vtxCnt, vtxIdxOff, vtx);
    return false;
}

bool gfx_dl_otr_filepath_handler_custom(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    char* fileName = (char*)cmd->words.w1;
    F3DGfx* nDL =
        (F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer((const char*)fileName);

    if (C0(16, 1) == 0 && nDL != nullptr) {
        g_exec_stack.call(*cmd0, nDL);
    } else {
        if (nDL != nullptr) {
            (*cmd0) = nDL;
            g_exec_stack.branch(cmd);
            return true; // shortcut cmd increment
        } else {
            assert(0 && "???");
            // gfx_path.pop_back();
            // cmd = cmd_stack.top();
            // cmd_stack.pop();
        }
    }
    return false;
}

// The original F3D microcode doesn't seem to have this opcode. Glide handles it as part of moveword
bool gfx_modify_vtx_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    gfx->GfxSpModifyVertex(C0(1, 15), C0(16, 8), (uint32_t)cmd->words.w1);
    return false;
}

// F3D, F3DEX, and F3DEX2 do the same thing but F3DEX2 has its own opcode number
bool gfx_dl_handler_common(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    F3DGfx* subGFX = (F3DGfx*)gfx->SegAddr(cmd->words.w1);
    if (C0(16, 1) == 0) {
        // Push return address
        if (subGFX != nullptr) {
            g_exec_stack.call(*cmd0, subGFX);
        }
    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;
}

bool gfx_dl_otr_hash_handler_custom(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    if (C0(16, 1) == 0) {
        // Push return address
        (*cmd0)++;

        uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

        F3DGfx* gfx = (F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);

        if (gfx != 0) {
            g_exec_stack.call(cmd, gfx);
        }
    } else {
        Interpreter* gfx = mInstance.lock().get();
        assert(0 && "????");
        (*cmd0) = (F3DGfx*)gfx->SegAddr((*cmd0)->words.w1);
        return true;
    }
    return false;
}
bool gfx_dl_index_handler(F3DGfx** cmd0) {
    // Compute seg addr by converting an index value to a offset value
    // handling 32 vs 64 bit size differences for Gfx
    // adding 1 to trigger the segaddr flow
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = (*cmd0);
    uint8_t segNum = (uint8_t)(cmd->words.w1 >> 24);
    uint32_t index = (uint32_t)(cmd->words.w1 & 0x00FFFFFF);
    uintptr_t segAddr = (segNum << 24) | (index * sizeof(F3DGfx)) + 1;

    F3DGfx* subGFX = (F3DGfx*)gfx->SegAddr(segAddr);
    if (C0(16, 1) == 0) {
        // Push return address
        if (subGFX != nullptr) {
            g_exec_stack.call((*cmd0), subGFX);
        }
    } else {
        (*cmd0) = subGFX;
        g_exec_stack.branch(cmd);
        return true; // shortcut cmd increment
    }
    return false;
}

// TODO handle special OTR opcodes later...
bool gfx_pushcd_handler_custom(F3DGfx** cmd0) {
    gfx_push_current_dir((char*)(*cmd0)->words.w1);
    return false;
}

// TODO handle special OTR opcodes later...
bool gfx_branch_z_otr_handler_f3dex2(F3DGfx** cmd0) {
    // Push return address
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = (*cmd0);

    uint8_t vbidx = (uint8_t)((*cmd0)->words.w0 & 0x00000FFF);
    uint32_t zval = (uint32_t)((*cmd0)->words.w1);

    (*cmd0)++;

    if (gfx->mRsp->loaded_vertices[vbidx].z <= zval ||
        (gfx->mRsp->extra_geometry_mode & G_EX_ALWAYS_EXECUTE_BRANCH) != 0) {
        uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

        F3DGfx* gfx = (F3DGfx*)Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(hash);

        if (gfx != 0) {
            (*cmd0) = gfx;
            g_exec_stack.branch(cmd);
            return true; // shortcut cmd increment
        }
    }
    return false;
}

// F3D, F3DEX, and F3DEX2 do the same thing but F3DEX2 has its own opcode number
bool gfx_end_dl_handler_common(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    gfx->mMarkerOn = false;
    g_exec_stack.ret();
    return true;
}

bool gfx_set_prim_depth_handler_rdp(F3DGfx** cmd) {
    Interpreter* gfx = mInstance.lock().get();
    uint32_t w1 = (*cmd)->words.w1;
    gfx->mRdp->prim_depth = (uint16_t)((w1 >> 16) & 0x7FFF); // Mask to 15 bits
    return false;
}

// Only on F3DEX2
bool gfx_geometry_mode_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(~C0(0, 24), (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_set_geometry_mode_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(0, (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_clear_geometry_mode_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode((uint32_t)cmd->words.w1, 0);
    return false;
}

bool gfx_tri1_otr_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();

    F3DGfx* cmd = *cmd0;
    uint8_t v00 = (uint8_t)(cmd->words.w0 & 0x0000FFFF);
    uint8_t v01 = (uint8_t)(cmd->words.w1 >> 16);
    uint8_t v02 = (uint8_t)(cmd->words.w1 & 0x0000FFFF);
    gfx->GfxSpTri1(v00, v01, v02, false);

    return false;
}

bool gfx_tri1_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2, false);

    return false;
}

bool gfx_tri1_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(17, 7), C1(9, 7), C1(1, 7), false);

    return false;
}

bool gfx_tri1_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(16, 8) / 10, C1(8, 8) / 10, C1(0, 8) / 10, false);

    return false;
}

// F3DEX, and F3DEX2 share a tri2 function, however F3DEX has a different quad function.
bool gfx_tri2_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(17, 7), C0(9, 7), C0(1, 7), false);
    gfx->GfxSpTri1(C1(17, 7), C1(9, 7), C1(1, 7), false);
    return false;
}

bool gfx_quad_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C0(16, 8) / 2, C0(8, 8) / 2, C0(0, 8) / 2, false);
    gfx->GfxSpTri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2, false);
    return false;
}

bool gfx_quad_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpTri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) / 2, false);
    gfx->GfxSpTri1(C1(16, 8) / 2, C1(0, 8) / 2, C1(24, 8) / 2, false);
    return false;
}

bool gfx_othermode_l_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(31 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, cmd->words.w1);

    return false;
}

bool gfx_othermode_l_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(C0(8, 8), C0(0, 8), cmd->words.w1);

    return false;
}

bool gfx_othermode_h_handler_f3dex2(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(63 - C0(8, 8) - C0(0, 8), C0(0, 8) + 1, (uint64_t)cmd->words.w1 << 32);

    return false;
}

// Only on F3DEX and older
bool gfx_set_geometry_mode_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode(0, (uint32_t)cmd->words.w1);
    return false;
}

// Only on F3DEX and older
bool gfx_clear_geometry_mode_handler_f3dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpGeometryMode((uint32_t)cmd->words.w1, 0);
    return false;
}

bool gfx_othermode_h_handler_f3d(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxSpSetOtherMode(C0(8, 8) + 32, C0(0, 8), (uint64_t)cmd->words.w1 << 32);

    return false;
}

// A resolved address still in the N64 segmented range (<= 0x0FFFFFFF) usually means SegAddr
// failed to resolve it (segment not set up). Keep it only if it belongs to a loaded module,
// where it's a real low pointer (e.g. a static TLUT) rather than an unresolved segment addr.
static bool IsValidResolvedAddress(uintptr_t addr) {
    if (addr > 0x0FFFFFFF) {
        return true;
    }

    // Still in the N64 segmented range, but might be a false positive (a real low pointer).
#ifdef _WIN32
    // For Windows, check whether the address belongs to a dll.
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(addr), &module) != 0;
#else
    // For non-Windows platforms, check whether the address belongs to a loaded object.
    Dl_info info;
    return dladdr(reinterpret_cast<void*>(addr), &info) != 0;
#endif
}

bool gfx_set_timg_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    uintptr_t i = (uintptr_t)gfx->SegAddr(cmd->words.w1);

    char* imgData = (char*)i;
    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetdata = {};
    // Default scale factors to 1 for raw N64 textures. OTR textures set these
    // from the resource, but raw textures would leave them at 0.
    rawTexMetdata.h_byte_scale = 1;
    rawTexMetdata.v_pixel_scale = 1;

    if ((i & 1) != 1) {
        if (gfx_check_image_signature(imgData) == 1) {
            std::shared_ptr<Fast::Texture> tex;
            if (gfx->mRsp->extra_geometry_mode & kExTextureRedirect) {
                tex = RedirectedTexture(imgData); // MergeShip2 (3DS characters)
            } else if (sInterfaceRedirect && strncmp(imgData + 7, "textures/", 9) == 0) {
                tex = RedirectedTexture(imgData); // MergeShip2 (3DS item and quest icons)
            }
            if (tex == nullptr) {
                tex = std::static_pointer_cast<Fast::Texture>(gfx->ResolveResourceCached(imgData));
            }
            tex = HdTexture(tex); // MergeShip2 (3DS sharper textures)

            if (tex == nullptr) {
                (*cmd0)++;
                return false;
            }

            i = (uintptr_t) reinterpret_cast<char*>(tex->ImageData);
            texFlags = tex->Flags;
            rawTexMetdata.width = tex->Width;
            rawTexMetdata.height = tex->Height;
            rawTexMetdata.h_byte_scale = tex->HByteScale;
            rawTexMetdata.v_pixel_scale = tex->VPixelScale;
            rawTexMetdata.type = tex->Type;
            rawTexMetdata.resource = tex;
        }
    }

    if (!IsValidResolvedAddress(i)) {
        return false;
    }

    gfx->GfxDpSetTextureImage(C0(21, 3), C0(19, 2), C0(0, 12) + 1, imgData, texFlags, rawTexMetdata, (void*)i);

    return false;
}

bool gfx_set_timg_otr_hash_handler_custom(F3DGfx** cmd0) {
    uintptr_t addr = (*cmd0)->words.w1;
    (*cmd0)++;
    uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (uint64_t)(*cmd0)->words.w1;

    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    // MergeShip2: the Ocarina of Time 3D pack's textures are remembered by their hash and kept loaded (they never
    // change, and the search by name is a couple of microseconds a material: PackMatrix has the measurements)
    struct PackTexture {
        std::shared_ptr<Fast::Texture> texture;
        const char* name; // (the archive's own string)
    };
    static std::unordered_map<uint64_t, PackTexture> sPackTextures;
    std::shared_ptr<Fast::Texture> texture;
    const char* fileName = nullptr;
    if (auto kept = sPackTextures.find(hash); kept != sPackTextures.end()) {
        texture = kept->second.texture;
        fileName = kept->second.name;
    } else {
        fileName = Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToCString(hash);
        if (fileName == nullptr) {
            (*cmd0)++;
            return false;
        }
        texture = std::static_pointer_cast<Fast::Texture>(
            Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
        if (texture != nullptr && strncmp(fileName, "port3ds/", 8) == 0) {
            sPackTextures.emplace(hash, PackTexture{ texture, fileName });
        }
    }
    texture = HdTexture(texture); // MergeShip2 (3DS sharper textures)
    if (texture != nullptr) {
        texFlags = texture->Flags;
        rawTexMetadata.width = texture->Width;
        rawTexMetadata.height = texture->Height;
        rawTexMetadata.h_byte_scale = texture->HByteScale;
        rawTexMetadata.v_pixel_scale = texture->VPixelScale;
        rawTexMetadata.type = texture->Type;
        rawTexMetadata.resource = texture;

        // OTRTODO: We have disabled caching for now to fix a texture corruption issue with HD texture
        // support. In doing so, there is a potential performance hit since we are not caching lookups. We
        // need to do proper profiling to see whether or not it is worth it to keep the caching system.

        char* tex = reinterpret_cast<char*>(texture->ImageData);

        if (tex != nullptr) {
            (*cmd0)--;
            uintptr_t oldData = (*cmd0)->words.w1;
            // TODO: wtf??
            (*cmd0)->words.w1 = (uintptr_t)tex;

            // if (ourHash != (uint64_t)-1) {
            //     auto res = ResourceLoad(ourHash);
            // }

            (*cmd0)++;
        }

        (*cmd0)--;
        F3DGfx* cmd = (*cmd0);
        uint32_t fmt = C0(21, 3);
        uint32_t size = C0(19, 2);
        uint32_t width = C0(0, 12) + 1;

        if (tex != NULL) {
            Interpreter* gfx = mInstance.lock().get();
            gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texFlags, rawTexMetadata, tex);
        }
    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_HASH: Texture is null");
    }

    (*cmd0)++;
    return false;
}

bool gfx_set_timg_otr_filepath_handler_custom(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    const char* fileName = (char*)cmd->words.w1;

    uint32_t texFlags = 0;
    RawTexMetadata rawTexMetadata = {};

    std::shared_ptr<Fast::Texture> texture = std::static_pointer_cast<Fast::Texture>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(fileName));
    texture = HdTexture(texture); // MergeShip2 (3DS sharper textures)
    if (texture != nullptr) {
        Interpreter* gfx = mInstance.lock().get();
        texFlags = texture->Flags;
        rawTexMetadata.width = texture->Width;
        rawTexMetadata.height = texture->Height;
        rawTexMetadata.h_byte_scale = texture->HByteScale;
        rawTexMetadata.v_pixel_scale = texture->VPixelScale;
        rawTexMetadata.type = texture->Type;
        rawTexMetadata.resource = texture;

        uint32_t fmt = C0(21, 3);
        uint32_t size = C0(19, 2);
        uint32_t width = C0(0, 12) + 1;

        gfx->GfxDpSetTextureImage(fmt, size, width, fileName, texFlags, rawTexMetadata,
                                  reinterpret_cast<char*>(texture->ImageData));
    } else {
        SPDLOG_ERROR("G_SETTIMG_OTR_FILEPATH: Texture is null");
    }
    return false;
}

bool gfx_set_fb_handler_custom(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = mInstance.lock().get();
    gfx->Flush();

    if (gfx->mShadowPass) {
        // MergeShip2 real-time shadows: the light's pass keeps to its own framebuffer; what the game draws into
        // another meanwhile is left out of it (GfxSpTri1)
        gfx->mFbActive = cmd->words.w1 != 0;
        gfx->mActiveFrameBuffer =
            cmd->words.w1 ? gfx->mFrameBuffers.find((int32_t)cmd->words.w1) : gfx->mFrameBuffers.end();
        return false;
    }
    if (cmd->words.w1) {
        gfx->SetFrameBuffer((int32_t)cmd->words.w1, 1.0f);
        gfx->mActiveFrameBuffer = gfx->mFrameBuffers.find((int32_t)cmd->words.w1);
        gfx->mFbActive = true;
    } else {
        gfx->ResetFrameBuffer();
        gfx->mFbActive = false;
        gfx->mActiveFrameBuffer = gfx->mFrameBuffers.end();
    }
    return false;
}

bool gfx_reset_fb_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    gfx->Flush();
    gfx->mFbActive = false;
    gfx->mActiveFrameBuffer = gfx->mFrameBuffers.end();
    if (gfx->mShadowPass) {
        return false; // (MergeShip2 real-time shadows: see gfx_set_fb_handler_custom)
    }
    gfx->mRapi->StartDrawToFramebuffer(gfx->mRendersToFb ? gfx->mGameFb : 0,
                                       (float)gfx->mCurDimensions.height / gfx->mNativeDimensions.height);
    // Force viewport and scissor to reapply against the main framebuffer, in case a previous smaller
    // framebuffer truncated the values
    gfx->mRdp->viewport_or_scissor_changed = true;
    gfx->mRenderingState.viewport = {};
    gfx->mRenderingState.scissor = {};
    return false;
}

bool gfx_copy_fb_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    bool* hasCopiedPtr = (bool*)cmd->words.w1;

    gfx->Flush();
    if (gfx->mShadowPass) {
        return false; // (MergeShip2 real-time shadows: see gfx_set_fb_handler_custom)
    }
    gfx->CopyFrameBuffer(C0(11, 11), C0(0, 11), (bool)C0(22, 1), hasCopiedPtr);
    return false;
}

bool gfx_read_fb_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    int32_t width, height;
    [[maybe_unused]] int32_t ulx, uly;
    uint16_t* rgba16Buffer = (uint16_t*)cmd->words.w1;
    int fbId = C0(0, 8);
    bool bswap = C0(8, 1);
    ++(*cmd0);
    cmd = *cmd0;
    // Specifying the upper left origin value is unused and unsupported at the renderer level
    ulx = C0(0, 16);
    uly = C0(16, 16);
    width = C1(0, 16);
    height = C1(16, 16);

    gfx->Flush();
    if (gfx->mShadowPass) {
        return false; // (MergeShip2 real-time shadows: read when the frame itself is drawn)
    }
    gfx->mRapi->ReadFramebufferToCPU(fbId, width, height, rgba16Buffer);

#ifndef IS_BIGENDIAN
    // byteswap the output to BE
    if (bswap) {
        for (size_t i = 0; i < (size_t)width * height; i++) {
            rgba16Buffer[i] = BE16SWAP(rgba16Buffer[i]);
        }
    }
#endif

    return false;
}

bool gfx_register_blended_texture_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    // Flush incase we are replacing a previous blended texture that hasn't been finialized to the GPU
    gfx->Flush();

    char* timg = (char*)cmd->words.w1;

    ++(*cmd0);
    cmd = *cmd0;

    uint8_t* mask = (uint8_t*)cmd->words.w0;
    uint8_t* replacementTex = (uint8_t*)cmd->words.w1;

    if (!gfx_check_image_signature(timg)) {
        SPDLOG_ERROR(
            "OTR_G_REGBLENDEDTEX: Texture is not a valid OTR resource name, unable to register blended texture");
        return false;
    }

    // With no mask, we should clear the blended texture
    if (mask == nullptr) {
        gfx->UnregisterBlendedTexture(timg);
    } else {
        gfx->RegisterBlendedTexture(timg, mask, replacementTex);
    }

    return false;
}

bool gfx_set_timg_fb_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->Flush();
    gfx->mRapi->SelectTextureFb((uint32_t)cmd->words.w1);
    gfx->mRdp->textures_changed[0] = false;
    gfx->mRdp->textures_changed[1] = false;
    return false;
}

bool gfx_set_grayscale_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->grayscale = cmd->words.w1;
    return false;
}

bool gfx_load_block_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadBlock(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_load_block_wide_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    uint32_t tile = cmd->words.w0 & 0x7;
    uint32_t lrs = cmd->words.w1;

    (*cmd0)++;
    cmd = *cmd0;

    uint32_t uls = (cmd->words.w0 >> 16) & 0xFFFF;
    uint32_t ult = (cmd->words.w0 >> 0) & 0xFFFF;
    uint32_t dxt = (cmd->words.w1 >> 0) & 0xFFF;

    gfx->GfxDpLoadBlock(tile, uls, ult, lrs, dxt);
    return false;
}

bool gfx_load_tile_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadTile(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_tile_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetTile(C0(21, 3), C0(19, 2), C0(9, 9), C0(0, 9), C1(24, 3), C1(20, 4), C1(18, 2), C1(14, 4), C1(10, 4),
                      C1(8, 2), C1(4, 4), C1(0, 4));
    return false;
}

bool gfx_set_tile_size_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetTileSize(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_tile_size_interp_handler_rdp(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = mInstance.lock().get();

    if (gfx->mInterpolationIndex == gfx->mInterpolationIndexTarget) {
        int tile = C1(24, 3);
        gfx->GfxDpSetTileSize(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
        ++(*cmd0);
        memcpy(&gfx->mRdp->texture_tile[tile].uls, &(*cmd0)->words.w0, sizeof(float));
        memcpy(&gfx->mRdp->texture_tile[tile].ult, &(*cmd0)->words.w1, sizeof(float));
        ++(*cmd0);
        memcpy(&gfx->mRdp->texture_tile[tile].lrs, &(*cmd0)->words.w0, sizeof(float));
        memcpy(&gfx->mRdp->texture_tile[tile].lrt, &(*cmd0)->words.w1, sizeof(float));
    } else {
        ++(*cmd0);
        ++(*cmd0);
    }

    return false;
}

bool gfx_set_tile_size_lerp_handler_rdp(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = mInstance.lock().get();

    int tile = C1(24, 3);
    float coords[8];
    for (int i = 0; i < 8; i += 2) {
        ++(*cmd0);
        memcpy(&coords[i], &(*cmd0)->words.w0, sizeof(float));
        memcpy(&coords[i + 1], &(*cmd0)->words.w1, sizeof(float));
    }

    float t = gfx->mInterpolationT;
    gfx->mRdp->texture_tile[tile].uls = coords[0] + t * (coords[4] - coords[0]);
    gfx->mRdp->texture_tile[tile].ult = coords[1] + t * (coords[5] - coords[1]);
    gfx->mRdp->texture_tile[tile].lrs = coords[2] + t * (coords[6] - coords[2]);
    gfx->mRdp->texture_tile[tile].lrt = coords[3] + t * (coords[7] - coords[3]);
    gfx->mRdp->textures_changed[0] = true;
    gfx->mRdp->textures_changed[1] = true;

    return false;
}

bool gfx_set_interpolation_index_target(F3DGfx** cmd0) {
    F3DGfx* cmd = *cmd0;
    Interpreter* gfx = mInstance.lock().get();

    gfx->mInterpolationIndexTarget = cmd->words.w1;
    return false;
}

bool gfx_load_tlut_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpLoadTlut(C1(24, 3), C1(14, 10));
    return false;
}

bool gfx_set_env_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetEnvColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_prim_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetPrimColor(C0(8, 8), C0(0, 8), C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_fog_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetFogColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

// CENTER/SCALE and K4/K5 are wired as combiner inputs, so the standard (A-B)*C+D
// shader path covers their common uses. TODO: chroma-key width/threshold
// gating from G_SETKEYR/GB (wR/wG/wB ignored) and the YUV->RGB matrix K0..K3
// applied during texture sampling.
// G_SETKEYR: w1 = [wR:12 | cR:8 | sR:8]
bool gfx_set_key_r_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->key_center.r = C1(8, 8);
    gfx->mRdp->key_scale.r = C1(0, 8);
    return false;
}

// G_SETKEYGB: w0 = [op:8 | wG:12 | _:4 | wB:12], w1 = [cG:8 | sG:8 | cB:8 | sB:8]
bool gfx_set_key_gb_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->key_center.g = C1(24, 8);
    gfx->mRdp->key_scale.g = C1(16, 8);
    gfx->mRdp->key_center.b = C1(8, 8);
    gfx->mRdp->key_scale.b = C1(0, 8);
    return false;
}

// G_SETCONVERT: w0 = [op:8 | k0:9 | k1:9 | k2_hi:4], w1 = [k2_lo:5 | k3:9 | k4:9 | k5:9]
// K0..K5 are signed 9-bit values; sign-extend after decoding.
bool gfx_set_convert_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->mRdp->convert_k[0] = sign_extend_9(C0(13, 9));
    gfx->mRdp->convert_k[1] = sign_extend_9(C0(4, 9));
    // k2 is split across w0 and w1
    gfx->mRdp->convert_k[2] = sign_extend_9((C0(0, 4) << 5) | C1(27, 5));
    gfx->mRdp->convert_k[3] = sign_extend_9(C1(18, 9));
    gfx->mRdp->convert_k[4] = sign_extend_9(C1(9, 9));
    gfx->mRdp->convert_k[5] = sign_extend_9(C1(0, 9));
    return false;
}

bool gfx_set_blend_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetBlendColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_fill_color_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetFillColor((uint32_t)cmd->words.w1);
    return false;
}

bool gfx_set_intensity_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetGrayscaleColor(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
    return false;
}

bool gfx_set_combine_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;

    gfx->GfxDpSetCombineMode(
        color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)), alpha_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)),
        color_comb(C0(5, 4), C1(24, 4), C0(0, 5), C1(6, 3)), alpha_comb(C1(21, 3), C1(3, 3), C1(18, 3), C1(0, 3)));
    return false;
}

bool gfx_tex_rect_and_flip_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    int8_t opcode = (int8_t)(cmd->words.w0 >> 24);
    int32_t lrx, lry, tile, ulx, uly;
    uint32_t uls, ult, dsdx, dtdy;

    lrx = C0(12, 12);
    lry = C0(0, 12);
    tile = C1(24, 3);
    ulx = C1(12, 12);
    uly = C1(0, 12);
    // TODO make sure I don't need to increment cmd0
    ++(*cmd0);
    cmd = *cmd0;
    uls = C1(16, 16);
    ult = C1(0, 16);
    ++(*cmd0);
    cmd = *cmd0;
    dsdx = C1(16, 16);
    dtdy = C1(0, 16);

    gfx->GfxDpTextureRectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == RDP_G_TEXRECTFLIP);
    return false;
}

bool gfx_tex_rect_wide_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    int8_t opcode = (int8_t)(cmd->words.w0 >> 24);
    int32_t lrx, lry, tile, ulx, uly;
    uint32_t uls, ult, dsdx, dtdy;

    lrx = static_cast<int32_t>((C0(0, 24) << 8)) >> 8;
    lry = static_cast<int32_t>((C1(0, 24) << 8)) >> 8;
    tile = C1(24, 3);
    ++(*cmd0);
    cmd = *cmd0;
    ulx = static_cast<int32_t>((C0(0, 24) << 8)) >> 8;
    uly = static_cast<int32_t>((C1(0, 24) << 8)) >> 8;
    ++(*cmd0);
    cmd = *cmd0;
    uls = C0(16, 16);
    ult = C0(0, 16);
    dsdx = C1(16, 16);
    dtdy = C1(0, 16);
    gfx->GfxDpTextureRectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == RDP_G_TEXRECTFLIP);
    return false;
}

bool gfx_image_rect_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    int16_t tile, iw, ih;
    int16_t x0, y0, s0, t0;
    int16_t x1, y1, s1, t1;
    tile = C0(0, 3);
    iw = C1(16, 16);
    ih = C1(0, 16);
    cmd = ++(*cmd0);
    x0 = C0(16, 16);
    y0 = C0(0, 16);
    s0 = C1(16, 16);
    t0 = C1(0, 16);
    cmd = ++(*cmd0);
    x1 = C0(16, 16);
    y1 = C0(0, 16);
    s1 = C1(16, 16);
    t1 = C1(0, 16);
    gfx->GfxDpImageRectangle(tile, iw, ih, x0, y0, s0, t0, x1, y1, s1, t1);

    return false;
}

bool gfx_fill_rect_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpFillRectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
    return false;
}

bool gfx_fill_wide_rect_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);
    int32_t lrx, lry, ulx, uly;

    lrx = (int32_t)(C0(0, 24) << 8) >> 8;
    lry = (int32_t)(C1(0, 24) << 8) >> 8;
    cmd = ++(*cmd0);
    ulx = (int32_t)(C0(0, 24) << 8) >> 8;
    uly = (int32_t)(C1(0, 24) << 8) >> 8;
    gfx->GfxDpFillRectangle(ulx, uly, lrx, lry);

    return false;
}

bool gfx_SetScissor_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetScissor(C1(24, 2), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
    return false;
}

bool gfx_set_z_img_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetZImage(gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_set_c_img_handler_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetColorImage(C0(21, 3), C0(19, 2), C0(0, 11), gfx->SegAddr(cmd->words.w1));
    return false;
}

bool gfx_rdp_set_other_mode_rdp(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxDpSetOtherMode(C0(0, 24), (uint32_t)cmd->words.w1);
    return false;
}

bool gfx_bg_copy_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    if (!gfx->mMarkerOn) {
        gfx->Gfxs2dexBgCopy((F3DuObjBg*)cmd->words.w1); // not gfx->SegAddr here it seems
    }
    return false;
}

bool gfx_bg_1cyc_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->Gfxs2dexBg1cyc((F3DuObjBg*)cmd->words.w1);
    return false;
}

bool gfx_obj_rectangle_handler_s2dex(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    if (!gfx->mMarkerOn) {
        gfx->Gfxs2dexRecyCopy((F3DuObjSprite*)cmd->words.w1); // not gfx->SegAddr here it seems
    }
    return false;
}

bool gfx_extra_geometry_mode_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->GfxSpExtraGeometryMode(~C0(0, 24), (uint32_t)cmd->words.w1);
    return false;
}

bool gfx_stubbed_command_handler(F3DGfx** cmd0) {
    return false;
}

// ---------------------------------------------------------------------------------------------------------------
// MergeShip2: extended vertices, for models that have what an N64 vertex cannot hold (tools/port3ds writes them).
//
// G_SKIN_PALETTE_OTR_HASH (0x4C; two words, then the palette resource's hash): w0 bits 0-7 are the entry count
//   (0 clears the palette). The palette is `count` records of { u8 slot; u8 pad[3]; float m[4][3]; }: a slot of the
//   matrix array in segment 0x0D (where the game's own skeletons put theirs) and the constant matrix that takes a
//   vertex from the model's bind pose into that matrix's frame (rows as this file's matrices, row 3 the
//   translation). Entry i becomes "constant i x array[slot i]" for the vertices loaded after it.
// G_VTX_EX_OTR_HASH (0x4D; two words, then the vertex resource's hash): w0 bits 12-19 are the count, bits 0-7 the
//   first vertex buffer slot; w1 is the byte offset into the resource. Loads F3DVtxEx records: a float position, a
//   normal AND a colour (a lit vertex is light x colour), two texture coordinate sets (the second goes to the slot 32
//   above, where G_EX_TEXEL1_UPPER_VERTEX reads it) and up to four palette entries with weights. A vertex without
//   weights goes through the model matrix like any other vertex.
// G_PUSH_SHADER_OTR_HASH (0x4E; two words, then a hash): G_PUSH_SHADER with the shader named by the hash of an
//   archive file (the shader's name without an extension is a file of its own in the pack), so a binary display
//   list can carry it. G_POP_SHADER ends it as usual.
static constexpr int8_t OTR_G_SKIN_PALETTE_OTR_HASH = OPCODE(0x4c);
static constexpr int8_t OTR_G_VTX_EX_OTR_HASH = OPCODE(0x4d);
static constexpr int8_t OTR_G_PUSH_SHADER_OTR_HASH = OPCODE(0x4e);

#pragma pack(push, 1)
struct F3DVtxEx {
    float pos[3];
    int8_t nrm[3];
    uint8_t flags; // 1: the first texture coordinate set comes from the normal (sphere map), 2: the second does
    uint8_t color[4];
    int16_t uv[2][2]; // 10.5 fixed; for a set made from the normal, the texture's size in the same units
    uint8_t bone[4];
    uint8_t weight[4]; // of 255; all zero: not skinned
};
struct SkinPaletteEntry {
    uint8_t slot;
    uint8_t pad[3];
    float m[4][3];
};
#pragma pack(pop)

static constexpr uint32_t kSkinPaletteMax = 96;
static float sSkinPalette[kSkinPaletteMax][4][4];
static uint32_t sSkinPaletteCount = 0;

// The pack's vertex and palette data, kept loaded while lists that use it may still run.
static const uint8_t* ExResource(uint64_t hash, size_t* size) {
    static std::unordered_map<uint64_t, std::shared_ptr<Ship::IResource>> sLoaded;
    auto it = sLoaded.find(hash);
    if (it == sLoaded.end()) {
        it = sLoaded.emplace(hash, Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(hash)).first;
    }
    if (it->second == nullptr) {
        *size = 0;
        return nullptr;
    }
    *size = it->second->GetPointerSize();
    return (const uint8_t*)it->second->GetRawPointer();
}

// A matrix of the game's, as GfxSpMatrix reads it (the interpolated one when the frame has it).
static void ReadGameMatrix(Interpreter* gfx, const int32_t* addr, float matrix[4][4]) {
    if (auto it = gfx->mCurMtxReplacements->find((Mtx*)addr); it != gfx->mCurMtxReplacements->end()) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                float v = it->second.mf[i][j];
                int as_int = (int)(v * 65536.0f);
                matrix[i][j] = as_int * (1.0f / 65536.0f);
            }
        }
        return;
    }
#ifndef GBI_FLOATS
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            int32_t int_part = addr[i * 2 + j / 2];
            uint32_t frac_part = addr[8 + i * 2 + j / 2];
            matrix[i][j] = (int32_t)((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
            matrix[i][j + 1] = (int32_t)((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
        }
    }
#else
    memcpy(matrix, addr, sizeof(float) * 16);
#endif
}

bool gfx_skin_palette_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    const uint32_t count = C0(0, 8);
    (*cmd0)++;
    const uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

    sSkinPaletteCount = 0;
    if (count == 0 || count > kSkinPaletteMax) {
        return false;
    }
    size_t size = 0;
    const uint8_t* data = ExResource(hash, &size);
    const uintptr_t array = gfx->mSegmentPointers[0x0D];
    if (data == nullptr || size < (size_t)count * sizeof(SkinPaletteEntry) || array == 0) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        SkinPaletteEntry entry;
        memcpy(&entry, data + (size_t)i * sizeof(SkinPaletteEntry), sizeof(entry));
        float constant[4][4];
        for (int r = 0; r < 4; r++) {
            constant[r][0] = entry.m[r][0];
            constant[r][1] = entry.m[r][1];
            constant[r][2] = entry.m[r][2];
            constant[r][3] = r == 3 ? 1.0f : 0.0f;
        }
        float game[4][4];
        ReadGameMatrix(gfx, (const int32_t*)(array + (uintptr_t)entry.slot * sizeof(Mtx)), game);
        Interpreter::MatrixMul(sSkinPalette[i], constant, game);
    }
    sSkinPaletteCount = count;
    return false;
}

static void GfxSpVertexEx(Interpreter* gfx, size_t count, size_t dest, const F3DVtxEx* vertices) {
    RSP* rsp = gfx->mRsp;
    const bool lighting = (rsp->geometry_mode & G_LIGHTING) != 0 && !gfx->mShadowPass; // (shapes alone there)
    const float(*model)[4] = rsp->modelview_matrix_stack[rsp->modelview_matrix_stack_size - 1];
    const int lights = std::clamp((int)rsp->current_num_lights - 1, 0, MAX_LIGHTS);

    // Light and look-at directions: in model space for plain vertices (as GfxSpVertex has them), and as they are
    // (world space: the game's model matrices carry no view) for skinned ones, whose palette is world matrices.
    float dirModel[MAX_LIGHTS][3], dirWorld[MAX_LIGHTS][3], lookModel[2][3], lookWorld[2][3];
    for (int i = 0; i < lights + 2; i++) {
        const F3DLight_t* light = i < lights ? &rsp->current_lights[i].l : &rsp->lookat[i - lights];
        float* world = i < lights ? dirWorld[i] : lookWorld[i - lights];
        float* local = i < lights ? dirModel[i] : lookModel[i - lights];
        world[0] = light->dir[0] / 127.0f;
        world[1] = light->dir[1] / 127.0f;
        world[2] = light->dir[2] / 127.0f;
        Interpreter::TransposedMatrixMul(local, world, model);
        Interpreter::NormalizeVector(local);
        Interpreter::NormalizeVector(world);
    }

    for (size_t n = 0; n < count; n++, dest++) {
        if (dest >= MAX_VERTICES) {
            return;
        }
        F3DVtxEx v;
        memcpy(&v, &vertices[n], sizeof(v));
        LoadedVertex* d = &rsp->loaded_vertices[dest];

        float pos[3] = { v.pos[0], v.pos[1], v.pos[2] };
        float nrm[3] = { v.nrm[0] / 127.0f, v.nrm[1] / 127.0f, v.nrm[2] / 127.0f };
        const uint32_t total = (uint32_t)v.weight[0] + v.weight[1] + v.weight[2] + v.weight[3];
        const bool skinned = total > 0 && sSkinPaletteCount > 0;
        const float(*clip)[4] = rsp->MP_matrix;
        const float(*dirs)[3] = dirModel;
        const float(*look)[3] = lookModel;
        if (skinned) {
            float p[3] = { 0.0f, 0.0f, 0.0f };
            float q[3] = { 0.0f, 0.0f, 0.0f };
            for (int k = 0; k < 4; k++) {
                if (v.weight[k] == 0 || v.bone[k] >= sSkinPaletteCount) {
                    continue;
                }
                const float w = v.weight[k] / (float)total;
                const float(*m)[4] = sSkinPalette[v.bone[k]];
                for (int c = 0; c < 3; c++) {
                    p[c] += w * (v.pos[0] * m[0][c] + v.pos[1] * m[1][c] + v.pos[2] * m[2][c] + m[3][c]);
                    q[c] += w * (nrm[0] * m[0][c] + nrm[1] * m[1][c] + nrm[2] * m[2][c]);
                }
            }
            memcpy(pos, p, sizeof(pos));
            memcpy(nrm, q, sizeof(nrm));
            Interpreter::NormalizeVector(nrm);
            // (real-time shadows, the light's pass: the world goes through the light's view, not the camera's)
            clip = gfx->mShadowPass ? gfx->mShadowLight : rsp->P_matrix;
            dirs = dirWorld;
            look = lookWorld;
        }

        float x = pos[0] * clip[0][0] + pos[1] * clip[1][0] + pos[2] * clip[2][0] + clip[3][0];
        float y = pos[0] * clip[0][1] + pos[1] * clip[1][1] + pos[2] * clip[2][1] + clip[3][1];
        float z = pos[0] * clip[0][2] + pos[1] * clip[1][2] + pos[2] * clip[2][2] + clip[3][2];
        float w = pos[0] * clip[0][3] + pos[1] * clip[1][3] + pos[2] * clip[2][3] + clip[3][3];
        if (!gfx->mShadowPass) { // (the light's view is square: GfxSpVertex)
            x = gfx->AdjXForAspectRatio(x);
        }

        if (lighting) {
            float r = rsp->current_lights[lights].l.col[0];
            float g = rsp->current_lights[lights].l.col[1];
            float b = rsp->current_lights[lights].l.col[2];
            for (int i = 0; i < lights; i++) {
                const float intensity = nrm[0] * dirs[i][0] + nrm[1] * dirs[i][1] + nrm[2] * dirs[i][2];
                if (intensity > 0.0f) {
                    r += intensity * rsp->current_lights[i].l.col[0];
                    g += intensity * rsp->current_lights[i].l.col[1];
                    b += intensity * rsp->current_lights[i].l.col[2];
                }
            }
            d->color.r = (uint8_t)(std::min(r, 255.0f) * v.color[0] / 255.0f);
            d->color.g = (uint8_t)(std::min(g, 255.0f) * v.color[1] / 255.0f);
            d->color.b = (uint8_t)(std::min(b, 255.0f) * v.color[2] / 255.0f);
            // (toon outlines: where the normal points on screen)
            sOutlineNormal[dest][0] =
                gfx->AdjXForAspectRatio(nrm[0] * clip[0][0] + nrm[1] * clip[1][0] + nrm[2] * clip[2][0]);
            sOutlineNormal[dest][1] = nrm[0] * clip[0][1] + nrm[1] * clip[1][1] + nrm[2] * clip[2][1];
            sOutlineHasNormal[dest] = true;
        } else {
            d->color.r = v.color[0];
            d->color.g = v.color[1];
            d->color.b = v.color[2];
            sOutlineHasNormal[dest] = false;
        }

        float uv[2][2];
        for (int set = 0; set < 2; set++) {
            if (v.flags & (1 << set)) {
                // sphere map from the normal: the view-space normal's x and y over the whole texture
                for (int axis = 0; axis < 2; axis++) {
                    float dot = nrm[0] * look[axis][0] + nrm[1] * look[axis][1] + nrm[2] * look[axis][2];
                    dot = std::clamp(dot, -1.0f, 1.0f);
                    uv[set][axis] = (dot + 1.0f) * 0.5f * v.uv[set][axis];
                }
            } else {
                uv[set][0] = (float)(v.uv[set][0] * rsp->texture_scaling_factor.s >> 16);
                uv[set][1] = (float)(v.uv[set][1] * rsp->texture_scaling_factor.t >> 16);
            }
        }
        d->u = uv[0][0];
        d->v = uv[0][1];
        if (dest + 32 < MAX_VERTICES) {
            rsp->loaded_vertices[dest + 32].u = uv[1][0];
            rsp->loaded_vertices[dest + 32].v = uv[1][1];
        }

        d->clip_rej = 0;
        if (x < -w) {
            d->clip_rej |= 1;
        }
        if (x > w) {
            d->clip_rej |= 2;
        }
        if (y < -w) {
            d->clip_rej |= 4;
        }
        if (y > w) {
            d->clip_rej |= 8;
        }
        if (z > w) {
            d->clip_rej |= 32;
        }
        d->x = x;
        d->y = y;
        d->z = z;
        d->w = w;

        if (rsp->geometry_mode & G_FOG) {
            if (fabsf(w) < 0.001f) {
                w = 0.001f;
            }
            float winv = 1.0f / w;
            if (winv < 0.0f) {
                winv = std::numeric_limits<int16_t>::max();
            }
            float fog_z = z * winv * rsp->fog_mul + rsp->fog_offset;
            fog_z = Ship::Math::clamp(fog_z, 0.0f, 255.0f);
            d->color.a = fog_z;
        } else {
            d->color.a = v.color[3];
        }
        d->alpha = v.color[3];
    }
}

bool gfx_vtx_ex_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *cmd0;
    const size_t count = C0(12, 8);
    const size_t dest = C0(0, 8);
    const uintptr_t offset = cmd->words.w1;
    (*cmd0)++;
    const uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

    size_t size = 0;
    const uint8_t* data = ExResource(hash, &size);
    if (data == nullptr || offset + count * sizeof(F3DVtxEx) > size) {
        return false;
    }
    GfxSpVertexEx(gfx, count, dest, (const F3DVtxEx*)(data + offset));
    return false;
}

bool gfx_push_shader_hash_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    (*cmd0)++;
    const uint64_t hash = ((uint64_t)(*cmd0)->words.w0 << 32) + (*cmd0)->words.w1;

    // (the names live as long as the program: the renderer keeps the pointers it is given)
    static std::unordered_map<uint64_t, std::string> sNames;
    auto it = sNames.find(hash);
    if (it == sNames.end()) {
        const std::string* name =
            Ship::Context::GetRawInstance()->GetResourceManager()->GetArchiveManager()->HashToString(hash);
        it = sNames.emplace(hash, name != nullptr ? *name : std::string()).first;
    }
    if (it->second.empty()) {
        gfx->mShaderStack.push(0xFFFF); // (no such shader: the default one, and the pop still pairs up)
        return false;
    }
    const char* path = it->second.c_str();
    size_t shaderId = static_cast<size_t>(-1);
    for (const auto& shader : gfx->mShaders) {
        if (strcmp(shader.second, path) == 0) {
            shaderId = shader.first;
            break;
        }
    }
    if (shaderId == static_cast<size_t>(-1)) {
        shaderId = gfx->mShadersIndex++;
        gfx->mShaders[shaderId] = path;
    }
    gfx->mShaderStack.push(shaderId);
    return false;
}

// MergeShip2: G_SETBLENDMODE_EX
bool gfx_set_blend_mode_ex_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);

    gfx->mRdp->blend_ex_mode = C0(0, 12);
    gfx->mRdp->blend_ex_color = { (uint8_t)C1(24, 8), (uint8_t)C1(16, 8), (uint8_t)C1(8, 8), (uint8_t)C1(0, 8) };
    return false;
}

// MergeShip2: G_REGS_EX (0x4F). A model that stands in for a skeleton is drawn after the skeleton's limbs have been
// walked, and the game's code sets the colour registers limb by limb while it walks (an eye in one colour, the body
// in another). The registers as they were at a limb are kept in a slot, and the model's lists take from the slot of
// the limb each part belongs to.
//   w0 bits 16-23 = 0: keep the environment and primitive colours, as they are now, in slot w1.
//                 = 1: lists read slot w1 from here on (0xFFFFFFFF: none). w0 bit 0: the model is being drawn where
//                      the game draws what fades (its translucent buffer).
//                 = 2: take from the slot being read. w1 bits 0-1: what the primitive colour's rgb becomes (0 stays,
//                      1 the kept environment colour's, 2 the kept primitive colour's), bits 2-4 its alpha (0 stays,
//                      1 the kept environment alpha, 2 the kept primitive alpha, 3 and 4 one minus those); bits 8-9
//                      and 10-12 the same for the environment colour. Nothing is taken while no slot is read.
//                 = 3: an opaque material whose alpha a kept register scales (w1: 1 the kept environment alpha, 2 the
//                      kept primitive alpha): while the model is drawn where the game draws what fades and that
//                      alpha is below 255, the material blends by its alpha (G_SETBLENDMODE_EX source alpha, one
//                      minus source alpha) instead of covering what is behind it.
// A model that stands in for one of the game's plain lists (a pot, a boulder) is lit as the 3DS game lights it: by
// the scene's own light settings and its material's factors, not by the lights the game bound for the actor (the
// game's own props are mostly shaded by their vertex colours, and its lights may be near black).
//                 = 4: keep the lights as they are now.
//                 = 5: put the kept lights back.
//                 = 6: light what follows by the scene's settings (7): two directional lights by the material's
//                      diffuse factor (w1 bits 8-15, of 255) and the ambient by its ambient factor (bits 0-7).
//                      Nothing changes while the game has given no settings or they are not valid.
//                 = 7: w1 points at the scene's settings, storage of the game's that is read whenever 6 runs:
//                      { u8 valid; u8 ambient[3]; u8 color1[3]; s8 dir1[3]; u8 color2[3]; s8 dir2[3]; }.
// Real-time shadows ("MergeShip2 real-time shadows" below):
//                 = 8: what has been drawn so far is darkened where it lies in shadow, and the next frame draws its
//                      shadows' depth first. The game puts it where the world's opaque surfaces are all drawn and
//                      nothing else yet: at the head of its translucent buffer, behind the view's matrices.
//                 = 9: w1 points at the light, storage of the game's that is read at the start of every frame:
//                      { u8 on; u8 pad[3]; f32 dir[3] (the way to the light); f32 focus[3] (the middle of what the
//                      shadows cover); f32 radius (half that area's width); f32 shade (how bright a surface in
//                      shadow stays, 0 to 1); f32 away (the same for a surface turned away from the light) }.
static constexpr int8_t OTR_G_REGS_EX = OPCODE(0x4f);
namespace {
struct RegSlot {
    RGBA env;
    RGBA prim;
};
constexpr uint32_t kRegSlots = 4096;
RegSlot sRegSlots[kRegSlots];
int32_t sRegSlotRead = -1;
bool sRegFades = false;
struct SceneLightsEx {
    uint8_t valid;
    uint8_t ambient[3];
    uint8_t color1[3];
    int8_t dir1[3];
    uint8_t color2[3];
    int8_t dir2[3];
};
const SceneLightsEx* sSceneLightsEx = nullptr;
struct ShadowLightEx {
    uint8_t on;
    uint8_t bothSides;
    uint8_t fromPlane; // the light comes down from a level plane at `top`: what is above it is not there for it
    uint8_t casts;     // shadows are cast (else the pass over the screen is only for the rest: ao, clouds)
    float dir[3];
    float focus[3];
    float radius;
    float shade;
    float away;
    float top;   // (fromPlane) the plane's height
    float deep;  // (fromPlane) how far under it the light reaches
    float reach; // (fromPlane) how high the plane is over the ground: a shadow fades out towards that far from
                 // what casts it
    float ao;      // how dark the ambient shading goes (0: none)
    float aoReach; // how far it looks, in the world's units
    float clouds;  // how dark a cloud's shadow is (0: none)
    float tint[3]; // what a shadow leaves of red, green and blue against its plain shade
    float sun[3];  // how much brighter what the light reaches is drawn (1: as it is)
    int32_t lampCount;   // the game's lamps (point lights) that light what stands about them: how many, at most 12
    float lampStrength;  // how strongly their light is drawn
    float lamps[12][8];  // each: its place (x, y, z), how far it reaches, its colour (0 to 1), one unused
};
const ShadowLightEx* sShadowLightEx = nullptr;
F3DLight sKeptLights[MAX_LIGHTS + 1];
uint8_t sKeptNumLights = 0;
bool sLightsKept = false;
} // namespace

bool gfx_regs_ex_handler_custom(F3DGfx** cmd0) {
    Interpreter* gfx = mInstance.lock().get();
    F3DGfx* cmd = *(cmd0);
    const uint32_t what = C0(16, 8);
    const uint32_t w1 = (uint32_t)cmd->words.w1;
    if (what == 8) {
        if (!gfx->mShadowPass) {
            gfx->ApplyShadows();
        }
    } else if (what == 10) {
        sSway = (const SwayEx*)cmd->words.w1; // (swaying grass; 0: none)
    } else if (what == 11) {
        sShadowNoCast = w1 != 0; // (what follows casts no shadow; 0: it does again)
    } else if (what == 9) {
        sShadowLightEx = (const ShadowLightEx*)cmd->words.w1;
    } else if (what == 13) {
        if (!gfx->mShadowPass) {
            gfx->KeepWorldCamera();
        }
    } else if (what == 12) {
        // (w1: the pass's numbers, 48 of them; 0: the game left the place empty)
        if (!gfx->mShadowPass && cmd->words.w1 != 0) {
            gfx->ApplyPost((const float*)cmd->words.w1);
        }
    } else if (what == 0) {
        if (w1 < kRegSlots) {
            sRegSlots[w1] = { gfx->mRdp->env_color, gfx->mRdp->prim_color };
        }
    } else if (what == 1) {
        sRegSlotRead = w1 < kRegSlots ? (int32_t)w1 : -1;
        sRegFades = sRegSlotRead >= 0 && C0(0, 1) != 0;
    } else if (what == 3) {
        if (sRegSlotRead >= 0 && sRegFades && (w1 == 1 || w1 == 2)) {
            const RegSlot& slot = sRegSlots[sRegSlotRead];
            if ((w1 == 1 ? slot.env.a : slot.prim.a) < 255) {
                gfx->mRdp->blend_ex_mode =
                    (G_BLENDEX_SRC_ALPHA << 8) | (G_BLENDEX_ONE_MINUS_SRC_ALPHA << 4) | G_BLENDEX_EQ_ADD;
            }
        }
    } else if (what == 2 && sRegSlotRead >= 0) {
        const RegSlot& slot = sRegSlots[sRegSlotRead];
        const auto take = [&slot](RGBA& to, uint32_t rgb, uint32_t alpha) {
            if (rgb == 1 || rgb == 2) {
                const RGBA& from = rgb == 1 ? slot.env : slot.prim;
                to.r = from.r;
                to.g = from.g;
                to.b = from.b;
            }
            switch (alpha) {
                case 1:
                    to.a = slot.env.a;
                    break;
                case 2:
                    to.a = slot.prim.a;
                    break;
                case 3:
                    to.a = 255 - slot.env.a;
                    break;
                case 4:
                    to.a = 255 - slot.prim.a;
                    break;
                default:
                    break;
            }
        };
        take(gfx->mRdp->prim_color, w1 & 3, (w1 >> 2) & 7);
        take(gfx->mRdp->env_color, (w1 >> 8) & 3, (w1 >> 10) & 7);
    } else if (what == 4) {
        memcpy(sKeptLights, gfx->mRsp->current_lights, sizeof(sKeptLights));
        sKeptNumLights = gfx->mRsp->current_num_lights;
        sLightsKept = true;
    } else if (what == 5) {
        if (sLightsKept) {
            memcpy(gfx->mRsp->current_lights, sKeptLights, sizeof(sKeptLights));
            gfx->mRsp->current_num_lights = sKeptNumLights;
            gfx->mRsp->lights_changed = true;
            sLightsKept = false;
        }
    } else if (what == 6) {
        const SceneLightsEx* scene = sSceneLightsEx;
        if (scene != nullptr && scene->valid) {
            const float ambient = (float)(w1 & 0xFF) / 255.0f;
            const float diffuse = (float)((w1 >> 8) & 0xFF) / 255.0f;
            F3DLight* lights = gfx->mRsp->current_lights;
            memset(lights, 0, sizeof(F3DLight) * 3);
            for (int c = 0; c < 3; c++) {
                const float first = scene->color1[c] * diffuse;
                const float second = scene->color2[c] * diffuse;
                const float around = scene->ambient[c] * ambient;
                lights[0].l.col[c] = lights[0].l.colc[c] = (uint8_t)(first > 255.0f ? 255.0f : first);
                lights[0].l.dir[c] = scene->dir1[c];
                lights[1].l.col[c] = lights[1].l.colc[c] = (uint8_t)(second > 255.0f ? 255.0f : second);
                lights[1].l.dir[c] = scene->dir2[c];
                lights[2].l.col[c] = lights[2].l.colc[c] = (uint8_t)(around > 255.0f ? 255.0f : around);
            }
            gfx->mRsp->current_num_lights = 3; // (two lights and the ambient)
            gfx->mRsp->lights_changed = true;
        }
    } else if (what == 7) {
        sSceneLightsEx = (const SceneLightsEx*)cmd->words.w1;
    }
    return false;
}

bool gfx_spnoop_command_handler_f3dex2(F3DGfx** cmd0) {
    return false;
}

class UcodeHandler {
  public:
    inline constexpr UcodeHandler(
        std::initializer_list<std::pair<int8_t, std::pair<const char*, GfxOpcodeHandlerFunc>>> initializer) {
        std::fill(std::begin(mHandlers), std::end(mHandlers),
                  std::pair<const char*, GfxOpcodeHandlerFunc>(nullptr, nullptr));

        for (const auto& [opcode, handler] : initializer) {
            mHandlers[static_cast<uint8_t>(opcode)] = handler;
        }
    }

    inline bool contains(int8_t opcode) const {
        return mHandlers[static_cast<uint8_t>(opcode)].first != nullptr;
    }

    inline std::pair<const char*, GfxOpcodeHandlerFunc> at(int8_t opcode) const {
        return mHandlers[static_cast<uint8_t>(opcode)];
    }

  private:
    std::pair<const char*, GfxOpcodeHandlerFunc> mHandlers[std::numeric_limits<uint8_t>::max() + 1];
};

static constexpr UcodeHandler rdpHandlers = {
    { RDP_G_SETTARGETINTERPINDEX,
      { "G_SETTARGETINTERPINDEX", gfx_set_interpolation_index_target } }, // G_SETTARGETINTERPINDEX
    { RDP_G_SETTILESIZE_INTERP,
      { "G_SETTILESIZE_INTERP", gfx_set_tile_size_interp_handler_rdp } },                     // G_SETTILESIZE_INTERP
    { RDP_G_SETTILESIZE_LERP, { "G_SETTILESIZE_LERP", gfx_set_tile_size_lerp_handler_rdp } }, // G_SETTILESIZE_LERP
    { RDP_G_TEXRECT, { "G_TEXRECT", gfx_tex_rect_and_flip_handler_rdp } },                    // G_TEXRECT (-28)
    { RDP_G_TEXRECTFLIP, { "G_TEXRECTFLIP", gfx_tex_rect_and_flip_handler_rdp } },            // G_TEXRECTFLIP (-27)
    { RDP_G_RDPLOADSYNC, { "mRdpLOADSYNC", gfx_stubbed_command_handler } },                   // mRdpLOADSYNC (-26)
    { RDP_G_RDPPIPESYNC, { "mRdpPIPESYNC", gfx_stubbed_command_handler } },                   // mRdpPIPESYNC (-25)
    { RDP_G_RDPTILESYNC, { "mRdpTILESYNC", gfx_stubbed_command_handler } },                   // mRdpPIPESYNC (-24)
    { RDP_G_RDPFULLSYNC, { "mRdpFULLSYNC", gfx_stubbed_command_handler } },                   // mRdpFULLSYNC (-23)
    { RDP_G_SETKEYGB, { "G_SETKEYGB", gfx_set_key_gb_handler_rdp } },                         // G_SETKEYGB (-22)
    { RDP_G_SETKEYR, { "G_SETKEYR", gfx_set_key_r_handler_rdp } },                            // G_SETKEYR (-21)
    { RDP_G_SETCONVERT, { "G_SETCONVERT", gfx_set_convert_handler_rdp } },                    // G_SETCONVERT (-20)
    { RDP_G_SETSCISSOR, { "G_SETSCISSOR", gfx_SetScissor_handler_rdp } },                     // G_SETSCISSOR (-19)
    { RDP_G_SETPRIMDEPTH, { "G_SETPRIMDEPTH", gfx_set_prim_depth_handler_rdp } },             // G_SETPRIMDEPTH (-18)
    { RDP_G_RDPSETOTHERMODE, { "mRdpSETOTHERMODE", gfx_rdp_set_other_mode_rdp } },            // mRdpSETOTHERMODE (-17)
    { RDP_G_LOADTLUT, { "G_LOADTLUT", gfx_load_tlut_handler_rdp } },                          // G_LOADTLUT (-16)
    { RDP_G_SETTILESIZE, { "G_SETTILESIZE", gfx_set_tile_size_handler_rdp } },                // G_SETTILESIZE (-14)
    { RDP_G_LOADBLOCK, { "G_LOADBLOCK", gfx_load_block_handler_rdp } },                       // G_LOADBLOCK (-13)
    { RDP_G_LOADTILE, { "G_LOADTILE", gfx_load_tile_handler_rdp } },                          // G_LOADTILE (-12)
    { RDP_G_SETTILE, { "G_SETTILE", gfx_set_tile_handler_rdp } },                             // G_SETTILE (-11)
    { RDP_G_FILLRECT, { "G_FILLRECT", gfx_fill_rect_handler_rdp } },                          // G_FILLRECT (-10)
    { RDP_G_SETFILLCOLOR, { "G_SETFILLCOLOR", gfx_set_fill_color_handler_rdp } },             // G_SETFILLCOLOR (-9)
    { RDP_G_SETFOGCOLOR, { "G_SETFOGCOLOR", gfx_set_fog_color_handler_rdp } },                // G_SETFOGCOLOR (-8)
    { RDP_G_SETBLENDCOLOR, { "G_SETBLENDCOLOR", gfx_set_blend_color_handler_rdp } },          // G_SETBLENDCOLOR (-7)
    { RDP_G_SETPRIMCOLOR, { "G_SETPRIMCOLOR", gfx_set_prim_color_handler_rdp } },             // G_SETPRIMCOLOR (-6)
    { RDP_G_SETENVCOLOR, { "G_SETENVCOLOR", gfx_set_env_color_handler_rdp } },                // G_SETENVCOLOR (-5)
    { RDP_G_SETCOMBINE, { "G_SETCOMBINE", gfx_set_combine_handler_rdp } },                    // G_SETCOMBINE (-4)
    { RDP_G_SETTIMG, { "G_SETTIMG", gfx_set_timg_handler_rdp } },                             // G_SETTIMG (-3)
    { RDP_G_SETZIMG, { "G_SETZIMG", gfx_set_z_img_handler_rdp } },                            // G_SETZIMG (-2)
    { RDP_G_SETCIMG, { "G_SETCIMG", gfx_set_c_img_handler_rdp } },                            // G_SETCIMG (-1)
};

static constexpr UcodeHandler otrHandlers = {
    { OTR_G_SETTIMG_OTR_HASH,
      { "G_SETTIMG_OTR_HASH", gfx_set_timg_otr_hash_handler_custom } },       // G_SETTIMG_OTR_HASH (0x20)
    { OTR_G_SETFB, { "G_SETFB", gfx_set_fb_handler_custom } },                // G_SETFB (0x21)
    { OTR_G_RESETFB, { "G_RESETFB", gfx_reset_fb_handler_custom } },          // G_RESETFB (0x22)
    { OTR_G_SETTIMG_FB, { "G_SETTIMG_FB", gfx_set_timg_fb_handler_custom } }, // G_SETTIMG_FB (0x23)
    { OTR_G_VTX_OTR_FILEPATH,
      { "G_VTX_OTR_FILEPATH", gfx_vtx_otr_filepath_handler_custom } }, // G_VTX_OTR_FILEPATH (0x24)
    { OTR_G_SETTIMG_OTR_FILEPATH,
      { "G_SETTIMG_OTR_FILEPATH", gfx_set_timg_otr_filepath_handler_custom } }, // G_SETTIMG_OTR_FILEPATH (0x25)
    { OTR_G_TRI1_OTR, { "G_TRI1_OTR", gfx_tri1_otr_handler_f3dex2 } },          // G_TRI1_OTR (0x26)
    { OTR_G_DL_OTR_FILEPATH, { "G_DL_OTR_FILEPATH", gfx_dl_otr_filepath_handler_custom } }, // G_DL_OTR_FILEPATH (0x27)
    { OTR_G_PUSHCD, { "G_PUSHCD", gfx_pushcd_handler_custom } },                            // G_PUSHCD (0x28)
    { OTR_G_MTX_OTR_FILEPATH,
      { "G_MTX_OTR_FILEPATH", gfx_mtx_otr_filepath_handler_custom } },          // G_MTX_OTR_FILEPATH (0x29)
    { OTR_G_DL_OTR_HASH, { "G_DL_OTR_HASH", gfx_dl_otr_hash_handler_custom } }, // G_DL_OTR_HASH (0x31)
    { OTR_G_VTX_OTR_HASH, { "G_VTX_OTR_HASH", gfx_vtx_hash_handler_custom } },  // G_VTX_OTR_HASH (0x32)
    { OTR_G_MARKER, { "G_MARKER", gfx_marker_handler_otr } },                   // G_MARKER (0X33)
    { OTR_G_INVALTEXCACHE, { "G_INVALTEXCACHE", gfx_invalidate_tex_cache_handler_f3dex2 } }, // G_INVALTEXCACHE (0X34)
    { OTR_G_BRANCH_Z_OTR, { "G_BRANCH_Z_OTR", gfx_branch_z_otr_handler_f3dex2 } },           // G_BRANCH_Z_OTR (0x35)
    { OTR_G_MTX_OTR, { "G_MTX_OTR", gfx_mtx_otr_handler_custom } },                          // G_MTX_OTR (0x36)
    { OTR_G_TEXRECT_WIDE, { "G_TEXRECT_WIDE", gfx_tex_rect_wide_handler_custom } },          // G_TEXRECT_WIDE (0x37)
    { OTR_G_FILLWIDERECT, { "G_FILLWIDERECT", gfx_fill_wide_rect_handler_custom } },         // G_FILLWIDERECT (0x38)
    { OTR_G_SETGRAYSCALE, { "G_SETGRAYSCALE", gfx_set_grayscale_handler_custom } },          // G_SETGRAYSCALE (0x39)
    { OTR_G_EXTRAGEOMETRYMODE,
      { "G_EXTRAGEOMETRYMODE", gfx_extra_geometry_mode_handler_custom } }, // G_EXTRAGEOMETRYMODE (0x3a)
    { OTR_G_COPYFB, { "G_COPYFB", gfx_copy_fb_handler_custom } },          // G_COPYFB (0x3b)
    { OTR_G_IMAGERECT, { "G_IMAGERECT", gfx_image_rect_handler_custom } }, // G_IMAGERECT (0x3c)
    { OTR_G_DL_INDEX, { "G_DL_INDEX", gfx_dl_index_handler } },            // G_DL_INDEX (0x3d)
    { OTR_G_READFB, { "G_READFB", gfx_read_fb_handler_custom } },          // G_READFB (0x3e)
    { OTR_G_REGBLENDEDTEX,
      { "G_REGBLENDEDTEX", gfx_register_blended_texture_handler_custom } },         // G_REGBLENDEDTEX (0x3f)
    { OTR_G_SETINTENSITY, { "G_SETINTENSITY", gfx_set_intensity_handler_custom } }, // G_SETINTENSITY (0x40)
    { OTR_G_MOVEMEM_HASH, { "OTR_G_MOVEMEM_HASH", gfx_movemem_handler_otr } },      // OTR_G_MOVEMEM_HASH
    { OTR_G_PUSH_SHADER, { "G_PUSH_SHADER", gfx_push_shader } },
    { OTR_G_SETBLENDMODE_EX, { "G_SETBLENDMODE_EX", gfx_set_blend_mode_ex_handler_custom } },
    { OTR_G_SKIN_PALETTE_OTR_HASH, { "G_SKIN_PALETTE_OTR_HASH", gfx_skin_palette_handler_custom } },
    { OTR_G_VTX_EX_OTR_HASH, { "G_VTX_EX_OTR_HASH", gfx_vtx_ex_handler_custom } },
    { OTR_G_PUSH_SHADER_OTR_HASH, { "G_PUSH_SHADER_OTR_HASH", gfx_push_shader_hash_handler_custom } },
    { OTR_G_REGS_EX, { "G_REGS_EX", gfx_regs_ex_handler_custom } },
    { OTR_G_POP_SHADER, { "G_POP_SHADER", gfx_pop_shader } },
    { RDP_G_LOADBLOCK_WIDE, { "G_LOADBLOCK_WIDE", gfx_load_block_wide_handler_rdp } }, // RDP_G_LOADBLOCK_WIDE (-15)
    { RDP_G_VTX_WIDE, { "G_VTX_WIDE", gfx_vtx_handler_f3dex2 } },                      // RDP_G_VTX_WIDE (-16)
    { RDP_G_TRI1_WIDE, { "G_TRI1_WIDE", gfx_tri1_handler_f3dex2 } },                   // RDP_G_TRI1_WIDE (-17)
};

static constexpr UcodeHandler f3dex2Handlers = {
    { F3DEX2_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX2_G_SPNOOP, { "G_SPNOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX2_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX2_G_MTX, { "G_MTX", gfx_mtx_handler_f3dex2 } },
    { F3DEX2_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3dex2 } },
    { F3DEX2_G_MOVEMEM, { "G_MOVEMEM", gfx_movemem_handler_f3dex2 } },
    { F3DEX2_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3dex2 } },
    { F3DEX2_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3dex2 } },
    { F3DEX2_G_VTX, { "G_VTX", gfx_vtx_handler_f3dex2 } },
    { F3DEX2_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX2_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX2_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX2_G_GEOMETRYMODE, { "G_GEOMETRYMODE", gfx_geometry_mode_handler_f3dex2 } },
    { F3DEX2_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3dex2 } },
    { F3DEX2_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX2_G_QUAD, { "G_QUAD", gfx_quad_handler_f3dex2 } },
    { F3DEX2_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3dex2 } },
    { F3DEX2_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3dex2 } },
};

static constexpr UcodeHandler f3dexHandlers = {
    { F3DEX_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX_G_MTX, { "G_MTX", gfx_mtx_handler_f3d } },
    { F3DEX_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3d } },
    { F3DEX_G_MOVEMEM, { "G_POPMEM", gfx_movemem_handler_f3d } },
    { F3DEX_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3d } },
    { F3DEX_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3d } },
    { F3DEX_G_SETGEOMETRYMODE, { "G_SETGEOMETRYMODE", gfx_set_geometry_mode_handler_f3dex } },
    { F3DEX_G_CLEARGEOMETRYMODE, { "G_CLEARGEOMETRYMODE", gfx_clear_geometry_mode_handler_f3dex } },
    { F3DEX_G_VTX, { "G_VTX", gfx_vtx_handler_f3dex } },
    { F3DEX_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3dex } },
    { F3DEX_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX_G_SPNOOP, { "G_SPNOOP", gfx_spnoop_command_handler_f3dex2 } },
    { F3DEX_G_RDPHALF_1, { "mRdpHALF_1", gfx_stubbed_command_handler } },
    { F3DEX_G_QUAD, { "G_QUAD", gfx_quad_handler_f3dex } },
};

static constexpr UcodeHandler f3dHandlers = {
    { F3DEX_G_NOOP, { "G_NOOP", gfx_noop_handler_f3dex2 } },
    { F3DEX_G_CULLDL, { "G_CULLDL", gfx_cull_dl_handler_f3dex2 } },
    { F3DEX_G_MTX, { "G_MTX", gfx_mtx_handler_f3d } },
    { F3DEX_G_POPMTX, { "G_POPMTX", gfx_pop_mtx_handler_f3d } },
    { F3DEX_G_MOVEMEM, { "G_POPMEM", gfx_movemem_handler_f3d } },
    { F3DEX_G_MOVEWORD, { "G_MOVEWORD", gfx_moveword_handler_f3d } },
    { F3DEX_G_TEXTURE, { "G_TEXTURE", gfx_texture_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_L, { "G_SETOTHERMODE_L", gfx_othermode_l_handler_f3d } },
    { F3DEX_G_SETOTHERMODE_H, { "G_SETOTHERMODE_H", gfx_othermode_h_handler_f3d } },
    { F3DEX_G_SETGEOMETRYMODE, { "G_SETGEOMETRYMODE", gfx_set_geometry_mode_handler_f3dex } },
    { F3DEX_G_CLEARGEOMETRYMODE, { "G_CLEARGEOMETRYMODE", gfx_clear_geometry_mode_handler_f3dex } },
    { F3DEX_G_VTX, { "G_VTX", gfx_vtx_handler_f3d } },
    { F3DEX_G_TRI1, { "G_TRI1", gfx_tri1_handler_f3d } },
    { F3DEX_G_MODIFYVTX, { "G_MODIFYVTX", gfx_modify_vtx_handler_f3dex2 } },
    { F3DEX_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
    { F3DEX_G_TRI2, { "G_TRI2", gfx_tri2_handler_f3dex } },
    { F3DEX_G_SPNOOP, { "G_SPNOOP", gfx_spnoop_command_handler_f3dex2 } },
    { F3DEX_G_RDPHALF_1, { "mRdpHALF_1", gfx_stubbed_command_handler } },
};

// LUSTODO: These S2DEX commands have different opcode numbers on F3DEX2 vs other ucodes. More research needs to be done
// to see if the implementations are different.
static constexpr UcodeHandler s2dexHandlers = {
    { F3DEX2_G_BG_COPY, { "G_BG_COPY", gfx_bg_copy_handler_s2dex } },
    { F3DEX2_G_BG_1CYC, { "G_BG_1CYC", gfx_bg_1cyc_handler_s2dex } },
    { F3DEX2_G_OBJ_RENDERMODE, { "G_OBJ_RENDERMODE", gfx_stubbed_command_handler } },
    { F3DEX2_G_OBJ_RECTANGLE_R, { "G_OBJ_RECTANGLE_R", gfx_stubbed_command_handler } },
    { F3DEX2_G_OBJ_RECTANGLE, { "G_OBJ_RECTANGLE", gfx_obj_rectangle_handler_s2dex } },
    { F3DEX2_G_DL, { "G_DL", gfx_dl_handler_common } },
    { F3DEX2_G_ENDDL, { "G_ENDDL", gfx_end_dl_handler_common } },
};

static constexpr std::array ucode_handlers = {
    &f3dHandlers,    // ucode_f3db
    &f3dHandlers,    // ucode_f3d
    &f3dexHandlers,  // ucode_f3dex
    &f3dexHandlers,  // ucode_f3dexb
    &f3dex2Handlers, // ucode_f3dex2
    &s2dexHandlers,  // ucode_s2dex
};

const char* GfxGetOpcodeName(int8_t opcode) {
    if (otrHandlers.contains(opcode)) {
        return otrHandlers.at(opcode).first;
    }

    if (rdpHandlers.contains(opcode)) {
        return rdpHandlers.at(opcode).first;
    }

    if (ucode_handler_index < ucode_handlers.size()) {
        if (ucode_handlers[ucode_handler_index]->contains(opcode)) {
            return ucode_handlers[ucode_handler_index]->at(opcode).first;
        } else {
            SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, for loaded ucode: {}", (uint8_t)opcode,
                            (uint32_t)ucode_handler_index);
        }
    } else {
        SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, invalid ucode: {}", (uint8_t)opcode, (uint32_t)ucode_handler_index);
    }

    return nullptr;
}

// TODO, implement a system where we can get the current opcode handler by writing to the GWords. If the powers that be
// are OK with that...
static void gfx_set_ucode_handler(UcodeHandlers ucode) {
    // Loaded ucode must be in range of the supported ucode_handlers
    assert(ucode < ucode_max);
    Interpreter* gfx = mInstance.lock().get();
    ucode_handler_index = ucode;

    // Reset some RSP state values upon ucode load to deal with hardware quirks discovered by emulators
    switch (ucode) {
        case ucode_f3d:
        case ucode_f3db:
        case ucode_f3dex:
        case ucode_f3dexb:
        case ucode_f3dex2:
            gfx->mRsp->fog_mul = 0;
            gfx->mRsp->fog_offset = 0;
            break;
        default:
            break;
    }
}

static void gfx_step_inner();

// (a measuring aid, "gMergeShip.ShadowTiming": the processor's time by command, both passes of a frame together)
static uint64_t sOpTicks[256];
static uint32_t sOpCount[256];

static void gfx_step() {
    if (!sShadowTiming) {
        gfx_step_inner();
        return;
    }
    const uint8_t op = (uint8_t)(g_exec_stack.currCmd()->words.w0 >> 24);
    const uint64_t before = __rdtsc();
    gfx_step_inner();
    sOpTicks[op] += __rdtsc() - before;
    sOpCount[op]++;
}

static void gfx_step_inner() {
    auto& cmd = g_exec_stack.currCmd();
    auto cmd0 = cmd;
    int8_t opcode = (int8_t)(cmd->words.w0 >> 24);

#ifdef USE_GBI_TRACE
    if (cmd->words.trace.valid &&
        Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger("gEnableGFXTrace", 0)) {
#define TRACE                                  \
    "\n====================================\n" \
    " - CMD: {:02X}\n"                         \
    " - Path: {}:{}\n"                         \
    " - W0: {:08X}\n"                          \
    " - W1: {:08X}\n"                          \
    "===================================="
        SPDLOG_INFO(TRACE, (uint8_t)opcode, cmd->words.trace.file, cmd->words.trace.idx, cmd->words.w0, cmd->words.w1);
    }
#endif

    if (opcode == F3DEX2_G_LOAD_UCODE) {
        gfx_set_ucode_handler((UcodeHandlers)(cmd->words.w0 & 0xFFFFFF));
        ++cmd;
        return;
        // Instead of having a handler for each ucode for switching ucode, just check for it early and return.
    }

    if (otrHandlers.contains(opcode)) {
        // OTR filepath handlers expect w1 to be a valid string pointer.
        // Guard against null or N64-segment addresses that would crash in strlen/strncmp.
        if (opcode == OTR_G_VTX_OTR_FILEPATH || opcode == OTR_G_SETTIMG_OTR_FILEPATH ||
            opcode == OTR_G_DL_OTR_FILEPATH || opcode == OTR_G_PUSHCD || opcode == OTR_G_MTX_OTR_FILEPATH) {
            uintptr_t w1 = (uintptr_t)cmd->words.w1;
            if (w1 < 0x10000
#if UINTPTR_MAX > 0xFFFFFFFFu
                // On 64-bit: filter kernel/sentinel addresses.
                || w1 > 0x0000FFFFFFFFFFFFull
#endif
            ) {
                ++g_exec_stack.currCmd();
                return;
            }
        }
        if (otrHandlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (rdpHandlers.contains(opcode)) {
        if (rdpHandlers.at(opcode).second(&cmd)) {
            return;
        }
    } else if (ucode_handler_index < ucode_handlers.size()) {
        if (ucode_handlers[ucode_handler_index]->contains(opcode)) {
            if (ucode_handlers[ucode_handler_index]->at(opcode).second(&cmd)) {
                return;
            }
        } else {
            SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, for loaded ucode: {}", (uint8_t)opcode,
                            (uint32_t)ucode_handler_index);
        }
    } else {
        SPDLOG_CRITICAL("Unhandled OP code: 0x{:X}, invalid ucode: {}", (uint8_t)opcode, (uint32_t)ucode_handler_index);
    }

    ++cmd;
}

// MergeShip2 real-time shadows. The frame's display list is run twice. First as the light sees the world
// (RunShadowPass): every vertex of the world goes through the light's view in place of the camera's, and only
// what covers what is behind it is drawn, into a framebuffer of its own whose depth is then the shadow map. Then
// as always; and where the game put G_REGS_EX 8, behind the world's opaque surfaces and before anything else, the
// backend darkens what is on the framebuffer by that map (GfxRenderingAPI::ApplyShadows): one pass over the
// screen, so no material's shader and no model has to know of it. The light is the game's to say (G_REGS_EX 9).
// A frame runs the light's pass only if the frame before asked for shadows, so a game or a screen that draws no
// world costs nothing.
namespace {
bool Invert4(const float m[4][4], float out[4][4]) {
    double a[4][8];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            a[i][j] = m[i][j];
            a[i][j + 4] = i == j ? 1.0 : 0.0;
        }
    }
    for (int c = 0; c < 4; c++) {
        int pivot = c;
        for (int r = c + 1; r < 4; r++) {
            if (fabs(a[r][c]) > fabs(a[pivot][c])) {
                pivot = r;
            }
        }
        if (fabs(a[pivot][c]) < 1e-12) {
            return false;
        }
        if (pivot != c) {
            for (int j = 0; j < 8; j++) {
                std::swap(a[c][j], a[pivot][j]);
            }
        }
        const double d = a[c][c];
        for (int j = 0; j < 8; j++) {
            a[c][j] /= d;
        }
        for (int r = 0; r < 4; r++) {
            if (r != c) {
                const double f = a[r][c];
                for (int j = 0; j < 8; j++) {
                    a[r][j] -= f * a[c][j];
                }
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i][j] = (float)a[i][j + 4];
        }
    }
    return true;
}
} // namespace

// The light's view from what the game gave: straight along the light, a square of twice the radius about the
// focus, and four radii deep (two towards the light from the focus, two beyond). The focus is moved onto the
// map's own grid, so that the shadows' edges do not crawl as it moves.
bool Interpreter::ShadowLightFromGame() {
    const ShadowLightEx* light = sShadowLightEx;
    if (light == nullptr || !light->on || !(light->radius > 1.0f)) {
        return false;
    }
    float to[3] = { light->dir[0], light->dir[1], light->dir[2] };
    const float length = sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
    if (!(length > 1e-6f)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        to[i] /= length;
    }
    const float up[3] = { 0.0f, fabsf(to[1]) > 0.99f ? 0.0f : 1.0f, fabsf(to[1]) > 0.99f ? 1.0f : 0.0f };
    float right[3] = { up[1] * to[2] - up[2] * to[1], up[2] * to[0] - up[0] * to[2], up[0] * to[1] - up[1] * to[0] };
    const float rightLength = sqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    for (int i = 0; i < 3; i++) {
        right[i] /= rightLength;
    }
    const float above[3] = { to[1] * right[2] - to[2] * right[1], to[2] * right[0] - to[0] * right[2],
                             to[0] * right[1] - to[1] * right[0] };
    const float radius = light->radius;
    const float texel = 2.0f * radius / (float)sShadowMapSize;
    float focus[3] = { light->focus[0], light->focus[1], light->focus[2] };
    const float alongRight = focus[0] * right[0] + focus[1] * right[1] + focus[2] * right[2];
    const float alongAbove = focus[0] * above[0] + focus[1] * above[1] + focus[2] * above[2];
    const float moveRight = roundf(alongRight / texel) * texel - alongRight;
    const float moveAbove = roundf(alongAbove / texel) * texel - alongAbove;
    float eye[3];
    for (int i = 0; i < 3; i++) {
        focus[i] += right[i] * moveRight + above[i] * moveAbove;
        eye[i] = focus[i] + to[i] * 2.0f * radius;
    }
    const float eyeRight = eye[0] * right[0] + eye[1] * right[1] + eye[2] * right[2];
    const float eyeAbove = eye[0] * above[0] + eye[1] * above[1] + eye[2] * above[2];
    const float eyeTo = eye[0] * to[0] + eye[1] * to[1] + eye[2] * to[2];
    memset(mShadowLight, 0, sizeof(mShadowLight));
    for (int i = 0; i < 3; i++) {
        mShadowLight[i][0] = right[i] / radius;
        mShadowLight[i][1] = above[i] / radius;
        mShadowLight[i][2] = -to[i] / (2.0f * radius);
        mShadowDir[i] = to[i];
    }
    mShadowLight[3][0] = -eyeRight / radius;
    mShadowLight[3][1] = -eyeAbove / radius;
    mShadowLight[3][2] = eyeTo / (2.0f * radius) - 1.0f;
    mShadowLight[3][3] = 1.0f;
    mShadowParams[0] = light->shade < 0.0f ? 0.0f : (light->shade > 1.0f ? 1.0f : light->shade);
    mShadowParams[1] = light->away < 0.0f ? 0.0f : (light->away > 1.0f ? 1.0f : light->away);
    mShadowParams[2] = 2.5f * texel / (4.0f * radius); // (two and a half texels of the map, as depth)
    mShadowParams[3] = 0.0f;
    sShadowBothSides = light->bothSides != 0;
    sShadowCasts = light->casts != 0;
    sShadowAir[0] = light->ao > 0.0f ? (light->ao < 1.0f ? light->ao : 1.0f) : 0.0f;
    sShadowAir[1] = light->aoReach > 1.0f ? light->aoReach : 30.0f;
    sShadowAir[2] = light->clouds > 0.0f ? (light->clouds < 1.0f ? light->clouds : 1.0f) : 0.0f;
    sShadowAir[3] = 640.0f; // (a cloud's size, in the world's units)
    for (int i = 0; i < 3; i++) {
        sShadowSun[i] = light->sun[i] >= 1.0f && light->sun[i] < 1.6f ? light->sun[i] : 1.0f;
    }
    sShadowAir[4] = light->tint[0];
    sShadowAir[5] = light->tint[1];
    sShadowAir[6] = light->tint[2];
    sShadowAir[7] = 42.0f; // (the clouds' speed, units a second)
    sShadowClip = false;
    // the soft edges: a texel and the light's depth in the world's units; an edge 0.07 wide per unit between what
    // casts the shadow and what it falls on (a crown 150 up: ten units), and never wider than nine and a half
    sShadowSoft[0] = texel;
    sShadowSoft[1] = light->fromPlane && light->deep > 1.0f && to[1] > 0.2f ? light->deep : 4.0f * radius;
    sShadowSoft[2] = 0.07f;
    sShadowSoft[3] = std::clamp(9.5f / texel, 2.0f, 16.0f);
    // (a light from a level plane: its shadows fade out towards the plane's height over the ground)
    sShadowSoft[4] = light->fromPlane && light->reach > 1.0f ? light->reach * 0.55f : 0.0f;
    sShadowSoft[5] = light->fromPlane && light->reach > 1.0f ? light->reach * 0.95f : 0.0f;
    if (light->fromPlane && light->deep > 1.0f && to[1] > 0.2f) {
        // The light's depth is the height under its plane, in place of the way along the light (either orders what
        // lies along one ray of the light the same way): the plane is then the map's near side, and the backend
        // cuts what is above it away there.
        mShadowLight[0][2] = 0.0f;
        mShadowLight[1][2] = -2.0f / light->deep;
        mShadowLight[2][2] = 0.0f;
        mShadowLight[3][2] = 2.0f * light->top / light->deep - 1.0f;
        mShadowParams[2] = 2.5f * texel / light->deep;
        sShadowClip = true;
    }
    return true;
}

// (in the light's pass) the matrix the vertices go through: the light's view where the game's projection is a view
// of the world, one with perspective; everything else (the screen's own flat projections) is left out.
void Interpreter::ShadowMatrix() {
    const float(*p)[4] = mRsp->P_matrix;
    mShadowWorld = fabsf(p[0][3]) + fabsf(p[1][3]) + fabsf(p[2][3]) > 1e-6f && mRsp->modelview_matrix_stack_size > 0;
    if (mShadowWorld) {
        MatrixMul(mRsp->MP_matrix, mRsp->modelview_matrix_stack[mRsp->modelview_matrix_stack_size - 1], mShadowLight);
    }
}

// (the backend's shadow map is what is drawn into: Run asked for it, GfxRenderingAPI::StartShadowMap)
void Interpreter::RunShadowPass(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    SpReset();
    mCurMtxReplacements = &mtx_replacements;
    mShadowPass = true;
    mShadowWorld = false;
    g_exec_stack.start((F3DGfx*)commands);
    while (!g_exec_stack.cmd_stack.empty()) {
        gfx_step();
    }
    Flush();
    mShadowPass = false;
    mShadowWorld = false;
    mFbActive = false;
    mActiveFrameBuffer = mFrameBuffers.end();
    while (!mShaderStack.empty()) {
        mShaderStack.pop(); // (should a list have left one standing)
    }
    currentDir = std::stack<std::string>();
}

// G_REGS_EX 8: what is on the framebuffer is darkened by this frame's shadow map; and the next frame is to have one.
void Interpreter::ApplyShadows() {
    mShadowArmed = true;
    if (!sShadowLit || mFbActive) {
        return;
    }
    // the camera as the vertices went through it: the projection in effect (the world's view: the game puts the
    // command behind it), with the window's shape taken into its x as GfxSpVertex takes it
    float camera[4][4];
    memcpy(camera, mRsp->P_matrix, sizeof(camera));
    const float adjust = AdjXForAspectRatio(1.0f);
    for (int i = 0; i < 4; i++) {
        camera[i][0] *= adjust;
    }
    float inverse[4][4];
    if (fabsf(camera[0][3]) + fabsf(camera[1][3]) + fabsf(camera[2][3]) <= 1e-6f || !Invert4(camera, inverse)) {
        return; // (not a view of the world)
    }
    Flush();
    const int viewport[4] = { (int)mRdp->viewport.x, (int)mRdp->viewport.y, (int)mRdp->viewport.width,
                              (int)mRdp->viewport.height };
    // (without the light's pass this frame nothing casts, and nothing is shaded for facing away either)
    float params[124] = { mShadowParams[0], mShadowDrawn ? mShadowParams[1] : 1.0f,
                          mShadowParams[2], mShadowParams[3],
                          sShadowSoft[0],   sShadowSoft[1],
                          sShadowSoft[2],   sShadowSoft[3],
                          sShadowSoft[4],   sShadowSoft[5],
                          mShadowDrawn ? 1.0f : 0.0f, sSwayTime,
                          sShadowAir[0],    sShadowAir[1],
                          sShadowAir[2],    sShadowAir[3],
                          sShadowAir[4],    sShadowAir[5],
                          sShadowAir[6],    sShadowAir[7],
                          mShadowDrawn ? sShadowSun[0] : 1.0f, mShadowDrawn ? sShadowSun[1] : 1.0f,
                          mShadowDrawn ? sShadowSun[2] : 1.0f, 0.0f };
    // (the game's lamps, as its light says them this frame: the rest of the array, zero where there are none)
    if (const ShadowLightEx* light = sShadowLightEx; light != nullptr && light->lampCount > 0) {
        const int lamps = light->lampCount < 12 ? light->lampCount : 12;
        params[24] = (float)lamps;
        params[25] = light->lampStrength;
        memcpy(&params[28], light->lamps, sizeof(float) * 8 * (size_t)lamps);
    }
    mRapi->ApplyShadows(0, inverse, mShadowLight, mShadowDir, params, viewport);
    // (the backend's viewport is the whole framebuffer now: the game's is set again with the next triangle)
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};
}

// MergeShip2 the picture's look. G_REGS_EX 13: the world's camera is the projection in effect (the game puts the
// command at the head of what it draws of the world); it is kept for G_REGS_EX 12.
void Interpreter::KeepWorldCamera() {
    float camera[4][4];
    memcpy(camera, mRsp->P_matrix, sizeof(camera));
    const float adjust = AdjXForAspectRatio(1.0f);
    for (int i = 0; i < 4; i++) {
        camera[i][0] *= adjust;
    }
    mWorldKept = false;
    if (mFbActive || fabsf(camera[0][3]) + fabsf(camera[1][3]) + fabsf(camera[2][3]) <= 1e-6f ||
        !Invert4(camera, mWorldInverse)) {
        return; // (not a view of the world)
    }
    mWorldViewport[0] = (int)mRdp->viewport.x;
    mWorldViewport[1] = (int)mRdp->viewport.y;
    mWorldViewport[2] = (int)mRdp->viewport.width;
    mWorldViewport[3] = (int)mRdp->viewport.height;
    mWorldKept = true;
}

// G_REGS_EX 12: what has been drawn so far -- the world; the game puts the command before its fades, its HUD and
// its text -- is given its glow and its look (GfxRenderingAPI::ApplyPost).
void Interpreter::ApplyPost(const float* params) {
    if (!mWorldKept || mFbActive) {
        return;
    }
    Flush();
    mRapi->ApplyPost(mWorldInverse, params, mWorldViewport);
    // (the backend's viewport is the whole framebuffer now: the game's is set again with the next triangle)
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};
}

void Interpreter::SpReset() {
    mWorldKept = false; // (MergeShip2 the picture's look: this frame's camera is not kept yet)
    sSway = nullptr; // (MergeShip2 swaying grass: none until a list asks)
    sShadowNoCast = false;
    while (!mShaderStack.empty()) {
        mShaderStack.pop();
    }
    // MergeShip2: no explicit blending or vertex-alpha / second-UV modes carried over from the last frame
    mRdp->blend_ex_mode = 0;
    mRsp->extra_geometry_mode &= ~(G_EX_KEEP_VERTEX_ALPHA | G_EX_TEXEL1_UPPER_VERTEX);
    sSkinPaletteCount = 0;
    mRsp->modelview_matrix_stack_size = 1;
    mRsp->current_num_lights = 2;
    mRsp->lights_changed = true;
    mRsp->lookat[0].dir[0] = 0;
    mRsp->lookat[0].dir[1] = 127;
    mRsp->lookat[0].dir[2] = 0;
    mRsp->lookat[1].dir[0] = 127;
    mRsp->lookat[1].dir[1] = 0;
    mRsp->lookat[1].dir[2] = 0;
    CalculateNormalDir(&mRsp->lookat[0], mRsp->current_lookat_coeffs[0]);
    CalculateNormalDir(&mRsp->lookat[1], mRsp->current_lookat_coeffs[1]);
}

void Interpreter::RegisterFbTexture(const void* cpuAddr, int fbId) {
    mFbTextures[(uintptr_t)cpuAddr] = fbId;
}

void Interpreter::UnregisterFbTexture(const void* cpuAddr) {
    mFbTextures.erase((uintptr_t)cpuAddr);
}

void Interpreter::GetDimensions(uint32_t* width, uint32_t* height, int32_t* posX, int32_t* posY) {
    mWapi->GetDimensions(width, height, posX, posY);
}

// MergeShip2 render styles: the multisampling level the settings ask for. The 16-bit and 8-bit styles draw without
// (hard edges), so mMsaaLevel is this or 1, decided at the start of each frame.
static unsigned int sUserMsaaLevel = 1;

void Interpreter::Init(class GfxWindowBackend* wapi, class GfxRenderingAPI* rapi, const char* game_name,
                       bool start_in_fullscreen, uint32_t width, uint32_t height, uint32_t posX, uint32_t posY) {
    mWapi = wapi;
    mRapi = rapi;
    mWapi->Init(game_name, rapi->GetName(), start_in_fullscreen, width, height, posX, posY);
    mRapi->Init();
    mRapi->UpdateFramebufferParameters(0, width, height, 1, false, true, true, true);
    mCurDimensions.internal_mul =
        Ship::Context::GetRawInstance()->GetConsoleVariables()->GetFloat(CVAR_INTERNAL_RESOLUTION, 1);
    mMsaaLevel = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger(CVAR_MSAA_VALUE, 1);
    sUserMsaaLevel = mMsaaLevel;

    mCurDimensions.width = width;
    mCurDimensions.height = height;

    mGameFb = mRapi->CreateFramebuffer();
    mGameFbMsaaResolved = mRapi->CreateFramebuffer();

    mNativeDimensions.width = SCREEN_WIDTH;
    mNativeDimensions.height = SCREEN_HEIGHT;

    for (int i = 0; i < MAX_SEGMENT_POINTERS; i++) {
        mSegmentPointers[i] = 0;
    }

    if (mTexUploadBuffer == nullptr) {
        // We cap texture max to 8k, because why would you need more?
        int max_tex_size = std::min(8192, mRapi->GetMaxTextureSize());
        mTexUploadBuffer = (uint8_t*)malloc(max_tex_size * max_tex_size * 4);
    }

    ucode_handler_index = UcodeHandlers::ucode_f3dex2;

    // Pre-allocate texture cache buckets to prevent rehash-induced iterator invalidation.
    mTextureCache.map.reserve(TEXTURE_CACHE_MAX_SIZE);
}

void Interpreter::Destroy() {
    // TODO: should also destroy rapi, and any other resources acquired in fast3d
    free(mTexUploadBuffer);
    mWapi->Destroy();

    // Texture cache and loaded textures store references to Resources which need to be unreferenced.
    TextureCacheClear();
    mRdp->texture_to_load.raw_tex_metadata.resource = nullptr;
    mRdp->loaded_texture[0].raw_tex_metadata.resource = nullptr;
    mRdp->loaded_texture[1].raw_tex_metadata.resource = nullptr;
}

GfxRenderingAPI* Interpreter::GetCurrentRenderingAPI() {
    return mRapi;
}

void Interpreter::SetGfxDebugger(std::shared_ptr<GfxDebugger> debugger) {
    mGfxDebugger = std::move(debugger);
}

std::shared_ptr<GfxDebugger> Interpreter::GetGfxDebugger() const {
    return mGfxDebugger;
}

void Interpreter::HandleWindowEvents() {
    mWapi->HandleEvents();
}

bool Interpreter::IsFrameReady() {
    return mWapi->IsFrameReady();
}

bool Interpreter::ViewportMatchesRendererResolution() {
#ifdef __APPLE__
    // Always treat the viewport as not matching the render resolution on mac
    // to avoid issues with retina scaling.
    return false;
#else
    if (mCurDimensions.width == mGameWindowViewport.width && mCurDimensions.height == mGameWindowViewport.height) {
        return true;
    }
    return false;
#endif
}

void Interpreter::StartFrame() {
    mWapi->GetDimensions(&mGfxCurrentWindowDimensions.width, &mGfxCurrentWindowDimensions.height, &mCurWindowPosX,
                         &mCurWindowPosY);
    mMsaaLevel = PixelStyle(mRenderStyle) ? 1 : sUserMsaaLevel; // MergeShip2 (see sUserMsaaLevel)
    if (mCurDimensions.height == 0) {
        // Avoid division by zero
        mCurDimensions.height = 1;
    }
    mCurDimensions.aspect_ratio = (float)mCurDimensions.width / (float)mCurDimensions.height;

    // Update the framebuffer sizes when the viewport or native dimension changes
    if (mCurDimensions.width != mPrvDimensions.width || mCurDimensions.height != mPrvDimensions.height ||
        mNativeDimensions.width != mPrevNativeDimensions.width ||
        mNativeDimensions.height != mPrevNativeDimensions.height) {

        for (auto& fb : mFrameBuffers) {
            uint32_t width = fb.second.orig_width, height = fb.second.orig_height;
            if (fb.second.resize) {
                AdjustWidthHeightForScale(width, height, fb.second.native_width, fb.second.native_height);
            }
            if (width != fb.second.applied_width || height != fb.second.applied_height) {
                mRapi->UpdateFramebufferParameters(fb.first, width, height, 1, true, true, true, true);
                fb.second.applied_width = width;
                fb.second.applied_height = height;
            }
        }
    }

    mPrvDimensions = mCurDimensions;
    mPrevNativeDimensions = mNativeDimensions;
    if (!ViewportMatchesRendererResolution() || mMsaaLevel > 1) {
        mRendersToFb = true;
        if (!ViewportMatchesRendererResolution()) {
            mRapi->UpdateFramebufferParameters(mGameFb, mCurDimensions.width, mCurDimensions.height, mMsaaLevel, true,
                                               true, true, true);
        } else {
            // MSAA framebuffer needs to be resolved to an equally sized target when complete, which must therefore
            // match the window size
            mRapi->UpdateFramebufferParameters(mGameFb, mGfxCurrentWindowDimensions.width,
                                               mGfxCurrentWindowDimensions.height, mMsaaLevel, false, true, true, true);
        }
        if (mMsaaLevel > 1 && !ViewportMatchesRendererResolution()) {
            mRapi->UpdateFramebufferParameters(mGameFbMsaaResolved, mCurDimensions.width, mCurDimensions.height, 1,
                                               false, false, false, false);
        }
    } else {
        mRendersToFb = false;
    }

    mFbActive = false;
}

GfxExecStack g_exec_stack = {};

void Interpreter::RunGuiOnly() {
    SpReset();

    mGetPixelDepthPending.clear();
    mGetPixelDepthCached.clear();

    mRapi->UpdateFramebufferParameters(0, mGfxCurrentWindowDimensions.width, mGfxCurrentWindowDimensions.height, 1,
                                       false, true, true, !mRendersToFb);
    mRapi->StartFrame();
    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, (float)mCurDimensions.height / mNativeDimensions.height);
    mRapi->ClearFramebuffer(true, true);
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};

    Flush();
    mGfxFrameBuffer = 0;

    if (mRendersToFb) {
        mRapi->StartDrawToFramebuffer(0, 1);
        mRapi->ClearFramebuffer(true, true);
        if (mMsaaLevel > 1) {
            if (!ViewportMatchesRendererResolution()) {
                mRapi->ResolveMSAAColorBuffer(mGameFbMsaaResolved, mGameFb);
                mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFbMsaaResolved);
            } else {
                mRapi->ResolveMSAAColorBuffer(0, mGameFb);
            }
        } else {
            mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFb);
        }
    } else if (mFbActive) {
        // Failsafe reset to main framebuffer to prevent softlocking the renderer
        mFbActive = 0;
        mRapi->StartDrawToFramebuffer(0, 1);

        assert(0 && "active framebuffer was never reset back to original");
    }
}

void Interpreter::Run(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtx_replacements) {
    SpReset();

    // MergeShip2: this frame's render style (RenderStyle in interpreter.h). A game whose own settings do not have the
    // variable (Termina inside the MergeShip host: the option is in Hyrule's menu) is drawn in the style last read.
    {
        static int sLastStyle = RENDER_STYLE_NONE;
        const int style =
            Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger("gMergeShip.RenderStyle", -1);
        if (style >= 0) {
            sLastStyle = style;
        }
        // (the colour filters after RENDER_STYLE_8BIT are not offered any more)
        mRenderStyle = (sLastStyle > 0 && sLastStyle <= RENDER_STYLE_8BIT) ? (uint32_t)sLastStyle : RENDER_STYLE_NONE;
        // (and the 3DS icons, the same way)
        static int sLastInterface = 0;
        const int interfaceOption = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger(
            "gEnhancements.MergeShip.Port3ds.Interface", -1);
        if (interfaceOption >= 0) {
            sLastInterface = interfaceOption;
        }
        sInterfaceRedirect = sLastInterface != 0;
        // (and the sharper textures)
        static int sLastHd = 0;
        const int hdOption = Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger(
            "gEnhancements.MergeShip.Port3ds.HdTextures", -1);
        if (hdOption >= 0) {
            sLastHd = hdOption;
        }
        sHdTextures = sLastHd != 0;
        // (and the shadows' detail: the map's size)
        static int sLastDetail = 2;
        const int detail =
            Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger("gMergeShip.ShadowDetail", -1);
        if (detail >= 0) {
            sLastDetail = detail;
        }
        sShadowMapSize = sLastDetail <= 0 ? 1024 : (sLastDetail == 1 ? 2048 : 4096);
        sShadowTiming =
            Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger("gMergeShip.ShadowTiming", 0) != 0;
    }
    const auto runStart = std::chrono::steady_clock::now();
    double lightMs = 0.0;
    {
        // (swaying grass: the wind's clock, the same for the light's pass and the frame's own. The real clock, so
        // that the wind keeps its pace whatever the frame rate; "gMergeShip.WindStepMs" makes it a fixed step a
        // drawn frame instead, for a recording that is made slower or faster than it is played.)
        static auto sWindLast = runStart;
        static double sWindSeconds = 0.0;
        const int step =
            Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger("gMergeShip.WindStepMs", 0);
        const double passed = std::chrono::duration<double>(runStart - sWindLast).count();
        sWindLast = runStart;
        sWindSeconds += step > 0 ? step / 1000.0 : (passed < 0.25 ? passed : 0.25);
        sSwayTime = (float)fmod(sWindSeconds, 7200.0);
    }

    mGetPixelDepthPending.clear();
    mGetPixelDepthCached.clear();

    mCurMtxReplacements = &mtx_replacements;

    mRapi->UpdateFramebufferParameters(0, mGfxCurrentWindowDimensions.width, mGfxCurrentWindowDimensions.height, 1,
                                       false, true, true, !mRendersToFb);
    mRapi->StartFrame();
    // MergeShip2 real-time shadows: the world as the light sees it, first (where the last frame asked for shadows)
    mShadowDrawn = false;
    sShadowLit = mShadowArmed && ShadowLightFromGame();
    if (sShadowLit && sShadowCasts && mRapi->StartShadowMap(sShadowMapSize, sShadowClip)) {
        RunShadowPass(commands, mtx_replacements);
        mShadowDrawn = true;
        SpReset();
        mCurMtxReplacements = &mtx_replacements;
        lightMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - runStart).count();
    }
    mShadowArmed = false;
    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, (float)mCurDimensions.height / mNativeDimensions.height);
    mRapi->ClearFramebuffer(true, true);
    mRdp->viewport_or_scissor_changed = true;
    mRenderingState.viewport = {};
    mRenderingState.scissor = {};

    auto dbg = mGfxDebugger;
    g_exec_stack.start((F3DGfx*)commands);
    while (!g_exec_stack.cmd_stack.empty()) {
        auto cmd = g_exec_stack.cmd_stack.top();

        if (dbg->IsDebugging()) {
            g_exec_stack.gfx_path.push_back(cmd);
            if (dbg->HasBreakPoint(g_exec_stack.gfx_path)) {
                // On a breakpoint with the active framebuffer still set, we need to reset back to prevent
                // soft locking the renderer
                if (mFbActive) {
                    mFbActive = 0;
                    mRapi->StartDrawToFramebuffer(mRendersToFb ? mGameFb : 0, 1);
                }

                break;
            }
            g_exec_stack.gfx_path.pop_back();
        }
        gfx_step();
    }

    Flush();
    mGfxFrameBuffer = 0;
    currentDir = std::stack<std::string>();

    if (mRendersToFb) {
        mRapi->StartDrawToFramebuffer(0, 1);
        mRapi->ClearFramebuffer(true, true);
        if (mMsaaLevel > 1) {
            if (!ViewportMatchesRendererResolution()) {
                mRapi->ResolveMSAAColorBuffer(mGameFbMsaaResolved, mGameFb);
                mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFbMsaaResolved);
            } else {
                mRapi->ResolveMSAAColorBuffer(0, mGameFb);
            }
        } else {
            mGfxFrameBuffer = (uintptr_t)mRapi->GetFramebufferTextureId(mGameFb);
        }
    } else if (mFbActive) {
        // Failsafe reset to main framebuffer to prevent softlocking the renderer
        mFbActive = 0;
        mRapi->StartDrawToFramebuffer(0, 1);

        assert(0 && "active framebuffer was never reset back to original");
    }

    if (sShadowTiming) {
        static double sLight = 0.0, sWhole = 0.0;
        static int sFrames = 0, sWith = 0;
        sWhole += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - runStart).count();
        sLight += lightMs;
        sWith += mShadowDrawn ? 1 : 0;
        if (++sFrames >= 200) {
            SPDLOG_INFO("shadow timing: {} frames, {} with shadows; drawing a frame {:.2f} ms, of that the light's "
                        "pass {:.2f} ms (processor time; map {})",
                        sFrames, sWith, sWhole / sFrames, sWith ? sLight / sWith : 0.0, sShadowMapSize);
            // (and by command: the eight that took the most, their share of all commands' time and how many a frame)
            uint64_t all = 0;
            for (int op = 0; op < 256; op++) {
                all += sOpTicks[op];
            }
            std::string most;
            for (int n = 0; n < 8 && all > 0; n++) {
                int top = 0;
                for (int op = 1; op < 256; op++) {
                    if (sOpTicks[op] > sOpTicks[top]) {
                        top = op;
                    }
                }
                if (sOpTicks[top] == 0) {
                    break;
                }
                most += fmt::format(" {:02X}:{:.0f}%/{}", top, 100.0 * (double)sOpTicks[top] / (double)all,
                                    sOpCount[top] / (uint32_t)sFrames);
                sOpTicks[top] = 0;
            }
            SPDLOG_INFO("shadow timing, by command (share of time / number a frame):{}", most);
            memset(sOpTicks, 0, sizeof(sOpTicks));
            memset(sOpCount, 0, sizeof(sOpCount));
            sLight = sWhole = 0.0;
            sFrames = sWith = 0;
        }
    }
}

void Interpreter::EndFrame() {
    mRapi->EndFrame();
    mWapi->SwapBuffersBegin();
    mRapi->FinishRender();
    mWapi->SwapBuffersEnd();
}

void gfx_set_target_ucode(UcodeHandlers ucode) {
    ucode_handler_index = ucode;
}

int Interpreter::GetTargetFps() {
    return mWapi->GetTargetFps();
}

void Interpreter::SetTargetFps(int fps) {
    mWapi->SetTargetFps(fps);
}

void Interpreter::SetMaxFrameLatency(int latency) {
    mWapi->SetMaxFrameLatency(latency);
}

int Interpreter::CreateFrameBuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                                   uint8_t resize, bool forceFixedAspect) {
    uint32_t orig_width = width, orig_height = height;
    if (resize) {
        AdjustWidthHeightForScale(width, height, native_width, native_height);
    }

    int fb = mRapi->CreateFramebuffer();
    mRapi->UpdateFramebufferParameters(fb, width, height, 1, true, true, true, true);

    mFrameBuffers[fb] = {
        orig_width, orig_height, width, height, native_width, native_height, static_cast<bool>(resize), forceFixedAspect
    };
    return fb;
}

void Interpreter::SetFrameBuffer(int fb, float noiseScale) {
    mRapi->StartDrawToFramebuffer(fb, noiseScale);
    mRapi->ClearFramebuffer(false, true);
}

void Interpreter::CopyFrameBuffer(int fb_dst_id, int fb_src_id, bool copyOnce, bool* hasCopiedPtr) {
    // Do not copy again if we have already copied before
    if (copyOnce && hasCopiedPtr != nullptr && *hasCopiedPtr) {
        return;
    }

    if (fb_src_id == 0 && mRendersToFb) {
        // read from the framebuffer we've been rendering to
        fb_src_id = mGameFb;
    }

    int srcX0, srcY0, srcX1, srcY1;
    int dstX0, dstY0, dstX1, dstY1;

    // When rendering to the main window buffer or MSAA is enabled with a buffer size equal to the view port,
    // then the source coordinates must account for any docked ImGui elements
    if (fb_src_id == 0 || (mMsaaLevel > 1 && mCurDimensions.width == mGameWindowViewport.width &&
                           mCurDimensions.height == mGameWindowViewport.height)) {
        srcX0 = mGameWindowViewport.x;
        srcY0 = mGameWindowViewport.y;
        srcX1 = mGameWindowViewport.x + mGameWindowViewport.width;
        srcY1 = mGameWindowViewport.y + mGameWindowViewport.height;
    } else {
        srcX0 = 0;
        srcY0 = 0;
        srcX1 = mCurDimensions.width;
        srcY1 = mCurDimensions.height;
    }

    dstX0 = 0;
    dstY0 = 0;
    dstX1 = mCurDimensions.width;
    dstY1 = mCurDimensions.height;

    mRapi->CopyFramebuffer(fb_dst_id, fb_src_id, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1);

    // Set the copied pointer if we have one
    if (hasCopiedPtr != nullptr) {
        *hasCopiedPtr = true;
    }
}

void Interpreter::ResetFrameBuffer() {
    mRapi->StartDrawToFramebuffer(0, (float)mCurDimensions.height / mNativeDimensions.height);
}

void Interpreter::AdjustPixelDepthCoordinates(float& x, float& y) {
    x = x * RATIO_X(mActiveFrameBuffer, mCurDimensions) -
        (mNativeDimensions.width * RATIO_X(mActiveFrameBuffer, mCurDimensions) - mCurDimensions.width) / 2;
    y *= RATIO_Y(mActiveFrameBuffer, mCurDimensions);
    if (!mRendersToFb || (mMsaaLevel > 1 && mCurDimensions.width == mGameWindowViewport.width &&
                          mCurDimensions.height == mGameWindowViewport.height)) {
        x += mGameWindowViewport.x;
        y += mGfxCurrentWindowDimensions.height - (mGameWindowViewport.y + mGameWindowViewport.height);
    }
}

void Interpreter::GetPixelDepthPrepare(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);
    mGetPixelDepthPending.emplace(x, y);
}

uint16_t Interpreter::GetPixelDepth(float x, float y) {
    AdjustPixelDepthCoordinates(x, y);

    if (auto it = mGetPixelDepthCached.find(std::make_pair(x, y)); it != mGetPixelDepthCached.end()) {
        return it->second;
    }

    mGetPixelDepthPending.emplace(x, y);

    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res =
        mRapi->GetPixelDepth(mRendersToFb ? mGameFb : 0, mGetPixelDepthPending);
    mGetPixelDepthCached.merge(res);
    mGetPixelDepthPending.clear();

    return mGetPixelDepthCached.find(std::make_pair(x, y))->second;
}

void gfx_push_current_dir(char* path) {
    if (gfx_check_image_signature(path) == 1)
        path = &path[7];

    currentDir.push(GetPathWithoutFileName(path));
}

int32_t gfx_check_image_signature(const char* imgData) {
    uintptr_t i = (uintptr_t)(imgData);

    if ((i & 1) == 1) {
        return 0;
    }

    // Filter addresses that are obviously not valid string pointers before
    // attempting to dereference for the "__OTR__" check.
    if (i == 0 || i < 0x10000) {
        return 0;
    }
#if UINTPTR_MAX > 0xFFFFFFFFu
    // On 64-bit: filter kernel/sentinel addresses. Upper bound covers all
    // user-space layouts (x86_64 47-bit canonical, ARM64 48-bit VA, etc.).
    if (i > 0x0000FFFFFFFFFFFFull) {
        return 0;
    }
#endif

    return Ship::Context::GetRawInstance()->GetResourceManager()->OtrSignatureCheck(imgData);
}

void Interpreter::RegisterBlendedTexture(const char* name, uint8_t* mask, uint8_t* replacement) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    if (gfx_check_image_signature(reinterpret_cast<char*>(replacement))) {
        Fast::Texture* tex = std::static_pointer_cast<Fast::Texture>(
                                 Ship::Context::GetRawInstance()->GetResourceManager()->LoadResourceProcess(
                                     reinterpret_cast<char*>(replacement)))
                                 .get();

        replacement = tex->ImageData;
    }

    mMaskedTextures[name] = MaskedTextureEntry{ mask, replacement };
}

void Interpreter::UnregisterBlendedTexture(const char* name) {
    if (gfx_check_image_signature(name)) {
        name += 7;
    }

    mMaskedTextures.erase(name);
}

// New getters and setters
void Interpreter::SetNativeDimensions(float width, float height) {
    mNativeDimensions.width = width;
    mNativeDimensions.height = height;
}

void Interpreter::SetResolutionMultiplier(float multiplier) {
    mCurDimensions.internal_mul = multiplier;
}

void Interpreter::SetMsaaLevel(uint32_t level) {
    sUserMsaaLevel = level;
    mMsaaLevel = PixelStyle(mRenderStyle) ? 1 : level; // MergeShip2 (see sUserMsaaLevel)
}

void Interpreter::GetCurDimensions(uint32_t* width, uint32_t* height) {
    *width = mCurDimensions.width;
    *height = mCurDimensions.height;
}

} // namespace Fast

void gfx_cc_get_features(uint64_t shader_id0, uint64_t shader_id1, struct CCFeatures* cc_features) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 4; k++) {
                cc_features->c[i][j][k] = shader_id0 >> i * 32 + j * 16 + k * 4 & 0xf;
            }
        }
    }

    cc_features->opt_alpha = (shader_id1 & SHADER_OPT(ALPHA)) != 0;
    cc_features->opt_fog = (shader_id1 & SHADER_OPT(FOG)) != 0;
    cc_features->opt_texture_edge = (shader_id1 & SHADER_OPT(TEXTURE_EDGE)) != 0;
    cc_features->opt_noise = (shader_id1 & SHADER_OPT(NOISE)) != 0;
    cc_features->opt_2cyc = (shader_id1 & SHADER_OPT(_2CYC)) != 0;
    cc_features->opt_alpha_threshold = (shader_id1 & SHADER_OPT(ALPHA_THRESHOLD)) != 0;
    cc_features->opt_invisible = (shader_id1 & SHADER_OPT(INVISIBLE)) != 0;
    cc_features->opt_grayscale = (shader_id1 & SHADER_OPT(GRAYSCALE)) != 0;
    cc_features->opt_prim_depth = (shader_id1 & SHADER_OPT(PRIM_DEPTH)) != 0;

    cc_features->clamp[0][0] = shader_id1 & SHADER_OPT(TEXEL0_CLAMP_S);
    cc_features->clamp[0][1] = shader_id1 & SHADER_OPT(TEXEL0_CLAMP_T);
    cc_features->clamp[1][0] = shader_id1 & SHADER_OPT(TEXEL1_CLAMP_S);
    cc_features->clamp[1][1] = shader_id1 & SHADER_OPT(TEXEL1_CLAMP_T);

    cc_features->usedTextures[0] = false;
    cc_features->usedTextures[1] = false;
    cc_features->used_masks[0] = false;
    cc_features->used_masks[1] = false;
    cc_features->used_blend[0] = false;
    cc_features->used_blend[1] = false;
    cc_features->numInputs = 0;

    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 4; j++) {
                if (cc_features->c[c][i][j] >= SHADER_INPUT_1 && cc_features->c[c][i][j] <= SHADER_INPUT_7) {
                    if (cc_features->c[c][i][j] > cc_features->numInputs) {
                        cc_features->numInputs = cc_features->c[c][i][j];
                    }
                }
                if (cc_features->c[c][i][j] == SHADER_TEXEL0 || cc_features->c[c][i][j] == SHADER_TEXEL0A) {
                    cc_features->usedTextures[0] = true;
                    if (cc_features->opt_2cyc) {
                        cc_features->usedTextures[1] = true;
                    }
                }
                if (cc_features->c[c][i][j] == SHADER_TEXEL1 || cc_features->c[c][i][j] == SHADER_TEXEL1A) {
                    cc_features->usedTextures[1] = true;
                    if (cc_features->opt_2cyc) {
                        cc_features->usedTextures[0] = true;
                    }
                }
            }
        }
    }

    for (int c = 0; c < 2; c++) {
        cc_features->do_single[c][0] = cc_features->c[c][0][2] == SHADER_0;
        cc_features->do_single[c][1] = cc_features->c[c][1][2] == SHADER_0;
        cc_features->do_multiply[c][0] = cc_features->c[c][0][1] == SHADER_0 && cc_features->c[c][0][3] == SHADER_0;
        cc_features->do_multiply[c][1] = cc_features->c[c][1][1] == SHADER_0 && cc_features->c[c][1][3] == SHADER_0;
        cc_features->do_mix[c][0] = cc_features->c[c][0][1] == cc_features->c[c][0][3];
        cc_features->do_mix[c][1] = cc_features->c[c][1][1] == cc_features->c[c][1][3];
        cc_features->color_alpha_same[c] = (shader_id0 >> c * 32 & 0xffff) == (shader_id0 >> c * 32 + 16 & 0xffff);
    }

    if (cc_features->usedTextures[0] && shader_id1 & SHADER_OPT(TEXEL0_MASK)) {
        cc_features->used_masks[0] = true;
    }
    if (cc_features->usedTextures[1] && shader_id1 & SHADER_OPT(TEXEL1_MASK)) {
        cc_features->used_masks[1] = true;
    }

    if (cc_features->usedTextures[0] && shader_id1 & SHADER_OPT(TEXEL0_BLEND)) {
        cc_features->used_blend[0] = true;
    }
    if (cc_features->usedTextures[1] && shader_id1 & SHADER_OPT(TEXEL1_BLEND)) {
        cc_features->used_blend[1] = true;
    }

    cc_features->shader_id = Fast::ShaderIdUnmask(shader_id1);
    cc_features->render_style = (int)((shader_id1 >> RENDER_STYLE_SHIFT) & 0xF);
    cc_features->shade_slot = (int)((shader_id1 >> RENDER_STYLE_SHADE_SHIFT) & 7);
}

extern "C" int gfx_create_framebuffer(uint32_t width, uint32_t height, uint32_t native_width, uint32_t native_height,
                                      uint8_t resize, bool forceFixedAspect) {
    return Fast::mInstance.lock().get()->CreateFrameBuffer(width, height, native_width, native_height, resize,
                                                           forceFixedAspect);
}

extern "C" void gfx_texture_cache_clear() {
    Fast::mInstance.lock().get()->TextureCacheClear();
}

extern "C" void gfx_shader_cache_clear() {
    auto instance = Fast::mInstance.lock().get();
    instance->mColorCombinerPool.clear();
    instance->mPrevCombiner = Fast::mInstance.lock().get()->mColorCombinerPool.end();
    instance->mRenderingState.mShaderProgram = nullptr;
    instance->mRapi->ClearShaderCache();
}

extern "C" void gfx_register_fb_texture(const void* cpuAddr, int fbId) {
    Fast::mInstance.lock().get()->RegisterFbTexture(cpuAddr, fbId);
}

extern "C" void gfx_unregister_fb_texture(const void* cpuAddr) {
    Fast::mInstance.lock().get()->UnregisterFbTexture(cpuAddr);
}