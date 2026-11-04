#pragma once
// OoT3D asset converter: the game's files, read straight out of the ROM.
//
// Accepted sources: a .3ds/.cci card image (partition 0), a bare NCCH (.cxi/.app), or a folder holding an
// already-extracted RomFS (actor/, scene/, ...). The NCCH must be decrypted. Many decrypters leave the NoCrypto flag
// unset (the USA Rev 1 dump this was written against does), so the check is on the plaintext IVFC magic.

#include "Oot3dBinary.h"

#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Oot3d {

class RomFS {
  public:
    // Throws FormatError with a user-facing reason (encrypted, not OoT3D, ...).
    static std::unique_ptr<RomFS> Open(const std::string& path);
    ~RomFS();

    const std::string& ProductCode() const {
        return mProductCode;
    }
    enum class Game { Unknown, Ocarina3D, Majora3D };
    std::string Title() const; // "Ocarina of Time 3D (USA, v2)" etc.
    Game GameId() const;

    bool Exists(const std::string& path) const;
    uint64_t FileSize(const std::string& path) const;
    Bytes Read(const std::string& path) const; // throws if missing
    // Paths matching a simple glob (* and ?), case-insensitive, sorted; "actor/*.zar" etc.
    std::vector<std::string> List(const std::string& pattern = "*") const;

  private:
    struct Entry {
        std::string path; // original case
        uint64_t offset;  // absolute file offset, or 0 for folder sources
        uint64_t size;
    };
    RomFS() = default;
    void ParseIvfc(uint64_t romfsOffset);

    FILE* mFile = nullptr;
    std::string mDir; // folder source
    std::string mProductCode;
    uint16_t mVersion = 0;
    std::map<std::string, Entry> mEntries; // key: lower-case path
    mutable std::mutex mLock;              // Read() is used from worker threads
};

bool GlobMatch(std::string_view pattern, std::string_view text); // case-sensitive; callers lower-case both

} // namespace Oot3d
