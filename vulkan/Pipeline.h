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

private:
    void createGraphicsPipeline();
    void createRenderPass();
    vk::UniqueShaderModule createShaderModule(const std::string &code);

    vk::UniqueRenderPass renderPass;
    vk::UniquePipelineLayout pipelineLayout;
    vk::UniquePipeline pipeline;

    std::shared_ptr<Device> device;
    std::shared_ptr<Swapchain> swapchain;
};


#endif//VULKANAPP_PIPELINE_H
