// Incogine - 2D vector animation: shared scalar/color/transform types.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Everything here is 2D and module-local on purpose. The engine already has
// `Position`/`Scale`/`Rotation` (3D, doubles, in `src/core/objects/objects.h`)
// and `icg::Vec3`/`icg::Mat4` (`src/core/render/camera_math.h`); this feature
// does not touch either, so a future 3D animation editor can sit beside this
// module instead of inheriting 2D-only assumptions.
//
// Header-only and dependency-free, mirroring camera_math.h.

#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace icg {
namespace anim {

// ---------------------------------------------------------------- scalars --

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float px, float py) : x(px), y(py) {}
};

inline Vec2 operator+(const Vec2& a, const Vec2& b) {
    return {a.x + b.x, a.y + b.y};
}
inline Vec2 operator-(const Vec2& a, const Vec2& b) {
    return {a.x - b.x, a.y - b.y};
}
inline Vec2 operator*(const Vec2& a, float s) {
    return {a.x * s, a.y * s};
}
inline Vec2 operator*(float s, const Vec2& a) {
    return a * s;
}
inline float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}
inline float Cross(const Vec2& a, const Vec2& b) {
    return a.x * b.y - a.y * b.x;
}
inline float Length(const Vec2& a) {
    return std::sqrt(a.x * a.x + a.y * a.y);
}
inline float Distance(const Vec2& a, const Vec2& b) {
    return Length(a - b);
}
inline Vec2 Lerp(const Vec2& a, const Vec2& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
inline Vec2 Normalized(const Vec2& a) {
    const float len = Length(a);
    return len > 1e-8f ? a * (1.0f / len) : Vec2{};
}

inline bool operator==(const Vec2& a, const Vec2& b) {
    return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const Vec2& a, const Vec2& b) {
    return !(a == b);
}

// ----------------------------------------------------------------- color --

// Straight (non-premultiplied) 8-bit RGBA. Authoring values; the rasterizer
// converts to float.
struct AnimColor {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;

    AnimColor() = default;
    AnimColor(int cr, int cg, int cb, int ca) : r(cr), g(cg), b(cb), a(ca) {}

    static AnimColor FromRgbaF(float fr, float fg, float fb, float fa);

    // "#RRGGBB" or "#RRGGBBAA" (alpha optional). Returns white on failure.
    static AnimColor FromHex(const std::string& hex);

    // Always "#RRGGBBAA" - round-trips through FromHex exactly.
    std::string ToHex() const;

    bool operator==(const AnimColor& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const AnimColor& o) const { return !(*this == o); }
};

// --------------------------------------------------- 2D affine transform --

// 2x3 affine: [ a c e ; b d f ], i.e. x' = a*x + c*y + e, y' = b*x + d*y + f.
// Column-major storage matches Mat4's convention: m[col * 3 + row].
// Rotation is in radians. Skew is in radians (shear).
struct Mat2x3 {
    float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, e = 0.0f, f = 0.0f;
};

// How a stroke ends.
enum class LineCap { Butt, Round, Square };
// How a stroke turns a corner.
enum class LineJoin { Miter, Round, Bevel };;

// Exact comparison: used to detect whether a cached baked path is still
// valid, not geometric closeness (same inputs recompute bitwise-identical
// outputs, so any difference means the transform really moved).
inline bool operator==(const Mat2x3& x, const Mat2x3& y) {
    return x.a == y.a && x.b == y.b && x.c == y.c && x.d == y.d &&
           x.e == y.e && x.f == y.f;
}
inline bool operator!=(const Mat2x3& x, const Mat2x3& y) {
    return !(x == y);
}

inline Mat2x3 Mat2x3Identity() {
    return Mat2x3();
}

inline Mat2x3 Mat2x3Translate(float tx, float ty) {
    Mat2x3 m;
    m.e = tx;
    m.f = ty;
    return m;
}

inline Mat2x3 Mat2x3Scale(float sx, float sy) {
    Mat2x3 m;
    m.a = sx;
    m.d = sy;
    return m;
}

inline Mat2x3 Mat2x3Rotate(float radians) {
    Mat2x3 m;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    m.a = c;
    m.b = s;
    m.c = -s;
    m.d = c;
    return m;
}

inline Mat2x3 Mat2x3Skew(float kx, float ky) {
    Mat2x3 m;
    m.c = std::tan(kx);
    m.b = std::tan(ky);
    return m;
}

// Composition: (lhs * rhs) applies rhs first, then lhs.
inline Mat2x3 operator*(const Mat2x3& lhs, const Mat2x3& rhs) {
    Mat2x3 out;
    out.a = lhs.a * rhs.a + lhs.c * rhs.b;
    out.b = lhs.b * rhs.a + lhs.d * rhs.b;
    out.c = lhs.a * rhs.c + lhs.c * rhs.d;
    out.d = lhs.b * rhs.c + lhs.d * rhs.d;
    out.e = lhs.a * rhs.e + lhs.c * rhs.f + lhs.e;
    out.f = lhs.b * rhs.e + lhs.d * rhs.f + lhs.f;
    return out;
}

inline Vec2 TransformPoint(const Mat2x3& m, const Vec2& p) {
    return {m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f};
}

inline Vec2 TransformVector(const Mat2x3& m, const Vec2& v) {
    // Direction only: translation must not apply.
    return {m.a * v.x + m.c * v.y, m.b * v.x + m.d * v.y};
}

// General 2x3 inverse. Returns false for a singular matrix (zero scale).
inline bool Invert(const Mat2x3& m, Mat2x3& out) {
    const float det = m.a * m.d - m.b * m.c;
    if (det > -1e-8f && det < 1e-8f) {
        return false;
    }
    const float invDet = 1.0f / det;
    out.a = m.d * invDet;
    out.b = -m.b * invDet;
    out.c = -m.c * invDet;
    out.d = m.a * invDet;
    out.e = (m.c * m.f - m.d * m.e) * invDet;
    out.f = (m.b * m.e - m.a * m.f) * invDet;
    return true;
}

// Average absolute scale - used to turn a stroke width into device pixels.
inline float Mat2x3MeanScale(const Mat2x3& m) {
    const float sx = std::sqrt(m.a * m.a + m.b * m.b);
    const float sy = std::sqrt(m.c * m.c + m.d * m.d);
    return (sx + sy) * 0.5f;
}

inline bool Mat2x3IsIdentity(const Mat2x3& m, float epsilon = 1e-6f) {
    return std::fabs(m.a - 1.0f) < epsilon && std::fabs(m.b) < epsilon &&
           std::fabs(m.c) < epsilon && std::fabs(m.d - 1.0f) < epsilon &&
           std::fabs(m.e) < epsilon && std::fabs(m.f) < epsilon;
}

// ---------------------------------------------------------------- easing --

// Cubic-bezier control points reused from the engine's own easing
// (`src/core/engine/math.h`) so easing curves match splash/menu motion.
enum class EasingKind {
    Linear = 0,
    CubicBezier = 1,
    // Reserved for later milestones; parsed and round-tripped but not sampled.
    EaseIn = 2,
    EaseOut = 3,
    EaseInOut = 4,
};

struct Easing {
    EasingKind kind = EasingKind::Linear;
    // Cubic-bezier control points; only meaningful for EasingKind::CubicBezier.
    float p1x = 0.0f, p1y = 0.0f, p2x = 1.0f, p2y = 1.0f;

    static Easing Linear() { return Easing(); }
    static Easing Cubic(float x1, float y1, float x2, float y2);

    // Maps linear progress 0..1 through the curve. CubicBezier shares the
    // engine's numeric solve; other kinds fall back to linear until they are
    // implemented (no reserved-but-silent behavior elsewhere in the codebase).
    float Apply(float t) const;

    bool operator==(const Easing& o) const {
        return kind == o.kind && p1x == o.p1x && p1y == o.p1y && p2x == o.p2x &&
               p2y == o.p2y;
    }
    bool operator!=(const Easing& o) const { return !(*this == o); }
};

// ------------------------------------------------------------- tween span --

// What a span between two keyframes interpolates. Motion and Shape Tween are
// not implemented yet - they are reserved in the timeline model so the spans
// exist, persist, and round-trip (Milestone 7).
enum class TweenType {
    None = 0,
    Motion = 1, // position, scale, rotation, alpha, color transform
    Shape = 2,  // morph between two 2D vector shapes
};

// Which properties a Motion Tween interpolates. Inert data until Motion Tween
// lands: stored in the file, never consulted by the rasterizer.
enum MotionFlag : uint8_t {
    kMotionPosition = 1u << 0,
    kMotionScale = 1u << 1,
    kMotionRotation = 1u << 2,
    kMotionAlpha = 1u << 3,
    kMotionColor = 1u << 4,
};

inline constexpr uint8_t kMotionAll = kMotionPosition | kMotionScale |
                                      kMotionRotation | kMotionAlpha | kMotionColor;

// Describes the span from the *previous* keyframe on a layer to the keyframe
// that owns this span (Flash-style). `None` means hold the previous keyframe's
// values, which is what every span does until Milestone 7.
struct TweenSpan {
    TweenType type = TweenType::None;
    Easing easing = Easing::Linear();
    // Only meaningful for TweenType::Motion.
    uint8_t motionFlags = kMotionAll;
    // Only meaningful for TweenType::Shape: record point counts and winding so
    // a morph can be matched point-for-point without re-guessing.
    bool shapeHints = true;

    bool Interpolates() const { return type != TweenType::None; }

    bool operator==(const TweenSpan& o) const {
        return type == o.type && easing == o.easing &&
               motionFlags == o.motionFlags && shapeHints == o.shapeHints;
    }
    bool operator!=(const TweenSpan& o) const { return !(*this == o); }
};

} // namespace anim
} // namespace icg
