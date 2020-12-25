//
// Created by Igor Frank on 25.12.20.
//

#ifndef VULKANAPP_VULKANSPH_H
#define VULKANAPP_VULKANSPH_H

#include "../utils/Config.h"
#include "types/DescriptorSet.h"
#include "types/Pipeline.h"
#include "types/Swapchain.h"
class VulkanSPH {
 public:
  VulkanSPH (const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
             Config config, std::shared_ptr<Swapchain> swapchain,
             const SimulationInfo &simulationInfo, const std::vector<ParticleRecord> &particles);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &semaphoreWait);

  const std::shared_ptr<Buffer> &getBufferParticles() const;

 private:
  std::array<PipelineLayoutBindingInfo, 1> bindingInfosCompute{
      PipelineLayoutBindingInfo{.binding = 0,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute}};

  Config config;
  SimulationInfo simulationInfo;

  std::shared_ptr<Device> device;

  std::shared_ptr<Pipeline> pipelineComputeMassDensity;
  std::shared_ptr<Pipeline> pipelineComputeForces;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSetCompute;

  std::shared_ptr<Buffer> bufferShaderStorage;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBufferCompute;

  vk::UniqueSemaphore semaphoreMassDensityFinished;
  vk::UniqueFence fence;


  void recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline);

};

#endif//VULKANAPP_VULKANSPH_H