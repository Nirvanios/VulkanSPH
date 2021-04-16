//
// Created by Igor Frank on 01.11.20.
//

#ifndef VULKANAPP_TESTRENDERER_H
#define VULKANAPP_TESTRENDERER_H

#include "../Third Party/Camera.h"
#include "../utils/Config.h"
#include "../vulkan/VulkanCore.h"
class TestRenderer {
 public:
  explicit TestRenderer(Config &config);
  virtual ~TestRenderer();
  void run();

 private:
  bool leftMouseButtonPressed = false;
  double xMousePosition = 0.0, yMousePosition = 0.0;
  Config &config;

  Camera camera;
  const std::string windowName = "TestRenderer";
  GlfwWindow window;
  VulkanCore vulkanCore;

  Unsubscriber keyMovementSubscriber;
  Unsubscriber mouseMovementSubscriber;
  Unsubscriber mouseButtonSubscriber;

  void cameraKeyMovement(KeyMessage messgae);
  void cameraMouseMovement(MouseMovementMessage message);
  void cameraMouseButton(MouseButtonMessage message);

  std::vector<ParticleRecord> createParticles();
  [[nodiscard]] Model createGrid(const SimulationInfoSPH &simulationInfo);
  [[nodiscard]] SimulationInfoSPH getSimulationInfoSPH();
  [[nodiscard]] SimulationInfoGridFluid getSimulationInfoGridFluid(float supportRadius);
};

#endif//VULKANAPP_TESTRENDERER_H
