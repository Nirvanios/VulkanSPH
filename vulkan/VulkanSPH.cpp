//
// Created by Igor Frank on 25.12.20.
//

#include "VulkanSPH.h"
#include "spdlog/spdlog.h"

#include <utility>

VulkanSPH::VulkanSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
                     Config config, std::shared_ptr<Swapchain> swapchain,
                     const SimulationInfo &simulationInfo,
                     const std::vector<ParticleRecord> &particles,
                     std::shared_ptr<Buffer> bufferIndexes,
                     std::shared_ptr<Buffer> bufferSortedPairs)
    : config(std::move(config)), simulationInfo(simulationInfo), device(std::move(device)),
      bufferGrid(std::move(bufferSortedPairs)), bufferIndexes(std::move(bufferIndexes)) {

  auto computePipelineBuilder = PipelineBuilder{this->config, this->device, swapchain}
                                    .setLayoutBindingInfo(bindingInfosCompute)
                                    .setPipelineType(PipelineType::Compute)
                                    .addPushConstant(vk::ShaderStageFlagBits::eCompute,
                                                     sizeof(SimulationInfo));
  if(config.getApp().simulation.useNNS)
    computePipelineBuilder.addShaderMacro("GRID");
  pipelineComputeMassDensity =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/MassDensity.comp")
          .build();
  pipelineComputeForces =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/Forces.comp")
          .build();

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBufferCompute = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  bufferParticles = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(ParticleRecord) * particles.size())
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eTransferSrc
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      this->device, commandPool, queue);
  bufferParticles->fill(particles);

  [[maybe_unused]] auto a = bufferParticles->read<ParticleRecord>();

  std::array<vk::DescriptorPoolSize, 1> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 3}};
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

  std::array<DescriptorBufferInfo, 3> descriptorBufferInfosCompute{
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferParticles, 1},
                           .bufferSize = bufferParticles->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGrid, 1},
                           .bufferSize = bufferGrid->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&this->bufferIndexes, 1},
                           .bufferSize = this->bufferIndexes->getSize()}};
  descriptorSetCompute = std::make_shared<DescriptorSet>(
      this->device, 1, pipelineComputeMassDensity->getDescriptorSetLayout(), descriptorPool);
  descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);

  fence = this->device->getDevice()->createFenceUnique({});
  semaphoreMassDensityFinished = this->device->getDevice()->createSemaphoreUnique({});
}



vk::UniqueSemaphore VulkanSPH::run(const vk::UniqueSemaphore &semaphoreWait) {
  vk::Semaphore semaphoreOut = this->device->getDevice()->createSemaphore({});
  std::array<vk::Semaphore, 1> semaphoreInput{semaphoreWait.get()};

  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};

  recordCommandBuffer(pipelineComputeMassDensity);
  vk::SubmitInfo submitInfoCompute{.waitSemaphoreCount = 1,
                                   .pWaitSemaphores = semaphoreInput.data(),
                                   .pWaitDstStageMask = waitStages.data(),
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &commandBufferCompute.get(),
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores = &semaphoreMassDensityFinished.get()};
  queue.submit(submitInfoCompute, fence.get());

  this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  this->device->getDevice()->resetFences(fence.get());

  recordCommandBuffer(pipelineComputeForces);
  submitInfoCompute.pCommandBuffers = &commandBufferCompute.get();
  submitInfoCompute.pWaitSemaphores = &semaphoreMassDensityFinished.get();
  submitInfoCompute.pSignalSemaphores = &semaphoreOut;
  queue.submit(submitInfoCompute, fence.get());
  this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  this->device->getDevice()->resetFences(fence.get());

  return vk::UniqueSemaphore(semaphoreOut, device->getDevice().get());
}

void VulkanSPH::recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline) {
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferCompute->begin(beginInfo);
  commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute,
                                     pipeline->getPipeline().get());
  commandBufferCompute->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSetCompute->getDescriptorSets()[0].get(), 0, nullptr);

  commandBufferCompute->pushConstants(pipeline->getPipelineLayout().get(),
                                      vk::ShaderStageFlagBits::eCompute, 0, sizeof(SimulationInfo),
                                      &simulationInfo);
  commandBufferCompute->dispatch(
      static_cast<int>(std::ceil(config.getApp().simulation.particleCount / 32.0)), 1, 1);
  commandBufferCompute->end();
}
const std::shared_ptr<Buffer> &VulkanSPH::getBufferParticles() const { return bufferParticles; }
