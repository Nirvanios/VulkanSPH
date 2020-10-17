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

class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void run();
private:
    bool debug;
    GlfwWindow &window;
    vk::UniqueInstance instance;
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createInstance();
};

#endif//VULKANAPP_VULKANCORE_H
