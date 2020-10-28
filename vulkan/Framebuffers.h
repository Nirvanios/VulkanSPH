//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_FRAMEBUFFERS_H
#define VULKANAPP_FRAMEBUFFERS_H

#include "Swapchain.h"
#include "Device.h"

class Framebuffers {
public:
    Framebuffers(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, const vk::RenderPass &renderPass, const vk::UniqueImageView &depthImageView);

    void createFramebuffers();

    const std::vector<vk::UniqueFramebuffer> &getSwapchainFramebuffers() const;
private:
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<Device> device;
    const vk::RenderPass &renderpass;
    const vk::UniqueImageView &depthImageView;

    std::vector<vk::UniqueFramebuffer> swapchainFramebuffers;


};


#endif//VULKANAPP_FRAMEBUFFERS_H
