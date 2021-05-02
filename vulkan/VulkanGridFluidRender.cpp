//
// Created by Igor Frank on 13.02.21.
//

#include "VulkanGridFluidRender.h"

#include "builders/ImageBuilder.h"
#include "builders/RenderPassBuilder.h"
#include <utility>

#include <glm/gtx/component_wise.hpp>

vk::UniqueSemaphore VulkanGridFluidRender::draw(const vk::UniqueSemaphore &inSemaphore,
                                                unsigned int imageIndex) {
  auto tmpfence = device->getDevice()->createFenceUnique({});

  vk::Semaphore semaphoreAfterRender = device->getDevice()->createSemaphore({});

  std::array<vk::PipelineStageFlags, 1> waitStagesRender{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  //device->getDevice()->waitForFences(fenceInFlight.get(), VK_TRUE, UINT64_MAX);

  recordCommandBuffers(imageIndex);

  vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &inSemaphore.get(),
                                  .pWaitDstStageMask = waitStagesRender.data(),
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &commandBuffers[imageIndex].get(),
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = &semaphoreAfterRender};

  queue.submit(submitInfoRender, nullptr);

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return vk::UniqueSemaphore(semaphoreAfterRender, device->getDevice().get());
}
VulkanGridFluidRender::VulkanGridFluidRender(
    Config config, const std::shared_ptr<Device> &device, const vk::UniqueSurfaceKHR &surface,
    std::shared_ptr<Swapchain> inSwapchain, const Model &model,
    const SimulationInfoGridFluid &inSimulationInfoGridFluid,
    std::shared_ptr<Buffer> inBufferDensity,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformMVP,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformCameraPos,
    std::shared_ptr<Buffer> inBufferTags)
    : currentFrame(0), config(config), simulationInfoGridFluid(inSimulationInfoGridFluid),
      device(device), swapchain(std::move(inSwapchain)),
      buffersUniformMVP(std::move(inBuffersUniformMVP)),
      buffersUniformCameraPos(std::move(inBuffersUniformCameraPos)),
      bufferDensity(std::move(inBufferDensity)), bufferTags(std::move(inBufferTags)),
      queue(device->getGraphicsQueue()) {

  pipeline =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfosRender)
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(config.getVulkan().shaderFolder / "GridFluid/Render/shader.vert")
          .setFragmentShaderPath(config.getVulkan().shaderFolder / "GridFluid/Render/shader.frag")
          .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(SimulationInfoGridFluid))
          .setBlendEnabled(true)
          .setDepthTestEnabled(false)
          .addRenderPass(
              "toSwapchain",
              RenderPassBuilder{device}
                  .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                  .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                  .setColorAttachementFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .build())
          .build();

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);

  createDepthResources();

  /*  framebuffersSwapchain = std::make_shared<Framebuffers>(device, swapchain->getSwapchainImages(),
                                                         pipeline->getRenderPass("toSwapchain"),
                                                         imageDepth->getImageView());*/

  commandBuffers.resize(swapchain->getSwapchainImageCount());
  commandBuffers = device->allocateCommandBuffer(commandPool, commandBuffers.size());

  createVertexBuffer({model});
  createIndexBuffer({model});
  createUniformBuffers();

  descriptorBufferInfos = {
      DescriptorBufferInfo{.buffer = buffersUniformMVP, .bufferSize = sizeof(UniformBufferObject)},
      DescriptorBufferInfo{.buffer = buffersUniformCameraPos, .bufferSize = sizeof(glm::vec3)},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensity, 1},
                           .bufferSize = bufferDensity->getSize()},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferTags, 1},
                           .bufferSize = bufferTags->getSize()},
      DescriptorBufferInfo{.buffer = buffersUniformAngles,
                           .bufferSize = buffersUniformAngles[0]->getSize()}};

  createDescriptorPool();

  descriptorSet =
      std::make_shared<DescriptorSet>(device, swapchain->getSwapchainImageCount(),
                                      pipeline->getDescriptorSetLayout(), descriptorPool);
  descriptorSet->updateDescriptorSet(descriptorBufferInfos, bindingInfosRender);
}

void VulkanGridFluidRender::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                             .descriptorCount =
                                 static_cast<uint32_t>(swapchain->getSwapchainImageCount()) * 2},
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
  descriptorPool = device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);
}

void VulkanGridFluidRender::createDepthResources() {
  auto builder = ImageBuilder()
                     .createView(true)
                     .setImageViewAspect(vk::ImageAspectFlagBits::eDepth)
                     .setFormat(VulkanUtils::findDepthFormat(device))
                     .setHeight(swapchain->getExtentHeight())
                     .setWidth(swapchain->getExtentWidth())
                     .setProperties(vk::MemoryPropertyFlagBits::eDeviceLocal)
                     .setTiling(vk::ImageTiling::eOptimal)
                     .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

  imageDepth = builder.build(device);

  imageDepth->transitionImageLayoutNow(device, commandPool, queue, vk::ImageLayout::eUndefined,
                                       vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

void VulkanGridFluidRender::createVertexBuffer(const std::vector<Model> &models) {
  auto size = 0;
  auto offset = 0;
  auto offsetCount = 0;
  std::for_each(models.begin(), models.end(),
                [&](const auto &model) { size += model.vertices.size(); });

  bufferVertex = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(Vertex) * size)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eVertexBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPool, queue);

  std::for_each(models.begin(), models.end(), [&](const auto &model) {
    verticesCountOffset.emplace_back(offsetCount);
    bufferVertex->fill(model.vertices, true, offset);
    offset += model.vertices.size() * sizeof(Vertex);
    offsetCount += model.vertices.size();
  });
}

