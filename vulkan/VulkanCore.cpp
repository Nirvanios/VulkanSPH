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
#include <pf_imgui/elements/Button.h>
#include <pf_imgui/elements/ComboBox.h>
#include <pf_imgui/elements/Group.h>
#include <pf_imgui/elements/Image.h>
#include <pf_imgui/elements/InputText.h>

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
  auto cellInfos = std::vector<CellInfo>(glm::compMul(simulationInfoSPH.gridSize.xyz()),
                                         CellInfo{.tags = 0, .indexes = -1});
  bufferIndexes->fill(cellInfos);

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
      simulationInfoGridFluid, vulkanGridFluid->getBufferValuesNew(), buffersUniformMVP,
      buffersUniformCameraPos, bufferIndexes);
  vulkanGridFluidRender->setFramebuffersSwapchain(framebuffersSwapchain);

  vulkanGridFluidSphCoupling = std::make_unique<VulkanGridFluidSPHCoupling>(
      config, vulkanGridSPH->getGridInfo(),
      SimulationInfo{simulationInfoSPH, simulationInfoGridFluid}, device, surface, swapchain,
      bufferIndexes, vulkanSPH->getBufferParticles(), vulkanGridFluid->getBufferValuesOld(),
      vulkanGridFluid->getBufferValuesNew(), bufferCellParticlePair,
      vulkanGridFluid->getBufferVelocitiesNew());

  vulkanSphMarchingCubes = std::make_unique<VulkanSPHMarchingCubes>(
      config, simulationInfoSPH, vulkanGridSPH->getGridInfo(), device, surface, swapchain,
      vulkanSPH->getBufferParticles(), bufferIndexes, bufferCellParticlePair, buffersUniformMVP,
      buffersUniformCameraPos);
  vulkanSphMarchingCubes->setFramebuffersSwapchain(framebuffersSwapchain);
  vulkanSphMarchingCubes->setImgui(imgui);

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
/*  videoDiskSaver.initStream(
      "./stream.mp4",
      *//*static_cast<unsigned int>(1.0 / static_cast<float>(config.getApp().simulationSPH.timeStep))*//*
      60, swapchain->getExtentWidth(), swapchain->getExtentHeight());*/
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

VulkanCore::VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos,
                       const float &yaw, const float &pitch)
    : indicesByteOffsets(0), cameraPos(cameraPos), yaw(yaw), pitch(pitch), config(config),
      window(window) {}

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

