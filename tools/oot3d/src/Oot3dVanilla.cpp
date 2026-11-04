#include "Oot3dVanilla.h"

#include "Oot3dSoh.h"

#include <algorithm>
#include <functional>
#include <set>

namespace Oot3d {

namespace {
constexpr uint32_t kTypeScene = 0x4F524F4D; // OROM: SoH's Room and Scene resources share it

struct CommandSpan {
    uint8_t id;
    size_t start, end;
};

class Cursor {
  public:
    explicit Cursor(const Bytes& b, size_t pos) : mView(b), mPos(pos) {
    }
    uint8_t U8() {
        return mView.U8(Adv(1));
    }
    int8_t S8() {
        return mView.S8(Adv(1));
    }
    uint16_t U16() {
        return mView.U16(Adv(2));
    }
    int16_t S16() {
        return mView.S16(Adv(2));
    }
    uint32_t U32() {
        return mView.U32(Adv(4));
    }
    int32_t S32() {
        return mView.S32(Adv(4));
    }
    std::string Str() {
        const uint32_t n = U32();
        View s = mView.Sub(Adv(n), n);
        return std::string(reinterpret_cast<const char*>(s.Data()), n);
    }
    void Skip(size_t n) {
        Adv(n);
    }
    size_t Pos() const {
        return mPos;
    }

