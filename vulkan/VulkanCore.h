//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_VULKANCORE_H
#define VULKANAPP_VULKANCORE_H

#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "../window/GlfwWindow.h"
#include "Device.h"
#include "Instance.h"
#include "Swapchain.h"


class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void run();

private:
    std::shared_ptr<Instance> instance;
    std::shared_ptr<Device> device;
    vk::UniqueSurfaceKHR surface;
    std::shared_ptr<Swapchain> swapchain;

    vk::Queue graphicsQueue;
    vk::Queue presentQueue;

    bool debug;
    GlfwWindow &window;

    void initVulkan();
    void mainLoop();
    void cleanup();

    void createSurface();
};

#endif//VULKANAPP_VULKANCORE_H
