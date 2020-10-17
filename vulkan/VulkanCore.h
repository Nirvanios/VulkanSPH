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
    void run();
private:
    GlfwWindow window = GlfwWindow{};
    void initVulkan();
    void mainLoop();
    void cleanup();
};

#endif//VULKANAPP_VULKANCORE_H
