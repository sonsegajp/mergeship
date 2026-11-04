#include "Oot3dRom.h"

#include <filesystem>
#include <fstream>

namespace Oot3d {

static constexpr uint64_t kMediaUnit = 0x200;

static int Seek64(FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, static_cast<__int64>(off), SEEK_SET);
#else
    return fseeko(f, static_cast<off_t>(off), SEEK_SET);
#endif
}

static Bytes ReadAt(FILE* f, uint64_t off, size_t size) {
    Bytes out(size);
    if (Seek64(f, off) != 0 || (size != 0 && fread(out.data(), 1, size, f) != size)) {
        throw FormatError("the ROM file is truncated");
    }
    return out;
}

static std::string Utf16ToUtf8(const uint8_t* p, size_t bytes) {
    std::string s;
    for (size_t i = 0; i + 1 < bytes; i += 2) {
        uint32_t c = p[i] | (p[i + 1] << 8);
        if (c < 0x80) {
            s.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (c >> 6)));
            s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xE0 | (c >> 12)));
            s.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return s;
}

bool GlobMatch(std::string_view p, std::string_view t) {
    size_t pi = 0, ti = 0, star = std::string_view::npos, mark = 0;
    while (ti < t.size()) {
        if (pi < p.size() && (p[pi] == '?' || p[pi] == t[ti])) {
            pi++;
            ti++;
        } else if (pi < p.size() && p[pi] == '*') {
            star = pi++;
            mark = ti;
        } else if (star != std::string_view::npos) {
            pi = star + 1;
            ti = ++mark;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == '*') {
        pi++;
    }
    return pi == p.size();
}

std::unique_ptr<RomFS> RomFS::Open(const std::string& path) {
    std::unique_ptr<RomFS> rom(new RomFS());
    namespace fs = std::filesystem;
    const fs::path fsPath = U8Path(path);

    if (fs::is_directory(fsPath)) {
        rom->mDir = PathString(fsPath);
        for (const auto& e : fs::recursive_directory_iterator(fsPath)) {
            if (!e.is_regular_file()) {
                continue;
            }
            std::string rel = PathString(fs::relative(e.path(), fsPath));
            rom->mEntries[ToLower(rel)] = { rel, 0, static_cast<uint64_t>(e.file_size()) };
        }
        if (rom->Exists("scene/spot00_info.zsi")) {
            rom->mProductCode = "CTR-P-AQE-folder";
        } else if (!rom->List("scene/z2_*").empty()) {
            rom->mProductCode = "CTR-P-AJR-folder";
        } else {
            throw FormatError("that folder is not an Ocarina of Time 3D or Majora's Mask 3D RomFS");
        }
        return rom;
    }

#ifdef _WIN32
    rom->mFile = _wfopen(fsPath.wstring().c_str(), L"rb");
#else
    rom->mFile = fopen(path.c_str(), "rb");
#endif
    if (rom->mFile == nullptr) {
        throw FormatError("cannot open " + path);
    }

    Bytes head = ReadAt(rom->mFile, 0, 0x200);
    uint64_t ncchBase = 0;
    if (memcmp(head.data() + 0x100, "NCSD", 4) == 0) {
        ncchBase = uint64_t(View(head).U32(0x120)) * kMediaUnit;
        head = ReadAt(rom->mFile, ncchBase, 0x200);
    }
    if (memcmp(head.data() + 0x100, "NCCH", 4) != 0) {
        throw FormatError("not a 3DS game image (.3ds, .cci or .cxi)");
    }
    View h(head);
    rom->mProductCode = h.Str(0x150, 16);
    rom->mVersion = h.U16(0x112);
    const uint64_t romfsOff = ncchBase + uint64_t(h.U32(0x1B0)) * kMediaUnit;
    const uint64_t romfsSize = uint64_t(h.U32(0x1B4)) * kMediaUnit;
    if (romfsSize == 0) {
        throw FormatError("the game image has no RomFS");
    }
    if (rom->GameId() == Game::Unknown) {
        throw FormatError("this is " + rom->mProductCode +
                          ", not The Legend of Zelda: Ocarina of Time 3D or Majora's Mask 3D");
    }
    Bytes magic = ReadAt(rom->mFile, romfsOff, 4);
    if (memcmp(magic.data(), "IVFC", 4) != 0) {
        throw FormatError("the ROM is encrypted. Decrypt it first (for example GodMode9's \"Decrypt file\"), then "
                          "select the decrypted .3ds");
    }
    rom->ParseIvfc(romfsOff);
    return rom;
}

void RomFS::ParseIvfc(uint64_t romfsOff) {
    Bytes ivfc = ReadAt(mFile, romfsOff, 0x60);
    View iv(ivfc);
    const uint64_t masterHashSize = iv.U32(0x08);
    const uint64_t block = uint64_t(1) << iv.U32(0x4C);
    const uint64_t lvl3 = romfsOff + ((0x60 + masterHashSize + block - 1) & ~(block - 1));

    Bytes hdrBytes = ReadAt(mFile, lvl3, 0x28);
    View hdr(hdrBytes);
    const uint32_t dirMetaOff = hdr.U32(0x0C), dirMetaSize = hdr.U32(0x10);
    const uint32_t fileMetaOff = hdr.U32(0x1C), fileMetaSize = hdr.U32(0x20);
    const uint32_t fileDataOff = hdr.U32(0x24);
    Bytes dmetaBytes = ReadAt(mFile, lvl3 + dirMetaOff, dirMetaSize);
    Bytes fmetaBytes = ReadAt(mFile, lvl3 + fileMetaOff, fileMetaSize);
    View dmeta(dmetaBytes), fmeta(fmetaBytes);

    std::map<uint32_t, std::string> dirPaths;
    std::vector<std::pair<uint32_t, std::string>> stack{ { 0u, std::string() } };
    while (!stack.empty()) {
        auto [off, base] = stack.back();
        stack.pop_back();
        const uint32_t nameLen = dmeta.U32(off + 0x14);
        View nameView = dmeta.Sub(off + 0x18, nameLen);
        std::string name = Utf16ToUtf8(nameView.Data(), nameLen);
        std::string path = name.empty() ? base : (base.empty() ? name : base + "/" + name);
        dirPaths[off] = path;
        for (uint32_t child = dmeta.U32(off + 0x08); child != 0xFFFFFFFF; child = dmeta.U32(child + 0x04)) {
            stack.emplace_back(child, path);
        }
    }

    const uint64_t dataBase = lvl3 + fileDataOff;
    for (uint32_t off = 0; off + 0x20 <= fileMetaSize;) {
        const uint32_t parent = fmeta.U32(off);
        const uint64_t dataOff = fmeta.U64(off + 0x08);
        const uint64_t dataSize = fmeta.U64(off + 0x10);
        const uint32_t nameLen = fmeta.U32(off + 0x1C);
        View nameView = fmeta.Sub(off + 0x20, nameLen);
        std::string name = Utf16ToUtf8(nameView.Data(), nameLen);
        const std::string& dir = dirPaths[parent];
        std::string full = dir.empty() ? name : dir + "/" + name;
        mEntries[ToLower(full)] = { full, dataBase + dataOff, dataSize };
        off += (0x20 + nameLen + 3) & ~3u;
    }
}

RomFS::~RomFS() {
    if (mFile) {
        fclose(mFile);
    }
}

std::string RomFS::Title() const {
    static const std::map<char, std::string> kRegions = {
        { 'E', "USA" }, { 'P', "Europe" }, { 'J', "Japan" }, { 'K', "Korea" }, { 'Z', "Taiwan" }, { 'C', "China" },
    };
    const Game g = GameId();
    if (g == Game::Unknown) {
        return mProductCode;
    }
    const std::string name = g == Game::Ocarina3D ? "Ocarina of Time 3D" : "Majora's Mask 3D";
    if (!mDir.empty()) {
        return name + " (RomFS folder)";
    }
    auto it = kRegions.find(mProductCode.size() > 9 ? mProductCode[9] : '?');
    return name + " (" + (it == kRegions.end() ? mProductCode : it->second) +
           (mVersion ? ", v" + std::to_string(mVersion) : "") + ")";
}

RomFS::Game RomFS::GameId() const {
    if (StartsWith(mProductCode, "CTR-P-AQE")) {
        return Game::Ocarina3D;
    }
    if (StartsWith(mProductCode, "CTR-P-AJR")) {
        return Game::Majora3D;
    }
    return Game::Unknown;
}

bool RomFS::Exists(const std::string& path) const {
    return mEntries.count(ToLower(path)) != 0;
}

uint64_t RomFS::FileSize(const std::string& path) const {
    auto it = mEntries.find(ToLower(path));
    return it == mEntries.end() ? 0 : it->second.size;
}

Bytes RomFS::Read(const std::string& path) const {
    auto it = mEntries.find(ToLower(path));
    if (it == mEntries.end()) {
        throw FormatError("the ROM has no " + path);
    }
    if (!mDir.empty()) {
        std::ifstream in(U8Path(mDir + "/" + it->second.path), std::ios::binary);
        Bytes out(static_cast<size_t>(it->second.size));
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        return out;
    }
    std::lock_guard<std::mutex> lock(mLock);
    return ReadAt(mFile, it->second.offset, static_cast<size_t>(it->second.size));
}

std::vector<std::string> RomFS::List(const std::string& pattern) const {
    const std::string pat = ToLower(pattern);
    std::vector<std::string> out;
    for (const auto& [key, e] : mEntries) {
        if (GlobMatch(pat, key)) {
            out.push_back(e.path);
        }
    }
    return out;
}

} // namespace Oot3d
