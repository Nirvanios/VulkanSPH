//
// Created by Igor Frank on 19.10.20.
//

#include "Pipeline.h"
#include "../../utils/Utilities.h"
#include "../Utils/VulkanUtils.h"
#include "Types.h"
#include <spdlog/spdlog.h>

const vk::RenderPass &Pipeline::getRenderPass() const { return renderPass->getRenderPass().get(); }

const vk::UniquePipeline &Pipeline::getPipeline() const { return pipeline; }

const vk::UniqueDescriptorSetLayout &Pipeline::getDescriptorSetLayout() const {
  return descriptorSetLayout;
}

const vk::UniquePipelineLayout &Pipeline::getPipelineLayout() const { return pipelineLayout; }

Pipeline::Pipeline(std::shared_ptr<RenderPass> renderPass, vk::UniquePipelineLayout pipelineLayout,
                   vk::UniquePipeline pipeline, vk::UniqueDescriptorSetLayout descriptorSetLayout)
    : renderPass(std::move(renderPass)), pipelineLayout(std::move(pipelineLayout)),
      pipeline(std::move(pipeline)), descriptorSetLayout(std::move(descriptorSetLayout)) {
  type = PipelineType::Graphics;
}

Pipeline::Pipeline(vk::UniquePipelineLayout pipelineLayout, vk::UniquePipeline pipeline,
                   vk::UniqueDescriptorSetLayout descriptorSetLayout)
    : pipelineLayout(std::move(pipelineLayout)), pipeline(std::move(pipeline)),
      descriptorSetLayout(std::move(descriptorSetLayout)) {
  type = PipelineType::Compute;
}
