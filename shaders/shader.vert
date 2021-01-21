#version 450

#extension GL_EXT_debug_printf : enable

#define DRAW_PARTICLE 0
#define DRAW_OTHER 1

layout(push_constant) uniform DrawType { int type; }
drawType;

struct ParticleRecord {
  vec4 position;
  vec4 velocity;
  vec4 previousVelocity;
  float massDensity;
  float pressure;
  int gridID;
  float dummy;
};

layout(std430, binding = 2) buffer positionBuffer { ParticleRecord particleRecords[]; };

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 3) out vec4 fragColor;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 outPosition;

void main() {
  switch (drawType.type) {
    case DRAW_PARTICLE:
      gl_Position = ubo.proj * ubo.view * ubo.model
          * (vec4(inPosition * 0.022 * 0.5, 1.0) + particleRecords[gl_InstanceIndex].position);
      fragColor = vec4(inColor, 0.1f);
      break;
    case DRAW_OTHER:
      debugPrintfEXT("%d", gl_VertexIndex);
      gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
      fragColor = vec4(inColor, 1.0f);
      break;
  }
  outPosition = gl_Position.xyz;
  outNormal = inNormal;
}