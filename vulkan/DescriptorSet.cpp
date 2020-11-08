//
// Created by Igor Frank on 07.11.20.
//

#include "DescriptorSet.h"
#include <range/v3/view/enumerate.hpp>

DescriptorSet::DescriptorSet(std::shared_ptr<Device> device, size_t descriptorsCount, const vk::UniqueDescriptorSetLayout &descriptorSetLayout,
                             const vk::UniqueDescriptorPool &descriptorPool)
    : device(std::move(device)) {
    std::vector<vk::DescriptorSetLayout> layouts(descriptorsCount, descriptorSetLayout.get());
    vk::DescriptorSetAllocateInfo allocateInfo{.descriptorPool = descriptorPool.get(),
                                               .descriptorSetCount = static_cast<uint32_t>(descriptorsCount),
                                               .pSetLayouts = layouts.data()};

    descriptorSets.resize(descriptorsCount);
    descriptorSets = this->device->getDevice()->allocateDescriptorSetsUnique(allocateInfo);
}
void DescriptorSet::updateDescriptorSet(std::span<DescriptorBufferInfo> bufferAndSizes, std::span<PipelineLayoutBindingInfo> bindingInfos) {
    for (auto keyValue : descriptorSets | ranges::views::enumerate) {
        auto i = keyValue.first;
        std::vector<vk::WriteDescriptorSet> writeDescriptorSet;
        std::vector<vk::DescriptorBufferInfo> bufferInfos{};
        std::for_each(bufferAndSizes.begin(), bufferAndSizes.end(), [&bufferInfos, i](const auto &in) {
            bufferInfos.emplace_back(vk::DescriptorBufferInfo{.buffer = in.buffer[i]->getBuffer().get(), .offset = 0, .range = in.bufferSize});
        });
        for (auto [index, bindingInfo] : bindingInfos | ranges::views::enumerate) {
            writeDescriptorSet.emplace_back(vk::WriteDescriptorSet{.dstSet = descriptorSets[i].get(),
                                                                   .dstBinding = bindingInfo.binding,
                                                                   .dstArrayElement = 0,
                                                                   .descriptorCount = bindingInfo.descriptorCount,
                                                                   .descriptorType = bindingInfo.descriptorType,
                                                                   .pImageInfo = nullptr,
                                                                   .pBufferInfo = &bufferInfos[index],
                                                                   .pTexelBufferView = nullptr});
        }
        device->getDevice()->updateDescriptorSets(writeDescriptorSet.size(), writeDescriptorSet.data(), 0, nullptr);
    }
}
const std::vector<vk::UniqueDescriptorSet> &DescriptorSet::getDescriptorSets() const { return descriptorSets; }
