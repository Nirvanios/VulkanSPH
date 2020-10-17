//
// Created by Igor Frank on 17.10.20.
//

#include "spdlog/spdlog.h"

#include "VulkanCore.h"

void VulkanCore::initVulkan() {

}
void VulkanCore::run() {
        initVulkan();
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
