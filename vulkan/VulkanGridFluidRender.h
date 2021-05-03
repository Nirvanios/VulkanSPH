//
// Created by Igor Frank on 13.02.21.
//

#ifndef VULKANAPP_VULKANGRIDFLUIDRENDER_H
#define VULKANAPP_VULKANGRIDFLUIDRENDER_H

#include "builders/PipelineBuilder.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Framebuffers.h"
#include "types/Pipeline.h"

#include "../ui/ImGuiGlfwVulkan.h"
#include "pf_imgui/ImGuiInterface.h"

#include <memory>

class VulkanGridFluidRender {
 public:
  VulkanGridFluidRender(Config config, const std::shared_ptr<Device> &device,
                        const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Swapchain> swapchain,
                        const Model &model,
                        const SimulationInfoGridFluid &inSimulationInfoGridFluid,
                        std::shared_ptr<Buffer> inBufferDensity,
                        std::vector<std::shared_ptr<Buffer>> buffersUniformMVP,
                        std::vector<std::shared_ptr<Buffer>> buffersUniformCameraPos,
                        std::shared_ptr<Buffer> inBufferTags);
  vk::UniqueSemaphore draw(const vk::UniqueSemaphore &inSemaphore, unsigned int imageIndex);
  void updateDensityBuffer(std::shared_ptr<Buffer> densityBufferNew);
  void rebuildPipeline(bool clearBeforeDraw);
  void setFramebuffersSwapchain(const std::shared_ptr<Framebuffers> &framebuffer);
  void recordRenderpass(unsigned int imageIndex, const vk::UniqueCommandBuffer &commandBuffer);
  void updateUniformBuffer(unsigned int imageIndex, float yaw, float pitch);

 private:
  std::array<PipelineLayoutBindingInfo, 5> bindingInfosRender{
      PipelineLayoutBindingInfo{.binding = 0,
                                .descriptorType = vk::DescriptorType::eUniformBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex},
      PipelineLayoutBindingInfo{.binding = 1,
                                .descriptorType = vk::DescriptorType::eUniformBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eFragment},
      PipelineLayoutBindingInfo{.binding = 2,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex},
      PipelineLayoutBindingInfo{.binding = 3,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex},
      PipelineLayoutBindingInfo{.binding = 4,
                                .descriptorType = vk::DescriptorType::eUniformBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eVertex}};

  std::vector<DescriptorBufferInfo> descriptorBufferInfos;

  int currentFrame;
  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

  std::vector<int> indicesByteOffsets;
  std::vector<int> indicesSizes;
  std::vector<int> verticesCountOffset;

  Config config;
  const SimulationInfoGridFluid &simulationInfoGridFluid;

  std::shared_ptr<Device> device;
  std::shared_ptr<Swapchain> swapchain;

  std::shared_ptr<Buffer> bufferVertex;
  std::shared_ptr<Buffer> bufferIndex;

  std::vector<std::shared_ptr<Buffer>> buffersUniformMVP;
  std::vector<std::shared_ptr<Buffer>> buffersUniformCameraPos;
  std::vector<std::shared_ptr<Buffer>> buffersUniformAngles;

  std::shared_ptr<Buffer> bufferDensity;
  std::shared_ptr<Buffer> bufferTags;

  std::shared_ptr<Pipeline> pipeline;

  std::shared_ptr<Framebuffers> framebuffersSwapchain;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSet;

  std::shared_ptr<Image> imageDepth;

  vk::Queue queue;

  vk::UniqueCommandPool commandPool;
  std::vector<vk::UniqueCommandBuffer> commandBuffers;

  //std::vector<vk::UniqueSemaphore> semaphoreImageAvailable;

  std::vector<vk::UniqueFence> fencesInFlight;
  std::vector<std::optional<vk::Fence>> fencesImagesInFlight;

  void createVertexBuffer(const std::vector<Model> &models);
  void createIndexBuffer(const std::vector<Model> &models);
  void createUniformBuffers();
  void recordCommandBuffers(unsigned int imageIndex);
  void createDescriptorPool();
  void createDepthResources();
};

#endif//VULKANAPP_VULKANGRIDFLUIDRENDER_H
