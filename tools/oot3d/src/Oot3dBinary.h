#pragma once
// OoT3D asset converter: helpers for reading and writing packed little-endian data.
// tools/oot3d/src is plain C++ (plus zlib) with no SoH dependencies, so the same files can later be compiled into
// soh.dll for extraction at runtime. For now they build tools/oot3d's standalone oot3d-extract.exe.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Oot3d {

using Bytes = std::vector<uint8_t>;

struct FormatError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// A bounds-checked view over a byte buffer. Offsets are relative to the view.
class View {
  public:
    View() = default;
    View(const uint8_t* data, size_t size) : mData(data), mSize(size) {
    }
    explicit View(const Bytes& b) : mData(b.data()), mSize(b.size()) {
    }

    const uint8_t* Data() const {
        return mData;
    }
    size_t Size() const {
        return mSize;
    }
    bool Empty() const {
        return mSize == 0;
    }

    View Sub(size_t off, size_t size) const {
        Check(off, size);
        return View(mData + off, size);
    }
    View From(size_t off) const {
        Check(off, 0);
        return View(mData + off, mSize - off);
    }

    template <typename T> T Get(size_t off) const {
        Check(off, sizeof(T));
        T v;
        memcpy(&v, mData + off, sizeof(T));
        return v;
    }
    uint8_t U8(size_t off) const {
        return Get<uint8_t>(off);
    }
    int8_t S8(size_t off) const {
        return Get<int8_t>(off);
    }
    uint16_t U16(size_t off) const {
        return Get<uint16_t>(off);
    }
    int16_t S16(size_t off) const {
        return Get<int16_t>(off);
    }
    uint32_t U32(size_t off) const {
        return Get<uint32_t>(off);
    }
    int32_t S32(size_t off) const {
        return Get<int32_t>(off);
    }
    uint64_t U64(size_t off) const {
        return Get<uint64_t>(off);
    }
    float F32(size_t off) const {
        return Get<float>(off);
    }
    // Big-endian u32 (the ZSI command word 0 and CMB colors are stored that way).
    uint32_t U32BE(size_t off) const {
        Check(off, 4);
        return (uint32_t(mData[off]) << 24) | (uint32_t(mData[off + 1]) << 16) | (uint32_t(mData[off + 2]) << 8) |
               uint32_t(mData[off + 3]);
    }
    bool Magic(size_t off, std::string_view m) const {
        return off + m.size() <= mSize && memcmp(mData + off, m.data(), m.size()) == 0;
    }
    // NUL-terminated string of at most `limit` bytes.
    std::string Str(size_t off, size_t limit) const {
        Check(off, 0);
        size_t n = 0;
        while (n < limit && off + n < mSize && mData[off + n] != 0) {
            n++;
        }
        return std::string(reinterpret_cast<const char*>(mData + off), n);
    }

  private:
    void Check(size_t off, size_t size) const {
        if (off > mSize || size > mSize - off) {
            throw FormatError("read of " + std::to_string(size) + " bytes at 0x" + Hex(off) + " past the end (0x" +
                              Hex(mSize) + ")");
        }
    }
    static std::string Hex(size_t v) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%zX", v);
        return buf;
    }

    const uint8_t* mData = nullptr;
    size_t mSize = 0;
};

// Appends little-endian values (SoH resources are little-endian).
class Writer {
  public:
    Bytes& Out() {
        return mOut;
    }
    size_t Size() const {
        return mOut.size();
    }
    void U8(uint8_t v) {
        mOut.push_back(v);
    }
    void S8(int8_t v) {
        mOut.push_back(static_cast<uint8_t>(v));
    }
    template <typename T> void Put(T v) {
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        mOut.insert(mOut.end(), p, p + sizeof(T));
    }
    void U16(uint16_t v) {
        Put(v);
    }
    void S16(int16_t v) {
        Put(v);
    }
    void U32(uint32_t v) {
        Put(v);
    }
    void S32(int32_t v) {
        Put(v);
    }
    void U64(uint64_t v) {
        Put(v);
    }
    void F32(float v) {
        Put(v);
    }
    void Raw(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        mOut.insert(mOut.end(), b, b + n);
    }
    // LUS BinaryWriter::Write(std::string): u32 length, then the bytes.
    void LpStr(std::string_view s) {
        U32(static_cast<uint32_t>(s.size()));
        Raw(s.data(), s.size());
    }
    void Pad(size_t align, uint8_t fill = 0) {
        while (mOut.size() % align) {
            mOut.push_back(fill);
        }
    }

  private:
    Bytes mOut;
};

// LUS resource path hash (libultraship StrHash64.cpp CRC64(const char*): ECMA-182, MSB first, init ~0, no final xor).
uint64_t PathHash(std::string_view path);

inline size_t AlignUp(size_t v, size_t a) {
    return (v + a - 1) & ~(a - 1);
}

std::string ToLower(std::string s);

// UTF-8 std::string <-> filesystem path (u8path is deprecated in C++20).
inline std::filesystem::path U8Path(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}
inline std::string PathString(const std::filesystem::path& p) {
    const std::u8string u = p.generic_u8string();
    return std::string(u.begin(), u.end());
}
bool EndsWith(std::string_view s, std::string_view suffix);
bool StartsWith(std::string_view s, std::string_view prefix);

} // namespace Oot3d
