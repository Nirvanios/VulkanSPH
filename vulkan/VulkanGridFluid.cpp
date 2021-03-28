//
// Created by Igor Frank on 12.02.21.
//

#include "VulkanGridFluid.h"
#include <glm/gtx/component_wise.hpp>
#include <magic_enum.hpp>
#include <utility>

vk::UniqueSemaphore VulkanGridFluid::run(const vk::UniqueSemaphore &semaphoreWait) {
  auto specificInfo = GaussSeidelFlags();

  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  /**Add velocities sources*/
  submit(Stages::addSourceVector, fence.get(), semaphoreWait.get());

  /**Diffuse velocities*/
  swapBuffers(bufferVelocitiesNew, bufferVelocitiesOld);

  for (auto i = 0; i < 20; ++i) {
    specificInfo.setStageType(GaussSeidelStageType::diffuse).setColor(GaussSeidelColorPhase::black);
    simulationInfo.specificInfo = static_cast<unsigned int>(specificInfo);
    submit(Stages::diffuseVector, fence.get());
    waitFence();

    specificInfo.setColor(GaussSeidelColorPhase::red);
    simulationInfo.specificInfo = static_cast<unsigned int>(specificInfo);
    submit(Stages::diffuseVector, fence.get());
    waitFence();

    simulationInfo.specificInfo = magic_enum::enum_integer(BufferType::vec4Type);
    submit(Stages::boundaryHandleVector, fence.get());
    waitFence();
  }

  /**Project*/
  for (auto i = 0; i < 1; ++i) { project(); }
  /**Advect velocities*/
  swapBuffers(bufferVelocitiesNew, bufferVelocitiesOld);
  submit(Stages::advectVector, fence.get());
  waitFence();
  simulationInfo.specificInfo = magic_enum::enum_integer(BufferType::vec4Type);
  submit(Stages::boundaryHandleVector, fence.get());
  waitFence();

  /**Project*/
  for (auto i = 0; i < 1; ++i) { project(); }
  waitFence();

  /**Add Density Sources*/
  submit(Stages::addSourceScalar, fence.get());

  /**Diffusion of density*/
  specificInfo.setStageType(GaussSeidelStageType::diffuse);
  swapBuffers(bufferValuesNew, bufferValuesOld);


  for (auto i = 0; i < 19; ++i) {
    simulationInfo.specificInfo =
        static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::black));
    submit(Stages::diffuseScalar, fence.get());
    waitFence();

    simulationInfo.specificInfo =
        static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::red));
    submit(Stages::diffuseScalar, fence.get());
    waitFence();
    submit(Stages::boundaryHandleVec2, fence.get());
    waitFence();

  }

  simulationInfo.specificInfo =
      static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::black));
  submit(Stages::diffuseScalar, fence.get());
  waitFence();

  simulationInfo.specificInfo =
      static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::red));
  submit(Stages::diffuseScalar, fence.get());
  waitFence();

  submit(Stages::boundaryHandleVec2, fence.get());


  /**Advect Density*/
  swapBuffers(bufferValuesNew, bufferValuesOld);
  submit(Stages::advectScalar, fence.get());
  waitFence();

  submit(Stages::boundaryHandleVec2, fence.get(), std::nullopt, outSemaphore);

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
}
void VulkanGridFluid::waitFence() {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
}

