//
// Created by Igor Frank on 23.01.21.
//

#include "RenderPassBuilder.h"

#include <utility>
std::shared_ptr<RenderPass> RenderPassBuilder::build() {

  vk::AttachmentReference colorAttachmentReference{.attachment = 0,
                                                   .layout =
                                                       vk::ImageLayout::eColorAttachmentOptimal};
  vk::AttachmentReference depthReference{.attachment = 1,
                                         .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal};

  vk::SubpassDescription subpassDescription{.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                            .colorAttachmentCount = 1,
                                            .pColorAttachments = &colorAttachmentReference,
                                            .pDepthStencilAttachment = &depthReference};

  vk::SubpassDependency dependency{
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
      .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
      .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

  std::array<vk::AttachmentDescription, 2> attachments = {colorAttachmentDescription, depthAttachmentDescription};
  vk::RenderPassCreateInfo renderPassCreateInfo{.attachmentCount = attachments.size(),
                                                .pAttachments = attachments.data(),
                                                .subpassCount = 1,
                                                .pSubpasses = &subpassDescription,
                                                .dependencyCount = 1,
                                                .pDependencies = &dependency};

  return std::make_shared<RenderPass>(device->getDevice()->createRenderPassUnique(renderPassCreateInfo));
}

RenderPassBuilder::RenderPassBuilder(std::shared_ptr<Device> devicePtr) : device(std::move(devicePtr)) {
  colorAttachmentDescription =
      vk::AttachmentDescription{.format = vk::Format::eUndefined,
                                .samples = vk::SampleCountFlagBits::e1,
                                .loadOp = vk::AttachmentLoadOp::eClear,
                                .storeOp = vk::AttachmentStoreOp::eStore,
                                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                .initialLayout = vk::ImageLayout::eUndefined,
                                .finalLayout = vk::ImageLayout::ePresentSrcKHR};

  depthAttachmentDescription =
      vk::AttachmentDescription{.format = vk::Format::eUndefined,
                                .samples = vk::SampleCountFlagBits::e1,
                                .loadOp = vk::AttachmentLoadOp::eClear,
                                .storeOp = vk::AttachmentStoreOp::eDontCare,
                                .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                                .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                .initialLayout = vk::ImageLayout::eUndefined,
                                .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal};
}

void RenderPassBuilder::setColorAttachmentFormat(vk::Format format) {
  colorAttachmentDescription.format = format;
}

void RenderPassBuilder::setDepthAttachmentFormat(vk::Format format) {
  depthAttachmentDescription.format = format;
}
