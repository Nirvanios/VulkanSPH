#version 460

#extension GL_EXT_debug_printf : enable

#define TO_INDEX_MC(a, b, c)                                                                       \
  (uint(a + (gridInfoMC.gridSize.x + 1) * (b + (gridInfoMC.gridSize.y + 1) * c)))
#define TO_INDEX_SPH(a, b, c)                                                                      \
  (uint(a                                                                                          \
        + (simulationInfoSph.gridSizeXYZcountW.x)                                                  \
            * (b + (simulationInfoSph.gridSizeXYZcountW.y) * c)))

#define VEC_TO_INDEX_MC(a) (TO_INDEX_MC(a.x, a.y, a.z))
#define VEC_TO_INDEX_SPH(a) (TO_INDEX_SPH(a.x, a.y, a.z))

#define TAG_CELL_TYPE_BIT 0
#define TAG_IS_INTERFACE_BIT 1

#define CELL_TYPE_FLUID 1
#define CELL_TYPE_AIR 0

#define IS_INTERFACE 1
#define IS_NOT_INTERFACE 0

layout(points) in;
layout(triangle_strip, max_vertices = 15) out;

layout(location = 0) in vec4 inColor[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec3 inPosition[];
layout(location = 3) in uint inGridID[];

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 outPosition;
layout(location = 3) out uint outGridID;

layout(std430, binding = 1) buffer Colors { float colorField[]; };
layout(std430, binding = 2) buffer EdgeToVertex { uvec2 edgeToVertexLUT[]; };
layout(std430, binding = 3) buffer PolygonCount { uint polygonCountLUT[]; };
layout(std430, binding = 4) buffer Edges { int edgesLUT[]; };

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

struct KeyValue {
  int key;  //Particle ID
  int value;//Cell ID
};

struct CellInfo {
  uint tags;
  int indexes;
};

layout(std430, binding = 5) buffer Indexes { CellInfo cellInfos[]; };
layout(std430, binding = 6) buffer positionBuffer { ParticleRecord particleRecords[]; };
layout(std430, binding = 7) buffer Grid { KeyValue grid[]; };

layout(binding = 9) uniform UniformBufferObject {
  mat4 model;
  mat4 view;
  mat4 proj;
}
ubo;

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
  float threshold;
};

layout(push_constant) uniform MarchingCubesInfo {
  SimulationInfoSPH simulationInfoSph;
  GridInfoMC gridInfoMC;
};

uint verticesIDs[] = {VEC_TO_INDEX_MC(ivec3(0, 0, 1)), VEC_TO_INDEX_MC(ivec3(0, 1, 1)),
                      VEC_TO_INDEX_MC(ivec3(1, 1, 1)), VEC_TO_INDEX_MC(ivec3(1, 0, 1)),
                      VEC_TO_INDEX_MC(ivec3(0, 0, 0)), VEC_TO_INDEX_MC(ivec3(0, 1, 0)),
                      VEC_TO_INDEX_MC(ivec3(1, 1, 0)), VEC_TO_INDEX_MC(ivec3(1, 0, 0))};
uvec3 vertices[] = {uvec3(0, 0, 1), uvec3(0, 1, 1), uvec3(1, 1, 1), uvec3(1, 0, 1),
                    uvec3(0, 0, 0), uvec3(0, 1, 0), uvec3(1, 1, 0), uvec3(1, 0, 0)};

uint idToCheck = -1;

float M_PI = 3.1415;

float defaultKernel(in vec4 position, in float positionNorm, in float supportRadius) {
  if (positionNorm >= 0.0f && positionNorm <= supportRadius)
    return (315 / (64 * M_PI * pow(supportRadius, 9)))
        * pow(pow(supportRadius, 2) - pow(positionNorm, 2), 3);
  else
    return 0.0f;
}

uint getDensity(uint vertexOriginId) {
  uint density = 0;

  for (int i = 7; i >= 0; --i) {
    density = density << 1;
    density += uint(colorField[vertexOriginId + verticesIDs[i]] > gridInfoMC.threshold);
    /*     if (inGridID[0] == idToCheck)
      debugPrintfEXT("vertexOriginId: %u, vertices: %u", inGridID[0],
                     vertexOriginId + verticesIDs[i]); */
  }
  return density;
}

