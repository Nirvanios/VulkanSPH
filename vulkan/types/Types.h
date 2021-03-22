//
// Created by Igor Frank on 25.10.20.
//

#ifndef VULKANAPP_TYPES_H
#define VULKANAPP_TYPES_H

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include <bitset>
#include <fmt/format.h>
#include <spdlog/fmt/ostr.h>
#include <vulkan/vulkan.hpp>
#include <magic_enum.hpp>

struct KeyValue {
  int key = 0;  //Particle ID
  int value = 0;//Cell ID

  template<typename OStream>
  friend OStream &operator<<(OStream &os, const KeyValue &in) {
    os << in.value;
    return os;
  }
};

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color{1.0,0.0,0.0};
  glm::vec3 normal{1.0f};

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attribute{
        vk::VertexInputAttributeDescription{.location = 0,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32B32Sfloat,
                                            .offset = offsetof(Vertex, pos)},
        vk::VertexInputAttributeDescription{.location = 1,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32B32Sfloat,
                                            .offset = offsetof(Vertex, color)},
        vk::VertexInputAttributeDescription{.location = 2,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32B32Sfloat,
                                            .offset = offsetof(Vertex, normal)}};
    return attribute;
  }
  bool operator==(const Vertex &other) const {
    return pos == other.pos && color == other.color && other.normal == normal;
  }
};

namespace std {
template<>
struct hash<Vertex> {
  size_t operator()(Vertex const &vertex) const {
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1))
            >> 1);// NOLINT(hicpp-signed-bitwise)
  }
};
}// namespace std

struct Model {
  std::string name = "";
  std::vector<Vertex> vertices;
  std::vector<uint16_t> indices;
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct alignas(16) ParticleRecord {
  glm::vec4 position;
  glm::vec4 velocity;
  glm::vec4 currentVelocity;
  glm::vec4 massDensityCenter;
  float massDensity;
  float pressure;
  float temperature;
  int gridID;
  int dummy;
  float surfaceArea;
  float weightingKernelFraction;

  template<typename OStream>
  friend OStream &operator<<(OStream &os, const ParticleRecord &particleRecord) {
    os << fmt::format("Position: [x:{},y:{},z:{}]", particleRecord.position.x,
                      particleRecord.position.y, particleRecord.position.z);
    os << fmt::format(" Velocity: [x:{},y:{},z:{}]", particleRecord.velocity.x,
                      particleRecord.velocity.y, particleRecord.velocity.z);
    os << fmt::format(" CurrentVelocity: [x:{},y:{},z:{}]", particleRecord.currentVelocity.x,
                      particleRecord.currentVelocity.y, particleRecord.currentVelocity.z);
    os << fmt::format(" MassDensity: {}", particleRecord.massDensity);
    os << fmt::format(" Pressure: {}", particleRecord.pressure);
    return os;
  }
};

struct SimulationInfoSPH {
  glm::ivec4 gridSize;
  glm::vec4 gridOrigin;
  glm::vec4 gravityForce;
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
  unsigned int particleCount;
  //unsigned int cellCount;
};

struct SimulationInfoGridFluid {
  glm::ivec4 gridSize;
  glm::vec4 gridOrigin;
  float timeStep;
  int cellCount;
  float cellSize;
  float diffusionCoefficient;
  int boundaryScale;
  unsigned int specificInfo;
};

struct GridInfo{
  glm::ivec4 gridSize;
  glm::vec4 gridOrigin;
  float cellSize;
  unsigned int particleCount;
};

struct DrawInfo{
  int drawType;
  int visualization;
};

enum class BufferType { floatType = 0, vec4Type = 1};

enum class GaussSeidelColorPhase { red = 0, black = 1 };

enum class GaussSeidelStageType { diffuse = 0, project = 1 };

enum class SimulationType {SPH = 0, Grid = 1, Combined = 2};

class GaussSeidelFlags{
 public:
  GaussSeidelFlags &setColor(GaussSeidelColorPhase color) {flags.set(0, magic_enum::enum_integer(color)); return *this;}
  GaussSeidelFlags &setStageType(GaussSeidelStageType stageType){flags.set(1, magic_enum::enum_integer(stageType)); return *this;};

  GaussSeidelColorPhase getColor(){return magic_enum::enum_value<GaussSeidelColorPhase>(flags.test(0));};
  GaussSeidelStageType getStage(){return magic_enum::enum_value<GaussSeidelStageType>(flags.test(1));};

  explicit operator unsigned int() const {return static_cast<unsigned int>(flags.to_ulong());};
 private:
  std::bitset<2> flags{};
};

#endif//VULKANAPP_TYPES_H
