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
#include "builders/RenderPassBuilder.h"
#include "enums.h"
#include <glm/gtx/component_wise.hpp>
#include <magic_enum.hpp>
#include <pf_imgui/elements.h>
#include <plf_nanotimer.h>

#include "../Third Party/imgui/imgui_impl_vulkan.h"

void VulkanCore::initVulkan(const std::vector<Model> &modelParticle,
                            const std::vector<ParticleRecord> particles,
                            const SimulationInfoSPH &simulationInfoSPH,
                            const SimulationInfoGridFluid &simulationInfoGridFluid) {

  this->simulationInfo = simulationInfoSPH;
  spdlog::debug("Vulkan initialization...");
  instance = std::make_shared<Instance>(window.getWindowName(), config.getApp().DEBUG);
  createSurface();
  spdlog::debug("Created surface.");
  device = std::make_shared<Device>(instance, surface, config.getApp().DEBUG);
  queueGraphics = device->getGraphicsQueue();
  queuePresent = device->getPresentQueue();
  spdlog::debug("Queues created.");
  swapchain = std::make_shared<Swapchain>(device, surface, window);
  auto pipelineBuilder =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfosRender)
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(config.getVulkan().shaderFolder / "SPH/shader.vert")
          .setFragmentShaderPath(config.getVulkan().shaderFolder / "SPH/shader.frag")
          .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(DrawInfo))
          .addRenderPass("toSwapchain",
                         RenderPassBuilder{device}
                             .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                             .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                             .build())
          .addRenderPass("toTexture",
                         RenderPassBuilder{device}
                             .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                             .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                             .build());
  pipelineGraphics = pipelineBuilder.build();
  pipelineGraphicsGrid =
      pipelineBuilder.setAssemblyInfo(vk::PrimitiveTopology::eLineStrip, true).build();
  createCommandPool();
  createDepthResources();
  createTextureImages();
  framebuffersSwapchain = std::make_shared<Framebuffers>(
      device, swapchain->getSwapchainImages(), pipelineGraphics->getRenderPass("toSwapchain"),
      imageDepthSwapchain->getImageView());
  framebuffersTexture = std::make_shared<Framebuffers>(device, imageColorTexture,
                                                       pipelineGraphics->getRenderPass("toTexture"),
                                                       imageDepthSwapchain->getImageView());
  createVertexBuffer(modelParticle);
  createIndexBuffer(modelParticle);
  createUniformBuffers();
  createDescriptorPool();

  bufferCellParticlePair = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(sizeof(KeyValue) * config.getApp().simulationSPH.particleCount)
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eTransferSrc
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPoolGraphics, queueGraphics);
  bufferIndexes = std::make_shared<Buffer>(
      BufferBuilder()
          .setSize(
              sizeof(CellInfo)
              * Utilities::getNextPow2Number(glm::compMul(config.getApp().simulationSPH.gridSize)))
          .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                         | vk::BufferUsageFlagBits::eStorageBuffer)
          .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
      device, commandPoolGraphics, queueGraphics);

  vulkanSPH = std::make_unique<VulkanSPH>(surface, device, config, swapchain, simulationInfoSPH,
                                          particles, bufferIndexes, bufferCellParticlePair);
  vulkanGridSPH = std::make_unique<VulkanGridSPH>(
      surface, device, config, swapchain, simulationInfoSPH, vulkanSPH->getBufferParticles(),

      bufferCellParticlePair, bufferIndexes);
  vulkanGridFluid = std::make_unique<VulkanGridFluid>(config, simulationInfoGridFluid, device,
                                                      surface, swapchain);
  vulkanGridFluidRender = std::make_unique<VulkanGridFluidRender>(
      config, device, surface, swapchain,
      Utilities::loadModelFromObj(config.getApp().simulationGridFluid.cellModel),
      simulationInfoGridFluid, vulkanGridFluid->getBufferDensity(), buffersUniformMVP,
      buffersUniformCameraPos);
  vulkanGridFluidRender->setFramebuffersSwapchain(framebuffersSwapchain);

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
  textureSampler = std::make_shared<TextureSampler>(device);
  initGui();
  vulkanGridFluidRender->setImgui(imgui);
  window.setIgnorePredicate(
      [this]() { return imgui->isWindowHovered() || imgui->isKeyboardCaptured(); });
  createCommandBuffers();
  spdlog::debug("Created command buffers");
  createSyncObjects();
  spdlog::debug("Created semaphores.");
  spdlog::debug("Vulkan OK.");
  videoDiskSaver.initStream(
      "./stream.mp4",
      static_cast<unsigned int>(1.0 / static_cast<float>(config.getApp().simulationSPH.timeStep)),
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
    //recordCommandBuffers(0);
    drawFrame();
    fpsCounter.newFrame();
  }

  device->getDevice()->waitIdle();
}

