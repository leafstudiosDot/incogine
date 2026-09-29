// Incogine Camera implementation (pure math, no SDL/GL).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "camera.h"

#include <cmath>

namespace icg {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kIsoYaw = -45.0f;
constexpr float kIsoPitch = 35.264f; // asin(tan(30deg)): true dimetric, eye above

float DegToRad(float degrees) {
    return degrees * (kPi / 180.0f);
}

// Standard GL ortho (column-major). With (0, w, h, 0, -1, 1) this reproduces
// QuadRenderer::SetViewport exactly.
Mat4 Ortho(float left, float right, float bottom, float top, float nearPlane,
           float farPlane) {
    Mat4 out;
    for (int i = 0; i < 16; ++i) {
        out.m[i] = 0.0f;
    }
    out.m[0] = 2.0f / (right - left);
    out.m[5] = 2.0f / (top - bottom);
    out.m[10] = -2.0f / (farPlane - nearPlane);
    out.m[12] = -(right + left) / (right - left);
    out.m[13] = -(top + bottom) / (top - bottom);
    out.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    out.m[15] = 1.0f;
    return out;
}

Mat4 Perspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) {
    Mat4 out;
    for (int i = 0; i < 16; ++i) {
        out.m[i] = 0.0f;
    }
    const float f = 1.0f / tanf(DegToRad(fovYDegrees) / 2.0f);
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return out;
}

Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 f = Normalized(center - eye);
    const Vec3 s = Normalized(Cross(f, up));
    const Vec3 u = Cross(s, f);
    Mat4 out;
    out.m[0] = s.x;
    out.m[1] = u.x;
    out.m[2] = -f.x;
    out.m[3] = 0.0f;
    out.m[4] = s.y;
    out.m[5] = u.y;
    out.m[6] = -f.y;
    out.m[7] = 0.0f;
    out.m[8] = s.z;
    out.m[9] = u.z;
    out.m[10] = -f.z;
    out.m[11] = 0.0f;
    out.m[12] = -Dot(s, eye);
    out.m[13] = -Dot(u, eye);
    out.m[14] = Dot(f, eye);
    out.m[15] = 1.0f;
    return out;
}

Mat4 Translate(float x, float y, float z) {
    Mat4 out;
    out.m[12] = x;
    out.m[13] = y;
    out.m[14] = z;
    return out;
}

Mat4 Scale(float x, float y, float z) {
    Mat4 out;
    out.m[0] = x;
    out.m[5] = y;
    out.m[10] = z;
    return out;
}

Vec3 OrbitDirection(float yawDegrees, float pitchDegrees) {
    const float yaw = DegToRad(yawDegrees);
    const float pitch = DegToRad(pitchDegrees);
    // Orbit: direction from target toward the eye.
    return Normalized(Vec3{cosf(pitch) * cosf(yaw), sinf(pitch),
                           cosf(pitch) * sinf(yaw)});
}

} // namespace

Camera Camera::Make2D(float width, float height) {
    Camera camera;
    camera.is2D_ = true;
    camera.projection_ = Projection::Orthographic;
    camera.orthoWidth_ = width;
    camera.orthoHeight_ = height;
    return camera;
}

Camera Camera::Make3D(const Vec3& target, float distance, float fovYDegrees) {
    Camera camera;
    camera.is2D_ = false;
    camera.projection_ = Projection::Perspective;
    camera.target_ = target;
    camera.distance_ = distance;
    camera.fovYDegrees_ = fovYDegrees;
    camera.yawDegrees_ = -45.0f;
    camera.pitchDegrees_ = 20.0f;
    return camera;
}

void Camera::SetProjection(Projection projection) {
    projection_ = projection;
    if (projection_ == Projection::Isometric) {
        ApplyIsometricPreset();
    }
}

void Camera::SetPan(float x, float y) {
    pan_.x = x;
    pan_.y = y;
}

void Camera::SetZoom(float zoom) {
    zoom_ = (zoom > 1e-6f) ? zoom : 1e-6f;
}

void Camera::SetTarget(const Vec3& target) {
    target_ = target;
}

void Camera::SetDistance(float distance) {
    distance_ = (distance > 1e-6f) ? distance : 1e-6f;
}

