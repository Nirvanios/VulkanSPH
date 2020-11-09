//
// Created by Igor Frank on 09.11.20.
//

#ifndef VULKANAPP_IMAGEBUILDER_H
#define VULKANAPP_IMAGEBUILDER_H

#include "../Device.h"
#include "../Image.h"
#include "vulkan/vulkan.hpp"

class ImageBuilder {
public:
    std::shared_ptr<Image> build(const std::shared_ptr<Device> &device);
    ImageBuilder &setWidth(uint32_t width);
    ImageBuilder &setHeight(uint32_t height);
    ImageBuilder &setFormat(vk::Format format);
    ImageBuilder &setTiling(vk::ImageTiling tiling);
    ImageBuilder &setUsage(const vk::ImageUsageFlags &usage);
    ImageBuilder &setProperties(const vk::MemoryPropertyFlags &properties);
    ImageBuilder &createView(bool createView);
    ImageBuilder &setImageViewAspect(const vk::ImageAspectFlags &aspect);

private:
    bool createImageView = false;
    uint32_t imageWidth;
    uint32_t imageHeight;
    vk::Format imageFormat;
    vk::ImageTiling imageTiling;
    vk::ImageUsageFlags imageUsage;
    vk::MemoryPropertyFlags memoryProperties;
    vk::ImageAspectFlags imageAspect;
};


#endif//VULKANAPP_IMAGEBUILDER_H
