#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../shared_with_shaders.h"

layout(location = SWS_LOC_SHADOW_RAY) rayPayloadInEXT ShadowRayPayload ShadowRay;

void main() {
    ShadowRay.distance = -1.0f;
}
