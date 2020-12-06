//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_PIPELINEBUILDER_H
#define VULKANAPP_PIPELINEBUILDER_H

#include "../Pipeline.h"
#include <span>
#include <vulkan/vulkan.hpp>

struct PipelineLayoutBindingInfo {//TODO Separate
  uint32_t binding;
  vk::DescriptorType descriptorType;
  uint32_t descriptorCount;
  vk::ShaderStageFlagBits stageFlags;
};

class PipelineBuilder {
 public:
  PipelineBuilder(Config config, std::shared_ptr<Device> device,
                  std::shared_ptr<Swapchain> swapchain);
  std::shared_ptr<Pipeline> build();
  PipelineBuilder &setLayoutBindingInfo(const std::span<PipelineLayoutBindingInfo> &info);
  PipelineBuilder &setDepthFormat(vk::Format format);
  PipelineBuilder &setPipelineType(PipelineType type);
  PipelineBuilder &setVertexShaderPath(const std::string &path);
  PipelineBuilder &setFragmentShaderPath(const std::string &path);
  PipelineBuilder &setComputeShaderPath(const std::string &path);
  PipelineBuilder &addPushConstant(vk::ShaderStageFlagBits stage, size_t pushConstantSize);

 private:
  Config config;

  std::shared_ptr<Device> device;
  std::shared_ptr<Swapchain> swapchain;

  vk::UniqueRenderPass createRenderPass();
  vk::UniqueDescriptorSetLayout createDescriptorSetLayout();
  std::pair<vk::UniquePipelineLayout, vk::UniquePipeline>
  createGraphicsPipeline(const vk::UniqueDescriptorSetLayout &descriptorSetLayout,
                         const vk::UniqueRenderPass &renderPass);
  std::pair<vk::UniquePipelineLayout, vk::UniquePipeline>
  createComputePipeline(const vk::UniqueDescriptorSetLayout &descriptorSetLayout);
  vk::UniqueShaderModule createShaderModule(const std::vector<uint32_t> &code);

  vk::Format depthFormat;
  std::span<PipelineLayoutBindingInfo> layoutBindingInfos;
  std::vector<vk::PushConstantRange> pushConstantRanges;
  PipelineType pipelineType;
  std::string vertexFile;
  std::string fragmentFile;
  std::string computeFile;
};

#endif//VULKANAPP_PIPELINEBUILDER_H
