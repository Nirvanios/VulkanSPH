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
    Pipeline(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain);
    const vk::RenderPass &getRenderPass() const;

    void createGraphicsPipeline();
    void createRenderPass();

private:
    vk::UniqueShaderModule createShaderModule(const std::string &code);

    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;

public:
    const vk::UniquePipeline &getPipeline() const;
private:
    std::shared_ptr<Device> device;
    std::shared_ptr<Swapchain> swapchain;
};


#endif//VULKANAPP_PIPELINE_H
