#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadInNVX RayPayload PrimaryRay;

void main() {
    const vec3 backgroundColor = vec3(0.412f, 0.796f, 1.0f);
    PrimaryRay.colorAndDist = vec4(backgroundColor, -1.0f);
    PrimaryRay.normalAndObjId = vec4(0.0f);
}
