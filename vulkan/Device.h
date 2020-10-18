//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_DEVICE_H
#define VULKANAPP_DEVICE_H


#include "Instance.h"
#include <vulkan/vulkan.hpp>


class Device {
public:
    explicit Device(std::shared_ptr<Instance> instance, bool debug = false);

    vk::Queue getGraphicsQueue();
    const vk::PhysicalDevice &getPhysicalDevice() const;
    const vk::UniqueDevice &getDevice() const;

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;

        [[nodiscard]] bool isComplete() const{
            return graphicsFamily.has_value();
        }
    };

    bool debug;

    vk::PhysicalDevice physicalDevice;
    vk::UniqueDevice device;

    std::shared_ptr<Instance> instance;

    void pickPhysicalDevice();
    static QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device);

    void createLogicalDevice();
};


#endif//VULKANAPP_DEVICE_H
