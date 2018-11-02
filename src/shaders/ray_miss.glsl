#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 ResultColor;

void main() {
    ResultColor = vec3(0.412f, 0.796f, 1.0f);
}
