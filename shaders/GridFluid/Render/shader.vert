#version 460

#extension GL_EXT_debug_printf : enable

#define TAG_CELL_TYPE_BIT 0
#define TAG_IS_INTERFACE_BIT 1

#define CELL_TYPE_FLUID 1
#define CELL_TYPE_AIR 0

#define IS_INTERFACE 1
#define IS_NOT_INTERFACE 0

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_NX 3
#define AXIS_NY 4
#define AXIS_NZ 5

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
  float ambientTemperature;
  float buoyancyAlpha;
  float buoyancyBeta;
}
simulationInfo;

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

layout(binding = 4) uniform Angles {
  float yaw;
  float pitch;
  float roll;
}
angles;

struct CellInfo {
  uint tags;
  int indexes;
};

layout(std430, binding = 2) buffer densityBuffer { vec2 density[]; };
layout(std430, binding = 3) buffer Indexes { CellInfo cellInfos[]; };

layout(location = 0) in vec3 inPosition;
//layout(location = 1) in vec3 inColor;
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

int getAlignedAxis() {
  float pitch = angles.pitch;
  float yaw = mod(angles.yaw, 360);
  /*   if (angles.pitch < 0) { pitch *= -1; }
  if (angles.yaw < 0) { yaw *= -1; } */

  if (pitch > 45) return AXIS_Y;
  if (pitch < -45) return AXIS_NY;
  if (yaw > 45 && yaw < 135) return AXIS_Z;
  if (yaw > 225 && yaw < 315) return AXIS_NZ;
  if (yaw > 315 || yaw < 45) return AXIS_X;
  else
    return AXIS_NX;
}

const float cellScale = 0.022 * 0.5;

void main() {
  vec3 color = vec3(1.0);
  mat4 rotMatrix = mat4(1.0);
  const int axis = getAlignedAxis();
  switch (axis) {
    case AXIS_NX:
      rotMatrix *= mat4(vec4(0, -1, 0, 0), vec4(1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
      break;
    case AXIS_X:
      rotMatrix *= mat4(vec4(0, 1, 0, 0), vec4(-1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
      break;
    case AXIS_Y:
      rotMatrix *= mat4(vec4(1, 0, 0, 0), vec4(0, -1, 0, 0), vec4(0, 0, -1, 0), vec4(0, 0, 0, 1));
      break;
    case AXIS_NY: rotMatrix = mat4(1.0); break;
    case AXIS_NZ:
      rotMatrix *= mat4(vec4(1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, -1, 0, 0), vec4(0, 0, 0, 1));
      break;
    case AXIS_Z:
      rotMatrix *= mat4(vec4(1, 0, 0, 0), vec4(0, 0, -1, 0), vec4(0, 1, 0, 0), vec4(0, 0, 0, 1));
      break;
    default: color = vec3(1); break;
  }

  uint cellType = bitfieldExtract(cellInfos[gl_InstanceIndex].tags, TAG_CELL_TYPE_BIT, 1);
  const ivec3 gridSizeWithBorders = simulationInfo.gridSize.xyz + ivec3(2);
  const ivec3 myId3D = getPosition(gl_InstanceIndex);
  const uint myId = (myId3D.x + 1) + (myId3D.y + 1) * gridSizeWithBorders.x
      + (myId3D.z + 1) * gridSizeWithBorders.x * gridSizeWithBorders.y;

  vec4 position = rotMatrix * (vec4(inPosition * 0.5 * simulationInfo.cellSize, 1.0));

  const ivec3 vertexId3D = myId3D + ivec3(((rotMatrix * vec4(inPosition, 0)).xyz + vec3(1)) * 0.5);
  const uint vertexId = (vertexId3D.x + 1) + (vertexId3D.y + 1) * gridSizeWithBorders.x
      + (vertexId3D.z + 1) * gridSizeWithBorders.x * gridSizeWithBorders.y;

  /*   if(gl_InstanceIndex == 0){
    debugPrintfEXT("myId: %d-%d, vertexId3D: %v3d", gl_InstanceIndex, gl_VertexIndex, vertexId3D);
  } */

  position +=
      vec4(myId3D * simulationInfo.cellSize, 0) + vec4(vec3(0.5 * simulationInfo.cellSize), 0);

  const float volume = pow(simulationInfo.cellSize, 3) * 500;

  gl_Position = ubo.proj * ubo.view * ubo.model * position;
  //gl_Position *= int(density[myId].x != 0);
  fragColor = vec4(color, (density[vertexId].x / volume) * int(cellType == CELL_TYPE_AIR));
  outPosition = gl_Position.xyz;
  outNormal = inNormal;
}