#ifdef ENABLE_DX11

#include <cstdio>
#include <vector>
#include <cmath>

#include <map>
#include <unordered_map>
#include <fstream>
#include <iterator>
#include <string>

#include <windows.h>
#include <versionhelpers.h>
#include <wrl/client.h>

#include <dxgi1_3.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#include "fast/backends/gfx_window_manager_api.h"
#include "fast/backends/gfx_direct3d_common.h"

#define DECLARE_GFX_DXGI_FUNCTIONS
#include "fast/backends/gfx_dxgi.h"

#include "fast/backends/gfx_screen_config.h"
#include "ship/window/gui/Gui.h"
#include "fast/Fast3dGui.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/window/Window.h"

#include "fast/backends/gfx_rendering_api.h"
#include "fast/interpreter.h"

#include <prism/processor.h>
#include "ship/config/ConsoleVariable.h"
#include <ship/resource/factory/ShaderFactory.h>
#include <ship/resource/ResourceManager.h>
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"

#include "fast/Fast3dWindow.h"

#define DEBUG_D3D 0

using namespace Microsoft::WRL; // For ComPtr

namespace Fast {

GfxRenderingAPIDX11::~GfxRenderingAPIDX11() {
}

GfxRenderingAPIDX11::GfxRenderingAPIDX11(GfxWindowBackendDXGI* backend) {
    mWindowBackend = backend;
}

void GfxRenderingAPIDX11::CreateDepthStencilObjects(uint32_t width, uint32_t height, uint32_t msaa_count,
                                                    ID3D11DepthStencilView** view, ID3D11ShaderResourceView** srv) {
    D3D11_TEXTURE2D_DESC texture_desc;
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format =
        mFeatureLevel >= D3D_FEATURE_LEVEL_10_0 ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;
    texture_desc.SampleDesc.Count = msaa_count;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | (srv != nullptr ? D3D11_BIND_SHADER_RESOURCE : 0);
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> texture;
    ThrowIfFailed(mDevice->CreateTexture2D(&texture_desc, nullptr, texture.GetAddressOf()));

    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc;
    view_desc.Format = mFeatureLevel >= D3D_FEATURE_LEVEL_10_0 ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
    view_desc.Flags = 0;
    if (msaa_count > 1) {
        view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        view_desc.Texture2DMS.UnusedField_NothingToDefine = 0;
    } else {
        view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        view_desc.Texture2D.MipSlice = 0;
    }

    ThrowIfFailed(mDevice->CreateDepthStencilView(texture.Get(), &view_desc, view));

    if (srv != nullptr) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format =
            mFeatureLevel >= D3D_FEATURE_LEVEL_10_0 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srv_desc.ViewDimension = msaa_count > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = -1;

        ThrowIfFailed(mDevice->CreateShaderResourceView(texture.Get(), &srv_desc, srv));
    }
}
static bool CreateDeviceFunc(class GfxRenderingAPIDX11* self, bool SoftwareRenderer) {
#if DEBUG_D3D
    UINT device_creation_flags = D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT device_creation_flags = 0;
#endif
    bool CreationFailed = false;
    const char HardwareText[320] = "\nUsing software renderer. Performance issues are to be expected.\n\n"
                                   "Please check your preferred GPU in Windows graphics or GPU driver settings and "
                                   "make sure you have the correct GPU drivers installed.\n\n"
                                   "You can also try to change the graphic backend of the port in its config file.\n"
                                   "Window->Backend->Id\n"
                                   "0 = DX11, 1 = OpenGL";
    const char SoftwareText[33] = "\nUsing software renderer failed.";

    if (SoftwareRenderer) {
        SPDLOG_INFO("Using software renderer.");
    }

    HRESULT res = self->mDX11CreateDevice(
        NULL, SoftwareRenderer ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE, nullptr, device_creation_flags, NULL,
        NULL, D3D11_SDK_VERSION, self->mDevice.GetAddressOf(), &self->mFeatureLevel, self->mContext.GetAddressOf());

    // Get and log name of adapter
    IDXGIDevice* DXGIDevice = nullptr;
    IDXGIAdapter* Adapter = nullptr;
    DXGI_ADAPTER_DESC adapterDesc;
    std::wstring adapterName;
    char adapterNameCStr[128] = "";
    char error_message[512];
    HRESULT res2;

    res2 = self->mDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);
    if (SUCCEEDED(res2)) {
        res2 = DXGIDevice->GetAdapter(&Adapter);
        if (SUCCEEDED(res2)) {
            res2 = Adapter->GetDesc(&adapterDesc);
            if (SUCCEEDED(res2)) {
                adapterName = adapterDesc.Description;
                wcstombs(adapterNameCStr, adapterName.c_str(), 128);
            }
            Adapter->Release();
        }
        DXGIDevice->Release();
    }
    SPDLOG_INFO("Using D3D adapter: {0}", adapterNameCStr);

    if (FAILED(res)) {
        CreationFailed = true;
        SPDLOG_WARN("Failed to create a D3D device. HRESULT: 0x{0:08x}", res);
        snprintf(error_message, sizeof(error_message), "Failed to create a D3D device on %s\nHRESULT: 0x%08X%s",
                 adapterNameCStr, res, SoftwareRenderer ? SoftwareText : HardwareText);
    }

    else if (self->mFeatureLevel < D3D_FEATURE_LEVEL_10_0) {
        CreationFailed = true;
        SPDLOG_WARN("D3D adapter doesn't support D3D feature level 10_0 or greater.");
        snprintf(error_message, sizeof(error_message), "%s doesn't support D3D feature level 10_0 or greater.%s",
                 adapterNameCStr, SoftwareRenderer ? SoftwareText : HardwareText);
    }

    else if (self->mFeatureLevel < D3D_FEATURE_LEVEL_10_1) {
        SPDLOG_WARN("D3D adapter doesn't support D3D feature level 10_1 or greater. MSAA setting will be ignored.");

    } else {
        // Check for Compute Shader support
        if (self->mDevice != NULL) {
            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS features;
            self->mDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &features,
                                               sizeof(D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS));
            if (features.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x == false) {
                CreationFailed = true;
                SPDLOG_WARN("D3D adapter doesn't support compute shaders.");
                snprintf(error_message, sizeof(error_message), "%s doesn't support compute shaders.%s", adapterNameCStr,
                         SoftwareRenderer ? SoftwareText : HardwareText);
            }
        }
    }

    if (CreationFailed) {
        if (self->mContext) {
            self->mContext->Release();
        }
        if (self->mDevice) {
            self->mDevice->Release();
        }
        MessageBoxA(self->mWindowBackend->GetWindowHandle(), error_message, "Warning", MB_OK | MB_ICONWARNING);
        return false;
    }
    return true;
};

void GfxRenderingAPIDX11::Init() {
    // Load d3d11.dll
    mDX11Module = LoadLibraryW(L"d3d11.dll");
    if (mDX11Module == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), mWindowBackend->GetWindowHandle(), "d3d11.dll not found");
    }
    mDX11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(mDX11Module, "D3D11CreateDevice");

    // Load D3DCompiler_47.dll
    mCompilerModule = LoadLibraryW(L"D3DCompiler_47.dll");
    if (mCompilerModule == nullptr) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()), mWindowBackend->GetWindowHandle(),
                      "D3DCompiler_47.dll not found");
    }
    mD3dCompile = (pD3DCompile)GetProcAddress(mCompilerModule, "D3DCompile");

    // Create D3D11 mDevice

    mWindowBackend->CreateFactoryAndDevice(DEBUG_D3D, 11, this, CreateDeviceFunc);

    // Create the swap chain
    mWindowBackend->CreateSwapChain(mDevice.Get(), [this]() {
        mFrameBuffers[0].render_target_view.Reset();
        mTextures[mFrameBuffers[0].texture_id].texture.Reset();
        mContext->ClearState();
        mContext->Flush();

        mLastShaderProgram = nullptr;
        mLastVertexBufferStride = 0;
        mLastBlendState.Reset();
        for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
            mLastResourceViews[i].Reset();
            mLastSamplerStates[i].Reset();
        }
        mLastDepthTest = -1;
        mLastDepthMask = -1;
        mLastZmodeDecal = -1;
        mLastPrimitaveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    });

    // Create D3D Debug mDevice if in debug mode

#if DEBUG_D3D
    ThrowIfFailed(mDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)debug.GetAddressOf()),
                  mWindowBackend->GetWindowHandle(), "Failed to get ID3D11Debug device.");
#endif

    // Create the default framebuffer which represents the window
    FramebufferDX11& fb = mFrameBuffers[CreateFramebuffer()];

    // Check the size of the window
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
    ThrowIfFailed(mWindowBackend->GetSwapChain()->GetDesc1(&swap_chain_desc));
    mTextures[fb.texture_id].width = swap_chain_desc.Width;
    mTextures[fb.texture_id].height = swap_chain_desc.Height;
    fb.msaa_level = 1;

    for (uint32_t sample_count = 1; sample_count <= D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT; sample_count++) {
        ThrowIfFailed(mDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, sample_count,
                                                             &mMsaaNumQualityLevels[sample_count - 1]));
    }

    // Create main vertex buffer

    D3D11_BUFFER_DESC vertex_buffer_desc;
    ZeroMemory(&vertex_buffer_desc, sizeof(D3D11_BUFFER_DESC));

    vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    vertex_buffer_desc.ByteWidth = 256 * 32 * 3 * sizeof(float); // Same as buf_vbo size in gfx_pc
    vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    vertex_buffer_desc.MiscFlags = 0;

    ThrowIfFailed(mDevice->CreateBuffer(&vertex_buffer_desc, nullptr, mVertexBuffer.GetAddressOf()),
                  mWindowBackend->GetWindowHandle(), "Failed to create vertex buffer.");

    // Create per-frame constant buffer

    D3D11_BUFFER_DESC constant_buffer_desc;
    ZeroMemory(&constant_buffer_desc, sizeof(D3D11_BUFFER_DESC));

    constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    constant_buffer_desc.ByteWidth = sizeof(PerFrameCB);
    constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constant_buffer_desc.MiscFlags = 0;

    ThrowIfFailed(mDevice->CreateBuffer(&constant_buffer_desc, nullptr, mPerFrameCb.GetAddressOf()),
                  mWindowBackend->GetWindowHandle(), "Failed to create per-frame constant buffer.");

    // Create per-draw constant buffer

    constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    constant_buffer_desc.ByteWidth = sizeof(PerDrawCB);
    constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constant_buffer_desc.MiscFlags = 0;

    ThrowIfFailed(mDevice->CreateBuffer(&constant_buffer_desc, nullptr, mPerDrawCb.GetAddressOf()),
                  mWindowBackend->GetWindowHandle(), "Failed to create per-draw constant buffer.");

    // Create per-prim-depth constant buffer (G_ZS_PRIM), uploaded only when mPrimDepthDirty

    constant_buffer_desc.ByteWidth = sizeof(PerPrimDepthCB);
    ThrowIfFailed(mDevice->CreateBuffer(&constant_buffer_desc, nullptr, mPerPrimDepthCb.GetAddressOf()),
                  mWindowBackend->GetWindowHandle(), "Failed to create per-prim-depth constant buffer.");

    // Create compute shader that can be used to retrieve depth buffer values

    const char* shader_source = R"(
sampler my_sampler : register(s0);
Texture2D<float> tex : register(t0);
StructuredBuffer<int2> coord : register(t1);
RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    output[DTid.x] = tex.Load(int3(coord[DTid.x], 0));
}
)";

    const char* shader_source_msaa = R"(
sampler my_sampler : register(s0);
Texture2DMS<float, 2> tex : register(t0);
StructuredBuffer<int2> coord : register(t1);
RWStructuredBuffer<float> output : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) {
    output[DTid.x] = tex.Load(coord[DTid.x], 0);
}
)";

#if DEBUG_D3D
    UINT compile_flags = D3DCOMPILE_DEBUG;
#else
    UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;
#endif

    ComPtr<ID3DBlob> cs, error_blob;
    HRESULT hr;

    hr = mD3dCompile(shader_source, strlen(shader_source), nullptr, nullptr, nullptr, "CSMain", "cs_4_0", compile_flags,
                     0, cs.GetAddressOf(), error_blob.GetAddressOf());

    if (FAILED(hr)) {
        char* err = (char*)error_blob->GetBufferPointer();
        MessageBoxA(mWindowBackend->GetWindowHandle(), err, "Error", MB_OK | MB_ICONERROR);
        throw Ship::HResultException(hr, "Compute shader compilation failed");
    }

    ThrowIfFailed(mDevice->CreateComputeShader(cs->GetBufferPointer(), cs->GetBufferSize(), nullptr,
                                               mComputeShader.GetAddressOf()));

    hr = mD3dCompile(shader_source_msaa, strlen(shader_source_msaa), nullptr, nullptr, nullptr, "CSMain", "cs_4_1",
                     compile_flags, 0, mComputeShaderMsaaBlob.GetAddressOf(), error_blob.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        char* err = (char*)error_blob->GetBufferPointer();
        MessageBoxA(mWindowBackend->GetWindowHandle(), err, "Error", MB_OK | MB_ICONERROR);
        throw Ship::HResultException(hr, "MSAA compute shader compilation failed");
    }

    // Create ImGui

    Fast::GuiWindowInitData window_impl;
    window_impl.Dx11 = { mWindowBackend->GetWindowHandle(), mContext.Get(), mDevice.Get() };
    window_impl.Backend = WindowBackend::FAST3D_DXGI_DX11;
    std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui())
        ->Init(window_impl);
}

int GfxRenderingAPIDX11::GetMaxTextureSize() {
    return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}

const char* GfxRenderingAPIDX11::GetName() {
    return "DirectX 11";
}

struct GfxClipParameters GfxRenderingAPIDX11::GetClipParameters() {
    return { true, false };
}

void GfxRenderingAPIDX11::UnloadShader(struct ShaderProgram* old_prg) {
}

void GfxRenderingAPIDX11::LoadShader(struct ShaderProgram* new_prg) {
    mShaderProgram = (struct ShaderProgramD3D11*)new_prg;
}

void GfxRenderingAPIDX11::ClearShaderCache() {
    mShaderProgramPool.clear();
}