void VulkanCore::cleanup() {}

VulkanCore::VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos)
    : indicesByteOffsets(0), cameraPos(cameraPos), config(config), window(window) {}

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
  commandBuffersGraphic.resize(framebuffersSwapchain->getFramebufferImageCount());

  commandBuffersGraphic =
      device->allocateCommandBuffer(commandPoolGraphics, commandBuffersGraphic.size());

  //recordCommandBuffers();
}

void VulkanCore::recordCommandBuffers(uint32_t imageIndex) {
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};
  auto &swapchainFramebuffers = framebuffersSwapchain->getFramebuffers();
  auto &textureFramebuffers = framebuffersTexture->getFramebuffers();
  auto &commandBufferGraphics = commandBuffersGraphic[imageIndex];
  DrawInfo drawInfo{.drawType = magic_enum::enum_integer(DrawType::Particles),
                    .visualization = magic_enum::enum_integer(Visualization::None)};
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferGraphics->begin(beginInfo);

  std::vector<vk::ClearValue> clearValues(2);
  clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
  clearValues[1].setDepthStencil({1.0f, 0});

  vk::RenderPassBeginInfo renderPassBeginInfo{
      .renderPass = pipelineGraphics->getRenderPass(""),
      .framebuffer = swapchainFramebuffers[imageIndex].get(),
      .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()};

  commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
  commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                      pipelineGraphics->getPipeline().get());
  commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
  commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0, vk::IndexType::eUint16);
  commandBufferGraphics->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
      &descriptorSetGraphics->getDescriptorSets()[imageIndex].get(), 0, nullptr);
  drawInfo.drawType = magic_enum::enum_integer(DrawType::Particles);
  commandBufferGraphics->pushConstants(pipelineGraphics->getPipelineLayout().get(),
                                       vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                       &drawInfo);

  commandBufferGraphics->drawIndexed(indicesSizes[0], config.getApp().simulationSPH.particleCount,
                                     0, 0, 0);

  commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                      pipelineGraphicsGrid->getPipeline().get());
  drawInfo.drawType = magic_enum::enum_integer(DrawType::Grid);
  commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), indicesByteOffsets[1],
                                         vk::IndexType::eUint16);
  commandBufferGraphics->pushConstants(pipelineGraphicsGrid->getPipelineLayout().get(),
                                       vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                       &drawInfo);
  commandBufferGraphics->drawIndexed(indicesSizes[1], 1, 0, verticesCountOffset[1], 0);
  imgui->addToCommandBuffer(commandBufferGraphics);
  commandBufferGraphics->endRenderPass();

  renderPassBeginInfo.renderPass = pipelineGraphics->getRenderPass("toTexture");
  renderPassBeginInfo.framebuffer = textureFramebuffers[imageIndex].get();
  drawInfo.visualization = magic_enum::enum_integer(textureVisualization);

  commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
  commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                      pipelineGraphics->getPipeline().get());
  commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
  commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0, vk::IndexType::eUint16);
  commandBufferGraphics->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
      &descriptorSetGraphics->getDescriptorSets()[imageIndex].get(), 0, nullptr);

  drawInfo.drawType = magic_enum::enum_integer(DrawType::Particles);
  commandBufferGraphics->pushConstants(pipelineGraphics->getPipelineLayout().get(),
                                       vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                       &drawInfo);

  commandBufferGraphics->drawIndexed(indicesSizes[0], config.getApp().simulationSPH.particleCount,
                                     0, 0, 0);
  commandBufferGraphics->endRenderPass();

  imageColorTexture[imageIndex]->transitionImageLayout(
      commandBufferGraphics,
      simulationType == SimulationType::Combined ? vk::ImageLayout::eColorAttachmentOptimal
                                                 : vk::ImageLayout::ePresentSrcKHR,
      vk::ImageLayout::eGeneral, vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);

  if (config.getApp().outputToFile) {
        swapchain->getSwapchainImages()[imageIndex]->transitionImageLayout(
        commandBufferGraphics, vk::ImageLayout::ePresentSrcKHR,
        vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eMemoryRead,
        vk::AccessFlagBits::eTransferRead);

    imageOutput->transitionImageLayout(commandBufferGraphics, vk::ImageLayout::eUndefined,
                                       vk::ImageLayout::eTransferDstOptimal, {},
                                       vk::AccessFlagBits::eTransferWrite);

    vk::ImageCopy imageCopyRegion{
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .extent = {.width = static_cast<uint32_t>(swapchain->getExtentWidth()),
                   .height = static_cast<uint32_t>(swapchain->getExtentHeight()),
                   .depth = 1}};

    commandBufferGraphics->copyImage(swapchain->getSwapchainImages()[imageIndex]->getRawImage(),
                                     vk::ImageLayout::eTransferSrcOptimal,
                                     imageOutput->getImage().get(),
                                     vk::ImageLayout::eTransferDstOptimal, 1, &imageCopyRegion);

    swapchain->getSwapchainImages()[imageIndex]->transitionImageLayout(
        commandBufferGraphics, vk::ImageLayout::eTransferSrcOptimal,
        vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits::eTransferRead,
        vk::AccessFlagBits::eMemoryRead);

    imageOutput->transitionImageLayout(
        commandBufferGraphics, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
        vk::AccessFlagBits ::eTransferWrite, vk::AccessFlagBits::eMemoryRead);
  }
  commandBufferGraphics->end();
}

