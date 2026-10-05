#include "anim_types.h"

#include <algorithm>
#include <cstdio>

#include "../engine/math.h"

namespace icg {
namespace anim {

// ----------------------------------------------------------------- color --

AnimColor AnimColor::FromRgbaF(float fr, float fg, float fb, float fa) {
    auto clamp8 = [](float v) {
        const float scaled = v * 255.0f + 0.5f;
        return static_cast<int>(std::min(255.0f, std::max(0.0f, scaled)));
    };
    return AnimColor(clamp8(fr), clamp8(fg), clamp8(fb), clamp8(fa));
}

AnimColor AnimColor::FromHex(const std::string& hex) {
    static const AnimColor kWhite(255, 255, 255, 255);
    if (hex.empty()) {
        return kWhite;
    }
    // Accepts a leading '#'; a bare 6/8-digit string is also fine.
    const char* digits = hex.c_str();
    if (*digits == '#') {
        ++digits;
    }
    const size_t len = std::string(digits).size();
    if (len != 6 && len != 8) {
        return kWhite;
    }
    int values[4] = {0, 0, 0, 255};
    const int count = static_cast<int>(len / 2);
    for (int i = 0; i < count; ++i) {
        const char hi = digits[i * 2];
        const char lo = digits[i * 2 + 1];
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            if (c >= 'A' && c <= 'F') {
                return c - 'A' + 10;
            }
            return -1;
        };
        const int h = nibble(hi);
        const int l = nibble(lo);
        if (h < 0 || l < 0) {
            return kWhite;
        }
        values[i] = h * 16 + l;
    }
    return AnimColor(values[0], values[1], values[2], values[3]);
}

std::string AnimColor::ToHex() const {
    auto byte = [](int v) -> int {
        return std::min(255, std::max(0, v));
    };
    char buf[10] = {};
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", byte(r), byte(g),
                  byte(b), byte(a));
    return std::string(buf);
}

// ---------------------------------------------------------------- easing --

Easing Easing::Cubic(float x1, float y1, float x2, float y2) {
    Easing easing;
    easing.kind = EasingKind::CubicBezier;
    easing.p1x = x1;
    easing.p1y = y1;
    easing.p2x = x2;
    easing.p2y = y2;
    return easing;
}

float Easing::Apply(float t) const {
    const float clamped = std::min(1.0f, std::max(0.0f, t));
    if (clamped <= 0.0f || clamped >= 1.0f) {
        return clamped;
    }
    switch (kind) {
        case EasingKind::CubicBezier:
            // Same solve as the engine's cubicBezier(), so easing curves are
            // identical between scenes and animation.
            return cubicBezier(clamped, p1x, p1y, p2x, p2y);
        case EasingKind::Linear:
        case EasingKind::EaseIn:
        case EasingKind::EaseOut:
        case EasingKind::EaseInOut:
        default:
            return clamped;
    }
}

} // namespace anim
} // namespace icg