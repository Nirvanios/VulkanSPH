//
// Created by Igor Frank on 12.02.21.
//

#ifndef VULKANAPP_VULKANGRIDFLUID_H
#define VULKANAPP_VULKANGRIDFLUID_H

#include "types/DescriptorSet.h"
#include "types/Pipeline.h"
class VulkanGridFluid {
 public:
  VulkanGridFluid(const Config &config, const SimulationInfoGridFluid &simulationInfo,
                  std::shared_ptr<Device> inDevice, const vk::UniqueSurfaceKHR &surface,
                  std::shared_ptr<Swapchain> swapchain);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &semaphoreWait);
  [[nodiscard]] const std::shared_ptr<Buffer> &getBufferDensity() const;

 private:
  enum class Stages {
    addSourceScalar,
    diffuse,
    advectScalar,
    addSourceVector,
    divergenceVector,
    GaussSeidelVector,
    gradientSubtractionVector,
    advectVector
  };

  enum class SpecificInfo{
    red = 0,
    black = 1
  };

  void submit(Stages pipelineStage, const vk::Fence = nullptr, const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
              const std::optional<vk::Semaphore> &outSemaphore = std::nullopt);
  void recordCommandBuffer(Stages pipelineStage);
  void swapBuffers(std::shared_ptr<Buffer> &buffer1, std::shared_ptr<Buffer> &buffer2);
  void updateDescriptorSets();
  void fillDescriptorBufferInfo();

  const Config &config;
  SimulationInfoGridFluid simulationInfo;

  std::shared_ptr<Device> device;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::map<Stages, std::shared_ptr<DescriptorSet>> descriptorSets;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBuffer;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  std::shared_ptr<Buffer> bufferDensityNew;
  std::shared_ptr<Buffer> bufferDensityOld;
  std::shared_ptr<Buffer> bufferDensitySources;

  std::vector<vk::UniqueSemaphore> semaphores;
  int currentSemaphore;

  vk::UniqueFence fence;

  std::map<Stages, std::vector<PipelineLayoutBindingInfo>> bindingInfosCompute{
      {Stages::addSourceScalar,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::diffuse,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}}};

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute;
};

#endif//VULKANAPP_VULKANGRIDFLUID_H