void VulkanCore::createSyncObjects() {
  fencesImagesInFlight.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSimulationSPH.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterMassDensity.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterForces.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSimulationGrid.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSort.resize(swapchain->getSwapchainImageCount());

  for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    semaphoreImageAvailable.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreRenderFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreBetweenRender.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    fencesInFlight.emplace_back(
        device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
  }
}
void VulkanCore::drawFrame() {
  auto tmpfence = device->getDevice()->createFenceUnique({});
  std::array<vk::Semaphore, 1> semaphoresAfterNextImage{
      semaphoreImageAvailable[currentFrame].get()};
  //  std::array<vk::Semaphore, 1> semaphoresAfterRender{semaphoreRenderFinished[currentFrame].get()};
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
  recordCommandBuffers(imageindex);

  if (fencesImagesInFlight[imageindex].has_value())
    device->getDevice()->waitForFences(fencesImagesInFlight[imageindex].value(), VK_TRUE,
                                       UINT64_MAX);
  fencesImagesInFlight[imageindex] = fencesInFlight[currentFrame].get();

  device->getDevice()->resetFences(fencesInFlight[currentFrame].get());

  device->getDevice()->waitForFences(tmpfence.get(), VK_TRUE, UINT64_MAX);

  auto semaphoreSPHRenderIn =
      std::vector<vk::Semaphore>{semaphoreImageAvailable[currentFrame].get()};
  auto semaphoreGridRenderIn = simulationType == SimulationType::Combined
      ? &semaphoreBetweenRender[currentFrame]
      : &semaphoreImageAvailable[currentFrame];
  if (simulate || step) {
    if (Utilities::isIn(simulationType, {SimulationType::SPH, SimulationType::Combined})) {
      semaphoreAfterSort[currentFrame] = vulkanGridSPH->run(semaphoreImageAvailable[currentFrame]);

      semaphoreAfterMassDensity[currentFrame] = vulkanSPH->run(semaphoreAfterSort[currentFrame], SPHStep::massDensity);
      semaphoreAfterForces[currentFrame] = vulkanSPH->run(semaphoreAfterMassDensity[currentFrame], SPHStep::force);
      semaphoreAfterSimulationSPH[currentFrame] = vulkanSPH->run(semaphoreAfterForces[currentFrame], SPHStep::advect);
    }
    if (Utilities::isIn(simulationType, {SimulationType::Grid, SimulationType::Combined})) {
      {
        semaphoreAfterSimulationGrid[currentFrame] =
            vulkanGridFluid->run(semaphoreImageAvailable[currentFrame]);
        device->getDevice()->waitForFences(vulkanGridFluid->getFenceAfterCompute().get(), VK_TRUE,
                                           UINT64_MAX);
        vulkanGridFluidRender->updateDensityBuffer(vulkanGridFluid->getBufferDensity());
      }
    }
    switch (simulationType) {
      case SimulationType::Grid:
        semaphoreGridRenderIn = &semaphoreAfterSimulationGrid[currentFrame];
        break;
      case SimulationType::SPH:
        semaphoreSPHRenderIn = {semaphoreAfterSimulationSPH[currentFrame].get()};
        break;
      case SimulationType::Combined:
        semaphoreSPHRenderIn = {semaphoreAfterSimulationGrid[currentFrame].get(),
                                semaphoreAfterSimulationSPH[currentFrame].get()};
        semaphoreGridRenderIn = &semaphoreBetweenRender[currentFrame];
        break;
    }
  }
  vk::SubmitInfo submitInfoRender{.waitSemaphoreCount =
                                      static_cast<uint32_t>(semaphoreSPHRenderIn.size()),
                                  .pWaitSemaphores = semaphoreSPHRenderIn.data(),
                                  .pWaitDstStageMask = waitStagesRender.data(),
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &commandBuffersGraphic[imageindex].get(),
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = simulationType == SimulationType::Combined
                                      ? &semaphoreBetweenRender[currentFrame].get()
                                      : &semaphoreRenderFinished[currentFrame].get()};
  vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                 .pWaitSemaphores = &semaphoreRenderFinished[currentFrame].get(),
                                 .swapchainCount = 1,
                                 .pSwapchains = swapchains.data(),
                                 .pImageIndices = &imageindex,
                                 .pResults = nullptr};

  if (Utilities::isIn(simulationType, {SimulationType::SPH, SimulationType::Combined}))
    queueGraphics.submit(
        submitInfoRender,
        simulationType == SimulationType::Combined ? nullptr : fencesInFlight[currentFrame].get());
  if (Utilities::isIn(simulationType, {SimulationType::Grid, SimulationType::Combined}))
    semaphoreRenderFinished[currentFrame] = vulkanGridFluidRender->draw(
        *semaphoreGridRenderIn, imageindex, fencesInFlight[currentFrame]);

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
  pipelineGraphics =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfosRender)
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(config.getVulkan().shaderFolder / "SPH/shader.vert")
          .setFragmentShaderPath(config.getVulkan().shaderFolder / "SPH/shader.frag")
          .addRenderPass("toSwapchain",
                         RenderPassBuilder{device}
                             .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                             .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                             .build())
          .addRenderPass("toTexture",
                         RenderPassBuilder{device}
                             .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                             .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                             .build())
          .build();
  framebuffersSwapchain->createFramebuffers();
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
  auto offsetCount = 0;
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
    verticesCountOffset.emplace_back(offsetCount);
    bufferVertex->fill(model.vertices, true, offset);
    offset += model.vertices.size() * sizeof(Vertex);
    offsetCount += model.vertices.size();
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
    indicesByteOffsets.emplace_back(offset);
    indicesSizes.emplace_back(model.indices.size());
    bufferIndex->fill(model.indices, true, offset);
    offset += model.indices.size() * sizeof(uint16_t);
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
  auto builder = ImageBuilder()
                     .createView(true)
                     .setImageViewAspect(vk::ImageAspectFlagBits::eDepth)
                     .setFormat(VulkanUtils::findDepthFormat(device))
                     .setHeight(swapchain->getExtentHeight())
                     .setWidth(swapchain->getExtentWidth())
                     .setProperties(vk::MemoryPropertyFlagBits::eDeviceLocal)
                     .setTiling(vk::ImageTiling::eOptimal)
                     .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);

  imageDepthSwapchain = builder.build(device);
  imageDepthTexture = builder.build(device);

  imageDepthSwapchain->transitionImageLayoutNow(device, commandPoolGraphics, queueGraphics,
                                                vk::ImageLayout::eUndefined,
                                                vk::ImageLayout::eDepthStencilAttachmentOptimal);
  imageDepthTexture->transitionImageLayoutNow(device, commandPoolGraphics, queueGraphics,
                                              vk::ImageLayout::eUndefined,
                                              vk::ImageLayout::eDepthStencilAttachmentOptimal);
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
  imgui = std::make_shared<ig::ImGuiGlfwVulkan>(
      device, instance, pipelineGraphics->getRenderPass(""), surface, swapchain,
      window.getWindow().get(), ImGuiConfigFlags{}, toml::table{});

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

  auto &controlButton = a.createChild<ig::Button>("button_control", "Start simulationSPH");
  auto &stepButton = a.createChild<ig::Button>("button_step", "Step simulationSPH");
  stepButton.setEnabled(pf::Enabled::Yes);
  controlButton.addClickListener([this, &controlButton, &stepButton]() {
    simulate = not simulate;
    if (simulate) {
      controlButton.setLabel("Pause simulationSPH");
      stepButton.setEnabled(pf::Enabled::Yes);
    } else {
      controlButton.setLabel("Start simulationSPH");
      stepButton.setEnabled(pf::Enabled::No);
    }
  });
  stepButton.addClickListener([this]() { step = true; });

  controlWindow
      .createChild<ig::ComboBox>("combobox_SimulationSelect", "Simulation:", "SPH",
                                 [] {
                                   auto names = magic_enum::enum_names<SimulationType>();
                                   return std::vector<std::string>{names.begin(), names.end()};
                                 }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<SimulationType>(value);
        simulationType = selected.has_value() ? selected.value() : SimulationType::SPH;
        rebuildRenderPipelines();
      });

  auto &debugVisualizationWindow =
      imgui->createChild<ig::Window>("window_visualization", "Visualization");
  debugVisualizationWindow
      .createChild<ig::ComboBox>("combobox_visualization", "Show", "None",
                                 [] {
                                   auto names = magic_enum::enum_names<Visualization>();
                                   return std::vector<std::string>{names.begin(), names.end()};
                                 }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<Visualization>(value);
        textureVisualization = selected.has_value() ? selected.value() : Visualization::None;
      });
  textureVisualization = Visualization::None;
  debugVisualizationWindow.createChild<ig::Image>(
      "image_debug",
      [&] {
        return (ImTextureID) ImGui_ImplVulkan_AddTexture(
            textureSampler->getSampler().get(),
            imageColorTexture[currentFrame]->getImageView().get(),
            static_cast<VkImageLayout>(imageColorTexture[currentFrame]->getLayout()));
      }(),
      ImVec2{200, 200}, ig::IsButton::No,
      [] {
        return std::pair(ImVec2{0, 0}, ImVec2{1, 1});
      });
}

