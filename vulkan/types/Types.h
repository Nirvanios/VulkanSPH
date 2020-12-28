//
// Created by Igor Frank on 25.10.20.
//

#ifndef VULKANAPP_TYPES_H
#define VULKANAPP_TYPES_H

#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include <fmt/format.h>
#include <spdlog/fmt/ostr.h>
#include <vulkan/vulkan.hpp>

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
  glm::vec3 color;
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
  float massDensity;
  float pressure;
  int gridID;
  float dummy;

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

struct SimulationInfo {
  glm::vec4 gravityForce;
  float particleMass;
  float restDensity;
  float viscosityCoefficient;
  float gasStiffnessConstant;
  float timeStep;
  float supportRadius;
  float tensionThreshold;
  float tensionCoefficient;
  unsigned int particleCount;
  std::array<int, 27> neighbourOffsets;
};

struct GridInfo{
  glm::ivec4 gridSize;
  glm::vec4 gridOrigin;
  float cellSize;
  unsigned int particleCount;
};

#endif//VULKANAPP_TYPES_H
