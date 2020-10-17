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

using UniqueDebugUtilsMessengerEXTDynamic = vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic>;

class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void run();
private:
    const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    bool debug;
    GlfwWindow &window;
    vk::UniqueInstance instance;
    UniqueDebugUtilsMessengerEXTDynamic debugMessenger;
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createInstance();
    bool checkValidationLayerSupport();
    std::vector<const char *> getRequiredExtensions();
    void setupDebugMessenger();
    vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();
};

#endif//VULKANAPP_VULKANCORE_H
