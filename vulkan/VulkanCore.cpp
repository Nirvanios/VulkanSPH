//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>
#include <span>

#include "glm/gtc/matrix_transform.hpp"
#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"
#include "VulkanUtils.h"

void VulkanCore::initVulkan() {
    //glfwSetWindowUserPointer(window.getWindow().get(), this);
    instance = std::make_shared<Instance>(window.getWindowName(), debug);
    createSurface();
    spdlog::debug("Created surface.");
    device = std::make_shared<Device>(instance, surface, debug);
    graphicsQueue = device->getGraphicsQueue();
    presentQueue = device->getPresentQueue();
    swapchain = std::make_shared<Swapchain>(device, surface, window);
    pipeline = std::make_shared<Pipeline>(device, swapchain, findDepthFormat());
    createCommandPool();
    createDepthResources();
    framebuffers = std::make_shared<Framebuffers>(device, swapchain, pipeline->getRenderPass(), depthImageView);
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSet();
    spdlog::debug("Created command pool");
    createCommandBuffers();
    spdlog::debug("Created command buffers");
    createSyncObjects();
    spdlog::debug("Created semaphores.");
}

void VulkanCore::run() {
    auto a = window.subscribeToKeyEvents([](KeyMessage message) {
        if (message.action == KeyAction::Release) spdlog::info(message.key);
    });
    mainLoop();
    cleanup();
    a.unsubscribe();
}

void VulkanCore::mainLoop() {
    while (!glfwWindowShouldClose(window.getWindow().get())) {
        glfwPollEvents();
        //glfwWaitEvents();
        drawFrame();
    }


    device->getDevice()->waitIdle();
}

void VulkanCore::cleanup() {}

VulkanCore::VulkanCore(GlfwWindow &window, bool debug) : debug(debug), window(window) {
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

    vk::CommandPoolCreateInfo commandPoolCreateInfo{.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

    commandPool = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfo);
}
void VulkanCore::createCommandBuffers() {
    commandBuffers.clear();
    commandBuffers.resize(framebuffers->getSwapchainFramebuffers().size());
    vk::CommandBufferAllocateInfo bufferAllocateInfo{.commandPool = commandPool.get(),
                                                     .level = vk::CommandBufferLevel::ePrimary,
                                                     .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};

    commandBuffers = device->getDevice()->allocateCommandBuffersUnique(bufferAllocateInfo);

    int i = 0;
    std::array<vk::Buffer, 1> vertexBuffers{vertexBuffer->getBuffer().get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto &commandBuffer : commandBuffers) {
        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse, .pInheritanceInfo = nullptr};

        commandBuffer->begin(beginInfo);

        std::vector<vk::ClearValue> clearValues(2);
        clearValues[0].setColor({std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}});
        clearValues[1].setDepthStencil({1.0f, 0});

        vk::RenderPassBeginInfo renderPassBeginInfo{.renderPass = pipeline->getRenderPass(),
                                                    .framebuffer = swapchainFramebuffers[i].get(),
                                                    .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
                                                    .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                                                    .pClearValues = clearValues.data()};

        commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->getPipeline().get());
        commandBuffer->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
        commandBuffer->bindIndexBuffer(indexBuffer->getBuffer().get(), 0, vk::IndexType::eUint16);
        commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout().get(), 0, 1, &descriptorSets[i].get(), 0, nullptr);
        commandBuffer->drawIndexed(indices.size(), 1, 0, 0, 0);
        //commandBuffer->draw(vertices.size(), 1, 0, 0);
        commandBuffer->endRenderPass();

        commandBuffer->end();
        ++i;
    }
}
void VulkanCore::createSyncObjects() {
    imagesInFlight.resize(swapchain->getSwapChainImageViews().size());

    for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphore.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        renderFinishedSemaphore.emplace_back(device->getDevice()->createSemaphoreUnique({}));
        inFlightFences.emplace_back(device->getDevice()->createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
}
void VulkanCore::drawFrame() {
    std::array<vk::Semaphore, 1> waitSemaphores{imageAvailableSemaphore[currentFrame].get()};
    std::array<vk::Semaphore, 1> signalSemaphores{renderFinishedSemaphore[currentFrame].get()};
    std::array<vk::PipelineStageFlags, 1> waitStages{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    std::array<vk::SwapchainKHR, 1> swapchains{swapchain->getSwapchain().get()};
    uint32_t imageindex;

    device->getDevice()->waitForFences(inFlightFences[currentFrame].get(), VK_TRUE, UINT64_MAX);
    try {
        device->getDevice()->acquireNextImageKHR(swapchain->getSwapchain().get(), UINT64_MAX, imageAvailableSemaphore[currentFrame].get(), nullptr,
                                                 &imageindex);
    } catch (const std::exception &e) {
        recreateSwapchain();
        return;
    }
    updateUniformBuffers(imageindex);

    if (imagesInFlight[imageindex].has_value()) device->getDevice()->waitForFences(imagesInFlight[imageindex].value(), VK_TRUE, UINT64_MAX);
    //inFlightFences[currentFrame].swap(imagesInFlight[imageindex].value());
    imagesInFlight[imageindex] = inFlightFences[currentFrame].get();


    vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                              .pWaitSemaphores = waitSemaphores.data(),
                              .pWaitDstStageMask = waitStages.data(),
                              .commandBufferCount = 1,
                              .pCommandBuffers = &commandBuffers[imageindex].get(),
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = signalSemaphores.data()};
    vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                   .pWaitSemaphores = signalSemaphores.data(),
                                   .swapchainCount = 1,
                                   .pSwapchains = swapchains.data(),
                                   .pImageIndices = &imageindex,
                                   .pResults = nullptr};

    device->getDevice()->resetFences(inFlightFences[currentFrame].get());
    graphicsQueue.submit(submitInfo, inFlightFences[currentFrame].get());

    try {
        presentQueue.presentKHR(presentInfo);
    } catch (const vk::OutOfDateKHRError &e) { recreateSwapchain(); }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
void VulkanCore::recreateSwapchain() {
    window.checkMinimized();
    device->getDevice().get().waitIdle();

    swapchain->createSwapchain();
    swapchain->createImageViews();
    pipeline->createRenderPass();
    pipeline->createGraphicsPipeline();
    framebuffers->createFramebuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSet();
    createCommandBuffers();
}

bool VulkanCore::isFramebufferResized() const { return framebufferResized; }

void VulkanCore::setFramebufferResized(bool framebufferResized) { VulkanCore::framebufferResized = framebufferResized; }

void VulkanCore::createVertexBuffer() {
    vertexBuffer = std::make_shared<Buffer>(BufferBuilder()
                                                    .setSize(sizeof(vertices[0]) * vertices.size())
                                                    .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer)
                                                    .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                            device, commandPool, graphicsQueue);
    vertexBuffer->fill(vertices);
}

