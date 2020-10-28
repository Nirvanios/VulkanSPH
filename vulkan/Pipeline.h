//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_PIPELINE_H
#define VULKANAPP_PIPELINE_H

#include "Device.h"
#include "Swapchain.h"
#include <vulkan//vulkan.hpp>

class Pipeline {
public:
    Pipeline(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, vk::Format depthFormat);
    const vk::RenderPass &getRenderPass() const;
    const vk::UniquePipeline &getPipeline() const;
    const vk::UniqueDescriptorSetLayout &getDescriptorSetLayout() const;
    void createGraphicsPipeline();
    void createRenderPass();
    const vk::UniquePipelineLayout &getPipelineLayout() const;

private:
    vk::UniqueShaderModule createShaderModule(const std::string &code);
    void createDescriptorSetLayout();

    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSetLayout descriptorSetLayout;

    std::shared_ptr<Device> device;
    std::shared_ptr<Swapchain> swapchain;
    vk::Format depthFormat;
};


#endif//VULKANAPP_PIPELINE_H
