//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>
#include <span>

#include "glm/gtc/matrix_transform.hpp"
#include "spdlog/spdlog.h"

#include "../utils/Utilities.h"
#include "../utils/saver/ScreenshotDiskSaver.h"
#include "Utils/VulkanUtils.h"
#include "VulkanCore.h"
#include "builders/ImageBuilder.h"
#include "builders/PipelineBuilder.h"
#include <glm/gtx/component_wise.hpp>
#include <pf_imgui/elements.h>
#include <plf_nanotimer.h>

void VulkanCore::initVulkan(const std::vector<Model> &modelParticle,
                            const std::vector<ParticleRecord> particles,
                            const SimulationInfo &simulationInfo) {

  this->simulationInfo = simulationInfo;
  spdlog::debug("Vulkan initialization...");
  instance = std::make_shared<Instance>(window.getWindowName(), config.getApp().DEBUG);
  createSurface();
  spdlog::debug("Created surface.");
  device = std::make_shared<Device>(instance, surface, config.getApp().DEBUG);
  queueGraphics = device->getGraphicsQueue();
  queuePresent = device->getPresentQueue();
  spdlog::debug("Queues created.");
  swapchain = std::make_shared<Swapchain>(device, surface, window);
  auto pipelineBuilder = PipelineBuilder{config, device, swapchain}
                             .setDepthFormat(findDepthFormat())
                             .setLayoutBindingInfo(bindingInfosRender)
                             .setPipelineType(PipelineType::Graphics)
                             .setVertexShaderPath(config.getVulkan().shaders.vertex)
                             .setFragmentShaderPath(config.getVulkan().shaders.fragemnt)
                             .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(int));
  pipelineGraphics = pipelineBuilder.build();
  pipelineGraphicsGrid =
      pipelineBuilder.setAssemblyInfo(vk::PrimitiveTopology::eLineStrip, true).build();
  createCommandPool();
  createDepthResources();
  framebuffers = std::make_shared<Framebuffers>(
      device, swapchain, pipelineGraphics->getRenderPass(), imageDepth->getImageView());
  createVertexBuffer(modelParticle);
  createIndexBuffer(modelParticle);
  createUniformBuffers();
  createDescriptorPool();

  bufferCellParticlePair = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(KeyValue) * config.getApp().simulation.particleCount)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eTransferSrc
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPoolGraphics, queueGraphics);
  bufferIndexes = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(
              sizeof(int)
              * Utilities::getNextPow2Number(glm::compMul(config.getApp().simulation.gridSize)))
          //.setSize(64*sizeof(int))
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPoolGraphics, queueGraphics);

  vulkanSPH = std::make_unique<VulkanSPH>(surface, device, config, swapchain, simulationInfo,
                                          particles, bufferIndexes, bufferCellParticlePair);
  vulkanGridSPH = std::make_unique<VulkanGridSPH>(surface, device, config, swapchain,
                                                  simulationInfo, vulkanSPH->getBufferParticles(),
                                                  bufferCellParticlePair, bufferIndexes);

  auto tmpBuffer = std::vector{vulkanSPH->getBufferParticles()};
  std::array<DescriptorBufferInfo, 3> descriptorBufferInfosGraphic{
      DescriptorBufferInfo{.buffer = buffersUniformMVP, .bufferSize = sizeof(UniformBufferObject)},
      DescriptorBufferInfo{.buffer = buffersUniformCameraPos, .bufferSize = sizeof(glm::vec3)},
      DescriptorBufferInfo{.buffer = tmpBuffer,
                           .bufferSize = sizeof(ParticleRecord) * particles.size()}};
  descriptorSetGraphics =
      std::make_shared<DescriptorSet>(device, swapchain->getSwapchainImageCount(),
                                      pipelineGraphics->getDescriptorSetLayout(), descriptorPool);
  descriptorSetGraphics->updateDescriptorSet(descriptorBufferInfosGraphic, bindingInfosRender);
  createOutputImage();
  spdlog::debug("Created command pool");
  initGui();
  window.setIgnorePredicate(
      [this]() { return imgui->isWindowHovered() || imgui->isKeyboardCaptured(); });
  createCommandBuffers();
  spdlog::debug("Created command buffers");
  createSyncObjects();
  spdlog::debug("Created semaphores.");
  spdlog::debug("Vulkan OK.");
  videoDiskSaver.initStream(
      "./stream.mp4",
      static_cast<unsigned int>(1.0 / static_cast<float>(config.getApp().simulation.timeStep)),
      swapchain->getExtentWidth(), swapchain->getExtentHeight());
}

