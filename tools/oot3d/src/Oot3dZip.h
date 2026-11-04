#pragma once
// OoT3D asset converter: .o2r archives (plain zip files) through zlib.
// ZipWriter streams each file to disk as it is added, so a large mod never has to sit in memory.

#include "Oot3dBinary.h"

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Oot3d {

class ZipReader {
  public:
    static std::unique_ptr<ZipReader> Open(const std::string& path); // throws FormatError
    ~ZipReader();

    bool Has(const std::string& name) const {
        return mEntries.count(name) != 0;
    }
    std::optional<Bytes> Read(const std::string& name) const;
    const std::vector<std::string>& Names() const {
        return mNames;
    }
    // Resource path hash (PathHash) -> name, for decoding the hash references inside display lists.
    const std::string* NameOfHash(uint64_t hash) const;

  private:
    struct Entry {
        uint64_t localHeader;
        uint32_t compSize, size;
        uint16_t method;
    };
    ZipReader() = default;

    FILE* mFile = nullptr;
    std::unordered_map<std::string, Entry> mEntries;
    std::vector<std::string> mNames;
    std::unordered_map<uint64_t, std::string> mHashes;
    mutable std::mutex mLock;
};

class ZipWriter {
  public:
    // Writes to `path` + ".tmp" and renames on Close(), so a failed build never leaves a half archive in mods/.
    explicit ZipWriter(const std::string& path, int level = 6);
    ~ZipWriter();

    // Thread-safe. Adding the same name twice keeps the first.
    void Add(const std::string& name, const Bytes& data);
    bool Has(const std::string& name) const;
    size_t Count() const;
    uint64_t BytesWritten() const {
        return mOffset;
    }
    void Close(); // throws on I/O failure
    void Abort(); // drops the temp file

  private:
    struct Central {
        std::string name;
        uint32_t crc, compSize, size, offset;
        uint16_t method;
    };
    std::string mPath, mTemp;
    FILE* mFile = nullptr;
    int mLevel;
    uint64_t mOffset = 0;
    std::vector<Central> mCentral;
    std::map<std::string, size_t> mIndex;
    mutable std::mutex mLock;
};

} // namespace Oot3d
