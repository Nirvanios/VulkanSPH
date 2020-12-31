//
// Created by Igor Frank on 26.12.20.
//

#include "VulkanGridSPH.h"
#include <spdlog/spdlog.h>
#include <iostream>

VulkanGridSPH::VulkanGridSPH(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
                             Config config, std::shared_ptr<Swapchain> swapchain, SimulationInfo simulationInfo,
                             std::shared_ptr<Buffer> bufferParticles,
                             std::shared_ptr<Buffer> bufferCellParticlesPair,
                             std::shared_ptr<Buffer> bufferIndexes)
    : config(std::move(config)), simulationInfo(simulationInfo), device(std::move(device)),
      bufferParticles(std::move(bufferParticles)),
      bufferCellParticlePair(std::move(bufferCellParticlesPair)),
      bufferIndexes(std::move(bufferIndexes)) {

  pipeline = PipelineBuilder{this->config, this->device, swapchain}
                 .setLayoutBindingInfo(bindingInfosCompute)
                 .setPipelineType(PipelineType::Compute)
                 .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfo))
                 .setComputeShaderPath(
                     "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/GridSearch/Init.comp")
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
                           .bufferSize = this->bufferParticles->getSize()}
      };
  descriptorSetCompute = std::make_shared<DescriptorSet>(
      this->device, 1, pipeline->getDescriptorSetLayout(), descriptorPool);
  descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);

  recordCommandBuffer(pipeline);

  vulkanSort = std::make_unique<VulkanSort>(surface, this->device, this->config, swapchain,
                                            this->bufferCellParticlePair, this->bufferIndexes);
}

void printBuffer1(const std::shared_ptr<Buffer> &buff, const std::string &msg) {
  int i = 0;
  auto a = buff->read<int>();
  spdlog::info(msg);
  i = 0;
  std::for_each(a.begin(), a.end(), [&i](auto &in) {
    std::cout << in << " ";
    if (i++ == 31) {
      std::cout << "| ";
      i = 0;
    }
  });
  spdlog::info(msg + " end.");
}

vk::UniqueSemaphore VulkanGridSPH::run(const vk::UniqueSemaphore &waitSemaphore) {
  vk::Semaphore semaphoreBeforeSort = device->getDevice()->createSemaphore({});
  std::array<vk::PipelineStageFlags, 1> stageFlags{vk::PipelineStageFlagBits::eComputeShader};
  auto fence = device->getDevice()->createFence({});
  device->getDevice()->resetFences(fence);
  vk::SubmitInfo submitInfoGridInit{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &waitSemaphore.get(),
                                    .pWaitDstStageMask = stageFlags.data(),
                                    .commandBufferCount = 1,
                                    .pCommandBuffers = &commandBufferCompute.get(),
                                    .signalSemaphoreCount = 1,
                                    .pSignalSemaphores = &semaphoreBeforeSort};
  queue.submit(submitInfoGridInit, fence);

  device->getDevice()->waitForFences(fence, VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence);


  int j = 0;
  auto a = bufferCellParticlePair->read<KeyValue>();
  spdlog::info("Unsorted");
  j = 0;
  std::for_each(a.begin(), a.end(), [&j](auto &in) {
    std::cout << "k" << in.key << " v" << in.value << " ";
    if (j++ == 31) {
      std::cout << "| ";
      j = 0;
    }
  });
  spdlog::info("Unsorted end.");


  return vulkanSort->run(vk::UniqueSemaphore(semaphoreBeforeSort, this->device->getDevice().get()));
}

void VulkanGridSPH::recordCommandBuffer(const std::shared_ptr<Pipeline> &pipeline) {
  GridInfo gridInfo{.gridSize = glm::ivec4(config.getApp().simulation.gridSize, 0),
                    .gridOrigin = glm::vec4(config.getApp().simulation.gridOrigin, 0),
                    .cellSize = simulationInfo.supportRadius,
                    .particleCount =
                        static_cast<unsigned int>(config.getApp().simulation.particleCount)};

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
      static_cast<int>(std::ceil(config.getApp().simulation.particleCount / 32.0)), 1, 1);
  commandBufferCompute->end();
}
