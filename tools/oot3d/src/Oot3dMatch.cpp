// OoT3D asset converter: telling which 3DS asset stands for which vanilla one by name and color.

#include "Oot3dMatch.h"

#include "Oot3dN64Texture.h"
#include "Oot3dZar.h"
#include "Oot3dVanilla.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>

namespace Oot3d {

// Lowercase words of a vanilla symbol: "gTwinrovaKotakeSkel" -> twinrova, kotake.
std::vector<std::string> NameTokens(const std::string& path) {
    std::string n = path.substr(path.find_last_of('/') + 1);
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        static const std::set<std::string> kGeneric = { "skel", "object", "anim", "model", "limb", "limbs", "eff",
                                                        "obj", "keep", "gameplay", "unused", "beta", "ref", "tex",
                                                        "dlist", "opa", "xlu", "field", "dangeon", "mdl", "info" };
        std::string l = ToLower(cur);
        if (l.size() >= 3 && !kGeneric.count(l)) {
            out.push_back(l);
        }
        cur.clear();
    };
    for (size_t i = 0; i < n.size(); i++) {
        const char c = n[i];
        if (c == '_' || isdigit(static_cast<unsigned char>(c))) {
            flush();
            continue;
        }
        if (isupper(static_cast<unsigned char>(c)) && !cur.empty()) {
            flush();
        }
        cur += c;
    }
    flush();
    if (!out.empty() && out[0].size() > 1 && out[0][0] == 'g') {
        out[0] = out[0].substr(1); // the "g" prefix of decomp symbols
    }
    return out;
}

// A word's consonants as romanized Japanese would spell them ("dinolfos" -> dnrfs, "dynafos" -> dnfs), so English
// decomp names line up with Grezzo's file names.
std::string Consonants(const std::string& w) {
    std::string out;
    for (size_t i = 0; i < w.size(); i++) {
        char c = w[i];
        if (c == 'p' && i + 1 < w.size() && w[i + 1] == 'h') {
            c = 'f';
            i++;
        }
        switch (c) {
            case 'a': case 'e': case 'i': case 'o': case 'u': case 'y': case 'h': continue;
            case 'l': c = 'r'; break;
            case 'z': c = 's'; break;
            case 'v': c = 'b'; break;
            case 'c': case 'q': c = 'k'; break;
            default: break;
        }
        if (!isalpha(static_cast<unsigned char>(c)) || (!out.empty() && out.back() == c)) {
            continue;
        }
        out += c;
    }
    return out;
}

