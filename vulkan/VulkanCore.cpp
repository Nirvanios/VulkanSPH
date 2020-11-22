//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/enumerate.hpp>
#include <span>
#include <utility>

#include "glm/gtc/matrix_transform.hpp"
#include "range/v3/view/zip.hpp"
#include "spdlog/spdlog.h"

#include "../utils/Utilities.h"
#include "Utils/VulkanUtils.h"
#include "VulkanCore.h"
#include "builders/ImageBuilder.h"
#include "builders/PipelineBuilder.h"

void VulkanCore::initVulkan(const Model &modelParticle, const std::span<ParticleRecord> particles) {
    spdlog::debug("Vulkan initialization...");
    instance = std::make_shared<Instance>(window.getWindowName(), config.getApp().DEBUG);
    createSurface();
    spdlog::debug("Created surface.");
    device = std::make_shared<Device>(instance, surface, config.getApp().DEBUG);
    queueGraphics = device->getGraphicsQueue();
    queuePresent = device->getPresentQueue();
    queueCompute = device->getComputeQueue();
    spdlog::debug("Queues created.");
    swapchain = std::make_shared<Swapchain>(device, surface, window);
    pipelineGraphics = PipelineBuilder{config, device, swapchain}
                               .setDepthFormat(findDepthFormat())
                               .setLayoutBindingInfo(bindingInfosRender)
                               .setPipelineType(PipelineType::Graphics)
                               .setVertexShaderPath(config.getVulkan().shaders.vertex)
                               .setFragmentShaderPath(config.getVulkan().shaders.fragemnt)
                               .build();
    auto computePipelineBuilder = PipelineBuilder{config, device, swapchain}
                                          .setLayoutBindingInfo(bindingInfosCompute)
                                          .setPipelineType(PipelineType::Compute)
                                          .addPushConstant(vk::ShaderStageFlagBits::eCompute, sizeof(SimulationInfo));
    pipelineComputeMassDensity = computePipelineBuilder.setComputeShaderPath(config.getVulkan().shaders.computeMassDensity).build();
    pipelineComputeForces = computePipelineBuilder.setComputeShaderPath(config.getVulkan().shaders.computeForces).build();
    createCommandPool();
    createDepthResources();
    framebuffers = std::make_shared<Framebuffers>(device, swapchain, pipelineGraphics->getRenderPass(), imageDepth->getImageView());
    createVertexBuffer(modelParticle.vertices);
    createIndexBuffer(modelParticle.indices);
    createUniformBuffers();
    createShaderStorageBuffer(particles);
    createDescriptorPool();
    std::array<DescriptorBufferInfo, 3> descriptorBufferInfosGraphic{
            DescriptorBufferInfo{.buffer = buffersUniformMVP, .bufferSize = sizeof(UniformBufferObject)},
            DescriptorBufferInfo{.buffer = buffersUniformCameraPos, .bufferSize = sizeof(glm::vec3)},
            DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferShaderStorage, 1}, .bufferSize = sizeof(ParticleRecord) * 32}};
    std::array<DescriptorBufferInfo, 1> descriptorBufferInfosCompute{
            DescriptorBufferInfo{.buffer = std::span<std::shared_ptr<Buffer>>{&bufferShaderStorage, 1}, .bufferSize = sizeof(ParticleRecord) * 32}};
    descriptorSetGraphics =
            std::make_shared<DescriptorSet>(device, swapchain->getSwapchainImageCount(), pipelineGraphics->getDescriptorSetLayout(), descriptorPool);
    descriptorSetGraphics->updateDescriptorSet(descriptorBufferInfosGraphic, bindingInfosRender);
    descriptorSetCompute = std::make_shared<DescriptorSet>(device, 1, pipelineComputeMassDensity->getDescriptorSetLayout(), descriptorPool);
    descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);
    //createDescriptorSet();
    spdlog::debug("Created command pool");
    createCommandBuffers();
    spdlog::debug("Created command buffers");
    createSyncObjects();
    spdlog::debug("Created semaphores.");
    spdlog::debug("Vulkan OK.");
}

void VulkanCore::run() {
    mainLoop();
    cleanup();
}

void VulkanCore::mainLoop() {
    while (!glfwWindowShouldClose(window.getWindow().get())) {
        glfwPollEvents();
        auto a = bufferShaderStorage->read<ParticleRecord>();
        spdlog::info(fmt::format("Pos = x: {} y: {} z: {}", a[1].position.x, a[1].position.y, a[1].position.z));
        spdlog::info(fmt::format("Vel = x: {} y: {} z: {}", a[1].velocity.x, a[1].velocity.y, a[1].velocity.z));
        drawFrame();
    }

    device->getDevice()->waitIdle();
}

void VulkanCore::cleanup() {}

VulkanCore::VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos)
    : indicesSize(0), cameraPos(cameraPos), config(config), window(window) {}

