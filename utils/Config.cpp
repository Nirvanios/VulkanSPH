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
  const auto &tomlSimulation = toml::find(tomlApp, "simulation");
  const auto &tomlVulkan = toml::find(data, "Vulkan");
  const auto &tomlWindow = toml::find(tomlVulkan, "window");

  app.DEBUG = toml::find<bool>(tomlApp, "DEBUG");
  app.outputToFile = toml::find<bool>(tomlApp, "outputToFile");

  app.simulation.particleModel = toml::find<std::string>(tomlSimulation, "particleModel");
  app.simulation.particleCount = toml::find<int>(tomlSimulation, "particleCount");
  app.simulation.fluidVolume = toml::find<float>(tomlSimulation, "fluidVolume");
  app.simulation.fluidDensity = toml::find<float>(tomlSimulation, "fluidDensity");
  app.simulation.particleSize =
      glm::make_vec3(toml::find<std::vector<int>>(tomlSimulation, "particleModelSize").data());
  app.simulation.gridOrigin =
      glm::make_vec3(toml::find<std::vector<float>>(tomlSimulation, "gridOrigin").data());
  app.simulation.gridSize =
      glm::make_vec3(toml::find<std::vector<int>>(tomlSimulation, "gridSize").data());
  app.simulation.gasStiffness = toml::find<float>(tomlSimulation, "gasStiffness");
  app.simulation.viscosityCoefficient = toml::find<float>(tomlSimulation, "viscosityCoefficient");
  app.simulation.timeStep = toml::find<float>(tomlSimulation, "timeStep");
  app.simulation.useNNS = toml::find<bool>(tomlSimulation, "useNNS");

  Vulkan.shaderFolder = toml::find<std::string>(tomlVulkan, "pathToShaders");

  Vulkan.window.height = toml::find<int>(tomlWindow, "height");
  Vulkan.window.width = toml::find<int>(tomlWindow, "width");
  Vulkan.window.name = toml::find<std::string>(tomlWindow, "name");

}
const AppConfig &Config::getApp() const { return app; }
const VulkanConfig &Config::getVulkan() const { return Vulkan; }
