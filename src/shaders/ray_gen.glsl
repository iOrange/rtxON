#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = SWS_SCENE_AS_SET,     binding = SWS_SCENE_AS_BINDING)            uniform accelerationStructureNVX Scene;
layout(set = SWS_RESULT_IMAGE_SET, binding = SWS_RESULT_IMAGE_BINDING, rgba8) uniform image2D ResultImage;

layout(set = SWS_CAMDATA_SET,      binding = SWS_CAMDATA_BINDING, std140)     uniform AppData {
    UniformParams Params;
};

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadNVX RayPayload PrimaryRay;
layout(location = SWS_LOC_SHADOW_RAY)  rayPayloadNVX ShadowRayPayload ShadowRay;


vec3 CalcRayDir(vec2 screenUV, float aspect) {
    vec3 u = Params.camSide.xyz;
    vec3 v = Params.camUp.xyz;

    const float planeWidth = tan(Params.camNearFarFov.z * 0.5f);

    u *= (planeWidth * aspect);
    v *= planeWidth;

    const vec3 rayDir = normalize(Params.camDir.xyz + (u * screenUV.x) - (v * screenUV.y));
    return rayDir;
}

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDNVX.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeNVX.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeNVX.x) / float(gl_LaunchSizeNVX.y);

    vec3 origin = Params.camPos.xyz;
    vec3 direction = CalcRayDir(uv, aspect);

    const uint rayFlags = gl_RayFlagsOpaqueNVX;
    const uint cullMask = 0xFF;

    const uint primaryRecordOffset = 0;
    const uint stbRecordStride = 0;
    const uint primaryMissIndex = 0;

    const uint shadowRayFlags = gl_RayFlagsOpaqueNVX | gl_RayFlagsTerminateOnFirstHitNVX;
    const uint shadowRecordOffset = 1;
    const uint shadowMissIndex = 1;

    const float tmin = 0.0f;
    const float tmax = Params.camNearFarFov.y;

    vec3 finalColor = vec3(0.0f);

    for (int i = 0; i < SWS_MAX_RECURSION; ++i) {
        traceNVX(Scene,
                 rayFlags,
                 cullMask,
                 primaryRecordOffset,
                 stbRecordStride,
                 primaryMissIndex,
                 origin,
                 tmin,
                 direction,
                 tmax,
                 SWS_LOC_PRIMARY_RAY);

        const vec3 hitColor = PrimaryRay.colorAndDist.rgb;
        const float hitDistance = PrimaryRay.colorAndDist.w;

        // if hit background - quit
        if (hitDistance < 0.0f) {
            finalColor += hitColor;
            break;
        } else {
            const vec3 hitNormal = PrimaryRay.normal.xyz;
            const float isTeapot = PrimaryRay.normal.w;

            const vec3 hitPos = origin + direction * hitDistance;

            if (isTeapot > 0.0f) {
                // our teapot is mirror, so continue

                origin = hitPos + hitNormal * 0.01f;
                direction = reflect(direction, hitNormal);
            } else {
                // we hit diffuse primitive - simple lambertian

                const vec3 toLight = normalize(Params.sunPosAndAmbient.xyz);
                const vec3 shadowRayOrigin = hitPos + hitNormal * 0.01f;

                traceNVX(Scene,
                         rayFlags,
                         cullMask,
                         shadowRecordOffset,
                         stbRecordStride,
                         shadowMissIndex,
                         shadowRayOrigin,
                         0.0f,
                         toLight,
                         tmax,
                         SWS_LOC_SHADOW_RAY);

                const float lighting = (ShadowRay.distance > 0.0f) ? Params.sunPosAndAmbient.w : max(Params.sunPosAndAmbient.w, dot(hitNormal, toLight));

                finalColor += hitColor * lighting;

                break;
            }
        }
    }

    imageStore(ResultImage, ivec2(gl_LaunchIDNVX.xy), vec4(LinearToSrgb(finalColor), 1.0f));
}
