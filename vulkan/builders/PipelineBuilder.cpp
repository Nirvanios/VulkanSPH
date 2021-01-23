//
// Created by Igor Frank on 06.11.20.
//

#include "PipelineBuilder.h"
#include "../Utils/VulkanUtils.h"
#include "../types/Types.h"
#include "RenderPassBuilder.h"
#include <shaderc/shaderc.h>
#include <spdlog/spdlog.h>

#include <utility>
std::shared_ptr<Pipeline> PipelineBuilder::build() {
  if (pipelineType == PipelineType::Graphics) {
    //auto renderpass = createRenderPass();
    spdlog::debug("Created renderpass.");
    auto descriptorSetLayout = createDescriptorSetLayout();
    auto [pipelineLayout, pipeline] = createGraphicsPipeline(descriptorSetLayout, renderPasses.begin()->second->getRenderPass());
    spdlog::debug("Created graphics pipeline.");

    return std::make_shared<Pipeline>(renderPasses, std::move(pipelineLayout),
                                      std::move(pipeline), std::move(descriptorSetLayout));
  } else {
    auto descriptorSetLayout = createDescriptorSetLayout();
    auto [pipelineLayout, pipeline] = createComputePipeline(descriptorSetLayout);
    spdlog::debug("Created compute pipeline.");

    return std::make_shared<Pipeline>(std::move(pipelineLayout), std::move(pipeline),
                                      std::move(descriptorSetLayout));
  }
}

std::pair<vk::UniquePipelineLayout, vk::UniquePipeline>
PipelineBuilder::createGraphicsPipeline(const vk::UniqueDescriptorSetLayout &descriptorSetLayout,
                                        const vk::UniqueRenderPass &renderPass) {
  auto vertShaderModule = createShaderModule(
      VulkanUtils::compileShader(vertexFile, shaderc_shader_kind::shaderc_vertex_shader,
                                 Utilities::readFile(vertexFile), macros));
  auto fragShaderModule = createShaderModule(
      VulkanUtils::compileShader(vertexFile, shaderc_shader_kind::shaderc_fragment_shader,
                                 Utilities::readFile(fragmentFile), macros));
  auto bindingDescription = Vertex::getBindingDescription();
  auto attributeDescriptions = Vertex::getAttributeDescriptions();

  vk::PipelineShaderStageCreateInfo vertexStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex,
                                                          .module = vertShaderModule.get(),
                                                          .pName = "main"};

  vk::PipelineShaderStageCreateInfo fragmentStageCreateInfo{.stage =
                                                                vk::ShaderStageFlagBits::eFragment,
                                                            .module = fragShaderModule.get(),
                                                            .pName = "main"};

  std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{vertexStageCreateInfo,
                                                                fragmentStageCreateInfo};

  vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount = attributeDescriptions.size(),
      .pVertexAttributeDescriptions = attributeDescriptions.data()};

  vk::Viewport viewport{.x = 0.0f,
                        .y = 0.0f,
                        .width = static_cast<float>(swapchain->getSwapchainExtent().width),
                        .height = static_cast<float>(swapchain->getSwapchainExtent().height),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f};

  vk::Rect2D scissor{.offset = {.x = 0, .y = 0}, .extent = swapchain->getSwapchainExtent()};

  vk::PipelineViewportStateCreateInfo viewportStateCreateInfo{.viewportCount = 1,
                                                              .pViewports = &viewport,
                                                              .scissorCount = 1,
                                                              .pScissors = &scissor};

  vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f};

  vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE};

  vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
          | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

  vk::PipelineColorBlendStateCreateInfo blendStateCreateInfo{
      .logicOpEnable = VK_FALSE,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachmentState,
      .blendConstants = std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}};

  /*
    std::array<vk::DynamicState, 2> dynamicStates{vk::DynamicState::eViewport,
                                                  vk::DynamicState::eLineWidth};

    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo{.dynamicStateCount = dynamicStates.size(),
                                                              .pDynamicStates = dynamicStates.data()};
*/

  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout.get(),
      .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
      .pPushConstantRanges = pushConstantRanges.data()};

  auto pipelineLayout = device->getDevice()->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

  vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE};

  vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
      .stageCount = 2,
      .pStages = shaderStages.data(),
      .pVertexInputState = &vertexInputCreateInfo,
      .pInputAssemblyState = &inputAssemblyStateCreateInfo,
      .pViewportState = &viewportStateCreateInfo,
      .pRasterizationState = &rasterizationStateCreateInfo,
      .pMultisampleState = &multisampleStateCreateInfo,
      .pDepthStencilState = &depthStencilStateCreateInfo,
      .pColorBlendState = &blendStateCreateInfo,
      .pDynamicState = nullptr,
      .layout = pipelineLayout.get(),
      .renderPass = renderPass.get(),
      .subpass = 0,
      .basePipelineHandle = nullptr,
      .basePipelineIndex = -1};

  return {std::move(pipelineLayout),
          device->getDevice()->createGraphicsPipelineUnique(nullptr, pipelineCreateInfo).value};
}

