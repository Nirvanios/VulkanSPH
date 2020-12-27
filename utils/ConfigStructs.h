//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_CONFIGSTRUCTS_H
#define VULKANAPP_CONFIGSTRUCTS_H

#include "glm/glm.hpp"
#include <string>

struct ShadersConfig {
  std::string vertex;
  std::string fragemnt;
  std::string computeMassDensity;
  std::string computeForces;
};

struct WindowConfig {
  std::string name;
  int width;
  int height;
};

struct VulkanConfig {
  WindowConfig window;
  ShadersConfig shaders;
};

struct SimulationConfig {
  float timeStep;
  float viscosityCoefficient;
  float gasStiffness;
  float fluidVolume;
  float fluidDensity;
  int particleCount;
  glm::ivec3 particleSize;
  std::string particleModel;
  glm::vec3 gridOrigin;
  glm::ivec3 gridSize;
};

struct AppConfig {
  bool DEBUG;
  bool outputToFile;
  SimulationConfig simulation;
};

#endif//VULKANAPP_CONFIGSTRUCTS_H
