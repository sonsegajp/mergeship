#pragma once
// OoT3D asset converter: small vector/affine-matrix helpers.

#include <algorithm>
#include <cmath>

namespace Oot3d {

struct Vec3 {
    float x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {
    }
    Vec3 operator+(const Vec3& o) const {
        return { x + o.x, y + o.y, z + o.z };
    }
    Vec3 operator-(const Vec3& o) const {
        return { x - o.x, y - o.y, z - o.z };
    }
    Vec3 operator*(float s) const {
        return { x * s, y * s, z * s };
    }
    Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    float Dot(const Vec3& o) const {
        return x * o.x + y * o.y + z * o.z;
    }
    Vec3 Cross(const Vec3& o) const {
        return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
    }
    float Length() const {
        return std::sqrt(Dot(*this));
    }
    Vec3 Normalized() const {
        const float l = Length();
        return l > 1e-12f ? *this * (1.0f / l) : Vec3(0, 1, 0);
    }
    float operator[](int i) const {
        return i == 0 ? x : (i == 1 ? y : z);
    }
};

// Affine 3x4 matrix, row-major: m[r][c], column 3 is the translation. Points are column vectors: p' = M p.
struct Mat34 {
    float m[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };

    static Mat34 Identity() {
        return {};
    }

    // T * Rz * Ry * Rx * S: the CMB bone / N64 limb convention (angles in radians).
    static Mat34 FromSRT(const Vec3& s, const Vec3& r, const Vec3& t) {
        const float sx = std::sin(r.x), cx = std::cos(r.x);
        const float sy = std::sin(r.y), cy = std::cos(r.y);
        const float sz = std::sin(r.z), cz = std::cos(r.z);
        Mat34 o;
        o.m[0][0] = s.x * (cy * cz);
        o.m[1][0] = s.x * (sz * cy);
        o.m[2][0] = s.x * (-sy);
        o.m[0][1] = s.y * (sx * cz * sy - cx * sz);
        o.m[1][1] = s.y * (sx * sz * sy + cx * cz);
        o.m[2][1] = s.y * (sx * cy);
        o.m[0][2] = s.z * (cx * cz * sy + sx * sz);
        o.m[1][2] = s.z * (cx * sz * sy - sx * cz);
        o.m[2][2] = s.z * (cy * cx);
        o.m[0][3] = t.x;
        o.m[1][3] = t.y;
        o.m[2][3] = t.z;
        return o;
    }

    Mat34 operator*(const Mat34& b) const {
        Mat34 o;
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                float v = m[r][0] * b.m[0][c] + m[r][1] * b.m[1][c] + m[r][2] * b.m[2][c];
                if (c == 3) {
                    v += m[r][3];
                }
                o.m[r][c] = v;
            }
        }
        return o;
    }

    Mat34 Scaled(float s) const {
        Mat34 o = *this;
        for (auto& row : o.m) {
            for (float& v : row) {
                v *= s;
            }
        }
        return o;
    }
    Mat34& operator+=(const Mat34& b) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                m[r][c] += b.m[r][c];
            }
        }
        return *this;
    }

    Vec3 Point(const Vec3& p) const {
        return { m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3],
                 m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3],
                 m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3] };
    }
    Vec3 Dir(const Vec3& d) const {
        return { m[0][0] * d.x + m[0][1] * d.y + m[0][2] * d.z, m[1][0] * d.x + m[1][1] * d.y + m[1][2] * d.z,
                 m[2][0] * d.x + m[2][1] * d.y + m[2][2] * d.z };
    }
    // Normals: inverse-transpose of the 3x3 part.
    Vec3 Normal(const Vec3& n) const {
        return Inverse().TransposedDir(n).Normalized();
    }
    Vec3 TransposedDir(const Vec3& d) const {
        return { m[0][0] * d.x + m[1][0] * d.y + m[2][0] * d.z, m[0][1] * d.x + m[1][1] * d.y + m[2][1] * d.z,
                 m[0][2] * d.x + m[1][2] * d.y + m[2][2] * d.z };
    }

    Mat34 Inverse() const {
        const float a = m[0][0], b = m[0][1], c = m[0][2];
        const float d = m[1][0], e = m[1][1], f = m[1][2];
        const float g = m[2][0], h = m[2][1], i = m[2][2];
        const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
        float det = a * A + b * B + c * C;
        if (std::fabs(det) < 1e-20f) {
            det = 1e-20f;
        }
        const float inv = 1.0f / det;
        Mat34 o;
        o.m[0][0] = A * inv;
        o.m[0][1] = -(b * i - c * h) * inv;
        o.m[0][2] = (b * f - c * e) * inv;
        o.m[1][0] = B * inv;
        o.m[1][1] = (a * i - c * g) * inv;
        o.m[1][2] = -(a * f - c * d) * inv;
        o.m[2][0] = C * inv;
        o.m[2][1] = -(a * h - b * g) * inv;
        o.m[2][2] = (a * e - b * d) * inv;
        const Vec3 t(m[0][3], m[1][3], m[2][3]);
        const Vec3 it = o.Dir(t);
        o.m[0][3] = -it.x;
        o.m[1][3] = -it.y;
        o.m[2][3] = -it.z;
        return o;
    }

    Vec3 Translation() const {
        return { m[0][3], m[1][3], m[2][3] };
    }
};

} // namespace Oot3d
