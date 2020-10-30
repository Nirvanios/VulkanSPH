//
// Created by Igor Frank on 30.10.20.
//

#include "VulkanUtils.h"
#include "Buffer.h"

#include <utility>

Buffer::Buffer(BufferBuilder builder, std::shared_ptr<Device> device, const vk::UniqueCommandPool &commandPool, const vk::Queue &queue)
    : size(builder.getSize()), device(device), commandPool(commandPool), queue(queue) {
    std::tie(buffer, deviceMemory) = builder.build(std::move(device));
}

void Buffer::copyBuffer(const vk::DeviceSize &size, const vk::UniqueBuffer &srcBuffer, const vk::UniqueBuffer &dstBuffer) {
    auto commandBuffer = VulkanUtils::beginOnetimeCommand(commandPool, device);

    vk::BufferCopy copyRegion{.srcOffset = 0, .dstOffset = 0, .size = size};
    commandBuffer->copyBuffer(srcBuffer.get(), dstBuffer.get(), 1, &copyRegion);

    VulkanUtils::endOnetimeCommand(std::move(commandBuffer), queue);
}
void Buffer::copy(const vk::DeviceSize &size, const Buffer &srcBuffer) {
    copyBuffer(size, srcBuffer.getBuffer(), buffer);
}
void Buffer::copy(const vk::DeviceSize &size, const Buffer &srcBuffer, const Buffer &dstBuffer) {
    copyBuffer(size, srcBuffer.getBuffer(), dstBuffer.getBuffer());
}
const vk::UniqueBuffer &Buffer::getBuffer() const { return buffer; }
const vk::UniqueDeviceMemory &Buffer::getDeviceMemory() const { return deviceMemory; }
