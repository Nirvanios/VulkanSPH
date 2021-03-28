#version 450

#extension GL_EXT_debug_printf : enable

#define DRAW_PARTICLE 0
#define DRAW_OTHER 1

#define TEXTURE_VIZUALIZE_NONE 0
#define TEXTURE_VIZUALIZE_MASSDENSITY 1
#define TEXTURE_VIZUALIZE_PRESSUREFORCE 2
#define TEXTURE_VIZUALIZE_VELOCITY 3

layout(push_constant) uniform DrawType {
  int drawType;
  int visualizationType;
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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 3) out vec4 fragColor;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec3 outPosition;

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
      gl_Position = ubo.proj * ubo.view * ubo.model
          * (vec4(inPosition * 0.022 * 0.5, 1.0) + particleRecords[gl_InstanceIndex].position);
      switch (drawType.visualizationType) {
        case TEXTURE_VIZUALIZE_NONE: fragColor = vec4(inColor, 1.0f); break;
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
              hsv2rgb(vec3(map2hue(length(particleRecords[gl_InstanceIndex].velocity), 1300, 700),
                           1.0, 1.0)),
              1.0f);
          break;
      }
      break;
    case DRAW_OTHER:
      gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
      fragColor = vec4(inColor, 1.0f);
      break;
  }
  outPosition = gl_Position.xyz;
  outNormal = inNormal;
}