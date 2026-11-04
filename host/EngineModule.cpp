#include "EngineModule.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstring>

namespace MergeShipHost {

#ifdef _WIN32
namespace {
std::string Narrow(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), length, nullptr, nullptr);
    return out;
}

template <typename Fn> bool Resolve(HMODULE module, const char* name, Fn& out, std::string& error) {
    out = reinterpret_cast<Fn>(GetProcAddress(module, name));
    if (out == nullptr) {
        error = std::string("missing export ") + name;
        return false;
    }
    return true;
}
} // namespace

bool EngineModule::Load(const std::wstring& path) {
    mName = Narrow(path);
    HMODULE module = LoadLibraryW(path.c_str());
    if (module == nullptr) {
        mError = "LoadLibrary failed with error " + std::to_string(GetLastError());
        return false;
    }
    mModule = module;
    if (!Resolve(module, "MsEngine_GetAbi", mGetAbi, mError) || !Resolve(module, "MsEngine_Init", mInit, mError) ||
        !Resolve(module, "MsEngine_RunFrame", mRunFrame, mError) ||
        !Resolve(module, "MsEngine_Suspend", mSuspend, mError) ||
        !Resolve(module, "MsEngine_Resume", mResume, mError) ||
        !Resolve(module, "MsEngine_Shutdown", mShutdown, mError) ||
        !Resolve(module, "MsEngine_GetContext", mGetContext, mError)) {
        return false;
    }
    // Optional: the module's own ROM extractor (MsHostServices.ExtractRom). NULL when it has none.
    mExtractRom = reinterpret_cast<MsEngine_ExtractRom_Fn>(GetProcAddress(module, "MsEngine_ExtractRom"));
    mAbi = mGetAbi();
    if (mAbi == nullptr || mAbi->size < sizeof(MsEngineAbi)) {
        mError = "MsEngine_GetAbi returned no usable description";
        return false;
    }
    mName = mAbi->engineName != nullptr ? mAbi->engineName : mName;
    return true;
}
#else
// Android (and any ELF system): the game is loaded privately (RTLD_LOCAL), so the two games' identically named
// functions never meet; each resolves its own first (they are linked -Bsymbolic).
namespace {
template <typename Fn> bool Resolve(void* module, const char* name, Fn& out, std::string& error) {
    out = reinterpret_cast<Fn>(dlsym(module, name));
    if (out == nullptr) {
        error = std::string("missing export ") + name;
        return false;
    }
    return true;
}
} // namespace

bool EngineModule::Load(const std::string& path) {
    mName = path;
    void* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) {
        const char* why = dlerror();
        mError = std::string("dlopen failed: ") + (why != nullptr ? why : "?");
        return false;
    }
    mModule = module;
    if (!Resolve(module, "MsEngine_GetAbi", mGetAbi, mError) || !Resolve(module, "MsEngine_Init", mInit, mError) ||
        !Resolve(module, "MsEngine_RunFrame", mRunFrame, mError) ||
        !Resolve(module, "MsEngine_Suspend", mSuspend, mError) ||
        !Resolve(module, "MsEngine_Resume", mResume, mError) ||
        !Resolve(module, "MsEngine_Shutdown", mShutdown, mError) ||
        !Resolve(module, "MsEngine_GetContext", mGetContext, mError)) {
        return false;
    }
    mExtractRom = reinterpret_cast<MsEngine_ExtractRom_Fn>(dlsym(module, "MsEngine_ExtractRom"));
    mAbi = mGetAbi();
    if (mAbi == nullptr || mAbi->size < sizeof(MsEngineAbi)) {
        mError = "MsEngine_GetAbi returned no usable description";
        return false;
    }
    mName = mAbi->engineName != nullptr ? mAbi->engineName : mName;
    return true;
}
#endif

bool EngineModule::CheckAbi(const char* loadedLusAbiId) {
    if (mAbi->apiVersion != MS_ENGINE_API_VERSION) {
        mError = "engine API version " + std::to_string(mAbi->apiVersion) + ", host expects " +
                 std::to_string(MS_ENGINE_API_VERSION);
        return false;
    }
    if (mAbi->lusAbiId == nullptr || loadedLusAbiId == nullptr || std::strcmp(mAbi->lusAbiId, loadedLusAbiId) != 0) {
        mError = std::string("built against libultraship ") + (mAbi->lusAbiId ? mAbi->lusAbiId : "?") +
                 ", but the loaded libultraship.dll is " + (loadedLusAbiId ? loadedLusAbiId : "?");
        return false;
    }
    return true;
}

int32_t EngineModule::Init(const MsHostServices* services, int32_t argc, char** argv) const {
    return mInit(services, argc, argv);
}

int32_t EngineModule::ExtractRom(const char* romPath, const char* destDir,
                                 void (*progress)(void* user, uint64_t done, uint64_t total), void* user) const {
    if (mExtractRom == nullptr) {
        return -1;
    }
    return romPath == nullptr ? 0 : mExtractRom(romPath, destDir, progress, user);
}

} // namespace MergeShipHost