void VulkanGridFluid::project() {
  auto specificInfo = GaussSeidelFlags();
  bufferPressures->fill(
      std::vector<float>(glm::compMul(simulationInfo.gridSize.xyz() + glm::ivec3(2)), 0));

  submit(Stages::divergenceVector, fence.get());
  waitFence();


  setBoundaryScalarStageBuffer(bufferDivergences);
  simulationInfo.specificInfo = magic_enum::enum_integer(BufferType::floatType);
  submit(Stages::boundaryHandleScalar, fence.get());
  waitFence();


  specificInfo.setStageType(GaussSeidelStageType::project);
  for (auto j = 0; j < 20; ++j) {
    simulationInfo.specificInfo =
        static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::black));
    submit(Stages::GaussSeidelDivergence, fence.get());
    waitFence();


    simulationInfo.specificInfo =
        static_cast<unsigned int>(specificInfo.setColor(GaussSeidelColorPhase::red));
    submit(Stages::GaussSeidelDivergence, fence.get());
    waitFence();


    setBoundaryScalarStageBuffer(bufferPressures);
    simulationInfo.specificInfo = magic_enum::enum_integer(BufferType::floatType);
    submit(Stages::boundaryHandleScalar, fence.get());
    waitFence();
  }

  submit(Stages::gradientSubtractionVector, fence.get());
  waitFence();

  simulationInfo.specificInfo = magic_enum::enum_integer(BufferType::vec4Type);
  submit(Stages::boundaryHandleVector, fence.get());

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
  if (Utilities::isIn(pipelineStage, {Stages::boundaryHandleVector, Stages::boundaryHandleScalar, Stages::boundaryHandleVec2}))
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

  std::map<Stages, std::string> fileNames{
      {Stages::addSourceScalar, "addForce.comp"},
      {Stages::diffuseScalar, "GaussSeidelRedBlack.comp"},
      {Stages::advectScalar, "advect.comp"},
      {Stages::boundaryHandleScalar, "boundary.comp"},
      {Stages::addSourceVector, "addForce.comp"},
      {Stages::diffuseVector, "GaussSeidelRedBlack.comp"},
      {Stages::divergenceVector, "divergence.comp"},
      {Stages::GaussSeidelDivergence, "GaussSeidelRedBlack.comp"},
      {Stages ::gradientSubtractionVector, "gradientSubtraction.comp"},
      {Stages::boundaryHandleVector, "boundary.comp"},
      {Stages::boundaryHandleVec2, "boundary.comp"},
      {Stages::advectVector, "advect.comp"}};
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "GridFluid/{}";
  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    auto pipelineBuilder =
        computePipelineBuilder.setLayoutBindingInfo(bindingInfosCompute[stage])
            .setComputeShaderPath(fmt::format(shaderPathTemplate.string(), fileNames[stage]));
    if (Utilities::isIn(stage, {Stages::boundaryHandleScalar, Stages::GaussSeidelDivergence}))
      pipelineBuilder.addShaderMacro("TYPENAME_T float").addShaderMacro("TYPE_FLOAT");
    else if (Utilities::isIn(
                 stage, {Stages::addSourceScalar, Stages ::diffuseScalar, Stages::advectScalar, Stages::boundaryHandleVec2}))
      pipelineBuilder.addShaderMacro("TYPENAME_T vec2").addShaderMacro("TYPE_VEC2");
    else if (Utilities::isIn(stage,
                             {Stages::addSourceVector, Stages::diffuseVector,
                              Stages::boundaryHandleVector, Stages::advectVector}))
      pipelineBuilder.addShaderMacro("TYPENAME_T vec4").addShaderMacro("TYPE_VEC4");
    if (descriptorBufferInfosCompute.contains(stage)) {
      pipelines[stage] = pipelineBuilder.build();
      descriptorSets[stage] = std::make_shared<DescriptorSet>(
          this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
      descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                 bindingInfosCompute[stage]);
    }
  }

  fence = device->getDevice()->createFenceUnique({});

  semaphores.resize(10000);//TODO pool
  std::generate_n(semaphores.begin(), 10000,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

void VulkanGridFluid::createBuffers() {
  auto cellCountBorder = glm::compMul(simulationInfo.gridSize.xyz() + glm::ivec3(2));
  auto initialDensity = std::vector<glm::vec2>(cellCountBorder, glm::vec2(300.0, 0.0));
  auto sources = std::vector<glm::vec2>(simulationInfo.cellCount, glm::vec2(0));
  [[maybe_unused]] auto positionSources =
      ((simulationInfo.gridSize.z / 2) * simulationInfo.gridSize.x * simulationInfo.gridSize.y)
      + (simulationInfo.gridSize.x / 2)
      + (simulationInfo.gridSize.x * (simulationInfo.gridSize.y - 1));
  //sources[positionSources] = 0.1;
  [[maybe_unused]] auto positionDensity =
      (((simulationInfo.gridSize.z + 2) / 2) * (simulationInfo.gridSize.x + 2)
       * (simulationInfo.gridSize.y + 2))
      + ((simulationInfo.gridSize.x + 2) / 2)
      + ((simulationInfo.gridSize.x + 2) * simulationInfo.gridSize.y);
  //initialDensity[positionDensity] = glm::vec2(0.0f, 0.0f);

  auto bufferBuilder = BufferBuilder()
                           .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);
  bufferValuesNew = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec2) * cellCountBorder), this->device, commandPool, queue);
  bufferValuesNew->fill(initialDensity);

  bufferValuesOld = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec2) * cellCountBorder), this->device, commandPool, queue);
  bufferValuesOld->fill(initialDensity);

  bufferValuesSources =
      std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(glm::vec2) * simulationInfo.cellCount),
                               this->device, commandPool, queue);
  bufferValuesSources->fill(sources);

  auto initialVelocities = std::vector<glm::vec4>(cellCountBorder, glm::vec4(0, -10, 0, 0));
  initialVelocities[positionDensity - (simulationInfo.gridSize.x + 2)].y = -10;
  bufferVelocitiesNew = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec4) * cellCountBorder), this->device, commandPool, queue);
  bufferVelocitiesNew->fill(initialVelocities);

  bufferVelocitiesOld = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec4) * cellCountBorder), this->device, commandPool, queue);
  bufferVelocitiesOld->fill(std::vector<glm::vec4>(cellCountBorder, glm::vec4(0, -10, 0, 0)));

  bufferVelocitySources =
      std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(glm::vec4) * simulationInfo.cellCount),
                               this->device, commandPool, queue);
  bufferVelocitySources->fill(
      std::vector<glm::vec4>(simulationInfo.cellCount, glm::vec4(0, 0, 0, 0)));

  bufferDivergences = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(float) * cellCountBorder), this->device, commandPool, queue);
  bufferDivergences->fill(std::vector<float>(cellCountBorder, 0));

  bufferPressures = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(float) * cellCountBorder),
                                             this->device, commandPool, queue);
  bufferPressures->fill(std::vector<float>(cellCountBorder, 0));
}

