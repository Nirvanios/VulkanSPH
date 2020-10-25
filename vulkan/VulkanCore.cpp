//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

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

void VulkanCore::cleanup() {
}

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
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto &commandBuffer : commandBuffers) {
        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse,
                                             .pInheritanceInfo = nullptr};

        commandBuffer->begin(beginInfo);

        vk::ClearValue clearValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        vk::RenderPassBeginInfo renderPassBeginInfo{.renderPass = pipeline->getRenderPass(),
                                                    .framebuffer = swapchainFramebuffers[i].get(),
                                                    .renderArea = {.offset = {0, 0},
                                                                   .extent = swapchain->getSwapchainExtent()},
                                                    .clearValueCount = 1,
                                                    .pClearValues = &clearValue};

        commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline->getPipeline().get());
        commandBuffer->draw(3, 1, 0, 0);
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
    uint32_t  imageindex;

    result = device->getDevice()->waitForFences(inFlightFences[currentFrame].get(), VK_TRUE, UINT64_MAX);
    try {
        result = device->getDevice()->acquireNextImageKHR(swapchain->getSwapchain().get(), UINT64_MAX, imageAvailableSemaphore[currentFrame].get(), nullptr, &imageindex);
    }
    catch (const std::exception &e) {
        recreateSwapchain();
        return;
    }

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
    } catch (const vk::OutOfDateKHRError &e) {
        recreateSwapchain();
    }

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
    createCommandBuffers();
}

bool VulkanCore::isFramebufferResized() const {
    return framebufferResized;
}
void VulkanCore::setFramebufferResized(bool framebufferResized) {
    VulkanCore::framebufferResized = framebufferResized;
}
