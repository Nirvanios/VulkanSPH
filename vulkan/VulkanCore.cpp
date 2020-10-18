//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"




void VulkanCore::initVulkan() {
    instance = Instance{window.getWindowName(), debug};
    pickPhysicalDevice();
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
void VulkanCore::pickPhysicalDevice() {

}
