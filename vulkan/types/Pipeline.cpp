//
// Created by Igor Frank on 19.10.20.
//

#include "Pipeline.h"
#include "../../utils/Utilities.h"
#include "../Utils/VulkanUtils.h"
#include "Types.h"
#include <spdlog/spdlog.h>

#include <utility>

const vk::RenderPass &Pipeline::getRenderPass(const std::string &name) {
  if (name.empty()) return renderPasses.begin()->second->getRenderPass().get();
  return renderPasses[name]->getRenderPass().get();
}

const vk::UniquePipeline &Pipeline::getPipeline() const { return pipeline; }

const vk::UniqueDescriptorSetLayout &Pipeline::getDescriptorSetLayout() const {
  return descriptorSetLayout;
}

const vk::UniquePipelineLayout &Pipeline::getPipelineLayout() const { return pipelineLayout; }

Pipeline::Pipeline(std::map<std::string, std::shared_ptr<RenderPass>> renderPasses,
                   vk::UniquePipelineLayout pipelineLayout, vk::UniquePipeline pipeline,
                   vk::UniqueDescriptorSetLayout descriptorSetLayout)
    : renderPasses(std::move(renderPasses)), pipelineLayout(std::move(pipelineLayout)),
      pipeline(std::move(pipeline)), descriptorSetLayout(std::move(descriptorSetLayout)) {
  type = PipelineType::Graphics;
}

Pipeline::Pipeline(vk::UniquePipelineLayout pipelineLayout, vk::UniquePipeline pipeline,
                   vk::UniqueDescriptorSetLayout descriptorSetLayout)
    : pipelineLayout(std::move(pipelineLayout)), pipeline(std::move(pipeline)),
      descriptorSetLayout(std::move(descriptorSetLayout)) {
  type = PipelineType::Compute;
}
