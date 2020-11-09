//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_VULKANCORE_H
#define VULKANAPP_VULKANCORE_H

#include <vulkan/vulkan.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "../utils/Config.h"
#include "../window/GlfwWindow.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "Framebuffers.h"
#include "Image.h"
#include "Instance.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Types.h"
#include "builders/PipelineBuilder.h"


class VulkanCore {

public:
    explicit VulkanCore(const Config &config, GlfwWindow &window, const glm::vec3 &cameraPos);
    void setViewMatrixGetter(std::function<glm::mat4()> getter);
    void run();

private:

    std::array<PipelineLayoutBindingInfo, 3> bindingInfosRender{PipelineLayoutBindingInfo{
                                                                        .binding = 0,
                                                                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                                        .descriptorCount = 1,
                                                                        .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                                                },
                                                                PipelineLayoutBindingInfo{
                                                                        .binding = 1,
                                                                        .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                                        .descriptorCount = 1,
                                                                        .stageFlags = vk::ShaderStageFlagBits::eFragment,
                                                                },
                                                                PipelineLayoutBindingInfo{.binding = 2,
                                                                                          .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                                                          .descriptorCount = 1,
                                                                                          .stageFlags = vk::ShaderStageFlagBits::eVertex}};
    std::array<PipelineLayoutBindingInfo, 1> bindingInfosCompute{PipelineLayoutBindingInfo{.binding = 0,
                                                                                           .descriptorType = vk::DescriptorType::eStorageBuffer,
                                                                                           .descriptorCount = 1,
                                                                                           .stageFlags = vk::ShaderStageFlagBits::eCompute}};

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    int currentFrame = 0;
    bool framebufferResized = false;
    int indicesSize;

    std::function<glm::mat4()> viewMatrixGetter = []() { return glm::mat4(1.0f); };
    const glm::vec3 &cameraPos;

    std::shared_ptr<Instance> instance;
    std::shared_ptr<Device> device;
    vk::UniqueSurfaceKHR surface;
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<Pipeline> pipelineGraphics;
    std::shared_ptr<Pipeline> pipelineCompute;
    std::shared_ptr<Framebuffers> framebuffers;
    vk::UniqueCommandPool commandPoolGraphics;
    vk::UniqueCommandPool commandPoolCompute;
    std::vector<vk::UniqueCommandBuffer> commandBuffersGraphic;
    std::vector<vk::UniqueCommandBuffer> commandBuffersCompute;

    std::vector<vk::UniqueSemaphore> semaphoreImageAvailable, semaphoreRenderFinished, semaphoreComputeFinished;
    std::vector<vk::UniqueFence> inFlightFences;
    std::vector<std::optional<vk::Fence>> imagesInFlight;

    vk::Queue queueGraphics;
    vk::Queue queuePresent;
    vk::Queue queueCompute;

    std::shared_ptr<Buffer> bufferVertex;
    std::shared_ptr<Buffer> bufferIndex;
    std::shared_ptr<Buffer> bufferShaderStorage;
    std::vector<std::shared_ptr<Buffer>> buffersUniformMVP;
    std::vector<std::shared_ptr<Buffer>> buffersUniformCameraPos;


    vk::UniqueDescriptorPool descriptorPool;
    std::shared_ptr<DescriptorSet> descriptorSetGraphics;
    std::shared_ptr<DescriptorSet> descriptorSetCompute;

    std::shared_ptr<Image> imageDepth;
    const Config &config;
    GlfwWindow &window;

    void mainLoop();
    void cleanup();

    void createSurface();

    void createVertexBuffer(const std::vector<Vertex> &vertices);
    void createIndexBuffer(const std::vector<uint16_t> &indices);
    void createShaderStorageBuffer(const std::span<ParticleRecord> &particles);
    void createUniformBuffers();

    void createCommandPool();
    void createCommandBuffers();
    void createDescriptorPool();
    //void createDescriptorSet();

    void createDepthResources();
    vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, const vk::FormatFeatureFlags &features);
    vk::Format findDepthFormat();

    void drawFrame();
    void updateUniformBuffers(uint32_t currentImage);
    void createSyncObjects();

    void recreateSwapchain();

public:
    [[nodiscard]] bool isFramebufferResized() const;
    void setFramebufferResized(bool framebufferResized);
    void initVulkan(const Model &modelParticle, const std::span<ParticleRecord> particles);
};

#endif//VULKANAPP_VULKANCORE_H