struct ShaderProgram* GfxRenderingAPIDX11::CreateAndLoadNewShader(uint64_t shader_id0, uint64_t shader_id1) {
    CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);

    char* buf;
    size_t len, numFloats;

    auto shader = gfx_direct3d_common_build_shader(numFloats, cc_features, false,
                                                   mCurrentFilterMode == FILTER_THREE_POINT, mSrgbMode);

    buf = shader.data();
    len = shader.size();

    ComPtr<ID3DBlob> vs, ps;
    ComPtr<ID3DBlob> error_blob;

#if DEBUG_D3D
    UINT compile_flags = D3DCOMPILE_DEBUG;
#else
    UINT compile_flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;
#endif

    HRESULT hr = mD3dCompile(buf, len, nullptr, nullptr, nullptr, "VSMain", "vs_4_0", compile_flags, 0,
                             vs.GetAddressOf(), error_blob.GetAddressOf());

    if (FAILED(hr)) {
        char* err = (char*)error_blob->GetBufferPointer();
        // MergeShip2: the compiler's message goes to the log too (a test run has no one to read the box)
        SPDLOG_ERROR("Vertex shader {:016x} {:016x} failed to compile: {}", shader_id0, shader_id1, err);
        MessageBoxA(mWindowBackend->GetWindowHandle(), err, "Error", MB_OK | MB_ICONERROR);
        throw Ship::HResultException(hr, "Vertex shader compilation failed");
    }

    hr = mD3dCompile(buf, len, nullptr, nullptr, nullptr, "PSMain", "ps_4_0", compile_flags, 0, ps.GetAddressOf(),
                     error_blob.GetAddressOf());

    if (FAILED(hr)) {
        char* err = (char*)error_blob->GetBufferPointer();
        SPDLOG_ERROR("Pixel shader {:016x} {:016x} failed to compile: {}", shader_id0, shader_id1, err);
        MessageBoxA(mWindowBackend->GetWindowHandle(), err, "Error", MB_OK | MB_ICONERROR);
        throw Ship::HResultException(hr, "Pixel shader compilation failed");
    }

    struct ShaderProgramD3D11* prg = &mShaderProgramPool[std::make_pair(shader_id0, shader_id1)];

    ThrowIfFailed(mDevice->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr,
                                              prg->vertex_shader.GetAddressOf()));
    ThrowIfFailed(mDevice->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr,
                                             prg->pixel_shader.GetAddressOf()));

    // Input Layout

    D3D11_INPUT_ELEMENT_DESC ied[16];
    uint8_t ied_index = 0;
    ied[ied_index++] = {
        "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0
    };
    for (UINT i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            ied[ied_index++] = {
                "TEXCOORD", i, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0
            };
            if (cc_features.clamp[i][0]) {
                ied[ied_index++] = { "TEXCLAMPS",
                                     i,
                                     DXGI_FORMAT_R32_FLOAT,
                                     0,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0 };
            }
            if (cc_features.clamp[i][1]) {
                ied[ied_index++] = { "TEXCLAMPT",
                                     i,
                                     DXGI_FORMAT_R32_FLOAT,
                                     0,
                                     D3D11_APPEND_ALIGNED_ELEMENT,
                                     D3D11_INPUT_PER_VERTEX_DATA,
                                     0 };
            }
        }
    }
    if (cc_features.opt_fog) {
        ied[ied_index++] = {
            "FOG", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0
        };
    }
    if (cc_features.opt_grayscale) {
        ied[ied_index++] = { "GRAYSCALE",
                             0,
                             DXGI_FORMAT_R32G32B32A32_FLOAT,
                             0,
                             D3D11_APPEND_ALIGNED_ELEMENT,
                             D3D11_INPUT_PER_VERTEX_DATA,
                             0 };
    }
    for (unsigned int i = 0; i < cc_features.numInputs; i++) {
        DXGI_FORMAT format = cc_features.opt_alpha ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R32G32B32_FLOAT;
        ied[ied_index++] = { "INPUT", i, format, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 };
    }

    ThrowIfFailed(mDevice->CreateInputLayout(ied, ied_index, vs->GetBufferPointer(), vs->GetBufferSize(),
                                             prg->input_layout.GetAddressOf()));

    // Blend state

    D3D11_BLEND_DESC blend_desc;
    ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));

    if (cc_features.opt_alpha) {
        blend_desc.RenderTarget[0].BlendEnable = true;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[0].DestBlendAlpha =
            D3D11_BLEND_ONE; // We initially clear alpha to 1.0f and want to keep it at 1.0f
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    } else {
        blend_desc.RenderTarget[0].BlendEnable = false;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }

    ThrowIfFailed(mDevice->CreateBlendState(&blend_desc, prg->blend_state.GetAddressOf()));

    // Save some values

    prg->shader_id0 = shader_id0;
    prg->shader_id1 = shader_id1;
    prg->numInputs = cc_features.numInputs;
    prg->numFloats = numFloats;
    prg->usedTextures[0] = cc_features.usedTextures[0];
    prg->usedTextures[1] = cc_features.usedTextures[1];
    prg->usedTextures[2] = cc_features.used_masks[0];
    prg->usedTextures[3] = cc_features.used_masks[1];
    prg->usedTextures[4] = cc_features.used_blend[0];
    prg->usedTextures[5] = cc_features.used_blend[1];

    return (struct ShaderProgram*)(mShaderProgram = prg);
}

struct ShaderProgram* GfxRenderingAPIDX11::LookupShader(uint64_t shader_id0, uint64_t shader_id1) {
    auto it = mShaderProgramPool.find(std::make_pair(shader_id0, shader_id1));
    return it == mShaderProgramPool.end() ? nullptr : (struct ShaderProgram*)&it->second;
}

void GfxRenderingAPIDX11::ShaderGetInfo(struct ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    struct ShaderProgramD3D11* p = (struct ShaderProgramD3D11*)prg;

    *numInputs = p->numInputs;
    usedTextures[0] = p->usedTextures[0];
    usedTextures[1] = p->usedTextures[1];
}

uint32_t GfxRenderingAPIDX11::NewTexture() {
    mTextures.resize(mTextures.size() + 1);
    return (uint32_t)(mTextures.size() - 1);
}

void GfxRenderingAPIDX11::DeleteTexture(uint32_t texID) {
    // glDeleteTextures(1, &texID);
}

void GfxRenderingAPIDX11::SelectTexture(int tile, uint32_t texture_id) {
    mCurrentTile = tile;
    mCurrentTextureIds[tile] = texture_id;
}

static D3D11_TEXTURE_ADDRESS_MODE gfx_cm_to_d3d11(uint32_t val) {
    // TODO: handle G_TX_MIRROR | G_TX_CLAMP
    if (val & G_TX_CLAMP) {
        return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
    return (val & G_TX_MIRROR) ? D3D11_TEXTURE_ADDRESS_MIRROR : D3D11_TEXTURE_ADDRESS_WRAP;
}

void GfxRenderingAPIDX11::UploadTexture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    // Create texture

    TextureData* texture_data = &mTextures[mCurrentTextureIds[mCurrentTile]];
    texture_data->width = width;
    texture_data->height = height;

    D3D11_TEXTURE2D_DESC texture_desc;
    ZeroMemory(&texture_desc, sizeof(D3D11_TEXTURE2D_DESC));

    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0; // D3D11_RESOURCE_MISC_GENERATE_MIPS ?
    texture_desc.ArraySize = 1;
    texture_desc.MipLevels = 1;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;

    D3D11_SUBRESOURCE_DATA resource_data;
    resource_data.pSysMem = rgba32_buf;
    resource_data.SysMemPitch = width * 4;
    resource_data.SysMemSlicePitch = resource_data.SysMemPitch * height;

    ThrowIfFailed(
        mDevice->CreateTexture2D(&texture_desc, &resource_data, texture_data->texture.ReleaseAndGetAddressOf()));

    // Create shader resource view from texture

    ThrowIfFailed(mDevice->CreateShaderResourceView(texture_data->texture.Get(), nullptr,
                                                    texture_data->resource_view.ReleaseAndGetAddressOf()));
}

void GfxRenderingAPIDX11::SetSamplerParameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    D3D11_SAMPLER_DESC sampler_desc;
    ZeroMemory(&sampler_desc, sizeof(D3D11_SAMPLER_DESC));

    sampler_desc.Filter = linear_filter && mCurrentFilterMode == FILTER_LINEAR ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
                                                                               : D3D11_FILTER_MIN_MAG_MIP_POINT;

    sampler_desc.AddressU = gfx_cm_to_d3d11(cms);
    sampler_desc.AddressV = gfx_cm_to_d3d11(cmt);
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

    TextureData* texture_data = &mTextures[mCurrentTextureIds[tile]];
    texture_data->linear_filtering = linear_filter;

    // This function is called twice per texture, the first one only to set default values.
    // Maybe that could be skipped? Anyway, make sure to release the first default sampler
    // state before setting the actual one.
    texture_data->sampler_state.Reset();

    ThrowIfFailed(mDevice->CreateSamplerState(&sampler_desc, texture_data->sampler_state.GetAddressOf()));
}

void GfxRenderingAPIDX11::SetDepthTestAndMask(bool depth_test, bool depth_mask) {
    mCurrentDepthTest = depth_test;
    mCurrentDepthMask = depth_mask;
}

void GfxRenderingAPIDX11::SetCurrentPrimDepth(float depth) {
    if (depth != mCurrentPrimDepth) {
        mCurrentPrimDepth = depth;
        mPrimDepthDirty = true;
    }
}

void GfxRenderingAPIDX11::SetZmodeDecal(bool zmode_decal) {
    mCurrentZmodeDecal = zmode_decal;
}

void GfxRenderingAPIDX11::SetViewport(int x, int y, int width, int height) {
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = x;
    viewport.TopLeftY = mRenderTargetHeight - y - height;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    mContext->RSSetViewports(1, &viewport);
}

void GfxRenderingAPIDX11::SetScissor(int x, int y, int width, int height) {
    D3D11_RECT rect;
    rect.left = x;
    rect.top = mRenderTargetHeight - y - height;
    rect.right = x + width;
    rect.bottom = mRenderTargetHeight - y;

    mContext->RSSetScissorRects(1, &rect);
}

void GfxRenderingAPIDX11::SetUseAlpha(bool use_alpha) {
    // Already part of the pipeline state from shader info
}

static D3D11_BLEND BlendExFactor(uint32_t factor) {
    switch (factor) {
        case G_BLENDEX_ZERO:
            return D3D11_BLEND_ZERO;
        case G_BLENDEX_ONE:
            return D3D11_BLEND_ONE;
        case G_BLENDEX_SRC_COLOR:
            return D3D11_BLEND_SRC_COLOR;
        case G_BLENDEX_ONE_MINUS_SRC_COLOR:
            return D3D11_BLEND_INV_SRC_COLOR;
        case G_BLENDEX_SRC_ALPHA:
            return D3D11_BLEND_SRC_ALPHA;
        case G_BLENDEX_ONE_MINUS_SRC_ALPHA:
            return D3D11_BLEND_INV_SRC_ALPHA;
        case G_BLENDEX_DST_COLOR:
            return D3D11_BLEND_DEST_COLOR;
        case G_BLENDEX_ONE_MINUS_DST_COLOR:
            return D3D11_BLEND_INV_DEST_COLOR;
        case G_BLENDEX_DST_ALPHA:
            return D3D11_BLEND_DEST_ALPHA;
        case G_BLENDEX_ONE_MINUS_DST_ALPHA:
            return D3D11_BLEND_INV_DEST_ALPHA;
        case G_BLENDEX_CONSTANT_COLOR:
        case G_BLENDEX_CONSTANT_ALPHA: // (D3D11 has one blend factor; the constant's alpha is copied to all four)
            return D3D11_BLEND_BLEND_FACTOR;
        case G_BLENDEX_ONE_MINUS_CONSTANT_COLOR:
        case G_BLENDEX_ONE_MINUS_CONSTANT_ALPHA:
            return D3D11_BLEND_INV_BLEND_FACTOR;
        default:
            return D3D11_BLEND_ONE;
    }
}

static D3D11_BLEND_OP BlendExOp(uint32_t eq) {
    switch (eq) {
        case G_BLENDEX_EQ_SUBTRACT:
            return D3D11_BLEND_OP_SUBTRACT;
        case G_BLENDEX_EQ_REVERSE_SUBTRACT:
            return D3D11_BLEND_OP_REV_SUBTRACT;
        case G_BLENDEX_EQ_MIN:
            return D3D11_BLEND_OP_MIN;
        case G_BLENDEX_EQ_MAX:
            return D3D11_BLEND_OP_MAX;
        default:
            return D3D11_BLEND_OP_ADD;
    }
}

void GfxRenderingAPIDX11::SetBlendMode(uint32_t mode, const uint8_t constantColor[4]) {
    mCurrentBlendMode = mode;
    const uint32_t src = (mode >> 8) & 0xF;
    const uint32_t dst = (mode >> 4) & 0xF;
    const bool alphaConstant = src == G_BLENDEX_CONSTANT_ALPHA || src == G_BLENDEX_ONE_MINUS_CONSTANT_ALPHA ||
                               dst == G_BLENDEX_CONSTANT_ALPHA || dst == G_BLENDEX_ONE_MINUS_CONSTANT_ALPHA;
    for (int i = 0; i < 4; i++) {
        mCurrentBlendFactor[i] = (alphaConstant ? constantColor[3] : constantColor[i]) / 255.0f;
    }
    if (mode != 0 && mBlendModeStates.find(mode) == mBlendModeStates.end()) {
        D3D11_BLEND_DESC blend_desc;
        ZeroMemory(&blend_desc, sizeof(D3D11_BLEND_DESC));
        blend_desc.RenderTarget[0].BlendEnable = true;
        blend_desc.RenderTarget[0].SrcBlend = BlendExFactor(src);
        blend_desc.RenderTarget[0].DestBlend = BlendExFactor(dst);
        blend_desc.RenderTarget[0].BlendOp = BlendExOp(mode & 0xF);
        // (the framebuffer's alpha stays as the usual blending leaves it)
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ThrowIfFailed(mDevice->CreateBlendState(&blend_desc, mBlendModeStates[mode].GetAddressOf()));
    }
}

