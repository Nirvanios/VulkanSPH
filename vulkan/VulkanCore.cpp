//
// Created by Igor Frank on 17.10.20.
//

#include <algorithm>

#include "spdlog/spdlog.h"

#include "../Utilities.h"
#include "VulkanCore.h"


void VulkanCore::initVulkan() {
    instance = std::make_shared<Instance>(window.getWindowName(), debug);
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
    auto devices = instance->getInstance().enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    auto it = std::find_if(devices.begin(), devices.end(),
                           [](const vk::PhysicalDevice &device) {
                               auto properties = device.getProperties();
                               auto features = device.getFeatures();
                               return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
                                      features.geometryShader &&
                                       VulkanCore::findQueueFamilies(device).isComplete();
                           });

    if (it == devices.end()) {
        throw std::runtime_error("Failed to find suitable GPU!");
    }
    for (const auto &device : devices) {
        if (device == *it)
            spdlog::info(fmt::format("Available GPU: {}.<-- SELECTED", device.getProperties().deviceName));
        else
            spdlog::info(fmt::format("Available GPU: {}.", device.getProperties().deviceName));
    }
}

VulkanCore::QueueFamilyIndices VulkanCore::findQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t i = 0;
    auto queueFamilies = device.getQueueFamilyProperties();
    std::for_each(queueFamilies.begin(), queueFamilies.end(),
                  [&i , &indices](const vk::QueueFamilyProperties &property){
        if (property.queueFlags & vk::QueueFlagBits::eGraphics)
            indices.graphicsFamily = i;
        ++i;
    });
    return indices;
}
