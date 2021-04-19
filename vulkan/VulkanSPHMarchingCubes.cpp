//
// Created by Igor Frank on 19.04.21.
//

#include "VulkanSPHMarchingCubes.h"
#include "builders/BufferBuilder.h"
#include "builders/PipelineBuilder.h"

#include "../utils/Exceptions.h"
#include "glm/gtx/component_wise.hpp"

VulkanSPHMarchingCubes::VulkanSPHMarchingCubes(const Config &config,
                                               const SimulationInfoGridFluid &simulationInfo,
                                               std::shared_ptr<Device> inDevice,
                                               const vk::UniqueSurfaceKHR &surface,
                                               std::shared_ptr<Swapchain> swapchain)
    : config(config), simulationInfo(simulationInfo), device(std::move(inDevice)) {

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBuffer = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  createBuffers();

  fillDescriptorBufferInfo();

  [[maybe_unused]] auto computePipelineBuilder =
      PipelineBuilder{this->config, this->device, swapchain}
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoGridFluid));

  std::array<vk::DescriptorPoolSize, 1> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1}};
  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets =
          [poolSize] {
            uint32_t i = 0;
            std::for_each(poolSize.begin(), poolSize.end(),
                          [&i](const auto &in) { i += in.descriptorCount; });
            return static_cast<uint32_t>(i);
          }(),
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data(),
  };
  descriptorPool = this->device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);

  std::map<Stages, std::string> fileNames{
      {Stages::ComputeColors, "colorsGrid.comp"},
  };
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "SPH/Render/{}";
  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    auto pipelineBuilder =
        computePipelineBuilder.setLayoutBindingInfo(bindingInfosCompute[stage])
            .setComputeShaderPath(fmt::format(shaderPathTemplate.string(), fileNames[stage]));

    if (descriptorBufferInfosCompute.contains(stage)) {
      pipelines[stage] = pipelineBuilder.build();
      descriptorSets[stage] = std::make_shared<DescriptorSet>(
          this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
      descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                 bindingInfosCompute[stage]);
    }
  }

  fence = device->getDevice()->createFenceUnique({});

  semaphores.resize(10);//TODO pool
  std::generate_n(semaphores.begin(), 10,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

void VulkanSPHMarchingCubes::createBuffers() {
  auto bufferSize = glm::compMul(simulationInfo.gridSize.xyz() + glm::ivec3(1));

  auto bufferBuilder = BufferBuilder()
                           .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);

  bufferGridColors = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(glm::vec2) * bufferSize),
                                              this->device, commandPool, queue);
}
void VulkanSPHMarchingCubes::fillDescriptorBufferInfo() {
  const auto descriptorBufferInfoGridColors =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridColors, 1},
                           .bufferSize = bufferGridColors->getSize()};

  descriptorBufferInfosCompute[Stages::ComputeColors] = {descriptorBufferInfoGridColors};
}
void VulkanSPHMarchingCubes::recordCommandBuffer(VulkanSPHMarchingCubes::Stages pipelineStage) {
  if (Utilities::isIn(pipelineStage, {Stages::ComputeColors})) {
    auto gridSize = config.getApp().simulationSPH.gridSize;
    [[maybe_unused]] auto gridSizeBorder = gridSize + 2;
    auto gridSizeColor = gridSize + 1;
    const auto &pipeline = pipelines[pipelineStage];
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                         .pInheritanceInfo = nullptr};

    commandBuffer->begin(beginInfo);
    commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());
    commandBuffer->bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
        &descriptorSets[pipelineStage]->getDescriptorSets()[0].get(), 0, nullptr);

    auto dispatchCount = gridSizeColor;
    commandBuffer->dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
    commandBuffer->end();
  }
}
void VulkanSPHMarchingCubes::swapBuffers(std::shared_ptr<Buffer> &buffer1,
                                         std::shared_ptr<Buffer> &buffer2) {

  throw NotImplementedException();
}
void VulkanSPHMarchingCubes::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfosCompute[stage]);
  });
}
void VulkanSPHMarchingCubes::waitFence() {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
}
void VulkanSPHMarchingCubes::submit(VulkanSPHMarchingCubes::Stages pipelineStage, const vk::Fence &submitFence,
                                    const std::optional<vk::Semaphore> &inSemaphore,
                                    const std::optional<vk::Semaphore> &outSemaphore,
                                    SubmitSemaphoreType submitSemaphoreType) {


  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};
  recordCommandBuffer(pipelineStage);
  vk::SubmitInfo submitInfoCompute{
      .waitSemaphoreCount = static_cast<uint32_t>(
          submitSemaphoreType == SubmitSemaphoreType::In && !inSemaphore.has_value() ? 0 : 1),
      .pWaitSemaphores =
      inSemaphore.has_value() ? &inSemaphore.value() : &semaphores[currentSemaphore - 1].get(),
      .pWaitDstStageMask = waitStages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer.get(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
      outSemaphore.has_value() ? &outSemaphore.value() : &semaphores[currentSemaphore].get()};
  queue.submit(submitInfoCompute, submitFence);

  currentSemaphore++;
}
vk::UniqueSemaphore VulkanSPHMarchingCubes::run(const vk::UniqueSemaphore &inSemaphore) {

  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  /**Add velocities sources*/
  submit(Stages::ComputeColors, fence.get(), inSemaphore.get(), outSemaphore, SubmitSemaphoreType::InOut);

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
}