void VulkanCore::createTextureImages() {
  imageColorTexture.clear();
  auto builder =
      ImageBuilder()
          .createView(true)
          .setFormat(swapchain->getSwapchainImageFormat())
          .setHeight(swapchain->getExtentHeight())
          .setWidth(swapchain->getExtentWidth())
          .setProperties(vk::MemoryPropertyFlagBits::eDeviceLocal)
          .setTiling(vk::ImageTiling::eOptimal)
          .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);
  for (auto _ : swapchain->getSwapchainImages()) {
    imageColorTexture.emplace_back(builder.build(device));
    imageColorTexture.back()->transitionImageLayoutNow(device, commandPoolGraphics, queueGraphics,
                                                       vk::ImageLayout::eUndefined,
                                                       vk::ImageLayout::eGeneral);
  }
}
void VulkanCore::rebuildRenderPipelines() {
  auto renderPassSPH = RenderPassBuilder{device}
                           .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                           .setColorAttachmentFormat(swapchain->getSwapchainImageFormat());

  auto pipelineBuilder =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfosRender)
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(config.getVulkan().shaderFolder / "SPH/shader.vert")
          .setFragmentShaderPath(config.getVulkan().shaderFolder / "SPH/shader.frag")
          .addPushConstant(vk::ShaderStageFlagBits::eVertex, sizeof(DrawInfo));

  device->getDevice()->waitIdle();

  switch (simulationType) {
    case SimulationType::SPH:
    case SimulationType::Grid:
      renderPassSPH.setColorAttachementFinalLayout(vk::ImageLayout::ePresentSrcKHR);
      pipelineBuilder.addRenderPass("toSwapchain", renderPassSPH.build())
          .addRenderPass("toTexture", renderPassSPH.build());
      pipelineGraphics = pipelineBuilder.build();

      vulkanGridFluidRender->rebuildPipeline(true);
      break;
    case SimulationType::Combined:
      renderPassSPH.setColorAttachementFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
      pipelineBuilder.addRenderPass("toSwapchain", renderPassSPH.build())
          .addRenderPass("toTexture", renderPassSPH.build());
      pipelineGraphics = pipelineBuilder.build();

      vulkanGridFluidRender->rebuildPipeline(false);
      break;
  }
}
