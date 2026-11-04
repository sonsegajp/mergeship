#include "Oot3dZar.h"

namespace Oot3d {

Zar Zar::Parse(Bytes bytes) {
    Zar z;
    z.bytes = std::move(bytes);
    View v(z.bytes);
    const bool gar = v.Magic(0, "GAR\x02");
    if (!gar && !v.Magic(0, "ZAR\x01")) {
        throw FormatError("not a ZAR/GAR archive");
    }
    const uint32_t numFiles = v.U16(0x0A);
    const uint32_t fileTable = v.U32(0x10);
    const uint32_t dataTable = v.U32(0x14);
    const uint32_t stride = gar ? 0x0C : 0x08;
    const uint32_t nameField = gar ? 0x08 : 0x04;
    z.files.reserve(numFiles);
    for (uint32_t i = 0; i < numFiles; i++) {
        const uint32_t size = v.U32(fileTable + i * stride);
        const uint32_t nameOff = v.U32(fileTable + i * stride + nameField);
        const uint32_t dataOff = v.U32(dataTable + i * 4);
        z.files.push_back({ v.Str(nameOff, 0x100), v.Sub(dataOff, size) });
    }
    return z;
}

const ZarFile* Zar::Find(const std::string& name) const {
    const std::string key = ToLower(name);
    for (const auto& f : files) {
        if (ToLower(f.name) == key) {
            return &f;
        }
    }
    return nullptr;
}

std::vector<const ZarFile*> Zar::WithExtension(const std::string& ext) const {
    std::vector<const ZarFile*> out;
    const std::string e = ToLower(ext);
    for (const auto& f : files) {
        if (EndsWith(ToLower(f.name), e)) {
            out.push_back(&f);
        }
    }
    return out;
}

std::string StemOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

} // namespace Oot3d
