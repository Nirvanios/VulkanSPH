//
// Created by Igor Frank on 07.11.20.
//

#ifndef VULKANAPP_DESCRIPTORSET_H
#define VULKANAPP_DESCRIPTORSET_H

#include "Buffer.h"
#include "Device.h"
#include "builders/PipelineBuilder.h"
#include "vulkan/vulkan.hpp"

#include <span>
struct DescriptorBufferInfo {
    std::span<std::shared_ptr<Buffer>> buffer;
    size_t bufferSize;
};

class DescriptorSet {
public:
    DescriptorSet(std::shared_ptr<Device> device, size_t descriptorsCount, const vk::UniqueDescriptorSetLayout &descriptorSetLayout,
                  const vk::UniqueDescriptorPool &descriptorPool);
    void updateDescriptorSet(std::span<DescriptorBufferInfo> bufferInfos, std::span<PipelineLayoutBindingInfo> bindingInfos);
    [[nodiscard]] const std::vector<vk::UniqueDescriptorSet> &getDescriptorSets() const;

private:
    std::shared_ptr<Device> device;
    std::vector<vk::UniqueDescriptorSet> descriptorSets;
};


#endif//VULKANAPP_DESCRIPTORSET_H
