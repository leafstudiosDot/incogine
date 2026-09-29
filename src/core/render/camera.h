// Incogine Camera: 2D + 3D views (Perspective / Orthographic / Isometric).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The missing camera the engine never had. 2D mode reproduces the
// QuadRenderer mapping exactly (top-left origin, y down, pixel units at
// zoom 1) plus pan/zoom; 3D mode is orbit-style with perspective,
// orthographic, or dimetric-isometric projection. All picking math
// (screen ray, plane intersection, world<->screen) runs through these
// same matrices, so editor gizmos agree with the renderer by construction.
//
// Not wired into QuadRenderer yet: scenes still render with the fixed
// ortho path. Adoption (renderer reads the active camera) is tracked
// follow-up work, not part of this change.
#pragma once

#include "camera_math.h"

namespace icg {

struct Ray {
    Vec3 origin;
    Vec3 direction; // normalized
};

class Camera {
public:
    enum class Projection {
        Perspective,
        Orthographic,
        Isometric, // dimetric preset: yaw -45, pitch -35.264
    };

    // 2D factory: pixel-exact QuadRenderer mapping (origin top-left).
    static Camera Make2D(float width, float height);
    // 3D orbit factory: eye looks at target from distance.
    static Camera Make3D(const Vec3& target, float distance, float fovYDegrees);

    void SetProjection(Projection projection);
    Projection GetProjection() const { return projection_; }
    bool Is2D() const { return is2D_; }
    void Set2D(bool enabled) { is2D_ = enabled; }

    // 2D: pan offset in pixels + zoom factor (1 = native pixels).
    void SetPan(float x, float y);
    void SetZoom(float zoom);
    Vec3 GetPan() const { return pan_; }
    float GetZoom() const { return zoom_; }

    // 3D orbit state (degrees). Isometric preset overwrites these.
    void SetTarget(const Vec3& target);
    void SetDistance(float distance);
    void SetYawPitch(float yawDegrees, float pitchDegrees);
    void SetFovY(float fovYDegrees);
    void SetNearFar(float nearPlane, float farPlane);
    void SetOrthoHeight(float height);
    void ApplyIsometricPreset();

    Mat4 GetViewMatrix() const;
    // Aspect = viewportWidth / viewportHeight. For 2D the stored pixel
    // size is used instead (projection already carries it).
    Mat4 GetProjectionMatrix(float aspect) const;

    Ray ScreenPointToRay(float pixelX, float pixelY, float viewportWidth,
                         float viewportHeight) const;
    Vec3 WorldToScreen(const Vec3& world, float viewportWidth,
                       float viewportHeight) const; // pixels, y down
    static bool IntersectRayPlane(const Ray& ray, const Vec3& planePoint,
                                  const Vec3& planeNormal, float& outT);

private:
    Projection projection_ = Projection::Orthographic;
    bool is2D_ = true;
    // 2D state.
    Vec3 pan_ = {0.0f, 0.0f, 0.0f};
    float zoom_ = 1.0f;
    float orthoWidth_ = 1280.0f;
    float orthoHeight_ = 720.0f;
    // 3D state.
    Vec3 target_ = {0.0f, 0.0f, 0.0f};
    float distance_ = 10.0f;
    float yawDegrees_ = -45.0f;
    float pitchDegrees_ = 35.264f;
    float fovYDegrees_ = 60.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 1000.0f;
    float ortho3DHeight_ = 10.0f;
};

} // namespace icg
