//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_VULKANCORE_H
#define VULKANAPP_VULKANCORE_H

#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "../utils/Config.h"
#include "../utils/saver/VideoDiskSaver.h"
#include "../window/GlfwWindow.h"

#include "VulkanGridSPH.h"
#include "VulkanSPH.h"
#include "VulkanSort.h"
#include "builders/PipelineBuilder.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Device.h"
#include "types/Framebuffers.h"
#include "types/Image.h"
#include "types/Instance.h"
#include "types/Pipeline.h"
#include "types/Swapchain.h"
#include "types/Types.h"

class VulkanCore {

 public:
  explicit VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos);
  ~VulkanCore();
  void setViewMatrixGetter(std::function<glm::mat4()> getter);
  [[nodiscard]] bool isFramebufferResized() const;
  void setFramebufferResized(bool resized);
  void initVulkan(const std::vector<Model> &modelParticle, const std::vector<ParticleRecord> particles, const SimulationInfo &simulationInfo);
  void run();

 private:
  std::array<PipelineLayoutBindingInfo, 3> bindingInfosRender{
      PipelineLayoutBindingInfo{
          .binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex,
      },
      PipelineLayoutBindingInfo{
          .binding = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eFragment,
      },
      PipelineLayoutBindingInfo{.binding = 2,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex}};

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
  int currentFrame = 0;
  bool framebufferResized = false;
  std::vector<int> indicesSize;
  std::vector<int> verticesSize;


  double time = 0;
  uint steps = 0;

  std::function<glm::mat4()> viewMatrixGetter = []() { return glm::mat4(1.0f); };
  const glm::vec3 &cameraPos;
  SimulationInfo simulationInfo;
  VideoDiskSaver videoDiskSaver;

  std::shared_ptr<Instance> instance;
  std::shared_ptr<Device> device;
  vk::UniqueSurfaceKHR surface;
  std::shared_ptr<Swapchain> swapchain;

  std::shared_ptr<Pipeline> pipelineGraphics;
  std::shared_ptr<Pipeline> pipelineGraphicsGrid;

  std::shared_ptr<Framebuffers> framebuffers;
  vk::UniqueCommandPool commandPoolGraphics;
  std::vector<vk::UniqueCommandBuffer> commandBuffersGraphic;

  std::vector<vk::UniqueSemaphore> semaphoreImageAvailable, semaphoreRenderFinished, semaphoreAfterSimulation, semaphoreAfterSort;
  std::vector<vk::UniqueFence> fencesInFlight;
  std::vector<std::optional<vk::Fence>> fencesImagesInFlight;

  vk::Queue queueGraphics;
  vk::Queue queuePresent;

  std::shared_ptr<Buffer> bufferVertex;
  std::shared_ptr<Buffer> bufferIndex;
  std::shared_ptr<Buffer> bufferCellParticlePair;
  std::shared_ptr<Buffer> bufferIndexes;
  std::vector<std::shared_ptr<Buffer>> buffersUniformMVP;
  std::vector<std::shared_ptr<Buffer>> buffersUniformCameraPos;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSetGraphics;

  std::shared_ptr<Image> imageDepth;
  std::shared_ptr<Image> imageOutput;
  std::shared_ptr<Image> stagingImage;
  const Config &config;
  GlfwWindow &window;

  std::unique_ptr<VulkanSPH> vulkanSPH;
  std::unique_ptr<VulkanGridSPH> vulkanGridSPH;

  void mainLoop();
  void cleanup();

  void createSurface();

  void createVertexBuffer(const std::vector<Model> &models);
  void createIndexBuffer(const std::vector<Model> &models);
  void createOutputImage();
  void createUniformBuffers();

  void createCommandPool();
  void createCommandBuffers();
  void createDescriptorPool();

  void createDepthResources();
  vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling,
                                 const vk::FormatFeatureFlags &features);
  vk::Format findDepthFormat();

  void drawFrame();
  void updateUniformBuffers(uint32_t currentImage);
  void createSyncObjects();

  void recreateSwapchain();

};

#endif//VULKANAPP_VULKANCORE_H