vk::UniqueShaderModule PipelineBuilder::createShaderModule(const std::vector<uint32_t> &code) {
  vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(uint32_t),
                                        .pCode = code.data()};

  return device->getDevice()->createShaderModuleUnique(createInfo);
}

vk::UniqueDescriptorSetLayout PipelineBuilder::createDescriptorSetLayout() {

  std::vector<vk::DescriptorSetLayoutBinding> layoutBindings{};
  for (const auto binding : layoutBindingInfos) {
    layoutBindings.emplace_back(
        vk::DescriptorSetLayoutBinding{.binding = binding.binding,
                                       .descriptorType = binding.descriptorType,
                                       .descriptorCount = binding.descriptorCount,
                                       .stageFlags = binding.stageFlags,
                                       .pImmutableSamplers = nullptr});
  }

  vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
      .pBindings = layoutBindings.data()};

  return device->getDevice()->createDescriptorSetLayoutUnique(layoutCreateInfo);
}
PipelineBuilder::PipelineBuilder(Config config, std::shared_ptr<Device> device,
                                 std::shared_ptr<Swapchain> swapchain)
    : config(std::move(config)), device(std::move(device)), swapchain(std::move(swapchain)) {}

PipelineBuilder &
PipelineBuilder::setLayoutBindingInfo(const std::span<PipelineLayoutBindingInfo> &info) {
  layoutBindingInfos = info;
  return *this;
}
std::pair<vk::UniquePipelineLayout, vk::UniquePipeline>
PipelineBuilder::createComputePipeline(const vk::UniqueDescriptorSetLayout &descriptorSetLayout) {

  auto computeModule = createShaderModule(
      VulkanUtils::compileShader(computeFile, shaderc_shader_kind::shaderc_compute_shader,
                                 Utilities::readFile(computeFile), macros));

  vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{
      .stage = vk::ShaderStageFlagBits::eCompute,
      .module = computeModule.get(),
      .pName = "main"};

  vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
      .setLayoutCount = 1,
      .pSetLayouts = &descriptorSetLayout.get(),
      .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
      .pPushConstantRanges = pushConstantRanges.data()};

  auto pipelineLayout = device->getDevice()->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

  vk::ComputePipelineCreateInfo computePipelineCreateInfo{.stage = pipelineShaderStageCreateInfo,
                                                          .layout = pipelineLayout.get()};

  return {std::move(pipelineLayout),
          device->getDevice()->createComputePipelineUnique(nullptr, computePipelineCreateInfo)};
}
PipelineBuilder &PipelineBuilder::setPipelineType(PipelineType type) {
  pipelineType = type;
  return *this;
}
PipelineBuilder &PipelineBuilder::setVertexShaderPath(const std::string &path) {
  vertexFile = path;
  return *this;
}
PipelineBuilder &PipelineBuilder::setFragmentShaderPath(const std::string &path) {
  fragmentFile = path;
  return *this;
}
PipelineBuilder &PipelineBuilder::setComputeShaderPath(const std::string &path) {
  computeFile = path;
  return *this;
}
PipelineBuilder &PipelineBuilder::addPushConstant(vk::ShaderStageFlagBits stage,
                                                  size_t pushConstantSize) {
  pushConstantRanges.emplace_back(
      vk::PushConstantRange{.stageFlags = stage,
                            .offset = 0,
                            .size = static_cast<uint32_t>(pushConstantSize)});
  return *this;
}
PipelineBuilder &PipelineBuilder::addShaderMacro(const std::string &name, const std::string &code) {
  macros.emplace_back(name, code);
  return *this;
}
PipelineBuilder &PipelineBuilder::setAssemblyInfo(vk::PrimitiveTopology topology,
                                                  bool usePrimitiveRestartIndex) {
  inputAssemblyStateCreateInfo.topology = topology;
  inputAssemblyStateCreateInfo.primitiveRestartEnable = usePrimitiveRestartIndex;
  return *this;
}

PipelineBuilder &PipelineBuilder::addRenderPass(const std::string& name, std::shared_ptr<RenderPass> renderPass) {
  renderPasses[name] = std::move(renderPass);
  return *this;
}
