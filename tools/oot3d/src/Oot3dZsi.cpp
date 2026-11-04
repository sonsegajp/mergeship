#include "Oot3dZsi.h"

#include <functional>

namespace Oot3d {

namespace {
constexpr uint8_t kCmdActors = 0x01;
constexpr uint8_t kCmdRooms = 0x04;
constexpr uint8_t kCmdMesh = 0x0A;
constexpr uint8_t kCmdSkybox = 0x11;
constexpr uint8_t kCmdEnd = 0x14;
constexpr uint8_t kCmdAltHeaders = 0x18;

// Walks one header; `fn(type, count, offset)`; offsets are relative to `h`.
void WalkHeader(View h, size_t off, const std::function<void(uint8_t, uint8_t, uint32_t)>& fn) {
    for (int guard = 0; guard < 64; guard++, off += 8) {
        const uint32_t w0 = h.U32BE(off);
        const uint32_t w1 = h.U32(off + 4);
        const uint8_t type = static_cast<uint8_t>(w0 >> 24);
        if (type == kCmdEnd) {
            return;
        }
        fn(type, static_cast<uint8_t>(w0 >> 16), w1);
    }
    throw FormatError("ZSI header without an end command");
}

std::optional<Cmb> CmbAt(View h, uint32_t off) {
    if (off == 0) {
        return std::nullopt;
    }
    return Cmb::Parse(h.From(off));
}

ZsiRoomSetup ParseRoomHeader(View h, size_t off, std::vector<size_t>* alternates) {
    ZsiRoomSetup s;
    WalkHeader(h, off, [&](uint8_t type, uint8_t count, uint32_t w1) {
        switch (type) {
            case kCmdActors:
                for (uint32_t i = 0; i < count; i++) {
                    const size_t a = w1 + i * 0x10;
                    ZsiActor act;
                    act.id = h.U16(a) & 0x0FFF;
                    for (int k = 0; k < 3; k++) {
                        act.pos[k] = h.S16(a + 2 + k * 2);
                        act.rot[k] = h.S16(a + 8 + k * 2);
                    }
                    act.params = h.U16(a + 0x0E);
                    s.actors.push_back(act);
                }
                break;
            case kCmdMesh: {
                const uint32_t hdr = h.U32BE(w1);
                const uint32_t meshType = hdr >> 24, entries = (hdr >> 16) & 0xFF;
                if (entries == 0) {
                    break;
                }
                // Like the N64's: type 0 entries are {opa, xlu}; type 2 entries add a culling sphere in front.
                const uint32_t entry = h.U32(w1 + 4);
                if (meshType == 0) {
                    s.opaque = CmbAt(h, h.U32(entry + 0x00));
                    s.translucent = CmbAt(h, h.U32(entry + 0x04));
                } else if (meshType == 2) {
                    s.opaque = CmbAt(h, h.U32(entry + 0x08));
                    s.translucent = CmbAt(h, h.U32(entry + 0x0C));
                } else {
                    throw FormatError("unexpected ZSI mesh type " + std::to_string(meshType));
                }
                break;
            }
            case kCmdAltHeaders:
                if (alternates) {
                    for (uint32_t i = 0; i < count; i++) {
                        alternates->push_back(h.U32(w1 + i * 4));
                    }
                }
                break;
            default: break;
        }
    });
    return s;
}
} // namespace

ZsiRoom ZsiRoom::Parse(Bytes bytes) {
    ZsiRoom r;
    r.bytes = std::move(bytes);
    View f(r.bytes);
    if (!f.Magic(0, "ZSI\x01") && !f.Magic(0, "ZSI\x09")) {
        throw FormatError("not a ZSI file");
    }
    r.codename = f.Str(4, 12);
    View h = f.From(0x10);
    std::vector<size_t> alternates;
    r.setups.push_back(ParseRoomHeader(h, 0, &alternates));
    for (size_t off : alternates) {
        r.setups.push_back(off ? ParseRoomHeader(h, off, nullptr) : ZsiRoomSetup{});
    }
    return r;
}

ZsiScene ZsiScene::Parse(const Bytes& bytes) {
    ZsiScene s;
    View f(bytes);
    if (!f.Magic(0, "ZSI\x01") && !f.Magic(0, "ZSI\x09")) {
        throw FormatError("not a ZSI file");
    }
    s.codename = f.Str(4, 12);
    View h = f.From(0x10);
    const bool ocarina = f.Magic(0, "ZSI\x01");
    WalkHeader(h, 0, [&](uint8_t type, uint8_t count, uint32_t w1) {
        if (type == kCmdRooms) {
            const size_t stride = ocarina ? 0x44 : 0x34;
            for (uint32_t i = 0; i < count; i++) {
                s.rooms.push_back(h.Str(w1 + i * stride, stride));
            }
        } else if (type == kCmdSkybox) {
            s.skyboxId = static_cast<uint8_t>(w1);
            s.lightMode = static_cast<uint8_t>(w1 >> 16);
        } else if (type == 0x0F && ocarina) {
            for (uint32_t i = 0; i < count; i++) {
                const size_t o = w1 + i * 0x1C;
                ZsiEnvironment e;
                e.drawDistance = h.F32(o + 0x00);
                e.fogEnd = h.F32(o + 0x04);
                e.fogStart = h.U16(o + 0x08) & 0x3FF;
                for (int k = 0; k < 3; k++) {
                    e.ambient[k] = h.U8(o + 0x0A + k);
                    e.light0Dir[k] = h.S8(o + 0x0D + k);
                    e.light0Color[k] = h.U8(o + 0x10 + k);
                    e.light1Dir[k] = h.S8(o + 0x13 + k);
                    e.light1Color[k] = h.U8(o + 0x16 + k);
                    e.fogColor[k] = h.U8(o + 0x19 + k);
                }
                s.environments.push_back(e);
            }
        }
    });
    return s;
}

} // namespace Oot3d
