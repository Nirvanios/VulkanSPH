//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"


void VulkanCore::initVulkan() {
    instance = std::make_shared<Instance>(window.getWindowName(), debug);
    createSurface();
    spdlog::debug("Created surface.");
    device = std::make_shared<Device>(instance, surface, debug);
    graphicsQueue = device->getGraphicsQueue();
    presentQueue = device->getPresentQueue();
    swapchain = std::make_shared<Swapchain>(device, surface, window.getWidth(), window.getHeight());
    pipeline = std::make_shared<Pipeline>(device, swapchain);
    framebuffers = std::make_shared<Framebuffers>(device, swapchain, pipeline->getRenderPass());
    createCommandPool();
    spdlog::debug("Created command pool");
    createCommandBuffers();
    spdlog::debug("Created command buffers");

}

void VulkanCore::run() {
    mainLoop();
    cleanup();
}

void VulkanCore::mainLoop() {
    while (!glfwWindowShouldClose(window.getWindow().get())) {
        glfwPollEvents();
    }
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
    commandBuffers.resize(framebuffers->getSwapchainFramebuffers().size());
    vk::CommandBufferAllocateInfo bufferAllocateInfo{.commandPool = commandPool.get(),
                                                     .level = vk::CommandBufferLevel::ePrimary,
                                                     .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};

    commandBuffers = device->getDevice()->allocateCommandBuffersUnique(bufferAllocateInfo);

    int i = 0;
    auto &swapchainFramebuffers = framebuffers->getSwapchainFramebuffers();
    for (auto &commandBuffer : commandBuffers) {
        vk::CommandBufferBeginInfo beginInfo{.pInheritanceInfo = nullptr};

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
        commandBuffer->draw(3,1,0,0);
        commandBuffer->endRenderPass();

        commandBuffer->end();

    }
}
