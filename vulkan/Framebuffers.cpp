//
// Created by Igor Frank on 19.10.20.
//

#include "Framebuffers.h"

Framebuffers::Framebuffers(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain, const vk::RenderPass &renderPass, const vk::UniqueImageView &depthImageView) : renderpass(renderPass), depthImageView(depthImageView) {
    this->swapchain = swapchain;
    this->device = device;
    createFramebuffers();
}

void Framebuffers::createFramebuffers() {
    swapchainFramebuffers.clear();
    for (size_t i = 0; i < swapchain->getSwapChainImageViews().size(); i++) {
        std::array<vk::ImageView, 2> attachments = {swapchain->getSwapChainImageViews()[i].get(), depthImageView.get()};

        vk::FramebufferCreateInfo framebufferInfo{.renderPass = renderpass,
                                                  .attachmentCount = attachments.size(),
                                                  .pAttachments = attachments.data(),
                                                  .width = swapchain->getSwapchainExtent().width,
                                                  .height = swapchain->getSwapchainExtent().height,
                                                  .layers = 1};

        swapchainFramebuffers.emplace_back(device->getDevice()->createFramebufferUnique(framebufferInfo));
    }
}
const std::vector<vk::UniqueFramebuffer> &Framebuffers::getSwapchainFramebuffers() const { return swapchainFramebuffers; }
