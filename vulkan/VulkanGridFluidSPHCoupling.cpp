//
// Created by Igor Frank on 23.03.21.
//

#include "VulkanGridFluidSPHCoupling.h"
#include "../utils/Exceptions.h"

#include <glm/gtx/component_wise.hpp>
VulkanGridFluidSPHCoupling::VulkanGridFluidSPHCoupling(
    const Config &config, const GridInfo &inGridInfo, const SimulationInfo &inSimulationInfo,
    std::shared_ptr<Device> inDevice, const vk::UniqueSurfaceKHR &surface,
    std::shared_ptr<Swapchain> swapchain, std::shared_ptr<Buffer> inBufferIndexes,
    std::shared_ptr<Buffer> inBufferParticles, std::shared_ptr<Buffer> inBufferGridValuesOld,
    std::shared_ptr<Buffer> inBufferGridValuesNew, std::shared_ptr<Buffer> inBufferGridSPH,
    std::shared_ptr<Buffer> inBufferVelocities)
    : config(config), gridInfo(inGridInfo), simulationInfo(inSimulationInfo),
      device(std::move(inDevice)), bufferIndexes(std::move(inBufferIndexes)),
      bufferParticles(std::move(inBufferParticles)),
      bufferGridValuesOld(std::move(inBufferGridValuesOld)),
      bufferGridValuesNew(std::move(inBufferGridValuesNew)),
      bufferGridVelocities(std::move(inBufferVelocities)),
      bufferGridSPH(std::move(inBufferGridSPH)) {

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
      PipelineBuilder{this->config, this->device, swapchain}.setPipelineType(PipelineType::Compute);

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

  std::map<Stages, std::string> fileNames{{Stages::Tag, "TagCells.comp"},
                                          {Stages::TransferHeatToCells, "HeatTransfer.comp"},
                                          {Stages::TransferHeatToParticles, "HeatTransfer.comp"},
                                          {Stages::WriteNewParticleTemps, "WriteNewTemps.comp"},
                                          {Stages::MassTransfer, "MassTransfer.comp"},
                                          {Stages::WeightDistribution, "WeightDistribution.comp"}};
  auto shaderPathTemplate = config.getVulkan().shaderFolder / "Evaporation/{}";
  for (const auto &stage : magic_enum::enum_values<Stages>()) {
    auto pipelineBuilder =
        computePipelineBuilder.setLayoutBindingInfo(bindingInfosCompute[stage])
            .setComputeShaderPath(fmt::format(shaderPathTemplate.string(), fileNames[stage]));
    if (Utilities::isIn(stage, {Stages::TransferHeatToCells})) {
      pipelineBuilder.addShaderMacro("PARTICLE_TO_CELL");
    } else if (Utilities::isIn(stage, {Stages::TransferHeatToParticles})) {
      pipelineBuilder.addShaderMacro("CELL_TO_PARTICLE");
    }

    if (stage == Stages::Tag) {
      pipelineBuilder.addPushConstant(vk::ShaderStageFlagBits::eCompute,
                                      sizeof(SimulationInfoGridFluid));
    }
    if (stage == Stages::WriteNewParticleTemps) {
      pipelineBuilder.addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfoSPH));
    }

    if (descriptorBufferInfosCompute.contains(stage)) {
      pipelines[stage] = pipelineBuilder.build();
      descriptorSets[stage] = std::make_shared<DescriptorSet>(
          this->device, 1, pipelines[stage]->getDescriptorSetLayout(), descriptorPool);
      descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                                 bindingInfosCompute[stage]);
    }
  }

  fence = device->getDevice()->createFenceUnique({});

  semaphores.resize(5);//TODO pool
  std::generate_n(semaphores.begin(), 5,
                  [&] { return device->getDevice()->createSemaphoreUnique({}); });
}

