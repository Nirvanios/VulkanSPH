//
// Created by Igor Frank on 07.10.19.
//

#ifndef VULKANTEST_UTILITIES_H
#define VULKANTEST_UTILITIES_H

#include "../vulkan/types/Types.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <tiny_obj_loader.h>
#include <vector>
#include <numbers>

namespace Utilities {

template<typename T>
concept Pointer = std::is_pointer_v<T>;

template<typename T>
concept RawDataProvider = requires(T t) {
  { t.data() } -> Pointer;
  { t.size() } -> std::convertible_to<std::size_t>;
};

template<Pointer T>
using ptr_val = decltype(*std::declval<T>());

inline std::string readFile(const std::filesystem::path &filename) {
  if (!exists(filename)) {
    throw std::runtime_error(fmt::format("File does not exist! {}", filename));
  }
  if (!is_regular_file(filename)) {
    throw std::runtime_error(fmt::format("Provided path is not a file! {}", filename));
  }

  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error(fmt::format("Could not open exist! {}", filename));
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::string buffer(fileSize, ' ');

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

inline void writeFile(const std::filesystem::path &filename, const std::string_view data) {
  std::ofstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) { throw std::runtime_error("failed to open file!"); }

  file.write(data.data(), data.size());

  file.close();
}

inline Model loadModelFromObj(const std::string &modelPath,
                              const glm::vec3 &color = glm::vec3{0.8f}) {

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;
  Model model;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
    throw std::runtime_error(warn + err);
  }

  for (const auto &shape : shapes) {
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    model.name += shape.name;
    if (&shape != &shapes.back()) { model.name += " + "; }
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};

      vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};
      /* Not needed for now
                if (!attrib.texcoords.empty())
                    vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
*/
      vertex.color = color;

      if (uniqueVertices.count(vertex) == 0) {
        vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
        uniqueVertices[vertex] = static_cast<uint32_t>(model.vertices.size());
        model.vertices.emplace_back(vertex);

      } else {
        vertex.normal = {attrib.normals[3 * index.normal_index + 0],
                         attrib.normals[3 * index.normal_index + 1],
                         attrib.normals[3 * index.normal_index + 2]};
        model.vertices[uniqueVertices[vertex]].normal += vertex.normal;
      }

      model.indices.emplace_back(uniqueVertices[vertex]);
    }
  }
  return model;
}

template<typename T, typename Container = std::vector<T>>
inline bool isIn(T value, Container &&container) {
  return std::any_of(container.begin(), container.end(),
                     [value](const auto &a) { return value == a; });
}

class ValuesPool {
 public:
  explicit ValuesPool(unsigned int size) {
    for (unsigned int i = 0; i < size; ++i) { values.emplace(i); }
  }
  [[nodiscard]] int getNextID() {
    auto id = *values.begin();
    values.erase(id);
    return id;
  }
  void returnID(int id) { values.insert(id); };

 private:
  std::set<int> values;
};

struct IdGenerator {
  std::size_t getNextID() { return id++; }
  std::size_t id = 0;
};

template<typename T>
int getNextPow2Number(T number) {
  return std::pow(2, std::ceil(std::log2(number)));
}

template<typename T>
requires std::is_enum_v<T>
class Flags {
  using underlying_type_t = typename std::underlying_type<T>::type;

 private:
  underlying_type_t flags = 0;

 public:
  Flags(const std::vector<T> &flagBits) {
    for (const auto &item : flagBits) { *this | item; }
  }
  Flags(){};
  void clear() { flags = 0; }
  bool has(T flagBit) const { return (flags & magic_enum::enum_integer(flagBit)) != 0; };
  bool hasAnyOf(const std::vector<T> &flagBit) const {
    for (const auto &item : flagBit)
      if ((flags & magic_enum::enum_integer(item)) != 0) { return true; }
    return false;
  };
  bool hasAllOf(const std::vector<T> &flagBit) const {
    for (const auto &item : flagBit)
      if ((flags & magic_enum::enum_integer(item)) == 0) { return false; }
    return true;
  };
  bool hasAny() const { return flags > 0; }

  Flags operator&(const Flags &other) {
    flags &= other.flags;
    return *this;
  }
  Flags operator&=(const Flags &other) {
    flags &= other.flags;
    return *this;
  }
  Flags operator&(const T &other) {
    flags &= magic_enum::enum_integer(other);
    return *this;
  }
  Flags operator&=(const T &other) {
    flags &= magic_enum::enum_integer(other);
    return *this;
  }
  Flags operator|(const Flags &other) {
    flags |= other.flags;
    return *this;
  }
  Flags operator|=(const Flags &other) {
    flags |= other.flags;
    return *this;
  }
  Flags operator|(const T &other) {
    flags |= magic_enum::enum_integer(other);
    return *this;
  }
  Flags operator|=(const T &other) {
    flags |= magic_enum::enum_integer(other);
    return *this;
  }
};

inline std::vector<ParticleRecord> generateParticles(float fluidVolume, int particleCount,
                                                     glm::ivec3 particleModelSize,
                                                     float temperature, glm::vec3 modelOrigin, int n = 20) {
  auto particleSize = glm::vec3(std::cbrt(fluidVolume / static_cast<float>(particleCount)));
  particleSize =
      glm::vec3(1.1 * std::cbrt((3 * fluidVolume * n) / (4 * std::numbers::pi * particleCount)))
      / 2.0f;
  std::vector<ParticleRecord> data{static_cast<size_t>(particleCount)};

  for (int z = 0; z < particleModelSize.z; ++z) {
    for (int y = 0; y < particleModelSize.y; ++y) {
      for (int x = 0; x < particleModelSize.x; ++x) {
        const auto id = (z * particleModelSize.y * particleModelSize.x) + (y * particleModelSize.x) + x;
        data[id].position = glm::vec4{0.01, 0.01f, 0.01, 0} + glm::vec4{modelOrigin, 0.0}
            + (glm::vec4{x, y, z, 0.0f} * glm::vec4(particleSize, 0.0f));
        data[id].currentVelocity = glm::vec4{0.0f};
        data[id].velocity = glm::vec4{0.0f};
        data[id].massDensity = -1.0f;
        data[id].pressure = -1.0f;
        data[id].temperature = temperature;
        data[id].surfaceArea = 0.0f;
        data[id].weight = particleModelSize.x < 30 ? 0.f : 1.f;
      }
    }
  }
  return data;
}

template <typename T>
std::vector<T> loadDataFromFile(const std::filesystem::path &path){
    auto fileSize = std::filesystem::file_size(path);
    auto data = std::vector<T>{fileSize / sizeof(T)};
    std::ifstream file(path, std::ios::binary);

    file.read(reinterpret_cast<char*>(data.data()), fileSize);

    return data;
}

template <typename T>
void saveDataToFile(const std::filesystem::path &path, std::vector<T> &data){
  std::ofstream file(path, std::ios::binary | std::ios::trunc);

  file.write(reinterpret_cast<char*>(data.data()), data.size() * sizeof(T));
}

}// namespace Utilities

#endif//VULKANTEST_UTILITIES_H
