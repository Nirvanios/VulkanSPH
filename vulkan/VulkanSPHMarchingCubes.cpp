//
// Created by Igor Frank on 19.04.21.
//

#include "VulkanSPHMarchingCubes.h"
#include "builders/BufferBuilder.h"
#include "builders/PipelineBuilder.h"

#include "../utils/Exceptions.h"
#include "builders/RenderPassBuilder.h"
#include "glm/gtx/component_wise.hpp"
#include "lookuptables.h"

VulkanSPHMarchingCubes::VulkanSPHMarchingCubes(
    const Config &inConfig, const SimulationInfoSPH &simulationInfo, const GridInfo &inGridInfo,
    std::shared_ptr<Device> inDevice, const vk::UniqueSurfaceKHR &surface,
    std::shared_ptr<Swapchain> inSwapchain, std::shared_ptr<Buffer> inBufferParticles,
    std::shared_ptr<Buffer> inBufferIndexes, std::shared_ptr<Buffer> inBufferSortedPairs,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformMVP,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformCameraPos,
    std::vector<std::shared_ptr<Buffer>> inBuffersUniformColor)
    : shaderPathTemplate(inConfig.getVulkan().shaderFolder / "SPH/Render/{}"), config(inConfig),
      device(std::move(inDevice)), swapchain(std::move(inSwapchain)),
      bufferParticles(std::move(inBufferParticles)), bufferGrid(std::move(inBufferSortedPairs)),
      bufferIndexes(std::move(inBufferIndexes)), bufferUnioformMVP(std::move(inBuffersUniformMVP)),
      bufferUnioformCameraPos(std::move(inBuffersUniformCameraPos)),
      bufferUnioformColor(std::move(inBuffersUniformColor)) {

  const auto &detailMC = config.getApp().marchingCubes.detail;

  marchingCubesInfo.simulationInfoSph = simulationInfo;
  marchingCubesInfo.gridInfoMC =
      GridInfoMC{.gridSize = (inGridInfo.gridSize + 2) * detailMC,
                 .gridOrigin = inGridInfo.gridOrigin,
                 .cellSize = inGridInfo.cellSize / static_cast<float>(detailMC),
                 .detail = detailMC};

  auto queueFamilyIndices = Device::findQueueFamilies(this->device->getPhysicalDevice(), surface);
  vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};
  vk::CommandPoolCreateInfo commandPoolCreateInfoRender{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  commandPoolCompute =
      this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
  commandPoolRender =
      this->device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoRender);
  commandBufferCompute = std::move(this->device->allocateCommandBuffer(commandPoolCompute, 1)[0]);
  commandBufferRender.resize(swapchain->getSwapchainImageCount());
  commandBufferRender =
      this->device->allocateCommandBuffer(commandPoolRender, commandBufferRender.size());
  queueCompute = this->device->getComputeQueue();
  queueRender = this->device->getGraphicsQueue();

  createBuffers();

  fillDescriptorBufferInfo();

  std::array<vk::DescriptorPoolSize, 2> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 11},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = 3}};
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

  auto computePipelineBuilder =
      PipelineBuilder{this->config, this->device, swapchain}
          .setPipelineType(PipelineType::Compute)
          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(MarchingCubesInfo));

  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    if (stage != Stages::Render) {
      auto pipelineBuilder =
          computePipelineBuilder.setLayoutBindingInfo(bindingInfos[stage])
              .setComputeShaderPath(fmt::format(shaderPathTemplate, computeShaderFiles[stage]));

      if (descriptorBufferInfosCompute.contains(stage)) {
        pipelines[stage] = pipelineBuilder.build();
        descriptorSets[stage] = std::make_shared<DescriptorSet>(
            this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
        descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                   bindingInfos[stage]);
      }
    }
  }

  pipelines[Stages::Render] =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfos[Stages::Render])
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Vertex]))
          .setFragmentShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Fragment]))
          .setGeometryShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Geometry]))
          .addPushConstant(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry,
                           sizeof(MarchingCubesInfo))
          .setAssemblyInfo(vk::PrimitiveTopology::ePointList, false)
          .setBlendEnabled(false)
          .setDepthTestEnabled(true)
          .addRenderPass(
              "toSwapchain",
              RenderPassBuilder{device}
                  .setColorAttachementFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                  .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                  .build())
          .build();
  descriptorSets[Stages::Render] = std::make_shared<DescriptorSet>(
      this->device, swapchain->getSwapchainImageCount(),
      pipelines[Stages::Render]->getDescriptorSetLayout(), descriptorPool);
  descriptorSets[Stages::Render]->updateDescriptorSet(descriptorBufferInfosCompute[Stages::Render],
                                                      bindingInfos[Stages::Render]);

  fence = device->getDevice()->createFenceUnique({});

  semaphores.resize(10);//TODO pool
  std::generate_n(semaphores.begin(), 10,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

void VulkanSPHMarchingCubes::createBuffers() {
  auto bufferSize = glm::compMul(marchingCubesInfo.gridInfoMC.gridSize.xyz() + glm::ivec3(1));

  auto bufferBuilder = BufferBuilder()
                           .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);

  bufferGridColors = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(float) * bufferSize),
                                              this->device, commandPoolCompute, queueCompute);
  bufferEdgeToVertexLUT =
      std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(mc::LUT::edgeToVertexIds)),
                               this->device, commandPoolCompute, queueCompute);
  bufferEdgeToVertexLUT->fill(mc::LUT::edgeToVertexIds);
  bufferEdgesLUT = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(mc::LUT::edges)),
                                            this->device, commandPoolCompute, queueCompute);
  bufferEdgesLUT->fill(mc::LUT::edges);
  bufferPolygonCountLUT =
      std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(mc::LUT::polygonCount)), this->device,
                               commandPoolCompute, queueCompute);
  bufferPolygonCountLUT->fill(mc::LUT::polygonCount);

  bufferSize = 1;
  bufferBuilder.setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                              | vk::BufferUsageFlagBits::eVertexBuffer);
  bufferVertex = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(Vertex) * bufferSize),
                                          this->device, commandPoolCompute, queueCompute);
  bufferVertex->fill(std::array<Vertex, 1>{
      Vertex{.pos = glm::vec4{0}, .color = glm::vec3{0.5, 0.8, 1.0}, .normal = glm::vec4{0}}});
}
void VulkanSPHMarchingCubes::fillDescriptorBufferInfo() {
  const auto descriptorBufferInfoGridColors =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridColors, 1},
                           .bufferSize = bufferGridColors->getSize()};
  const auto descriptorBufferInfoParticles =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferParticles, 1},
                           .bufferSize = bufferParticles->getSize()};
  const auto descriptorBufferInfoGrid =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGrid, 1},
                           .bufferSize = bufferGrid->getSize()};
  const auto descriptorBufferInfoIndexes =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferIndexes, 1},
                           .bufferSize = bufferIndexes->getSize()};
  const auto descriptorBufferInfoMVP =
      DescriptorBufferInfo{.buffer = bufferUnioformMVP,
                           .bufferSize = bufferUnioformMVP[0]->getSize()};
  const auto descriptorBufferInfoCameraPos =
      DescriptorBufferInfo{.buffer = bufferUnioformCameraPos,
                           .bufferSize = bufferUnioformCameraPos[0]->getSize()};
  const auto descriptorBufferEdgesToVertexLUT =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferEdgeToVertexLUT, 1},
                           .bufferSize = bufferEdgeToVertexLUT->getSize()};
  const auto descriptorBufferEdgesLUT =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferEdgesLUT, 1},
                           .bufferSize = bufferEdgesLUT->getSize()};
  const auto descriptorBufferPolygonCountLUT =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferPolygonCountLUT, 1},
                           .bufferSize = bufferPolygonCountLUT->getSize()};
  const auto descriptorBufferUniformColor =
      DescriptorBufferInfo{.buffer = bufferUnioformColor,
                           .bufferSize = bufferUnioformColor[0]->getSize()};

  descriptorBufferInfosCompute[Stages::ComputeColors] = {
      descriptorBufferInfoParticles, descriptorBufferInfoGrid, descriptorBufferInfoIndexes,
      descriptorBufferInfoGridColors};
  descriptorBufferInfosCompute[Stages::Render] = {
      descriptorBufferInfoMVP,          descriptorBufferInfoGridColors,
      descriptorBufferEdgesToVertexLUT, descriptorBufferPolygonCountLUT,
      descriptorBufferEdgesLUT,         descriptorBufferInfoIndexes,
      descriptorBufferInfoParticles,    descriptorBufferInfoGrid,
      descriptorBufferInfoCameraPos,    descriptorBufferInfoMVP,
      descriptorBufferUniformColor};
}
void VulkanSPHMarchingCubes::recordCommandBuffer(VulkanSPHMarchingCubes::Stages pipelineStage,
                                                 unsigned int imageIndex = -1) {
  if (Utilities::isIn(pipelineStage, {Stages::ComputeColors})) {
    auto gridSize = marchingCubesInfo.gridInfoMC.gridSize;
    [[maybe_unused]] auto gridSizeBorder = gridSize + 2;
    auto gridSizeColor = gridSize + 1;
    const auto &pipeline = pipelines[pipelineStage];
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                         .pInheritanceInfo = nullptr};

    commandBufferCompute->begin(beginInfo);
    commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute,
                                       pipeline->getPipeline().get());
    commandBufferCompute->bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
        &descriptorSets[pipelineStage]->getDescriptorSets()[0].get(), 0, nullptr);
    commandBufferCompute->pushConstants(pipeline->getPipelineLayout().get(),
                                        vk::ShaderStageFlagBits::eCompute, 0,
                                        sizeof(MarchingCubesInfo), &marchingCubesInfo);

    auto dispatchCount = glm::ivec3(glm::ceil(glm::compMul(gridSizeColor.xyz()) / 32.0), 1, 1);
    ;
    commandBufferCompute->dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
    commandBufferCompute->end();
  }
  if (pipelineStage == Stages::Render) {
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                         .pInheritanceInfo = nullptr};

    commandBufferRender[imageIndex]->begin(beginInfo);

    recordRenderpass(imageIndex, commandBufferRender[imageIndex]);

    commandBufferRender[imageIndex]->end();
  }
}