float computeColor(float neighbourWeightedMass, uint neighbourID, vec4 positionDiff) {
  return (neighbourWeightedMass / particleRecords[neighbourID].massDensity)
      * defaultKernel(positionDiff, length(positionDiff.xyz), simulationInfoSph.supportRadius);
}

void main() {
  vec4 worldPolygon[3];
  vec3 normal[3];

  const uint myIdmc = inGridID[0];
  ivec3 myId3Dmc =
      ivec3(myIdmc % gridInfoMC.gridSize.x,
            (myIdmc % (gridInfoMC.gridSize.x * gridInfoMC.gridSize.y) / gridInfoMC.gridSize.x),
            myIdmc / (gridInfoMC.gridSize.x * gridInfoMC.gridSize.y));
  ivec3 myId3Dsph = myId3Dmc / gridInfoMC.detail;
  myId3Dsph -= ivec3(1);

  const uint myIdsph = VEC_TO_INDEX_SPH(myId3Dsph);

  /*   if (myIdsph >= 0) {
    const bool isInterface =
        bool(bitfieldExtract(cellInfos[myIdsph].tags, TAG_IS_INTERFACE_BIT, 1));

    //if(!isInterface) return;
  } */

  uint vertexOriginId = VEC_TO_INDEX_MC(myId3Dmc);

  const uint density = getDensity(vertexOriginId);
  const uint edgesIndex = density * 5 * 3;

  /*   if (myIdmc == idToCheck)
    debugPrintfEXT("myIdmc: %u, vertexOriginId: %u, density: %u", myIdmc, vertexOriginId, density); */

  const uint polyCount = polygonCountLUT[density];
  //if(polyCount > 0) debugPrintfEXT("myIdmc: %u, polyCount: %u", myIdmc, polyCount);

  const vec3 scale = (gridInfoMC.gridSize.xyz - 1) / vec3(gridInfoMC.gridSize.xyz);

  for (uint i = 0; i < polyCount; ++i) {

    const uint polyIndex = 3 * i;
    for (uint j = polyIndex; j < polyIndex + 3; ++j) {
      const int edge = edgesLUT[edgesIndex + j];
      const uvec2 edgeVertices = edgeToVertexLUT[edge];
      const uvec3 halfEdge = uvec3(notEqual(vertices[edgeVertices.x], vertices[edgeVertices.y]));
      const uvec3 fullEdge = uvec3(vertices[edgeVertices.x] * vertices[edgeVertices.y]);
      const float v0 = colorField[vertexOriginId + verticesIDs[edgeVertices.x]] - gridInfoMC.threshold;
      const float v1 = colorField[vertexOriginId + verticesIDs[edgeVertices.y]] - gridInfoMC.threshold;
      const float edgeInterpolation = -v0 / (v1 - v0);
      const vec4 polygon =
          vec4(inPosition[0]*scale + (edgeInterpolation * halfEdge * gridInfoMC.cellSize * scale)
                   + (fullEdge * gridInfoMC.cellSize *scale),
               1);
      const vec4 polygon2 =
          vec4(inPosition[0] + (edgeInterpolation * halfEdge * gridInfoMC.cellSize)
                   + (fullEdge * gridInfoMC.cellSize),
               1);
      worldPolygon[j % 3] = ubo.proj * ubo.view * ubo.model
          * ((polygon + vec4(vec3(0.5 * gridInfoMC.cellSize * scale), 0)));

      /*       if (myIdmc == idToCheck) debugPrintfEXT("myIdmc: %u, edge: %d", myIdmc, edge);
      if (myIdmc == idToCheck)
        debugPrintfEXT("myIdmc: %u, edgeVertices: %v2u", myIdmc, edgeVertices);
      if (myIdmc == idToCheck) debugPrintfEXT("myIdmc: %u, halfEdge: %v3u", myIdmc, halfEdge);
      if (myIdmc == idToCheck) debugPrintfEXT("myIdmc: %u, fullEdge: %v3u", myIdmc, fullEdge);
      if (myIdmc == idToCheck) debugPrintfEXT("myIdmc: %u, v1: %f, v0: %f", myIdmc, v1, v0);
      if (myIdmc == idToCheck)
        debugPrintfEXT("myIdmc: %u, edgeInterpolation: %f", myIdmc, edgeInterpolation);
      if (myIdmc == idToCheck)
        debugPrintfEXT("myIdmc: %u, worldPolygon: %v3f", myIdmc,
                       vec3(inPosition[0] + (edgeInterpolation * halfEdge * gridInfoMC.cellSize)
                            + (fullEdge * gridInfoMC.cellSize))); */

      float d = (simulationInfoSph.supportRadius * gridInfoMC.detail)
          / (gridInfoMC.gridSize.x * gridInfoMC.gridSize.y * gridInfoMC.gridSize.z); 
      d = d  < 0.000003 ? 0.000003 : d;

      vec3 positiveGradient = vec3(0);
      vec3 negativeGradient = vec3(0);
      vec4 myPosition = polygon2;

      for (int z = -1; z < 2; ++z) {
        for (int y = -1; y < 2; ++y) {
          for (int x = -1; x < 2; ++x) {
            ivec3 currentGridID3D = myId3Dsph + ivec3(x, y, z);

            if (all(lessThan(currentGridID3D, simulationInfoSph.gridSizeXYZcountW.xyz))
                && all(greaterThanEqual(currentGridID3D, ivec3(0)))) {
              uint currentGridID = VEC_TO_INDEX_SPH(currentGridID3D);
              int sortedID = cellInfos[currentGridID].indexes;

/*               if (myIdmc == idToCheck)
                debugPrintfEXT("myIdmc: %u, currentGridID: %d, currentGridID3D: %v3d", myIdmc,
                               currentGridID, currentGridID3D); */

              if (sortedID == -1) { continue; }

              while (grid[sortedID].value == currentGridID
                     && sortedID < simulationInfoSph.particleCount) {

                const int neighbourID = grid[sortedID].key;
                if (particleRecords[neighbourID].weight > 0) {

                  const vec4 neighbourPosition = particleRecords[neighbourID].position;
                  vec4 positionDiff = myPosition - neighbourPosition;
/* 
                  if (myIdmc == idToCheck)
                    debugPrintfEXT(
                        "myIdsph: %u, myPosition: %v4f, neighbourPosition: %v4f, positionDiff: %f",
                        myIdmc, myPosition, neighbourPosition, length(positionDiff.xyz)); */
                  if (length(positionDiff.xyz) < simulationInfoSph.supportRadius) {

                    float neighbourWeightedMass =
                        simulationInfoSph.particleMass * particleRecords[neighbourID].weight;

                    positiveGradient.x +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition + vec4(d, 0, 0, 0)) - neighbourPosition);
                    positiveGradient.y +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition + vec4(0, d, 0, 0)) - neighbourPosition);
                    positiveGradient.z +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition + vec4(0, 0, d, 0)) - neighbourPosition);

                    negativeGradient.x +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition - vec4(d, 0, 0, 0)) - neighbourPosition);
                    negativeGradient.y +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition - vec4(0, d, 0, 0)) - neighbourPosition);
                    negativeGradient.z +=
                        computeColor(neighbourWeightedMass, neighbourID,
                                     (myPosition - vec4(0, 0, d, 0)) - neighbourPosition);
                  }
                }
                ++sortedID;
              }
            }
          }
        }
      }
      normal[j % 3] = -normalize(positiveGradient - negativeGradient);
     /*  if (myIdmc == idToCheck)
        debugPrintfEXT("myIdmc: %u, negativeGradient: %v3f", myIdmc, negativeGradient);
      if (myIdmc == idToCheck)
        debugPrintfEXT("myIdmc: %u, positiveGradient: %v3f", myIdmc, positiveGradient);
      if (myIdmc == idToCheck) debugPrintfEXT("myIdmc: %d, normal: %v3f", myIdmc, normal[j % 3]); */
    }

    /*     vec3 normal =
        normalize(cross(worldPolygon[2].xyz - worldPolygon[0].xyz, worldPolygon[1].xyz - worldPolygon[0].xyz)); */

    //if(myIdmc == 0) debugPrintfEXT("myIdmc: %u, norm: %v3f", myIdmc, normal);

    for (int j = 2; j >= 0; --j) {
      outPosition.xyz = worldPolygon[j].xyz;
      gl_Position.xyz = worldPolygon[j].xyz;
      outColor = inColor[0];
      outNormal = normal[j];
      outGridID = inGridID[0];
      EmitVertex();
      //debugPrintfEXT("gridID: %u", myIdmc);
    }

    EndPrimitive();
  }
}
