//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

#include "glm/gtc/matrix_transform.hpp"
#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"

void VulkanCore::initVulkan() {
    glfwSetWindowUserPointer(window.getWindow().get(), this);
    instance = std::make_shared<Instance>(window.getWindowName(), debug);
    createSurface();
    spdlog::debug("Created surface.");
    device = std::make_shared<Device>(instance, surface, debug);
    graphicsQueue = device->getGraphicsQueue();
    presentQueue = device->getPresentQueue();
    swapchain = std::make_shared<Swapchain>(device, surface, window);
    pipeline = std::make_shared<Pipeline>(device, swapchain);
    framebuffers = std::make_shared<Framebuffers>(device, swapchain, pipeline->getRenderPass());
    createCommandPool();
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
    mainLoop();
    cleanup();
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
    vk::ObjectDestroy<vk::Instance, vk::DispatchLoaderStatic> surfaceDeleter(instance->getInstance());
    surface = vk::UniqueSurfaceKHR{tmpSurface, surfaceDeleter};
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
    std::array<vk::Buffer, 1> vertexBuffers{vertexBuffer.get()};
    std::array<vk::DeviceSize, 1> offsets{0};
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto &commandBuffer : commandBuffers) {
        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse, .pInheritanceInfo = nullptr};

        commandBuffer->begin(beginInfo);

        vk::ClearValue clearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        vk::RenderPassBeginInfo renderPassBeginInfo{.renderPass = pipeline->getRenderPass(),
                                                    .framebuffer = swapchainFramebuffers[i].get(),
                                                    .renderArea = {.offset = {0, 0}, .extent = swapchain->getSwapchainExtent()},
                                                    .clearValueCount = 1,
                                                    .pClearValues = &clearValue};

        commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->getPipeline().get());
        commandBuffer->bindVertexBuffers(0, 1, vertexBuffers.data(), offsets.data());
        commandBuffer->bindIndexBuffer(indexBuffer.get(), 0, vk::IndexType::eUint16);
        commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline->getPipelineLayout().get(), 0, 1, &descriptorSets[i].get(), 0, nullptr);
        commandBuffer->drawIndexed(indices.size(), 1, 0, 0, 0);
        //commandBuffer->draw(vertices.size(), 1, 0, 0);
        commandBuffer->endRenderPass();

        commandBuffer->end();
        ++i;
    }
}
void VulkanCore::createSyncObjects() {
    for ([[maybe_unused]] const auto &image : swapchain->getSwapChainImageViews()) {
        imagesInFlight.emplace_back(device->getDevice()->createFence({.flags = vk::FenceCreateFlagBits::eSignaled}));
    }
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
    vk::Result result;
    uint32_t imageindex;

    result = device->getDevice()->waitForFences(inFlightFences[currentFrame].get(), VK_TRUE, UINT64_MAX);
    try {
        result = device->getDevice()->acquireNextImageKHR(swapchain->getSwapchain().get(), UINT64_MAX, imageAvailableSemaphore[currentFrame].get(), nullptr,
                                                          &imageindex);
    } catch (const std::exception &e) {
        recreateSwapchain();
        return;
    }
    updateUniformBuffers(imageindex);

    result = device->getDevice()->waitForFences(imagesInFlight[imageindex], VK_TRUE, UINT64_MAX);
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
        result = presentQueue.presentKHR(presentInfo);
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
    auto size = sizeof(vertices[0]) * vertices.size();
    vk::UniqueBuffer stagingBuffer;
    vk::UniqueDeviceMemory stagingBufferMemory;
    createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                 stagingBuffer, stagingBufferMemory);

    auto data = device->getDevice()->mapMemory(stagingBufferMemory.get(), 0, size);
    memcpy(data, vertices.data(), size);
    device->getDevice()->unmapMemory(stagingBufferMemory.get());

    createBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer,
                 vertexBufferMemory);
    copyBuffer(stagingBuffer, vertexBuffer, size);
}


uint32_t VulkanCore::findMemoryType(uint32_t typeFilter, const vk::MemoryPropertyFlags &properties) {
    auto memProperties = device->getPhysicalDevice().getMemoryProperties();
    uint32_t i = 0;
    for (const auto &type : memProperties.memoryTypes) {
        if ((typeFilter & (1 << i)) && (type.propertyFlags & properties)) { return i; }
        ++i;
    }

    throw std::runtime_error("failed to find suitable memory type!");
}
void VulkanCore::createBuffer(vk::DeviceSize size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::UniqueBuffer &buffer,
                              vk::UniqueDeviceMemory &memory) {

    vk::BufferCreateInfo bufferCreateInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};

    buffer = device->getDevice()->createBufferUnique(bufferCreateInfo);

    auto memRequirements = device->getDevice()->getBufferMemoryRequirements(buffer.get());
    auto memProperties = device->getPhysicalDevice().getMemoryProperties();
    vk::MemoryAllocateInfo allocateInfo{.allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};

    memory = device->getDevice()->allocateMemoryUnique(allocateInfo);
    device->getDevice()->bindBufferMemory(buffer.get(), memory.get(), 0);
}
void VulkanCore::copyBuffer(const vk::UniqueBuffer &srcBuffer, const vk::UniqueBuffer &dstBuffer, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo allocateInfo{.commandPool = commandPool.get(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
    auto commandBuffer = device->getDevice()->allocateCommandBuffersUnique(allocateInfo);

    vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
    commandBuffer[0]->begin(beginInfo);

    vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};

    commandBuffer[0]->copyBuffer(srcBuffer.get(), dstBuffer.get(), 1, &copyRegion);
    commandBuffer[0]->end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &commandBuffer[0].get()};

    graphicsQueue.submit(1, &submitInfo, nullptr);
    graphicsQueue.waitIdle();
}
void VulkanCore::createIndexBuffer() {
    auto size = sizeof(indices[0]) * indices.size();
    vk::UniqueBuffer stagingBuffer;
    vk::UniqueDeviceMemory stagingBufferMemory;
    createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible,
                 stagingBuffer, stagingBufferMemory);

    auto data = device->getDevice()->mapMemory(stagingBufferMemory.get(), 0, size);
    memcpy(data, indices.data(), size);
    device->getDevice()->unmapMemory(stagingBufferMemory.get());

    createBuffer(size, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer,
                 indexBufferMemory);
    copyBuffer(stagingBuffer, indexBuffer, size);
}
void VulkanCore::createUniformBuffers() {
    vk::DeviceSize size = sizeof(UniformBufferObject);

    for (const auto &swapImage : swapchain->getSwapChainImageViews()) {
        unifromsBuffers.emplace_back();
        uniformBufferMemories.emplace_back();
        createBuffer(size, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     unifromsBuffers.back(), uniformBufferMemories.back());
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
    auto data = device->getDevice()->mapMemory(uniformBufferMemories[currentImage].get(), 0, sizeof(ubo));
    memcpy(data, &ubo, sizeof(ubo));
    device->getDevice()->unmapMemory(uniformBufferMemories[currentImage].get());
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
        vk::DescriptorBufferInfo bufferInfo{.buffer = unifromsBuffers[i].get(), .offset = 0, .range = sizeof(UniformBufferObject)};
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
