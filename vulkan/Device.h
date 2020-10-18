//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_DEVICE_H
#define VULKANAPP_DEVICE_H


#include "Instance.h"
#include <vulkan/vulkan.hpp>


class Device {
public:
    explicit Device(std::shared_ptr<Instance> instance, const vk::UniqueSurfaceKHR &surface, bool debug = false);

    [[nodiscard]] vk::Queue getGraphicsQueue() const;
    [[nodiscard]] vk::Queue getPresentQueue() const;
    [[nodiscard]] const vk::PhysicalDevice &getPhysicalDevice() const;
    [[nodiscard]] const vk::UniqueDevice &getDevice() const;

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]] bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices indices;

    bool debug;

    vk::PhysicalDevice physicalDevice;
    vk::UniqueDevice device;
    const vk::UniqueSurfaceKHR &surface;

    std::shared_ptr<Instance> instance;

    void pickPhysicalDevice();
    QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device) const ;

    void createLogicalDevice();
};


#endif//VULKANAPP_DEVICE_H
