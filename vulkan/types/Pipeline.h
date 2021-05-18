//
// Created by Igor Frank on 19.10.20.
//

#ifndef VULKANAPP_PIPELINE_H
#define VULKANAPP_PIPELINE_H

#include "../../utils/Config.h"
#include "Device.h"
#include "RenderPass.h"
#include "Swapchain.h"
#include <vulkan/vulkan.hpp>

enum class PipelineType { Graphics, Compute };

class Pipeline {
 public:
  Pipeline(std::map<std::string, std::shared_ptr<RenderPass>>  renderPasses, vk::UniquePipelineLayout pipelineLayout,
           vk::UniquePipeline pipeline, vk::UniqueDescriptorSetLayout descriptorSetLayout);
  Pipeline(vk::UniquePipelineLayout pipelineLayout, vk::UniquePipeline pipeline,
           vk::UniqueDescriptorSetLayout descriptorSetLayout);
  [[nodiscard]] const vk::RenderPass &getRenderPass(const std::string& name = "");
  [[nodiscard]] const vk::UniquePipeline &getPipeline() const;
  [[nodiscard]] const vk::UniqueDescriptorSetLayout &getDescriptorSetLayout() const;
  [[nodiscard]] const vk::UniquePipelineLayout &getPipelineLayout() const;

 private:
  PipelineType type;
  std::map<std::string, std::shared_ptr<RenderPass>> renderPasses;
  vk::UniquePipelineLayout pipelineLayout;
  vk::UniquePipeline pipeline;
  vk::UniqueDescriptorSetLayout descriptorSetLayout;
};

#endif//VULKANAPP_PIPELINE_H
