#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 color;

layout(std140, set = 0, binding = 0) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    color = inColor;
}