//
// Created by Igor Frank on 24.01.21.
//

#include "TextureSampler.h"
#include "Buffer.h"
TextureSampler::TextureSampler(const std::shared_ptr<Device>& device) {
  auto properties = device->getPhysicalDevice().getProperties();
  vk::SamplerCreateInfo samplerCreateInfo{.magFilter = vk::Filter::eLinear,
                                          .minFilter = vk::Filter::eLinear,
                                          .mipmapMode = vk::SamplerMipmapMode::eLinear,
                                          .addressModeU = vk::SamplerAddressMode::eRepeat,
                                          .addressModeV = vk::SamplerAddressMode::eRepeat,
                                          .addressModeW = vk::SamplerAddressMode::eRepeat,
                                          .mipLodBias = 0.0f,
                                          .anisotropyEnable = VK_TRUE,
                                          .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                          .compareEnable = VK_FALSE,
                                          .compareOp = vk::CompareOp::eAlways,
                                          .minLod = 0.0f,
                                          .maxLod = 0.0f,
                                          .borderColor = vk::BorderColor::eIntOpaqueBlack,
                                          .unnormalizedCoordinates = VK_FALSE};

  sampler = device->getDevice()->createSamplerUnique(samplerCreateInfo);
}
const vk::UniqueSampler &TextureSampler::getSampler() const { return sampler; }
