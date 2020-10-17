//
// Created by Igor Frank on 17.10.20.
//

#include "spdlog/spdlog.h"

#include "VulkanCore.h"


void VulkanCore::initVulkan() {
    createInstance();
    spdlog::debug("Created vulkan instance.");
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
}
void VulkanCore::createInstance() {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    const vk::ApplicationInfo appInfo{.pApplicationName = window.getWindowName().c_str(),
                                      .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
                                      .pEngineName = "No Engine",
                                      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
                                      .apiVersion = VK_API_VERSION_1_2};
    const vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
                                            .enabledExtensionCount = glfwExtensionCount,
                                            .ppEnabledExtensionNames = glfwExtensions};
    instance = vk::createInstanceUnique(createInfo);
}
