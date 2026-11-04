#pragma once
// OoT3D asset converter: pairing each vanilla N64 animation of a skeleton with the 3DS animation (CSAB) of the same
// motion, so the 3DS model always plays 3DS animation data (the 3DS animations are authoritative, no N64 fallback).
//
// Grezzo ported every animation, mostly with the N64 frame count, but rebuilt many rigs, renamed everything
// (Japanese names), and split or added clips. Pairing therefore works on motion: joint positions sampled over
// normalized time, compared through a limb -> bone correspondence found from the most unambiguous candidate pair.

#include "Oot3dAnim.h"
#include "Oot3dZar.h"

#include <string>
#include <vector>

namespace Oot3d {

struct AnimCandidate {
    std::string name; // CSAB path inside the archive
    const Csab* csab = nullptr;
};

struct AnimPair {
    std::string n64Path;
    int n64Frames = 0;
    int csab = -1; // index into the candidate list, -1: none (a bug to fix, never an N64 fallback)
    float score = 0; // lower is better: motion error (fraction of model height) + frame count penalty
    bool frameCountMatches = false;
};

struct AnimPairing {
    std::vector<int> limbToBone; // N64 limb -> 3DS bone (-1: no counterpart)
    float correspondenceError = 0;
    std::vector<AnimPair> pairs;
};

AnimPairing PairAnimations(const VanillaSkeleton& skel, const std::vector<VanillaAnimation>& n64,
                           const Cmb& cmb, const std::vector<AnimCandidate>& csabs);

// Minimum-cost one-to-one assignment of rows to columns (-1: row left unassigned when rows > columns).
std::vector<int> AssignMinCost(const std::vector<std::vector<float>>& cost);

} // namespace Oot3d
