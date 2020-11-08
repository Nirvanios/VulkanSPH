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
#include "VulkanCore.h"
#include "VulkanUtils.h"
#include "builders/PipelineBuilder.h"

void VulkanCore::initVulkan() {
    //glfwSetWindowUserPointer(window.getWindow().get(), this);
    instance = std::make_shared<Instance>(window.getWindowName(), config.getApp().DEBUG);
    createSurface();
    spdlog::debug("Created surface.");
    device = std::make_shared<Device>(instance, surface, config.getApp().DEBUG);
    queueGraphics = device->getGraphicsQueue();
    queuePresent = device->getPresentQueue();
    queueCompute = device->getComputeQueue();
    swapchain = std::make_shared<Swapchain>(device, surface, window);
    pipelineGraphics = PipelineBuilder{config, device, swapchain}
                               .setDepthFormat(findDepthFormat())
                               .setLayoutBindingInfo(bindingInfosRender)
                               .setPipelineType(PipelineType::Graphics)
                               .setVertexShaderPath(config.getVulkan().shaders.vertex)
                               .setFragmentShaderPath(config.getVulkan().shaders.fragemnt)
                               .build();
    pipelineCompute = PipelineBuilder{config, device, swapchain}
                              .setLayoutBindingInfo(bindingInfosCompute)
                              .setPipelineType(PipelineType::Compute)
                              .setComputeShaderPath(config.getVulkan().shaders.compute)
                              .build();
    createCommandPool();
    createDepthResources();
    framebuffers = std::make_shared<Framebuffers>(device, swapchain, pipelineGraphics->getRenderPass(), depthImageView);
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createShaderStorageBuffer();
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
    descriptorSetCompute = std::make_shared<DescriptorSet>(device, 1, pipelineCompute->getDescriptorSetLayout(), descriptorPool);
    descriptorSetCompute->updateDescriptorSet(descriptorBufferInfosCompute, bindingInfosCompute);
    //createDescriptorSet();
    spdlog::debug("Created command pool");
    createCommandBuffers();
    spdlog::debug("Created command buffers");
    createSyncObjects();
    spdlog::debug("Created semaphores.");
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

VulkanCore::VulkanCore(Config config, GlfwWindow &window, const glm::vec3 &cameraPos, Model model)
    : model(std::move(model)), cameraPos(cameraPos), config(std::move(config)), window(window) {
    spdlog::debug("Vulkan initialization...");
    initVulkan();
    spdlog::debug("Vulkan OK.");
}

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
    vk::CommandPoolCreateInfo commandPoolCreateInfoCompute{.queueFamilyIndex = queueFamilyIndices.computeFamily.value()};

    commandPoolGraphics = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoGraphics);
    commandPoolCompute = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoCompute);
}
void VulkanCore::createCommandBuffers() {
    commandBuffersGraphic.clear();
    commandBuffersGraphic.resize(framebuffers->getSwapchainFramebuffers().size());
    commandBuffersCompute.clear();
    commandBuffersCompute.resize(framebuffers->getSwapchainFramebuffers().size());
    vk::CommandBufferAllocateInfo bufferAllocateInfoGraphics{.commandPool = commandPoolGraphics.get(),
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = static_cast<uint32_t>(commandBuffersGraphic.size())};
    commandBuffersCompute.resize(framebuffers->getSwapchainFramebuffers().size());
    vk::CommandBufferAllocateInfo bufferAllocateInfoCompute{.commandPool = commandPoolCompute.get(),
                                                            .level = vk::CommandBufferLevel::ePrimary,
                                                            .commandBufferCount = static_cast<uint32_t>(commandBuffersCompute.size())};

    commandBuffersGraphic = device->getDevice()->allocateCommandBuffersUnique(bufferAllocateInfoGraphics);
    commandBuffersCompute = device->getDevice()->allocateCommandBuffersUnique(bufferAllocateInfoCompute);

    int i = 0;
    std::array<vk::Buffer, 1> vertexBuffers{bufferVertex->getBuffer().get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto [commandBufferGraphics, commandBufferCompute] : ranges::views::zip(commandBuffersGraphic, commandBuffersCompute)) {
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
        commandBufferGraphics->drawIndexed(model.indices.size(), config.getApp().simulation.particleCount, 0, 0, 0);
        commandBufferGraphics->endRenderPass();

        commandBufferGraphics->end();

        commandBufferCompute->begin(beginInfo);
        commandBufferCompute->bindPipeline(vk::PipelineBindPoint::eCompute, pipelineCompute->getPipeline().get());
        commandBufferCompute->bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineCompute->getPipelineLayout().get(), 0, 1,
                                                 &descriptorSetCompute->getDescriptorSets()[0].get(), 0, nullptr);
        commandBufferCompute->dispatch(1, 1, 1);
        commandBufferCompute->end();
        ++i;
    }
}
void VulkanCore::createSyncObjects() {
    imagesInFlight.resize(swapchain->getSwapChainImageViews().size());

    for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        semaphoreImageAvailable.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        semaphoreRenderFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        semaphoreComputeFinished.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        inFlightFences.emplace_back(device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
}
void VulkanCore::drawFrame() {
    std::array<vk::Semaphore, 1> semaphoresAfterNextImage{semaphoreImageAvailable[currentFrame].get()};
    std::array<vk::Semaphore, 1> semaphoresAfterCompute{semaphoreComputeFinished[currentFrame].get()};
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


    vk::SubmitInfo submitInfoCompute{.waitSemaphoreCount = 1,
                                     .pWaitSemaphores = semaphoresAfterNextImage.data(),
                                     .pWaitDstStageMask = waitStagesCompute.data(),
                                     .commandBufferCount = 1,
                                     .pCommandBuffers = &commandBuffersCompute[imageindex].get(),
                                     .signalSemaphoreCount = 1,
                                     .pSignalSemaphores = semaphoresAfterCompute.data()};
    vk::SubmitInfo submitInfoRender{.waitSemaphoreCount = 1,
                                    .pWaitSemaphores = semaphoresAfterCompute.data(),
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

    device->getDevice()->resetFences(inFlightFences[currentFrame].get());
    queueCompute.submit(submitInfoCompute, inFlightFences[currentFrame].get());
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

void VulkanCore::createVertexBuffer() {
    bufferVertex = std::make_shared<Buffer>(BufferBuilder()
                                                    .setSize(sizeof(model.vertices[0]) * model.vertices.size())
                                                    .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer)
                                                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                            device, commandPoolGraphics, queueGraphics);
    bufferVertex->fill(model.vertices);
}

void VulkanCore::createIndexBuffer() {
    bufferIndex = std::make_shared<Buffer>(BufferBuilder()
                                                   .setSize(sizeof(model.indices[0]) * model.indices.size())
                                                   .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer)
                                                   .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                           device, commandPoolGraphics, queueGraphics);
    bufferIndex->fill(model.indices);
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
                                                     swapchain->getSwapchainExtent().width / static_cast<float>(swapchain->getSwapchainExtent().height), 0.01f,
                                                     1000.f)};
    ubo.proj[1][1] *= -1;
    buffersUniformMVP[currentImage]->fill(std::span(&ubo, 1), false);
    buffersUniformCameraPos[currentImage]->fill(std::span(&cameraPos, 1), false);
}
void VulkanCore::createDescriptorPool() {
    std::array<vk::DescriptorPoolSize, 2> poolSize{
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = static_cast<uint32_t>(swapchain->getSwapchainImageCount())},
            vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = 1}};
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
/*void VulkanCore::createDescriptorSet() {
    std::vector<vk::DescriptorSetLayout> layouts(swapchain->getSwapChainImageViews().size(), pipelineGraphics->getDescriptorSetLayout().get());
    vk::DescriptorSetAllocateInfo allocateInfo{.descriptorPool = descriptorPool.get(),
                                               .descriptorSetCount = static_cast<uint32_t>(swapchain->getSwapChainImageViews().size()),
                                               .pSetLayouts = layouts.data()};

    descriptorSetGraphics.resize(swapchain->getSwapChainImageViews().size());
    descriptorSetGraphics = device->getDevice()->allocateDescriptorSetsUnique(allocateInfo);

    for (size_t i = 0; i < descriptorSetGraphics.size(); i++) {
        std::array<vk::DescriptorBufferInfo, 2> bufferInfos{
                vk::DescriptorBufferInfo{.buffer = buffersUniformMVP[i]->getBuffer().get(), .offset = 0, .range = sizeof(UniformBufferObject)},
                vk::DescriptorBufferInfo{.buffer = buffersUniformCameraPos[i]->getBuffer().get(), .offset = 0, .range = sizeof(glm::vec3)}};
        std::vector<vk::WriteDescriptorSet> writeDescriptorSet;
        for (auto [index, bindingInfo] : bindingInfosRender | ranges::views::enumerate) {

            writeDescriptorSet.emplace_back(vk::WriteDescriptorSet{.dstSet = descriptorSetGraphics[i].get(),
                                                                   .dstBinding = bindingInfo.binding,
                                                                   .dstArrayElement = 0,
                                                                   .descriptorCount = bindingInfo.descriptorCount,
                                                                   .descriptorType = bindingInfo.descriptorType,
                                                                   .pImageInfo = nullptr,
                                                                   .pBufferInfo = &bufferInfos[index],
                                                                   .pTexelBufferView = nullptr});
        }
        device->getDevice()->updateDescriptorSets(writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);
    }
}*/
void VulkanCore::createDepthResources() {
    auto depthFormat = findDepthFormat();
    createImage(swapchain->getSwapchainExtent().width, swapchain->getSwapchainExtent().height, depthFormat, vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
    transitionImageLayout(depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

bool VulkanCore::hasStencilComponent(vk::Format format) { return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint; }

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
void VulkanCore::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, const vk::ImageUsageFlags &usage,
                             const vk::MemoryPropertyFlags &properties, vk::UniqueImage &image, vk::UniqueDeviceMemory &imageMemory) {
    vk::ImageCreateInfo imageCreateInfo{.imageType = vk::ImageType::e2D,
                                        .format = format,
                                        .extent = {.width = width, .height = height, .depth = 1},
                                        .mipLevels = 1,
                                        .arrayLayers = 1,
                                        .samples = vk::SampleCountFlagBits::e1,
                                        .tiling = tiling,
                                        .usage = usage,
                                        .sharingMode = vk::SharingMode::eExclusive,
                                        .initialLayout = vk::ImageLayout::eUndefined};
    image = device->getDevice()->createImageUnique(imageCreateInfo);

    auto memRequirements = device->getDevice()->getImageMemoryRequirements(image.get());
    vk::MemoryAllocateInfo allocateInfo{.allocationSize = memRequirements.size,
                                        .memoryTypeIndex = VulkanUtils::findMemoryType(device, memRequirements.memoryTypeBits, properties)};
    imageMemory = device->getDevice()->allocateMemoryUnique(allocateInfo);
    device->getDevice()->bindImageMemory(image.get(), imageMemory.get(), 0);
}
void VulkanCore::transitionImageLayout(const vk::UniqueImage &image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandBufferAllocateInfo allocateInfo{.commandPool = commandPoolGraphics.get(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
    auto commandBuffer = device->getDevice()->allocateCommandBuffersUnique(allocateInfo);

    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffer[0]->begin(beginInfo);

    vk::ImageMemoryBarrier barrier{
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image.get(),
            .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1},
    };


    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);

        if (hasStencilComponent(format)) { barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil; }
    } else {
        barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    }

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    commandBuffer[0]->pipelineBarrier(sourceStage, destinationStage, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    commandBuffer[0]->end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &commandBuffer[0].get()};

    queueGraphics.submit(1, &submitInfo, nullptr);
    queueGraphics.waitIdle();
}
vk::UniqueImageView VulkanCore::createImageView(const vk::UniqueImage &image, vk::Format format, const vk::ImageAspectFlags &aspectFlags) {
    vk::ImageViewCreateInfo viewCreateInfo{
            .image = image.get(),
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    return device->getDevice()->createImageViewUnique(viewCreateInfo);
}
void VulkanCore::setViewMatrixGetter(std::function<glm::mat4()> getter) { viewMatrixGetter = getter; }

void VulkanCore::createShaderStorageBuffer() {
    std::array<ParticleRecord, 32> data{};
    std::srand(std::time(nullptr));
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < 2; ++z) {
                data[(z * 2) + (y * 4) + x].position = glm::vec4{x, y, z, 0.0f};
                data[(z * 2) + (y * 4) + x].velocity = glm::vec4{std::rand(), std::rand(), std::rand(), 0.0f};
                data[(z * 2) + (y * 4) + x].velocity /= RAND_MAX;
                data[(z * 2) + (y * 4) + x].velocity *= 1;
            }
        }
    }
    bufferShaderStorage = std::make_shared<Buffer>(
            BufferBuilder()
                    .setSize(sizeof(ParticleRecord) * data.size())
                    .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer)
                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
            device, commandPoolGraphics, queueGraphics);
    bufferShaderStorage->fill(data);
}
