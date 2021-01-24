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
        int height, const vk::ImageAspectFlags &aspect, vk::ImageLayout imageLayout);
  Image(vk::UniqueImage image, vk::Format imageFormat, int width, int height,
        const vk::ImageAspectFlags &aspect, vk::ImageLayout imageLayout);
  Image(const vk::Image &image, vk::Format imageFormat, int width, int height,
        const vk::ImageAspectFlags &aspect, vk::ImageLayout imageLayout);
  void createImageView(const std::shared_ptr<Device> &device,
                       const vk::ImageAspectFlags &aspectFlags);
  void createImageView(const std::shared_ptr<Device> &device,
                       const vk::ImageViewCreateInfo &createInfo);
  void transitionImageLayoutNow(const std::shared_ptr<Device> &device,
                                const vk::UniqueCommandPool &commandPool, const vk::Queue &queue,
                                vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
  void transitionImageLayout(const vk::UniqueCommandBuffer &commandBuffer,
                             vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                             const vk::AccessFlags &srcAccessFlags,
                             const vk::AccessFlags &dstAccessFlags);
  [[nodiscard]] const vk::UniqueImageView &getImageView() const;
  [[nodiscard]] const vk::UniqueHandle<vk::Image, vk::DispatchLoaderStatic> &getImage() const;
  [[nodiscard]] const vk::Image &getRawImage() const;
  [[nodiscard]] vk::Extent3D getExtent() const;
  vk::ImageLayout getLayout() const;

  [[nodiscard]] std::vector<std::byte> read(const std::shared_ptr<Device> &device) {
    if (not imageMemory.has_value()) {
      throw std::runtime_error("Reading from image without accessible memory!");
    }

    //TODO Fromat Size
    auto size = 4 * width * height;
    std::vector<std::byte> data{};
    data.resize(size);
    auto bufferData = device->getDevice()->mapMemory(imageMemory->get(), 0, VK_WHOLE_SIZE);
    memcpy(data.data(), bufferData, size);
    device->getDevice()->unmapMemory(imageMemory->get());
    return data;
  }

 private:
  vk::Image imageRaw;
  vk::UniqueImage image;
  std::optional<vk::UniqueDeviceMemory> imageMemory;
  vk::UniqueImageView imageView;

  vk::ImageAspectFlags imageAspect;
  vk::Format imageFormat;
  vk::ImageLayout layout;
  int width{};
  int height{};
  int depth = 1;
};

#endif//VULKANAPP_IMAGE_H
