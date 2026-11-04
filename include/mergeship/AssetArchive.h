#pragma once

#include <ship/resource/archive/O2rArchive.h>
#include <ship/resource/File.h>
#include <ship/utils/StrHash64.h>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MergeShip {

enum class AssetArchiveMode {
    // A namespaced archive of the other game: only its <namespace>/ resources are visible (SoH's view of mm.o2r).
    Foreign,
    // A namespaced archive of this game: its <namespace>/ resources, plus root aliases for the game's existing
    // generated asset headers (2Ship's view of its own mm.o2r). A stock archive is accepted as is.
    Native,
    // The other game's stock archive, namespaced as it is read (2Ship's view of SoH's oot.o2r): every entry appears
    // under <namespace>/, and the references inside each resource are rewritten to match when it loads (Rebase
    // below), so the one archive SoH extracted from the player's ROM serves both games. An archive that is already
    // namespaced (a torch --namespace export such as oot-assets.o2r) is mounted as Foreign.
    Rebase,
};

// Keep foreign resources out of the host's root namespace and version list.
// Native MM also exposes root aliases for its existing generated asset headers.
// Embedded references retain their explicit namespace in either runtime.
class AssetArchive final : public Ship::Archive {
  public:
    AssetArchive(const std::string& path, const std::string& assetNamespace, bool native = false)
        : AssetArchive(path, assetNamespace, native ? AssetArchiveMode::Native : AssetArchiveMode::Foreign) {}

    AssetArchive(const std::string& path, const std::string& assetNamespace, AssetArchiveMode mode)
        : Archive(path), mBacking(std::make_shared<Ship::O2rArchive>(path)), mPrefix(assetNamespace + "/"),
          mMode(mode) {}

