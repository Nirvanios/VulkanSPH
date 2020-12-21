//
// Created by Igor Frank on 20.12.20.
//

#include "VulkanSort.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <utility>

void printBuffer(const std::shared_ptr<Buffer> &buff, const std::string &msg) {
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

VulkanSort::VulkanSort(const vk::UniqueSurfaceKHR &surface, std::shared_ptr<Device> device,
                       const Config &config, std::shared_ptr<Swapchain> swapchain,
                       std::shared_ptr<Buffer> bufferToSort)
    : surface(surface), device(std::move(device)), bufferBins(std::move(bufferToSort)) {

  auto computePipelineBuilder =
      PipelineBuilder{config, this->device, std::move(swapchain)}
          .setLayoutBindingInfo(bindingInfosCompute)
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfo));
  pipelinesSort.emplace_back(
      computePipelineBuilder
          .setComputeShaderPath(
              "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/count sort/Count.comp")
          .build());
  pipelinesSort.emplace_back(
      computePipelineBuilder
          .setComputeShaderPath(
              "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/count sort/Upsweep.comp")
          .build());
  pipelinesSort.emplace_back(
      computePipelineBuilder
          .setComputeShaderPath(
              "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/count sort/Downsweep.comp")
          .build());
  pipelinesSort.emplace_back(
      computePipelineBuilder
          .setComputeShaderPath(
              "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/count sort/addSums.comp")
          .build());
  pipelinesSort.emplace_back(
      computePipelineBuilder
          .setComputeShaderPath(
              "/home/aka/CLionProjects/VulkanSPH/shaders/SPH/count sort/CreateSorted.comp")
          .build());

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandBuffer = std::move(this->device->allocateCommandBuffer(commandPool, 1)[0]);
  queue = this->device->getComputeQueue();

  buffersUniformSort = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(int) * 2)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible
                                  | vk::MemoryPropertyFlagBits::eHostCoherent)
          .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer),
      this->device, commandPool, queue);
  auto builder = BufferBuilder()
      .setSize(bufferBins->getSize())
      .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                     | vk::BufferUsageFlagBits::eTransferSrc
                     | vk::BufferUsageFlagBits::eStorageBuffer)
      .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);

  bufferBinsSorted = std::make_shared<Buffer>(builder, this->device, commandPool, queue);
  bufferCounter = std::make_shared<Buffer>(builder.setSize(sizeof(int) * 256), this->device,
                                           commandPool, queue);
  bufferCounter->fill(std::array<int, 256>{});
  bufferSums =
      std::make_shared<Buffer>(builder.setSize(64), this->device, commandPool, queue);

  std::array<vk::DescriptorPoolSize, 2> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1},
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

  std::array<DescriptorBufferInfo, 5> descriptorBufferInfosCompute{
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&buffersUniformSort, 1},
                           .bufferSize = sizeof(int) * 2},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferBins, 1},
                           .bufferSize = sizeof(KeyValue) * 512},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferBinsSorted, 1},
                           .bufferSize = sizeof(KeyValue) * 512},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferCounter, 1},
                           .bufferSize = sizeof(int) * 256},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferSums, 1},
                           .bufferSize = sizeof(int) * 16}};

  descriptorSet = std::make_shared<DescriptorSet>(
      this->device, 1, pipelinesSort[0]->getDescriptorSetLayout(), descriptorPool);
  descriptorSet->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);

  fence = this->device->getDevice()->createFenceUnique({});
}

vk::UniqueSemaphore VulkanSort::run(const vk::UniqueSemaphore &semaphoreWait) {

  std::array<vk::Semaphore, 1> semaphoreInput{semaphoreWait.get()};
  auto semaphoreOut = device->getDevice()->createSemaphore({});

  buffersUniformSort->fill(std::array<int, 2>{512, 0}, false);

  int j = 0;
  auto a = bufferBins->read<KeyValue>();
  spdlog::info("Unsorted");
  j = 0;
  std::for_each(a.begin(), a.end(), [&j](auto &in) {
    std::cout << in << " ";
    if (j++ == 31) {
      std::cout << "| ";
      j = 0;
    }
  });
  spdlog::info("Unsorted end.");

  device->getDevice()->resetFences(fence.get());
  recordCommandBuffersCompute(pipelinesSort[0], 16);
  vk::SubmitInfo submitInfoCompute{.waitSemaphoreCount = 1,
                                   .pWaitSemaphores = semaphoreInput.data(),
                                   .pWaitDstStageMask = waitStagesCompute.data(),
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &commandBuffer.get(),
                                   .signalSemaphoreCount = 0};
  queue.submit(submitInfoCompute, fence.get());

  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());

  printBuffer(bufferCounter, "counter");

  submitInfoCompute.waitSemaphoreCount = 0;
  submitInfoCompute.signalSemaphoreCount = 0;
  recordCommandBuffersCompute(pipelinesSort[1], 8);
  submitInfoCompute.pCommandBuffers = &commandBuffer.get();
  for (int i = 0; i < 8; i++) {
    buffersUniformSort->fill(std::array<int, 2>{256, i}, false);
    queue.submit(submitInfoCompute, fence.get());
    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    device->getDevice()->resetFences(fence.get());
  }

  recordCommandBuffersCompute(pipelinesSort[2], 8);
  submitInfoCompute.pCommandBuffers = &commandBuffer.get();
  for (int i = 7; i >= 0; i--) {
    buffersUniformSort->fill(std::array<int, 2>{256, i}, false);
    queue.submit(submitInfoCompute, fence.get());
    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    device->getDevice()->resetFences(fence.get());
  }

  printBuffer(bufferCounter, "Prefix sum");

  /*  recordCommandBuffersCompute(pipelinesSort[3]);
  submitInfoCompute.pCommandBuffers = &commandBuffersCompute[imageindex].get();
  submitInfoCompute.waitSemaphoreCount = 0;
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = semaphoresAfterComputeMassDensity.data();
  queueCompute.submit(submitInfoCompute, fence.get());

  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());*/

  recordCommandBuffersCompute(pipelinesSort[4], 16);
  buffersUniformSort->fill(std::array<int, 2>{512, 0}, false);
  submitInfoCompute.pCommandBuffers = &commandBuffer.get();
  submitInfoCompute.waitSemaphoreCount = 0;
  submitInfoCompute.signalSemaphoreCount = 1;
  submitInfoCompute.pSignalSemaphores = &semaphoreOut;
  queue.submit(submitInfoCompute, fence.get());

  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());

    auto b = bufferBinsSorted->read<KeyValue>();
    spdlog::info("Sorted");
    std::for_each(b.begin(), b.end(), [&j](auto &in){
      std::cout << in << " ";
      if(j++ == 31){ std::cout << "| "; j = 0;}
    });
  spdlog::info("Sorted end.");

  return vk::UniqueSemaphore(semaphoreOut);
}

void VulkanSort::recordCommandBuffersCompute(const std::shared_ptr<Pipeline> &pipeline,
                                             int dispatchCount) {
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBuffer->begin(beginInfo);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());

  commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                    pipeline->getPipelineLayout().get(), 0, 1,
                                    &descriptorSet->getDescriptorSets()[0].get(), 0, nullptr);

  commandBuffer->dispatch(static_cast<int>(dispatchCount), 1, 1);

  commandBuffer->end();
}
