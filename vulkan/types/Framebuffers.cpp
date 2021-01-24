//
// Created by Igor Frank on 19.10.20.
//

#include "Framebuffers.h"

Framebuffers::Framebuffers(std::shared_ptr<Device> device,
                           std::vector<std::shared_ptr<Image>> images,
                           const vk::RenderPass &renderPass,
                           const vk::UniqueImageView &depthImageView)
    : renderpass(renderPass), depthImageView(depthImageView) {
  this->images = images;
  this->device = device;
  createFramebuffers();
}

void Framebuffers::createFramebuffers() {
  swapchainFramebuffers.clear();
  for (auto &image : images) {
    std::array<vk::ImageView, 2> attachments = {image->getImageView().get(), depthImageView.get()};

    const auto extent = image->getExtent();

    vk::FramebufferCreateInfo framebufferInfo{.renderPass = renderpass,
                                              .attachmentCount = attachments.size(),
                                              .pAttachments = attachments.data(),
                                              .width = extent.width,
                                              .height = extent.height,
                                              .layers = 1};

    swapchainFramebuffers.emplace_back(
        device->getDevice()->createFramebufferUnique(framebufferInfo));
  }
}
const std::vector<vk::UniqueFramebuffer> &Framebuffers::getFramebuffers() const {
  return swapchainFramebuffers;
}

size_t Framebuffers::getFramebufferImageCount() const { return swapchainFramebuffers.size(); }
