#ifndef MATH_H
#define MATH_H

#include <cmath>
#include <algorithm>

// Template functions in headers are implicitly inline, but you can add the keyword for clarity.
template<typename T>
inline T lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

inline float cubicBezierX(float t, float x1, float x2) {
    float invT = 1.0f - t;
    return 3 * invT * invT * t * x1 + 3 * invT * t * t * x2 + t * t * t;
}

inline float cubicBezierY(float t, float y1, float y2) {
    float invT = 1.0f - t;
    return 3 * invT * invT * t * y1 + 3 * invT * t * t * y2 + t * t * t;
}

inline float cubicBezier(float t, float x1, float y1, float x2, float y2) {
    float t0 = 0.0f, t1 = 1.0f;
    float tPrime = t;
    for (int i = 0; i < 20; ++i) {
        float x = cubicBezierX(tPrime, x1, x2);
        float error = x - t;
        if (std::fabs(error) < 0.0001f)
            break;
        if (error > 0)
            t1 = tPrime;
        else
            t0 = tPrime;
        tPrime = (t0 + t1) * 0.5f;
    }
    return cubicBezierY(tPrime, y1, y2);
}

#endif // MATH_H
