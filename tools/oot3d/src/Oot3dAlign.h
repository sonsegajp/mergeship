#pragma once
// OoT3D asset converter: rigid alignment of point sets (3DS models onto the vanilla geometry they stand in for).

#include "Oot3dMath.h"

#include <vector>

namespace Oot3d {

std::vector<Vec3> Subsample(const std::vector<Vec3>& p, size_t n);
Vec3 Mean(const std::vector<Vec3>& p);
// Mean distance from each of `from` (transformed by m) to its nearest point of `to`.
float MeanNearest(const std::vector<Vec3>& from, const std::vector<Vec3>& to, const Mat34& m);
// Best-fit rotation + translation taking a onto b (pairs by index).
Mat34 Kabsch(const std::vector<Vec3>& a, const std::vector<Vec3>& b);
// Rigid transform taking the 3DS points onto the vanilla ones: tries the principal-axis orientations, refines each
// with ICP, keeps the best. `error` is the mean nearest-point distance both ways.
Mat34 Align(const std::vector<Vec3>& src3ds, const std::vector<Vec3>& dstN64, float& error);
// ICP from a given start (no orientation search): takes out a small systematic offset between two placements that
// already roughly agree. `error` is the mean distance from a's points to b afterwards.
Mat34 Refine(const std::vector<Vec3>& a, const std::vector<Vec3>& b, const Mat34& start, int iterations, float& error);
// The same, one way: how well `part` fits somewhere onto `whole` (a hand onto a hand-and-ocarina). `error` is the
// mean distance from part's points to whole.
Mat34 AlignInto(const std::vector<Vec3>& part, const std::vector<Vec3>& whole, float& error);

} // namespace Oot3d
