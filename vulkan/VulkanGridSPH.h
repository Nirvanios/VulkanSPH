//
// Created by Igor Frank on 26.12.20.
//

#ifndef VULKANAPP_VULKANGRIDSPH_H
#define VULKANAPP_VULKANGRIDSPH_H

#include "VulkanSort.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Device.h"
#include <memory>
class VulkanGridSPH {

 public:
  VulkanGridSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device, Config config,
                std::shared_ptr<Swapchain> swapchain, SimulationInfoSPH simulationInfo, std::shared_ptr<Buffer> bufferParticles,
                std::shared_ptr<Buffer> bufferCellParticlesPair,
                std::shared_ptr<Buffer> bufferIndexes);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &waitSemaphore);
  const GridInfo &getGridInfo() const;

 private:
  std::array<PipelineLayoutBindingInfo, 2> bindingInfosCompute{
      PipelineLayoutBindingInfo{.binding = 0,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 1,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute}};

  Config config;
  SimulationInfoSPH simulationInfo;
  GridInfo gridInfo;

  std::unique_ptr<VulkanSort> vulkanSort;

  std::shared_ptr<Device> device;

  std::shared_ptr<Pipeline> pipeline;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSetCompute;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBufferCompute;

  std::shared_ptr<Buffer> bufferParticles;
  std::shared_ptr<Buffer> bufferCellParticlePair;
  std::shared_ptr<Buffer> bufferIndexes;

  void recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline);
};

#endif//VULKANAPP_VULKANGRIDSPH_H
