//
// Created by Igor Frank on 18.10.20.
//

#include <algorithm>
#include <utility>

#include "Device.h"
#include <spdlog/spdlog.h>


Device::Device(std::shared_ptr<Instance> instance, bool debug) : debug(debug) {
    this->instance = instance;
    pickPhysicalDevice();
    createLogicalDevice();
    spdlog::debug("Created logical device.");
}


void Device::pickPhysicalDevice() {
    auto devices = instance->getInstance().enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    auto it = std::find_if(devices.begin(), devices.end(),
                           [](const vk::PhysicalDevice &phyDevice) {
                             auto properties = phyDevice.getProperties();
                             auto features = phyDevice.getFeatures();
                             return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
                                    features.geometryShader &&
                                    Device::findQueueFamilies(phyDevice).isComplete();
                           });

    if (it == devices.end()) {
        throw std::runtime_error("Failed to find suitable GPU!");
    }
    physicalDevice = *it;
    for (const auto &availableDevice : devices) {
        if (availableDevice == *it)
            spdlog::info(fmt::format("Available GPU: {}.<-- SELECTED", availableDevice.getProperties().deviceName));
        else
            spdlog::info(fmt::format("Available GPU: {}.", availableDevice.getProperties().deviceName));
    }
}

Device::QueueFamilyIndices Device::findQueueFamilies(const vk::PhysicalDevice &device) {
    QueueFamilyIndices indices;

    uint32_t i = 0;
    auto queueFamilies = device.getQueueFamilyProperties();
    std::for_each(queueFamilies.begin(), queueFamilies.end(),
                  [&i, &indices](const vk::QueueFamilyProperties &property) {
                    if (property.queueFlags & vk::QueueFlagBits::eGraphics)
                        indices.graphicsFamily = i;
                    ++i;
                  });
    return indices;
}

void Device::createLogicalDevice() {
    auto indices = findQueueFamilies(physicalDevice);
    auto priority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo{.queueFamilyIndex = indices.graphicsFamily.value(),
            .queueCount = 1,
            .pQueuePriorities = &priority};
    vk::PhysicalDeviceFeatures deviceFeatures{};
    vk::DeviceCreateInfo createInfo{.queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = 0,
            .pEnabledFeatures = &deviceFeatures};

    if (debug) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(instance->getValidationLayers().size());
        createInfo.ppEnabledLayerNames = instance->getValidationLayers().data();
    } else {
        createInfo.enabledLayerCount = 0;
    }
    device = physicalDevice.createDeviceUnique(createInfo);
}
vk::Queue Device::getGraphicsQueue() {
    auto indices = findQueueFamilies(physicalDevice);
    return device.get().getQueue(indices.graphicsFamily.value(), 0);
}
const vk::PhysicalDevice &Device::getPhysicalDevice() const {
    return physicalDevice;
}
const vk::UniqueDevice &Device::getDevice() const {
    return device;
}