void VulkanCore::createIndexBuffer() {
    indexBuffer = std::make_shared<Buffer>(BufferBuilder()
                                                   .setSize(sizeof(indices[0]) * indices.size())
                                                   .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer)
                                                   .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eDeviceLocal),
                                           device, commandPool, graphicsQueue);
    indexBuffer->fill(indices);
}
void VulkanCore::createUniformBuffers() {
    vk::DeviceSize size = sizeof(UniformBufferObject);
    auto builder = BufferBuilder()
                           .setSize(size)
                           .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
                           .setUsageFlags(vk::BufferUsageFlagBits::eUniformBuffer);

    for ([[maybe_unused]] const auto &swapImage : swapchain->getSwapChainImageViews()) {
        unifromsBuffers.emplace_back(std::make_shared<Buffer>(builder, device, commandPool, graphicsQueue));
    }
}
void VulkanCore::updateUniformBuffers(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    UniformBufferObject ubo{.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                            .view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                            .proj = glm::perspective(glm::radians(45.0f),
                                                     swapchain->getSwapchainExtent().width / static_cast<float>(swapchain->getSwapchainExtent().height), 0.1f,
                                                     10.f)};
    ubo.proj[1][1] *= -1;
    unifromsBuffers[currentImage]->fill(std::span(&ubo, 1), false);
}
void VulkanCore::createDescriptorPool() {
    vk::DescriptorPoolSize poolSize{.descriptorCount = static_cast<uint32_t>(swapchain->getSwapChainImageViews().size())};
    vk::DescriptorPoolCreateInfo poolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = static_cast<uint32_t>(swapchain->getSwapChainImageViews().size()),
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
    };
    descriptorPool = device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);
}
void VulkanCore::createDescriptorSet() {
    std::vector<vk::DescriptorSetLayout> layouts(swapchain->getSwapChainImageViews().size(), pipeline->getDescriptorSetLayout().get());
    vk::DescriptorSetAllocateInfo allocateInfo{.descriptorPool = descriptorPool.get(),
                                               .descriptorSetCount = static_cast<uint32_t>(swapchain->getSwapChainImageViews().size()),
                                               .pSetLayouts = layouts.data()};

    descriptorSets.resize(swapchain->getSwapChainImageViews().size());
    descriptorSets = device->getDevice()->allocateDescriptorSetsUnique(allocateInfo);

    for (size_t i = 0; i < descriptorSets.size(); i++) {
        vk::DescriptorBufferInfo bufferInfo{.buffer = unifromsBuffers[i]->getBuffer().get(), .offset = 0, .range = sizeof(UniformBufferObject)};
        vk::WriteDescriptorSet writeDescriptorSet{.dstSet = descriptorSets[i].get(),
                                                  .dstBinding = 0,
                                                  .dstArrayElement = 0,
                                                  .descriptorCount = 1,
                                                  .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                  .pImageInfo = nullptr,
                                                  .pBufferInfo = &bufferInfo,
                                                  .pTexelBufferView = nullptr};
        device->getDevice()->updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
    }
}
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
    vk::CommandBufferAllocateInfo allocateInfo{.commandPool = commandPool.get(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
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

    graphicsQueue.submit(1, &submitInfo, nullptr);
    graphicsQueue.waitIdle();
}
vk::UniqueImageView VulkanCore::createImageView(const vk::UniqueImage &image, vk::Format format, const vk::ImageAspectFlags &aspectFlags) {
    vk::ImageViewCreateInfo viewCreateInfo{
            .image = image.get(),
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    return device->getDevice()->createImageViewUnique(viewCreateInfo);
}
