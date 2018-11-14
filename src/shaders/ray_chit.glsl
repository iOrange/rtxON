#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../shared_with_shaders.h"

layout(set = SWS_MATIDS_SET, binding = 0, std430) readonly buffer MatIDsBuffer {
    uint MatIDs[];
} MatIDsArray[];

layout(set = SWS_ATTRIBS_SET, binding = 0, std430) readonly buffer AttribsBuffer {
    VertexAttribute VertexAttribs[];
} AttribsArray[];

layout(set = SWS_FACES_SET, binding = 0, std430) readonly buffer FacesBuffer {
    uvec4 Faces[];
} FacesArray[];

layout(set = SWS_TEXTURES_SET, binding = 0) uniform sampler2D TexturesArray[];

layout(location = SWS_LOC_PRIMARY_RAY) rayPayloadInNVX RayPayload PrimaryRay;
layout(location = SWS_LOC_HIT_ATTRIBS) hitAttributeNVX vec2 HitAttribs;

void main() {
    const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

    const uint matID = MatIDsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].MatIDs[gl_PrimitiveID];

    const uvec4 face = FacesArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].Faces[gl_PrimitiveID];

    VertexAttribute v0 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].VertexAttribs[int(face.x)];
    VertexAttribute v1 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].VertexAttribs[int(face.y)];
    VertexAttribute v2 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].VertexAttribs[int(face.z)];

    // interpolate our vertex attribs
    const vec3 normal = normalize(BaryLerp(v0.normal.xyz, v1.normal.xyz, v2.normal.xyz, barycentrics));
    const vec2 uv = BaryLerp(v0.uv.xy, v1.uv.xy, v2.uv.xy, barycentrics);

    const vec3 texel = textureLod(TexturesArray[nonuniformEXT(matID)], uv, 0.0f).rgb;

    const float objId = float(gl_InstanceCustomIndexNVX);

    PrimaryRay.colorAndDist = vec4(texel, gl_HitTNVX);
    PrimaryRay.normalAndObjId = vec4(normal, objId);
}