void GfxRenderingAPIDX11::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {

    if (mLastDepthTest != mCurrentDepthTest || mLastDepthMask != mCurrentDepthMask) {
        mLastDepthTest = mCurrentDepthTest;
        mLastDepthMask = mCurrentDepthMask;

        mDepthStencilState.Reset();

        D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
        ZeroMemory(&depth_stencil_desc, sizeof(D3D11_DEPTH_STENCIL_DESC));

        depth_stencil_desc.DepthEnable = mCurrentDepthTest || mCurrentDepthMask;
        depth_stencil_desc.DepthWriteMask =
            mCurrentDepthMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        depth_stencil_desc.DepthFunc = mCurrentDepthTest
                                           ? (mCurrentZmodeDecal ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_LESS)
                                           : D3D11_COMPARISON_ALWAYS;
        depth_stencil_desc.StencilEnable = false;

        ThrowIfFailed(mDevice->CreateDepthStencilState(&depth_stencil_desc, mDepthStencilState.GetAddressOf()));
        mContext->OMSetDepthStencilState(mDepthStencilState.Get(), 0);
    }

    if (mLastZmodeDecal != mCurrentZmodeDecal) {
        mLastZmodeDecal = mCurrentZmodeDecal;

        mRasterizerState.Reset();

        D3D11_RASTERIZER_DESC rasterizer_desc;
        ZeroMemory(&rasterizer_desc, sizeof(D3D11_RASTERIZER_DESC));

        rasterizer_desc.FillMode = D3D11_FILL_SOLID;
        rasterizer_desc.CullMode = D3D11_CULL_NONE;
        rasterizer_desc.FrontCounterClockwise = true;
        rasterizer_desc.DepthBias = 0;
        // SSDB = SlopeScaledDepthBias 120 leads to -2 at 240p which is the same as N64 mode which has very little
        // fighting
        const int n64modeFactor = 120;
        const int noVanishFactor = 100;
        float SSDB = -2;

        switch (Ship::Context::GetRawInstance()->GetConsoleVariables()->GetInteger(CVAR_Z_FIGHTING_MODE, 0)) {
            case 1: // scaled z-fighting (N64 mode like)
                SSDB = -1.0f * (float)mRenderTargetHeight / n64modeFactor;
                break;
            case 2: // no vanishing paths
                SSDB = -1.0f * (float)mRenderTargetHeight / noVanishFactor;
                break;
            case 0: // disabled
            default:
                SSDB = -2;
        }
        rasterizer_desc.SlopeScaledDepthBias = mCurrentZmodeDecal ? SSDB : 0.0f;
        rasterizer_desc.DepthBiasClamp = 0.0f;
        // (MergeShip2 real-time shadows: only the shadow map's drawing may cut at the near side, StartShadowMap)
        rasterizer_desc.DepthClipEnable = mShadowMapDrawing && mShadowMapClip;
        rasterizer_desc.ScissorEnable = true;
        rasterizer_desc.MultisampleEnable = false;
        rasterizer_desc.AntialiasedLineEnable = false;

        ThrowIfFailed(mDevice->CreateRasterizerState(&rasterizer_desc, mRasterizerState.GetAddressOf()));
        mContext->RSSetState(mRasterizerState.Get());
    }

    bool textures_changed = false;

    for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
        if (mShaderProgram->usedTextures[i]) {
            // mTextures is append-only (NewTexture just resizes +1, DeleteTexture is a no-op),
            // so this is really just catching stale IDs left over from before we zero-initialized
            // mCurrentTextureIds. No entries are ever removed, so gaps aren't a concern.
            if (mCurrentTextureIds[i] >= mTextures.size()) {
                continue;
            }
            if (mLastResourceViews[i].Get() != mTextures[mCurrentTextureIds[i]].resource_view.Get()) {
                mLastResourceViews[i] = mTextures[mCurrentTextureIds[i]].resource_view.Get();
                mContext->PSSetShaderResources(i, 1, mTextures[mCurrentTextureIds[i]].resource_view.GetAddressOf());

                if (mCurrentFilterMode == FILTER_THREE_POINT) {
                    mPerDrawCbData.mTextures[i].width = mTextures[mCurrentTextureIds[i]].width;
                    mPerDrawCbData.mTextures[i].height = mTextures[mCurrentTextureIds[i]].height;
                    mPerDrawCbData.mTextures[i].linear_filtering = mTextures[mCurrentTextureIds[i]].linear_filtering;
                    textures_changed = true;
                }

                if (mLastSamplerStates[i].Get() != mTextures[mCurrentTextureIds[i]].sampler_state.Get()) {
                    mLastSamplerStates[i] = mTextures[mCurrentTextureIds[i]].sampler_state.Get();
                }
            }
            mContext->PSSetSamplers(i, 1, mTextures[mCurrentTextureIds[i]].sampler_state.GetAddressOf());
        }
    }

    // Set per-draw constant buffer
    if (textures_changed) {
        D3D11_MAPPED_SUBRESOURCE ms;
        ZeroMemory(&ms, sizeof(D3D11_MAPPED_SUBRESOURCE));
        mContext->Map(mPerDrawCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, &mPerDrawCbData, sizeof(PerDrawCB));
        mContext->Unmap(mPerDrawCb.Get(), 0);
    }

    // G_ZS_PRIM: upload prim_depth cbuffer when it changed
    if (mPrimDepthDirty) {
        mPerPrimDepthCbData.prim_depth = mCurrentPrimDepth;
        D3D11_MAPPED_SUBRESOURCE ms;
        ZeroMemory(&ms, sizeof(D3D11_MAPPED_SUBRESOURCE));
        mContext->Map(mPerPrimDepthCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
        memcpy(ms.pData, &mPerPrimDepthCbData, sizeof(PerPrimDepthCB));
        mContext->Unmap(mPerPrimDepthCb.Get(), 0);
        mPrimDepthDirty = false;
    }

    // Set vertex buffer data

    D3D11_MAPPED_SUBRESOURCE ms;
    ZeroMemory(&ms, sizeof(D3D11_MAPPED_SUBRESOURCE));
    mContext->Map(mVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, buf_vbo, buf_vbo_len * sizeof(float));
    mContext->Unmap(mVertexBuffer.Get(), 0);

    uint32_t stride = mShaderProgram->numFloats * sizeof(float);
    uint32_t offset = 0;

    if (mLastVertexBufferStride != stride) {
        mLastVertexBufferStride = stride;
        mContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), &stride, &offset);
    }

    if (mLastShaderProgram != mShaderProgram) {
        mLastShaderProgram = mShaderProgram;
        mContext->IASetInputLayout(mShaderProgram->input_layout.Get());
        mContext->VSSetShader(mShaderProgram->vertex_shader.Get(), 0, 0);
        mContext->PSSetShader(mShaderProgram->pixel_shader.Get(), 0, 0);
    }

    // The program's blend state, or the explicit one G_SETBLENDMODE_EX asked for (MergeShip2)
    ID3D11BlendState* blendState = mShaderProgram->blend_state.Get();
    const bool explicitBlend = mCurrentBlendMode != 0;
    if (explicitBlend) {
        blendState = mBlendModeStates[mCurrentBlendMode].Get();
    }
    if (mLastBlendState.Get() != blendState ||
        (explicitBlend && memcmp(mLastBlendFactor, mCurrentBlendFactor, sizeof(mLastBlendFactor)) != 0)) {
        mLastBlendState = blendState;
        memcpy(mLastBlendFactor, mCurrentBlendFactor, sizeof(mLastBlendFactor));
        mContext->OMSetBlendState(blendState, explicitBlend ? mCurrentBlendFactor : nullptr, 0xFFFFFFFF);
    }

    if (mLastPrimitaveTopology != D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
        mLastPrimitaveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        mContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    mContext->Draw(buf_vbo_num_tris * 3, 0);
}

void GfxRenderingAPIDX11::OnResize() {
    // create_render_target_views(true);
}

void GfxRenderingAPIDX11::StartFrame() {
    // Set per-frame constant buffer
    ID3D11Buffer* buffers[3] = { mPerFrameCb.Get(), mPerDrawCb.Get(), mPerPrimDepthCb.Get() };
    mContext->PSSetConstantBuffers(0, 3, buffers);

    mPerFrameCbData.noise_frame++;
    if (mPerFrameCbData.noise_frame > 150) {
        // No high values, as noise starts to look ugly
        mPerFrameCbData.noise_frame = 0;
    }

    // (MergeShip2 real-time shadows: the map is let go once nothing has been drawn into it for a while)
    if (mShadowMap.Get() != nullptr && ++mShadowMapIdle > 600) {
        mShadowMapSrv.Reset();
        mShadowMapDsv.Reset();
        mShadowMap.Reset();
        mShadowMapSize = 0;
    }
    // (the same for the picture's copy, and for the pictures the glow is made in)
    if (mSceneCopy.Get() != nullptr && ++mSceneCopyIdle > 600) {
        mSceneCopySrv.Reset();
        mSceneCopy.Reset();
        mSceneCopyWidth = mSceneCopyHeight = 0;
    }
    if (mPostSoft[0].texture.Get() != nullptr && ++mPostIdle > 600) {
        for (PostTarget& each : mPostSoft) {
            each = PostTarget();
        }
        for (PostTarget& each : mPostGlow) {
            each = PostTarget();
        }
        mPostWidth = mPostHeight = 0;
    }
}

void GfxRenderingAPIDX11::EndFrame() {
    mContext->Flush();
}

void GfxRenderingAPIDX11::FinishRender() {
}

int GfxRenderingAPIDX11::CreateFramebuffer() {
    uint32_t texture_id = NewTexture();
    TextureData& t = mTextures[texture_id];

    size_t index = mFrameBuffers.size();
    mFrameBuffers.resize(mFrameBuffers.size() + 1);
    FramebufferDX11& data = mFrameBuffers.back();
    data.texture_id = texture_id;

    uint32_t tile = 0;
    uint32_t saved = mCurrentTextureIds[tile];
    mCurrentTextureIds[tile] = texture_id;
    SetSamplerParameters(0, true, G_TX_WRAP, G_TX_WRAP);
    mCurrentTextureIds[tile] = saved;

    return (int)index;
}

void GfxRenderingAPIDX11::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                                      bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                                      bool can_extract_depth) {
    FramebufferDX11& fb = mFrameBuffers[fb_id];
    TextureData& tex = mTextures[fb.texture_id];

    width = ((width) > (1U) ? (width) : (1U));
    height = ((height) > (1U) ? (height) : (1U));
    // We can't use MSAA the way we are using it on feature level 10_0 hardware, so disable it altogether.
    msaa_level = mFeatureLevel < D3D_FEATURE_LEVEL_10_1 ? 1 : msaa_level;
    while (msaa_level > 1 && mMsaaNumQualityLevels[msaa_level - 1] == 0) {
        --msaa_level;
    }

    bool diff = tex.width != width || tex.height != height || fb.msaa_level != msaa_level;

    if (diff || (fb.render_target_view.Get() != nullptr) != render_target) {
        if (fb_id != 0) {
            D3D11_TEXTURE2D_DESC texture_desc;
            texture_desc.Width = width;
            texture_desc.Height = height;
            texture_desc.Usage = D3D11_USAGE_DEFAULT;
            texture_desc.BindFlags =
                (msaa_level <= 1 ? D3D11_BIND_SHADER_RESOURCE : 0) | (render_target ? D3D11_BIND_RENDER_TARGET : 0);
            texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture_desc.CPUAccessFlags = 0;
            texture_desc.MiscFlags = 0;
            texture_desc.ArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.SampleDesc.Count = msaa_level;
            texture_desc.SampleDesc.Quality = 0;

            ThrowIfFailed(mDevice->CreateTexture2D(&texture_desc, nullptr, tex.texture.ReleaseAndGetAddressOf()));

            if (msaa_level <= 1) {
                ThrowIfFailed(mDevice->CreateShaderResourceView(tex.texture.Get(), nullptr,
                                                                tex.resource_view.ReleaseAndGetAddressOf()));
            }
        } else if (diff || (render_target && tex.texture.Get() == nullptr)) {
            DXGI_SWAP_CHAIN_DESC1 desc1;
            IDXGISwapChain1* swap_chain = mWindowBackend->GetSwapChain();
            ThrowIfFailed(swap_chain->GetDesc1(&desc1));
            if (desc1.Width != width || desc1.Height != height) {
                fb.render_target_view.Reset();
                tex.texture.Reset();
                ThrowIfFailed(swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, desc1.Flags));
            }
            ThrowIfFailed(
                swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)tex.texture.ReleaseAndGetAddressOf()));
        }
        if (render_target) {
            ThrowIfFailed(mDevice->CreateRenderTargetView(tex.texture.Get(), nullptr,
                                                          fb.render_target_view.ReleaseAndGetAddressOf()));
        }

        tex.width = width;
        tex.height = height;
    }

    if (has_depth_buffer &&
        (diff || !fb.has_depth_buffer || (fb.depth_stencil_srv.Get() != nullptr) != can_extract_depth)) {
        fb.depth_stencil_srv.Reset();
        CreateDepthStencilObjects(width, height, msaa_level, fb.depth_stencil_view.ReleaseAndGetAddressOf(),
                                  can_extract_depth ? fb.depth_stencil_srv.GetAddressOf() : nullptr);
    }
    if (!has_depth_buffer) {
        fb.depth_stencil_view.Reset();
        fb.depth_stencil_srv.Reset();
    }

    fb.has_depth_buffer = has_depth_buffer;
    fb.msaa_level = msaa_level;
}