  private:
    size_t Adv(size_t n) {
        const size_t p = mPos;
        mPos += n;
        return p;
    }
    View mView;
    size_t mPos;
};

void ParseMesh(Cursor& c, VanillaRoom& room) {
    c.S8(); // data
    room.meshType = c.S8();
    if (room.meshType == 1) {
        c.U8(); // format
        c.Str();
        c.Str();
        const uint32_t images = c.U32();
        for (uint32_t k = 0; k < images; k++) {
            c.Skip(3);
            c.Str();
            c.Skip(4 + 4 + 2 + 2 + 1 + 1 + 2 + 2);
        }
        c.S8();
        VanillaMeshEntry e;
        e.opa = c.Str();
        e.xlu = c.Str();
        room.entries.push_back(e);
        return;
    }
    const int count = c.U8();
    for (int k = 0; k < count; k++) {
        VanillaMeshEntry e;
        c.S8(); // polyType
        if (room.meshType == 2) {
            e.hasSphere = true;
            e.x = c.S16();
            e.y = c.S16();
            e.z = c.S16();
            e.radius = c.S16();
        }
        e.opa = c.Str();
        e.xlu = c.Str();
        room.entries.push_back(e);
    }
}

// Parses a Scene/Room command list with the layouts of SoH's scene command factories, recording each command's byte
// span; fills `room`/`scene` with the parts they describe.
std::vector<CommandSpan> Scan(const Bytes& b, VanillaRoom* room, VanillaScene* scene) {
    auto hdr = ParseResourceHeader(b);
    if (!hdr || hdr->type != kTypeScene) {
        throw FormatError("not a Scene/Room resource");
    }
    VanillaRoom scratchRoom;
    VanillaScene scratchScene;
    VanillaRoom& r = room ? *room : scratchRoom;
    VanillaScene& s = scene ? *scene : scratchScene;
    std::vector<CommandSpan> spans;
    Cursor c(b, 64);
    const uint32_t count = c.U32();
    for (uint32_t i = 0; i < count; i++) {
        const size_t start = c.Pos();
        const int32_t id = c.S32();
        switch (id) {
            case 0x00: // start positions
            case 0x01: // actors
            case 0x0E: // transition actors
            {
                const uint32_t n = c.U32();
                c.Skip(size_t(n) * 16);
                break;
            }
            case 0x02: // cs camera
            case 0x08: // room behavior
            case 0x19: // camera settings
                c.Skip(5);
                break;
            case 0x03: // collision
            case 0x17: // cutscenes
                c.Str();
                break;
            case 0x04: { // room list
                const uint32_t n = c.U32();
                for (uint32_t k = 0; k < n; k++) {
                    s.rooms.push_back(c.Str());
                    c.Skip(8);
                }
                break;
            }
            case 0x05: c.Skip(4); break; // wind
            case 0x06:                   // entrances
            case 0x0B:                   // objects
            case 0x13: {                 // exits
                const uint32_t n = c.U32();
                c.Skip(size_t(n) * 2);
                break;
            }
            case 0x07: c.Skip(3); break; // special objects
            case 0x0A: ParseMesh(c, r); break;
            case 0x0C: { // light list
                const uint32_t n = c.U32();
                c.Skip(size_t(n) * 13);
                break;
            }
            case 0x0D: { // pathways
                const uint32_t n = c.U32();
                for (uint32_t k = 0; k < n; k++) {
                    c.Str();
                }
                break;
            }
            case 0x0F: { // lighting settings
                const uint32_t n = c.U32();
                for (uint32_t k = 0; k < n; k++) {
                    VanillaScene::Light l;
                    for (auto* arr : { &l.ambient, &l.dir1, &l.color1, &l.dir2, &l.color2, &l.fog }) {
                        for (auto& v : *arr) {
                            v = c.U8();
                        }
                    }
                    l.fogNear = c.S16();
                    l.fogFar = c.U16();
                    s.lights.push_back(l);
                }
                break;
            }
            case 0x10: c.Skip(3); break; // time
            case 0x11: c.Skip(4); break; // skybox
            case 0x12: c.Skip(2); break; // skybox modifier
            case 0x14: break;            // end
            case 0x15: c.Skip(3); break; // sound
            case 0x16: c.Skip(1); break; // echo
            case 0x18: {                 // alternate headers
                const uint32_t n = c.U32();
                for (uint32_t k = 0; k < n; k++) {
                    std::string h = c.Str();
                    r.alternateHeaders.push_back(h);
                    s.alternateHeaders.push_back(h);
                }
                break;
            }
            default: throw FormatError("unknown scene command " + std::to_string(id));
        }
        spans.push_back({ static_cast<uint8_t>(id), start, c.Pos() });
    }
    return spans;
}
} // namespace

std::optional<ResourceHeaderInfo> ParseResourceHeader(const Bytes& b) {
    if (b.size() < 64) {
        return std::nullopt;
    }
    View v(b);
    return ResourceHeaderInfo{ v.U32(4), v.U32(8) };
}

VanillaRoom ParseVanillaRoom(const Bytes& b) {
    VanillaRoom room;
    Scan(b, &room, nullptr);
    return room;
}

VanillaScene ParseVanillaScene(const Bytes& b) {
    VanillaScene scene;
    Scan(b, nullptr, &scene);
    return scene;
}

Bytes RewriteRoomMesh(const Bytes& room, int meshType, const std::vector<VanillaMeshEntry>& entries) {
    const auto spans = Scan(room, nullptr, nullptr);
    Writer w;
    w.Raw(room.data(), 64);
    w.U32(static_cast<uint32_t>(spans.size()));
    bool replaced = false;
    for (const CommandSpan& sp : spans) {
        if (sp.id != 0x0A) {
            w.Raw(room.data() + sp.start, sp.end - sp.start);
            continue;
        }
        replaced = true;
        w.S32(0x0A);
        w.S8(static_cast<int8_t>(room[sp.start + 4])); // data
        w.S8(static_cast<int8_t>(meshType));
        w.U8(static_cast<uint8_t>(entries.size()));
        for (const VanillaMeshEntry& e : entries) {
            w.S8(0);
            if (meshType == 2) {
                w.S16(e.x);
                w.S16(e.y);
                w.S16(e.z);
                w.S16(e.radius);
            }
            w.LpStr(e.opa);
            w.LpStr(e.xlu);
        }
    }
    if (!replaced) {
        throw FormatError("room has no mesh command");
    }
    return w.Out();
}

VanillaDlInfo InspectVanillaDl(const ZipReader& oot, const std::string& path, bool wantPositions) {
    VanillaDlInfo info;
    std::set<std::string> visited;
    std::function<void(const std::string&, int)> walk = [&](const std::string& p, int depth) {
        if (depth > 8 || !visited.insert(p).second) {
            return;
        }
        auto data = oot.Read(p);
        if (!data || data->size() < 72) {
            return;
        }
        info.valid = true;
        View v(*data);
        size_t off = 64 + 8;
        while (off + 8 <= v.Size()) {
            const uint32_t w0 = v.U32(off), w1 = v.U32(off + 4);
            const uint8_t op = static_cast<uint8_t>(w0 >> 24);
            const bool expanded = op == 0x20 || op == 0x31 || op == 0x32 || op == 0x33 || op == 0x35 || op == 0x36 ||
                                  op == 0x42;
            uint64_t hash = 0;
            if (expanded && off + 16 <= v.Size()) {
                hash = (uint64_t(v.U32(off + 8)) << 32) | v.U32(off + 12);
            }
            if (op == 0x32) { // vertices
                const std::string* name = oot.NameOfHash(hash);
                if (name) {
                    auto arr = oot.Read(*name);
                    if (arr && arr->size() >= 72) {
                        View a(*arr);
                        const uint32_t total = a.U32(68);
                        const uint32_t n = (w0 >> 12) & 0xFF;
                        const uint32_t first = w1 / 16;
                        for (uint32_t k = first; k < first + n && k < total; k++) {
                            const size_t o = 72 + size_t(k) * 16;
                            const Vec3 p3(a.S16(o), a.S16(o + 2), a.S16(o + 4));
                            info.min = { std::min(info.min.x, p3.x), std::min(info.min.y, p3.y),
                                         std::min(info.min.z, p3.z) };
                            info.max = { std::max(info.max.x, p3.x), std::max(info.max.y, p3.y),
                                         std::max(info.max.z, p3.z) };
                            info.vertexCount++;
                            if (wantPositions) {
                                info.positions.push_back(p3);
                            }
                        }
                    }
                }
            } else if (op == 0x31) { // call by hash
                if (const std::string* name = oot.NameOfHash(hash)) {
                    if (depth == 0) {
                        info.calledLists.push_back(*name);
                    }
                    walk(*name, depth + 1);
                }
            } else if (op == 0x20) {
                if (const std::string* name = oot.NameOfHash(hash)) {
                    info.textures.push_back(*name);
                }
            } else if (op == 0xFD) { // plain gDPSetTextureImage: a segment address set by the actor
                info.segmentTextures = true;
            } else if (op == 0xD9) { // geometry mode: w0 = ~clear, w1 = set
                if (w1 & 0x00020000) {
                    info.lighting = true;
                }
            } else if (op == 0xDE) { // segmented call
                info.segmentCalls.push_back(w1 >> 24);
            } else if (op == 0xFC) { // combine: color inputs a b c d of both cycles
                const uint32_t a0 = (w0 >> 20) & 0xF, c0 = (w0 >> 15) & 0x1F, a1 = (w0 >> 5) & 0xF, c1 = w0 & 0x1F;
                const uint32_t b0 = (w1 >> 28) & 0xF, d0 = (w1 >> 15) & 7, b1 = (w1 >> 24) & 0xF, d1 = (w1 >> 6) & 7;
                for (uint32_t x : { a0, b0, d0, a1, b1, d1 }) {
                    info.usesEnv |= x == 5;
                    info.usesPrim |= x == 3;
                }
                for (uint32_t x : { c0, c1 }) {
                    info.usesEnv |= x == 5;
                    info.usesPrim |= x == 3;
                }
                // Alpha inputs (a b c d of both cycles): PRIMITIVE 3, ENVIRONMENT 5 in every slot.
                for (uint32_t x : { (w0 >> 12) & 7u, (w0 >> 9) & 7u, (w1 >> 21) & 7u, (w1 >> 18) & 7u, (w1 >> 12) & 7u,
                                    (w1 >> 9) & 7u, (w1 >> 3) & 7u, w1 & 7u }) {
                    info.usesPrimAlpha |= x == 3;
                    info.usesEnvAlpha |= x == 5;
                }
            } else if (op == 0xFB) {
                info.setsEnv = true;
            } else if (op == 0xFA) {
                info.setsPrim = true;
            } else if (op == 0xDF) {
                break;
            }
            off += expanded ? 16 : 8;
        }
    };
    walk(path, 0);
    return info;
}

} // namespace Oot3d
