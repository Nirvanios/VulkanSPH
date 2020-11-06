//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_PIPELINEBUILDER_H
#define VULKANAPP_PIPELINEBUILDER_H

#include "../Pipeline.h"
#include <span>
#include <vulkan/vulkan.hpp>
struct PipelineLayoutBindingInfo {
    int binding;
    vk::DescriptorType descriptorType;
    int descriptorCount;
    vk::ShaderStageFlagBits stageFlags;
};

class PipelineBuilder {
public:
    PipelineBuilder(Config config, std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain);
    std::shared_ptr<Pipeline> build();
    PipelineBuilder &setLayoutBindingInfo(const std::span<PipelineLayoutBindingInfo> &info);
    PipelineBuilder &setDepthFormat(vk::Format format);

private:
    Config config;

    std::shared_ptr<Device> device;
    std::shared_ptr<Swapchain> swapchain;

    vk::UniqueRenderPass createRenderPass();
    vk::UniqueDescriptorSetLayout createDescriptorSetLayout();
    std::pair<vk::UniquePipelineLayout, vk::UniquePipeline> createGraphicsPipeline(const vk::UniqueDescriptorSetLayout &descriptorSetLayout,
                                                                                   const vk::UniqueRenderPass &renderPass);
    vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t> &code);


    vk::Format depthFormat;
    std::span<PipelineLayoutBindingInfo> layoutBindingInfos;
};


#endif//VULKANAPP_PIPELINEBUILDER_H
