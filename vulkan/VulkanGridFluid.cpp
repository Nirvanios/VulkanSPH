//
// Created by Igor Frank on 12.02.21.
//

#include "VulkanGridFluid.h"
#include <magic_enum.hpp>
#include <numeric>
#include <ranges>
#include <utility>

vk::UniqueSemaphore VulkanGridFluid::run(const vk::UniqueSemaphore &semaphoreWait) {

  auto outSemaphore = device->getDevice()->createSemaphoreUnique({});

  submit(Stages::addSourceScalar, semaphoreWait.get(), outSemaphore.get());

  return std::move(outSemaphore);
  /*
  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore1.get();
  submitInfoCompute.pSignalSemaphores = &semaphore2.get();
  recordCommandBuffer(Stages::diffuse);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound

  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::advectScalar);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound

  */
  /**Add Source*/        /*
  submitInfoCompute.pWaitSemaphores = &semaphore1.get();
  submitInfoCompute.pSignalSemaphores = &semaphore2.get();
  recordCommandBuffer(Stages::addSourceVector);
  queue.submit(submitInfoCompute, nullptr);

  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::diffuse);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound

  */
  /**Project*/           /*
  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::divergenceVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::GaussSeidelVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::gradientSubtractionVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound

  */
  /**Advect velocities*/ /*
  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::advectVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound

  */
  /**Project*/           /*
  //TODO SWAP
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::divergenceVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::GaussSeidelVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound
  submitInfoCompute.pWaitSemaphores = &semaphore2.get();
  submitInfoCompute.pSignalSemaphores = &semaphore1.get();
  recordCommandBuffer(Stages::gradientSubtractionVector);
  queue.submit(submitInfoCompute, nullptr);
  //TODO bound
  */
}
void VulkanGridFluid::recordCommandBuffer(Stages pipelineStage) {
  const auto &pipeline = pipelines[pipelineStage];
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferCompute->begin(beginInfo);
  commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute,
                                     pipeline->getPipeline().get());
  commandBufferCompute->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSetCompute->getDescriptorSets()[0].get(), 0, nullptr);

  commandBufferCompute->pushConstants(pipeline->getPipelineLayout().get(),
                                      vk::ShaderStageFlagBits::eCompute, 0,
                                      sizeof(SimulationInfoSPH), &simulationInfo);
  commandBufferCompute->dispatch(
      static_cast<int>(std::ceil(config.getApp().simulation.particleCount / 32.0)), 1, 1);
  commandBufferCompute->end();
}
void VulkanGridFluid::submit(Stages pipelineStage,
                             const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
                             const std::optional<vk::Semaphore> &outSemaphore = std::nullopt) {
  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};
  recordCommandBuffer(pipelineStage);
  vk::SubmitInfo submitInfoCompute{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          inSemaphore.has_value() ? &inSemaphore.value() : &semaphores[currentSemaphore - 1].get(),
      .pWaitDstStageMask = waitStages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferCompute.get(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          outSemaphore.has_value() ? &outSemaphore.value() : &semaphores[currentSemaphore].get()};
  queue.submit(submitInfoCompute, nullptr);

  currentSemaphore++;
}
VulkanGridFluid::VulkanGridFluid(const Config &config, const SimulationInfoGrid &simulationInfo,
                                 std::shared_ptr<Device> device,
                                 const vk::UniqueSurfaceKHR &surface,
                                 std::shared_ptr<Swapchain> swapchain)
    : config(config), simulationInfo(simulationInfo), device(std::move(device)) {

  std::map<Stages, std::vector<PipelineLayoutBindingInfo>> bindingInfosCompute{};
  bindingInfosCompute[Stages::addSourceScalar].emplace_back(
      PipelineLayoutBindingInfo{.binding = 0,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute});
  bindingInfosCompute[Stages::addSourceScalar].emplace_back(
      PipelineLayoutBindingInfo{.binding = 1,
                                .descriptorType = vk::DescriptorType::eStorageBuffer,
                                .descriptorCount = 1,
                                .stageFlags = vk::ShaderStageFlagBits::eCompute});

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute{
      {Stages::addSourceScalar,
       {DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensity, 1},
                             .bufferSize = bufferDensity->getSize()},
        DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensitySources, 1},
                             .bufferSize = bufferDensitySources->getSize()}}}};

  bufferDensity = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(float) * simulationInfo.cellCount)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      this->device, commandPool, queue);
  bufferDensity->fill(std::vector<float>(simulationInfo.cellCount));

  bufferDensitySources = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(float) * simulationInfo.cellCount)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      this->device, commandPool, queue);
  bufferDensitySources->fill(std::vector<float>(simulationInfo.cellCount, 0.01));

  auto computePipelineBuilder =
      PipelineBuilder{this->config, this->device, swapchain}
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoGrid));

  std::map<Stages, std::string> fileNames{{Stages::addSourceScalar, "addForce.comp"}};
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "GridFluid/{}";
  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    if (stage == Stages::addSourceScalar) {
      pipelines[stage] =
          computePipelineBuilder.setLayoutBindingInfo(bindingInfosCompute[stage])
              .setComputeShaderPath(fmt::format(shaderPathTemplate.string(), fileNames[stage]))
              .addShaderMacro("TYPENAME_T float")
              .build();

      descriptorSetCompute = std::make_shared<DescriptorSet>(
          this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
      descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                bindingInfosCompute[stage]);
    }
  }

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBufferCompute = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  std::array<vk::DescriptorPoolSize, 1> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 4}};
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

  std::generate_n(semaphores.begin(), 2,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); })
}
