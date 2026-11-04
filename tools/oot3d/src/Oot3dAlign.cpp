#include "Oot3dAlign.h"

#include <algorithm>
#include <cmath>

namespace Oot3d {

std::vector<Vec3> Subsample(const std::vector<Vec3>& p, size_t n) {
    if (p.size() <= n) {
        return p;
    }
    std::vector<Vec3> out;
    const double step = double(p.size()) / n;
    for (size_t i = 0; i < n; i++) {
        out.push_back(p[size_t(i * step)]);
    }
    return out;
}

Vec3 Mean(const std::vector<Vec3>& p) {
    Vec3 m;
    for (const Vec3& v : p) {
        m += v;
    }
    return p.empty() ? m : m * (1.0f / p.size());
}

namespace {
// Symmetric 3x3 eigen decomposition (Jacobi); columns of `vec` are the eigenvectors, sorted by eigenvalue (desc).
void Eigen3(double a[3][3], double vec[3][3], double val[3]) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            vec[i][j] = i == j;
        }
    }
    for (int sweep = 0; sweep < 50; sweep++) {
        for (int p = 0; p < 2; p++) {
            for (int q = p + 1; q < 3; q++) {
                if (std::fabs(a[p][q]) < 1e-12) {
                    continue;
                }
                const double theta = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const double t = (theta >= 0 ? 1 : -1) / (std::fabs(theta) + std::sqrt(theta * theta + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (int k = 0; k < 3; k++) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; k++) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; k++) {
                    const double vkp = vec[k][p], vkq = vec[k][q];
                    vec[k][p] = c * vkp - s * vkq;
                    vec[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        val[i] = a[i][i];
    }
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (val[j] > val[i]) {
                std::swap(val[i], val[j]);
                for (int k = 0; k < 3; k++) {
                    std::swap(vec[k][i], vec[k][j]);
                }
            }
        }
    }
}

void Axes(const std::vector<Vec3>& p, const Vec3& mean, Vec3 axes[3]) {
    double c[3][3] = {};
    for (const Vec3& v : p) {
        const double d[3] = { v.x - mean.x, v.y - mean.y, v.z - mean.z };
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                c[i][j] += d[i] * d[j];
            }
        }
    }
    double vec[3][3], val[3];
    Eigen3(c, vec, val);
    for (int k = 0; k < 3; k++) {
        axes[k] = Vec3(float(vec[0][k]), float(vec[1][k]), float(vec[2][k]));
    }
}

} // namespace

float MeanNearest(const std::vector<Vec3>& from, const std::vector<Vec3>& to, const Mat34& m) {
    double sum = 0;
    for (const Vec3& v : from) {
        const Vec3 p = m.Point(v);
        float best = 1e30f;
        for (const Vec3& t : to) {
            best = std::min(best, (p - t).Dot(p - t));
        }
        sum += std::sqrt(best);
    }
    return from.empty() ? 1e30f : float(sum / from.size());
}

// Best-fit rotation (Kabsch via the 3x3 SVD done as eigen decomposition of H^T H).
Mat34 Kabsch(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    const Vec3 ca = Mean(a), cb = Mean(b);
    double h[3][3] = {};
    for (size_t i = 0; i < a.size(); i++) {
        const double pa[3] = { a[i].x - ca.x, a[i].y - ca.y, a[i].z - ca.z };
        const double pb[3] = { b[i].x - cb.x, b[i].y - cb.y, b[i].z - cb.z };
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                h[r][c] += pa[r] * pb[c];
            }
        }
    }
    // R = V U^T from H = U S V^T; with H^T H = V S^2 V^T and U = H V S^-1.
    double hth[3][3] = {};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            for (int k = 0; k < 3; k++) {
                hth[r][c] += h[k][r] * h[k][c];
            }
        }
    }
    double v[3][3], s2[3];
    Eigen3(hth, v, s2);
    double u[3][3];
    for (int k = 0; k < 3; k++) {
        const double s = std::sqrt(std::max(s2[k], 1e-18));
        for (int r = 0; r < 3; r++) {
            double acc = 0;
            for (int c = 0; c < 3; c++) {
                acc += h[r][c] * v[c][k];
            }
            u[r][k] = acc / s;
        }
    }
    double rot[3][3];
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            rot[r][c] = 0;
            for (int k = 0; k < 3; k++) {
                rot[r][c] += v[r][k] * u[c][k];
            }
        }
    }
    const double det = rot[0][0] * (rot[1][1] * rot[2][2] - rot[1][2] * rot[2][1]) -
                       rot[0][1] * (rot[1][0] * rot[2][2] - rot[1][2] * rot[2][0]) +
                       rot[0][2] * (rot[1][0] * rot[2][1] - rot[1][1] * rot[2][0]);
    if (det < 0) {
        for (int r = 0; r < 3; r++) {
            v[r][2] = -v[r][2];
        }
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                rot[r][c] = 0;
                for (int k = 0; k < 3; k++) {
                    rot[r][c] += v[r][k] * u[c][k];
                }
            }
        }
    }
    Mat34 m;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            m.m[r][c] = float(rot[r][c]);
        }
    }
    const Vec3 t = cb - m.Dir(ca);
    m.m[0][3] = t.x, m.m[1][3] = t.y, m.m[2][3] = t.z;
    return m;
}