void VulkanCore::run() {
  mainLoop();
  cleanup();
}

void VulkanCore::mainLoop() {
  while (!glfwWindowShouldClose(window.getWindow().get())) {
    glfwPollEvents();
    imgui->render();
    recordCommandBuffers();
    drawFrame();
    fpsCounter.newFrame();
  }

  device->getDevice()->waitIdle();
}

void VulkanCore::cleanup() {}

VulkanCore::VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos)
    : indicesSize(0), cameraPos(cameraPos), config(config), window(window) {}

void VulkanCore::createSurface() {
  VkSurfaceKHR tmpSurface;
  if (glfwCreateWindowSurface(instance->getInstance(), window.getWindow().get(), nullptr,
                              &tmpSurface)
      != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
  surface = vk::UniqueSurfaceKHR{tmpSurface, instance->getInstance()};
}
void VulkanCore::createCommandPool() {
  auto queueFamilyIndices = Device::findQueueFamilies(device->getPhysicalDevice(), surface);

  vk::CommandPoolCreateInfo commandPoolCreateInfoGraphics{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  commandPoolGraphics = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoGraphics);
}
void VulkanCore::createCommandBuffers() {
  commandBuffersGraphic.clear();
  commandBuffersGraphic.resize(framebuffers->getFramebufferImageCount());

  commandBuffersGraphic =
      device->allocateCommandBuffer(commandPoolGraphics, commandBuffersGraphic.size());

  //recordCommandBuffers();
}

void VulkanCore::recordCommandBuffers() {
  int i = 0;
  int drawType = 0;
  int drawType2 = 1;
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};
  auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
  for (auto &commandBufferGraphics : commandBuffersGraphic) {
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                         .pInheritanceInfo = nullptr};

    commandBufferGraphics->begin(beginInfo);

    std::vector<vk::ClearValue> clearValues(2);
    clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
    clearValues[1].setDepthStencil({1.0f, 0});

    vk::RenderPassBeginInfo renderPassBeginInfo{
        .renderPass = pipelineGraphics->getRenderPass(),
        .framebuffer = swapchainFramebuffers[i].get(),
        .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()};

    commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                        pipelineGraphics->getPipeline().get());
    commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
    commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0,
                                           vk::IndexType::eUint16);
    commandBufferGraphics->bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
        &descriptorSetGraphics->getDescriptorSets()[i].get(), 0, nullptr);
    drawType = 0;
    commandBufferGraphics->pushConstants(pipelineGraphics->getPipelineLayout().get(),
                                         vk::ShaderStageFlagBits::eVertex, 0, sizeof(int),
                                         &drawType);

    commandBufferGraphics->drawIndexed(indicesSize[0], config.getApp().simulation.particleCount, 0,
                                       0, 0);

    commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                        pipelineGraphicsGrid->getPipeline().get());
    drawType = 1;
    commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 2880 * sizeof(uint16_t),
                                           vk::IndexType::eUint16);
    commandBufferGraphics->pushConstants(pipelineGraphicsGrid->getPipelineLayout().get(),
                                         vk::ShaderStageFlagBits::eVertex, 0, sizeof(int),
                                         &drawType2);
    commandBufferGraphics->drawIndexed(24, 1, 0, 2880, 0);
    imgui->addToCommandBuffer(commandBufferGraphics);
    commandBufferGraphics->endRenderPass();

    if (config.getApp().outputToFile) {
      swapchain->getSwapchainImages()[i].transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::ePresentSrcKHR,
          vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eMemoryRead,
          vk::AccessFlagBits::eTransferRead);
      stagingImage->transitionImageLayout(commandBufferGraphics, vk::ImageLayout::eUndefined,
                                          vk::ImageLayout::eTransferDstOptimal, {},
                                          vk::AccessFlagBits::eTransferWrite);

      vk::ArrayWrapper1D<vk::Offset3D, 2> offset{
          std::array<vk::Offset3D, 2>{vk::Offset3D{.x = 0, .y = 0, .z = 0},
                                      vk::Offset3D{.x = swapchain->getExtentWidth(),
                                                   .y = swapchain->getExtentHeight(),
                                                   .z = 1}}};
      vk::ImageBlit imageBlitRegion{
          .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
          .srcOffsets = offset,
          .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
          .dstOffsets = offset};

      commandBufferGraphics->blitImage(
          swapchain->getSwapchainImages()[i].getRawImage(), vk::ImageLayout::eTransferSrcOptimal,
          stagingImage->getImage().get(), vk::ImageLayout::eTransferDstOptimal, 1, &imageBlitRegion,
          vk::Filter::eLinear);

      swapchain->getSwapchainImages()[i].transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::eTransferSrcOptimal,
          vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eTransferRead,
          vk::AccessFlagBits::eMemoryRead);
      stagingImage->transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::eTransferDstOptimal,
          vk::ImageLayout::eTransferSrcOptimal, {}, vk::AccessFlagBits::eTransferWrite);
      imageOutput->transitionImageLayout(commandBufferGraphics, vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eTransferDstOptimal, {},
                                         vk::AccessFlagBits::eTransferWrite);

      vk::ImageCopy imageCopyRegion{
          .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
          .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
          .extent = {.width = static_cast<uint32_t>(swapchain->getExtentWidth()),
                     .height = static_cast<uint32_t>(swapchain->getExtentHeight()),
                     .depth = 1}};

      commandBufferGraphics->copyImage(
          stagingImage->getImage().get(), vk::ImageLayout::eTransferSrcOptimal,
          imageOutput->getImage().get(), vk::ImageLayout::eTransferDstOptimal, 1, &imageCopyRegion);

      stagingImage->transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral,
          {}, vk::AccessFlagBits::eTransferWrite);
      imageOutput->transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
          vk::AccessFlagBits ::eTransferWrite, vk::AccessFlagBits::eMemoryRead);
    }
    commandBufferGraphics->end();
    ++i;
  }
}
void VulkanCore::createSyncObjects() {
  fencesImagesInFlight.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSimulation.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSort.resize(swapchain->getSwapchainImageCount());

  for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    semaphoreImageAvailable.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreRenderFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    fencesInFlight.emplace_back(
        device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
  }
}
void VulkanCore::drawFrame() {
  auto tmpfence = device->getDevice()->createFenceUnique({});
  std::array<vk::Semaphore, 1> semaphoresAfterNextImage{
      semaphoreImageAvailable[currentFrame].get()};
  std::array<vk::Semaphore, 1> semaphoresAfterRender{semaphoreRenderFinished[currentFrame].get()};
  std::array<vk::PipelineStageFlags, 1> waitStagesRender{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  std::array<vk::SwapchainKHR, 1> swapchains{swapchain->getSwapchain().get()};
  uint32_t imageindex;

  device->getDevice()->waitForFences(fencesInFlight[currentFrame].get(), VK_TRUE, UINT64_MAX);
  try {
    device->getDevice()->acquireNextImageKHR(swapchain->getSwapchain().get(), UINT64_MAX,
                                             *semaphoresAfterNextImage.begin(), tmpfence.get(),
                                             &imageindex);
  } catch (const std::exception &e) {
    recreateSwapchain();
    return;
  }
  updateUniformBuffers(imageindex);

  if (fencesImagesInFlight[imageindex].has_value())
    device->getDevice()->waitForFences(fencesImagesInFlight[imageindex].value(), VK_TRUE,
                                       UINT64_MAX);
  fencesImagesInFlight[imageindex] = fencesInFlight[currentFrame].get();

  device->getDevice()->resetFences(fencesInFlight[currentFrame].get());

  device->getDevice()->waitForFences(tmpfence.get(), VK_TRUE, UINT64_MAX);

  if (simulate || step) {
    semaphoreAfterSort[currentFrame] = vulkanGridSPH->run(semaphoreImageAvailable[currentFrame]);

    semaphoreAfterSimulation[currentFrame] = vulkanSPH->run(semaphoreAfterSort[currentFrame]);
  }

  vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = (simulate || step)
                                      ? &semaphoreAfterSimulation[currentFrame].get()
                                      : &semaphoreImageAvailable[currentFrame].get(),
                                  .pWaitDstStageMask = waitStagesRender.data(),
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &commandBuffersGraphic[imageindex].get(),
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = semaphoresAfterRender.data()};
  vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                 .pWaitSemaphores = semaphoresAfterRender.data(),
                                 .swapchainCount = 1,
                                 .pSwapchains = swapchains.data(),
                                 .pImageIndices = &imageindex,
                                 .pResults = nullptr};

  queueGraphics.submit(submitInfoRender, fencesInFlight[currentFrame].get());

  if (config.getApp().outputToFile) {
    device->getDevice()->waitForFences(fencesInFlight[currentFrame].get(), VK_TRUE, UINT64_MAX);

    vk::ImageSubresource subResource{vk::ImageAspectFlagBits::eColor, 0, 0};
    vk::SubresourceLayout subresourceLayout;
    device->getDevice()->getImageSubresourceLayout(imageOutput->getImage().get(), &subResource,
                                                   &subresourceLayout);

    auto output = imageOutput->read(device);
    videoDiskSaver.insertFrameBlocking(output, PixelFormat::RGBA);
  }

  try {
    queuePresent.presentKHR(presentInfo);
  } catch (const vk::OutOfDateKHRError &e) { recreateSwapchain(); }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  step = false;
}
void VulkanCore::recreateSwapchain() {
  window.checkMinimized();
  device->getDevice().get().waitIdle();

  swapchain->createSwapchain();
  swapchain->createImageViews();
  pipelineGraphics = PipelineBuilder{config, device, swapchain}
                         .setDepthFormat(findDepthFormat())
                         .setLayoutBindingInfo(bindingInfosRender)
                         .setPipelineType(PipelineType::Graphics)
                         .setVertexShaderPath(config.getVulkan().shaders.vertex)
                         .setFragmentShaderPath(config.getVulkan().shaders.fragemnt)
                         .build();
  framebuffers->createFramebuffers();
  createUniformBuffers();
  createDescriptorPool();
  //    createDescriptorSet();
  createCommandBuffers();
}

