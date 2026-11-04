#include "Oot3dZip.h"

#include <filesystem>
#include <memory>
#include <zlib.h>

namespace Oot3d {

namespace fs = std::filesystem;

static FILE* OpenFile(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::wstring wmode(mode, mode + strlen(mode));
    return _wfopen(U8Path(path).wstring().c_str(), wmode.c_str());
#else
    return fopen(path.c_str(), mode);
#endif
}

static int SeekTo(FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, static_cast<__int64>(off), SEEK_SET);
#else
    return fseeko(f, static_cast<off_t>(off), SEEK_SET);
#endif
}

static uint64_t FileLength(FILE* f) {
#ifdef _WIN32
    _fseeki64(f, 0, SEEK_END);
    return static_cast<uint64_t>(_ftelli64(f));
#else
    fseeko(f, 0, SEEK_END);
    return static_cast<uint64_t>(ftello(f));
#endif
}

// -- reader --------------------------------------------------------------------------------------------------------

std::unique_ptr<ZipReader> ZipReader::Open(const std::string& path) {
    std::unique_ptr<ZipReader> z(new ZipReader());
    z->mFile = OpenFile(path, "rb");
    if (!z->mFile) {
        throw FormatError("cannot open " + path);
    }
    const uint64_t len = FileLength(z->mFile);
    const uint64_t tail = std::min<uint64_t>(len, 0x10000 + 22);
    Bytes buf(static_cast<size_t>(tail));
    SeekTo(z->mFile, len - tail);
    if (fread(buf.data(), 1, buf.size(), z->mFile) != buf.size()) {
        throw FormatError(path + ": read error");
    }
    size_t eocd = std::string::npos;
    for (size_t i = buf.size() >= 22 ? buf.size() - 22 : 0;; i--) {
        if (buf[i] == 0x50 && buf[i + 1] == 0x4B && buf[i + 2] == 0x05 && buf[i + 3] == 0x06) {
            eocd = i;
            break;
        }
        if (i == 0) {
            break;
        }
    }
    if (eocd == std::string::npos) {
        throw FormatError(path + " is not a zip/.o2r archive");
    }
    View e(buf);
    const uint32_t count = e.U16(eocd + 10);
    const uint32_t cdSize = e.U32(eocd + 12);
    const uint32_t cdOff = e.U32(eocd + 16);
    Bytes cd(cdSize);
    SeekTo(z->mFile, cdOff);
    if (fread(cd.data(), 1, cd.size(), z->mFile) != cd.size()) {
        throw FormatError(path + ": truncated central directory");
    }
    View c(cd);
    size_t o = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (c.U32(o) != 0x02014B50) {
            throw FormatError(path + ": bad central directory");
        }
        Entry en;
        en.method = c.U16(o + 10);
        en.compSize = c.U32(o + 20);
        en.size = c.U32(o + 24);
        const uint32_t nameLen = c.U16(o + 28), extraLen = c.U16(o + 30), commentLen = c.U16(o + 32);
        en.localHeader = c.U32(o + 42);
        std::string name(reinterpret_cast<const char*>(c.Sub(o + 46, nameLen).Data()), nameLen);
        o += 46 + nameLen + extraLen + commentLen;
        if (!name.empty() && name.back() == '/') {
            continue;
        }
        z->mHashes.emplace(PathHash(name), name);
        z->mNames.push_back(name);
        z->mEntries.emplace(std::move(name), en);
    }
    return z;
}

ZipReader::~ZipReader() {
    if (mFile) {
        fclose(mFile);
    }
}

const std::string* ZipReader::NameOfHash(uint64_t hash) const {
    auto it = mHashes.find(hash);
    return it == mHashes.end() ? nullptr : &it->second;
}

std::optional<Bytes> ZipReader::Read(const std::string& name) const {
    auto it = mEntries.find(name);
    if (it == mEntries.end()) {
        return std::nullopt;
    }
    const Entry& en = it->second;
    Bytes comp;
    {
        std::lock_guard<std::mutex> lock(mLock);
        uint8_t local[30];
        SeekTo(mFile, en.localHeader);
        if (fread(local, 1, 30, mFile) != 30) {
            return std::nullopt;
        }
        const uint32_t skip = uint32_t(local[26] | (local[27] << 8)) + uint32_t(local[28] | (local[29] << 8));
        comp.resize(en.compSize);
        SeekTo(mFile, en.localHeader + 30 + skip);
        if (en.compSize && fread(comp.data(), 1, comp.size(), mFile) != comp.size()) {
            return std::nullopt;
        }
    }
    if (en.method == 0) {
        return comp;
    }
    if (en.method != 8) {
        return std::nullopt;
    }
    Bytes out(en.size);
    z_stream zs{};
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        return std::nullopt;
    }
    zs.next_in = comp.data();
    zs.avail_in = static_cast<uInt>(comp.size());
    zs.next_out = out.data();
    zs.avail_out = static_cast<uInt>(out.size());
    const int rc = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (rc != Z_STREAM_END) {
        return std::nullopt;
    }
    return out;
}

// -- writer --------------------------------------------------------------------------------------------------------

