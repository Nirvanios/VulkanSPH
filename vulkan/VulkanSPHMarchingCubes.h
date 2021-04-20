//
// Created by Igor Frank on 19.04.21.
//

#ifndef VULKANAPP_VULKANSPHMARCHINGCUBES_H
#define VULKANAPP_VULKANSPHMARCHINGCUBES_H

#include "../utils/Config.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Device.h"
#include "types/Swapchain.h"
#include "types/Types.h"
class VulkanSPHMarchingCubes {

 public:
  VulkanSPHMarchingCubes(const Config &config, const SimulationInfoSPH &simulationInfo,
                         std::shared_ptr<Device> inDevice, const vk::UniqueSurfaceKHR &surface,
                         std::shared_ptr<Swapchain> swapchain,
                         std::shared_ptr<Buffer> inBufferParticles,
                         std::shared_ptr<Buffer> inBufferIndexes,
                         std::shared_ptr<Buffer> inBufferSortedPairs);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &inSemaphore);

 private:
  enum class Stages { ComputeColors };

  void submit(Stages pipelineStage, const vk::Fence &submitFence = nullptr,
              const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
              const std::optional<vk::Semaphore> &outSemaphore = std::nullopt,
              SubmitSemaphoreType submitSemaphoreType = SubmitSemaphoreType::None);
  void recordCommandBuffer(Stages pipelineStage);
  void swapBuffers(std::shared_ptr<Buffer> &buffer1, std::shared_ptr<Buffer> &buffer2);
  void updateDescriptorSets();
  void fillDescriptorBufferInfo();
  void createBuffers();
  void waitFence();

  const Config &config;
  SimulationInfoSPH simulationInfo;

  std::shared_ptr<Device> device;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::map<Stages, std::shared_ptr<DescriptorSet>> descriptorSets;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBuffer;

  std::vector<vk::UniqueSemaphore> semaphores;
  int currentSemaphore;

  vk::UniqueFence fence;

  std::shared_ptr<Buffer> bufferGridColors;
  std::shared_ptr<Buffer> bufferParticles;
  std::shared_ptr<Buffer> bufferGrid;
  std::shared_ptr<Buffer> bufferIndexes;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute;

  std::map<Stages, std::vector<PipelineLayoutBindingInfo>> bindingInfosCompute{
      {Stages::ComputeColors,
       {PipelineLayoutBindingInfo{.binding = 0,
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
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 3,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}}};
};

#endif//VULKANAPP_VULKANSPHMARCHINGCUBES_H
