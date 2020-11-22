//
// Created by Igor Frank on 30.10.20.
//

#include "BufferBuilder.h"
#include "../Device.h"
#include "../Utils/VulkanUtils.h"

std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory> BufferBuilder::build(const std::shared_ptr<Device>& device) {

    vk::BufferCreateInfo bufferCreateInfo{.size = size, .usage = usageFlags, .sharingMode = vk::SharingMode::eExclusive};

    auto buffer = device->getDevice()->createBufferUnique(bufferCreateInfo);

    auto memRequirements = device->getDevice()->getBufferMemoryRequirements(buffer.get());
    vk::MemoryAllocateInfo allocateInfo{.allocationSize = memRequirements.size,
                                        .memoryTypeIndex = VulkanUtils::findMemoryType(device, memRequirements.memoryTypeBits, memoryPropertyFlags)};

    auto memory = device->getDevice()->allocateMemoryUnique(allocateInfo);
    device->getDevice()->bindBufferMemory(buffer.get(), memory.get(), 0);

    return std::make_pair(std::move(buffer), std::move(memory));
}
BufferBuilder &BufferBuilder::setSize(int size) {
    this->size = size;
    return *this;
}
BufferBuilder &BufferBuilder::setUsageFlags(vk::BufferUsageFlags usageFlags) {
    this->usageFlags = usageFlags;
    return *this;
}
BufferBuilder &BufferBuilder::setMemoryPropertyFlags(vk::MemoryPropertyFlags memoryPropertyFlags) {
    this->memoryPropertyFlags = memoryPropertyFlags;
    return *this;
}
vk::DeviceSize BufferBuilder::getSize() const { return size; }