    bool Open() override {
        mPaths.clear();
        mRebaseHashes.clear();
        mRebasing = false;
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
        if (mMode == AssetArchiveMode::Rebase && !namespaced) {
            // The stock game's version stays out of this game's version list: metadata is not exposed.
            mRebasing = true;
            for (const auto& [hash, path] : files) {
                if (!IsMetadata(path)) {
                    const std::string visible = mPrefix + path;
                    AddPath(visible, path);
                    mRebaseHashes[CRC64(path.c_str())] = CRC64(visible.c_str());
                }
            }
            return true;
        }
        const bool native = mMode == AssetArchiveMode::Native;
        // A stock archive is only accepted for its own game.
        if (!namespaced && !native) {
            SPDLOG_ERROR("MergeShip: {} lacks the {} namespace; archive not mounted", GetPath(), mPrefix);
            ListFiles()->clear();
            return false;
        }
        ListFiles()->clear();
        for (const auto& [hash, path] : files) {
            if (path.starts_with(mPrefix)) {
                AddPath(path, path);
                if (native) {
                    AddPath(path.substr(mPrefix.size()), path);
                }
            } else if (native && (!namespaced || IsMetadata(path))) {
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
        return LoadMapped(CRC64(path.c_str()));
    }

    std::shared_ptr<Ship::File> LoadFile(uint64_t hash) override {
        return LoadMapped(hash);
    }

    bool WriteFile(const std::string&, const std::vector<uint8_t>&) override { return false; }

    bool Close() override { return mBacking->Close(); }

    // True once Open found a stock archive and mounted it re-namespaced (AssetArchiveMode::Rebase).
    bool IsRebased() const { return mRebasing; }

  private:
    static bool IsMetadata(const std::string& path) {
        return path == "version" || path == "portVersion" || path == "manifest.json";
    }
    void AddPath(const std::string& visible, const std::string& physical) {
        IndexFile(visible);
        mPaths[CRC64(visible.c_str())] = physical;
    }

    std::shared_ptr<Ship::File> LoadMapped(uint64_t hash) {
        const auto it = mPaths.find(hash);
        if (it == mPaths.end()) {
            return nullptr;
        }
        auto file = mBacking->LoadFile(it->second);
        if (mRebasing && (file != nullptr) && (file->Buffer != nullptr)) {
            Rebase(*file->Buffer);
        }
        return file;
    }

    // Rewrites a stock resource's references to other resources into the namespace, reproducing byte for byte what
    // torch's --namespace export writes (checked over a whole oot.o2r against oot-assets.o2r by
    // tools/verify_asset_namespace.py --rebase: 39055 resources, no difference):
    //  - display lists: the 64-bit resource hash after each Fast3D opcode that carries one (the opcodes
    //    verify_asset_namespace.py normalizes) becomes the hash of the namespaced path;
    //  - every other resource but the leaf data types: each length-prefixed string that names a resource of this
    //    archive (optionally "__OTR__"-prefixed) gets the namespace inserted.
    void Rebase(std::vector<char>& buffer) const {
        constexpr size_t kHeaderSize = 64; // OTR_HEADER_SIZE
        const size_t size = buffer.size();
        if ((size < kHeaderSize) || (buffer[0] == '<')) { // XML resources carry no binary references
            return;
        }
        uint8_t* data = reinterpret_cast<uint8_t*>(buffer.data());
        const bool little = data[0] == 0;
        auto read32 = [&](size_t at) -> uint32_t {
            return little ? (uint32_t)data[at] | ((uint32_t)data[at + 1] << 8) | ((uint32_t)data[at + 2] << 16) |
                                ((uint32_t)data[at + 3] << 24)
                          : ((uint32_t)data[at] << 24) | ((uint32_t)data[at + 1] << 16) |
                                ((uint32_t)data[at + 2] << 8) | (uint32_t)data[at + 3];
        };
        auto write32 = [&](size_t at, uint32_t value) {
            for (int i = 0; i < 4; i++) {
                data[at + i] = (uint8_t)(value >> (little ? 8 * i : 24 - 8 * i));
            }
        };
        switch (read32(4)) {
            case 0x4F444C54: { // 'ODLT' display list: the commands start after the list's own 8-byte header
                for (size_t at = kHeaderSize + 8; at + 8 <= size;) {
                    const uint32_t opcode = read32(at) >> 24;
                    at += 8;
                    if ((opcode == 0x20) || (opcode == 0x31) || (opcode == 0x32) || (opcode == 0x33) ||
                        (opcode == 0x35) || (opcode == 0x36) || (opcode == 0x42)) {
                        if (at + 8 > size) {
                            break;
                        }
                        const uint64_t hash = ((uint64_t)read32(at) << 32) | read32(at + 4);
                        const auto it = mRebaseHashes.find(hash);
                        if (it != mRebaseHashes.end()) {
                            write32(at, (uint32_t)(it->second >> 32));
                            write32(at + 4, (uint32_t)it->second);
                        }
                        at += 8;
                    }
                }
                return;
            }
            case 0x4F544558: // 'OTEX' texture
            case 0x4F415252: // 'OARR' array (vertices and other raw data)
            case 0x4F534D50: // 'OSMP' audio sample
            case 0x4F565458: // 'OVTX' vertices
                return;
            default:
                break;
        }

        // Length-prefixed strings: a u32 length, then that many printable characters, ending inside one run of
        // printable bytes. A run starts at the string (little-endian lengths end in zero bytes) or up to three bytes
        // before it (big-endian lengths), so only the first four positions of each run can start one.
        auto printable = [](uint8_t c) { return (c >= 0x20) && (c <= 0x7E); };
        constexpr size_t kOtrMarkerLength = 7; // "__OTR__"
        std::vector<std::pair<size_t, std::string>> edits; // (start of the string, the namespaced string)
        for (size_t at = 0; at < size;) {
            if (!printable(data[at])) {
                at++;
                continue;
            }
            const size_t runStart = at;
            while ((at < size) && printable(data[at])) {
                at++;
            }
            const size_t runEnd = at;
            if (runEnd - runStart < 3) {
                continue;
            }
            for (size_t start = runStart; (start < runStart + 4) && (start < runEnd); start++) {
                if (start < kHeaderSize + 4) {
                    continue;
                }
                const size_t length = read32(start - 4);
                if ((length < 3) || (start + length > runEnd)) {
                    continue;
                }
                const std::string text(reinterpret_cast<const char*>(data + start), length);
                const bool marked = text.starts_with("__OTR__");
                const std::string path = marked ? text.substr(kOtrMarkerLength) : text;
                const auto it = mPaths.find(CRC64((mPrefix + path).c_str()));
                if ((it != mPaths.end()) && (it->second == path)) {
                    edits.emplace_back(start, marked ? "__OTR__" + mPrefix + path : mPrefix + path);
                    break;
                }
            }
        }
        if (edits.empty()) {
            return;
        }
        std::vector<char> out;
        out.reserve(size + edits.size() * mPrefix.size());
        size_t copied = 0;
        for (const auto& [start, text] : edits) {
            const size_t oldLength = read32(start - 4);
            out.insert(out.end(), buffer.begin() + copied, buffer.begin() + (start - 4));
            const uint32_t newLength = (uint32_t)text.size();
            for (int i = 0; i < 4; i++) {
                out.push_back((char)(uint8_t)(newLength >> (little ? 8 * i : 24 - 8 * i)));
            }
            out.insert(out.end(), text.begin(), text.end());
            copied = start + oldLength;
        }
        out.insert(out.end(), buffer.begin() + copied, buffer.end());
        buffer.swap(out);
    }

    std::shared_ptr<Ship::O2rArchive> mBacking;
    std::string mPrefix;
    AssetArchiveMode mMode;
    bool mRebasing = false;
    // Visible path hash -> the backing archive's path.
    std::unordered_map<uint64_t, std::string> mPaths;
    // Rebase: stock path hash -> namespaced path hash, for the display lists' hash references.
    std::unordered_map<uint64_t, uint64_t> mRebaseHashes;
};
} // namespace MergeShip