bool VulkanCore::isFramebufferResized() const { return framebufferResized; }

void VulkanCore::setFramebufferResized(bool resized) { VulkanCore::framebufferResized = resized; }

void VulkanCore::createVertexBuffer(const std::vector<Model> &models) {
  auto size = 0;
  auto offset = 0;
  std::for_each(models.begin(), models.end(),
                [&](const auto &model) { size += model.vertices.size(); });

  bufferVertex = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(Vertex) * size)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eVertexBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPoolGraphics, queueGraphics);

  std::for_each(models.begin(), models.end(), [&](const auto &model) {
    bufferVertex->fill(model.vertices, true, offset);
    offset += model.vertices.size() * sizeof(Vertex);
    verticesSize.emplace_back(offset);
  });
}

void VulkanCore::createIndexBuffer(const std::vector<Model> &models) {
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
      device, commandPoolGraphics, queueGraphics);
  std::for_each(models.begin(), models.end(), [&](const auto &model) {
    bufferIndex->fill(model.indices, true, offset);
    offset += model.indices.size() * sizeof(uint16_t);
    indicesSize.emplace_back(model.indices.size());
  });
  size = 0;
}

void VulkanCore::createOutputImage() {
  imageOutput = ImageBuilder()
                    .createView(false)
                    .setFormat(vk::Format::eR8G8B8A8Srgb)
                    .setHeight(swapchain->getExtentHeight())
                    .setWidth(swapchain->getExtentWidth())
                    .setProperties(vk::MemoryPropertyFlagBits::eHostVisible
                                   | vk::MemoryPropertyFlagBits::eHostCoherent)
                    .setTiling(vk::ImageTiling::eLinear)
                    .setUsage(vk::ImageUsageFlagBits::eTransferDst)
                    .build(device);
  stagingImage =
      ImageBuilder()
          .createView(false)
          .setFormat(vk::Format::eR8G8B8A8Srgb)
          .setHeight(swapchain->getExtentHeight())
          .setWidth(swapchain->getExtentWidth())
          .setProperties(vk::MemoryPropertyFlagBits::eDeviceLocal)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc)
          .build(device);
}