size_t CommonSubsequence(const std::string& a, const std::string& b) {
    std::vector<size_t> prev(b.size() + 1, 0), cur(b.size() + 1, 0);
    for (size_t i = 1; i <= a.size(); i++) {
        for (size_t j = 1; j <= b.size(); j++) {
            cur[j] = a[i - 1] == b[j - 1] ? prev[j - 1] + 1 : std::max(prev[j], cur[j - 1]);
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

// 0..1: how much of the vanilla name's words appear in the 3DS model name (the longest common substring of a word,
// or its consonants in order).
float NameMatch(const std::vector<std::string>& tokens, const std::string& modelPath) {
    // The file name without Grezzo's generic parts ("_model", "_modelT", "_mdl_info").
    std::string stem = ToLower(StemOf(modelPath));
    for (const char* g : { "_modelt", "_model", "model", "_mdl", "_info", "_obj" }) {
        for (size_t at; (at = stem.find(g)) != std::string::npos;) {
            stem.erase(at, strlen(g));
        }
    }
    const std::string stemC = Consonants(stem);
    float best = 0;
    for (const std::string& t : tokens) {
        size_t lcs = 0;
        for (size_t i = 0; i < t.size(); i++) {
            for (size_t j = 0; j < stem.size(); j++) {
                size_t k = 0;
                while (i + k < t.size() && j + k < stem.size() && t[i + k] == stem[j + k]) {
                    k++;
                }
                lcs = std::max(lcs, k);
            }
        }
        if (lcs >= 4 || (lcs >= 3 && lcs == t.size())) {
            best = std::max(best, float(lcs) / float(t.size()));
        }
        const std::string tc = Consonants(t);
        if (tc.size() >= 4) {
            const float r = float(CommonSubsequence(tc, stemC)) / float(tc.size());
            if (r >= 0.75f) {
                best = std::max(best, r);
            }
        }
    }
    return best;
}

// How different two mean colors are: hue (chromaticity) mostly, brightness a little. 0 = the same.
float ColorDistance(const Vec3& a, const Vec3& b) {
    const float sa = a.x + a.y + a.z + 1e-3f, sb = b.x + b.y + b.z + 1e-3f;
    const float hue = std::fabs(a.x / sa - b.x / sb) + std::fabs(a.y / sa - b.y / sb) + std::fabs(a.z / sa - b.z / sb);
    return 3.0f * hue + 0.3f * std::fabs(std::log(sa / sb));
}

// Mean color of a vanilla skeleton's limb textures (palettes stand in for their color-indexed textures).
std::optional<Vec3> SkeletonColor(const ZipReader& oot, const VanillaSkeleton& skel) {
    std::set<std::string> lists, textures;
    for (const VanillaLimb& l : skel.limbs) {
        for (const std::string& d : { l.dList, l.dList2 }) {
            if (!d.empty()) {
                lists.insert(d);
            }
        }
    }
    for (const std::string& d : lists) {
        const VanillaDlInfo info = InspectVanillaDl(oot, d);
        textures.insert(info.textures.begin(), info.textures.end());
        for (const std::string& c : info.calledLists) {
            const VanillaDlInfo ci = InspectVanillaDl(oot, c);
            textures.insert(ci.textures.begin(), ci.textures.end());
        }
    }
    Vec3 sum;
    int n = 0;
    for (const std::string& t : textures) {
        auto tex = ReadVanillaTexture(oot, t);
        if (!tex || tex->type == 3 || tex->type == 4 || tex->type == 5 || tex->type == 7) {
            continue; // indexed (their palettes count instead) and intensity-only textures carry no hue
        }
        sum += MeanColor(tex->image);
        n++;
    }
    if (n == 0) {
        return std::nullopt;
    }
    return sum * (1.0f / n);
}

// Mean color of a 3DS model's textures.
std::optional<Vec3> ModelColor(const Cmb& cmb) {
    Vec3 sum;
    int n = 0;
    for (size_t i = 0; i < cmb.textures.size(); i++) {
        try {
            const Image img = cmb.DecodeTexture(i);
            if (img.Valid()) {
                sum += MeanColor(img);
                n++;
            }
        } catch (const std::exception&) {
        }
    }
    if (n == 0) {
        return std::nullopt;
    }
    return sum * (1.0f / n);
}

// Mean color of a vanilla display list's textures (palettes stand in for their color-indexed textures).
std::optional<Vec3> ListColor(const ZipReader& oot, const std::string& path) {
    std::set<std::string> textures;
    const VanillaDlInfo info = InspectVanillaDl(oot, path);
    textures.insert(info.textures.begin(), info.textures.end());
    for (const std::string& c : info.calledLists) {
        const VanillaDlInfo ci = InspectVanillaDl(oot, c);
        textures.insert(ci.textures.begin(), ci.textures.end());
    }
    Vec3 sum;
    int n = 0;
    for (const std::string& t : textures) {
        auto tex = ReadVanillaTexture(oot, t);
        if (!tex || tex->type == 3 || tex->type == 4 || tex->type == 5 || tex->type == 7) {
            continue;
        }
        sum += MeanColor(tex->image);
        n++;
    }
    if (n == 0) {
        return std::nullopt;
    }
    return sum * (1.0f / n);
}

} // namespace Oot3d
