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
  vk::UniqueSemaphore run();// TODO wait semaphore
  [[nodiscard]] const std::shared_ptr<Buffer> &getBufferValuesNew() const;
  [[nodiscard]] const std::shared_ptr<Buffer> &getBufferValuesOld() const;
  [[nodiscard]] const vk::UniqueFence &getFenceAfterCompute() const;
  const std::shared_ptr<Buffer> &getBufferVelocitiesNew() const;

 private:
  enum class Stages {
    addSourceScalar,
    diffuseScalar,
    advectScalar,
    addSourceVector,
    diffuseVector,
    divergenceVector,
    GaussSeidelDivergence,
    gradientSubtractionVector,
    advectVector,
    boundaryHandleScalar,
    boundaryHandleVector,
    boundaryHandleVec2
  };

  void submit(Stages pipelineStage, const vk::Fence = nullptr,
              const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
              const std::optional<vk::Semaphore> &outSemaphore = std::nullopt,
              SubmitSemaphoreType submitSemaphoreType = SubmitSemaphoreType::None);
  void recordCommandBuffer(Stages pipelineStage);
  void swapBuffers(std::shared_ptr<Buffer> &buffer1, std::shared_ptr<Buffer> &buffer2);
  void updateDescriptorSets();
  void fillDescriptorBufferInfo();
  void createBuffers();
  void setBoundaryScalarStageBuffer(std::shared_ptr<Buffer> &buffer);
  void project();
  void waitFence();

  const Config &config;
  SimulationInfoGridFluid simulationInfo;

  std::shared_ptr<Device> device;

  vk::Queue queue;

  vk::UniqueDescriptorPool descriptorPool;
  std::map<Stages, std::shared_ptr<DescriptorSet>> descriptorSets;

  vk::UniqueCommandPool commandPool;
  vk::UniqueCommandBuffer commandBuffer;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  /** Density + temperature*/
  std::shared_ptr<Buffer> bufferValuesNew;
  std::shared_ptr<Buffer> bufferValuesOld;
  std::shared_ptr<Buffer> bufferValuesSources;

  std::shared_ptr<Buffer> bufferVelocitiesNew;
  std::shared_ptr<Buffer> bufferVelocitiesOld;
  std::shared_ptr<Buffer> bufferVelocitySources;

  std::shared_ptr<Buffer> bufferDivergences;
  std::shared_ptr<Buffer> bufferPressures;

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
      {Stages::diffuseScalar,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::advectScalar,
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
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::boundaryHandleScalar,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::addSourceVector,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::diffuseVector,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::divergenceVector,
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
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::GaussSeidelDivergence,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::gradientSubtractionVector,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::boundaryHandleVector,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::boundaryHandleVec2,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::advectVector,
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
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}}};

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute;
};

#endif//VULKANAPP_VULKANGRIDFLUID_H