void GfxRenderingAPIDX11::StartDrawToFramebuffer(int fb_id, float noise_scale) {
    FramebufferDX11& fb = mFrameBuffers[fb_id];
    mRenderTargetHeight = mTextures[fb.texture_id].height;

    mContext->OMSetRenderTargets(1, fb.render_target_view.GetAddressOf(),
                                 fb.has_depth_buffer ? fb.depth_stencil_view.Get() : nullptr);

    mCurrentFramebuffer = fb_id;
    if (mShadowMapDrawing) {
        // (MergeShip2 real-time shadows: the shadow map is done, and the rasterizer state is the usual one again)
        mShadowMapDrawing = false;
        mLastZmodeDecal = -1;
    }

    if (noise_scale != 0.0f) {
        mPerFrameCbData.noise_scale = 1.0f / noise_scale;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    ZeroMemory(&ms, sizeof(D3D11_MAPPED_SUBRESOURCE));
    mContext->Map(mPerFrameCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, &mPerFrameCbData, sizeof(PerFrameCB));
    mContext->Unmap(mPerFrameCb.Get(), 0);
}

void GfxRenderingAPIDX11::ClearFramebuffer(bool color, bool depth) {
    FramebufferDX11& fb = mFrameBuffers[mCurrentFramebuffer];
    if (color) {
        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        mContext->ClearRenderTargetView(fb.render_target_view.Get(), clearColor);
    }
    if (depth && fb.has_depth_buffer) {
        mContext->ClearDepthStencilView(fb.depth_stencil_view.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

void GfxRenderingAPIDX11::ResolveMSAAColorBuffer(int fb_id_target, int fb_id_source) {
    FramebufferDX11& fb_dst = mFrameBuffers[fb_id_target];
    FramebufferDX11& fb_src = mFrameBuffers[fb_id_source];

    mContext->ResolveSubresource(mTextures[fb_dst.texture_id].texture.Get(), 0,
                                 mTextures[fb_src.texture_id].texture.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
}

void* GfxRenderingAPIDX11::GetFramebufferTextureId(int fb_id) {
    return (void*)mTextures[mFrameBuffers[fb_id].texture_id].resource_view.Get();
}

void GfxRenderingAPIDX11::SelectTextureFb(int fbID) {
    int tile = 0;
    SelectTexture(tile, mFrameBuffers[fbID].texture_id);
}

void GfxRenderingAPIDX11::CopyFramebuffer(int fb_dst_id, int fb_src_id, int srcX0, int srcY0, int srcX1, int srcY1,
                                          int dstX0, int dstY0, int dstX1, int dstY1) {
    if (fb_src_id >= (int)mFrameBuffers.size() || fb_dst_id >= (int)mFrameBuffers.size()) {
        return;
    }

    FramebufferDX11& fb_dst = mFrameBuffers[fb_dst_id];
    FramebufferDX11& fb_src = mFrameBuffers[fb_src_id];

    TextureData& td_dst = mTextures[fb_dst.texture_id];
    TextureData& td_src = mTextures[fb_src.texture_id];

    // Textures are the same size so we can do a direct copy or resolve
    if (td_src.height == td_dst.height && td_src.width == td_dst.width) {
        if (fb_src.msaa_level <= 1) {
            mContext->CopyResource(td_dst.texture.Get(), td_src.texture.Get());
        } else {
            mContext->ResolveSubresource(td_dst.texture.Get(), 0, td_src.texture.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        }
        return;
    }

    if (srcY1 > (int)td_src.height || srcX1 > (int)td_src.width || srcX0 < 0 || srcY0 < 0 ||
        dstY1 > (int)td_dst.height || dstX1 > (int)td_dst.width || dstX0 < 0 || dstY0 < 0) {
        // Using a source region larger than the source resource or copy outside of the destination resource is
        // considered undefined behavior and could lead to removal of the rendering mDevice
        return;
    }

    D3D11_BOX region;
    region.left = srcX0;
    region.right = srcX1;
    region.top = srcY0;
    region.bottom = srcY1;
    region.front = 0;
    region.back = 1;

    // We can't region copy a multi-sample texture to a single sample texture
    if (fb_src.msaa_level <= 1) {
        mContext->CopySubresourceRegion(td_dst.texture.Get(), dstX0, dstY0, 0, 0, td_src.texture.Get(), 0, &region);
    } else {
        // Setup a temporary texture
        TextureData td_resolved;
        td_resolved.width = td_src.width;
        td_resolved.height = td_src.height;

        D3D11_TEXTURE2D_DESC texture_desc;
        texture_desc.Width = td_src.width;
        texture_desc.Height = td_src.height;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.CPUAccessFlags = 0;
        texture_desc.MiscFlags = 0;
        texture_desc.ArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;

        ThrowIfFailed(mDevice->CreateTexture2D(&texture_desc, nullptr, td_resolved.texture.GetAddressOf()));

        // Resolve multi-sample to temporary
        mContext->ResolveSubresource(td_resolved.texture.Get(), 0, td_src.texture.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        // Then copy the region to the destination
        mContext->CopySubresourceRegion(td_dst.texture.Get(), dstX0, dstY0, 0, 0, td_resolved.texture.Get(), 0,
                                        &region);
    }
}

void GfxRenderingAPIDX11::ReadFramebufferToCPU(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
    if (fb_id >= (int)mFrameBuffers.size()) {
        return;
    }

    FramebufferDX11& fb = mFrameBuffers[fb_id];
    TextureData& td = mTextures[fb.texture_id];

    // Query actual texture dimensions — CopyResource requires matching sizes
    D3D11_TEXTURE2D_DESC srcDesc;
    td.texture->GetDesc(&srcDesc);

    // Reuse cached staging texture when dimensions match — avoids per-frame CreateTexture2D
    if (!mReadbackStaging || mReadbackStagingW != srcDesc.Width || mReadbackStagingH != srcDesc.Height) {
        mReadbackStaging.Reset();

        D3D11_TEXTURE2D_DESC texture_desc;
        texture_desc.Width = srcDesc.Width;
        texture_desc.Height = srcDesc.Height;
        texture_desc.Usage = D3D11_USAGE_STAGING;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        texture_desc.BindFlags = 0;
        texture_desc.MiscFlags = 0;
        texture_desc.ArraySize = 1;
        texture_desc.MipLevels = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.SampleDesc.Quality = 0;

        ThrowIfFailed(mDevice->CreateTexture2D(&texture_desc, nullptr, mReadbackStaging.GetAddressOf()));
        mReadbackStagingW = srcDesc.Width;
        mReadbackStagingH = srcDesc.Height;
    }

    // Copy the framebuffer texture to the staging texture
    mContext->CopyResource(mReadbackStaging.Get(), td.texture.Get());

    // Map the staging texture to a resource that we can read
    D3D11_MAPPED_SUBRESOURCE resource = {};
    ThrowIfFailed(mContext->Map(mReadbackStaging.Get(), 0, D3D11_MAP_READ, 0, &resource));

    if (!resource.pData) {
        mContext->Unmap(mReadbackStaging.Get(), 0);
        return;
    }

    // Convert RGBA32 → RGBA16 with nearest-neighbor scaling from actual texture
    // dimensions to requested output dimensions, respecting RowPitch for row stride
    for (uint32_t j = 0; j < height; j++) {
        uint32_t srcY = j * srcDesc.Height / height;
        uint8_t* srcRow = (uint8_t*)resource.pData + srcY * resource.RowPitch;
        for (uint32_t i = 0; i < width; i++) {
            uint32_t srcX = i * srcDesc.Width / width;
            uint32_t pixel = ((uint32_t*)srcRow)[srcX];
            uint8_t r = (((pixel & 0xFF) + 4) * 0x1F) / 0xFF;
            uint8_t g = ((((pixel >> 8) & 0xFF) + 4) * 0x1F) / 0xFF;
            uint8_t b = ((((pixel >> 16) & 0xFF) + 4) * 0x1F) / 0xFF;
            uint8_t a = ((pixel >> 24) & 0xFF) ? 1 : 0;

            rgba16_buf[i + (j * width)] = (r << 11) | (g << 6) | (b << 1) | a;
        }
    }

    mContext->Unmap(mReadbackStaging.Get(), 0);
}

void GfxRenderingAPIDX11::SetTextureFilter(FilteringMode mode) {
    mCurrentFilterMode = mode;
    gfx_texture_cache_clear();
}

FilteringMode GfxRenderingAPIDX11::GetTextureFilter() {
    return mCurrentFilterMode;
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIDX11::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    FramebufferDX11& fb = mFrameBuffers[fb_id];
    TextureData& td = mTextures[fb.texture_id];

    if (coordinates.size() > mCoordBufferSize) {
        mCoordBuffer.Reset();
        mCoordBufferSrv.Reset();
        mDepthValueOutputBuffer.Reset();
        mDepthValueOutputUav.Reset();
        mDepthValueOutputBufferCopy.Reset();

        D3D11_BUFFER_DESC coord_buf_desc;
        coord_buf_desc.Usage = D3D11_USAGE_DYNAMIC;
        coord_buf_desc.ByteWidth = sizeof(Coord) * coordinates.size();
        coord_buf_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        coord_buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        coord_buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        coord_buf_desc.StructureByteStride = sizeof(Coord);

        ThrowIfFailed(mDevice->CreateBuffer(&coord_buf_desc, nullptr, mCoordBuffer.GetAddressOf()));

        D3D11_SHADER_RESOURCE_VIEW_DESC coord_buf_srv_desc;
        coord_buf_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        coord_buf_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        coord_buf_srv_desc.Buffer.FirstElement = 0;
        coord_buf_srv_desc.Buffer.NumElements = coordinates.size();

        ThrowIfFailed(
            mDevice->CreateShaderResourceView(mCoordBuffer.Get(), &coord_buf_srv_desc, mCoordBufferSrv.GetAddressOf()));

        D3D11_BUFFER_DESC output_buffer_desc;
        output_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        output_buffer_desc.ByteWidth = sizeof(float) * coordinates.size();
        output_buffer_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        output_buffer_desc.CPUAccessFlags = 0;
        output_buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        output_buffer_desc.StructureByteStride = sizeof(float);
        ThrowIfFailed(mDevice->CreateBuffer(&output_buffer_desc, nullptr, mDepthValueOutputBuffer.GetAddressOf()));

        D3D11_UNORDERED_ACCESS_VIEW_DESC output_buffer_uav_desc;
        output_buffer_uav_desc.Format = DXGI_FORMAT_UNKNOWN;
        output_buffer_uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        output_buffer_uav_desc.Buffer.FirstElement = 0;
        output_buffer_uav_desc.Buffer.NumElements = coordinates.size();
        output_buffer_uav_desc.Buffer.Flags = 0;
        ThrowIfFailed(mDevice->CreateUnorderedAccessView(mDepthValueOutputBuffer.Get(), &output_buffer_uav_desc,
                                                         mDepthValueOutputUav.GetAddressOf()));

        output_buffer_desc.Usage = D3D11_USAGE_STAGING;
        output_buffer_desc.BindFlags = 0;
        output_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ThrowIfFailed(mDevice->CreateBuffer(&output_buffer_desc, nullptr, mDepthValueOutputBufferCopy.GetAddressOf()));

        mCoordBufferSize = coordinates.size();
    }

    D3D11_MAPPED_SUBRESOURCE ms;

    if (fb.msaa_level > 1 && mComputeShaderMsaa.Get() == nullptr) {
        ThrowIfFailed(mDevice->CreateComputeShader(mComputeShaderMsaaBlob->GetBufferPointer(),
                                                   mComputeShaderMsaaBlob->GetBufferSize(), nullptr,
                                                   mComputeShaderMsaa.GetAddressOf()));
    }

    // ImGui overwrites these values, so we cannot set them once at init
    mContext->CSSetShader(fb.msaa_level > 1 ? mComputeShaderMsaa.Get() : mComputeShader.Get(), nullptr, 0);
    mContext->CSSetUnorderedAccessViews(0, 1, mDepthValueOutputUav.GetAddressOf(), nullptr);

    ThrowIfFailed(mContext->Map(mCoordBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms));
    Coord* coord_cb = (Coord*)ms.pData;
    {
        size_t i = 0;
        for (const auto& coord : coordinates) {
            coord_cb[i].x = coord.first;
            // We invert y because the gfx_pc assumes OpenGL coordinates (bottom-left corner is origin), while DX's
            // origin is top-left corner
            coord_cb[i].y = td.height - 1 - coord.second;
            ++i;
        }
    }
    mContext->Unmap(mCoordBuffer.Get(), 0);

    // The depth stencil texture can only have one mapping at a time, so unbind from the OM
    ID3D11RenderTargetView* null_arr1[1] = { nullptr };
    mContext->OMSetRenderTargets(1, null_arr1, nullptr);

    ID3D11ShaderResourceView* srvs[2] = { fb.depth_stencil_srv.Get(), mCoordBufferSrv.Get() };
    mContext->CSSetShaderResources(0, 2, srvs);

    mContext->Dispatch(coordinates.size(), 1, 1);

    mContext->CopyResource(mDepthValueOutputBufferCopy.Get(), mDepthValueOutputBuffer.Get());
    ThrowIfFailed(mContext->Map(mDepthValueOutputBufferCopy.Get(), 0, D3D11_MAP_READ, 0, &ms));
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;
    {
        size_t i = 0;
        for (const auto& coord : coordinates) {
            res.emplace(coord, ((float*)ms.pData)[i++] * 65532.0f);
        }
    }
    mContext->Unmap(mDepthValueOutputBufferCopy.Get(), 0);

    ID3D11ShaderResourceView* null_arr[2] = { nullptr, nullptr };
    mContext->CSSetShaderResources(0, 2, null_arr);

    return res;
}

ImTextureID GfxRenderingAPIDX11::GetTextureById(int id) {
    return mTextures[id].resource_view.Get();
}

void GfxRenderingAPIDX11::SetSrgbMode() {
    mSrgbMode = true;
}

// MergeShip2 real-time shadows (gfx_rendering_api.h, ApplyShadows). One triangle over the whole framebuffer: each
// pixel's place in the world is worked out from the framebuffer's depth, taken into the light's view, and held
// against the depth the light saw there; the pixel is multiplied darker where something stands between. How the
// surface faces is read off the pixel's neighbours (no material's shader needs to know of any of this, so the
// game's own models, the 3DS ones and anything a pack draws are shadowed alike).
static const char* sShadowShaderSource = R"(
cbuffer ShadowCB : register(b0) {
    row_major float4x4 cameraInverse;
    row_major float4x4 light;
    float4 lightDir; // xyz: the way to the light
    float4 params;   // x: shade in shadow, y: shade turned away from the light, z: depth bias, w: the map's size
    float4 soft;     // x: a texel of the map in the world's units, y: the light's whole depth in them, z: how much
                     // wider a shadow's edge grows per unit between what casts it and what it falls on, w: the
                     // widest edge, in texels
    float4 reach;    // x, y: (a light that comes down from a level plane) a shadow fades away between these
                     // distances from what casts it (y of 0: it does not fade); z: 1 where there is a shadow map
                     // (0: no cast shadows, only what follows); w: the wind's clock, seconds
    float4 air;      // x: how dark the ambient shading goes (0: none), y: how far it looks, in the world's units;
                     // z: how dark a cloud's shadow is (0: no clouds), w: how large a cloud is, in the world's units
    float4 tint;     // rgb: what a shadow leaves of each colour, against its plain shade (1, 1, 1: grey);
                     // w: how fast the clouds go, units a second
    float4 sun;      // rgb: how much brighter what the light reaches is drawn (1, 1, 1: as it is)
    float4 view;     // the part of the framebuffer the world is drawn into: left, top, width, height (pixels)
    float4 lampInfo; // x: how many lamps follow, y: how strongly their light is drawn
    float4 lamps[24]; // two a lamp: xyz its place and w how far it reaches, then rgb its colour (0 to 1)
};
#if MSAA
Texture2DMS<float> g_depth : register(t0);
#else
Texture2D<float> g_depth : register(t0);
#endif
Texture2D<float> g_shadow : register(t1);
#if READS
// The picture as it is (one sample a pixel). With it the pass writes the finished colour; without, it darkens and
// brightens what is there by blending, which cannot brighten past twice. READS is set where there are lamps.
Texture2D<float4> g_scene : register(t2);
#endif
SamplerComparisonState g_compare : register(s0);

static const float2 kDisk[16] = {
    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.09418410, -0.92938870),
    float2(0.34495938, 0.29387760),   float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543, 0.27676845),  float2(0.97484398, 0.75648379),  float2(0.44323325, -0.97511554),
    float2(0.53742981, -0.47373420),  float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    float2(-0.24188840, 0.99706507),  float2(-0.81409955, 0.91437590), float2(0.19984126, 0.78641367),
    float2(0.14383161, -0.14100790)
};

struct VSOut {
    float4 pos : SV_Position;
};

VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float SceneDepth(int2 p) {
    p = clamp(p, int2(view.xy), int2(view.xy + view.zw) - int2(1, 1));
#if MSAA
    return g_depth.Load(p, 0);
#else
    return g_depth.Load(int3(p, 0));
#endif
}

float3 World(int2 p, float depth) {
    float2 uv = (float2(p) + float2(0.5, 0.5) - view.xy) / view.zw;
    float4 at = mul(float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth * 2.0 - 1.0, 1.0), cameraInverse);
    return at.xyz / at.w;
}

// (clouds: a soft pattern that is the same every time for the same place)
float Hash(float2 p) {
    float3 q = frac(float3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return frac((q.x + q.y) * q.z);
}

float Noise(float2 p) {
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(Hash(i), Hash(i + float2(1.0, 0.0)), f.x),
                lerp(Hash(i + float2(0.0, 1.0)), Hash(i + float2(1.0, 1.0)), f.x), f.y);
}

// How the surface at a pixel faces: from its neighbours on the screen, of each pair the one nearer in depth (the
// other may be across an edge).
float3 Facing(int2 p, float depth, float3 world) {
    float dl = SceneDepth(p + int2(-1, 0));
    float dr = SceneDepth(p + int2(1, 0));
    float du = SceneDepth(p + int2(0, -1));
    float dd = SceneDepth(p + int2(0, 1));
    float3 dx = abs(dl - depth) < abs(dr - depth) ? world - World(p + int2(-1, 0), dl) : World(p + int2(1, 0), dr) - world;
    float3 dy = abs(du - depth) < abs(dd - depth) ? world - World(p + int2(0, -1), du) : World(p + int2(0, 1), dd) - world;
    float3 normal = cross(dy, dx);
    float len = length(normal);
    normal = len > 1e-12 ? normal / len : lightDir.xyz;
    float4 eye = mul(float4(0.0, 0.0, -1.0, 1.0), cameraInverse);
    if (dot(normal, world - eye.xyz / eye.w) > 0.0) {
        normal = -normal; // (towards the eye)
    }
    return normal;
}

// The game's lamps (torches, fires, fairies, spells): how much brighter each colour of a surface is for them (0:
// none reaches it). The game's own radius for a lamp is how far from it a character is lit; a wall is lit a good
// way further, by a light that falls off as light does: fast near the flame, with a long faint reach. A lamp of a
// wide radius is a strong one.
static const float kLampReach = 2.5; // how much further than the game's radius
static const float kLampGain = 3.0;  // how many times brighter a surface at one flame is, before the screen's top
float3 Lamps(float3 world, float3 normal) {
    float3 lamp = float3(0.0, 0.0, 0.0);
    for (int n = 0; n < 12; n++) {
        if ((float)n >= lampInfo.x) {
            break;
        }
        float game = lamps[n * 2].w;
        float far = clamp(game * kLampReach, 150.0, 900.0);
        float3 to = lamps[n * 2].xyz - world;
        float d2 = dot(to, to);
        float s2 = d2 / (far * far);
        if (s2 < 1.0) {
            float fall = (1.0 - s2) * (1.0 - s2) / (1.0 + 8.0 * s2);
            float faces = saturate(dot(normal, to * rsqrt(max(d2, 0.0001))) * 0.75 + 0.25);
            float3 colour = lamps[n * 2 + 1].rgb;
            // (a flame's light is orange, where the game colours it a plain yellow)
            colour.g -= 0.26 * saturate(min(colour.r, colour.g) - colour.b);
            lamp += colour * (fall * faces * clamp(game / 200.0, 0.35, 1.3));
        }
    }
    // (several lamps over one place add up to less than their sum: three fires do not whiten a floor)
    lamp /= float3(1.0, 1.0, 1.0) + 0.35 * lamp;
    return lamp * (kLampGain * lampInfo.y);
}

float4 PSMain(VSOut input) : SV_Target {
    int2 p = int2(input.pos.xy);
    float depth = SceneDepth(p);
    if (depth >= 0.9999995) {
        // (nothing drawn there: the sky)
#if READS
        return float4(g_scene.Load(int3(p, 0)).rgb, 1.0);
#else
        // (what is there is multiplied by twice this shader's colour: a half leaves it as it is)
        return float4(0.5, 0.5, 0.5, 1.0);
#endif
    }
    float3 world = World(p, depth);
    float3 normal = Facing(p, depth, world);
    float facing = dot(normal, lightDir.xyz);

    // (the look-ups' pattern is turned by a different angle on every pixel: bands become a fine grain)
    float turn = frac(52.9829189 * frac(dot(input.pos.xy, float2(0.06711056, 0.00583715)))) * 6.2831853;
    float2 rot = float2(cos(turn), sin(turn));

    // Ambient shading: how much of the sky a place sees. Where other surfaces stand close over it -- a corner, the
    // foot of a wall, the ground under a bush -- less light of any kind arrives, and it is darker.
    float open = 1.0;
    if (air.x > 0.0) {
        float aPixel = length(World(p + int2(1, 0), depth) - world); // (the world's units a pixel, at this depth)
        float around = clamp(air.y / max(aPixel, 1e-4), 3.0, 72.0);
        float shut = 0.0;
        for (int k = 0; k < 12; k++) {
            float2 o = float2(kDisk[k].x * rot.x - kDisk[k].y * rot.y, kDisk[k].x * rot.y + kDisk[k].y * rot.x) * around;
            int2 q = p + int2(o);
            float dq = SceneDepth(q);
            if (dq < 0.9999995) {
                float3 v = World(q, dq) - world;
                float vv = dot(v, v);
                float over = dot(v, normal) - 0.08 * sqrt(vv);
                shut += max(over, 0.0) / (vv + 0.01) * saturate(1.0 - vv / (air.y * air.y));
            }
        }
        open = saturate(1.0 - air.x * shut * air.y * (0.6 / 12.0));
    }

    // Cast shadows.
    float cast = 0.0;
    float away = 0.0;
    if (reach.z > 0.5) {
        // The place in the light's view, taken a little off the surface (more where the light grazes it): however
        // wide the look-up around it, the surface is not found standing in its own light.
        float3 lifted = world + normal * (soft.x * (1.0 + 2.5 * (1.0 - saturate(facing))));
        float4 into = mul(float4(lifted, 1.0), light);
        float3 at = into.xyz / into.w;
        float edge = max(abs(at.x), abs(at.y));
        if (edge < 1.0 && at.z < 1.0 && at.z > -1.0) {
            float mapSize = params.w;
            float2 suv = float2(at.x * 0.5 + 0.5, 0.5 - at.y * 0.5);
            float sd = at.z * 0.5 + 0.5;
            float slope = sqrt(saturate(1.0 - facing * facing)) / max(facing, 0.08);

            // What stands in the light around this place, and how far from it: a shadow's edge is as wide as that
            // is far (sharp at the feet of what casts it, soft far from it).
            float searchBias = params.z * (1.0 + min(slope, 4.0) * (0.5 + 0.4 * soft.w));
            float blockers = 0.0;
            float blockerDepth = 0.0;
            for (int i = 0; i < 12; i++) {
                float2 o = float2(kDisk[i].x * rot.x - kDisk[i].y * rot.y, kDisk[i].x * rot.y + kDisk[i].y * rot.x) * soft.w;
                int2 q = clamp(int2(suv * mapSize + o), int2(0, 0), int2(mapSize - 1.0, mapSize - 1.0));
                float d = g_shadow.Load(int3(q, 0));
                if (d < sd - searchBias) {
                    blockers += 1.0;
                    blockerDepth += d;
                }
            }
            float width = 1.0;
            float within = 1.0;
            if (blockers > 0.0) {
                float between = (sd - blockerDepth / blockers) * soft.y;
                width = clamp(between * soft.z / soft.x, 1.0, soft.w);
                if (reach.y > 0.0) {
                    within = 1.0 - smoothstep(reach.x, reach.y, between);
                }
            }
            float bias = params.z * (1.0 + min(slope, 4.0) * (0.5 + 0.4 * width));
            float lit = 0.0;
            for (int j = 0; j < 16; j++) {
                float2 o = float2(kDisk[j].x * rot.x - kDisk[j].y * rot.y, kDisk[j].x * rot.y + kDisk[j].y * rot.x) * width;
                lit += g_shadow.SampleCmpLevelZero(g_compare, suv + o / mapSize, sd - bias);
            }
            float fade = 1.0 - smoothstep(0.8, 1.0, edge);
            // a shadow is cast on what faces the light; what is turned away from it is in its own shadow, by its
            // facing
            cast = (1.0 - lit / 16.0) * within * smoothstep(0.0, 0.15, facing) * fade;
            away = smoothstep(0.3, -0.3, facing) * fade;
        }
    }

    // Clouds: their shadows drift over the land with the wind. They lie where the light meets a layer of cloud high
    // above, so they slide over hills as real ones do.
    float clouded = 0.0;
    if (air.z > 0.0) {
        float2 under = world.xz + lightDir.xz / max(lightDir.y, 0.25) * (2200.0 - world.y);
        float2 c = (under - float2(0.80, 0.60) * (reach.w * tint.w)) / air.w;
        float n = 0.55 * Noise(c) + 0.28 * Noise(c * 2.07 + 13.1) + 0.17 * Noise(c * 4.13 + 71.7);
        clouded = smoothstep(0.50, 0.70, n) * air.z * smoothstep(-0.1, 0.3, facing);
    }

    // what is left of the light's own share: a cast shadow takes it, a cloud takes part of it
    float lost = 1.0 - (1.0 - cast) * (1.0 - clouded);
    float3 shade = lerp(float3(1.0, 1.0, 1.0), params.x * tint.rgb, lost);
    float turned = lerp(1.0, params.y, away);
    shade = min(shade, float3(turned, turned, turned));
    // what the light does reach is a little brighter and warmer for it
    float reached = (1.0 - lost) * smoothstep(0.05, 0.6, facing);
    shade *= lerp(float3(1.0, 1.0, 1.0), sun.rgb, reached);

    shade *= open;
#if READS
    // With lamps about: the finished colour, from the picture as it is. What they make brighter than the screen can
    // show runs out softly towards its top and keeps its colour (a lit orange floor stays orange; with each colour
    // cut off at its top by itself it would turn yellow, then white). Where no lamp reaches nothing changes.
    float3 base = g_scene.Load(int3(p, 0)).rgb * shade;
    float3 lit = base * (float3(1.0, 1.0, 1.0) + Lamps(world, normal));
    float before = max(base.r, max(base.g, base.b));
    float peak = max(lit.r, max(lit.g, lit.b));
    if (peak > before + 0.0001) {
        float room = max(1.0 - before, 0.02);
        float over = (peak - before) / room;
        lit *= (before + room * over / (1.0 + over)) / peak;
    }
    return float4(saturate(lit), 1.0);
#else
    return float4(min(shade, float3(2.0, 2.0, 2.0)) * 0.5, 1.0);
#endif
}
)";

// The shadow map (gfx_rendering_api.h, StartShadowMap): a depth picture of the backend's own, with no colour beside
// it. At 4096 a side it is 32 MB, where a framebuffer of that size is 128.
bool GfxRenderingAPIDX11::StartShadowMap(int size, bool clip) {
    if (mShadowFailed || size < 64 || size > 8192 || mFeatureLevel < D3D_FEATURE_LEVEL_10_0) {
        return false;
    }
    if (mShadowMap.Get() == nullptr || mShadowMapSize != size) {
        mShadowMapSrv.Reset();
        mShadowMapDsv.Reset();
        mShadowMap.Reset();
        mShadowMapSize = 0;
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = (UINT)size;
        desc.Height = (UINT)size;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R16_TYPELESS;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        D3D11_DEPTH_STENCIL_VIEW_DESC dsv;
        ZeroMemory(&dsv, sizeof(dsv));
        dsv.Format = DXGI_FORMAT_D16_UNORM;
        dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        D3D11_SHADER_RESOURCE_VIEW_DESC srv;
        ZeroMemory(&srv, sizeof(srv));
        srv.Format = DXGI_FORMAT_R16_UNORM;
        srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.MipLevels = 1;
        if (FAILED(mDevice->CreateTexture2D(&desc, nullptr, mShadowMap.GetAddressOf())) ||
            FAILED(mDevice->CreateDepthStencilView(mShadowMap.Get(), &dsv, mShadowMapDsv.GetAddressOf())) ||
            FAILED(mDevice->CreateShaderResourceView(mShadowMap.Get(), &srv, mShadowMapSrv.GetAddressOf()))) {
            mShadowMapSrv.Reset();
            mShadowMapDsv.Reset();
            mShadowMap.Reset();
            SPDLOG_ERROR("The shadow map ({} a side) could not be made", size);
            return false;
        }
        mShadowMapSize = size;
    }
    mShadowMapIdle = 0;
    // (the triangles' drawing makes its rasterizer state anew: the near side cutting or not, DrawTriangles)
    mShadowMapDrawing = true;
    mShadowMapClip = clip;
    mLastZmodeDecal = -1;
    mRenderTargetHeight = size;
    ID3D11RenderTargetView* none = nullptr;
    mContext->OMSetRenderTargets(1, &none, mShadowMapDsv.Get());
    mContext->ClearDepthStencilView(mShadowMapDsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    D3D11_VIEWPORT area = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
    mContext->RSSetViewports(1, &area);
    D3D11_RECT all = { 0, 0, size, size };
    mContext->RSSetScissorRects(1, &all);
    return true;
}

void GfxRenderingAPIDX11::ApplyShadows(int shadowFb, const float cameraInverse[4][4], const float light[4][4],
                                       const float lightDir[3], const float params[124], const int viewport[4]) {
    // (the map is the backend's own, StartShadowMap's: shadowFb is 0)
    if (mShadowFailed || shadowFb != 0 || mCurrentFramebuffer < 0 ||
        mCurrentFramebuffer >= (int)mFrameBuffers.size()) {
        return;
    }
    // (without a map drawn this frame there are no cast shadows: the ambient shading and the clouds remain)
    const bool casts = params[10] > 0.5f && mShadowMapSrv.Get() != nullptr;
    FramebufferDX11& target = mFrameBuffers[mCurrentFramebuffer];
    if (target.render_target_view.Get() == nullptr || target.depth_stencil_srv.Get() == nullptr) {
        return;
    }
    const int multisampled = target.msaa_level > 1 ? 1 : 0;

    struct ShadowCB {
        float cameraInverse[16];
        float light[16];
        float lightDir[4];
        float params[4];
        float soft[4];
        float reach[4];
        float air[4];
        float tint[4];
        float sun[4];
        float view[4];
        float lampInfo[4];
        float lamps[24][4];
    };

    // With lamps about, the pass writes the finished colour from a copy of the picture (their light is more than
    // blending can brighten by); without, it darkens and brightens what is there by blending, and no copy is made.
    const int lampCount = params[24] > 0.0f ? (params[24] < 12.0f ? (int)params[24] : 12) : 0;
    const bool reads = lampCount > 0 && CopyScene();
    ComPtr<ID3D11PixelShader>& shader = reads ? mShadowReadPs[multisampled] : mShadowPs[multisampled];

    if (mShadowVs.Get() == nullptr || shader.Get() == nullptr) {
        const D3D_SHADER_MACRO macros[3] = { { "MSAA", multisampled ? "1" : "0" },
                                             { "READS", reads ? "1" : "0" },
                                             { nullptr, nullptr } };
        ComPtr<ID3DBlob> code, errors;
        // (a file beside the program stands in for the built-in source: for trying a change without a build)
        std::string source = sShadowShaderSource;
        {
            std::ifstream file("mergeship_shadow.hlsl", std::ios::binary);
            if (file) {
                source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                SPDLOG_INFO("The shadows' shader is read from mergeship_shadow.hlsl ({} bytes)", source.size());
            }
        }
        if (mShadowVs.Get() == nullptr) {
            if (FAILED(mD3dCompile(source.c_str(), source.size(), nullptr, macros, nullptr, "VSMain",
                                   "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL2, 0, code.GetAddressOf(),
                                   errors.GetAddressOf())) ||
                FAILED(mDevice->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                                   mShadowVs.GetAddressOf()))) {
                SPDLOG_ERROR("The shadows' vertex shader failed: {}",
                             errors.Get() != nullptr ? (const char*)errors->GetBufferPointer() : "no message");
                mShadowFailed = true;
                return;
            }
        }
        code.Reset();
        errors.Reset();
        // (a multisampled texture is read by a shader of level 4.1)
        if (FAILED(mD3dCompile(source.c_str(), source.size(), nullptr, macros, nullptr, "PSMain",
                               multisampled ? "ps_4_1" : "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL2, 0,
                               code.GetAddressOf(), errors.GetAddressOf())) ||
            FAILED(mDevice->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                              shader.GetAddressOf()))) {
            SPDLOG_ERROR("The shadows' pixel shader failed: {}",
                         errors.Get() != nullptr ? (const char*)errors->GetBufferPointer() : "no message");
            mShadowFailed = true;
            return;
        }
    }
    if (mShadowCb.Get() == nullptr) {
        D3D11_BUFFER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = sizeof(ShadowCB);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_BLEND_DESC blend;
        ZeroMemory(&blend, sizeof(blend));
        // (what is there, times twice the shader's colour -- it can brighten as well as darken, and a half leaves
        // a pixel as it is; the framebuffer's alpha stays)
        blend.RenderTarget[0].BlendEnable = true;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_DEPTH_STENCIL_DESC depth;
        ZeroMemory(&depth, sizeof(depth));
        depth.DepthEnable = false;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        D3D11_RASTERIZER_DESC raster;
        ZeroMemory(&raster, sizeof(raster));
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = false;
        raster.ScissorEnable = false;
        // (the shadow map answers "is this depth in the light?", smoothed between its texels)
        D3D11_SAMPLER_DESC compare;
        ZeroMemory(&compare, sizeof(compare));
        compare.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        compare.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        compare.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        compare.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        compare.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        compare.MaxAnisotropy = 1;
        compare.MinLOD = 0.0f;
        compare.MaxLOD = D3D11_FLOAT32_MAX;
        // (with lamps about: the colour as the shader gives it; the framebuffer's alpha stays)
        D3D11_BLEND_DESC plain;
        ZeroMemory(&plain, sizeof(plain));
        plain.RenderTarget[0].BlendEnable = false;
        plain.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(mDevice->CreateBlendState(&plain, mShadowPlain.GetAddressOf())) ||
            FAILED(mDevice->CreateSamplerState(&compare, mShadowSampler.GetAddressOf())) ||
            FAILED(mDevice->CreateBuffer(&desc, nullptr, mShadowCb.GetAddressOf())) ||
            FAILED(mDevice->CreateBlendState(&blend, mShadowBlend.GetAddressOf())) ||
            FAILED(mDevice->CreateDepthStencilState(&depth, mShadowDepthOff.GetAddressOf())) ||
            FAILED(mDevice->CreateRasterizerState(&raster, mShadowRaster.GetAddressOf()))) {
            mShadowFailed = true;
            return;
        }
    }

    const TextureData& colour = mTextures[target.texture_id];
    ShadowCB cb;
    memcpy(cb.cameraInverse, cameraInverse, sizeof(cb.cameraInverse));
    memcpy(cb.light, light, sizeof(cb.light));
    cb.lightDir[0] = lightDir[0];
    cb.lightDir[1] = lightDir[1];
    cb.lightDir[2] = lightDir[2];
    cb.lightDir[3] = 0.0f;
    cb.params[0] = params[0];
    cb.params[1] = params[1];
    cb.params[2] = params[2];
    cb.params[3] = (float)mShadowMapSize;
    cb.soft[0] = params[4];
    cb.soft[1] = params[5];
    cb.soft[2] = params[6];
    cb.soft[3] = params[7];
    cb.reach[0] = params[8];
    cb.reach[1] = params[9];
    cb.reach[2] = casts ? 1.0f : 0.0f;
    cb.reach[3] = params[11];
    cb.air[0] = params[12];
    cb.air[1] = params[13];
    cb.air[2] = params[14];
    cb.air[3] = params[15] > 1.0f ? params[15] : 1.0f;
    cb.tint[0] = params[16];
    cb.tint[1] = params[17];
    cb.tint[2] = params[18];
    cb.tint[3] = params[19];
    cb.sun[0] = params[20] > 0.0f ? params[20] : 1.0f;
    cb.sun[1] = params[21] > 0.0f ? params[21] : 1.0f;
    cb.sun[2] = params[22] > 0.0f ? params[22] : 1.0f;
    cb.sun[3] = 0.0f;
    // (the game's viewport counts its y from the bottom, as SetViewport takes it)
    cb.view[0] = (float)viewport[0];
    cb.view[1] = (float)((int)colour.height - viewport[1] - viewport[3]);
    cb.view[2] = (float)viewport[2];
    cb.view[3] = (float)viewport[3];
    if (cb.view[2] < 1.0f || cb.view[3] < 1.0f) {
        return;
    }
    {
        // (without the picture's copy the lamps are left out: blending cannot draw their light)
        const int lamps = reads ? lampCount : 0;
        cb.lampInfo[0] = (float)lamps;
        cb.lampInfo[1] = params[25];
        cb.lampInfo[2] = cb.lampInfo[3] = 0.0f;
        memset(cb.lamps, 0, sizeof(cb.lamps));
        for (int n = 0; n < lamps; n++) {
            const float* lamp = &params[28 + n * 8];
            cb.lamps[n * 2][0] = lamp[0];
            cb.lamps[n * 2][1] = lamp[1];
            cb.lamps[n * 2][2] = lamp[2];
            cb.lamps[n * 2][3] = lamp[3] > 1.0f ? lamp[3] : 1.0f;
            cb.lamps[n * 2 + 1][0] = lamp[4];
            cb.lamps[n * 2 + 1][1] = lamp[5];
            cb.lamps[n * 2 + 1][2] = lamp[6];
            cb.lamps[n * 2 + 1][3] = 0.0f;
        }
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    ZeroMemory(&ms, sizeof(ms));
    if (FAILED(mContext->Map(mShadowCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        return;
    }
    memcpy(ms.pData, &cb, sizeof(cb));
    mContext->Unmap(mShadowCb.Get(), 0);

    // (the framebuffer's depth is read, so it is not bound for writing meanwhile)
    mContext->OMSetRenderTargets(1, target.render_target_view.GetAddressOf(), nullptr);
    D3D11_VIEWPORT area = { cb.view[0], cb.view[1], cb.view[2], cb.view[3], 0.0f, 1.0f };
    mContext->RSSetViewports(1, &area);
    mContext->RSSetState(mShadowRaster.Get());
    mContext->OMSetDepthStencilState(mShadowDepthOff.Get(), 0);
    mContext->OMSetBlendState(reads ? mShadowPlain.Get() : mShadowBlend.Get(), nullptr, 0xFFFFFFFF);
    mContext->IASetInputLayout(nullptr);
    mContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mContext->VSSetShader(mShadowVs.Get(), nullptr, 0);
    mContext->PSSetShader(shader.Get(), nullptr, 0);
    mContext->PSSetConstantBuffers(0, 1, mShadowCb.GetAddressOf());
    mContext->PSSetSamplers(0, 1, mShadowSampler.GetAddressOf()); // (the triangles' drawing sets its own each time)
    ID3D11ShaderResourceView* views[3] = { target.depth_stencil_srv.Get(), casts ? mShadowMapSrv.Get() : nullptr,
                                           reads ? mSceneCopySrv.Get() : nullptr };
    mContext->PSSetShaderResources(0, 3, views);
    mContext->Draw(3, 0);

    // everything back as the triangles' drawing has it (what it remembers of the state is forgotten, so that it
    // sets all of it again)
    ID3D11ShaderResourceView* none[3] = { nullptr, nullptr, nullptr };
    mContext->PSSetShaderResources(0, 3, none);
    ID3D11Buffer* buffers[3] = { mPerFrameCb.Get(), mPerDrawCb.Get(), mPerPrimDepthCb.Get() };
    mContext->PSSetConstantBuffers(0, 3, buffers);
    mContext->OMSetRenderTargets(1, target.render_target_view.GetAddressOf(),
                                 target.has_depth_buffer ? target.depth_stencil_view.Get() : nullptr);
    for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
        mLastResourceViews[i].Reset();
    }
    mLastDepthTest = -1;
    mLastDepthMask = -1;
    mLastZmodeDecal = -1;
    mLastShaderProgram = nullptr;
    mLastBlendState = nullptr;
    mLastVertexBufferStride = 0;
    mLastPrimitaveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

// A copy of the framebuffer in use as it is now, one sample a pixel, for a pass that reads the picture it writes
// (mSceneCopySrv). It is kept from frame to frame and let go when it has not been asked for in a while. false: none
// could be made.
bool GfxRenderingAPIDX11::CopyScene() {
    if (mSceneCopyFailed || mCurrentFramebuffer < 0 || mCurrentFramebuffer >= (int)mFrameBuffers.size()) {
        return false;
    }
    TextureData& colour = mTextures[mFrameBuffers[mCurrentFramebuffer].texture_id];
    if (colour.texture.Get() == nullptr) {
        return false;
    }
    D3D11_TEXTURE2D_DESC from;
    colour.texture->GetDesc(&from);
    if (mSceneCopy.Get() == nullptr || mSceneCopyWidth != from.Width || mSceneCopyHeight != from.Height ||
        mSceneCopyFormat != (uint32_t)from.Format) {
        mSceneCopySrv.Reset();
        mSceneCopy.Reset();
        mSceneCopyWidth = mSceneCopyHeight = 0;
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = from.Width;
        desc.Height = from.Height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = from.Format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(mDevice->CreateTexture2D(&desc, nullptr, mSceneCopy.GetAddressOf())) ||
            FAILED(mDevice->CreateShaderResourceView(mSceneCopy.Get(), nullptr, mSceneCopySrv.GetAddressOf()))) {
            mSceneCopySrv.Reset();
            mSceneCopy.Reset();
            mSceneCopyFailed = true; // (not tried again every frame)
            SPDLOG_ERROR("The picture's copy ({} x {}) could not be made", from.Width, from.Height);
            return false;
        }
        mSceneCopyWidth = from.Width;
        mSceneCopyHeight = from.Height;
        mSceneCopyFormat = (uint32_t)from.Format;
    }
    mSceneCopyIdle = 0;
    if (from.SampleDesc.Count > 1) {
        mContext->ResolveSubresource(mSceneCopy.Get(), 0, colour.texture.Get(), 0, from.Format);
    } else {
        mContext->CopyResource(mSceneCopy.Get(), colour.texture.Get());
    }
    return true;
}

// MergeShip2, the finished picture's glow and look (gfx_rendering_api.h, ApplyPost). A file mergeship_post.hlsl
// beside the program stands in for this source (for trying a change without a build).
static const char* sPostShaderSource = R"(
cbuffer PostCB : register(b0) {
    row_major float4x4 cameraInverse; // a point of the picture (x and y from -1 to 1 with y up, depth from -1 to 1)
                                      // back into the world
    float4 view;  // the part of the framebuffer the world is drawn into: left, top, width, height (pixels)
    float4 size;  // xy: a texel of the picture being read, in its own 0-to-1 measure; zw: unused
    float4 glow;  // x: how bright a thing must be to glow (0 to 1), y: how strongly the glow is laid over the
                  // picture (0: none), z: unused, w: unused
    float4 look;  // x: how much of the film's curve (0: none), y: saturation (1: as it is), z: how dark the corners
                  // go (0: not at all), w: how much of the grade (cool shadows, warm lights; 0: none)
    float4 focus; // x: how strongly what is out of focus is blurred (0: nothing is), y: the distance in focus (the
                  // world's units from the eye), z: how far either side of it stays sharp, w: over how much further
                  // the blur grows to its full
    float4 spare[7];
};
Texture2D g_a : register(t0); // the picture being read
Texture2D g_b : register(t1); // (the last pass) the glow
Texture2D g_c : register(t2); // (the last pass) the picture blurred a little
Texture2D g_d : register(t3); // (the last pass) the picture blurred more
#if MSAA
Texture2DMS<float> g_depth : register(t4);
#else
Texture2D<float> g_depth : register(t4);
#endif
SamplerState g_linear : register(s0);

struct VSOut {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

// A picture half the size of the one read, a little blurred: five taps, each between four texels.
float3 Half(float2 uv) {
    float3 c = g_a.Sample(g_linear, uv).rgb * 4.0;
    c += g_a.Sample(g_linear, uv + size.xy * float2(-1.0, -1.0)).rgb;
    c += g_a.Sample(g_linear, uv + size.xy * float2(1.0, -1.0)).rgb;
    c += g_a.Sample(g_linear, uv + size.xy * float2(-1.0, 1.0)).rgb;
    c += g_a.Sample(g_linear, uv + size.xy * float2(1.0, 1.0)).rgb;
    return c * 0.125;
}

float4 PSHalf(VSOut i) : SV_Target {
    return float4(Half(i.uv), 1.0);
}

// What glows: the bright parts of the picture, half its size. A thing glows by how far it is over the threshold:
// by its brightness to the eye, or by its strongest colour (a flame or lava is not bright by the eye's measure, and
// glows all the same).
float4 PSBright(VSOut i) : SV_Target {
    float3 c = Half(i.uv);
    float bright = max(dot(c, float3(0.299, 0.587, 0.114)), 0.9 * max(c.r, max(c.g, c.b)));
    float over = saturate((bright - glow.x) / max(1.0 - glow.x, 0.001));
    return float4(c * over * over, 1.0);
}

// A picture twice the size of the one read, blurred as it grows: eight taps about each place. It is added to what
// the larger picture holds (the glow of every width, one over the other).
float4 PSGrow(VSOut i) : SV_Target {
    float2 h = size.xy * 0.5;
    float3 c = g_a.Sample(g_linear, i.uv + float2(-h.x * 2.0, 0.0)).rgb;
    c += g_a.Sample(g_linear, i.uv + float2(-h.x, h.y)).rgb * 2.0;
    c += g_a.Sample(g_linear, i.uv + float2(0.0, h.y * 2.0)).rgb;
    c += g_a.Sample(g_linear, i.uv + float2(h.x, h.y)).rgb * 2.0;
    c += g_a.Sample(g_linear, i.uv + float2(h.x * 2.0, 0.0)).rgb;
    c += g_a.Sample(g_linear, i.uv + float2(h.x, -h.y)).rgb * 2.0;
    c += g_a.Sample(g_linear, i.uv + float2(0.0, -h.y * 2.0)).rgb;
    c += g_a.Sample(g_linear, i.uv + float2(-h.x, -h.y)).rgb * 2.0;
    return float4(c / 12.0, 1.0);
}

float SceneDepth(int2 p) {
    p = clamp(p, int2(view.xy), int2(view.xy + view.zw) - int2(1, 1));
#if MSAA
    return g_depth.Load(p, 0);
#else
    return g_depth.Load(int3(p, 0));
#endif
}

float3 World(int2 p, float depth) {
    float2 uv = (float2(p) + float2(0.5, 0.5) - view.xy) / view.zw;
    float4 at = mul(float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth * 2.0 - 1.0, 1.0), cameraInverse);
    return at.xyz / at.w;
}

// The finished picture: what is out of focus blurred, the glow laid over it, and the look: a film's curve (deeper
// darks, the middle where it was, the bright parts running out softly), cooler shadows and warmer lights, colours a
// little fuller, the corners a little darker.
float4 PSFinal(VSOut i) : SV_Target {
    float3 c = g_a.Sample(g_linear, i.uv).rgb;
    if (focus.x > 0.0) {
        int2 p = int2(i.pos.xy);
        float depth = SceneDepth(p);
        float4 eye = mul(float4(0.0, 0.0, -1.0, 1.0), cameraInverse);
        float away = depth >= 0.9999995 ? 1.0e6 : length(World(p, depth) - eye.xyz / eye.w);
        float blur = saturate((abs(away - focus.y) - focus.z) / max(focus.w, 1.0)) * focus.x;
        if (blur > 0.0) {
            float3 soft = g_c.Sample(g_linear, i.uv).rgb;
            float3 softer = g_d.Sample(g_linear, i.uv).rgb;
            c = lerp(c, lerp(soft, softer, saturate(blur * 2.0 - 1.0)), saturate(blur * 2.0));
        }
    }
    if (glow.y > 0.0) {
        // (laid over as light is: what is bright already takes less of it, so nothing burns out flat)
        float3 halo = g_b.Sample(g_linear, i.uv).rgb * (glow.y * 0.25);
        c = 1.0 - (1.0 - c) * (1.0 - saturate(halo));
    }
    if (look.x > 0.0) {
        float3 curve = c * c * (3.0 - 2.0 * c);
        c = lerp(c, curve, look.x);
    }
    float grey = dot(c, float3(0.299, 0.587, 0.114));
    if (look.w > 0.0) {
        float dark = (1.0 - grey) * (1.0 - grey);
        float light = grey * grey;
        c += (float3(-0.020, 0.004, 0.030) * dark + float3(0.030, 0.012, -0.024) * light) * look.w;
    }
    c = lerp(float3(grey, grey, grey), c, look.y);
    if (look.z > 0.0) {
        float2 from = i.uv - float2(0.5, 0.5);
        c *= 1.0 - look.z * smoothstep(0.30, 1.05, dot(from, from) * 2.6);
    }
    // (a grain finer than the screen's steps, so that the curve leaves no bands in a sky)
    float grain = frac(52.9829189 * frac(dot(i.pos.xy, float2(0.06711056, 0.00583715)))) - 0.5;
    c += grain / 255.0;
    return float4(saturate(c), 1.0);
}
)";

void GfxRenderingAPIDX11::ApplyPost(const float cameraInverse[4][4], const float params[48], const int viewport[4]) {
    if (mPostFailed || mCurrentFramebuffer < 0 || mCurrentFramebuffer >= (int)mFrameBuffers.size() ||
        mFeatureLevel < D3D_FEATURE_LEVEL_10_0) {
        return;
    }
    FramebufferDX11& target = mFrameBuffers[mCurrentFramebuffer];
    const TextureData& colour = mTextures[target.texture_id];
    if (target.render_target_view.Get() == nullptr || colour.texture.Get() == nullptr || colour.width < 64 ||
        colour.height < 64) {
        return;
    }
    const int multisampled = target.msaa_level > 1 ? 1 : 0;
    const bool glows = params[1] > 0.0f;
    // (what is out of focus is found by the framebuffer's depth)
    const bool focuses = params[8] > 0.0f && target.depth_stencil_srv.Get() != nullptr;

    struct PostCB {
        float cameraInverse[16];
        float view[4];
        float size[4];
        float glow[4];
        float look[4];
        float focus[4];
        float spare[7][4];
    };

    if (mPostVs.Get() == nullptr || mPostFinalPs[multisampled].Get() == nullptr) {
        std::string source = sPostShaderSource;
        {
            std::ifstream file("mergeship_post.hlsl", std::ios::binary);
            if (file) {
                source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
                SPDLOG_INFO("The picture's look: the shader is read from mergeship_post.hlsl ({} bytes)",
                            source.size());
            }
        }
        const D3D_SHADER_MACRO plain[2] = { { "MSAA", "0" }, { nullptr, nullptr } };
        const D3D_SHADER_MACRO sampled[2] = { { "MSAA", multisampled ? "1" : "0" }, { nullptr, nullptr } };
        const auto pixel = [&](const char* entry, const D3D_SHADER_MACRO* macros, const char* level,
                               ID3D11PixelShader** to) -> bool {
            ComPtr<ID3DBlob> code, errors;
            if (FAILED(mD3dCompile(source.c_str(), source.size(), nullptr, macros, nullptr, entry, level,
                                   D3DCOMPILE_OPTIMIZATION_LEVEL2, 0, code.GetAddressOf(),
                                   errors.GetAddressOf())) ||
                FAILED(mDevice->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, to))) {
                SPDLOG_ERROR("The picture's look: the shader {} failed: {}", entry,
                             errors.Get() != nullptr ? (const char*)errors->GetBufferPointer() : "no message");
                return false;
            }
            return true;
        };
        if (mPostVs.Get() == nullptr) {
            ComPtr<ID3DBlob> code, errors;
            if (FAILED(mD3dCompile(source.c_str(), source.size(), nullptr, plain, nullptr, "VSMain", "vs_4_0",
                                   D3DCOMPILE_OPTIMIZATION_LEVEL2, 0, code.GetAddressOf(),
                                   errors.GetAddressOf())) ||
                FAILED(mDevice->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                                   mPostVs.GetAddressOf()))) {
                SPDLOG_ERROR("The picture's look: the vertex shader failed: {}",
                             errors.Get() != nullptr ? (const char*)errors->GetBufferPointer() : "no message");
                mPostFailed = true;
                return;
            }
            if (!pixel("PSHalf", plain, "ps_4_0", mPostPs[0].GetAddressOf()) ||
                !pixel("PSBright", plain, "ps_4_0", mPostPs[1].GetAddressOf()) ||
                !pixel("PSGrow", plain, "ps_4_0", mPostPs[2].GetAddressOf())) {
                mPostVs.Reset();
                mPostFailed = true;
                return;
            }
        }
        // (a multisampled texture is read by a shader of level 4.1)
        if (!pixel("PSFinal", sampled, multisampled ? "ps_4_1" : "ps_4_0", mPostFinalPs[multisampled].GetAddressOf())) {
            mPostFailed = true;
            return;
        }
    }
    if (mPostCb.Get() == nullptr) {
        D3D11_BUFFER_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = sizeof(PostCB);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SAMPLER_DESC smooth;
        ZeroMemory(&smooth, sizeof(smooth));
        smooth.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        smooth.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        smooth.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        smooth.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        smooth.ComparisonFunc = D3D11_COMPARISON_NEVER;
        smooth.MaxAnisotropy = 1;
        smooth.MinLOD = 0.0f;
        smooth.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_BLEND_DESC plain;
        ZeroMemory(&plain, sizeof(plain));
        plain.RenderTarget[0].BlendEnable = false;
        plain.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_BLEND_DESC add = plain;
        add.RenderTarget[0].BlendEnable = true;
        add.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        add.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        add.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        add.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        add.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        add.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        // (onto the framebuffer: its alpha stays)
        D3D11_BLEND_DESC keep = plain;
        keep.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        D3D11_DEPTH_STENCIL_DESC depth;
        ZeroMemory(&depth, sizeof(depth));
        depth.DepthEnable = false;
        depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
        D3D11_RASTERIZER_DESC raster;
        ZeroMemory(&raster, sizeof(raster));
        raster.FillMode = D3D11_FILL_SOLID;
        raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = false;
        raster.ScissorEnable = false;
        if (FAILED(mDevice->CreateSamplerState(&smooth, mPostSampler.GetAddressOf())) ||
            FAILED(mDevice->CreateBlendState(&plain, mPostPlain.GetAddressOf())) ||
            FAILED(mDevice->CreateBlendState(&add, mPostAdd.GetAddressOf())) ||
            FAILED(mDevice->CreateBlendState(&keep, mPostKeepAlpha.GetAddressOf())) ||
            FAILED(mDevice->CreateDepthStencilState(&depth, mPostDepthOff.GetAddressOf())) ||
            FAILED(mDevice->CreateRasterizerState(&raster, mPostRaster.GetAddressOf())) ||
            FAILED(mDevice->CreateBuffer(&desc, nullptr, mPostCb.GetAddressOf()))) {
            mPostCb.Reset();
            mPostFailed = true;
            return;
        }
    }

    // the pictures between: the picture at a half, a quarter and an eighth of its size (what is out of focus), and
    // what glows from a quarter down to a 64th (in a format with room over white and fine steps under it)
    D3D11_TEXTURE2D_DESC from;
    colour.texture->GetDesc(&from);
    if (mPostSoft[0].texture.Get() == nullptr || mPostWidth != from.Width || mPostHeight != from.Height ||
        mPostFormat != (uint32_t)from.Format) {
        const auto make = [&](PostTarget& to, uint32_t width, uint32_t height, DXGI_FORMAT format) -> bool {
            to = PostTarget();
            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(mDevice->CreateTexture2D(&desc, nullptr, to.texture.GetAddressOf())) ||
                FAILED(mDevice->CreateRenderTargetView(to.texture.Get(), nullptr, to.target.GetAddressOf())) ||
                FAILED(mDevice->CreateShaderResourceView(to.texture.Get(), nullptr, to.view.GetAddressOf()))) {
                return false;
            }
            to.width = width;
            to.height = height;
            return true;
        };
        bool made = true;
        uint32_t width = from.Width;
        uint32_t height = from.Height;
        for (int i = 0; i < 3 && made; i++) {
            width = width > 1 ? (width + 1) / 2 : 1;
            height = height > 1 ? (height + 1) / 2 : 1;
            made = make(mPostSoft[i], width, height, from.Format);
        }
        width = mPostSoft[0].width;
        height = mPostSoft[0].height;
        for (int i = 0; i < 5 && made; i++) {
            width = width > 1 ? (width + 1) / 2 : 1;
            height = height > 1 ? (height + 1) / 2 : 1;
            made = make(mPostGlow[i], width, height, DXGI_FORMAT_R11G11B10_FLOAT);
        }
        if (!made) {
            for (PostTarget& each : mPostSoft) {
                each = PostTarget();
            }
            for (PostTarget& each : mPostGlow) {
                each = PostTarget();
            }
            SPDLOG_ERROR("The picture's look: its pictures ({} x {}) could not be made", from.Width, from.Height);
            mPostFailed = true;
            return;
        }
        mPostWidth = from.Width;
        mPostHeight = from.Height;
        mPostFormat = (uint32_t)from.Format;
    }
    mPostIdle = 0;
    if (!CopyScene()) {
        return;
    }

    PostCB cb;
    memset(&cb, 0, sizeof(cb));
    memcpy(cb.cameraInverse, cameraInverse, sizeof(cb.cameraInverse));
    // (the game's viewport counts its y from the bottom, as SetViewport takes it)
    cb.view[0] = (float)viewport[0];
    cb.view[1] = (float)((int)colour.height - viewport[1] - viewport[3]);
    cb.view[2] = viewport[2] > 0 ? (float)viewport[2] : (float)mPostWidth;
    cb.view[3] = viewport[3] > 0 ? (float)viewport[3] : (float)mPostHeight;
    cb.glow[0] = params[0];
    cb.glow[1] = glows ? params[1] : 0.0f;
    cb.look[0] = params[4];
    cb.look[1] = params[5] > 0.0f ? params[5] : 1.0f;
    cb.look[2] = params[6];
    cb.look[3] = params[7];
    cb.focus[0] = focuses ? params[8] : 0.0f;
    cb.focus[1] = params[9];
    cb.focus[2] = params[10];
    cb.focus[3] = params[11];

    mContext->RSSetState(mPostRaster.Get());
    mContext->OMSetDepthStencilState(mPostDepthOff.Get(), 0);
    mContext->IASetInputLayout(nullptr);
    mContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mContext->VSSetShader(mPostVs.Get(), nullptr, 0);
    mContext->PSSetConstantBuffers(0, 1, mPostCb.GetAddressOf());
    mContext->PSSetSamplers(0, 1, mPostSampler.GetAddressOf()); // (the triangles' drawing sets its own each time)

    ID3D11ShaderResourceView* const nothing[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    // one picture drawn into another through a shader
    const auto pass = [&](ID3D11PixelShader* shader, ID3D11ShaderResourceView* read, uint32_t readWidth,
                          uint32_t readHeight, PostTarget& to, ID3D11BlendState* blend) {
        cb.size[0] = 1.0f / (float)readWidth;
        cb.size[1] = 1.0f / (float)readHeight;
        D3D11_MAPPED_SUBRESOURCE ms;
        ZeroMemory(&ms, sizeof(ms));
        if (SUCCEEDED(mContext->Map(mPostCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &cb, sizeof(cb));
            mContext->Unmap(mPostCb.Get(), 0);
        }
        mContext->PSSetShaderResources(0, 5, nothing);
        mContext->OMSetRenderTargets(1, to.target.GetAddressOf(), nullptr);
        D3D11_VIEWPORT area = { 0.0f, 0.0f, (float)to.width, (float)to.height, 0.0f, 1.0f };
        mContext->RSSetViewports(1, &area);
        mContext->OMSetBlendState(blend, nullptr, 0xFFFFFFFF);
        mContext->PSSetShader(shader, nullptr, 0);
        mContext->PSSetShaderResources(0, 1, &read);
        mContext->Draw(3, 0);
    };
    if (glows || focuses) {
        pass(mPostPs[0].Get(), mSceneCopySrv.Get(), mPostWidth, mPostHeight, mPostSoft[0], mPostPlain.Get());
    }
    if (focuses) {
        pass(mPostPs[0].Get(), mPostSoft[0].view.Get(), mPostSoft[0].width, mPostSoft[0].height, mPostSoft[1],
             mPostPlain.Get());
        pass(mPostPs[0].Get(), mPostSoft[1].view.Get(), mPostSoft[1].width, mPostSoft[1].height, mPostSoft[2],
             mPostPlain.Get());
    }
    if (glows) {
        // the bright parts, smaller and smaller, then each grown back over the one above it: a glow of every width
        pass(mPostPs[1].Get(), mPostSoft[0].view.Get(), mPostSoft[0].width, mPostSoft[0].height, mPostGlow[0],
             mPostPlain.Get());
        for (int i = 1; i < 5; i++) {
            pass(mPostPs[0].Get(), mPostGlow[i - 1].view.Get(), mPostGlow[i - 1].width, mPostGlow[i - 1].height,
                 mPostGlow[i], mPostPlain.Get());
        }
        for (int i = 4; i >= 1; i--) {
            pass(mPostPs[2].Get(), mPostGlow[i].view.Get(), mPostGlow[i].width, mPostGlow[i].height,
                 mPostGlow[i - 1], mPostAdd.Get());
        }
    }

    // the last pass, onto the framebuffer (its depth is read, so it is not bound for writing meanwhile)
    cb.size[0] = 1.0f / (float)mPostWidth;
    cb.size[1] = 1.0f / (float)mPostHeight;
    {
        D3D11_MAPPED_SUBRESOURCE ms;
        ZeroMemory(&ms, sizeof(ms));
        if (SUCCEEDED(mContext->Map(mPostCb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &cb, sizeof(cb));
            mContext->Unmap(mPostCb.Get(), 0);
        }
    }
    mContext->PSSetShaderResources(0, 5, nothing);
    mContext->OMSetRenderTargets(1, target.render_target_view.GetAddressOf(), nullptr);
    D3D11_VIEWPORT whole = { 0.0f, 0.0f, (float)mPostWidth, (float)mPostHeight, 0.0f, 1.0f };
    mContext->RSSetViewports(1, &whole);
    mContext->OMSetBlendState(mPostKeepAlpha.Get(), nullptr, 0xFFFFFFFF);
    mContext->PSSetShader(mPostFinalPs[multisampled].Get(), nullptr, 0);
    ID3D11ShaderResourceView* views[5] = { mSceneCopySrv.Get(), glows ? mPostGlow[0].view.Get() : nullptr,
                                           focuses ? mPostSoft[1].view.Get() : nullptr,
                                           focuses ? mPostSoft[2].view.Get() : nullptr,
                                           focuses ? target.depth_stencil_srv.Get() : nullptr };
    mContext->PSSetShaderResources(0, 5, views);
    mContext->Draw(3, 0);

    // everything back as the triangles' drawing has it (what it remembers of the state is forgotten, so that it
    // sets all of it again)
    mContext->PSSetShaderResources(0, 5, nothing);
    ID3D11Buffer* buffers[3] = { mPerFrameCb.Get(), mPerDrawCb.Get(), mPerPrimDepthCb.Get() };
    mContext->PSSetConstantBuffers(0, 3, buffers);
    mContext->OMSetRenderTargets(1, target.render_target_view.GetAddressOf(),
                                 target.has_depth_buffer ? target.depth_stencil_view.Get() : nullptr);
    for (int i = 0; i < SHADER_MAX_TEXTURES; i++) {
        mLastResourceViews[i].Reset();
    }
    mLastDepthTest = -1;
    mLastDepthMask = -1;
    mLastZmodeDecal = -1;
    mLastShaderProgram = nullptr;
    mLastBlendState = nullptr;
    mLastVertexBufferStride = 0;
    mLastPrimitaveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

#define RAND_NOISE "((random(float3(floor(screenSpace.xy * noise_scale), noise_frame)) + 1.0) / 2.0)"

static const char* prism_shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                            bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            default:
            case SHADER_0:
                return with_alpha ? "float4(0.0, 0.0, 0.0, 0.0)" : "float3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "float4(1.0, 1.0, 1.0, 1.0)" : "float3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "input.input1" : "input.input1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "input.input2" : "input.input2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "input.input3" : "input.input3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "input.input4" : "input.input4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "float4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "float3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "float4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "float3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "float4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "float3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "float4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "float3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "float4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "float3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            default:
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "input.input1.a";
            case SHADER_INPUT_2:
                return "input.input2.a";
            case SHADER_INPUT_3:
                return "input.input3.a";
            case SHADER_INPUT_4:
                return "input.input4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
}

bool prism_get_bool(prism::ContextTypes* value) {
    if (std::holds_alternative<int>(*value)) {
        return std::get<int>(*value) == 1;
    }
    return false;
}

#undef RAND_NOISE

prism::ContextTypes* prism_append_formula(prism::ContextTypes* _, prism::ContextTypes* a_arg,
                                          prism::ContextTypes* a_single, prism::ContextTypes* a_mult,
                                          prism::ContextTypes* a_mix, prism::ContextTypes* a_with_alpha,
                                          prism::ContextTypes* a_only_alpha, prism::ContextTypes* a_alpha,
                                          prism::ContextTypes* a_first_cycle) {
    auto c = std::get<prism::MTDArray<int>>(*a_arg);
    bool do_single = prism_get_bool(a_single);
    bool do_multiply = prism_get_bool(a_mult);
    bool do_mix = prism_get_bool(a_mix);
    bool with_alpha = prism_get_bool(a_with_alpha);
    bool only_alpha = prism_get_bool(a_only_alpha);
    bool opt_alpha = prism_get_bool(a_alpha);
    bool first_cycle = prism_get_bool(a_first_cycle);
    std::string out = "";
    if (do_single) {
        out += prism_shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    } else if (do_multiply) {
        out += prism_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " * ";
        out += prism_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
    } else if (do_mix) {
        out += "lerp(";
        out += prism_shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += prism_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += prism_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += ")";
    } else {
        out += "(";
        out += prism_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " - ";
        out += prism_shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ") * ";
        out += prism_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += " + ";
        out += prism_shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    }
    return new prism::ContextTypes{ out };
}

static size_t raw_numFloats = 0;

prism::ContextTypes* update_raw_floats(prism::ContextTypes* _, prism::ContextTypes* num) {
    raw_numFloats += std::get<int>(*num);
    return nullptr;
}

std::optional<std::string> dx_include_fs(const std::string& path) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = static_pointer_cast<Ship::Shader>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true, init));
    if (res == nullptr) {
        return std::nullopt;
    }

    auto inc = static_cast<std::string*>(res->GetRawPointer());
    return *inc;
}