void VulkanSPHMarchingCubes::recordRenderpass(unsigned int imageIndex,
                                              const vk::UniqueCommandBuffer &commandBuffer) {
  const auto &swapchainFramebuffers = framebuffersSwapchain->getFramebuffers();
  std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
  std::array<vk::DeviceSize, 1> offsets{0};
  const auto invocationCount = glm::compMul(marchingCubesInfo.gridInfoMC.gridSize.xyz());

  std::vector<vk::ClearValue> clearValues(2);
  clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
  clearValues[1].setDepthStencil({1.0f, 0});

  vk::RenderPassBeginInfo renderPassBeginInfo{
      .renderPass = pipelines[Stages::Render]->getRenderPass("toSwapchain"),
      .framebuffer = swapchainFramebuffers[imageIndex].get(),
      .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()};

  commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics,
                              pipelines[Stages::Render]->getPipeline().get());
  commandBuffer->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
  commandBuffer->pushConstants(pipelines[Stages::Render]->getPipelineLayout().get(),
                               vk::ShaderStageFlagBits::eGeometry
                                   | vk::ShaderStageFlagBits::eVertex,
                               0, sizeof(MarchingCubesInfo), &marchingCubesInfo);

  commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, pipelines[Stages::Render]->getPipelineLayout().get(), 0, 1,
      &descriptorSets[Stages::Render]->getDescriptorSets()[imageIndex].get(), 0, nullptr);

  commandBuffer->draw(1, invocationCount, 0, 0);
  commandBuffer->endRenderPass();
}