void VulkanCore::createUniformBuffers() {
  vk::DeviceSize size = sizeof(UniformBufferObject);
  auto builderMVP = BufferBuilder()
                        .setSize(size)
                        .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible
                                                | vk::MemoryPropertyFlagBits::eHostCoherent)
                        .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer);
  auto builderCameraPos = BufferBuilder()
                              .setSize(size)
                              .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible
                                                      | vk::MemoryPropertyFlagBits::eHostCoherent)
                              .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer);

  for ([[maybe_unused]] const auto &swapImage : swapchain->getSwapChainImageViews()) {
    buffersUniformMVP.emplace_back(
        std::make_shared<Buffer>(builderMVP, device, commandPoolGraphics, queueGraphics));
    buffersUniformCameraPos.emplace_back(
        std::make_shared<Buffer>(builderCameraPos, device, commandPoolGraphics, queueGraphics));
  }
}
void VulkanCore::updateUniformBuffers(uint32_t currentImage) {
  UniformBufferObject ubo{
      .model = glm::mat4{1.0f},
      .view = viewMatrixGetter(),
      .proj = glm::perspective(glm::radians(45.0f),
                               static_cast<float>(swapchain->getExtentWidth())
                                   / static_cast<float>(swapchain->getExtentHeight()),
                               0.01f, 1000.f)};
  ubo.proj[1][1] *= -1;
  buffersUniformMVP[currentImage]->fill(std::span(&ubo, 1), false);
  buffersUniformCameraPos[currentImage]->fill(std::span(&cameraPos, 1), false);
}
void VulkanCore::createDescriptorPool() {
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

void VulkanCore::createDepthResources() {
  imageDepth = ImageBuilder()
                   .createView(true)
                   .setImageViewAspect(vk::ImageAspectFlagBits::eDepth)
                   .setFormat(findDepthFormat())
                   .setHeight(swapchain->getExtentHeight())
                   .setWidth(swapchain->getExtentWidth())
                   .setProperties(vk::MemoryPropertyFlagBits::eDeviceLocal)
                   .setTiling(vk::ImageTiling::eOptimal)
                   .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
                   .build(device);

  imageDepth->transitionImageLayoutNow(device, commandPoolGraphics, queueGraphics,
                                       vk::ImageLayout::eUndefined,
                                       vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

vk::Format VulkanCore::findDepthFormat() {
  return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format VulkanCore::findSupportedFormat(const std::vector<vk::Format> &candidates,
                                           vk::ImageTiling tiling,
                                           const vk::FormatFeatureFlags &features) {
  for (const auto &format : candidates) {
    auto properties = device->getPhysicalDevice().getFormatProperties(format);
    if ((tiling == vk::ImageTiling::eLinear
         && (properties.linearTilingFeatures & features) == features)
        || (tiling == vk::ImageTiling::eOptimal
            && (properties.optimalTilingFeatures & features) == features))
      return format;
  }
  throw std::runtime_error("Failed to find supported format!");
}

void VulkanCore::setViewMatrixGetter(std::function<glm::mat4()> getter) {
  viewMatrixGetter = getter;
}

VulkanCore::~VulkanCore() {
  videoDiskSaver.endStream();
  spdlog::info("Avg time to SPH: {}ms.", time / static_cast<double>(steps));
}
void VulkanCore::initGui() {
  using namespace pf::ui;
  imgui = std::make_unique<ig::ImGuiGlfwVulkan>(device, instance, pipelineGraphics->getRenderPass(),
                                                surface, swapchain, window.getWindow().get(),
                                                ImGuiConfigFlags{}, toml::table{});

  auto &infoWindow = imgui->createChild<ig::Window>("info_window", "Info");
  auto &labelFPS = infoWindow.createChild<ig::Text>("text_FPS", "");
  auto &labelFrameTime = infoWindow.createChild<ig::Text>("text_FrameTime", "");

  fpsCounter.setOnNewFrameCallback([this, &labelFrameTime, &labelFPS]() {
    labelFPS.setText(fmt::format("FPS: {:.1f} AVG: {:.1f}", fpsCounter.getCurrentFPS(),
                                 fpsCounter.getAverageFPS()));
    labelFrameTime.setText(fmt::format("Frame time: {:.1f}ms, AVG: {:.1f}ms",
                                       fpsCounter.getCurrentFrameTime(),
                                       fpsCounter.getAverageFrameTime()));
  });
  auto &controlWindow = imgui->createChild<ig::Window>("control_window", "Control");
  auto &a = controlWindow.createChild<ig::Group>("aaa", "aaaaa");

  auto &controlButton = a.createChild<ig::Button>("button_control", "Start simulation");
  auto &stepButton = a.createChild<ig::Button>("button_step", "Step simulation");
  stepButton.setEnabled(pf::Enabled::No);
  controlButton.addClickListener([this, &controlButton, &stepButton]() {
    simulate = not simulate;
    if (simulate) {
      controlButton.setLabel("Pause simulation");
      stepButton.setEnabled(pf::Enabled::No);
    } else {
      controlButton.setLabel("Start simulation");
      stepButton.setEnabled(pf::Enabled::Yes);
    }
  });
  stepButton.addClickListener([this](){step = true;});
}