std::string gfx_direct3d_common_build_shader(size_t& numFloats, const CCFeatures& cc_features,
                                             bool include_root_signature, bool three_point_filtering, bool use_srgb) {
    raw_numFloats = 4;

    prism::Processor processor;
    prism::ContextItems mContext = {
        { "SHADER_0", SHADER_0 },
        { "SHADER_INPUT_1", SHADER_INPUT_1 },
        { "SHADER_INPUT_2", SHADER_INPUT_2 },
        { "SHADER_INPUT_3", SHADER_INPUT_3 },
        { "SHADER_INPUT_4", SHADER_INPUT_4 },
        { "SHADER_INPUT_5", SHADER_INPUT_5 },
        { "SHADER_INPUT_6", SHADER_INPUT_6 },
        { "SHADER_INPUT_7", SHADER_INPUT_7 },
        { "SHADER_TEXEL0", SHADER_TEXEL0 },
        { "SHADER_TEXEL0A", SHADER_TEXEL0A },
        { "SHADER_TEXEL1", SHADER_TEXEL1 },
        { "SHADER_TEXEL1A", SHADER_TEXEL1A },
        { "SHADER_1", SHADER_1 },
        { "SHADER_COMBINED", SHADER_COMBINED },
        { "SHADER_NOISE", SHADER_NOISE },
        { "o_c", M_ARRAY(cc_features.c, int, 2, 2, 4) },
        { "o_alpha", cc_features.opt_alpha },
        { "o_fog", cc_features.opt_fog },
        { "o_texture_edge", cc_features.opt_texture_edge },
        { "o_noise", cc_features.opt_noise },
        { "o_2cyc", cc_features.opt_2cyc },
        { "o_alpha_threshold", cc_features.opt_alpha_threshold },
        { "o_invisible", cc_features.opt_invisible },
        { "o_grayscale", cc_features.opt_grayscale },
        // MergeShip2 render styles (RenderStyle in interpreter.h)
        { "o_style", cc_features.render_style },
        { "o_shade_slot", cc_features.shade_slot },
        { "o_toon", (cc_features.render_style == RENDER_STYLE_TOON ||
                     cc_features.render_style == RENDER_STYLE_TOON_FLAT ||
                     cc_features.render_style == 14) && // (14: toon on a surface with painted light)
                        cc_features.shade_slot > 0 },
        { "o_dither", cc_features.render_style == RENDER_STYLE_16BIT ||
                          cc_features.render_style == RENDER_STYLE_8BIT ||
                          cc_features.render_style == RENDER_STYLE_GAME_BOY },
        { "o_prim_depth", cc_features.opt_prim_depth },
        { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
        { "o_masks", M_ARRAY(cc_features.used_masks, bool, 2) },
        { "o_blend", M_ARRAY(cc_features.used_blend, bool, 2) },
        { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
        { "o_inputs", cc_features.numInputs },
        { "o_do_mix", M_ARRAY(cc_features.do_mix, bool, 2, 2) },
        { "o_do_single", M_ARRAY(cc_features.do_single, bool, 2, 2) },
        { "o_do_multiply", M_ARRAY(cc_features.do_multiply, bool, 2, 2) },
        { "o_color_alpha_same", M_ARRAY(cc_features.color_alpha_same, bool, 2) },
        { "o_root_signature", include_root_signature },
        { "o_three_point_filtering", three_point_filtering },
        { "srgb_mode", use_srgb },
        { "append_formula", (InvokeFunc)prism_append_formula },
        { "update_floats", (InvokeFunc)update_raw_floats },
    };
    processor.populate(mContext);
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    const char* shaderName = Fast::gfx_get_shader(cc_features.shader_id);
    std::string path = "shaders/directx/default.shader.hlsl";

    if (nullptr != shaderName) {
        path = std::string(shaderName) + ".hlsl";
    }

    auto res = static_pointer_cast<Ship::Shader>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path, true, init));

    if (res == nullptr) {
        SPDLOG_ERROR("Failed to load default directx shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(dx_include_fs);
    auto result = processor.process();
    numFloats = raw_numFloats;
    // SPDLOG_INFO("=========== DX11 SHADER ============");
    // SPDLOG_INFO(result);
    // SPDLOG_INFO("====================================");
    return result;
}
} // namespace Fast
#endif
