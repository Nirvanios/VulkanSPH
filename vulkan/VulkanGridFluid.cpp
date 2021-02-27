//
// Created by Igor Frank on 12.02.21.
//

#include "VulkanGridFluid.h"
#include <glm/gtx/component_wise.hpp>
#include <magic_enum.hpp>
#include <numeric>
#include <ranges>
#include <utility>

vk::UniqueSemaphore VulkanGridFluid::run(const vk::UniqueSemaphore &semaphoreWait) {

  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  submit(Stages::addSourceScalar, fence.get(), semaphoreWait.get());

  swapBuffers(bufferDensityNew, bufferDensityOld);
  /*  for (int i = 0; i < 10; ++i) {
    simulationInfo.specificInfo = magic_enum::enum_integer(SpecificInfo::black);
    submit(Stages::diffuse, fence.get());
    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    device->getDevice()->resetFences(fence.get());

    simulationInfo.specificInfo = magic_enum::enum_integer(SpecificInfo::red);
    submit(Stages::diffuse, fence.get());
    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    device->getDevice()->resetFences(fence.get());
    //TODO BOUND
  }*/

  simulationInfo.specificInfo = magic_enum::enum_integer(SpecificInfo::black);
  submit(Stages::diffuse, fence.get());
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());

  simulationInfo.specificInfo = magic_enum::enum_integer(SpecificInfo::red);
  submit(Stages::diffuse, fence.get());

    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
  submit(Stages::boundaryHandle, fence.get());

  swapBuffers(bufferDensityNew, bufferDensityOld);

  submit(Stages::advectScalar, fence.get());

  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
  submit(Stages::boundaryHandle, fence.get(), std::nullopt, outSemaphore);

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
  /*
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
  auto gridSize = config.getApp().simulationSPH.gridSize;
  auto gridSizeBorder = gridSize + 2;
  const auto &pipeline = pipelines[pipelineStage];
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBuffer->begin(beginInfo);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());
  commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSets[pipelineStage]->getDescriptorSets()[0].get(), 0, nullptr);

  commandBuffer->pushConstants(pipeline->getPipelineLayout().get(),
                               vk::ShaderStageFlagBits::eCompute, 0,
                               sizeof(SimulationInfoGridFluid), &simulationInfo);
  auto dispatchCount = glm::ivec3(glm::ceil(simulationInfo.cellCount / 32.0), 1, 1);
  if (pipelineStage == Stages::boundaryHandle)
    dispatchCount = glm::ivec3(
        glm::ceil((2 * gridSizeBorder.x * gridSizeBorder.y + 2 * gridSizeBorder.x * gridSizeBorder.z
                   + 2 * gridSizeBorder.y * gridSizeBorder.z)
                  / 32.0),
        1, 1);
  commandBuffer->dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
  commandBuffer->end();
}
void VulkanGridFluid::submit(Stages pipelineStage, const vk::Fence submitFence,
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
VulkanGridFluid::VulkanGridFluid(const Config &config,
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

  auto computePipelineBuilder =
      PipelineBuilder{this->config, this->device, swapchain}
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoGridFluid));

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

  std::map<Stages, std::string> fileNames{{Stages::addSourceScalar, "addForce.comp"},
                                          {Stages::diffuse, "GaussSeidelRedBlack.comp"},
                                          {Stages::advectScalar, "advect.comp"},
                                          {Stages::boundaryHandle, "boundary.comp"}};
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "GridFluid/{}";
  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    if (Utilities::isIn(stage,
                        {Stages::addSourceScalar, Stages ::diffuse, Stages::advectScalar,
                         Stages::boundaryHandle})) {
      pipelines[stage] =
          computePipelineBuilder.setLayoutBindingInfo(bindingInfosCompute[stage])
              .setComputeShaderPath(fmt::format(shaderPathTemplate.string(), fileNames[stage]))
              .addShaderMacro("TYPENAME_T float")
              .build();

      descriptorSets[stage] = std::make_shared<DescriptorSet>(
          this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
      descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                 bindingInfosCompute[stage]);
    }
  }
  fence = device->getDevice()->createFenceUnique({});

  semaphores.resize(23);
  std::generate_n(semaphores.begin(), 23,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

void VulkanGridFluid::createBuffers() {
  auto cellCountBorder = glm::compMul(simulationInfo.gridSize.xyz() + glm::ivec3(2));
  auto initialDensity = std::vector<float>(cellCountBorder, 0);
  auto sources = std::vector<float>(simulationInfo.cellCount, 0);
  [[maybe_unused]] auto positionSources =
      ((simulationInfo.gridSize.z / 2) * simulationInfo.gridSize.x * simulationInfo.gridSize.y)
      + (simulationInfo.gridSize.x / 2)
      + (simulationInfo.gridSize.x * (simulationInfo.gridSize.y - 1));
  sources[positionSources] = 0.1;
  [[maybe_unused]] auto positionDensity =
      (((simulationInfo.gridSize.z + 2) / 2) * (simulationInfo.gridSize.x + 2)
       * (simulationInfo.gridSize.y + 2))
      + ((simulationInfo.gridSize.x + 2) / 2)
      + ((simulationInfo.gridSize.x + 2) * simulationInfo.gridSize.y);
  //initialDensity[positionDensity] = 1.0f;

  auto bufferBuilder = BufferBuilder()
                           .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);
  bufferDensityNew = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(float) * cellCountBorder), this->device, commandPool, queue);
  bufferDensityNew->fill(initialDensity);

  bufferDensityOld = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(float) * cellCountBorder), this->device, commandPool, queue);
  bufferDensityOld->fill(initialDensity);

  bufferDensitySources = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(float) * cellCountBorder), this->device, commandPool, queue);
  bufferDensitySources->fill(sources);

  bufferVelocitiesNew = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec4) * cellCountBorder), this->device, commandPool, queue);
  bufferVelocitiesNew->fill(std::vector<glm::vec4>(cellCountBorder, glm::vec4(0, -10, 0, 0)));

  bufferVelocitiesOld = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec4) * cellCountBorder), this->device, commandPool, queue);
  bufferVelocitiesOld->fill(std::vector<glm::vec4>(cellCountBorder, glm::vec4(0, -10, 0, 0)));
}

const std::shared_ptr<Buffer> &VulkanGridFluid::getBufferDensity() const {
  return bufferDensityNew;
}

void VulkanGridFluid::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfosCompute[stage]);
  });
}

void VulkanGridFluid::swapBuffers(std::shared_ptr<Buffer> &buffer1,
                                  std::shared_ptr<Buffer> &buffer2) {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
  buffer1.swap(buffer2);
  updateDescriptorSets();
}

void VulkanGridFluid::fillDescriptorBufferInfo() {
  descriptorBufferInfosCompute[Stages::addSourceScalar] = {
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityNew, 1},
                           .bufferSize = bufferDensityNew->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensitySources, 1},
                           .bufferSize = bufferDensitySources->getSize()}};
  descriptorBufferInfosCompute[Stages::diffuse] = {
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityNew, 1},
                           .bufferSize = bufferDensityNew->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityOld, 1},
                           .bufferSize = bufferDensityOld->getSize()}};
  descriptorBufferInfosCompute[Stages::advectScalar] = {
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityNew, 1},
                           .bufferSize = bufferDensityNew->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityOld, 1},
                           .bufferSize = bufferDensityOld->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferVelocitiesNew, 1},
                           .bufferSize = bufferVelocitiesNew->getSize()}};
  descriptorBufferInfosCompute[Stages::boundaryHandle] = {
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensityNew, 1},
                           .bufferSize = bufferDensityNew->getSize()}

  };
}
const vk::UniqueFence &VulkanGridFluid::getFenceAfterCompute() const { return fence; }
