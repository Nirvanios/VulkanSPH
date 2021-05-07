//
// Created by Igor Frank on 26.12.20.
//

#include "VulkanGridSPH.h"
#include <iostream>
#include <spdlog/spdlog.h>

VulkanGridSPH::VulkanGridSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
                             Config inConfig, std::shared_ptr<Swapchain> swapchain,
                             const SimulationInfoSPH &simulationInfo,
                             std::shared_ptr<Buffer> bufferParticles,
                             std::shared_ptr<Buffer> bufferCellParticlesPair,
                             std::shared_ptr<Buffer> bufferIndexes)
    : config(std::move(inConfig)), simulationInfo(simulationInfo), device(std::move(device)),
      bufferParticles(std::move(bufferParticles)),
      bufferCellParticlePair(std::move(bufferCellParticlesPair)),
      bufferIndexes(std::move(bufferIndexes)) {

  gridInfo = {.gridSize = glm::ivec4(config.getApp().simulationSPH.gridSize, 0),
              .gridOrigin = glm::vec4(config.getApp().simulationSPH.gridOrigin, 0),
              .cellSize = simulationInfo.supportRadius,
              .particleCount =
                  static_cast<unsigned int>(simulationInfo.particleCount)};

  pipeline =
      PipelineBuilder{this->config, this->device, swapchain}
          .setLayoutBindingInfo(bindingInfosCompute)
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoSPH))
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSearch/Init.comp")
          .build();

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBufferCompute = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  std::array<vk::DescriptorPoolSize, 2> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 2},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1}};
  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets =
          [poolSize] {
            uint32_t i = 0;
            std::for_each(poolSize.begin(), poolSize.end(),
                          [&i](const auto &in) { i += in.descriptorCount; });
            return i;
          }(),
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data(),
  };
  descriptorPool = this->device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);

  std::array<DescriptorBufferInfo, 2> descriptorBufferInfosCompute{
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferCellParticlePair, 1},
                           .bufferSize = bufferCellParticlePair->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&this->bufferParticles, 1},
                           .bufferSize = this->bufferParticles->getSize()}};
  descriptorSetCompute = std::make_shared<DescriptorSet>(
      this->device, 1, pipeline->getDescriptorSetLayout(), descriptorPool);
  descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);

  recordCommandBuffer(pipeline);

  vulkanSort = std::make_unique<VulkanSort>(surface, this->device, this->config, swapchain,
                                            this->bufferCellParticlePair, this->bufferIndexes,
                                            simulationInfo);
}

vk::UniqueSemaphore VulkanGridSPH::run(const vk::UniqueSemaphore &waitSemaphore) {
  vk::Semaphore semaphoreBeforeSort = device->getDevice()->createSemaphore({});
  std::array<vk::PipelineStageFlags, 1> stageFlags{vk::PipelineStageFlagBits::eComputeShader};
  auto fence = device->getDevice()->createFenceUnique({});
  device->getDevice()->resetFences(fence.get());
  vk::SubmitInfo submitInfoGridInit{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &waitSemaphore.get(),
                                    .pWaitDstStageMask = stageFlags.data(),
                                    .commandBufferCount = 1,
                                    .pCommandBuffers = &commandBufferCompute.get(),
                                    .signalSemaphoreCount = 1,
                                    .pSignalSemaphores = &semaphoreBeforeSort};
  queue.submit(submitInfoGridInit, fence.get());

  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());

  return vulkanSort->run(vk::UniqueSemaphore(semaphoreBeforeSort, this->device->getDevice().get()));
}

void VulkanGridSPH::recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline) {

  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferCompute->begin(beginInfo);
  commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute,
                                     pipeline->getPipeline().get());
  commandBufferCompute->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSetCompute->getDescriptorSets()[0].get(), 0, nullptr);
  commandBufferCompute->pushConstants(pipeline->getPipelineLayout().get(),
                                      vk::ShaderStageFlagBits::eCompute, 0, sizeof(GridInfo),
                                      &gridInfo);
  commandBufferCompute->dispatch(
      static_cast<int>(std::ceil(simulationInfo.particleCount / 32.0)), 1, 1);
  commandBufferCompute->end();
}
const GridInfo &VulkanGridSPH::getGridInfo() const { return gridInfo; }
void VulkanGridSPH::updateInfo(const Settings &settings) {
  gridInfo = {.gridSize = glm::ivec4(settings.simulationInfoSPH.gridSize),
              .gridOrigin = glm::vec4(settings.simulationInfoSPH.gridOrigin),
              .cellSize = settings.simulationInfoSPH.supportRadius,
              .particleCount =
                  static_cast<unsigned int>(settings.simulationInfoSPH.particleCount)};
}
