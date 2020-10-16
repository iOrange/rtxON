#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadInEXT RayPayload PrimaryRay;

void main() {
    const vec3 backgroundColor = vec3(0.412f, 0.796f, 1.0f);
    PrimaryRay.colorAndDist = vec4(backgroundColor, -1.0f);
    PrimaryRay.normalAndObjId = vec4(0.0f);
}
