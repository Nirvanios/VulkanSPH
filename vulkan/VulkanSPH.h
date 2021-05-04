//
// Created by Igor Frank on 25.12.20.
//

#ifndef VULKANAPP_VULKANSPH_H
#define VULKANAPP_VULKANSPH_H

#include "../utils/Config.h"
#include "enums.h"
#include "types/DescriptorSet.h"
#include "types/Pipeline.h"
#include "types/Swapchain.h"
class VulkanSPH {
 public:
  VulkanSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device, Config config,
            std::shared_ptr<Swapchain> swapchain, const SimulationInfoSPH &simulationInfo,
            const std::vector<ParticleRecord> &inParticles, std::shared_ptr<Buffer> bufferIndexes,
            std::shared_ptr<Buffer> bufferSortedPairs);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &semaphoreWait, SPHStep step);
  void resetBuffers(std::optional<float> newTemp = std::nullopt);

  [[nodiscard]] const std::shared_ptr<Buffer> &getBufferParticles() const;

 private:
  void createBuffers();

  std::vector<ParticleRecord> particles;

  std::array<PipelineLayoutBindingInfo, 3> bindingInfosCompute{
      PipelineLayoutBindingInfo{.binding = 0,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 1,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 2,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute}};

  Config config;
  const SimulationInfoSPH &simulationInfo;

  std::shared_ptr<Device> device;

  std::shared_ptr<Pipeline> pipelineComputeMassDensity;
  std::shared_ptr<Pipeline> pipelineComputeMassDensityCenter;
  std::shared_ptr<Pipeline> pipelineComputeForces;
  std::shared_ptr<Pipeline> pipelineAdvect;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSetCompute;

  std::shared_ptr<Buffer> bufferParticles;
  std::shared_ptr<Buffer> bufferGrid;
  std::shared_ptr<Buffer> bufferIndexes;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBufferCompute;

  vk::UniqueSemaphore semaphoreMassDensityFinished;
  vk::UniqueSemaphore semaphoreMassDensityCenterFinished;
  vk::UniqueFence fence;

  void recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline);
};

#endif//VULKANAPP_VULKANSPH_H
