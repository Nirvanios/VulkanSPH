//
// Created by Igor Frank on 02.11.20.
//
#include "Config.h"

#include "glm/gtc/type_ptr.hpp"
#include <iostream>
#include <toml.hpp>

#include "Utilities.h"

Config::Config(const std::string &configFile) : file(configFile) {
  const auto data = toml::parse(file);
  const auto &tomlApp = toml::find(data, "App");
  const auto &tomlSimulationSPH = toml::find(tomlApp, "simulationSPH");
  const auto &tomlSimulationGridFluid = toml::find(tomlApp, "simulationGridFluid");
  const auto &tomlMarchingCubes = toml::find(tomlApp, "MarchingCubes");
  const auto &tomlVulkan = toml::find(data, "Vulkan");
  const auto &tomlWindow = toml::find(tomlVulkan, "window");

  app.DEBUG = toml::find<bool>(tomlApp, "DEBUG");
  app.outputToFile = toml::find<bool>(tomlApp, "outputToFile");
  app.cameraPos = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "cameraPos").data());
  app.yaw = toml::find<float>(tomlApp, "yaw");
  app.pitch = toml::find<float>(tomlApp, "pitch");
  app.lightColor = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "lightColor").data());
  app.lightPos = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "lightPos").data());

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
  app.simulationSPH.heatCapacity = toml::find<float>(tomlSimulationSPH, "heatCapacity");
  app.simulationSPH.heatConductivity = toml::find<float>(tomlSimulationSPH, "heatConductivity");
  app.simulationSPH.viscosityCoefficient =
      toml::find<float>(tomlSimulationSPH, "viscosityCoefficient");
  app.simulationSPH.timeStep = toml::find<float>(tomlSimulationSPH, "timeStep");
  app.simulationSPH.useNNS = toml::find<bool>(tomlSimulationSPH, "useNNS");

  app.simulationGridFluid.cellModel = toml::find<std::string>(tomlSimulationGridFluid, "cellModel");
  app.simulationGridFluid.ambientTemperature =
      toml::find<float>(tomlSimulationGridFluid, "ambientTemperature");

  app.marchingCubes.detail = toml::find<int>(tomlMarchingCubes, "detail");
  app.marchingCubes.threshold = toml::find<float>(tomlMarchingCubes, "threshold");

  Vulkan.shaderFolder = toml::find<std::string>(tomlVulkan, "pathToShaders");

  Vulkan.window.height = toml::find<int>(tomlWindow, "height");
  Vulkan.window.width = toml::find<int>(tomlWindow, "width");
  Vulkan.window.name = toml::find<std::string>(tomlWindow, "name");
}
const AppConfig &Config::getApp() const { return app; }
const VulkanConfig &Config::getVulkan() const { return Vulkan; }

void Config::updateCameraPos(const Camera &camera){
    app.cameraPos = camera.Position;
    app.yaw = camera.Yaw;
    app.pitch = camera.Pitch;
}

void Config::save() {
  auto data = toml::parse(file);
  auto &tomlApp = toml::find(data, "App");

  tomlApp.as_table().at("cameraPos").as_array() = {app.cameraPos.x, app.cameraPos.y, app.cameraPos.z};
  tomlApp.as_table().at("yaw").as_floating() = app.yaw;
  tomlApp.as_table().at("pitch").as_floating() = app.pitch;
  Utilities::writeFile(file, toml::format(data));
}
