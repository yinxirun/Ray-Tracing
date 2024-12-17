#version 460

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;
layout(location = 0) out vec3 OutColor;
layout(location = 1) out vec2 OutUV;

layout(set = 0, binding = 0) uniform Camera{
    mat4 model;
    mat4 view;
    mat4 proj;
}camera;

void main(){
    gl_Position = camera.proj * camera.view * camera.model * vec4(pos, 1);
    OutColor = color;
    OutUV = uv;
}