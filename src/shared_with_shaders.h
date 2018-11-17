#ifndef SHARED_WITH_SHADERS_H
#define SHARED_WITH_SHADERS_H

#ifdef __cplusplus
// include vec & mat types (same namings as in GLSL)
#include "framework/common.h"
#endif // __cplusplus

//
#define SWS_PRIMARY_HIT_SHADERS_IDX     0
#define SWS_PRIMARY_MISS_SHADERS_IDX    0
#define SWS_SHADOW_HIT_SHADERS_IDX      1
#define SWS_SHADOW_MISS_SHADERS_IDX     1

// resource locations
#define SWS_SCENE_AS_SET                0
#define SWS_SCENE_AS_BINDING            0
#define SWS_RESULT_IMAGE_SET            0
#define SWS_RESULT_IMAGE_BINDING        1
#define SWS_CAMDATA_SET                 0
#define SWS_CAMDATA_BINDING             2

#define SWS_MATIDS_SET                  1
#define SWS_ATTRIBS_SET                 2
#define SWS_FACES_SET                   3
#define SWS_TEXTURES_SET                4

#define SWS_NUM_SETS                    5

// cross-shader locations
#define SWS_LOC_PRIMARY_RAY             0
#define SWS_LOC_HIT_ATTRIBS             1
#define SWS_LOC_SHADOW_RAY              2

#define SWS_MAX_RECURSION               10

#define OBJECT_ID_BUNNY                 0.0f
#define OBJECT_ID_PLANE                 1.0f
#define OBJECT_ID_TEAPOT                2.0f


struct RayPayload {
    vec4 colorAndDist;
    vec4 normalAndObjId;
};

struct ShadowRayPayload {
    float distance;
};

struct VertexAttribute {
    vec4 normal;
    vec4 uv;
};

// packed std140
struct UniformParams {
    // Lighting
    vec4 sunPosAndAmbient;

    // Camera
    vec4 camPos;
    vec4 camDir;
    vec4 camUp;
    vec4 camSide;
    vec4 camNearFarFov;
};


// shaders helper functions
vec2 BaryLerp(vec2 a, vec2 b, vec2 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

vec3 BaryLerp(vec3 a, vec3 b, vec3 c, vec3 barycentrics) {
    return a * barycentrics.x + b * barycentrics.y + c * barycentrics.z;
}

float LinearToSrgb(float channel) {
    if (channel <= 0.0031308f) {
        return 12.92f * channel;
    } else {
        return 1.055f * pow(channel, 1.0f / 2.4f) - 0.055f;
    }
}

vec3 LinearToSrgb(vec3 linear) {
    return vec3(LinearToSrgb(linear.r), LinearToSrgb(linear.g), LinearToSrgb(linear.b));
}

#endif // SHARED_WITH_SHADERS_H
