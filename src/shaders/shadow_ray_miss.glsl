#version 460
#extension GL_NV_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_SHADOW_RAY) rayPayloadInNV ShadowRayPayload ShadowRay;

void main() {
    ShadowRay.distance = -1.0f;
}
