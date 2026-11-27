#pragma once

#include <stdint.h>

#include <unordered_map>
#include <set>
#include "imconfig.h"

namespace Fast {
struct ShaderProgram;

struct GfxClipParameters {
    bool z_is_from_0_to_1;
    bool invertY;
};

enum FilteringMode { FILTER_THREE_POINT, FILTER_LINEAR, FILTER_NONE };

// A hash function used to hash a: pair<float, float>
struct hash_pair_ff {
    size_t operator()(const std::pair<float, float>& p) const {
        const auto hash1 = std::hash<float>{}(p.first);
        const auto hash2 = std::hash<float>{}(p.second);

        // If hash1 == hash2, their XOR is zero.
        return (hash1 != hash2) ? hash1 ^ hash2 : hash1;
    }
};

class GfxRenderingAPI {
  public:
    virtual ~GfxRenderingAPI() = default;
    virtual const char* GetName() = 0;
    virtual int GetMaxTextureSize() = 0;
    virtual GfxClipParameters GetClipParameters() = 0;
    virtual void UnloadShader(ShaderProgram* oldPrg) = 0;
    virtual void LoadShader(ShaderProgram* newPrg) = 0;
    virtual void ClearShaderCache() = 0;
    virtual ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint64_t shaderId1) = 0;
    virtual ShaderProgram* LookupShader(uint64_t shaderId0, uint64_t shaderId1) = 0;
    virtual void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) = 0;
    virtual uint32_t NewTexture() = 0;
    virtual void SelectTexture(int tile, uint32_t textureId) = 0;
    virtual void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) = 0;
    virtual void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) = 0;
    virtual void SetDepthTestAndMask(bool depth_test, bool z_upd) = 0;
    virtual void SetZmodeDecal(bool decal) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    virtual void SetScissor(int x, int y, int width, int height) = 0;
    virtual void SetUseAlpha(bool useAlpha) = 0;
    virtual void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) = 0;
    virtual void Init() = 0;
    virtual void OnResize() = 0;
    virtual void StartFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void FinishRender() = 0;
    virtual int CreateFramebuffer() = 0;
    virtual void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                             bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                             bool can_extract_depth) = 0;
    virtual void StartDrawToFramebuffer(int fbId, float noiseScale) = 0;
    virtual void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0,
                                 int dstY0, int dstX1, int dstY1) = 0;
    virtual void ClearFramebuffer(bool color, bool depth) = 0;
    virtual void ClearDepthRegion(int x, int y, int w, int h) {
        // Default: full depth clear. Backends that support scissored depth clears
        // (e.g. OpenGL) should override for a more precise partial clear.
        ClearFramebuffer(false, true);
    }
    virtual void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) = 0;
    virtual void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) = 0;
    virtual std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) = 0;
    virtual void* GetFramebufferTextureId(int fbId) = 0;
    virtual void SelectTextureFb(int fbId) = 0;
    virtual void DeleteTexture(uint32_t texId) = 0;
    virtual void SetTextureFilter(FilteringMode mode) = 0;
    virtual FilteringMode GetTextureFilter() = 0;
    virtual void SetSrgbMode() = 0;
    virtual ImTextureID GetTextureById(int id) = 0;
    virtual void SetCurrentPrimDepth(float depth) = 0;

    // MergeShip2 additions. They stay at the end of the virtual table, and everything they need is kept out of the
    // classes above, so that a game module built before them still runs against this library.
    //
    // Explicit blend factors (G_BLENDEX_MODE in lus_gbi.h), or 0 for the usual alpha blending. A backend without it
    // keeps alpha blending.
    virtual void SetBlendMode(uint32_t mode, const uint8_t constantColor[4]) {
    }

    // Real-time shadows (Interpreter::ApplyShadows). What has been drawn on the framebuffer in use is darkened where
    // it lies in the shadow of what was drawn into `shadowFb` from the light (that framebuffer's depth is the light's
    // view). `cameraInverse` takes a point of the framebuffer (x and y from -1 to 1 with y up, z from -1 to 1) back
    // into the world, `light` takes a point of the world into the light's view (the same ranges); both are the
    // interpreter's matrices (a row vector times the matrix). `lightDir`: the way to the light, in the world.
    // params: [0] how bright a surface in shadow stays (0 to 1), [1] the same for a surface turned away from the
    // light (1: left as it is), [2] the depth bias, in the light's depth, [3] unused, [4] a texel of the shadow map
    // in the world's units, [5] the light's whole depth in them, [6] how much wider a shadow's edge grows per unit
    // between what casts it and what it falls on, [7] the widest edge, in texels, [8] and [9] the distances from
    // what casts it between which a shadow fades away ([9] of 0: it does not), [10] 1 where the shadow map was
    // drawn this frame (0: no cast shadows, only the rest), [11] the wind's clock in seconds, [12] how dark the
    // ambient shading goes (0: none) and [13] how far it looks in the world's units, [14] how dark a cloud's shadow
    // is (0: none) and [15] a cloud's size in the world's units, [16] to [18] what a shadow leaves of red, green and
    // blue against its plain shade, [19] the clouds' speed in units a second, [20] to [22] how much brighter what
    // the light reaches is drawn, red, green and blue (1: as it is), [23] unused, [24] how many lamps follow (at
    // most 12) and [25] how strongly their light is drawn, [26] and [27] unused, then eight a lamp from [28]: its
    // place (x, y, z), how far it reaches, its colour (red, green, blue, 0 to 1) and one unused. viewport: the
    // part of the framebuffer the world is drawn into, as SetViewport takes it (x, y, width, height). A backend
    // without it draws none of this.
    virtual void ApplyShadows(int shadowFb, const float cameraInverse[4][4], const float light[4][4],
                              const float lightDir[3], const float params[124], const int viewport[4]) {
    }

    // Real-time shadows: what is drawn from here until the next StartDrawToFramebuffer goes into the shadow map, a
    // square picture of depth alone, `size` texels a side, that the backend keeps for it (and lets go of when it
    // has not been asked for in a while); it starts cleared, its viewport and scissor the whole of it. ApplyShadows
    // with a shadowFb of 0 reads that map. false: this backend has none, and nothing was changed.
    // clip: what lies nearer the light than the map's near side is cut away there (the light starts at that plane);
    // without it such a thing is pressed onto the near side and still covers what is behind it.
    virtual bool StartShadowMap(int size, bool clip) {
        return false;
    }

    // The finished picture's glow and look (Interpreter, G_REGS_EX 12). What has been drawn on the framebuffer in use
    // -- the world, before the game's fades, its HUD and its text -- is drawn over itself with: what is out of focus
    // blurred, the bright parts glowing over what is about them, and a film's look. `cameraInverse` and `viewport`
    // are the world's, as ApplyShadows takes them. params: [0] how bright a thing must be to glow (0 to 1), [1] how
    // strongly the glow is laid over the picture (0: none), [2], [3] unused; [4] how much of the film's curve (0:
    // none), [5] saturation (1: as it is), [6] how dark the corners go (0: not at all), [7] how much of the grade
    // (cool shadows, warm lights); [8] how strongly what is out of focus is blurred (0: nothing is), [9] the
    // distance in focus (the world's units from the eye), [10] how far either side of it stays sharp, [11] over how
    // much further the blur grows to its full; the rest unused (0). A backend without it draws none of this.
    virtual void ApplyPost(const float cameraInverse[4][4], const float params[48], const int viewport[4]) {
    }

  protected:
    int8_t mCurrentDepthTest = 0;
    int8_t mCurrentDepthMask = 0;
    int8_t mCurrentZmodeDecal = 0;
    int8_t mLastDepthTest = -1;
    int8_t mLastDepthMask = -1;
    int8_t mLastZmodeDecal = -1;
    bool mSrgbMode = false;
    float mCurrentPrimDepth = 0.0f;
    bool mPrimDepthDirty = true;
};
} // namespace Fast
