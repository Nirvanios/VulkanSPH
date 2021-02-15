//
// Created by Igor Frank on 02.11.20.
//
#include "Config.h"

#include "glm/gtc/type_ptr.hpp"
#include <iostream>
#include <toml.hpp>

Config::Config(const std::string &configFile) {
  const auto data = toml::parse(configFile);
  const auto &tomlApp = toml::find(data, "App");
  const auto &tomlSimulationSPH = toml::find(tomlApp, "simulationSPH");
  const auto &tomlSimulationGridFluid = toml::find(tomlApp, "simulationGridFluid");
  const auto &tomlVulkan = toml::find(data, "Vulkan");
  const auto &tomlWindow = toml::find(tomlVulkan, "window");

  app.DEBUG = toml::find<bool>(tomlApp, "DEBUG");
  app.outputToFile = toml::find<bool>(tomlApp, "outputToFile");

  app.simulationSPH.particleModel = toml::find<std::string>(tomlSimulationSPH, "particleModel");
  app.simulationSPH.particleCount = toml::find<int>(tomlSimulationSPH, "particleCount");
  app.simulationSPH.fluidVolume = toml::find<float>(tomlSimulationSPH, "fluidVolume");
  app.simulationSPH.fluidDensity = toml::find<float>(tomlSimulationSPH, "fluidDensity");
  app.simulationSPH.particleSize =
      glm::make_vec3(toml::find<std::vector<int>>(tomlSimulationSPH, "particleModelSize").data());
  app.simulationSPH.gridOrigin =
      glm::make_vec3(toml::find<std::vector<float>>(tomlSimulationSPH, "gridOrigin").data());
  app.simulationSPH.gridSize =
      glm::make_vec3(toml::find<std::vector<int>>(tomlSimulationSPH, "gridSize").data());
  app.simulationSPH.gasStiffness = toml::find<float>(tomlSimulationSPH, "gasStiffness");
  app.simulationSPH.viscosityCoefficient = toml::find<float>(tomlSimulationSPH, "viscosityCoefficient");
  app.simulationSPH.timeStep = toml::find<float>(tomlSimulationSPH, "timeStep");
  app.simulationSPH.useNNS = toml::find<bool>(tomlSimulationSPH, "useNNS");

  app.simulationGridFluid.cellModel = toml::find<std::string>(tomlSimulationGridFluid, "cellModel");

  Vulkan.shaderFolder = toml::find<std::string>(tomlVulkan, "pathToShaders");

  Vulkan.window.height = toml::find<int>(tomlWindow, "height");
  Vulkan.window.width = toml::find<int>(tomlWindow, "width");
  Vulkan.window.name = toml::find<std::string>(tomlWindow, "name");

}
const AppConfig &Config::getApp() const { return app; }
const VulkanConfig &Config::getVulkan() const { return Vulkan; }
