//
// Created by Igor Frank on 23.03.21.
//

#ifndef VULKANAPP_VULKANGRIDFLUIDSPHCOUPLING_H
#define VULKANAPP_VULKANGRIDFLUIDSPHCOUPLING_H

#include "../utils/Config.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Device.h"
#include "types/Pipeline.h"
#include "types/Swapchain.h"
class VulkanGridFluidSPHCoupling {
 public:
  VulkanGridFluidSPHCoupling(
      const Config &config, const GridInfo &inGridInfo, const SimulationInfo &inSimulationInfo,
      std::shared_ptr<Device> inDevice, const vk::UniqueSurfaceKHR &surface,
      std::shared_ptr<Swapchain> swapchain, std::shared_ptr<Buffer> inBufferIndexes,
      std::shared_ptr<Buffer> inBufferParticles, std::shared_ptr<Buffer> inBufferGridValuesOld,
      std::shared_ptr<Buffer> inBufferGridValuesNew, std::shared_ptr<Buffer> inBufferGridSPH);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &semaphoreWait);
  [[nodiscard]] const vk::UniqueFence &getFenceAfterCompute() const;

 private:
  enum class Stages { tag, TransferHeatToCells, TransferHeatToParticles };

  void submit(Stages pipelineStage, const vk::Fence submitFence = nullptr,
              const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
              const std::optional<vk::Semaphore> &outSemaphore = std::nullopt);
  void recordCommandBuffer(Stages pipelineStage);
  void swapBuffers(std::shared_ptr<Buffer> &buffer1, std::shared_ptr<Buffer> &buffer2);
  void updateDescriptorSets();
  void fillDescriptorBufferInfo();
  void createBuffers();
  void waitFence();

  const Config &config;
  GridInfo gridInfo;
  SimulationInfo simulationInfo;

  std::shared_ptr<Device> device;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::map<Stages, std::shared_ptr<DescriptorSet>> descriptorSets;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBuffer;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  /** Density + temperature*/
  std::shared_ptr<Buffer> bufferIndexes;
  std::shared_ptr<Buffer> bufferParticles;
  std::shared_ptr<Buffer> bufferParticleTempsNew;
  std::shared_ptr<Buffer> bufferGridValuesOld;
  std::shared_ptr<Buffer> bufferGridValuesNew;
  std::shared_ptr<Buffer> bufferGridSPH;

  std::shared_ptr<Buffer> bufferUniformSimulationInfo;

  std::vector<vk::UniqueSemaphore> semaphores;
  int currentSemaphore;

  vk::UniqueFence fence;

  std::map<Stages, std::vector<PipelineLayoutBindingInfo>> bindingInfosCompute{
      {Stages::tag,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::TransferHeatToParticles,
       {{PipelineLayoutBindingInfo{.binding = 0,
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
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 4,
                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 5,
                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 6,
                                   .descriptorType = vk::DescriptorType::eUniformBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute}}}},
      {Stages::TransferHeatToCells,
       {{PipelineLayoutBindingInfo{.binding = 0,
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
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 4,
                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 5,
                                   .descriptorType = vk::DescriptorType::eStorageBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute},
         PipelineLayoutBindingInfo{.binding = 6,
                                   .descriptorType = vk::DescriptorType::eUniformBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eCompute}}}}};

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute;
};

#endif//VULKANAPP_VULKANGRIDFLUIDSPHCOUPLING_H
