#version 450

#extension GL_EXT_debug_printf : enable

#define TAG_CELL_TYPE_BIT 0
#define TAG_IS_INTERFACE_BIT 1

#define CELL_TYPE_FLUID 1
#define CELL_TYPE_AIR 0

#define IS_INTERFACE 1
#define IS_NOT_INTERFACE 0

layout(push_constant) uniform GridSimulationInfoUniform {
  ivec4 gridSize;
  vec4 gridOrigin;
  float timeStep;
  int cellCount;
  float cellSize;
  float diffusionCoefficient;
  int boundaryScale;
  uint specificInfo;
  float heatConductivity;
  float heatCapacity;
  float specificGasConstant;
}
simulationInfo;

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

struct CellInfo {
  uint tags;
  int indexes;
};

layout(std430, binding = 2) buffer densityBuffer { vec2 density[]; };
layout(std430, binding = 3) buffer Indexes { CellInfo cellInfos[]; };

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

ivec3 getPosition(int index) {
  int z = index / (simulationInfo.gridSize.x * simulationInfo.gridSize.y);
  int zOffset = z * simulationInfo.gridSize.x * simulationInfo.gridSize.y;
  return ivec3((index - zOffset) % simulationInfo.gridSize.x,
               (index - zOffset) / simulationInfo.gridSize.x, z);
}

const float cellScale = 0.022 * 0.5;

void main() {
  uint cellType = bitfieldExtract(cellInfos[gl_InstanceIndex].tags, TAG_CELL_TYPE_BIT, 1);
  const ivec3 gridSizeWithBorders = simulationInfo.gridSize.xyz + ivec3(2);
  const ivec3 myId3D = getPosition(gl_InstanceIndex);
  const uint myId = (myId3D.x + 1) + (myId3D.y + 1) * gridSizeWithBorders.x
      + (myId3D.z + 1) * gridSizeWithBorders.x * gridSizeWithBorders.y;
  

  gl_Position = ubo.proj * ubo.view * ubo.model
      * (vec4(inPosition * 0.5 * simulationInfo.cellSize, 1.0)
         + vec4(myId3D * simulationInfo.cellSize, 0) + vec4(vec3(0.5 * simulationInfo.cellSize), 0));
  gl_Position *= int(density[myId].x != 0);
  fragColor = vec4(inColor, (density[myId].x) * 0.0 /* * int(cellType == CELL_TYPE_AIR) */);
  outPosition = gl_Position.xyz;
  outNormal = inNormal;
}