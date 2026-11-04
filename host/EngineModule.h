#pragma once

#include <mergeship/EngineApi.h>

#include <string>

namespace MergeShipHost {

// One game module (soh.dll or 2ship.dll) and its EngineApi exports.
class EngineModule {
  public:
    // Loads the DLL (Android: the shared library, by file name from the app's library folder) and resolves every
    // export. On failure, Error() says why.
#ifdef _WIN32
    bool Load(const std::wstring& path);
#else
    bool Load(const std::string& path);
#endif
    // True if the module was built against the libultraship the host loaded, with the host's API version.
    bool CheckAbi(const char* loadedLusAbiId);

    const std::string& Error() const { return mError; }
    const std::string& Name() const { return mName; }
    int32_t Id() const { return mAbi != nullptr ? mAbi->engineId : MS_ENGINE_NONE; }
    const MsEngineAbi* Abi() const { return mAbi; }

    int32_t Init(const MsHostServices* services, int32_t argc, char** argv) const;
    int32_t RunFrame() const { return mRunFrame(); }
    void Suspend() const { mSuspend(); }
    void Resume(const MsTransfer* arrival) const { mResume(arrival); }
    void Shutdown() const { mShutdown(); }
    void* Context() const { return mGetContext(); }
    // MsEngine_ExtractRom, if the module has it (-1 otherwise). romPath NULL: 0 if it has an extractor.
    int32_t ExtractRom(const char* romPath, const char* destDir,
                       void (*progress)(void* user, uint64_t done, uint64_t total), void* user) const;

  private:
    void* mModule = nullptr;
    const MsEngineAbi* mAbi = nullptr;
    std::string mName;
    std::string mError;
    MsEngine_GetAbi_Fn mGetAbi = nullptr;
    MsEngine_Init_Fn mInit = nullptr;
    MsEngine_RunFrame_Fn mRunFrame = nullptr;
    MsEngine_Suspend_Fn mSuspend = nullptr;
    MsEngine_Resume_Fn mResume = nullptr;
    MsEngine_Shutdown_Fn mShutdown = nullptr;
    MsEngine_GetContext_Fn mGetContext = nullptr;
    MsEngine_ExtractRom_Fn mExtractRom = nullptr;
};

} // namespace MergeShipHost
