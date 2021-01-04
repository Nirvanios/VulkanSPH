//
// Created by Igor Frank on 07.10.19.
//

#ifndef VULKANTEST_UTILITIES_H
#define VULKANTEST_UTILITIES_H

#include "../vulkan/types/Types.h"
#include <fstream>
#include <set>
#include <string>
#include <tiny_obj_loader.h>
#include <vector>

namespace Utilities {
inline std::string readFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) { throw std::runtime_error("failed to open file!"); }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::string buffer(fileSize, ' ');

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
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

template <typename T>
int getNextPow2Number(T number){
  return std::pow(2, std::ceil(std::log2(number)));
}
}// namespace Utilities

#endif//VULKANTEST_UTILITIES_H
