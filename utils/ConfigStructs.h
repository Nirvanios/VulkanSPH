//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_CONFIGSTRUCTS_H
#define VULKANAPP_CONFIGSTRUCTS_H

#include "glm/glm.hpp"
#include <filesystem>
#include <string>
#include <vector>

struct WindowConfig {
  std::string name;
  int width;
  int height;
};

struct VulkanConfig {
  WindowConfig window;
  std::filesystem::path shaderFolder;
};

struct SPHDataFiles{
  std::filesystem::path particles;
};

struct SPHModel{
  glm::ivec3 modelSize;
  glm::vec3 modelOrigin;
};

struct SimulationSPHConfig {
  float timeStep;
  float viscosityCoefficient;
  float gasStiffness;
  float heatConductivity;
  float heatCapacity;
  float fluidVolume;
  float fluidDensity;
  float temperature;
  std::string particleModel;
  glm::vec3 gridOrigin;
  glm::ivec3 gridSize;
  SPHDataFiles dataFiles;
  std::vector<SPHModel> models;
};

struct GridFluidDataFiles{
  std::filesystem::path velocities;
  std::filesystem::path velocitySources;
  std::filesystem::path values;
  std::filesystem::path valuesSources;
};

struct SimulationGridFluidConfig {
  std::filesystem::path cellModel;
  float ambientTemperature;
  float buoyancyAlpha;
  float buoyancyBeta;
  float diffusionCoefficient;
  float heatConductivity;
  float heatCapacity;
  float specificGasConstant;
  GridFluidDataFiles datafiles;
};

struct Evaportaion{
  float coefficientA;
  float coefficientB;
};

struct MarchingCubes{
  int detail;
  float threshold;
};

struct AppConfig {
  bool DEBUG;
  glm::vec3 cameraPos;
  glm::vec3 lightPos;
  glm::vec3 lightColor;
  float yaw;
  float pitch;
  SimulationSPHConfig simulationSPH;
  SimulationGridFluidConfig simulationGridFluid;
  Evaportaion evaportaion;
  MarchingCubes marchingCubes;
};

#endif//VULKANAPP_CONFIGSTRUCTS_H
