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
  const auto &tomlSPHModels = toml::find<toml::array>(tomlSimulationSPH, "Model");
  const auto &tomlSimulationSPHDatafiles = toml::find(tomlSimulationSPH, "datafiles");
  const auto &tomlSimulationGridFluid = toml::find(tomlApp, "simulationGridFluid");
  const auto &tomlSimulationGridFluidDatafiles = toml::find(tomlSimulationGridFluid, "datafiles");
  const auto &tomlEvaporation = toml::find(tomlApp, "Evaporation");
  const auto &tomlMarchingCubes = toml::find(tomlApp, "MarchingCubes");
  const auto &tomlVulkan = toml::find(data, "Vulkan");
  const auto &tomlWindow = toml::find(tomlVulkan, "window");

  app.DEBUG = toml::find<bool>(tomlApp, "DEBUG");
  app.cameraPos = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "cameraPos").data());
  app.yaw = toml::find<float>(tomlApp, "yaw");
  app.pitch = toml::find<float>(tomlApp, "pitch");
  app.lightColor = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "lightColor").data());
  app.lightPos = glm::make_vec3(toml::find<std::vector<float>>(tomlApp, "lightPos").data());

  app.simulationSPH.fluidVolume = toml::find<float>(tomlSimulationSPH, "fluidVolume");
  app.simulationSPH.fluidDensity = toml::find<float>(tomlSimulationSPH, "fluidDensity");
  app.simulationSPH.particleModel = toml::find<std::string>(tomlSimulationSPH, "particleModel");
  app.simulationSPH.gridOrigin =
      glm::make_vec3(toml::find<std::vector<float>>(tomlSimulationSPH, "gridOrigin").data());
  app.simulationSPH.gridSize =
      glm::make_vec3(toml::find<std::vector<int>>(tomlSimulationSPH, "gridSize").data());
  app.simulationSPH.gasStiffness = toml::find<float>(tomlSimulationSPH, "gasStiffness");
  app.simulationSPH.heatCapacity = toml::find<float>(tomlSimulationSPH, "heatCapacity");
  app.simulationSPH.heatConductivity = toml::find<float>(tomlSimulationSPH, "heatConductivity");
  app.simulationSPH.temperature = toml::find<float>(tomlSimulationSPH, "temperature");
  app.simulationSPH.viscosityCoefficient =
      toml::find<float>(tomlSimulationSPH, "viscosityCoefficient");
  app.simulationSPH.timeStep = toml::find<float>(tomlSimulationSPH, "timeStep");

  for (auto &table : tomlSPHModels) {
    app.simulationSPH.models.emplace_back(SPHModel{
        glm::ivec3(
            glm::make_vec3(toml::find<std::vector<int>>(table, "particleModelSize").data())),
        glm::make_vec3(toml::find<std::vector<float>>(table, "particleModelOrigin").data())});
  }

  app.simulationSPH.dataFiles.particles =
      toml::find_or<std::string>(tomlSimulationSPHDatafiles, "particles", "");

  app.simulationGridFluid.cellModel = toml::find<std::string>(tomlSimulationGridFluid, "cellModel");
  app.simulationGridFluid.ambientTemperature =
      toml::find<float>(tomlSimulationGridFluid, "ambientTemperature");
  app.simulationGridFluid.buoyancyAlpha =
      toml::find<float>(tomlSimulationGridFluid, "buoyancyAlpha");
  app.simulationGridFluid.buoyancyBeta = toml::find<float>(tomlSimulationGridFluid, "buoyancyBeta");
  app.simulationGridFluid.diffusionCoefficient =
      toml::find<float>(tomlSimulationGridFluid, "diffusionCoefficient");
  app.simulationGridFluid.heatConductivity =
      toml::find<float>(tomlSimulationGridFluid, "heatConductivity");
  app.simulationGridFluid.heatCapacity = toml::find<float>(tomlSimulationGridFluid, "heatCapacity");
  app.simulationGridFluid.specificGasConstant =
      toml::find<float>(tomlSimulationGridFluid, "specificGasConstant");

  app.simulationGridFluid.datafiles.velocities =
      toml::find_or<std::string>(tomlSimulationGridFluidDatafiles, "velocity", "");
  app.simulationGridFluid.datafiles.velocitySources =
      toml::find_or<std::string>(tomlSimulationGridFluidDatafiles, "velocitySrc", "");
  app.simulationGridFluid.datafiles.values =
      toml::find_or<std::string>(tomlSimulationGridFluidDatafiles, "values", "");
  app.simulationGridFluid.datafiles.valuesSources =
      toml::find_or<std::string>(tomlSimulationGridFluidDatafiles, "valuesSrc", "");

  app.evaportaion.coefficientA = toml::find<float>(tomlEvaporation, "coefficientA");
  app.evaportaion.coefficientB = toml::find<float>(tomlEvaporation, "coefficientB");

  app.marchingCubes.detail = toml::find<int>(tomlMarchingCubes, "detail");
  app.marchingCubes.threshold = toml::find<float>(tomlMarchingCubes, "threshold");

  Vulkan.shaderFolder = toml::find<std::string>(tomlVulkan, "pathToShaders");

  Vulkan.window.height = toml::find<int>(tomlWindow, "height");
  Vulkan.window.width = toml::find<int>(tomlWindow, "width");
  Vulkan.window.name = toml::find<std::string>(tomlWindow, "name");
}
const AppConfig &Config::getApp() const { return app; }
const VulkanConfig &Config::getVulkan() const { return Vulkan; }

void Config::updateCameraPos(const Camera &camera) {
  app.cameraPos = camera.Position;
  app.yaw = camera.Yaw;
  app.pitch = camera.Pitch;
}

void Config::save() {
  auto data = toml::parse(file);
  auto &tomlApp = toml::find(data, "App");

  tomlApp.as_table().at("cameraPos").as_array() = {app.cameraPos.x, app.cameraPos.y,
                                                   app.cameraPos.z};
  tomlApp.as_table().at("yaw").as_floating() = app.yaw;
  tomlApp.as_table().at("pitch").as_floating() = app.pitch;
  Utilities::writeFile(file, toml::format(data));
}