vk::UniqueSemaphore
VulkanGridFluidSPHCoupling::run(const std::vector<vk::Semaphore> &semaphoreWait) {

  currentSemaphore = 0;

  device->getDevice()->resetFences(fence.get());

  auto outSemaphore = device->getDevice()->createSemaphore({});

  submit(Stages::Tag, fence.get(), semaphoreWait);
  waitFence();

  submit(Stages::TransferHeatToCells, fence.get());
  waitFence();

  submit(Stages::TransferHeatToParticles, fence.get());
  waitFence();
  [[maybe_unused]] auto oldparticles = bufferParticles->read<ParticleRecord>();
  [[maybe_unused]] auto tempsParticle = bufferParticleTempsNew->read<float>();
/*  for (const auto &item : tempsParticle){
    if(std::isnan(item) || std::isinf(item)){
      break;
    }
  }*/

  submit(Stages::WriteNewParticleTemps, fence.get());
  waitFence();
  //swapBuffers(bufferGridValuesNew, bufferGridValuesOld);

  submit(Stages::MassTransfer, fence.get());
  waitFence();

  submit(Stages::WeightDistribution, fence.get(), std::nullopt, outSemaphore);
  waitFence();





  //[[maybe_unused]] auto ind = bufferIndexes->read<CellInfo>();
  //[[maybe_unused]] auto tempsCells = bufferGridValuesNew->read<glm::vec2>();
  //[[maybe_unused]] auto tempsCellsOld = bufferGridValuesOld->read<glm::vec2>();
  [[maybe_unused]] auto particles = bufferParticles->read<ParticleRecord>();
  //[[maybe_unused]] auto tempsParticle = bufferParticleTempsNew->read<float>();
  //[[maybe_unused]] auto hasPair = bufferHasPair->read<KeyValue>();

  auto i = 0;
  for (const auto &item : particles){
    if((std::isnan(item.temperature) || std::isinf(item.temperature)) && item.weight > 0){
      break;
    }
    ++i;
  }

  return vk::UniqueSemaphore(outSemaphore, device->getDevice().get());
}

vk::UniqueSemaphore VulkanGridFluidSPHCoupling::run(const vk::Semaphore &semaphoreWait) {
  return run(std::vector<vk::Semaphore>{semaphoreWait});
}

const vk::UniqueFence &VulkanGridFluidSPHCoupling::getFenceAfterCompute() const { return fence; }

void VulkanGridFluidSPHCoupling::submit(
    VulkanGridFluidSPHCoupling::Stages pipelineStage, const vk::Fence submitFence,
    const std::optional<std::vector<vk::Semaphore>> &inSemaphore,
    const std::optional<vk::Semaphore> &outSemaphore) {

  std::array<vk::PipelineStageFlags, 2> waitStages{vk::PipelineStageFlagBits::eComputeShader,
                                                   vk::PipelineStageFlagBits::eComputeShader};
  recordCommandBuffer(pipelineStage);
  vk::SubmitInfo submitInfoCompute{
      .waitSemaphoreCount =
          static_cast<uint32_t>(inSemaphore.has_value() ? inSemaphore.value().size() : 1),
      .pWaitSemaphores = inSemaphore.has_value() ? inSemaphore.value().data()
                                                 : &semaphores[currentSemaphore - 1].get(),
      .pWaitDstStageMask = waitStages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer.get(),
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          outSemaphore.has_value() ? &outSemaphore.value() : &semaphores[currentSemaphore].get()};
  queue.submit(submitInfoCompute, submitFence);

  currentSemaphore++;
}
void VulkanGridFluidSPHCoupling::recordCommandBuffer(
    VulkanGridFluidSPHCoupling::Stages pipelineStage) {

  const auto &pipeline = pipelines[pipelineStage];
  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                       .pInheritanceInfo = nullptr};

  commandBuffer->begin(beginInfo);
  commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());
  commandBuffer->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
      &descriptorSets[pipelineStage]->getDescriptorSets()[0].get(), 0, nullptr);

  if (Utilities::isIn(pipelineStage, {Stages::Tag})) {
    commandBuffer->pushConstants(pipeline->getPipelineLayout().get(),
                                 vk::ShaderStageFlagBits::eCompute, 0, sizeof(GridInfo), &gridInfo);
  } else if (Utilities::isIn(pipelineStage, {Stages::WriteNewParticleTemps})) {
    commandBuffer->pushConstants(pipeline->getPipelineLayout().get(),
                                 vk::ShaderStageFlagBits::eCompute, 0, sizeof(SimulationInfoSPH),
                                 &simulationInfo.simulationInfoSPH);
  }
  auto dispatchCount = glm::ivec3(0.0);
  if (Utilities::isIn(pipelineStage,
                      {Stages::Tag, Stages::TransferHeatToParticles, Stages::WeightDistribution})) {
    dispatchCount = glm::ivec3(glm::ceil(glm::compMul(gridInfo.gridSize.xyz()) / 32.0), 1, 1);
  }
  if (Utilities::isIn(
          pipelineStage,
          {Stages::TransferHeatToCells, Stages::WriteNewParticleTemps, Stages::MassTransfer})) {
    dispatchCount =
        glm::ivec3(glm::ceil(simulationInfo.simulationInfoSPH.particleCount / 32.0), 1, 1);
  }
  commandBuffer->dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
  commandBuffer->end();
}

void VulkanGridFluidSPHCoupling::updateDescriptorSets() {
  std::for_each(pipelines.begin(), pipelines.end(), [&](auto &in) {
    const auto &[stage, _] = in;
    descriptorSets[stage]->updateDescriptorSet(descriptorBufferInfosCompute[stage],
                                               bindingInfosCompute[stage]);
  });
}

