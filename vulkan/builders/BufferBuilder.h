//
// Created by Igor Frank on 30.10.20.
//

#ifndef VULKANAPP_BUFFERBUILDER_H
#define VULKANAPP_BUFFERBUILDER_H

#include <memory>
#include <vulkan/vulkan.hpp>

#include "../Device.h"

class BufferBuilder {
public:

    [[nodiscard]] std::pair<vk::UniqueBuffer, vk::UniqueDeviceMemory> build(const std::shared_ptr<Device>& device);
    BufferBuilder &setSize(int size);
    BufferBuilder &setUsageFlags(vk::BufferUsageFlags usageFlags);
    BufferBuilder &setMemoryPropertyFlags(vk::MemoryPropertyFlags memoryPropertyFlags);
    [[nodiscard]] vk::DeviceSize getSize() const;

private:
    vk::DeviceSize size;
    vk::BufferUsageFlags usageFlags;
    vk::MemoryPropertyFlags memoryPropertyFlags;

};


#endif//VULKANAPP_BUFFERBUILDER_H