const std::shared_ptr<Buffer> &VulkanGridFluid::getBufferValuesNew() const { return bufferValuesNew; }

const std::shared_ptr<Buffer> &VulkanGridFluid::getBufferValuesOld() const { return bufferValuesOld; }

void VulkanGridFluid::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfosCompute[stage]);
  });
}

void VulkanGridFluid::swapBuffers(std::shared_ptr<Buffer> &buffer1,
                                  std::shared_ptr<Buffer> &buffer2) {
  waitFence();
  buffer1.swap(buffer2);
  updateDescriptorSets();
}

void VulkanGridFluid::fillDescriptorBufferInfo() {
  const auto descriptorBufferInfoValuesNew =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferValuesNew, 1},
                           .bufferSize = bufferValuesNew->getSize()};
  const auto descriptorBufferInfoValuesOld =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferValuesOld, 1},
                           .bufferSize = bufferValuesOld->getSize()};
  const auto descriptorBufferInfoDensitySources =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferValuesSources, 1},
                           .bufferSize = bufferValuesSources->getSize()};
  const auto descriptorBufferInfoVelocitiesNew =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferVelocitiesNew, 1},
                           .bufferSize = bufferVelocitiesNew->getSize()};
  const auto descriptorBufferInfoVelocitiesOld =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferVelocitiesOld, 1},
                           .bufferSize = bufferVelocitiesOld->getSize()};
  const auto descriptorBufferInfoVelocitiesSources =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferVelocitySources, 1},
                           .bufferSize = bufferVelocitySources->getSize()};
  const auto descriptorBufferInfoPressure =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferPressures, 1},
                           .bufferSize = bufferPressures->getSize()};
  const auto descriptorBufferInfoDivergences =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferDivergences, 1},
                           .bufferSize = bufferDivergences->getSize()};

  descriptorBufferInfosCompute[Stages::addSourceScalar] = {descriptorBufferInfoValuesNew,
                                                           descriptorBufferInfoDensitySources};
  descriptorBufferInfosCompute[Stages::diffuseScalar] = {descriptorBufferInfoValuesNew,
                                                         descriptorBufferInfoValuesOld};
  descriptorBufferInfosCompute[Stages::advectScalar] = {descriptorBufferInfoValuesNew,
                                                        descriptorBufferInfoValuesOld,
                                                        descriptorBufferInfoVelocitiesNew};
  descriptorBufferInfosCompute[Stages::boundaryHandleScalar] = {descriptorBufferInfoValuesNew};
  descriptorBufferInfosCompute[Stages::addSourceVector] = {descriptorBufferInfoVelocitiesNew,
                                                           descriptorBufferInfoVelocitiesSources};
  descriptorBufferInfosCompute[Stages::diffuseVector] = {descriptorBufferInfoVelocitiesNew,
                                                         descriptorBufferInfoVelocitiesOld};
  descriptorBufferInfosCompute[Stages::divergenceVector] = {descriptorBufferInfoVelocitiesNew,
                                                            descriptorBufferInfoDivergences,
                                                            descriptorBufferInfoPressure};
  descriptorBufferInfosCompute[Stages::GaussSeidelDivergence] = {descriptorBufferInfoPressure,
                                                                 descriptorBufferInfoDivergences};
  descriptorBufferInfosCompute[Stages::gradientSubtractionVector] = {
      descriptorBufferInfoVelocitiesNew, descriptorBufferInfoPressure};
  descriptorBufferInfosCompute[Stages::boundaryHandleVector] = {descriptorBufferInfoVelocitiesNew};
  descriptorBufferInfosCompute[Stages::boundaryHandleVec2] = {descriptorBufferInfoValuesNew};
  descriptorBufferInfosCompute[Stages::advectVector] = {descriptorBufferInfoVelocitiesNew,
                                                        descriptorBufferInfoVelocitiesOld,
                                                        descriptorBufferInfoVelocitiesOld};
}

const vk::UniqueFence &VulkanGridFluid::getFenceAfterCompute() const { return fence; }

void VulkanGridFluid::setBoundaryScalarStageBuffer(std::shared_ptr<Buffer> &buffer) {
  descriptorBufferInfosCompute[Stages::boundaryHandleScalar].begin()->buffer =
      std::span<std::shared_ptr<Buffer>>{&buffer, 1};
  descriptorBufferInfosCompute[Stages::boundaryHandleScalar].begin()->bufferSize =
      buffer->getSize();
  updateDescriptorSets();
}
