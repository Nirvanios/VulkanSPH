//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_FRAMEBUFFERS_H
#define VULKANAPP_FRAMEBUFFERS_H

#include "Swapchain.h"
#include "Device.h"

class Framebuffers {
public:
    Framebuffers(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, const vk::RenderPass &renderPass);

private:
    std::vector<vk::UniqueFramebuffer> swapchainFramebuffers;

public:
    const std::vector<vk::UniqueFramebuffer> &getSwapchainFramebuffers() const;
private:
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<Device> device;
    const vk::RenderPass &renderpass;

    void createFramebuffers();

};


#endif//VULKANAPP_FRAMEBUFFERS_H