void VulkanGridFluidRender::createIndexBuffer(const std::vector<Model> &models) {
  auto size = 0;
  auto offset = 0;
  std::for_each(models.begin(), models.end(),
                [&](const auto &model) { size += model.indices.size(); });
  bufferIndex = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(uint16_t) * size)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eIndexBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPool, queue);
  std::for_each(models.begin(), models.end(), [&](const auto &model) {
    indicesByteOffsets.emplace_back(offset);
    indicesSizes.emplace_back(model.indices.size());
    bufferIndex->fill(model.indices, true, offset);
    offset += model.indices.size() * sizeof(uint16_t);
  });
  size = 0;
}

void VulkanGridFluidRender::recordCommandBuffers(unsigned int imageIndex) {
  const auto &commandBufferGraphics = commandBuffers[imageIndex];

  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferGraphics->begin(beginInfo);

  recordRenderpass(imageIndex, commandBufferGraphics);

  commandBufferGraphics->end();
}
void VulkanGridFluidRender::recordRenderpass(unsigned int imageIndex,
                                             const vk::UniqueCommandBuffer &commandBuffer) {
  const auto &swapchainFramebuffers = framebuffersSwapchain->getFramebuffers();
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};

  std::vector<vk::ClearValue> clearValues(2);
  clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
  clearValues[1].setDepthStencil({1.0f, 0});

  vk::RenderPassBeginInfo renderPassBeginInfo{
      .renderPass = pipeline->getRenderPass(""),
      .framebuffer = swapchainFramebuffers[imageIndex].get(),
      .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()};

  commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->getPipeline().get());
  commandBuffer->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
  commandBuffer->bindIndexBuffer(bufferIndex->getBuffer().get(), 0, vk::IndexType::eUint16);
  commandBuffer->pushConstants(pipeline->getPipelineLayout().get(),
                               vk::ShaderStageFlagBits::eVertex, 0, sizeof(SimulationInfoGridFluid),
                               &simulationInfoGridFluid);

  commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSet->getDescriptorSets()[imageIndex].get(), 0, nullptr);

  commandBuffer->drawIndexed(indicesSizes[0], glm::compMul(config.getApp().simulationSPH.gridSize),
                             0, 0, 0);
  commandBuffer->endRenderPass();
}

void VulkanGridFluidRender::updateDensityBuffer(std::shared_ptr<Buffer> densityBufferNew) {
  bufferDensity = std::move(densityBufferNew);
  descriptorSet->updateDescriptorSet(descriptorBufferInfos, bindingInfosRender);
}
void VulkanGridFluidRender::rebuildPipeline(bool clearBeforeDraw) {
  pipeline =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfosRender)
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(config.getVulkan().shaderFolder / "GridFluid/Render/shader.vert")
          .setFragmentShaderPath(config.getVulkan().shaderFolder / "GridFluid/Render/shader.frag")
          .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(SimulationInfoGridFluid))
          .setBlendEnabled(true)
          .setDepthTestEnabled(false)
          .addRenderPass(
              "toSwapchain",
              RenderPassBuilder{device}
                  .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                  .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                  .setColorAttachementFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setColorAttachmentLoadOp(clearBeforeDraw ? vk::AttachmentLoadOp::eClear
                                                            : vk::AttachmentLoadOp::eLoad)
                  .build())
          .build();

  descriptorSet =
      std::make_shared<DescriptorSet>(device, swapchain->getSwapchainImageCount(),
                                      pipeline->getDescriptorSetLayout(), descriptorPool);
  descriptorSet->updateDescriptorSet(descriptorBufferInfos, bindingInfosRender);
}
void VulkanGridFluidRender::setFramebuffersSwapchain(
    const std::shared_ptr<Framebuffers> &framebuffer) {
  framebuffersSwapchain = framebuffer;
}
void VulkanGridFluidRender::createUniformBuffers() {

  auto builder = BufferBuilder()
                     .setSize(sizeof(float) * 3)
                     .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible
                                             | vk::MemoryPropertyFlagBits::eHostCoherent)
                     .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer
                                    | vk::BufferUsageFlagBits::eTransferDst);

  for ([[maybe_unused]] const auto &swapImage : swapchain->getSwapChainImageViews()) {
    buffersUniformAngles.emplace_back(
        std::make_shared<Buffer>(builder, device, commandPool, queue));
  }
}
void VulkanGridFluidRender::updateUniformBuffer(unsigned int imageIndex, float yaw, float pitch) {

  auto data = std::vector<glm::vec3>{glm::vec3{yaw, pitch, 0}};
  buffersUniformAngles[imageIndex]->fill(data);
}
