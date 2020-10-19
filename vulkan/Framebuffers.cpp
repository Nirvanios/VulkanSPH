//
// Created by Igor Frank on 19.10.20.
//

#include "Framebuffers.h"

Framebuffers::Framebuffers(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, const vk::RenderPass &renderPass) : renderpass(renderPass) {
    this->swapchain = swapchain;
    this->device = device;
    createFramebuffers();
}

void Framebuffers::createFramebuffers() {
    for (const auto &swapchainImageView : swapchain->getSwapChainImageViews()) {
        vk::ImageView attachments[] = {
                swapchainImageView.get()};

        vk::FramebufferCreateInfo framebufferInfo{.renderPass = renderpass,
                                                  .attachmentCount = 1,
                                                  .pAttachments = attachments,
                                                  .width = swapchain->getSwapchainExtent().width,
                                                  .height = swapchain->getSwapchainExtent().height,
                                                  .layers = 1};

        swapchainFramebuffers.emplace_back(device->getDevice()->createFramebufferUnique(framebufferInfo));
    }
}
const std::vector<vk::UniqueFramebuffer> &Framebuffers::getSwapchainFramebuffers() const {
    return swapchainFramebuffers;
}
