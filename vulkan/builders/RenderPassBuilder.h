//
// Created by Igor Frank on 23.01.21.
//

#ifndef VULKANAPP_RENDERPASSBUILDER_H
#define VULKANAPP_RENDERPASSBUILDER_H

#include "../types/Device.h"
#include "../types/RenderPass.h"

class RenderPassBuilder {
 public:
  RenderPassBuilder(std::shared_ptr<Device> devicePtr);
  std::shared_ptr<RenderPass> build();

  RenderPassBuilder &setColorAttachmentFormat(vk::Format format);
  RenderPassBuilder &setColorAttachmentLoadOp(vk::AttachmentLoadOp op);
  RenderPassBuilder &setDepthAttachmentFormat(vk::Format format);
  RenderPassBuilder &setColorAttachementFinalLayout(vk::ImageLayout layout);

 private:
  std::shared_ptr<Device> device;

  bool useDepth = false;
  vk::AttachmentDescription colorAttachmentDescription;
  vk::AttachmentDescription depthAttachmentDescription;

};

#endif//VULKANAPP_RENDERPASSBUILDER_H
