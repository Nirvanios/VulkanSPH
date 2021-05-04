//
// Created by Igor Frank on 01.11.20.
//

#include "TestRenderer.h"

#include "glm/gtx/component_wise.hpp"
#include "glm/gtx/string_cast.hpp"
#include <numbers>
#include <spdlog/spdlog.h>

TestRenderer::TestRenderer(Config &config)
    : config(config), camera(config.getApp().cameraPos, glm::vec3(0, 1, 0), config.getApp().yaw,
                             config.getApp().pitch),
      window(config.getVulkan().window.name, config.getVulkan().window.width,
             config.getVulkan().window.height),
      vulkanCore(config, window, camera.Position, camera.Yaw, camera.Pitch),
      keyMovementSubscriber(
          window.subscribeToKeyEvents([this](KeyMessage message) { cameraKeyMovement(message); })),
      mouseMovementSubscriber(window.subscribeToMouseMovementEvents(
          [this](MouseMovementMessage message) { cameraMouseMovement(message); })),
      mouseButtonSubscriber(window.subscribeToMouseButtonEvents(
          [this](MouseButtonMessage message) { cameraMouseButton(message); })) {
  vulkanCore.setViewMatrixGetter([this]() { return camera.GetViewMatrix(); });

  auto simulationInfoSPH = getSimulationInfoSPH();
  auto particles = createParticles();
  auto gridModel = createGrid(simulationInfoSPH);
  vulkanCore.initVulkan({Utilities::loadModelFromObj(config.getApp().simulationSPH.particleModel,
                                                     glm::vec3{0.5, 0.8, 1.0}),
                         gridModel},
                        particles, simulationInfoSPH,
                        getSimulationInfoGridFluid(simulationInfoSPH.supportRadius));
  //vulkanCore.setSimulationInfo(getSimulationInfoSPH());
}

void TestRenderer::run() { vulkanCore.run(); }

