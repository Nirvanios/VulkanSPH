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

  void submit(Stages pipelineStage, const std::optional<vk::Semaphore> &inSemaphore,
              const std::optional<vk::Semaphore> &outSemaphore);
  void recordCommandBuffer(Stages pipelineStage);

  const Config &config;
  const SimulationInfoGridFluid simulationInfo;

  std::shared_ptr<Device> device;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<DescriptorSet> descriptorSetCompute;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBufferCompute;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  std::shared_ptr<Buffer> bufferDensity;
  std::shared_ptr<Buffer> bufferDensitySources;

  std::vector<vk::UniqueSemaphore> semaphores;
  int currentSemaphore;

  vk::UniqueFence fence;
};

#endif//VULKANAPP_VULKANGRIDFLUID_H