void VulkanCore::recordCommandBuffers(uint32_t imageIndex, Utilities::Flags<DrawType> stageRecord) {
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};
  auto &swapchainFramebuffers = framebuffersSwapchain->getFramebuffers();
  auto &textureFramebuffers = framebuffersTexture->getFramebuffers();
  auto &commandBufferGraphics = commandBuffersGraphic[imageIndex];
  DrawInfo drawInfo{.drawType = magic_enum::enum_integer(DrawType::Particles),
                    .visualization = magic_enum::enum_integer(Visualization::None),
                    .supportRadius = simulationInfo.supportRadius};
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBufferGraphics->begin(beginInfo);

  if (stageRecord.hasAny()) {

    std::vector<vk::ClearValue> clearValues(2);
    clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
    clearValues[1].setDepthStencil({1.0f, 0});

    vk::RenderPassBeginInfo renderPassBeginInfo{
        .renderPass = pipelineGraphics->getRenderPass(""),
        .framebuffer = swapchainFramebuffers[imageIndex].get(),
        .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()};

    //vulkanGridFluidRender->recordRenderpass(imageIndex, commandBufferGraphics);

    commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());

    if (stageRecord.has(DrawType::Particles)) {
      drawInfo.drawType = magic_enum::enum_integer(DrawType::Particles);

      commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                          pipelineGraphics->getPipeline().get());
      commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0,
                                             vk::IndexType::eUint16);
      commandBufferGraphics->bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
          &descriptorSetGraphics->getDescriptorSets()[imageIndex].get(), 0, nullptr);
      commandBufferGraphics->pushConstants(pipelineGraphics->getPipelineLayout().get(),
                                           vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                           &drawInfo);

      commandBufferGraphics->drawIndexed(indicesSizes[0],
                                         config.getApp().simulationSPH.particleCount, 0, 0, 0);
    }

    if (stageRecord.has(DrawType::Grid)) {
      drawInfo.drawType = magic_enum::enum_integer(DrawType::Grid);
      commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                          pipelineGraphicsGrid->getPipeline().get());
      commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), indicesByteOffsets[1],
                                             vk::IndexType::eUint16);
      commandBufferGraphics->bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
          &descriptorSetGraphics->getDescriptorSets()[imageIndex].get(), 0, nullptr);
      commandBufferGraphics->pushConstants(pipelineGraphicsGrid->getPipelineLayout().get(),
                                           vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                           &drawInfo);
      commandBufferGraphics->drawIndexed(indicesSizes[1], 1, 0, verticesCountOffset[1], 0);
      imgui->addToCommandBuffer(commandBufferGraphics);
    }

    commandBufferGraphics->endRenderPass();

    if (stageRecord.has(DrawType::ToTexture)) {

      imageColorTexture[imageIndex]->transitionImageLayout(
          commandBufferGraphics,
          /*simulationType != SimulationType::SPH ? vk::ImageLayout::eColorAttachmentOptimal
                                              : */
          vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal,
          vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);

      renderPassBeginInfo.renderPass = pipelineGraphics->getRenderPass("toTexture");
      renderPassBeginInfo.framebuffer = textureFramebuffers[imageIndex].get();
      drawInfo.visualization = magic_enum::enum_integer(textureVisualization);

      commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
      commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics,
                                          pipelineGraphics->getPipeline().get());
      commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
      commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0,
                                             vk::IndexType::eUint16);
      commandBufferGraphics->bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
          &descriptorSetGraphics->getDescriptorSets()[imageIndex].get(), 0, nullptr);

      drawInfo.drawType = magic_enum::enum_integer(DrawType::Particles);
      commandBufferGraphics->pushConstants(pipelineGraphics->getPipelineLayout().get(),
                                           vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawInfo),
                                           &drawInfo);

      commandBufferGraphics->drawIndexed(indicesSizes[0],
                                         config.getApp().simulationSPH.particleCount, 0, 0, 0);
      commandBufferGraphics->endRenderPass();

      imageColorTexture[imageIndex]->transitionImageLayout(
          commandBufferGraphics, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eGeneral,
          vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eMemoryRead);
    }

    if (stageRecord.has(DrawType::ToFile)) {

      if (outputToFile) {
        swapchain->getSwapchainImages()[imageIndex]->transitionImageLayout(
            commandBufferGraphics, vk::ImageLayout::ePresentSrcKHR,
            vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eMemoryRead,
            vk::AccessFlagBits::eTransferRead);

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
      }
    }
  }
  commandBufferGraphics->end();
}

