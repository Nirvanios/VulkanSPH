//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_CONFIGSTRUCTS_H
#define VULKANAPP_CONFIGSTRUCTS_H

#include "glm/glm.hpp"
#include <string>
#include <filesystem>


struct WindowConfig {
  std::string name;
  int width;
  int height;
};

struct VulkanConfig {
  WindowConfig window;
  std::filesystem::path shaderFolder;
};

struct SimulationSPHConfig {
  float timeStep;
  float viscosityCoefficient;
  float gasStiffness;
  float heatConductivity;
  float heatCapacity;
  float fluidVolume;
  float fluidDensity;
  int particleCount;
  glm::ivec3 particleSize;
  std::string particleModel;
  glm::vec3 gridOrigin;
  glm::ivec3 gridSize;
  bool useNNS;
};

struct SimulationGridFluidConfig {
  std::filesystem::path cellModel;
  float ambientTemperature;
};

struct AppConfig {
  bool DEBUG;
  bool outputToFile;
  SimulationSPHConfig simulationSPH;
  SimulationGridFluidConfig simulationGridFluid;
};

#endif//VULKANAPP_CONFIGSTRUCTS_H
