//
// Created by Igor Frank on 23.01.21.
//

#ifndef VULKANAPP_RENDERPASS_H
#define VULKANAPP_RENDERPASS_H

#include <vulkan/vulkan.hpp>

class RenderPass {
 public:
  RenderPass(vk::UniqueRenderPass renderPass);

 private:
  vk::UniqueRenderPass renderPass;

};

#endif//VULKANAPP_RENDERPASS_H
