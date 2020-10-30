//
// Created by Igor Frank on 30.10.20.
//

#ifndef VULKANAPP_VULKANUTILS_H
#define VULKANAPP_VULKANUTILS_H

#include <vulkan/vulkan.hpp>

#include "Device.h"


namespace VulkanUtils {

    inline uint32_t findMemoryType(const std::shared_ptr<Device>& device, uint32_t typeFilter, const vk::MemoryPropertyFlags &properties) {
        auto memProperties = device->getPhysicalDevice().getMemoryProperties();
        uint32_t i = 0;
        for (const auto &type : memProperties.memoryTypes) {
            if ((typeFilter & (1 << i)) && (type.propertyFlags & properties)) { return i; }
            ++i;
        }
        throw std::runtime_error("failed to find suitable memory type!");
    }

    inline vk::UniqueCommandBuffer beginOnetimeCommand(const vk::UniqueCommandPool &commandPool, std::shared_ptr<Device> device){
        vk::CommandBufferAllocateInfo allocateInfo{.commandPool = commandPool.get(), .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
        auto commandBuffer = device->getDevice()->allocateCommandBuffersUnique(allocateInfo);

        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        commandBuffer[0]->begin(beginInfo);
        return std::move(commandBuffer[0]);
    }
    inline void endOnetimeCommand(vk::UniqueCommandBuffer commandBuffer, const vk::Queue &queue){
        commandBuffer->end();
        vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &commandBuffer.get()};

        queue.submit(1, &submitInfo, nullptr);
        queue.waitIdle();
    }

}

#endif//VULKANAPP_VULKANUTILS_H
