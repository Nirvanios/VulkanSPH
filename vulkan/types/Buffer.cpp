//
// Created by Igor Frank on 30.10.20.
//

#include "Buffer.h"
#include "../Utils/VulkanUtils.h"

#include <utility>

Buffer::Buffer(BufferBuilder builder, std::shared_ptr<Device> device,
               const vk::UniqueCommandPool &commandPool, const vk::Queue &queue)
    : size(builder.getSize()), device(device), commandPool(commandPool), queue(queue) {
  std::tie(buffer, deviceMemory) = builder.build(std::move(device));
}

void Buffer::copyBuffer(const vk::DeviceSize &copySize, const vk::UniqueBuffer &srcBuffer,
                        const vk::UniqueBuffer &dstBuffer,
                        const std::vector<vk::Semaphore> &semaphores, int offset) {
  auto commandBuffer = VulkanUtils::beginOnetimeCommand(commandPool, device);

  vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = static_cast<vk::DeviceSize>(offset), .size = copySize};
  commandBuffer->copyBuffer(srcBuffer.get(), dstBuffer.get(), 1, &copyRegion);

  VulkanUtils::endOnetimeCommand(std::move(commandBuffer), queue, semaphores);
}

void Buffer::copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, int offset,
                  const std::vector<vk::Semaphore> &semaphores) {
  copyBuffer(copySize, srcBuffer.getBuffer(), buffer, semaphores, offset);
}

void Buffer::copy(const vk::DeviceSize &copySize, const Buffer &srcBuffer, const Buffer &dstBuffer, int offset,
                  const std::vector<vk::Semaphore> &semaphores) {
  copyBuffer(copySize, srcBuffer.getBuffer(), dstBuffer.getBuffer(), semaphores, offset);
}

const vk::UniqueBuffer &Buffer::getBuffer() const { return buffer; }

const vk::UniqueDeviceMemory &Buffer::getDeviceMemory() const { return deviceMemory; }

vk::DeviceSize Buffer::getSize() const { return size; }
