#include "Oot3dMaterial.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace Oot3d {

namespace {

using Vec = std::array<float, 3>;
using namespace Soh::Gbi;

Vec V3(float v) {
    return { v, v, v };
}

Poly Const(const Vec& c) {
    Poly p;
    if (c[0] != 0 || c[1] != 0 || c[2] != 0) {
        p.terms[{}] = c;
    }
    return p;
}

Poly AtomPoly(uint8_t atom) {
    Poly p;
    p.terms[{ atom }] = V3(1);
    return p;
}

void Clean(Poly& p) {
    for (auto it = p.terms.begin(); it != p.terms.end();) {
        const Vec& c = it->second;
        if (std::fabs(c[0]) < 1e-5f && std::fabs(c[1]) < 1e-5f && std::fabs(c[2]) < 1e-5f) {
            it = p.terms.erase(it);
        } else {
            ++it;
        }
    }
}

Poly Add(const Poly& a, const Poly& b, float sign = 1) {
    Poly r = a;
    for (const auto& [m, c] : b.terms) {
        Vec& d = r.terms[m];
        for (int i = 0; i < 3; i++) {
            d[i] += sign * c[i];
        }
    }
    Clean(r);
    return r;
}

Poly Mul(const Poly& a, const Poly& b) {
    Poly r;
    for (const auto& [ma, ca] : a.terms) {
        for (const auto& [mb, cb] : b.terms) {
            Mono m = ma;
            m.insert(m.end(), mb.begin(), mb.end());
            std::sort(m.begin(), m.end());
            Vec& d = r.terms[m];
            for (int i = 0; i < 3; i++) {
                d[i] += ca[i] * cb[i];
            }
        }
    }
    Clean(r);
    return r;
}

Poly Scale(const Poly& a, float s) {
    return Mul(a, Const(V3(s)));
}

Vec Rgb(const Rgba8& c) {
    return { c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f };
}

// Symbolic PICA combiner evaluation.
struct TevEval {
    const CmbMaterial& m;
    bool approximate = false;
    // >= 0: constant colors stored as black read as this gray (they are set by the game at runtime).
    float placeholder = -1;
    bool usedPlaceholder = false;
    Poly prevRgb, prevA, bufRgb, bufA;

    explicit TevEval(const CmbMaterial& mat) : m(mat) {
        bufRgb = Const({ m.bufferColor[0], m.bufferColor[1], m.bufferColor[2] });
        bufA = Const(V3(m.bufferColor[3]));
    }

    // The source's color and alpha polys.
    void Source(uint16_t src, const CmbCombiner& k, Poly& rgb, Poly& a) {
        switch (src) {
            case Tev::Texture0:
            case Tev::Texture1:
            case Tev::Texture2: {
                const uint8_t t = static_cast<uint8_t>(src - Tev::Texture0);
                rgb = AtomPoly(static_cast<uint8_t>(AT0 + t));
                a = AtomPoly(static_cast<uint8_t>(AT0a + t));
                break;
            }
            case Tev::Constant: {
                const Rgba8& c = m.constants[k.constantIndex];
                rgb = Const(Rgb(c));
                if (placeholder >= 0 && c[0] == 0 && c[1] == 0 && c[2] == 0) {
                    rgb = Const(V3(placeholder));
                    usedPlaceholder = true;
                }
                a = Const(V3(c[3] / 255.0f));
                break;
            }
            case Tev::PrimaryColor:
            case Tev::FragPrimary:
                rgb = AtomPoly(AV);
                a = AtomPoly(AVa);
                break;
            case Tev::FragSecondary:
                rgb = Poly();
                a = Poly();
                break;
            case Tev::Previous:
                rgb = prevRgb;
                a = prevA;
                break;
            case Tev::PreviousBuffer:
                rgb = bufRgb;
                a = bufA;
                break;
            default:
                approximate = true;
                rgb = Const(V3(1));
                a = Const(V3(1));
                break;
        }
    }

