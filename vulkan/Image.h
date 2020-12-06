//
// Created by Igor Frank on 09.11.20.
//

#ifndef VULKANAPP_IMAGE_H
#define VULKANAPP_IMAGE_H

#include "Device.h"
#include "vulkan/vulkan.hpp"

class Image {

 public:
  Image(vk::UniqueImage image, vk::UniqueDeviceMemory imageMemory, vk::Format format, int width,
        int height, const vk::ImageAspectFlags &aspect);
  Image(vk::UniqueImage image, vk::Format imageFormat, int width, int height,
        const vk::ImageAspectFlags &aspect);
  void createImageView(const std::shared_ptr<Device> &device,
                       const vk::ImageAspectFlags &aspectFlags);
  void transitionImageLayoutNow(const std::shared_ptr<Device> &device,
                                const vk::UniqueCommandPool &commandPool, const vk::Queue &queue,
                                vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
  void transitionImageLayout(const vk::UniqueCommandBuffer &commandBuffer,
                             vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                             const vk::AccessFlags &srcAccessFlags,
                             const vk::AccessFlags &dstAccessFlags) const;
  [[nodiscard]] const vk::UniqueImageView &getImageView() const;
  [[nodiscard]] const vk::UniqueHandle<vk::Image, vk::DispatchLoaderStatic> &getImage() const;

  [[nodiscard]] std::vector<char> read(const std::shared_ptr<Device> &device) {
    if (not imageMemory.has_value()) {
      throw std::runtime_error("Reading from image without accessible memory!");
    }

    //TODO Fromat Size
    auto size = 4 * width * height;
    std::vector<char> data{};
    data.resize(size);
    auto bufferData = device->getDevice()->mapMemory(imageMemory->get(), 0, VK_WHOLE_SIZE);
    memcpy(data.data(), bufferData, size);
    device->getDevice()->unmapMemory(imageMemory->get());
    return data;
  }

 private:
  vk::UniqueImage image;
  std::optional<vk::UniqueDeviceMemory> imageMemory;
  vk::UniqueImageView imageView;

  vk::ImageAspectFlags imageAspect;
  vk::Format imageFormat;
  int width{};
  int height{};
};

#endif//VULKANAPP_IMAGE_H
