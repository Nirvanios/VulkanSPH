//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_DEVICE_H
#define VULKANAPP_DEVICE_H


#include "Instance.h"
#include <vulkan/vulkan.hpp>


class Device {
private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    QueueFamilyIndices indices;

    bool debug;

    vk::PhysicalDevice physicalDevice;
    vk::UniqueDevice device;
    const vk::UniqueSurfaceKHR &surface;

    std::shared_ptr<Instance> instance;

    void pickPhysicalDevice();

    void createLogicalDevice();
    bool checkDeviceExtensionSupport(const vk::PhysicalDevice &device);

public:
    explicit Device(std::shared_ptr<Instance> instance, const vk::UniqueSurfaceKHR &surface, bool debug = false);

    [[nodiscard]] vk::Queue getGraphicsQueue() const;
    [[nodiscard]] vk::Queue getPresentQueue() const;
    [[nodiscard]] const vk::PhysicalDevice &getPhysicalDevice() const;
    [[nodiscard]] const vk::UniqueDevice &getDevice() const;

    [[nodiscard]] static QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device, const vk::UniqueSurfaceKHR &surface);
};


#endif//VULKANAPP_DEVICE_H