void VulkanCore::createSurface() {
    VkSurfaceKHR tmpSurface;
    if (glfwCreateWindowSurface(instance->getInstance(), window.getWindow().get(), nullptr, &tmpSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::UniqueSurfaceKHR{tmpSurface, instance->getInstance()};
}
void VulkanCore::createCommandPool() {
    auto queueFamilyIndices = Device::findQueueFamilies(device->getPhysicalDevice(), surface);

    vk::CommandPoolCreateInfo commandPoolCreateInfoGraphics{.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};
    vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                           .queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

    commandPoolGraphics = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoGraphics);
    commandPoolCompute = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
}
void VulkanCore::createCommandBuffers() {
    commandBuffersGraphic.clear();
    commandBuffersGraphic.resize(framebuffers->getFramebufferImageCount());
    commandBuffersCompute.clear();
    commandBuffersCompute.resize(framebuffers->getFramebufferImageCount());

    commandBuffersGraphic = device->allocateCommandBuffer(commandPoolGraphics, commandBuffersGraphic.size());
    commandBuffersCompute = device->allocateCommandBuffer(commandPoolCompute, commandBuffersCompute.size());

    int i = 0;
    std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto &commandBufferGraphics : commandBuffersGraphic) {
        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse, .pInheritanceInfo = nullptr};

        commandBufferGraphics->begin(beginInfo);

        std::vector<vk::ClearValue> clearValues(2);
        clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
        clearValues[1].setDepthStencil({1.0f, 0});

        vk::RenderPassBeginInfo renderPassBeginInfo{.renderPass = pipelineGraphics->getRenderPass(),
                                                    .framebuffer = swapchainFramebuffers[i].get(),
                                                    .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
                                                    .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                                                    .pClearValues = clearValues.data()};

        commandBufferGraphics->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBufferGraphics->bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipeline().get());
        commandBufferGraphics->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
        commandBufferGraphics->bindIndexBuffer(bufferIndex->getBuffer().get(), 0, vk::IndexType::eUint16);
        commandBufferGraphics->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineGraphics->getPipelineLayout().get(), 0, 1,
                                                  &descriptorSetGraphics->getDescriptorSets()[i].get(), 0, nullptr);
        commandBufferGraphics->drawIndexed(indicesSize, config.getApp().simulation.particleCount, 0, 0, 0);
        commandBufferGraphics->endRenderPass();

        commandBufferGraphics->end();
        ++i;
    }
}
void VulkanCore::createSyncObjects() {
    imagesInFlight.resize(swapchain->getSwapchainImageCount());

    for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        semaphoreImageAvailable.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        semaphoreRenderFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        semaphoreComputeFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        inFlightFences.emplace_back(device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
}
void VulkanCore::drawFrame() {
    std::array<vk::Semaphore, 1> semaphoresAfterNextImage{semaphoreImageAvailable[currentFrame].get()};
    std::array<vk::Semaphore, 1> semaphoresAfterComputeMassDensity{semaphoreComputeFinished[currentFrame].get()};
    std::array<vk::Semaphore, 1> semaphoresAfterComputeForces{semaphoreRenderFinished[currentFrame].get()};
    std::array<vk::Semaphore, 1> semaphoresAfterRender{semaphoreRenderFinished[currentFrame].get()};
    std::array<vk::PipelineStageFlags, 1> waitStagesRender{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::PipelineStageFlags, 1> waitStagesCompute{vk::PipelineStageFlagBits::eComputeShader};
    std::array<vk::SwapchainKHR, 1> swapchains{swapchain->getSwapchain().get()};
    uint32_t imageindex;

    device->getDevice()->waitForFences(inFlightFences[currentFrame].get(), VK_TRUE, UINT64_MAX);
    try {
        device->getDevice()->acquireNextImageKHR(swapchain->getSwapchain().get(), UINT64_MAX, *semaphoresAfterNextImage.begin(), nullptr, &imageindex);
    } catch (const std::exception &e) {
        recreateSwapchain();
        return;
    }
    updateUniformBuffers(imageindex);

    if (imagesInFlight[imageindex].has_value()) device->getDevice()->waitForFences(imagesInFlight[imageindex].value(), VK_TRUE, UINT64_MAX);
    imagesInFlight[imageindex] = inFlightFences[currentFrame].get();

    device->getDevice()->resetFences(inFlightFences[currentFrame].get());
    recordCommandBuffersCompute(pipelineComputeMassDensity);
    vk::SubmitInfo submitInfoCompute{.waitSemaphoreCount = 1,
                                     .pWaitSemaphores = semaphoresAfterNextImage.data(),
                                     .pWaitDstStageMask = waitStagesCompute.data(),
                                     .commandBufferCount = 1,
                                     .pCommandBuffers = &commandBuffersCompute[imageindex].get(),
                                     .signalSemaphoreCount = 1,
                                     .pSignalSemaphores = semaphoresAfterComputeMassDensity.data()};
    queueCompute.submit(submitInfoCompute, inFlightFences[currentFrame].get());

    recordCommandBuffersCompute(pipelineComputeForces);
    submitInfoCompute.pCommandBuffers = &commandBuffersCompute[imageindex].get();
    submitInfoCompute.pWaitSemaphores = semaphoresAfterComputeMassDensity.data();
    submitInfoCompute.pSignalSemaphores = semaphoresAfterComputeForces.data();
    queueCompute.submit(submitInfoCompute, nullptr);


    vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = semaphoresAfterComputeForces.data(),
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


    queueGraphics.submit(submitInfoRender, nullptr);

    try {
        queuePresent.presentKHR(presentInfo);
    } catch (const vk::OutOfDateKHRError &e) { recreateSwapchain(); }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

void VulkanCore::setFramebufferResized(bool framebufferResized) { VulkanCore::framebufferResized = framebufferResized; }

void VulkanCore::createVertexBuffer(const std::vector<Vertex> &vertices) {
    bufferVertex = std::make_shared<Buffer>(BufferBuilder()
                                                    .setSize(sizeof(vertices[0]) * vertices.size())
                                                    .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer)
                                                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                            device, commandPoolGraphics, queueGraphics);
    bufferVertex->fill(vertices);
}

void VulkanCore::createIndexBuffer(const std::vector<uint16_t> &indices) {
    bufferIndex = std::make_shared<Buffer>(BufferBuilder()
                                                   .setSize(sizeof(indices[0]) * indices.size())
                                                   .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer)
                                                   .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                           device, commandPoolGraphics, queueGraphics);
    bufferIndex->fill(indices);
    indicesSize = indices.size();
}
void VulkanCore::createUniformBuffers() {
    vk::DeviceSize size = sizeof(UniformBufferObject);
    auto builderMVP = BufferBuilder()
                              .setSize(size)
                              .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                              .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer);
    auto builderCameraPos = BufferBuilder()
                                    .setSize(size)
                                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                                    .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer);

    for ([[maybe_unused]] const auto &swapImage : swapchain->getSwapChainImageViews()) {
        buffersUniformMVP.emplace_back(std::make_shared<Buffer>(builderMVP, device, commandPoolGraphics, queueGraphics));
        buffersUniformCameraPos.emplace_back(std::make_shared<Buffer>(builderCameraPos, device, commandPoolGraphics, queueGraphics));
    }
}
void VulkanCore::updateUniformBuffers(uint32_t currentImage) {
    UniformBufferObject ubo{.model = glm::mat4{1.0f},
                            .view = viewMatrixGetter(),
                            .proj = glm::perspective(glm::radians(45.0f),
                                                     static_cast<float>(swapchain->getExtentWidth()) / static_cast<float>(swapchain->getExtentHeight()), 0.01f,
                                                     1000.f)};
    ubo.proj[1][1] *= -1;
    buffersUniformMVP[currentImage]->fill(std::span(&ubo, 1), false);
    buffersUniformCameraPos[currentImage]->fill(std::span(&cameraPos, 1), false);
}
void VulkanCore::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 2> poolSize{vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                                                                          .descriptorCount = static_cast<uint32_t>(swapchain->getSwapchainImageCount()) * 2},
                                                   vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 3}};
    vk::DescriptorPoolCreateInfo poolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets =
                    [poolSize] {
                        uint32_t i = 0;
                        std::for_each(poolSize.begin(), poolSize.end(), [&i](const auto &in) { i += in.descriptorCount; });
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

    imageDepth->transitionImageLayout(device, commandPoolGraphics, queueGraphics, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

vk::Format VulkanCore::findDepthFormat() {
    return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal,
                               vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format VulkanCore::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, const vk::FormatFeatureFlags &features) {
    for (const auto &format : candidates) {
        auto properties = device->getPhysicalDevice().getFormatProperties(format);
        if ((tiling == vk::ImageTiling::eLinear && (properties.linearTilingFeatures & features) == features) ||
            (tiling == vk::ImageTiling::eOptimal && (properties.optimalTilingFeatures & features) == features))
            return format;
    }
    throw std::runtime_error("Failed to find supported format!");
}

void VulkanCore::setViewMatrixGetter(std::function<glm::mat4()> getter) { viewMatrixGetter = getter; }

void VulkanCore::createShaderStorageBuffer(const std::span<ParticleRecord> &particles) {

    bufferShaderStorage = std::make_shared<Buffer>(
            BufferBuilder()
                    .setSize(sizeof(ParticleRecord) * particles.size())
                    .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer)
                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
            device, commandPoolGraphics, queueGraphics);
    bufferShaderStorage->fill(particles);
}
void VulkanCore::recordCommandBuffersCompute(const std::shared_ptr<Pipeline> &pipeline) {
    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse, .pInheritanceInfo = nullptr};

    for (auto &commandBufferCompute : commandBuffersCompute) {
        commandBufferCompute->begin(beginInfo);
        commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->getPipeline().get());
        commandBufferCompute->bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline->getPipelineLayout().get(), 0, 1,
                                                 &descriptorSetCompute->getDescriptorSets()[0].get(), 0, nullptr);
        commandBufferCompute->pushConstants(pipeline->getPipelineLayout().get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(SimulationInfo), &simulationInfo);
        commandBufferCompute->dispatch(1, 1, 1);
        commandBufferCompute->end();
    }
}
void VulkanCore::setSimulationInfo(const SimulationInfo &simulationInfo) { VulkanCore::simulationInfo = simulationInfo; }