void VulkanCore::createSyncObjects() {
  fencesImagesInFlight.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSimulationSPH.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterTag.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterCoupling.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterMassDensity.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterForces.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSimulationGrid.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterSort.resize(swapchain->getSwapchainImageCount());
  semaphoreAfterMC.resize(swapchain->getSwapchainImageCount());

  for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    semaphoreImageAvailable.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreRenderFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreBetweenRender.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreBeforeSPH.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreBeforeGrid.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    semaphoreBeforeMC.emplace_back(device->getDevice()->createSemaphoreUnique({}));
    fencesInFlight.emplace_back(
        device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
  }
}
void VulkanCore::drawFrame() {
  auto tmpfence = device->getDevice()->createFenceUnique({});
  std::array<vk::Semaphore, 1> semaphoresAfterNextImage{
      semaphoreImageAvailable[currentFrame].get()};
  std::array<vk::PipelineStageFlags, 1> waitStagesRender{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};
  std::vector<vk::Semaphore> semaphoreBeforeSim{};
  switch (simulationType) {
    case SimulationType::Grid:
      semaphoreBeforeSim.emplace_back(semaphoreBeforeGrid[currentFrame].get());
      break;
    case SimulationType::SPH:
      semaphoreBeforeSim.emplace_back(semaphoreBeforeSPH[currentFrame].get());
      break;
    case SimulationType::Combined:
      semaphoreBeforeSim.emplace_back(semaphoreBeforeGrid[currentFrame].get());
      semaphoreBeforeSim.emplace_back(semaphoreBeforeSPH[currentFrame].get());
      break;
  }

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

  auto drawFlags = Utilities::Flags<DrawType>{std::vector<DrawType>{}};

  if (simulate || step) {
    ++simStep;
    auto fence = device->getDevice()->createFenceUnique({});
    vk::SubmitInfo submitInfoRenderBefore{
        .waitSemaphoreCount = semaphoresAfterNextImage.size(),
        .pWaitSemaphores = semaphoresAfterNextImage.data(),
        .pWaitDstStageMask = waitStagesRender.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffersGraphic[imageindex].get(),
        .signalSemaphoreCount = static_cast<uint32_t>(semaphoreBeforeSim.size()),
        .pSignalSemaphores = semaphoreBeforeSim.data()};
    recordCommandBuffers(imageindex, drawFlags);
    queueGraphics.submit(submitInfoRenderBefore, fence.get());
    device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  }

  device->getDevice()->waitForFences(tmpfence.get(), VK_TRUE, UINT64_MAX);

  switch (simulationType) {
    case SimulationType::Combined:
    case SimulationType::SPH:
      if (renderType == RenderType::MarchingCubes) {
        drawFlags =
            Utilities::Flags<DrawType>{std::vector<DrawType>{DrawType::Grid, DrawType::ToFile}};
      } else {
        drawFlags = Utilities::Flags<DrawType>{std::vector<DrawType>{
            DrawType::Particles, DrawType::Grid, DrawType::ToTexture, DrawType::ToFile}};
      }
      break;
    case SimulationType::Grid:
      drawFlags =
          Utilities::Flags<DrawType>{std::vector<DrawType>{DrawType::Grid, DrawType::ToFile}};
      break;
  }
  recordCommandBuffers(imageindex, drawFlags);

  auto semaphoreSPHRenderIn = simulationType == SimulationType::Combined
      ? &semaphoreBetweenRender[currentFrame]
      : &semaphoreImageAvailable[currentFrame];
  auto semaphoreGridRenderIn = &semaphoreImageAvailable[currentFrame];
  if (simulate || step) {
    if (Utilities::isIn(simulationType, {SimulationType::SPH, SimulationType::Combined})) {
      semaphoreAfterSort[currentFrame] = vulkanGridSPH->run(semaphoreBeforeSPH[currentFrame]);

      semaphoreAfterMassDensity[currentFrame] =
          vulkanSPH->run(semaphoreAfterSort[currentFrame], SPHStep::massDensity);
      semaphoreAfterForces[currentFrame] =
          vulkanSPH->run(semaphoreAfterMassDensity[currentFrame], SPHStep::force);
      semaphoreAfterSimulationSPH[currentFrame] =
          vulkanSPH->run(semaphoreAfterForces[currentFrame], SPHStep::advect);
    }
    if (Utilities::isIn(simulationType, {SimulationType::Grid, SimulationType::Combined})) {
      {
        semaphoreAfterSimulationGrid[currentFrame] =
            vulkanGridFluid->run(semaphoreBeforeGrid[currentFrame]);
        device->getDevice()->waitForFences(vulkanGridFluid->getFenceAfterCompute().get(), VK_TRUE,
                                           UINT64_MAX);
        vulkanGridFluidRender->updateDensityBuffer(vulkanGridFluid->getBufferValuesNew());
      }
      if (simulationType == SimulationType::Combined) {
        semaphoreAfterTag[currentFrame] =
            vulkanGridFluidSphCoupling->run({semaphoreAfterSimulationSPH[currentFrame].get(),
                                             semaphoreAfterSimulationGrid[currentFrame].get()},
                                            CouplingStep::tag);
        semaphoreAfterCoupling[currentFrame] = vulkanGridFluidSphCoupling->run(
            semaphoreAfterTag[currentFrame].get(), CouplingStep::transfer);
      }
    }
    switch (simulationType) {
      case SimulationType::Grid:
        //semaphoreSPHRenderIn = &semaphoreAfterSimulationGrid[currentFrame];
        semaphoreGridRenderIn = &semaphoreAfterSimulationGrid[currentFrame];
        break;
      case SimulationType::SPH:
        semaphoreSPHRenderIn = &semaphoreAfterSimulationSPH[currentFrame];
        break;
      case SimulationType::Combined:
        semaphoreSPHRenderIn = &semaphoreBetweenRender[currentFrame];
        semaphoreGridRenderIn = &semaphoreAfterCoupling[currentFrame];
        break;
    }
  } else if (initSPH) {
    initSPH = false;
    semaphoreAfterSort[currentFrame] = vulkanGridSPH->run(semaphoreImageAvailable[currentFrame]);
    semaphoreAfterMassDensity[currentFrame] =
        vulkanSPH->run(semaphoreAfterSort[currentFrame], SPHStep::massDensity);
    semaphoreSPHRenderIn = &semaphoreAfterMassDensity[currentFrame];
  }

  vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &semaphoreSPHRenderIn->get(),
                                  .pWaitDstStageMask = waitStagesRender.data(),
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &commandBuffersGraphic[imageindex].get(),
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores =
                                      &semaphoreRenderFinished[currentFrame].get()};

  if (Utilities::isIn(simulationType, {SimulationType::Grid, SimulationType::Combined})) {
    vulkanGridFluidRender->updateUniformBuffer(imageindex, yaw, pitch);
    semaphoreBetweenRender[currentFrame] =
        vulkanGridFluidRender->draw(*semaphoreGridRenderIn, imageindex);
    if (simulationType == SimulationType::Grid) {
      submitInfoRender.pWaitSemaphores = &semaphoreBetweenRender[currentFrame].get();
      queueGraphics.submit(submitInfoRender, fencesInFlight[currentFrame].get());
    }
  }
  if (Utilities::isIn(simulationType, {SimulationType::SPH, SimulationType::Combined})) {
    if (renderType == RenderType::Particles) {
      queueGraphics.submit(submitInfoRender, fencesInFlight[currentFrame].get());
    } else if (renderType == RenderType::MarchingCubes) {
      if (simulationType == SimulationType::SPH && (simulate || step || computeColors)) {
        semaphoreAfterTag[currentFrame] =
            vulkanGridFluidSphCoupling->run({semaphoreSPHRenderIn->get()}, CouplingStep::tag);
        semaphoreBeforeMC[currentFrame] =
            vulkanSphMarchingCubes->run(semaphoreAfterTag[currentFrame]);
        semaphoreAfterMC[currentFrame] =
            vulkanSphMarchingCubes->draw(semaphoreBeforeMC[currentFrame], imageindex);
      } else if (simulate || step || computeColors) {
        semaphoreBeforeMC[currentFrame] = vulkanSphMarchingCubes->run(*semaphoreSPHRenderIn);
        semaphoreAfterMC[currentFrame] =
            vulkanSphMarchingCubes->draw(semaphoreBeforeMC[currentFrame], imageindex);
      } else {
        semaphoreAfterMC[currentFrame] =
            vulkanSphMarchingCubes->draw(*semaphoreSPHRenderIn, imageindex);
      }
      submitInfoRender.pWaitSemaphores = &semaphoreAfterMC[currentFrame].get();
      queueGraphics.submit(submitInfoRender, fencesInFlight[currentFrame].get());
      computeColors = false;
    }
  }

  if (outputToFile) {
    ++capturedFrameCount;
    if (capturedFrameCount % 17 == 0) {
      onFrameSaveCallback();
      device->getDevice()->waitForFences(fencesInFlight[currentFrame].get(), VK_TRUE, UINT64_MAX);

      vk::ImageSubresource subResource{vk::ImageAspectFlagBits::eColor, 0, 0};
      vk::SubresourceLayout subresourceLayout;
      device->getDevice()->getImageSubresourceLayout(imageOutput->getImage().get(), &subResource,
                                                     &subresourceLayout);

      auto output = imageOutput->read(device);
      //videoDiskSaver.insertFrameBlocking(output, PixelFormat::RGBA);
      if (previousFrame.valid()) { previousFrame.wait(); }
      videoDiskSaver.insertFrameAsync(output, PixelFormat::RGBA);
    }
  }

  vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                 .pWaitSemaphores = &semaphoreRenderFinished[currentFrame].get(),
                                 .swapchainCount = 1,
                                 .pSwapchains = swapchains.data(),
                                 .pImageIndices = &imageindex,
                                 .pResults = nullptr};

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

  imageOutput->transitionImageLayoutNow(device, commandPoolGraphics, queueGraphics,
                                        vk::ImageLayout::eUndefined,
                                        vk::ImageLayout::eTransferDstOptimal);
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

  auto &infoWindow = imgui->createWindow("info_window", "Info");
  auto &labelFPS = infoWindow.createChild<ig::Text>("text_FPS", "");
  auto &labelFrameTime = infoWindow.createChild<ig::Text>("text_FrameTime", "");
  auto &labelSimStep = infoWindow.createChild<ig::Text>("text_SimStep", "");
  auto &labelYaw = infoWindow.createChild<ig::Text>("text_yaw", "");
  auto &labelPitch = infoWindow.createChild<ig::Text>("text_pitch:", "");

  fpsCounter.setOnNewFrameCallback(
      [this, &labelFrameTime, &labelFPS, &labelSimStep, &labelYaw, &labelPitch]() {
        labelFPS.setText(fmt::format("FPS: {:.1f} AVG: {:.1f}", fpsCounter.getCurrentFPS(),
                                     fpsCounter.getAverageFPS()));
        labelFrameTime.setText(fmt::format("Frame time: {:.1f}ms, AVG: {:.1f}ms",
                                           fpsCounter.getCurrentFrameTime(),
                                           fpsCounter.getAverageFrameTime()));
        labelSimStep.setText(fmt::format("Simulation step: {}", simStep));
        labelYaw.setText("Yaw: {}", yaw);
        labelPitch.setText("Pitch: {}", pitch);
      });
  auto &controlWindow = imgui->createWindow("control_window", "Control");
  auto &controlGroup = controlWindow.createChild<ig::Group>("aaa", "aaaaa");

  auto &controlButton =
      controlGroup.createChild<ig::Button>("button_control", "Start simulationSPH");
  auto &stepButton = controlGroup.createChild<ig::Button>("button_step", "Step simulationSPH");
  stepButton.setEnabled(pf::Enabled::Yes);
  controlButton.addClickListener([this, &controlButton, &stepButton]() {
    simulate = not simulate;
    if (simulate) {
      controlButton.setLabel("Pause simulationSPH");
      stepButton.setEnabled(pf::Enabled::No);
    } else {
      controlButton.setLabel("Start simulationSPH");
      stepButton.setEnabled(pf::Enabled::Yes);
    }
  });
  stepButton.addClickListener([this]() { step = true; });

  controlWindow
      .createChild<ig::ComboBox<std::string>>(
          "combobox_SimulationSelect", "Simulation:", "SPH",
          [] {
            auto names = magic_enum::enum_names<SimulationType>();
            return std::vector<std::string>{names.begin(), names.end()};
          }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<SimulationType>(value);
        simulationType = selected.has_value() ? selected.value() : SimulationType::SPH;
        rebuildRenderPipelines();
      });
  controlWindow
      .createChild<ig::ComboBox<std::string>>(
          "combobox_RenderSelect", "Render:", "Particles",
          [] {
            auto names = magic_enum::enum_names<RenderType>();
            return std::vector<std::string>{names.begin(), names.end()};
          }())
      .addValueListener([this](auto value) {
        auto selected = magic_enum::enum_cast<RenderType>(value);
        renderType = selected.has_value() ? selected.value() : RenderType::Particles;
        computeColors = renderType == RenderType::MarchingCubes;
        rebuildRenderPipelines();
      });

  auto &debugVisualizationWindow = imgui->createWindow("window_visualization", "Visualization");
  debugVisualizationWindow
      .createChild<ig::ComboBox<std::string>>(
          "combobox_visualization", "Show", "None",
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

  auto &recordingWindow = imgui->createWindow("window_Recording", "Recording");
  auto &framesCount = recordingWindow.createChild<ig::Text>("text_RecordedFramesCount", "");
  onFrameSaveCallback = [this, &framesCount] {
    framesCount.setText("Recorded frames: {}, Recorded time: {:.3}s", capturedFrameCount / 17,
                        (capturedFrameCount / 17 )/ 60.0);
  };
  auto &editFilename = recordingWindow.createChild<ig::InputText>("memo_Filename", "Filename:");

  auto &buttonRecording =
      recordingWindow.createChild<ig::Button>("button_startRecord", "Start Recording");
  buttonRecording.addClickListener([this, &buttonRecording, &editFilename] {
    outputToFile = !outputToFile;
    if (outputToFile) {
      buttonRecording.setLabel("Stop Recording");
      videoDiskSaver.initStream(fmt::format("./{}", editFilename.getText()), 60,
                                swapchain->getExtentWidth(), swapchain->getExtentHeight());
    } else {
      buttonRecording.setLabel("Start Recording");
      videoDiskSaver.endStream();
    }
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
      renderPassSPH.setColorAttachementFinalLayout(vk::ImageLayout::ePresentSrcKHR);
      break;
    case SimulationType::Grid:
    case SimulationType::Combined:
      renderPassSPH.setColorAttachementFinalLayout(vk::ImageLayout::ePresentSrcKHR)
          .setColorAttachmentLoadOp(vk::AttachmentLoadOp::eLoad);
      break;
  }
  if (renderType == RenderType::MarchingCubes && simulationType == SimulationType::Combined) {
    vulkanSphMarchingCubes->rebuildPipeline(false);
  } else {
    vulkanSphMarchingCubes->rebuildPipeline(true);
  }
  if (renderType == RenderType::MarchingCubes) {
    renderPassSPH.setColorAttachmentLoadOp(vk::AttachmentLoadOp::eLoad);
  }

  pipelineBuilder.addRenderPass("toSwapchain", renderPassSPH.build())
      .addRenderPass("toTexture", renderPassSPH.build());
  pipelineGraphics = pipelineBuilder.build();

  vulkanGridFluidRender->rebuildPipeline(true);
}