// Rigid transform taking the 3DS points onto the vanilla ones: tries the principal-axis orientations, refines each
// with ICP, keeps the best. `error` is the mean nearest-point distance afterwards.
Mat34 Align(const std::vector<Vec3>& src3ds, const std::vector<Vec3>& dstN64, float& error) {
    const std::vector<Vec3> a = Subsample(src3ds, 600), b = Subsample(dstN64, 600);
    const Vec3 ma = Mean(a), mb = Mean(b);
    Vec3 ax[3], bx[3];
    Axes(a, ma, ax);
    Axes(b, mb, bx);
    Mat34 best;
    error = 1e30f;
    // Candidate sign flips of the first two axes (the third follows for a proper rotation), plus the identity.
    std::vector<Mat34> starts{ Mat34::Identity() };
    for (int s0 : { 1, -1 }) {
        for (int s1 : { 1, -1 }) {
            const Vec3 a0 = ax[0] * float(s0), a1 = ax[1] * float(s1), a2 = a0.Cross(a1);
            const Vec3 b0 = bx[0], b1 = bx[1], b2 = b0.Cross(b1);
            // R maps a-basis onto b-basis: R = B A^T
            Mat34 r;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    r.m[i][j] = b0[i] * a0[j] + b1[i] * a1[j] + b2[i] * a2[j];
                }
            }
            const Vec3 t = mb - r.Dir(ma);
            r.m[0][3] = t.x, r.m[1][3] = t.y, r.m[2][3] = t.z;
            starts.push_back(r);
        }
    }
    for (Mat34 m : starts) {
        for (int it = 0; it < 25; it++) {
            std::vector<Vec3> from, to;
            for (const Vec3& v : a) {
                const Vec3 p = m.Point(v);
                const Vec3* nearest = nullptr;
                float bd = 1e30f;
                for (const Vec3& t : b) {
                    const float d = (p - t).Dot(p - t);
                    if (d < bd) {
                        bd = d;
                        nearest = &t;
                    }
                }
                if (nearest) {
                    from.push_back(v);
                    to.push_back(*nearest);
                }
            }
            if (from.size() < 3) {
                break;
            }
            m = Kabsch(from, to);
        }
        const float e = 0.5f * (MeanNearest(a, b, m) + MeanNearest(b, a, m.Inverse()));
        if (e < error) {
            error = e;
            best = m;
        }
    }
    return best;
}


Mat34 AlignInto(const std::vector<Vec3>& part, const std::vector<Vec3>& whole, float& error) {
    const std::vector<Vec3> a = Subsample(part, 400), b = Subsample(whole, 800);
    error = 1e30f;
    Mat34 best;
    if (a.size() < 3 || b.size() < 3) {
        return best;
    }
    const Vec3 ma = Mean(a), mb = Mean(b);
    Vec3 ax[3], bx[3];
    Axes(a, ma, ax);
    Axes(b, mb, bx);
    // As placed, centered on the whole, and the principal-axis orientations.
    std::vector<Mat34> starts{ Mat34::Identity() };
    Mat34 shift = Mat34::Identity();
    const Vec3 t0 = mb - ma;
    shift.m[0][3] = t0.x, shift.m[1][3] = t0.y, shift.m[2][3] = t0.z;
    starts.push_back(shift);
    for (int s0 : { 1, -1 }) {
        for (int s1 : { 1, -1 }) {
            const Vec3 a0 = ax[0] * float(s0), a1 = ax[1] * float(s1), a2 = a0.Cross(a1);
            const Vec3 b0 = bx[0], b1 = bx[1], b2 = b0.Cross(b1);
            Mat34 r;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    r.m[i][j] = b0[i] * a0[j] + b1[i] * a1[j] + b2[i] * a2[j];
                }
            }
            const Vec3 t = mb - r.Dir(ma);
            r.m[0][3] = t.x, r.m[1][3] = t.y, r.m[2][3] = t.z;
            starts.push_back(r);
        }
    }
    for (Mat34 m : starts) {
        for (int it = 0; it < 20; it++) {
            std::vector<Vec3> from, to;
            for (const Vec3& v : a) {
                const Vec3 p = m.Point(v);
                const Vec3* nearest = nullptr;
                float bd = 1e30f;
                for (const Vec3& t : b) {
                    const float d = (p - t).Dot(p - t);
                    if (d < bd) {
                        bd = d;
                        nearest = &t;
                    }
                }
                if (nearest) {
                    from.push_back(v);
                    to.push_back(*nearest);
                }
            }
            if (from.size() < 3) {
                break;
            }
            m = Kabsch(from, to);
        }
        const float e = MeanNearest(a, b, m);
        if (e < error) {
            error = e;
            best = m;
        }
    }
    return best;
}

Mat34 Refine(const std::vector<Vec3>& aIn, const std::vector<Vec3>& bIn, const Mat34& start, int iterations,
             float& error) {
    const std::vector<Vec3> a = Subsample(aIn, 400), b = Subsample(bIn, 800);
    Mat34 m = start;
    for (int it = 0; it < iterations && a.size() >= 3 && b.size() >= 3; it++) {
        std::vector<Vec3> from, to;
        for (const Vec3& v : a) {
            const Vec3 p = m.Point(v);
            const Vec3* nearest = nullptr;
            float bd = 1e30f;
            for (const Vec3& t : b) {
                const float d = (p - t).Dot(p - t);
                if (d < bd) {
                    bd = d;
                    nearest = &t;
                }
            }
            if (nearest) {
                from.push_back(v);
                to.push_back(*nearest);
            }
        }
        if (from.size() < 3) {
            break;
        }
        m = Kabsch(from, to);
    }
    error = MeanNearest(a, b, m);
    return m;
}

} // namespace Oot3d
