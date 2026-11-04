#include "Oot3dCmab.h"

#include <algorithm>
#include <cmath>

namespace Oot3d {

namespace {

std::string CString(View v, size_t off) {
    std::string s;
    while (off < v.Size() && v.U8(off) != 0) {
        s.push_back(static_cast<char>(v.U8(off++)));
    }
    return s;
}

CmabTrack ParseTrack(View t) {
    CmabTrack tr;
    tr.type = static_cast<CmabTrack::Type>(t.U32(0x00));
    const uint32_t count = t.U32(0x04);
    tr.timeStart = t.U32(0x08);
    tr.timeEnd = t.U32(0x0C);
    size_t o = 0x10;
    for (uint32_t i = 0; i < count; i++) {
        CmabKey k;
        k.time = static_cast<float>(t.U32(o));
        k.value = t.F32(o + 4);
        if (tr.type == CmabTrack::Type::Hermite) {
            k.tangentIn = t.F32(o + 8);
            k.tangentOut = t.F32(o + 12);
            o += 16;
        } else {
            o += 8;
        }
        tr.keys.push_back(k);
    }
    return tr;
}

} // namespace

float CmabTrack::Sample(float frame) const {
    if (keys.empty()) {
        return 0;
    }
    size_t i1 = 0;
    while (i1 < keys.size() && !(frame < keys[i1].time)) {
        i1++;
    }
    if (i1 == 0) {
        return keys.front().value;
    }
    if (i1 == keys.size()) {
        return keys.back().value;
    }
    const CmabKey& k0 = keys[i1 - 1];
    const CmabKey& k1 = keys[i1];
    if (type == Type::Integer) {
        return k0.value;
    }
    const float length = k1.time - k0.time;
    const float t = length > 0 ? (frame - k0.time) / length : 0;
    if (type == Type::Linear) {
        return k0.value + (k1.value - k0.value) * t;
    }
    if (length == 1) {
        return k0.value; // Nintendo's "reset" tangents (noclip)
    }
    const float t2 = t * t, t3 = t2 * t;
    const float s0 = k0.tangentOut * length, s1 = k1.tangentIn * length;
    return (2 * t3 - 3 * t2 + 1) * k0.value + (t3 - 2 * t2 + t) * s0 + (-2 * t3 + 3 * t2) * k1.value + (t3 - t2) * s1;
}

Cmab Cmab::Parse(View f) {
    if (f.Size() < 0x40 || !f.Magic(0, "cmab")) {
        throw FormatError("not a CMAB");
    }
    Cmab c;
    // String table (texture names).
    std::vector<std::string> strings;
    const uint32_t strt = f.U32(0x18);
    if (strt && strt + 8 <= f.Size() && f.Magic(strt, "strt")) {
        const uint32_t n = f.U32(strt + 4);
        const size_t data = strt + 8 + size_t(n) * 4;
        for (uint32_t i = 0; i < n; i++) {
            strings.push_back(CString(f, data + f.U32(strt + 8 + i * 4)));
        }
    }
    const uint32_t texData = f.U32(0x1C);
    c.duration = f.U32(0x24);
    c.loop = f.U32(0x28) == 1;
    const uint32_t txpt = f.U32(0x30);
    const size_t mads = 0x34;
    if (!f.Magic(mads, "mads")) {
        throw FormatError("CMAB without mads");
    }
    const uint32_t count = f.U32(mads + 4);
    for (uint32_t i = 0; i < count; i++) {
        const size_t mm = mads + f.U32(mads + 8 + i * 4);
        const View m = f.From(mm);
        if (!m.Magic(0, "mmad")) {
            continue;
        }
        CmabEntry e;
        e.type = m.U32(0x04);
        e.material = m.U32(0x08);
        size_t table = 0x0C;
        const bool hasChannel = e.type == CmabEntry::Translation || e.type == CmabEntry::Rotation ||
                                e.type == CmabEntry::Scale || e.type == CmabEntry::TexturePalette ||
                                e.type == CmabEntry::ConstColor;
        if (hasChannel) {
            e.channel = m.U32(0x0C);
            table += 4;
        }
        int tracks = 0;
        switch (e.type) {
            case CmabEntry::Translation:
            case CmabEntry::Scale: tracks = 2; break;
            case CmabEntry::TexturePalette:
            case CmabEntry::Rotation: tracks = 1; break;
            default: tracks = 4; break;
        }
        e.tracks.resize(tracks);
        e.present.assign(tracks, false);
        for (int k = 0; k < tracks; k++) {
            const uint16_t off = m.U16(table + k * 2);
            if (off == 0) {
                continue;
            }
            CmabTrack t = ParseTrack(m.From(off));
            if (!t.keys.empty()) {
                e.tracks[k] = std::move(t);
                e.present[k] = true;
            }
        }
        c.entries.push_back(std::move(e));
    }
    // Palette textures.
    if (txpt != 0 && texData != 0) {
        const View tp = f.From(0x20 + txpt);
        if (tp.Magic(0, "txpt")) {
            const uint16_t n = tp.U16(0x04);
            const View data = f.From(texData);
            for (uint16_t i = 0; i < n; i++) {
                const size_t e = 0x08 + size_t(i) * 0x18;
                const uint32_t size = tp.U32(e + 0x00);
                const uint32_t w = tp.U16(e + 0x08), h = tp.U16(e + 0x0A);
                const uint32_t gl = tp.U32(e + 0x0C);
                const uint32_t off = tp.U32(e + 0x10);
                const uint32_t nameIdx = tp.U32(e + 0x14);
                CmabTexture t;
                t.name = nameIdx < strings.size() ? strings[nameIdx] : "tex" + std::to_string(i);
                const PicaFormat fmt = PicaFormatFromGl(gl);
                const size_t need = PicaByteSize(fmt, w, h);
                if (off + std::min<size_t>(need, size) <= data.Size()) {
                    t.image = DecodePica(fmt, w, h, data.Sub(off, need));
                }
                c.textures.push_back(std::move(t));
            }
        }
    }
    return c;
}

} // namespace Oot3d
