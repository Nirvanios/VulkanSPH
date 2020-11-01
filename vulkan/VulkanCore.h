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
#include "Buffer.h"
#include "Device.h"
#include "Framebuffers.h"
#include "Instance.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Types.h"


class VulkanCore {

public:
    explicit VulkanCore(GlfwWindow &window, bool debug = false);
    void setViewMatrixGetter(std::function<glm::mat4()> getter);
    void run();

private:
    const std::vector<Vertex> vertices = {
            {.pos = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},  {.pos = {0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},
            {.pos = {0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},    {.pos = {-0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},

            {.pos = {-0.5f, -0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}}, {.pos = {0.5f, -0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},
            {.pos = {0.5f, 0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},   {.pos = {-0.5f, 0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},

    };
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

    const int MAX_FRAMES_IN_FLIGHT = 2;
    int currentFrame = 0;
    bool framebufferResized = false;

    std::function<glm::mat4()> viewMatrixGetter = [](){return glm::mat4(1.0f);};

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
    std::vector<std::optional<vk::Fence>> imagesInFlight;

    vk::Queue graphicsQueue;
    vk::Queue presentQueue;

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> indexBuffer;
    std::vector<std::shared_ptr<Buffer>> unifromsBuffers;


    vk::UniqueDescriptorPool descriptorPool;
    std::vector<vk::UniqueDescriptorSet> descriptorSets;

    vk::UniqueImage depthImage;
    vk::UniqueDeviceMemory depthImageMemory;
    vk::UniqueImageView depthImageView;

    bool debug;
    GlfwWindow &window;

    void initVulkan();
    void mainLoop();
    void cleanup();

    void createSurface();

    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();

    void createCommandPool();
    void createCommandBuffers();
    void createDescriptorPool();
    void createDescriptorSet();

    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, const vk::ImageUsageFlags &usage,
                     const vk::MemoryPropertyFlags &properties, vk::UniqueImage &image, vk::UniqueDeviceMemory &imageMemory);
    vk::UniqueImageView createImageView(const vk::UniqueImage &image, vk::Format format, const vk::ImageAspectFlags &aspectFlags);
    void transitionImageLayout(const vk::UniqueImage &image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

    void createDepthResources();
    vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, const vk::FormatFeatureFlags &features);
    vk::Format findDepthFormat();
    bool hasStencilComponent(vk::Format format);

    void drawFrame();
    void updateUniformBuffers(uint32_t currentImage);
    void createSyncObjects();

    void recreateSwapchain();

public:
    bool isFramebufferResized() const;
    void setFramebufferResized(bool framebufferResized);
};

#endif//VULKANAPP_VULKANCORE_H