ZipWriter::ZipWriter(const std::string& path, int level) : mPath(path), mTemp(path + ".tmp"), mLevel(level) {
    std::error_code ec;
    fs::create_directories(U8Path(path).parent_path(), ec);
    mFile = OpenFile(mTemp, "wb");
    if (!mFile) {
        throw FormatError("cannot write " + mTemp);
    }
}

ZipWriter::~ZipWriter() {
    if (mFile) {
        Abort();
    }
}

bool ZipWriter::Has(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mLock);
    return mIndex.count(name) != 0;
}

size_t ZipWriter::Count() const {
    std::lock_guard<std::mutex> lock(mLock);
    return mCentral.size();
}

void ZipWriter::Add(const std::string& name, const Bytes& data) {
    if (Has(name)) {
        return;
    }
    const uint32_t crc = static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size())));
    Bytes comp;
    uint16_t method = 0;
    if (mLevel > 0 && data.size() > 64) {
        z_stream zs{};
        deflateInit2(&zs, mLevel, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        comp.resize(deflateBound(&zs, static_cast<uLong>(data.size())));
        zs.next_in = const_cast<uint8_t*>(data.data());
        zs.avail_in = static_cast<uInt>(data.size());
        zs.next_out = comp.data();
        zs.avail_out = static_cast<uInt>(comp.size());
        const int rc = deflate(&zs, Z_FINISH);
        comp.resize(zs.total_out);
        deflateEnd(&zs);
        if (rc == Z_STREAM_END && comp.size() < data.size()) {
            method = 8;
        }
    }
    const Bytes& payload = method == 8 ? comp : data;

    std::lock_guard<std::mutex> lock(mLock);
    if (mIndex.count(name) || !mFile) {
        return;
    }
    if (mOffset + payload.size() + name.size() + 30 > 0xFFFFFFFFull || mCentral.size() >= 0xFFFF) {
        throw FormatError("the archive would exceed the zip limits (4 GB / 65535 files)");
    }
    Writer h;
    h.U32(0x04034B50);
    h.U16(20);
    h.U16(0);
    h.U16(method);
    h.U16(0);      // time
    h.U16(0x21);   // date: 1980-01-01
    h.U32(crc);
    h.U32(static_cast<uint32_t>(payload.size()));
    h.U32(static_cast<uint32_t>(data.size()));
    h.U16(static_cast<uint16_t>(name.size()));
    h.U16(0);
    h.Raw(name.data(), name.size());
    if (fwrite(h.Out().data(), 1, h.Size(), mFile) != h.Size() ||
        (!payload.empty() && fwrite(payload.data(), 1, payload.size(), mFile) != payload.size())) {
        throw FormatError("write error on " + mTemp + " (disk full?)");
    }
    mIndex[name] = mCentral.size();
    mCentral.push_back({ name, crc, static_cast<uint32_t>(payload.size()), static_cast<uint32_t>(data.size()),
                         static_cast<uint32_t>(mOffset), method });
    mOffset += h.Size() + payload.size();
}

void ZipWriter::Close() {
    std::lock_guard<std::mutex> lock(mLock);
    if (!mFile) {
        return;
    }
    Writer cd;
    for (const Central& c : mCentral) {
        cd.U32(0x02014B50);
        cd.U16(20);
        cd.U16(20);
        cd.U16(0);
        cd.U16(c.method);
        cd.U16(0);
        cd.U16(0x21);
        cd.U32(c.crc);
        cd.U32(c.compSize);
        cd.U32(c.size);
        cd.U16(static_cast<uint16_t>(c.name.size()));
        cd.U16(0);
        cd.U16(0);
        cd.U16(0);
        cd.U16(0);
        cd.U32(0);
        cd.U32(c.offset);
        cd.Raw(c.name.data(), c.name.size());
    }
    cd.U32(0x06054B50);
    cd.U16(0);
    cd.U16(0);
    cd.U16(static_cast<uint16_t>(mCentral.size()));
    cd.U16(static_cast<uint16_t>(mCentral.size()));
    cd.U32(static_cast<uint32_t>(cd.Size() - 0)); // patched below
    cd.U32(static_cast<uint32_t>(mOffset));
    cd.U16(0);
    Bytes& out = cd.Out();
    const uint32_t cdSize = static_cast<uint32_t>(out.size() - 22);
    memcpy(out.data() + out.size() - 10, &cdSize, 4);
    const bool ok = fwrite(out.data(), 1, out.size(), mFile) == out.size();
    const bool closed = fclose(mFile) == 0;
    mFile = nullptr;
    if (!ok || !closed) {
        std::error_code ec;
        fs::remove(U8Path(mTemp), ec);
        throw FormatError("write error on " + mTemp);
    }
    std::error_code ec;
    fs::rename(U8Path(mTemp), U8Path(mPath), ec);
    if (ec) {
        fs::remove(U8Path(mPath), ec);
        fs::rename(U8Path(mTemp), U8Path(mPath), ec);
        if (ec) {
            throw FormatError("cannot replace " + mPath + ": " + ec.message() + " (is the game using it?)");
        }
    }
}

void ZipWriter::Abort() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mFile) {
        fclose(mFile);
        mFile = nullptr;
    }
    std::error_code ec;
    fs::remove(U8Path(mTemp), ec);
}

} // namespace Oot3d
