#version 460

#extension GL_EXT_debug_printf : enable

#define DRAW_PARTICLE 1
#define DRAW_GRID 2

#define TEXTURE_VIZUALIZE_NONE 0
#define TEXTURE_VIZUALIZE_MASSDENSITY 1
#define TEXTURE_VIZUALIZE_PRESSUREFORCE 2
#define TEXTURE_VIZUALIZE_VELOCITY 3
#define TEXTURE_VIZUALIZE_TEMPERATURE 4

layout(push_constant) uniform DrawType {
  int drawType;
  int visualizationType;
  float supportRadius;
}
drawType;

struct ParticleRecord {
  vec4 position;
  vec4 velocity;
  vec4 previousVelocity;
  vec4 massDensityCenter;
  vec4 force;
  float massDensity;
  float pressure;
  float temperature;
  int gridID;
  float pressureForceLength;
  float surfaceArea;
  float weightingKernelFraction;
  float weight;
};

layout(std430, binding = 2) buffer positionBuffer { ParticleRecord particleRecords[]; };

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;
layout(binding = 3) uniform Color {
  vec4 inColor;
};

layout(location = 0) in vec3 inPosition;
//layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outPosition;

float map2hue(float value, float max, float min) {
  if (value > max) return 0.0;
  if (value < min) return 0.666;
  return ((value - min) / (max - min)) * 0.666;
}

vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main() {
  switch (drawType.drawType) {
    case DRAW_PARTICLE:
      const float particleSizeModifier =
          drawType.supportRadius * 0.25 * particleRecords[gl_InstanceIndex].weight;
      gl_Position = ubo.proj * ubo.view * ubo.model
          * (vec4(inPosition * particleSizeModifier, 1.0)
             + particleRecords[gl_InstanceIndex].position + particleSizeModifier);
      switch (drawType.visualizationType) {
        case TEXTURE_VIZUALIZE_NONE: fragColor = inColor; break;
        case TEXTURE_VIZUALIZE_PRESSUREFORCE:
          fragColor =
              vec4(hsv2rgb(vec3(
                       map2hue(particleRecords[gl_InstanceIndex].pressureForceLength, 1300, 700),
                       1.0, 1.0)),
                   1.0f);
          break;
        case TEXTURE_VIZUALIZE_MASSDENSITY:
          fragColor =
              vec4(hsv2rgb(vec3(map2hue(particleRecords[gl_InstanceIndex].massDensity, 1300, 700),
                                1.0, 1.0)),
                   1.0f);
          break;
        case TEXTURE_VIZUALIZE_VELOCITY:
          fragColor = vec4(
              hsv2rgb(vec3(map2hue(length(particleRecords[gl_InstanceIndex].velocity), 20, 0),
                           1.0, 1.0)),
              1.0f);
          break;
        case TEXTURE_VIZUALIZE_TEMPERATURE:
          fragColor = vec4(
              hsv2rgb(vec3(map2hue(length(particleRecords[gl_InstanceIndex].velocity), 100, 0),
                           1.0, 1.0)),
              1.0f);
          break;
      }
      break;
    case DRAW_GRID:
    default:
      gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
      fragColor = inColor;
      break;
  }
  outPosition = gl_Position.xyz;
  outNormal = inNormal;
}