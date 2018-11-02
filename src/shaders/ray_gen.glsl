#version 460
#extension GL_NVX_raytracing : require

layout(set = 0, binding = 0) uniform accelerationStructureNVX Scene;
layout(set = 0, binding = 1, rgba8) uniform image2D ResultImage;

layout(location = 0) rayPayloadNVX vec3 ResultColor;

void main() {
    const vec2 uv = vec2(gl_LaunchIDNVX.xy) / vec2(gl_LaunchSizeNVX.xy - 1);

    const vec3 origin = vec3(uv.x, 1.0f - uv.y, -1.0f);
    const vec3 direction = vec3(0.0f, 0.0f, 1.0f);

    const uint rayFlags = gl_RayFlagsNoneNVX;
    const uint cullMask = 0xFF;
    const uint sbtRecordOffset = 0;
    const uint sbtRecordStride = 0;
    const uint missIndex = 0;
    const float tmin = 0.0f;
    const float tmax = 10.0f;
    const int payloadLocation = 0;

    traceNVX(Scene,
             rayFlags,
             cullMask,
             sbtRecordOffset,
             sbtRecordStride,
             missIndex,
             origin,
             tmin,
             direction,
             tmax,
             payloadLocation);

    imageStore(ResultImage, ivec2(gl_LaunchIDNVX.xy), vec4(ResultColor, 1.0f));
}
