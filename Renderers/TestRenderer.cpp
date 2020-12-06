//
// Created by Igor Frank on 01.11.20.
//

#include "TestRenderer.h"

#include "glm/gtx/string_cast.hpp"
#include <numbers>
#include <spdlog/spdlog.h>

TestRenderer::TestRenderer(const Config &config)
    : config(config), window(config.getVulkan().window.name, config.getVulkan().window.width,
                             config.getVulkan().window.height),
      vulkanCore(config, window, camera.Position),
      keyMovementSubscriber(
          window.subscribeToKeyEvents([this](KeyMessage message) { cameraKeyMovement(message); })),
      mouseMovementSubscriber(window.subscribeToMouseMovementEvents(
          [this](MouseMovementMessage message) { cameraMouseMovement(message); })),
      mouseButtonSubscriber(window.subscribeToMouseButtonEvents(
          [this](MouseButtonMessage message) { cameraMouseButton(message); })) {
  vulkanCore.setViewMatrixGetter([this]() { return camera.GetViewMatrix(); });

  auto particles = createParticles();
  vulkanCore.initVulkan(Utilities::loadModelFromObj(config.getApp().simulation.particleModel,
                                                    glm::vec3{0.5, 0.8, 1.0}),
                        std::span<ParticleRecord>{particles.data(), particles.size()});
  vulkanCore.setSimulationInfo(getSimulationInfo());
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
  const auto &simConfig = config.getApp().simulation;
  const auto particleSize =
      glm::vec3(std::cbrt(simConfig.fluidVolume / static_cast<float>(simConfig.particleCount)));
  std::vector<ParticleRecord> data{static_cast<size_t>(config.getApp().simulation.particleCount)};

  int sizeZ = config.getApp().simulation.particleSize.z;
  int sizeY = config.getApp().simulation.particleSize.y;
  int sizeX = config.getApp().simulation.particleSize.x;
  for (int z = 0; z < sizeZ; ++z) {
    for (int y = 0; y < sizeY; ++y) {
      for (int x = 0; x < sizeX; ++x) {
        data[(z * sizeY * sizeX) + (y * sizeX) + x].position =
            glm::vec4{x, y, z, 0.0f} * glm::vec4(particleSize, 0.0f);
        data[(z * sizeY * sizeX) + (y * sizeX) + x].currentVelocity = glm::vec4{0.0f};
        data[(z * sizeY * sizeX) + (y * sizeX) + x].velocity = glm::vec4{0.0f};
        data[(z * sizeY * sizeX) + (y * sizeX) + x].massDensity = -1.0f;
        data[(z * sizeY * sizeX) + (y * sizeX) + x].pressure = -1.0f;
      }
    }
  }
  return data;
}

TestRenderer::~TestRenderer() {
  mouseButtonSubscriber.unsubscribe();
  mouseMovementSubscriber.unsubscribe();
  keyMovementSubscriber.unsubscribe();
}
SimulationInfo TestRenderer::getSimulationInfo() {
  const auto &simConfig = config.getApp().simulation;
  const auto mass = simConfig.fluidDensity
      * (simConfig.fluidVolume / static_cast<float>(simConfig.particleCount));
  const auto x = 20;
  const auto supportRadius =
      std::cbrt((3 * simConfig.fluidVolume * x) / (4 * std::numbers::pi * simConfig.particleCount));
  return SimulationInfo{.gravityForce = glm::vec4{0.0f, -9.8, 0.0, 0.0},
                        .particleMass = mass,
                        .restDensity = simConfig.fluidDensity,
                        .viscosityCoefficient = simConfig.viscosityCoefficient,
                        .gasStiffnessConstant = simConfig.gasStiffness,
                        .timeStep = simConfig.timeStep,
                        .supportRadius = static_cast<float>(supportRadius),
                        .tensionThreshold = 7.065,
                        .tensionCoefficient = 0.0728,
                        .particleCount = static_cast<unsigned int>(simConfig.particleCount)};
}
