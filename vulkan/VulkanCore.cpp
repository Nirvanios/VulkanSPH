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
