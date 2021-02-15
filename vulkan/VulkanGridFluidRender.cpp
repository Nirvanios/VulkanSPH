//
// Created by Igor Frank on 13.02.21.
//

#include "VulkanGridFluidRender.h"

#include "builders/ImageBuilder.h"
#include "builders/RenderPassBuilder.h"
#include <utility>

#include <glm/gtx/component_wise.hpp>

vk::UniqueSemaphore VulkanGridFluidRender::draw(const vk::UniqueSemaphore &inSemaphore,
                                                unsigned int imageIndex,
                                                const vk::UniqueFence &fenceInFlight) {
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

  queue.submit(submitInfoRender, fenceInFlight.get());

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return vk::UniqueSemaphore(semaphoreAfterRender, device->getDevice().get());
}
VulkanGridFluidRender::VulkanGridFluidRender(
    Config config, const std::shared_ptr<Device> &device, const vk::UniqueSurfaceKHR &surface,
    std::shared_ptr<Swapchain> inSwapchain, const Model &model,
    const SimulationInfoGridFluid &inSimulationInfoGridFluid,
    std::shared_ptr<Buffer> inBufferDensity,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformMVP,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformCameraPos)
    : currentFrame(0), config(config), simulationInfoGridFluid(inSimulationInfoGridFluid),
      device(device), swapchain(std::move(inSwapchain)),
      buffersUniformMVP(std::move(inBuffersUniformMVP)),
      buffersUniformCameraPos(std::move(inBuffersUniformCameraPos)),
      bufferDensity(std::move(inBufferDensity)), queue(device->getGraphicsQueue()) {

  pipeline = PipelineBuilder{config, device, swapchain}
                 .setLayoutBindingInfo(bindingInfosRender)
                 .setPipelineType(PipelineType::Graphics)
                 .setVertexShaderPath(config.getVulkan().shaderFolder / "GridFluid/shader.vert")
                 .setFragmentShaderPath(config.getVulkan().shaderFolder / "GridFluid/shader.frag")
                 .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(SimulationInfoGridFluid))
                 .setBlendEnabled(true)
                 .addRenderPass("toSwapchain",
                                RenderPassBuilder{device}
                                    .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                                    .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                                    .build())
                 .build();

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  commandPool = this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);

  createDepthResources();

  framebuffersSwapchain = std::make_shared<Framebuffers>(device, swapchain->getSwapchainImages(),
                                                         pipeline->getRenderPass("toSwapchain"),
                                                         imageDepth->getImageView());

  commandBuffers.resize(framebuffersSwapchain->getFramebufferImageCount());
  commandBuffers = device->allocateCommandBuffer(commandPool, commandBuffers.size());

  createVertexBuffer({model});
  createIndexBuffer({model});

  std::array<DescriptorBufferInfo, 3> descriptorBufferInfos{
      DescriptorBufferInfo{.buffer = buffersUniformMVP, .bufferSize = sizeof(UniformBufferObject)},
      DescriptorBufferInfo{.buffer = buffersUniformCameraPos, .bufferSize = sizeof(glm::vec3)},
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDensity, 1},
                           .bufferSize = bufferDensity->getSize()}};

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
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};
  auto &swapchainFramebuffers = framebuffersSwapchain->getFramebuffers();
  const auto &commandBufferGraphics = commandBuffers[imageIndex];

  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferGraphics->begin(beginInfo);

  std::vector<vk::ClearValue> clearValues(2);
  clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
  clearValues[1].setDepthStencil({1.0f, 0});

  vk::RenderPassBeginInfo renderPassBeginInfo{
      .renderPass = pipeline->getRenderPass(""),
      .framebuffer = swapchainFramebuffers[imageIndex].get(),
      .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()};

  commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
  commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                      pipeline->getPipeline().get());
  commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
  commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0, vk::IndexType::eUint16);
  commandBufferGraphics->pushConstants(pipeline->getPipelineLayout().get(),
                                       vk::ShaderStageFlagBits::eVertex, 0,
                                       sizeof(SimulationInfoGridFluid), &simulationInfoGridFluid);

  commandBufferGraphics->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSet->getDescriptorSets()[imageIndex].get(), 0, nullptr);

  commandBufferGraphics->drawIndexed(indicesSizes[0],
                                     glm::compMul(config.getApp().simulationSPH.gridSize), 0, 0, 0);
  imgui->addToCommandBuffer(commandBufferGraphics);
  commandBufferGraphics->endRenderPass();

  commandBufferGraphics->end();
}
void VulkanGridFluidRender::setImgui(std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> &inImgui) {
  VulkanGridFluidRender::imgui = inImgui;
}
