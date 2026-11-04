#include "Oot3dSkeleton.h"

#include "Oot3dVanilla.h"

namespace Oot3d {

namespace {
class Cur {
  public:
    Cur(const Bytes& b, size_t p) : v(b), pos(p) {
    }
    uint8_t U8() {
        return v.U8(Adv(1));
    }
    int16_t S16() {
        return v.S16(Adv(2));
    }
    uint16_t U16() {
        return v.U16(Adv(2));
    }
    uint32_t U32() {
        return v.U32(Adv(4));
    }
    float F32() {
        return v.F32(Adv(4));
    }
    std::string Str() {
        const uint32_t n = U32();
        View s = v.Sub(Adv(n), n);
        return std::string(reinterpret_cast<const char*>(s.Data()), n);
    }
    void Skip(size_t n) {
        Adv(n);
    }

  private:
    size_t Adv(size_t n) {
        const size_t p = pos;
        pos += n;
        return p;
    }
    View v;
    size_t pos;
};

std::optional<VanillaLimb> ReadLimb(const ZipReader& oot, const std::string& path) {
    auto b = oot.Read(path);
    if (!b) {
        return std::nullopt;
    }
    auto h = ParseResourceHeader(*b);
    if (!h || h->type != kTypeSkeletonLimb) {
        return std::nullopt;
    }
    Cur c(*b, 64);
    VanillaLimb l;
    l.path = path;
    l.limbType = c.U8();
    c.U8();    // skin segment type
    c.Str();   // skin dlist
    c.U16();   // skin vtx count
    const uint32_t modifs = c.U32();
    for (uint32_t i = 0; i < modifs; i++) {
        c.U16();
        const uint32_t nv = c.U32();
        c.Skip(size_t(nv) * 10);
        const uint32_t nt = c.U32();
        c.Skip(size_t(nt) * 8);
    }
    c.Str(); // skin dlist 2
    c.F32();
    c.F32();
    c.F32();
    c.U16();
    c.U16();
    c.U16();
    c.Str(); // child
    c.Str(); // sibling
    l.dList = c.Str();
    l.dList2 = c.Str();
    for (int k = 0; k < 3; k++) {
        l.trans[k] = c.S16();
    }
    l.childIndex = c.U8();
    l.siblingIndex = c.U8();
    return l;
}
} // namespace

std::optional<VanillaSkeleton> ReadVanillaSkeleton(const ZipReader& oot, const std::string& path) {
    auto b = oot.Read(path);
    if (!b) {
        return std::nullopt;
    }
    auto h = ParseResourceHeader(*b);
    if (!h || h->type != kTypeSkeleton) {
        return std::nullopt;
    }
    Cur c(*b, 64);
    VanillaSkeleton s;
    s.path = path;
    s.type = c.U8();
    s.limbType = c.U8();
    c.U32(); // limb count
    s.dListCount = c.U32();
    c.U8(); // limb table type
    const uint32_t n = c.U32();
    for (uint32_t i = 0; i < n; i++) {
        const std::string lp = c.Str();
        auto limb = ReadLimb(oot, lp);
        if (!limb) {
            return std::nullopt;
        }
        s.limbs.push_back(*limb);
    }
    // Parents from the child/sibling links (limb 0 is the root).
    for (size_t i = 0; i < s.limbs.size(); i++) {
        const uint8_t child = s.limbs[i].childIndex;
        uint8_t k = child;
        int guard = 0;
        while (k != 0xFF && k < s.limbs.size() && guard++ < 256) {
            s.limbs[k].parent = static_cast<int>(i);
            k = s.limbs[k].siblingIndex;
        }
    }
    return s;
}

std::vector<int> ParentsOf(const VanillaSkeleton& s) {
    std::vector<int> p;
    for (const auto& l : s.limbs) {
        p.push_back(l.parent);
    }
    return p;
}

} // namespace Oot3d
