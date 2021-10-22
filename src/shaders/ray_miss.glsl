#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(set = SWS_ENVS_SET, binding = 0) uniform sampler2D EnvTexture;

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadInEXT RayPayload PrimaryRay;

const float MY_PI = 3.1415926535897932384626433832795;
const float MY_INV_PI  = 1.0 / MY_PI;

vec2 DirToLatLong(vec3 dir) {
    float phi = atan(dir.x, dir.z);
    float theta = acos(dir.y);

    return vec2((MY_PI + phi) * (0.5 / MY_PI), theta * MY_INV_PI);
}

void main() {
    vec2 uv = DirToLatLong(gl_WorldRayDirectionEXT);
    vec3 envColor = textureLod(EnvTexture, uv, 0.0).rgb;
    PrimaryRay.colorAndDist = vec4(envColor, -1.0);
    PrimaryRay.normalAndObjId = vec4(0.0);
}
