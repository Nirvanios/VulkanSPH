//
// Created by Igor Frank on 20.12.20.
//

#ifndef VULKANAPP_VULKANSORT_H
#define VULKANAPP_VULKANSORT_H

#include "Buffer.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include <vulkan/vulkan.hpp>

class VulkanSort {
 public:
  VulkanSort(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
             const Config &config, std::shared_ptr<Swapchain> swapchain,
             std::shared_ptr<Buffer> bufferToSort);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &semaphoreWait);


 private:
  std::array<PipelineLayoutBindingInfo, 5> bindingInfosCompute{
      PipelineLayoutBindingInfo{.binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 2,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 3,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
      PipelineLayoutBindingInfo{.binding = 4,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eCompute},
  };

  const std::array<vk::PipelineStageFlags, 1> waitStagesCompute{
      vk::PipelineStageFlagBits::eComputeShader};

  const vk::UniqueSurfaceKHR &surface;
  std::shared_ptr<Device> device;

  std::vector<std::shared_ptr<Pipeline>> pipelinesSort;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBuffer;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSet;

  vk::UniqueFence fence;

  std::shared_ptr<Buffer> buffersUniformSort;

  std::shared_ptr<Buffer> bufferBins;
  std::shared_ptr<Buffer> bufferBinsSorted;
  std::shared_ptr<Buffer> bufferCounter;
  std::shared_ptr<Buffer> bufferSums;

  void recordCommandBuffersCompute(const std::shared_ptr<Pipeline> &pipeline,
                                   int dispatchCount);
};

#endif//VULKANAPP_VULKANSORT_H
