#include "Oot3dBinary.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Oot3d {

static const std::array<uint64_t, 256>& Crc64Table() {
    static const std::array<uint64_t, 256> table = [] {
        std::array<uint64_t, 256> t{};
        const uint64_t poly = 0x42F0E1EBA9EA3693ULL;
        for (uint64_t i = 0; i < 256; i++) {
            uint64_t crc = i << 56;
            for (int b = 0; b < 8; b++) {
                crc = (crc & 0x8000000000000000ULL) ? (crc << 1) ^ poly : (crc << 1);
            }
            t[i] = crc;
        }
        return t;
    }();
    return table;
}

uint64_t PathHash(std::string_view path) {
    const auto& table = Crc64Table();
    uint64_t crc = 0xFFFFFFFFFFFFFFFFULL;
    for (unsigned char c : path) {
        crc = table[static_cast<uint8_t>(crc >> 56) ^ c] ^ (crc << 8);
    }
    return crc;
}

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool EndsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool StartsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace Oot3d