void TestRenderer::cameraKeyMovement(KeyMessage messgae) {
  switch (messgae.key) {
    case GLFW_KEY_W:
    case GLFW_KEY_UP: camera.ProcessKeyboard(Camera_Movement::FORWARD, 0.1); break;
    case GLFW_KEY_S:
    case GLFW_KEY_DOWN: camera.ProcessKeyboard(Camera_Movement::BACKWARD, 0.1); break;
    case GLFW_KEY_A:
    case GLFW_KEY_LEFT: camera.ProcessKeyboard(Camera_Movement::LEFT, 0.1); break;
    case GLFW_KEY_D:
    case GLFW_KEY_RIGHT: camera.ProcessKeyboard(Camera_Movement::RIGHT, 0.1); break;
  }
}
void TestRenderer::cameraMouseMovement(MouseMovementMessage message) {
  if (leftMouseButtonPressed) {
    camera.ProcessMouseMovement(static_cast<float>(xMousePosition - message.xPosition),
                                static_cast<float>(message.yPosition - yMousePosition));
    xMousePosition = message.xPosition;
    yMousePosition = message.yPosition;
  }
}
void TestRenderer::cameraMouseButton(MouseButtonMessage message) {
  leftMouseButtonPressed =
      (message.button == GLFW_MOUSE_BUTTON_LEFT && message.action == MouseButtonAction::Press);
  if (leftMouseButtonPressed) {
    glfwGetCursorPos(window.getWindow().get(), &xMousePosition, &yMousePosition);
    glfwSetInputMode(window.getWindow().get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  } else {
    glfwSetInputMode(window.getWindow().get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

std::vector<ParticleRecord> TestRenderer::createParticles() {
  const auto &simConfig = config.getApp().simulationSPH;
  if(!std::empty(simConfig.dataFiles.particles)) {
    if (std::filesystem::exists(simConfig.dataFiles.particles)
        && std::filesystem::is_regular_file(simConfig.dataFiles.particles)){
      return Utilities::loadDataFromFile<ParticleRecord>(simConfig.dataFiles.particles);
    }
    else{
      throw std::runtime_error(fmt::format("File {} does not exist.", simConfig.dataFiles.particles));
    }
  }
  return Utilities::generateParticles(simConfig.fluidVolume, simConfig.particleCount,
                                      simConfig.particleSize, simConfig.temperature);
}

TestRenderer::~TestRenderer() {
  mouseButtonSubscriber.unsubscribe();
  mouseMovementSubscriber.unsubscribe();
  keyMovementSubscriber.unsubscribe();

  config.updateCameraPos(camera);
  config.save();
}
SimulationInfoSPH TestRenderer::getSimulationInfoSPH() {
  const auto &simConfig = config.getApp().simulationSPH;
  const auto mass = simConfig.fluidDensity
      * (simConfig.fluidVolume / static_cast<float>(simConfig.particleCount));
  const auto x = 20;
  const auto supportRadius =
      std::cbrt((3 * simConfig.fluidVolume * x) / (4 * std::numbers::pi * simConfig.particleCount));
  return SimulationInfoSPH{
      .gridSize = glm::ivec4(
          config.getApp().simulationSPH.gridSize,
          static_cast<unsigned int>(glm::compMul(config.getApp().simulationSPH.gridSize))),
      .gridOrigin = glm::vec4(config.getApp().simulationSPH.gridOrigin, 0),
      .gravityForce = glm::vec4{0.0f, -9.8, 0.0, 0.0},
      .particleMass = mass,
      .restDensity = simConfig.fluidDensity,
      .viscosityCoefficient = simConfig.viscosityCoefficient,
      .gasStiffnessConstant = simConfig.gasStiffness,
      .heatConductivity = simConfig.heatConductivity,
      .heatCapacity = simConfig.heatCapacity,
      .timeStep = simConfig.timeStep,
      .supportRadius = static_cast<float>(supportRadius),
      .tensionThreshold = 7.065,
      .tensionCoefficient = 0.0728,
      .particleCount = static_cast<unsigned int>(simConfig.particleCount)
      /*.cellCount = static_cast<unsigned int>(glm::compMul(config.getApp().simulationSPH.gridSize))*/};
}
Model TestRenderer::createGrid(const SimulationInfoSPH &simulationInfo) {
  auto gridSize = glm::vec3(simulationInfo.gridSize.xyz()) * simulationInfo.supportRadius;
  auto &gridOrigin = config.getApp().simulationSPH.gridOrigin;
  std::vector<glm::vec3> positions{gridOrigin,
                                   gridOrigin + glm::vec3(gridSize.x, 0, 0),
                                   gridOrigin + glm::vec3(gridSize.xy(), 0),
                                   gridOrigin + glm::vec3(0, gridSize.y, 0),
                                   gridOrigin + glm::vec3(0, 0, gridSize.z),
                                   gridOrigin + glm::vec3(gridSize.x, 0, gridSize.z),
                                   gridOrigin + gridSize.xyz(),
                                   gridOrigin + glm::vec3(0, gridSize.yz())};

  std::vector<Vertex> gridVertices(8);
  {
    auto i = 0;
    std::for_each(gridVertices.begin(), gridVertices.end(),
                  [&](auto &vertex) { vertex.pos = positions[i++]; });
  }
  std::vector<uint16_t> gridIndices{0, 1, 2, 3, 0, static_cast<unsigned short>(-1),
                                    4, 5, 6, 7, 4, static_cast<unsigned short>(-1),
                                    0, 3, 7, 4, 0, static_cast<unsigned short>(-1),
                                    1, 2, 6, 5, 1, static_cast<unsigned short>(-1)};

  return {.vertices = gridVertices, .indices = gridIndices};
}
SimulationInfoGridFluid TestRenderer::getSimulationInfoGridFluid(float supportRadius) {
  return SimulationInfoGridFluid{
      .gridSize = glm::ivec4(config.getApp().simulationSPH.gridSize, 0),
      .gridOrigin = glm::vec4(config.getApp().simulationSPH.gridOrigin, 0),
      .timeStep = config.getApp().simulationSPH.timeStep,
      .cellCount = glm::compMul(config.getApp().simulationSPH.gridSize),
      .cellSize = supportRadius,
      .diffusionCoefficient = 0.001,
      .boundaryScale = 1,
      .specificInfo = 0,
      .heatConductivity = 0.62,//TODO config
      .heatCapacity = 4.179,
      .specificGasConstant = 461.5,
      .ambientTemperature = config.getApp().simulationGridFluid.ambientTemperature,
      .buoyancyAlpha = config.getApp().simulationGridFluid.buoyancyAlpha,
      .buoyancyBeta = config.getApp().simulationGridFluid.buoyancyBeta,
  };
}
