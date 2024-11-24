#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 color;
layout(location = 1) out vec2 uv;

layout(std140, set = 0, binding = 0) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

void main() {
    gl_Position = camera.proj * camera.view * camera.model * vec4(inPosition, 1.0);
    uv = inUV;
    color = inColor;
}