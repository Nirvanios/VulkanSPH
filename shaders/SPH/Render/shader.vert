#version 460

#extension GL_EXT_debug_printf : enable

layout(binding = 0) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPosition;
layout(location = 3) out uint outGridID;



struct SimulationInfoSPH {
  ivec4 gridSizeXYZcountW;
  vec4 GridOrigin;
  vec4 gravityForce;
  float particleMass;
  float restDensity;
  float viscosityCoefficient;
  float gasStiffnessConstant;
  float heatConductivity;
  float heatCapacity;
  float timeStep;
  float supportRadius;
  float tensionThreshold;
  float tensionCoefficient;
  uint particleCount;
};

struct GridInfoMC {
  ivec4 gridSize;
  vec4 gridOrigin;
  float cellSize;
  int detail;
};

layout(push_constant) uniform MarchingCubesInfo {
  SimulationInfoSPH simulationInfoSph;
  GridInfoMC gridInfoMC;
};

void main() {
  const uint myId = gl_InstanceIndex;
  //debugPrintfEXT("myId: %d", myId);
  const ivec3 myId3D =
      ivec3(myId % gridInfoMC.gridSize.x,
            uint(myId % (gridInfoMC.gridSize.x * gridInfoMC.gridSize.y) / gridInfoMC.gridSize.x),
            uint(myId / (gridInfoMC.gridSize.x * gridInfoMC.gridSize.y)));
  const vec3 myPosition = (myId3D - vec3(1 * gridInfoMC.detail)) * gridInfoMC.cellSize;

  gl_Position = /* ubo.proj * ubo.view * ubo.model * */ vec4(myPosition, 1.0);

  outColor = vec4(inColor, 1.0f);
  outPosition = gl_Position;
  outNormal = vec4(inNormal, 1.0f);
  outGridID = gl_InstanceIndex;
}