void VulkanSPHMarchingCubes::setFramebuffersSwapchain(
    const std::shared_ptr<Framebuffers> &framebuffer) {
  framebuffersSwapchain = framebuffer;
}

void VulkanSPHMarchingCubes::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfos[stage]);
  });
}
void VulkanSPHMarchingCubes::waitFence() {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
}
void VulkanSPHMarchingCubes::submit(VulkanSPHMarchingCubes::Stages pipelineStage,
                                    const vk::Fence &submitFence,
                                    const std::optional<vk::Semaphore> &inSemaphore,
                                    const std::optional<vk::Semaphore> &outSemaphore,
                                    [[maybe_unused]] SubmitSemaphoreType submitSemaphoreType) {

  std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eComputeShader};
  recordCommandBuffer(pipelineStage);
  vk::SubmitInfo submitInfoCompute{
      .waitSemaphoreCount = static_cast<uint32_t>(1),
      .pWaitSemaphores =
          inSemaphore.has_value() ? &inSemaphore.value() : &semaphores[currentSemaphore - 1].get(),
      .pWaitDstStageMask = waitStages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBufferCompute.get(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          outSemaphore.has_value() ? &outSemaphore.value() : &semaphores[currentSemaphore].get()};
  queueCompute.submit(submitInfoCompute, submitFence);

  currentSemaphore++;
}
vk::UniqueSemaphore VulkanSPHMarchingCubes::run(const vk::UniqueSemaphore &inSemaphore) {

  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  /**Add velocities sources*/
  submit(Stages::ComputeColors, fence.get(), inSemaphore.get(), outSemaphore,
         SubmitSemaphoreType::InOut);

  waitFence();

  /*  [[maybe_unused]] auto colors = bufferGridColors->read<float>();
  [[maybe_unused]] auto parts = bufferParticles->read<ParticleRecord>();*/

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
}
vk::UniqueSemaphore VulkanSPHMarchingCubes::draw(const vk::UniqueSemaphore &inSemaphore,
                                                 unsigned int imageIndex) {
  auto tmpfence = device->getDevice()->createFenceUnique({});

  vk::Semaphore semaphoreAfterRender = device->getDevice()->createSemaphore({});

  std::array<vk::PipelineStageFlags, 1> waitStagesRender{
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  recordCommandBuffer(Stages::Render, imageIndex);

  vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &inSemaphore.get(),
                                  .pWaitDstStageMask = waitStagesRender.data(),
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &commandBufferRender[imageIndex].get(),
                                  .signalSemaphoreCount = 1,
                                  .pSignalSemaphores = &semaphoreAfterRender};

  queueRender.submit(submitInfoRender);

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return vk::UniqueSemaphore(semaphoreAfterRender, device->getDevice().get());
}

