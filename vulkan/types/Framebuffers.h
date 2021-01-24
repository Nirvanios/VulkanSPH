//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_FRAMEBUFFERS_H
#define VULKANAPP_FRAMEBUFFERS_H

#include "Device.h"
#include "Swapchain.h"

class Framebuffers {
 public:
  Framebuffers(std::shared_ptr<Device> device, std::vector<std::shared_ptr<Image>> images,
               const vk::RenderPass &renderPass, const vk::UniqueImageView &depthImageView);
  void createFramebuffers();
  [[nodiscard]] const std::vector<vk::UniqueFramebuffer> &getFramebuffers() const;
  size_t getFramebufferImageCount() const;

 private:
  std::vector<std::shared_ptr<Image>> images;
  std::shared_ptr<Device> device;
  const vk::RenderPass &renderpass;
  const vk::UniqueImageView &depthImageView;

  std::vector<vk::UniqueFramebuffer> swapchainFramebuffers;
};

#endif//VULKANAPP_FRAMEBUFFERS_H
