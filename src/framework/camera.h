#pragma once
#include "common.h"

class Camera {
public:
    Camera();
    ~Camera();

    void        SetViewport(const Recti& viewport);
    void        SetFovY(const float fovy);
    void        SetViewPlanes(const float nearZ, const float farZ);
    void        SetPosition(const vec3& pos);

    void        LookAt(const vec3& pos, const vec3& target);
    void        Move(const float side, const float direction);
    void        Rotate(const float angleX, const float angleY);

    float       GetNearPlane() const;
    float       GetFarPlane() const;
    float       GetFovY() const;

    const mat4& GetProjection() const;
    const mat4& GetTransform() const;

    const vec3& GetPosition() const;
    const vec3& GetDirection() const;
    const vec3  GetUp() const;
    const vec3  GetSide() const;

private:
    void        MakeProjection();
    void        MakeTransform();

private:
    Recti   mViewport;
    float   mFovY;
    float   mNearZ;
    float   mFarZ;
    vec3    mPosition;
    vec3    mDirection;
    quat    mRotation;
    mat4    mProjection;
    mat4    mTransform;
};
