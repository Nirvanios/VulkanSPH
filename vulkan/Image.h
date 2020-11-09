//
// Created by Igor Frank on 09.11.20.
//

#ifndef VULKANAPP_IMAGE_H
#define VULKANAPP_IMAGE_H

#include "Device.h"
#include "vulkan/vulkan.hpp"

class Image {

public:
    Image(vk::UniqueImage image, vk::UniqueDeviceMemory imageMemory, vk::Format format);
    void createImageView(const std::shared_ptr<Device> &device, const vk::ImageAspectFlags &aspectFlags);
    void transitionImageLayout(const std::shared_ptr<Device> &device, const vk::UniqueCommandPool &commandPool, const vk::Queue &queue,
                               vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    const vk::UniqueImageView &getImageView() const;

private:
    vk::UniqueImage image;
    vk::UniqueDeviceMemory imageMemory;
    vk::UniqueImageView imageView;

    vk::Format imageFormat;
};


#endif//VULKANAPP_IMAGE_H
