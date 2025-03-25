#version 450

layout(location = 0) in vec2 inVtx;
layout(location = 1) in vec3 inData;

void main() {
    gl_Position = vec4((inData.z * inVtx)+inData.xy, 0.0, 1.0);
}
