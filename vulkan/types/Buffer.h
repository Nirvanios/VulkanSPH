//
// Created by Igor Frank on 30.10.20.
//

#ifndef VULKANAPP_BUFFER_H
#define VULKANAPP_BUFFER_H

#include "../builders/BufferBuilder.h"
#include "Device.h"
#include "Types.h"
#include "../../utils/Utilities.h"
#include <span>


class Buffer {
 public:
  Buffer(BufferBuilder builder, std::shared_ptr<Device> device,
         const vk::UniqueCommandPool &commandPool, const vk::Queue &queue);

  void fill(const Utilities::RawDataProvider auto &input, bool useStaging = true, int offset = 0) {
    assert(size >= input.size());
    using ValueType = Utilities::ptr_val<decltype(input.data())>;
    auto fillSize = sizeof(ValueType) * input.size();

    if (useStaging) {
      auto stagingBuffer =
          Buffer(BufferBuilder()
                     .setSize(fillSize)
                     .setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
                     .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostCoherent
                                             | vk::MemoryPropertyFlagBits::eHostVisible),
                 device, commandPool, queue);

      auto data =
          device->getDevice()->mapMemory(stagingBuffer.getDeviceMemory().get(), 0, fillSize);
      memcpy(data, input.data(), fillSize);
      device->getDevice()->unmapMemory(stagingBuffer.getDeviceMemory().get());

      copy(fillSize, stagingBuffer, offset);
    } else {
      auto data = device->getDevice()->mapMemory(deviceMemory.get(), offset, fillSize);
      memcpy(data, input.data(), fillSize);
      device->getDevice()->unmapMemory(deviceMemory.get());
    }
  };

  template<typename T>
  void fill(const T &input, bool useStaging = true, int offset = 0) {
    auto valueSize = sizeof(T);
    auto itemCount = (size / valueSize);

    if (useStaging) {
      auto stagingBuffer =
          Buffer(BufferBuilder()
                     .setSize(size)
                     .setUsageFlags(vk::BufferUsageFlagBits::eTransferSrc)
                     .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostCoherent
                                             | vk::MemoryPropertyFlagBits::eHostVisible),
                 device, commandPool, queue);

      auto data =
          reinterpret_cast<T*>(device->getDevice()->mapMemory(stagingBuffer.getDeviceMemory().get(), 0, size));
      auto dataSpan = std::span<T>{data, static_cast<size_t>(itemCount)};
      std::fill(dataSpan.begin(), dataSpan.end(), input);

      device->getDevice()->unmapMemory(stagingBuffer.getDeviceMemory().get());

      copy(size, stagingBuffer, offset);
    } else {
      auto data = reinterpret_cast<T*>(device->getDevice()->mapMemory(deviceMemory.get(), offset, size));
      auto dataSpan = std::span<T>{data,  static_cast<size_t>(itemCount)};
      std::fill(dataSpan.begin(), dataSpan.end(), input);
      device->getDevice()->unmapMemory(deviceMemory.get());
    }
  }

  void copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, int offset = 0,
            const std::vector<vk::Semaphore> &semaphores = {});
  void copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, const Buffer &dstBuffer, int offset = 0,
            const std::vector<vk::Semaphore> &semaphores = {});
  template<typename T>
  [[nodiscard]] std::vector<T> read() {
    auto stagingBuffer =
        Buffer(BufferBuilder()
                   .setSize(size)
                   .setUsageFlags(vk::BufferUsageFlagBits::eTransferDst)
                   .setMemoryPropertyFlags(vk::MemoryPropertyFlagBits::eHostCoherent
                                           | vk::MemoryPropertyFlagBits::eHostVisible),
               device, commandPool, queue);

    stagingBuffer.copy(size, *this, 0);

    std::vector<T> data{};
    data.resize(size / sizeof(T));
    auto bufferData = device->getDevice()->mapMemory(stagingBuffer.deviceMemory.get(), 0, size);
    memcpy(data.data(), bufferData, size);
    device->getDevice()->unmapMemory(stagingBuffer.deviceMemory.get());
    return data;
  }
  [[nodiscard]] const vk::UniqueBuffer &getBuffer() const;
  [[nodiscard]] const vk::UniqueDeviceMemory &getDeviceMemory() const;
  [[nodiscard]] vk::DeviceSize getSize() const;

 private:
  void copyBuffer(const vk::DeviceSize &copySize, const vk::UniqueBuffer &srcBuffer,
                  const vk::UniqueBuffer &dstBuffer, const std::vector<vk::Semaphore> &semaphores,
                  int offset);

  vk::UniqueBuffer buffer;
  vk::UniqueDeviceMemory deviceMemory;
  vk::DeviceSize size;

  std::shared_ptr<Device> device;
  const vk::UniqueCommandPool &commandPool;
  const vk::Queue &queue;
};

#endif//VULKANAPP_BUFFER_H