    Poly Operand(uint16_t src, uint16_t op, const CmbCombiner& k, bool alphaChannel) {
        Poly rgb, a;
        Source(src, k, rgb, a);
        const Poly one = Const(V3(1));
        switch (op) {
            case Tev::SrcColor: return alphaChannel ? a : rgb;
            case Tev::OneMinusSrcColor: return Add(one, alphaChannel ? a : rgb, -1);
            case Tev::SrcAlpha: return a;
            case Tev::OneMinusSrcAlpha: return Add(one, a, -1);
            case Tev::SrcR:
            case Tev::SrcG:
            case Tev::SrcB:
                // Single-channel reads (grayscale tricks) are approximated by the whole color.
                approximate = true;
                return alphaChannel ? a : rgb;
            case Tev::OneMinusSrcR:
            case Tev::OneMinusSrcG:
            case Tev::OneMinusSrcB: approximate = true; return Add(one, alphaChannel ? a : rgb, -1);
            default: approximate = true; return alphaChannel ? a : rgb;
        }
    }

    Poly Combine(uint16_t op, const Poly& a, const Poly& b, const Poly& c) {
        switch (op) {
            case Tev::Replace: return a;
            case Tev::Modulate: return Mul(a, b);
            case Tev::Add: return Add(a, b);
            case Tev::AddSigned: return Add(Add(a, b), Const(V3(0.5f)), -1);
            case Tev::Interpolate: return Add(Mul(a, c), Mul(b, Add(Const(V3(1)), c, -1)));
            case Tev::Subtract: return Add(a, b, -1);
            case Tev::MultAdd: return Add(Mul(a, b), c);
            case Tev::AddMult: return Mul(Add(a, b), c);
            default: approximate = true; return a; // dot3 (bump) has no N64 counterpart
        }
    }

