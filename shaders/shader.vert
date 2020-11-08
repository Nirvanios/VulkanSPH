#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 3) out vec3 fragColor;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 outPosition;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition + vec3(gl_InstanceIndex*2, 0, 0), 1.0);
    outPosition = gl_Position.xyz;
    fragColor = inColor;
    outNormal = inNormal;
}