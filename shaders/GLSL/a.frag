#version 460

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 OutColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main(){
    OutColor = texture(texSampler, uv);
    OutColor = vec4(color, 1);
}