    void Run() {
        prevRgb = Const(V3(0));
        prevA = Const(V3(0));
        if (m.combiners.empty()) {
            prevRgb = AtomPoly(AV);
            prevA = AtomPoly(AVa);
            return;
        }
        for (size_t i = 0; i < m.combiners.size(); i++) {
            const CmbCombiner& k = m.combiners[i];
            Poly rgb = Combine(k.combineRgb, Operand(k.srcRgb[0], k.opRgb[0], k, false),
                               Operand(k.srcRgb[1], k.opRgb[1], k, false), Operand(k.srcRgb[2], k.opRgb[2], k, false));
            Poly a = Combine(k.combineAlpha, Operand(k.srcAlpha[0], k.opAlpha[0], k, true),
                             Operand(k.srcAlpha[1], k.opAlpha[1], k, true),
                             Operand(k.srcAlpha[2], k.opAlpha[2], k, true));
            if (k.scaleRgb > 1) {
                rgb = Scale(rgb, static_cast<float>(k.scaleRgb));
            }
            if (k.scaleAlpha > 1) {
                a = Scale(a, static_cast<float>(k.scaleAlpha));
            }
            // The buffer the next stage reads (PICA: stages 1-4 may latch the previous output).
            if (i > 0) {
                if (k.bufferInputRgb == Tev::Previous) {
                    bufRgb = prevRgb;
                }
                if (k.bufferInputAlpha == Tev::Previous) {
                    bufA = prevA;
                }
            }
            prevRgb = std::move(rgb);
            prevA = std::move(a);
        }
    }
};

bool HasAtom(const Mono& m, uint8_t a) {
    return std::find(m.begin(), m.end(), a) != m.end();
}

// Splits p = V * q + r (q: terms that had exactly one `vAtom`, removed; r: terms without it). Terms with the vertex
// color more than once, or with the "other" vertex atom (alpha in a color expression), are approximated.
void SplitVertex(const Poly& p, uint8_t vAtom, uint8_t otherV, Poly& q, Poly& r, bool& approx) {
    for (const auto& [key, c] : p.terms) {
        Mono m = key;
        m.erase(std::remove(m.begin(), m.end(), otherV), m.end()); // vertex alpha in color: ~1 (approximation)
        const auto n = std::count(m.begin(), m.end(), vAtom);
        if (n > 1) {
            approx = true;
        }
        if (n >= 1) {
            m.erase(std::remove(m.begin(), m.end(), vAtom), m.end());
            Vec& d = q.terms[m];
            for (int i = 0; i < 3; i++) {
                d[i] += c[i];
            }
        } else {
            Vec& d = r.terms[m];
            for (int i = 0; i < 3; i++) {
                d[i] += c[i];
            }
        }
    }
    Clean(q);
    Clean(r);
}

float MaxOf(const Vec& v) {
    return std::max({ v[0], v[1], v[2] });
}

uint8_t ToByte(float v) {
    return static_cast<uint8_t>(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
}

// Texture atoms -> N64 inputs. `tex` maps T0/T1/T2 to TEXEL0 (0) / TEXEL1 (1) / unavailable (-1).
struct Inputs {
    int tex[3] = { -1, -1, -1 };
    uint8_t A(uint8_t atom) const { // as color A/B/D-style input for a color atom
        const int t = tex[atom % 3];
        return t == 0 ? ATex0 : ATex1;
    }
    uint8_t C(uint8_t atom) const {
        if (atom >= AT0a && atom <= AT2a) {
            return tex[atom - AT0a] == 0 ? CTex0Alpha : CTex1Alpha;
        }
        return tex[atom] == 0 ? CTex0 : CTex1;
    }
    uint8_t Alpha(uint8_t atom) const { // alpha-combiner input for T*a atoms
        return tex[atom - AT0a] == 0 ? AcTex0 : AcTex1;
    }
    bool Available(uint8_t atom) const {
        const int t = atom >= AT0a ? atom - AT0a : atom;
        return t < 3 && tex[t] >= 0;
    }
};

// A color atom usable in the A slot (color, not alpha).
bool IsColorTex(uint8_t a) {
    return a <= AT2;
}

} // namespace

N64Material TranslateMaterial(const Cmb& cmb, const CmbMaterial& m, bool drawnTranslucent) {
    N64Material out;
    out.description = DescribeMaterial(m);
    TevEval tev(m);
    tev.Run();
    // A black constant that only scales the result is a color the game sets at runtime (Grezzo's per-character
    // clothing tints, the N64's actor-set env colors): evaluate it as white and multiply by ENV instead.
    {
        TevEval white(m), gray(m);
        white.placeholder = 1.0f;
        gray.placeholder = 0.5f;
        white.Run();
        gray.Run();
        if (white.usedPlaceholder) {
            bool scales = !white.prevRgb.terms.empty();
            for (const auto& [mono, c] : white.prevRgb.terms) {
                auto it = gray.prevRgb.terms.find(mono);
                for (int i = 0; i < 3 && scales; i++) {
                    const float g = it == gray.prevRgb.terms.end() ? 0.0f : it->second[i];
                    scales = std::fabs(c[i] * 0.5f - g) <= 1e-4f + 1e-3f * std::fabs(c[i]);
                }
            }
            scales = scales && gray.prevRgb.terms.size() == white.prevRgb.terms.size();
            if (scales) {
                tev.prevRgb = white.prevRgb;
                out.envTint = true;
            }
        }
    }
    out.approximate = tev.approximate;

    // Textures sampled through an environment or projection mapping (sphere/cube reflections, projected glows and
    // shadows) are effects the 3DS game drives at runtime; sampled with the model's UVs on the N64 they smear over the
    // surface (the large crate's emblem projection). Drop them: zero where they add on, one where they scale (so the
    // base texture never vanishes).
    for (int t = 0; t < 3; t++) {
        if (t >= int(m.bindingCount) || m.bindings[t].texture < 0) {
            continue;
        }
        const TexMapping map = m.coordinators[m.CoordinatorForSlot(t)].mapping;
        if (map != TexMapping::CubeEnv && map != TexMapping::SphereEnv && map != TexMapping::Projection) {
            continue;
        }
        auto texturesIn = [](const Poly& p) {
            for (const auto& [mono, c] : p.terms) {
                for (uint8_t a : mono) {
                    if (a <= AT2 || (a >= AT0a && a <= AT2a)) {
                        return true;
                    }
                }
            }
            return false;
        };
        auto substitute = [t](const Poly& p, bool one) {
            Poly r;
            for (const auto& [mono, c] : p.terms) {
                Mono k;
                bool has = false;
                for (uint8_t a : mono) {
                    if (a == AT0 + t || a == AT0a + t) {
                        has = true;
                    } else {
                        k.push_back(a);
                    }
                }
                if (has && !one) {
                    continue;
                }
                auto& d = r.terms[k];
                for (int i = 0; i < 3; i++) {
                    d[i] += c[i];
                }
            }
            return r;
        };
        // A texture without an alpha channel (ETC1, RGB) reads alpha 1: its alpha terms keep their other factors.
        const int tb = m.bindings[t].texture;
        const bool texAlpha = tb >= 0 && tb < int(cmb.textures.size()) && PicaFormatHasAlpha(cmb.textures[tb].format);
        Poly rgb = substitute(tev.prevRgb, false), a = substitute(tev.prevA, !texAlpha);
        bool lostBase = false;
        for (int u = 0; u < 3; u++) {
            if (u == t) {
                continue;
            }
            Poly only;
            for (const auto& [mono, c] : tev.prevRgb.terms) {
                if (std::find(mono.begin(), mono.end(), uint8_t(AT0 + u)) != mono.end()) {
                    only.terms[mono] = c;
                }
            }
            if (!only.terms.empty() && !texturesIn(rgb)) {
                lostBase = true;
            }
        }
        if (lostBase || rgb.terms.empty()) {
            rgb = substitute(tev.prevRgb, true);
            a = substitute(tev.prevA, true);
        } else if (map == TexMapping::SphereEnv && out.envSlot < 0) {
            // The reflection is added on (metal, glass, the hearts' shine): its coefficient, for a G_TEXTURE_GEN pass.
            Poly env;
            bool ok = true;
            for (const auto& [mono, c] : tev.prevRgb.terms) {
                const auto n = std::count(mono.begin(), mono.end(), uint8_t(AT0 + t));
                ok &= std::count(mono.begin(), mono.end(), uint8_t(AT0a + t)) == 0 && n <= 1;
                if (n == 1) {
                    Mono k;
                    for (uint8_t at : mono) {
                        if (at != AT0 + t) {
                            k.push_back(at);
                        }
                    }
                    Vec& d = env.terms[k];
                    for (int i = 0; i < 3; i++) {
                        d[i] += c[i];
                    }
                }
            }
            Clean(env);
            if (ok && !env.terms.empty()) {
                out.envSlot = t;
                out.envTerm = std::move(env);
            }
        }
        tev.prevRgb = rgb;
        tev.prevA = a;
        out.description += " (env-mapped T" + std::to_string(t) + " dropped)";
    }

    // Which textures the program reads.
    std::set<int> used;
    for (const Poly* p : { &tev.prevRgb, &tev.prevA }) {
        for (const auto& [mono, c] : p->terms) {
            for (uint8_t a : mono) {
                if (a <= AT2) {
                    used.insert(a);
                } else if (a >= AT0a && a <= AT2a) {
                    used.insert(a - AT0a);
                }
            }
        }
    }
    Inputs in;
    int next = 0;
    for (int t : used) {
        if (m.bindings[t].texture < 0 || m.bindings[t].texture >= int(cmb.textures.size())) {
            continue;
        }
        if (next < 2) {
            in.tex[t] = next;
            out.texSlot[next] = t;
            next++;
        } else {
            out.approximate = true; // a third texture: dropped
        }
    }
    // Drop terms reading unavailable textures.
    auto dropUnavailable = [&](Poly& p) {
        for (auto it = p.terms.begin(); it != p.terms.end();) {
            bool ok = true;
            for (uint8_t a : it->first) {
                if (a != AV && a != AVa && !in.Available(a)) {
                    ok = false;
                }
            }
            if (!ok) {
                it = p.terms.erase(it);
                out.approximate = true;
            } else {
                ++it;
            }
        }
    };
    Poly rgb = tev.prevRgb, alpha = tev.prevA;
    dropUnavailable(rgb);
    dropUnavailable(alpha);

    out.vertexLighting = m.vertexLighting;
    out.translucent = m.Blended() || (drawnTranslucent && !(m.alphaTest && m.alphaFunc != Gl::Always));
    out.additive = m.Blended() && m.blendDstRgb == Gl::One;
    out.alphaTest = m.alphaTest && m.alphaFunc != Gl::Always;
    out.alphaRef = m.alphaRef;
    out.cullMode = (m.cullMode == 0 || m.cullMode == 3) ? 0 : m.cullMode;
    out.decal = m.polygonOffset;

    // ---- color ----
    Poly q, r;
    bool approx = false;
    SplitVertex(rgb, AV, AVa, q, r, approx);
    out.approximate |= approx;
    out.shadeUsesVertex = !q.terms.empty();
    if (!out.shadeUsesVertex) {
        std::swap(q, r); // no vertex term: SHADE is a constant 1 * coefficients
    }
    // Normalize q so its largest coefficient goes into SHADE.
    Vec shade = V3(1);
    Soh::CcCycle c1, c2;
    bool two = false;
    auto setPrimRgb = [&](const Vec& v) {
        out.prim[0] = ToByte(v[0]);
        out.prim[1] = ToByte(v[1]);
        out.prim[2] = ToByte(v[2]);
    };
    auto setEnvRgb = [&](const Vec& v) {
        out.env[0] = ToByte(v[0]);
        out.env[1] = ToByte(v[1]);
        out.env[2] = ToByte(v[2]);
    };
    // Collect q terms by shape.
    std::vector<std::pair<Mono, Vec>> qt(q.terms.begin(), q.terms.end());
    std::vector<std::pair<Mono, Vec>> rt(r.terms.begin(), r.terms.end());
    auto isTex1 = [](const Mono& mo) { return mo.size() == 1 && IsColorTex(mo[0]); };
    auto isConst = [](const Mono& mo) { return mo.empty(); };
    bool matched = true;

    // (X + Y) * X: X^2 and X*Y with one coefficient, a glow masked by itself with Y scrolling over it (torch flames,
    // spin trails, fire circles). The mask must have no alpha of its own (its alpha carries part of the product).
    uint8_t maskX = 0xFF, maskY = 0xFF;
    if (qt.size() == 2 && rt.empty()) {
        auto isSquare = [](const Mono& mo) { return mo.size() == 2 && mo[0] == mo[1] && IsColorTex(mo[0]); };
        auto isPair = [](const Mono& mo) {
            return mo.size() == 2 && mo[0] != mo[1] && IsColorTex(mo[0]) && IsColorTex(mo[1]);
        };
        const auto* sq = isSquare(qt[0].first) ? &qt[0] : isSquare(qt[1].first) ? &qt[1] : nullptr;
        const auto* pr = isPair(qt[0].first) ? &qt[0] : isPair(qt[1].first) ? &qt[1] : nullptr;
        if (sq && pr) {
            const uint8_t x = sq->first[0];
            const uint8_t y = pr->first[0] == x ? pr->first[1] : pr->first[0];
            bool same = pr->first[0] == x || pr->first[1] == x;
            for (int i = 0; i < 3; i++) {
                same &= std::fabs(sq->second[i] - pr->second[i]) <= 0.02f * std::max(MaxOf(sq->second), 1e-3f);
            }
            const int xb = m.bindings[x].texture;
            const bool noAlpha = xb >= 0 && xb < int(cmb.textures.size()) && !PicaFormatHasAlpha(cmb.textures[xb].format);
            const bool gray = noAlpha && PicaFormatIsGray(cmb.textures[xb].format);
            if (same && in.Available(x) && in.Available(y) && noAlpha && (out.additive || gray)) {
                maskX = x;
                maskY = y;
            }
        }
    }

    if (maskX != 0xFF) {
        shade = qt[0].second;
        out.squareMask = in.tex[maskX];
        if (out.additive) {
            // The mask's (normalized) color x SHADE; the intensity goes to alpha (below).
            c1 = { in.A(maskX), B0, CShade, D0 };
        } else {
            // (Y - 0) * X.a + X, the mask texture holding X^2 in color and X in alpha: X*Y + X^2.
            c1 = { in.A(maskY), B0, in.C(static_cast<uint8_t>(AT0a + maskX)),
                   static_cast<uint8_t>(in.A(maskX) == ATex0 ? DTex0 : DTex1) };
            c2 = { AComb, B0, CShade, D0 };
            two = true;
        }
    } else if (qt.size() == 1 && isConst(qt[0].first) && rt.empty()) {
        // V * c
        shade = qt[0].second;
        c1 = { A0, B0, C0, DShade };
    } else if (qt.size() == 1 && isTex1(qt[0].first) && rt.empty()) {
        // V * c * T
        shade = qt[0].second;
        c1 = { in.A(qt[0].first[0]), B0, CShade, D0 };
    } else if (qt.size() == 1 && qt[0].first.size() == 2 && rt.empty()) {
        // V * c * X * Y (lightmap multiply, color x other alpha)
        const Mono& mo = qt[0].first;
        shade = qt[0].second;
        const uint8_t x = IsColorTex(mo[0]) ? mo[0] : mo[1];
        const uint8_t y = x == mo[0] ? mo[1] : mo[0];
        if (IsColorTex(x)) {
            c1 = { in.A(x), B0, in.C(y), D0 };
            c2 = { AComb, B0, CShade, D0 };
            two = true;
        } else {
            matched = false;
        }
    } else if (qt.size() == 2 && isTex1(qt[0].first) && isTex1(qt[1].first) && rt.empty()) {
        // V * (c0 * Ta + c1 * Tb): a lerp or a weighted sum
        Vec ca = qt[0].second, cb = qt[1].second;
        uint8_t ta = qt[0].first[0], tb = qt[1].first[0];
        if (MaxOf(ca) > MaxOf(cb)) {
            std::swap(ca, cb);
            std::swap(ta, tb);
        }
        // = cb * (ca/cb * Ta + Tb)
        Vec k;
        for (int i = 0; i < 3; i++) {
            k[i] = cb[i] > 1e-5f ? ca[i] / cb[i] : 0;
        }
        shade = cb;
        const Vec sum = { ca[0] + cb[0], ca[1] + cb[1], ca[2] + cb[2] };
        const bool lerp = std::fabs(sum[0] - sum[1]) < 0.02f && std::fabs(sum[1] - sum[2]) < 0.02f;
        if (lerp) {
            // (Ta - Tb) * k + Tb, k = ca / (ca + cb)
            for (int i = 0; i < 3; i++) {
                k[i] = sum[i] > 1e-5f ? ca[i] / sum[i] : 0;
            }
            shade = sum;
            setPrimRgb(k);
            c1 = { in.A(ta), in.A(tb), CPrim, DTex0 };
            c1.d = in.A(tb) == ATex0 ? DTex0 : DTex1;
        } else {
            setPrimRgb(k);
            c1 = { in.A(ta), B0, CPrim, static_cast<uint8_t>(in.A(tb) == ATex0 ? DTex0 : DTex1) };
        }
        c2 = { AComb, B0, CShade, D0 };
        two = true;
    } else if (qt.size() == 2 && isTex1(qt[0].first) != isTex1(qt[1].first) &&
               (isConst(qt[0].first) || isConst(qt[1].first)) && rt.empty()) {
        // V * (c1 * T + c0)
        const auto& tex = isTex1(qt[0].first) ? qt[0] : qt[1];
        const auto& cst = isConst(qt[0].first) ? qt[0] : qt[1];
        shade = tex.second;
        Vec e;
        for (int i = 0; i < 3; i++) {
            e[i] = tex.second[i] > 1e-5f ? cst.second[i] / tex.second[i] : 0;
        }
        setEnvRgb(e);
        c1 = { in.A(tex.first[0]), B0, CPrim, DEnv };
        out.prim[0] = out.prim[1] = out.prim[2] = 255;
        c2 = { AComb, B0, CShade, D0 };
        two = true;
    } else if (qt.size() == 1 && isTex1(qt[0].first) && rt.size() == 1 && isConst(rt[0].first)) {
        // V * c1 * T + c0
        shade = qt[0].second;
        setPrimRgb(rt[0].second);
        c1 = { in.A(qt[0].first[0]), B0, CShade, DPrim };
    } else if (qt.size() == 1 && isTex1(qt[0].first) && rt.size() == 1 && isTex1(rt[0].first)) {
        // V * c1 * Ta + c2 * Tb (an unlit glow/detail map added on top)
        shade = qt[0].second;
        setPrimRgb(rt[0].second);
        c1 = { in.A(rt[0].first[0]), B0, CPrim, D0 };
        c2 = { in.A(qt[0].first[0]), B0, CShade, DComb };
        two = true;
    } else if (qt.empty()) {
        c1 = { A0, B0, C0, D0 };
    } else {
        matched = false;
    }
    if (!matched) {
        // Fallback: the strongest term with a texture, times SHADE.
        out.approximate = true;
        const std::pair<Mono, Vec>* best = nullptr;
        for (const auto& t : qt) {
            if (!t.first.empty() && IsColorTex(t.first[0]) && (!best || MaxOf(t.second) > MaxOf(best->second))) {
                best = &t;
            }
        }
        if (best) {
            shade = best->second;
            c1 = { in.A(best->first[0]), B0, CShade, D0 };
        } else {
            c1 = { A0, B0, C0, DShade };
        }
        two = false;
    }
    out.shadeScale = shade;

    // ---- alpha ----
    if (maskX != 0xFF) {
        // The mask has no alpha on the 3DS (1); its texture's alpha now carries part of the color product.
        Poly a2;
        for (const auto& [mono, c] : alpha.terms) {
            Mono k;
            for (uint8_t at : mono) {
                if (at != AT0a + maskX) {
                    k.push_back(at);
                }
            }
            Vec& d = a2.terms[k];
            for (int i = 0; i < 3; i++) {
                d[i] += c[i];
            }
        }
        Clean(a2);
        alpha = std::move(a2);
    }
    Poly qa, ra;
    bool approxA = false;
    SplitVertex(alpha, AVa, AV, qa, ra, approxA);
    out.approximate |= approxA;
    std::vector<std::pair<Mono, Vec>> qat(qa.terms.begin(), qa.terms.end());
    std::vector<std::pair<Mono, Vec>> rat(ra.terms.begin(), ra.terms.end());
    auto alphaTex = [&](uint8_t atom) -> uint8_t {
        // a color atom in an alpha expression (SrcColor used on an alpha operand) reads that texture's alpha
        if (atom <= AT2) {
            atom = static_cast<uint8_t>(atom + AT0a);
        }
        return in.Alpha(atom);
    };
    uint8_t aa = Ac0, ba = Ac0, ca = Ac0, da = Ac1;
    bool twoAlpha = false;
    Soh::CcCycle alpha2;
    if (qat.size() == 1 && qat[0].first.empty() && rat.empty()) {
        out.shadeAlphaScale = qat[0].second[0];
        aa = Ac0, ba = Ac0, ca = Ac0, da = AcShade;
        out.usesShadeAlpha = true;
    } else if (qat.size() == 1 && qat[0].first.size() == 1 && rat.empty()) {
        out.shadeAlphaScale = qat[0].second[0];
        aa = alphaTex(qat[0].first[0]), ba = Ac0, ca = AcShade, da = Ac0;
        out.usesShadeAlpha = true;
    } else if (qat.size() == 1 && qat[0].first.size() == 2 && rat.empty()) {
        out.shadeAlphaScale = qat[0].second[0];
        aa = alphaTex(qat[0].first[0]), ba = Ac0, ca = alphaTex(qat[0].first[1]), da = Ac0;
        alpha2 = {};
        alpha2.aa = AcComb, alpha2.ba = Ac0, alpha2.ca = AcShade, alpha2.da = Ac0;
        twoAlpha = true;
        out.usesShadeAlpha = true;
    } else if (qat.empty() && rat.size() == 1 && rat[0].first.empty()) {
        out.prim[3] = ToByte(rat[0].second[0]);
        aa = Ac0, ba = Ac0, ca = Ac0, da = AcPrim;
    } else if (qat.empty() && rat.size() == 1 && rat[0].first.size() == 1) {
        out.prim[3] = ToByte(rat[0].second[0]);
        aa = alphaTex(rat[0].first[0]), ba = Ac0, ca = AcPrim, da = Ac0;
    } else if (qat.empty() && rat.empty()) {
        aa = Ac0, ba = Ac0, ca = Ac0, da = Ac0;
    } else {
        // Fallback: vertex alpha x first texture alpha.
        out.approximate = true;
        out.usesShadeAlpha = true;
        if (out.texSlot[0] >= 0) {
            aa = AcTex0, ba = Ac0, ca = AcShade, da = Ac0;
        } else {
            aa = Ac0, ba = Ac0, ca = Ac0, da = AcShade;
        }
    }
    if (maskX != 0xFF && out.additive) {
        // Additive glow drawn with alpha blending: alpha is its intensity, (Y + 1) * f(X) (the mask texture's alpha
        // is f(X), Oot3dMesh) in cycle 1, times the material's own alpha (vertex or constant, through SHADE).
        float k = -1;
        if (qat.size() == 1 && qat[0].first.empty() && rat.empty()) {
            k = qat[0].second[0];
        } else if (qat.empty() && rat.size() == 1 && rat[0].first.empty()) {
            k = rat[0].second[0];
        }
        if (k >= 0) {
            aa = in.Alpha(static_cast<uint8_t>(AT0a + maskY)), ba = Ac0;
            ca = da = in.Alpha(static_cast<uint8_t>(AT0a + maskX));
            alpha2 = {};
            alpha2.aa = AcComb, alpha2.ba = Ac0, alpha2.ca = AcShade, alpha2.da = Ac0;
            twoAlpha = true;
            out.usesShadeAlpha = true;
            out.shadeAlphaScale = k;
            out.approximate = true; // f is a fit
        } else {
            out.squareMask = -1; // an alpha of another shape: the mask times SHADE, as before
        }
    }
    if (!out.translucent && !out.alphaTest) {
        // Opaque: alpha is irrelevant, keep it at 1 (and leave SHADE alpha free for fog).
        aa = Ac0, ba = Ac0, ca = Ac0, da = Ac1;
        twoAlpha = false;
        out.usesShadeAlpha = false;
    }

    // Assemble the cycles: in two-cycle mode the second cycle passes whatever finished in the first.
    c1.aa = aa, c1.ba = ba, c1.ca = ca, c1.da = da;
    if (two || twoAlpha) {
        if (!two) {
            c2.a = A0, c2.b = B0, c2.c = C0, c2.d = DComb;
        }
        if (twoAlpha) {
            c2.aa = alpha2.aa, c2.ba = alpha2.ba, c2.ca = alpha2.ca, c2.da = alpha2.da;
        } else {
            c2.aa = Ac0, c2.ba = Ac0, c2.ca = Ac0, c2.da = AcComb;
        }
        out.twoCycle = true;
    } else {
        // One cycle's worth of combining; rooms still run in two-cycle mode for fog, so pass it through.
        c2 = { A0, B0, C0, DComb, Ac0, Ac0, Ac0, AcComb };
        out.twoCycle = false;
    }
    out.cycle1 = c1;
    out.cycle2 = c2;
    return out;
}

std::array<uint8_t, 4> BakeShade(const N64Material& mat, const CmbMaterial& m, const CmbVertex& v, const Vec3& normal,
                                 const SceneLight& light, bool hasVertexColors) {
    // noclip's DMP vertex stage: the primary color is lighting x vertex color (vertex lit) or the vertex color, and
    // shapes without vertex colors use the material's diffuse color instead.
    const std::array<float, 4> vertex =
        hasVertexColors ? v.color
                        : std::array<float, 4>{ m.diffuse[0] / 255.0f, m.diffuse[1] / 255.0f, m.diffuse[2] / 255.0f,
                                                m.diffuse[3] / 255.0f };
    std::array<float, 4> col{ 1, 1, 1, 1 };
    if (mat.vertexLighting) {
        std::array<float, 3> acc{ 0, 0, 0 };
        const Vec3 n = normal.Normalized();
        for (int i = 0; i < 2; i++) {
            const float ndl = std::max(0.0f, -(light.dir[i].Dot(n)));
            for (int c = 0; c < 3; c++) {
                const float diffuse = light.diffuse[i][c] * m.diffuse[c] / 255.0f;
                const float ambient = light.ambient[i][c] * m.ambient[c] / 255.0f;
                acc[c] += diffuse * ndl + ambient;
            }
        }
        col = { acc[0], acc[1], acc[2], m.diffuse[3] / 255.0f };
        if (hasVertexColors) {
            for (int c = 0; c < 4; c++) {
                col[c] *= v.color[c];
            }
        }
    } else {
        col = vertex;
    }
    std::array<uint8_t, 4> out;
    for (int c = 0; c < 3; c++) {
        const float base = mat.shadeUsesVertex ? std::clamp(col[c], 0.0f, 1.0f) : 1.0f;
        out[c] = ToByte(base * mat.shadeScale[c]);
    }
    out[3] = ToByte(std::clamp(col[3], 0.0f, 1.0f) * mat.shadeAlphaScale);
    return out;
}

} // namespace Oot3d
