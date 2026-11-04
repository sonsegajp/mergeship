#pragma once
// OoT3D asset converter: telling which 3DS asset stands for which vanilla one by name and color. Grezzo's file
// names are romanized Japanese ("dynafos", "gibud") where the decomp's are English ("Dinolfos", "Gibdo"), so names
// also compare by their consonants; colors compare by hue.

#include "Oot3dCmb.h"
#include "Oot3dMath.h"
#include "Oot3dSkeleton.h"
#include "Oot3dZip.h"

#include <optional>
#include <string>
#include <vector>

namespace Oot3d {

// Lowercase words of a vanilla symbol: "gTwinrovaKotakeSkel" -> twinrova, kotake.
std::vector<std::string> NameTokens(const std::string& path);
// A word's consonants as romanized Japanese would spell them ("dinolfos" -> dnrfs, "dynafos" -> dnfs).
std::string Consonants(const std::string& w);
size_t CommonSubsequence(const std::string& a, const std::string& b);
// 0..1: how much of the vanilla name's words appear in the 3DS file name.
float NameMatch(const std::vector<std::string>& tokens, const std::string& modelPath);
// How different two mean colors are: hue mostly, brightness a little. 0 = the same.
float ColorDistance(const Vec3& a, const Vec3& b);
// Mean colors of a vanilla skeleton's limb textures, a vanilla display list's textures, a 3DS model's textures.
std::optional<Vec3> SkeletonColor(const ZipReader& oot, const VanillaSkeleton& skel);
std::optional<Vec3> ListColor(const ZipReader& oot, const std::string& path);
std::optional<Vec3> ModelColor(const Cmb& cmb);

} // namespace Oot3d
