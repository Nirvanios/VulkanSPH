//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_PIPELINE_H
#define VULKANAPP_PIPELINE_H

#include "../utils/Config.h"
#include "Device.h"
#include "Swapchain.h"
#include <vulkan//vulkan.hpp>

class Pipeline {
public:
    Pipeline(Config config, std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, vk::Format depthFormat);
    [[nodiscard]] const vk::RenderPass &getRenderPass() const;
    [[nodiscard]] const vk::UniquePipeline &getPipeline() const;
    [[nodiscard]] const vk::UniqueDescriptorSetLayout &getDescriptorSetLayout() const;
    void createGraphicsPipeline();
    void createRenderPass();
    [[nodiscard]] const vk::UniquePipelineLayout &getPipelineLayout() const;

private:
    vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t> &code);
    void createDescriptorSetLayout();

    const Config config;

    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSetLayout descriptorSetLayout;

    std::shared_ptr<Device> device;
    std::shared_ptr<Swapchain> swapchain;
    vk::Format depthFormat;
};


#endif//VULKANAPP_PIPELINE_H