void VulkanGridFluidSPHCoupling::fillDescriptorBufferInfo() {
  const auto descriptorBufferIndexes =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferIndexes, 1},
                           .bufferSize = bufferIndexes->getSize()};
  const auto descriptorBufferParticles =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferParticles, 1},
                           .bufferSize = bufferParticles->getSize()};
  const auto descriptorBufferParticlesTempsNew =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferParticleTempsNew, 1},
                           .bufferSize = bufferParticleTempsNew->getSize()};
  const auto descriptorBufferGridOld =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridValuesOld, 1},
                           .bufferSize = bufferGridValuesOld->getSize()};
  const auto descriptorBufferGridNew =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridValuesNew, 1},
                           .bufferSize = bufferGridValuesNew->getSize()};
  const auto descriptorBufferGridSPH =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridSPH, 1},
                           .bufferSize = bufferGridSPH->getSize()};
  const auto descriptorBufferUniformSimualtionInfo = DescriptorBufferInfo{
      .buffer = std::span<std::shared_ptr<Buffer>>{&bufferUniformSimulationInfo, 1},
      .bufferSize = bufferUniformSimulationInfo->getSize()};
  const auto descriptorBufferGridVelocities =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferGridVelocities, 1},
                           .bufferSize = bufferGridVelocities->getSize()};
  const auto descriptorBufferHasPair =
      DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferHasPair, 1},
                           .bufferSize = bufferHasPair->getSize()};

  descriptorBufferInfosCompute[Stages::Tag] = {descriptorBufferIndexes};
  descriptorBufferInfosCompute[Stages::TransferHeatToParticles] = {
      descriptorBufferIndexes,
      descriptorBufferParticles,
      descriptorBufferParticlesTempsNew,
      descriptorBufferGridNew,
      descriptorBufferGridOld,
      descriptorBufferGridSPH,
      descriptorBufferUniformSimualtionInfo};
  descriptorBufferInfosCompute[Stages::TransferHeatToCells] = {
      descriptorBufferIndexes,
      descriptorBufferParticles,
      descriptorBufferParticlesTempsNew,
      descriptorBufferGridOld,
      descriptorBufferGridNew,
      descriptorBufferGridSPH,
      descriptorBufferUniformSimualtionInfo};
  descriptorBufferInfosCompute[Stages::WriteNewParticleTemps] = {descriptorBufferParticles,
                                                                 descriptorBufferParticlesTempsNew};
  descriptorBufferInfosCompute[Stages::MassTransfer] = {
      descriptorBufferIndexes, descriptorBufferParticles, descriptorBufferGridVelocities,
      descriptorBufferGridNew, descriptorBufferUniformSimualtionInfo};
  descriptorBufferInfosCompute[Stages::WeightDistribution] = {
      descriptorBufferIndexes, descriptorBufferParticles, descriptorBufferParticlesTempsNew,
      descriptorBufferGridSPH, descriptorBufferHasPair,   descriptorBufferUniformSimualtionInfo};
}

void VulkanGridFluidSPHCoupling::createBuffers() {
  const auto particleCount = simulationInfo.simulationInfoSPH.particleCount;
  auto bufferBuilder = BufferBuilder()
                           .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                                          | vk::BufferUsageFlagBits::eStorageBuffer)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal);

  bufferParticleTempsNew = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(glm::vec2) * particleCount), this->device, commandPool, queue);

  bufferHasPair = std::make_shared<Buffer>(bufferBuilder.setSize(sizeof(KeyValue) * particleCount),
                                           this->device, commandPool, queue);
  bufferHasPair->fill(std::vector<KeyValue>(particleCount, {-1, -1}));

  bufferBuilder.setUsageFlags(vk::BufferUsageFlagBits::eTransferDst
                              | vk::BufferUsageFlagBits::eUniformBuffer);

  bufferUniformSimulationInfo = std::make_shared<Buffer>(
      bufferBuilder.setSize(sizeof(SimulationInfo)), this->device, commandPool, queue);
  bufferUniformSimulationInfo->fill(std::array<SimulationInfo, 1>{simulationInfo});
}

void VulkanGridFluidSPHCoupling::waitFence() {
  device->getDevice()->waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
  device->getDevice()->resetFences(fence.get());
}

void VulkanGridFluidSPHCoupling::swapBuffers(std::shared_ptr<Buffer> &buffer1,
                                             std::shared_ptr<Buffer> &buffer2) {

  waitFence();
  buffer1.swap(buffer2);
  updateDescriptorSets();
}
