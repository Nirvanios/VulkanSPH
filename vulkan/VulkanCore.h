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
            {.pos = {-0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},  {.pos = {0.5f, -0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},
            {.pos = {0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},    {.pos = {-0.5f, 0.5f, 0.0f}, .color = {1.0f, 0.0f, 0.0f}},

            {.pos = {-0.5f, -0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}}, {.pos = {0.5f, -0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},
            {.pos = {0.5f, 0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},   {.pos = {-0.5f, 0.5f, -0.5f}, .color = {0.0f, 0.0f, 1.0f}},

    };
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
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
    std::vector<vk::Fence> imagesInFlight;//TODO optional

    vk::Queue graphicsQueue;
    vk::Queue presentQueue;

    vk::UniqueBuffer vertexBuffer;
    vk::UniqueBuffer indexBuffer;
    std::vector<vk::UniqueBuffer> unifromsBuffers;
    vk::UniqueDeviceMemory vertexBufferMemory;
    vk::UniqueDeviceMemory indexBufferMemory;
    std::vector<vk::UniqueDeviceMemory> uniformBufferMemories;

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

    void createBuffer(vk::DeviceSize size, const vk::BufferUsageFlags &usage, const vk::MemoryPropertyFlags &properties, vk::UniqueBuffer &buffer,
                      vk::UniqueDeviceMemory &memory);
    void copyBuffer(const vk::UniqueBuffer &srcBuffer, const vk::UniqueBuffer &dstBuffer, vk::DeviceSize size);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    uint32_t findMemoryType(uint32_t typeFilter, const vk::MemoryPropertyFlags &properties);

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