void Camera::SetYawPitch(float yawDegrees, float pitchDegrees) {
    yawDegrees_ = yawDegrees;
    if (pitchDegrees > 89.0f) {
        pitchDegrees_ = 89.0f;
    } else if (pitchDegrees < -89.0f) {
        pitchDegrees_ = -89.0f;
    } else {
        pitchDegrees_ = pitchDegrees;
    }
}

void Camera::SetFovY(float fovYDegrees) {
    fovYDegrees_ = fovYDegrees;
}

void Camera::SetNearFar(float nearPlane, float farPlane) {
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
}

void Camera::SetOrthoHeight(float height) {
    ortho3DHeight_ = (height > 1e-6f) ? height : 1e-6f;
}

void Camera::ApplyIsometricPreset() {
    projection_ = Projection::Isometric;
    yawDegrees_ = kIsoYaw;
    pitchDegrees_ = kIsoPitch;
}

Mat4 Camera::GetViewMatrix() const {
    if (is2D_) {
        // Screen = world * zoom - pan (identity at defaults: pixel-exact).
        return Multiply(Translate(-pan_.x, -pan_.y, 0.0f),
                        Scale(zoom_, zoom_, 1.0f));
    }
    const Vec3 eye = target_ + OrbitDirection(yawDegrees_, pitchDegrees_) * distance_;
    Vec3 up = {0.0f, 1.0f, 0.0f};
    if (fabsf(Dot(Normalized(eye - target_), up)) > 0.999f) {
        up = {0.0f, 0.0f, -1.0f};
    }
    return LookAt(eye, target_, up);
}

Mat4 Camera::GetProjectionMatrix(float aspect) const {
    if (is2D_ || projection_ == Projection::Orthographic ||
        projection_ == Projection::Isometric) {
        if (is2D_) {
            return Ortho(0.0f, orthoWidth_, orthoHeight_, 0.0f, -1.0f, 1.0f);
        }
        const float half = ortho3DHeight_ / 2.0f;
        return Ortho(-half * aspect, half * aspect, -half, half, nearPlane_,
                     farPlane_);
    }
    return Perspective(fovYDegrees_, aspect, nearPlane_, farPlane_);
}

Ray Camera::ScreenPointToRay(float pixelX, float pixelY, float viewportWidth,
                             float viewportHeight) const {
    const float aspect =
        (viewportHeight > 1e-6f) ? viewportWidth / viewportHeight : 1.0f;
    const Mat4 vp = Multiply(GetProjectionMatrix(aspect), GetViewMatrix());
    Mat4 inv;
    const float ndcX = 2.0f * pixelX / viewportWidth - 1.0f;
    const float ndcY = 1.0f - 2.0f * pixelY / viewportHeight; // y-down screen
    Ray ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    if (!Inverse(vp, inv)) {
        return ray;
    }
    const Vec3 nearPoint = TransformPoint(inv, Vec3{ndcX, ndcY, -1.0f});
    const Vec3 farPoint = TransformPoint(inv, Vec3{ndcX, ndcY, 1.0f});
    ray.origin = nearPoint;
    ray.direction = Normalized(farPoint - nearPoint);
    return ray;
}

Vec3 Camera::WorldToScreen(const Vec3& world, float viewportWidth,
                           float viewportHeight) const {
    const float aspect =
        (viewportHeight > 1e-6f) ? viewportWidth / viewportHeight : 1.0f;
    const Mat4 vp = Multiply(GetProjectionMatrix(aspect), GetViewMatrix());
    // Project forward: clip = VP * world, then NDC -> pixels (y down).
    const Vec3 ndc = TransformPoint(vp, world);
    return Vec3{(ndc.x + 1.0f) * 0.5f * viewportWidth,
                (1.0f - ndc.y) * 0.5f * viewportHeight, ndc.z};
}

bool Camera::IntersectRayPlane(const Ray& ray, const Vec3& planePoint,
                               const Vec3& planeNormal, float& outT) {
    const float denom = Dot(ray.direction, planeNormal);
    if (denom > -1e-8f && denom < 1e-8f) {
        return false;
    }
    outT = Dot(planePoint - ray.origin, planeNormal) / denom;
    return outT >= 0.0f;
}

} // namespace icg
