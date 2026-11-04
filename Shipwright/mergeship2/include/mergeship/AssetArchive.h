#pragma once

#include <ship/resource/archive/O2rArchive.h>
#include <ship/resource/File.h>
#include <ship/utils/StrHash64.h>
#include <spdlog/spdlog.h>
#include <unordered_map>

namespace MergeShip {
// Keep foreign resources out of the host's root namespace and version list.
// Native MM also exposes root aliases for its existing generated asset headers.
// Embedded references retain their explicit namespace in either runtime.
class AssetArchive final : public Ship::Archive {
  public:
    AssetArchive(const std::string& path, const std::string& assetNamespace, bool native = false)
        : Archive(path), mBacking(std::make_shared<Ship::O2rArchive>(path)),
          mPrefix(assetNamespace + "/"), mNative(native) {}

    bool Open() override {
        mPaths.clear();
        ListFiles()->clear();
        mBacking->ListFiles()->clear();
        if (!mBacking->Open()) {
            return false;
        }
        const auto files = *mBacking->ListFiles();
        bool namespaced = false;
        for (const auto& [hash, path] : files) {
            namespaced |= path.starts_with(mPrefix);
        }
        // A stock archive is only accepted for its own game.
        if (!namespaced && !mNative) {
            SPDLOG_ERROR("MergeShip: {} lacks the {} namespace; archive not mounted", GetPath(), mPrefix);
            ListFiles()->clear();
            return false;
        }
        ListFiles()->clear();
        for (const auto& [hash, path] : files) {
            if (path.starts_with(mPrefix)) {
                AddPath(path, path);
                if (mNative) {
                    AddPath(path.substr(mPrefix.size()), path);
                }
            } else if (mNative && (!namespaced || IsMetadata(path))) {
                AddPath(path, path);
            } else if (!IsMetadata(path)) {
                SPDLOG_ERROR("MergeShip: {} contains unnamespaced resource {}; archive not mounted", GetPath(), path);
                mPaths.clear();
                ListFiles()->clear();
                return false;
            }
        }
        return true;
    }

    std::shared_ptr<Ship::File> LoadFile(const std::string& path) override {
        const auto it = mPaths.find(CRC64(path.c_str()));
        return it == mPaths.end() ? nullptr : mBacking->LoadFile(it->second);
    }

    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override {
        const auto it = mPaths.find(hash);
        return it == mPaths.end() ? nullptr : mBacking->LoadFile(it->second);
    }

    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override { return false; }

    bool Close() override { return mBacking->Close(); }

  private:
    static bool IsMetadata(const std::string& path) {
        return path == "version" || path == "portVersion" || path == "manifest.json";
    }
    void AddPath(const std::string& visible, const std::string& physical) {
        IndexFile(visible);
        mPaths[CRC64(visible.c_str())] = physical;
    }
    std::shared_ptr<Ship::O2rArchive> mBacking;
    std::string mPrefix;
    bool mNative;
    std::unordered_map<uint64_t, std::string> mPaths;
};
} // namespace MergeShip
