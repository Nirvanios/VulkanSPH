//
// Created by Igor Frank on 25.12.20.
//

#include "VulkanSPH.h"
#include "spdlog/spdlog.h"

#include <utility>

VulkanSPH::VulkanSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
                     Config config, std::shared_ptr<Swapchain> swapchain,
                     const SimulationInfoSPH &simulationInfo,
                     const std::vector<ParticleRecord> &inParticles,
                     std::shared_ptr<Buffer> bufferIndexes,
                     std::shared_ptr<Buffer> bufferSortedPairs)
    : particles(inParticles), config(std::move(config)), simulationInfo(simulationInfo), device(std::move(device)),
      bufferGrid(std::move(bufferSortedPairs)), bufferIndexes(std::move(bufferIndexes)) {

  auto computePipelineBuilder = PipelineBuilder{this->config, this->device, swapchain}
                                    .setLayoutBindingInfo(bindingInfosCompute)
                                    .setPipelineType(PipelineType::Compute)
                                    .addPushConstant(vk::ShaderStageFlagBits::eCompute,
                                                     sizeof(SimulationInfoSPH));

  pipelineComputeMassDensity =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/MassDensity.comp")
          .build();
  pipelineComputeMassDensityCenter =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/MassDensityCenter.comp")
          .build();
  pipelineComputeForces =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/Forces.comp")
          .build();
  pipelineAdvect =
      computePipelineBuilder
          .setComputeShaderPath(this->config.getVulkan().shaderFolder / "SPH/GridSPH/Advect.comp")
          .build();

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBufferCompute = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  createBuffers();

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
  semaphoreMassDensityCenterFinished = this->device->getDevice()->createSemaphoreUnique({});
}



vk::UniqueSemaphore VulkanSPH::run(const vk::UniqueSemaphore &semaphoreWait, SPHStep step) {
  vk::Semaphore semaphoreOut = this->device->getDevice()->createSemaphore({});
  std::array<vk::Semaphore, 1> semaphoreInput{semaphoreWait.get()};

  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};
  vk::SubmitInfo submitInfoCompute{.waitSemaphoreCount = 1,
      .pWaitSemaphores = semaphoreInput.data(),
      .pWaitDstStageMask = waitStages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferCompute.get(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &semaphoreMassDensityFinished.get()};

  switch (step){

    case SPHStep::advect:
      recordCommandBuffer(pipelineAdvect);
      submitInfoCompute.pSignalSemaphores = &semaphoreOut;
      queue.submit(submitInfoCompute, fence.get());
      this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
      this->device->getDevice()->resetFences(fence.get());
      break;
    case SPHStep::massDensity:
      recordCommandBuffer(pipelineComputeMassDensity);
      queue.submit(submitInfoCompute, fence.get());

      this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
      this->device->getDevice()->resetFences(fence.get());

      recordCommandBuffer(pipelineComputeMassDensityCenter);
      submitInfoCompute.pWaitSemaphores = &semaphoreMassDensityFinished.get();
      submitInfoCompute.pSignalSemaphores = &semaphoreOut;
      queue.submit(submitInfoCompute, fence.get());
      this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
      this->device->getDevice()->resetFences(fence.get());
      break;

    case SPHStep::force:
      recordCommandBuffer(pipelineComputeForces);
      submitInfoCompute.pWaitSemaphores = semaphoreInput.data();
      submitInfoCompute.pSignalSemaphores = &semaphoreOut;
      queue.submit(submitInfoCompute, fence.get());
      this->device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
      this->device->getDevice()->resetFences(fence.get());
      break;

  }

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
                                      vk::ShaderStageFlagBits::eCompute, 0, sizeof(SimulationInfoSPH),
                                      &simulationInfo);
  commandBufferCompute->dispatch(
      static_cast<int>(std::ceil(simulationInfo.particleCount / 32.0)), 1, 1);
  commandBufferCompute->end();
}
const std::shared_ptr<Buffer> &VulkanSPH::getBufferParticles() const { return bufferParticles; }

void VulkanSPH::createBuffers() {
  bufferParticles = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(ParticleRecord) * particles.size())
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eTransferSrc
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      this->device, commandPool, queue);
  bufferParticles->fill(particles);
}
void VulkanSPH::resetBuffers(std::optional<float> newTemp) {
  if(newTemp.has_value()) {
    std::ranges::for_each(particles.begin(), particles.end(),
                          [&newTemp](auto &item) { item.temperature = newTemp.value(); });
  }
  bufferParticles->fill(particles);
}
void VulkanSPH::setWeight(float weight) {
  auto tmp = bufferParticles->read<ParticleRecord>();
  std::for_each(tmp.begin(), tmp.end(), [&weight](auto &particle){particle.weight = weight;});
  bufferParticles->fill(tmp);
}
