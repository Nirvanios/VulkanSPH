//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"


void VulkanCore::initVulkan() {
    instance = std::make_shared<Instance>(window.getWindowName(), debug);
    device = std::make_shared<Device>(instance, debug);
    graphicsQueue = device->getGraphicsQueue();
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
