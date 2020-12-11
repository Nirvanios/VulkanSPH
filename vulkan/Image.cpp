//
// Created by Igor Frank on 09.11.20.
//

#include "Image.h"
#include "Utils/VulkanUtils.h"

Image::Image(vk::UniqueImage image, vk::UniqueDeviceMemory imageMemory, vk::Format format,
             int width, int height, const vk::ImageAspectFlags &aspect)
    : imageRaw(image.get()), image(std::move(image)), imageMemory(std::move(imageMemory)), imageAspect(aspect),
      imageFormat(format), width(width), height(height) {
}

Image::Image(vk::UniqueImage image, vk::Format imageFormat, int width, int height,
             const vk::ImageAspectFlags &aspect)
    : imageRaw(image.get()), image(std::move(image)), imageAspect(aspect), imageFormat(imageFormat), width(width),
      height(height) {}

void Image::transitionImageLayoutNow(const std::shared_ptr<Device> &device,
                                     const vk::UniqueCommandPool &commandPool,
                                     const vk::Queue &queue, vk::ImageLayout oldLayout,
                                     vk::ImageLayout newLayout) {
  auto commandBuffer = VulkanUtils::beginOnetimeCommand(commandPool, device);

  vk::ImageMemoryBarrier barrier{
      .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = imageRaw,
      .subresourceRange = {.aspectMask = imageAspect,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
  };

  /*    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);

        if (VulkanUtils::hasStencilComponent(imageFormat)) { barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil; }
    } else {
        barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    }*/

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined
      && newLayout == vk::ImageLayout::eTransferDstOptimal) {
    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal
             && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else if (oldLayout == vk::ImageLayout::eUndefined
             && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead
        | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
  } else if (oldLayout == vk::ImageLayout::eUndefined
             && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {
    barrier.dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }

  commandBuffer->pipelineBarrier(sourceStage, destinationStage, {}, 0, nullptr, 0, nullptr, 1,
                                 &barrier);

  VulkanUtils::endOnetimeCommand(std::move(commandBuffer), queue);
}

void Image::transitionImageLayout(const vk::UniqueCommandBuffer &commandBuffer,
                                  vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                  const vk::AccessFlags &srcAccessFlags,
                                  const vk::AccessFlags &dstAccessFlags) const {
  vk::ImageMemoryBarrier imageMemoryBarrier{.srcAccessMask = srcAccessFlags,
                                            .dstAccessMask = dstAccessFlags,
                                            .oldLayout = oldLayout,
                                            .newLayout = newLayout,
                                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                            .image = imageRaw,
                                            .subresourceRange = {.aspectMask = imageAspect,
                                                                 .baseMipLevel = 0,
                                                                 .levelCount = 1,
                                                                 .baseArrayLayer = 0,
                                                                 .layerCount = 1}};

  commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                 vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, 0, nullptr,
                                 1, &imageMemoryBarrier);
}

void Image::createImageView(const std::shared_ptr<Device> &device,
                            const vk::ImageAspectFlags &aspectFlags) {
  vk::ImageViewCreateInfo viewCreateInfo{.image = imageRaw,
                                         .viewType = vk::ImageViewType::e2D,
                                         .format = imageFormat,
                                         .subresourceRange = {.aspectMask = aspectFlags,
                                                              .baseMipLevel = 0,
                                                              .levelCount = 1,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1}};
  imageView = device->getDevice()->createImageViewUnique(viewCreateInfo);
}

const vk::UniqueImageView &Image::getImageView() const { return imageView; }

const vk::UniqueHandle<vk::Image, vk::DispatchLoaderStatic> &Image::getImage() const {
  return image;
}

const vk::Image &Image::getRawImage() const {
  return imageRaw;
}

Image::Image(const vk::Image &image, vk::Format imageFormat, int width, int height,
             const vk::ImageAspectFlags &aspect)
    : imageRaw(image), imageAspect(aspect), imageFormat(imageFormat), width(width),
      height(height) {}
