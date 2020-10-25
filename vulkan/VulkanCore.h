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
#include "Framebuffers.h"
#include "Instance.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Types.h"


class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void run();

private:
    const std::vector<Vertex> vertices = {
            {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    const int MAX_FRAMES_IN_FLIGHT = 2;
    int currentFrame = 0;
    bool framebufferResized = false;

    std::shared_ptr<Instance> instance;
    std::shared_ptr<Device> device;
    vk::UniqueSurfaceKHR surface;
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<Pipeline> pipeline;
    std::shared_ptr<Framebuffers> framebuffers;
    vk::UniqueCommandPool commandPool;
    std::vector<vk::UniqueCommandBuffer> commandBuffers;

    std::vector<vk::UniqueSemaphore> imageAvailableSemaphore, renderFinishedSemaphore;
    std::vector<vk::UniqueFence> inFlightFences;
    std::vector<vk::Fence> imagesInFlight; //TODO optional

    vk::Queue graphicsQueue;
    vk::Queue presentQueue;

    bool debug;
    GlfwWindow &window;

    void initVulkan();
    void mainLoop();
    void cleanup();

    void createSurface();

    void createCommandPool();
    void createCommandBuffers();

    void drawFrame();
    void createSyncObjects();

    void recreateSwapchain();

public:
    bool isFramebufferResized() const;
    void setFramebufferResized(bool framebufferResized);
};

#endif//VULKANAPP_VULKANCORE_H