void VulkanSPHMarchingCubes::rebuildPipeline(bool clearBeforeDraw) {
  pipelines[Stages::Render] =
      PipelineBuilder{config, device, swapchain}
          .setLayoutBindingInfo(bindingInfos[Stages::Render])
          .setPipelineType(PipelineType::Graphics)
          .setVertexShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Vertex]))
          .setFragmentShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Fragment]))
          .setGeometryShaderPath(
              fmt::format(shaderPathTemplate, renderShaderFiles[RenderStages::Geometry]))
          .addPushConstant(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry,
                           sizeof(MarchingCubesInfo))
          .setAssemblyInfo(vk::PrimitiveTopology::ePointList, false)
          .setBlendEnabled(false)
          .setDepthTestEnabled(true)
          .addRenderPass(
              "toSwapchain",
              RenderPassBuilder{device}
                  .setColorAttachementFinalLayout(vk::ImageLayout::eColorAttachmentOptimal)
                  .setDepthAttachmentFormat(VulkanUtils::findDepthFormat(device))
                  .setColorAttachmentFormat(swapchain->getSwapchainImageFormat())
                  .setColorAttachmentLoadOp(clearBeforeDraw ? vk::AttachmentLoadOp::eClear
                                                            : vk::AttachmentLoadOp::eLoad)
                  .build())

          .build();

  descriptorSets[Stages::Render] = std::make_shared<DescriptorSet>(
      device, swapchain->getSwapchainImageCount(),
      pipelines[Stages::Render]->getDescriptorSetLayout(), descriptorPool);
  descriptorSets[Stages::Render]->updateDescriptorSet(descriptorBufferInfosCompute[Stages::Render],
                                                      bindingInfos[Stages::Render]);
}