//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_VULKANCORE_H
#define VULKANAPP_VULKANCORE_H

#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "../window/GlfwWindow.h"
#include "Instance.h"


class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void run();
private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;

        [[nodiscard]] bool isComplete() const{
            return graphicsFamily.has_value();
        }
    };

    std::shared_ptr<Instance> instance;

    vk::PhysicalDevice physicalDevice;

    bool debug;
    GlfwWindow &window;

    void initVulkan();
    void mainLoop();
    void cleanup();

    void pickPhysicalDevice();
    static QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

};

#endif//VULKANAPP_VULKANCORE_H
