//
// Created by Igor Frank on 23.03.21.
//

#include "VulkanGridFluidSPHCoupling.h"
#include "../utils/Exceptions.h"

#include <glm/gtx/component_wise.hpp>
VulkanGridFluidSPHCoupling::VulkanGridFluidSPHCoupling(const Config &config,
                                                       const GridInfo &inGridInfo,
                                                       std::shared_ptr<Device> inDevice,
                                                       const vk::UniqueSurfaceKHR &surface,
                                                       std::shared_ptr<Swapchain> swapchain,
                                                       std::shared_ptr<Buffer> inBufferIndexes)
    : config(config), gridInfo(inGridInfo), device(std::move(inDevice)), bufferIndexes(std::move(inBufferIndexes)) {

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBuffer = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  fillDescriptorBufferInfo();

  auto computePipelineBuilder =
      PipelineBuilder{this->config, this->device, swapchain}
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoGridFluid));

  std::array<vk::DescriptorPoolSize, 1> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 8}};
  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets =
          [poolSize] {
            uint32_t i = 0;
            std::for_each(poolSize.begin(), poolSize.end(),
                          [&i](const auto &in) { i += in.descriptorCount; });
            return static_cast<uint32_t>(20);
          }(),
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data(),
  };
  descriptorPool = this->device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);

  std::map<Stages, std::string> fileNames{{Stages::tag, "TagCells.comp"}};
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "Evaporation/{}";
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

  semaphores.resize(3);//TODO pool
  std::generate_n(semaphores.begin(), 3,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

vk::UniqueSemaphore VulkanGridFluidSPHCoupling::run(const vk::UniqueSemaphore &semaphoreWait) {
  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  submit(Stages::tag, fence.get(), semaphoreWait.get(), outSemaphore);
  waitFence();
  [[maybe_unused]] auto ind = bufferIndexes->read<CellInfo>();

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
}
const vk::UniqueFence &VulkanGridFluidSPHCoupling::getFenceAfterCompute() const { return fence; }
void VulkanGridFluidSPHCoupling::submit(VulkanGridFluidSPHCoupling::Stages pipelineStage,
                                        const vk::Fence submitFence,
                                        const std::optional<vk::Semaphore> &inSemaphore,
                                        const std::optional<vk::Semaphore> &outSemaphore) {

  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};
  recordCommandBuffer(pipelineStage);
  vk::SubmitInfo submitInfoCompute{
      .waitSemaphoreCount = 1,
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
void VulkanGridFluidSPHCoupling::recordCommandBuffer(
    VulkanGridFluidSPHCoupling::Stages pipelineStage) {

  const auto &pipeline = pipelines[pipelineStage];
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBuffer->begin(beginInfo);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());
  commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSets[pipelineStage]->getDescriptorSets()[0].get(), 0, nullptr);

  commandBuffer->pushConstants(pipeline->getPipelineLayout().get(),
                               vk::ShaderStageFlagBits::eCompute, 0, sizeof(GridInfo), &gridInfo);
  auto dispatchCount = glm::ivec3(glm::ceil(glm::compMul(gridInfo.gridSize.xyz()) / 32.0), 1, 1);
  commandBuffer->dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
  commandBuffer->end();
}

void VulkanGridFluidSPHCoupling::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfosCompute[stage]);
  });
}
void VulkanGridFluidSPHCoupling::fillDescriptorBufferInfo() {
  const auto descriptorBufferInfoValuesNew =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferIndexes, 1},
                           .bufferSize = bufferIndexes->getSize()};

  descriptorBufferInfosCompute[Stages::tag] = {descriptorBufferInfoValuesNew};
}
void VulkanGridFluidSPHCoupling::createBuffers() { throw NotImplementedException(); }
void VulkanGridFluidSPHCoupling::waitFence() {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
}
