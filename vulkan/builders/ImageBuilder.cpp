//
// Created by Igor Frank on 09.11.20.
//

#include "ImageBuilder.h"
#include "../Utils/VulkanUtils.h"
std::shared_ptr<Image> ImageBuilder::build(const std::shared_ptr<Device> &device) {
    vk::ImageCreateInfo imageCreateInfo{.imageType = vk::ImageType::e2D,
                                        .format = imageFormat,
                                        .extent = {.width = imageWidth, .height = imageHeight, .depth = 1},
                                        .mipLevels = 1,
                                        .arrayLayers = 1,
                                        .samples = vk::SampleCountFlagBits::e1,
                                        .tiling = imageTiling,
                                        .usage = imageUsage,
                                        .sharingMode = vk::SharingMode::eExclusive,
                                        .initialLayout = vk::ImageLayout::eUndefined};
    auto image = device->getDevice()->createImageUnique(imageCreateInfo);

    auto memRequirements = device->getDevice()->getImageMemoryRequirements(image.get());
    vk::MemoryAllocateInfo allocateInfo{.allocationSize = memRequirements.size,
                                        .memoryTypeIndex = VulkanUtils::findMemoryType(device, memRequirements.memoryTypeBits, memoryProperties)};
    auto imageMemory = device->getDevice()->allocateMemoryUnique(allocateInfo);
    device->getDevice()->bindImageMemory(image.get(), imageMemory.get(), 0);
    auto builtImage = std::make_shared<Image>(std::move(image), std::move(imageMemory), imageFormat, imageWidth, imageHeight, imageAspect);

    if (createImageView) builtImage->createImageView(device, imageAspect);

    return builtImage;
}
ImageBuilder &ImageBuilder::setWidth(uint32_t width) {
    imageWidth = width;
    return *this;
}
ImageBuilder &ImageBuilder::setHeight(uint32_t height) {
    imageHeight = height;
    return *this;
}
ImageBuilder &ImageBuilder::setFormat(vk::Format format) {
    imageFormat = format;
    return *this;
}
ImageBuilder &ImageBuilder::setTiling(vk::ImageTiling tiling) {
    imageTiling = tiling;
    return *this;
}
ImageBuilder &ImageBuilder::setUsage(const vk::ImageUsageFlags &usage) {
    imageUsage = usage;
    return *this;
}
ImageBuilder &ImageBuilder::setProperties(const vk::MemoryPropertyFlags &properties) {
    memoryProperties = properties;
    return *this;
}
ImageBuilder &ImageBuilder::createView(bool createView) {
    createImageView = createView;
    return *this;
}
ImageBuilder &ImageBuilder::setImageViewAspect(const vk::ImageAspectFlags &aspect) {
    imageAspect = aspect;
    return *this;
}
ImageBuilder &ImageBuilder::setInitialLayout(vk::ImageLayout layout) {
    initialLayout = layout;
    return *this;
}
