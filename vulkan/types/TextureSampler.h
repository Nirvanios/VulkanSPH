//
// Created by Igor Frank on 24.01.21.
//

#ifndef VULKANAPP_TEXTURESAMPLER_H
#define VULKANAPP_TEXTURESAMPLER_H

#include "Device.h"
class TextureSampler {
 public:
  TextureSampler(const std::shared_ptr<Device>& device);

  const vk::UniqueSampler &getSampler() const;

 private:
  vk::UniqueSampler sampler;
};

#endif//VULKANAPP_TEXTURESAMPLER_H
