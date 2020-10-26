//
// Created by Igor Frank on 19.10.20.
//

#include "Pipeline.h"
#include "../Utilities.h"
#include "Types.h"
#include <spdlog/spdlog.h>


Pipeline::Pipeline(std::shared_ptr<Device> device, std::shared_ptr<Swapchain> swapchain) {
    this->device = device;
    this->swapchain = swapchain;
    createRenderPass();
    spdlog::debug("Created renderpass.");
    createDescriptorSetLayout();
    createGraphicsPipeline();
    spdlog::debug("Created pipeline.");
}
void Pipeline::createGraphicsPipeline() {
    //TODO config
    auto vertShaderCode = Utilities::readFile("/home/kuro/CLionProjects/VulkanApp/shaders/shader.vert.spv");
    auto fragShaderCode = Utilities::readFile("/home/kuro/CLionProjects/VulkanApp/shaders/shader.frag.spv");

    auto vertShaderModule = createShaderModule(vertShaderCode);
    auto fragShaderModule = createShaderModule(fragShaderCode);
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineShaderStageCreateInfo vertexStageCreateInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = vertShaderModule.get(), .pName = "main"};

    vk::PipelineShaderStageCreateInfo fragmentStageCreateInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = fragShaderModule.get(), .pName = "main"};

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages{vertexStageCreateInfo, fragmentStageCreateInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputCreateInfo{.vertexBindingDescriptionCount = 1,
                                                                 .pVertexBindingDescriptions = &bindingDescription,
                                                                 .vertexAttributeDescriptionCount = attributeDescriptions.size(),
                                                                 .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{.topology = vk::PrimitiveTopology::eTriangleList, .primitiveRestartEnable = VK_FALSE};

    vk::Viewport viewport{.x = 0.0f,
                          .y = 0.0f,
                          .width = static_cast<float>(swapchain->getSwapchainExtent().width),
                          .height = static_cast<float>(swapchain->getSwapchainExtent().height),
                          .minDepth = 0.0f,
                          .maxDepth = 1.0f};

    vk::Rect2D scissor{.offset = {.x = 0, .y = 0}, .extent = swapchain->getSwapchainExtent()};

    vk::PipelineViewportStateCreateInfo viewportStateCreateInfo{.viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{.depthClampEnable = VK_FALSE,
                                                                          .rasterizerDiscardEnable = VK_FALSE,
                                                                          .polygonMode = vk::PolygonMode::eFill,
                                                                          .cullMode = vk::CullModeFlagBits::eBack,
                                                                          .frontFace = vk::FrontFace::eCounterClockwise,
                                                                          .depthBiasEnable = VK_FALSE,
                                                                          .depthBiasConstantFactor = 0.0f,
                                                                          .depthBiasClamp = 0.0f,
                                                                          .depthBiasSlopeFactor = 0.0f,
                                                                          .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                                      .sampleShadingEnable = VK_FALSE,
                                                                      .minSampleShading = 1.0f,
                                                                      .pSampleMask = nullptr,
                                                                      .alphaToCoverageEnable = VK_FALSE,
                                                                      .alphaToOneEnable = VK_FALSE};

    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{.blendEnable = VK_FALSE,
                                                                    .srcColorBlendFactor = vk::BlendFactor::eOne,
                                                                    .dstColorBlendFactor = vk::BlendFactor::eZero,
                                                                    .colorBlendOp = vk::BlendOp::eAdd,
                                                                    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                    .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                                                                    .alphaBlendOp = vk::BlendOp::eAdd,
                                                                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                                                                      vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo blendStateCreateInfo{.logicOpEnable = VK_FALSE,
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

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{.setLayoutCount = 1,
                                                          .pSetLayouts = &descriptorSetLayout.get(),
                                                          .pushConstantRangeCount = 0,
                                                          .pPushConstantRanges = nullptr};

    pipelineLayout = device->getDevice()->createPipelineLayoutUnique(pipelineLayoutCreateInfo);

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo{.stageCount = 2,
                                                      .pStages = shaderStages.data(),
                                                      .pVertexInputState = &vertexInputCreateInfo,
                                                      .pInputAssemblyState = &inputAssemblyCreateInfo,
                                                      .pViewportState = &viewportStateCreateInfo,
                                                      .pRasterizationState = &rasterizationStateCreateInfo,
                                                      .pMultisampleState = &multisampleStateCreateInfo,
                                                      .pDepthStencilState = nullptr,
                                                      .pColorBlendState = &blendStateCreateInfo,
                                                      .pDynamicState = nullptr,
                                                      .layout = pipelineLayout.get(),
                                                      .renderPass = renderPass.get(),
                                                      .subpass = 0,
                                                      .basePipelineHandle = nullptr,
                                                      .basePipelineIndex = -1};

    pipeline = device->getDevice()->createGraphicsPipelineUnique(nullptr, pipelineCreateInfo);
}
vk::UniqueShaderModule Pipeline::createShaderModule(const std::string &code) {
    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(), .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    return device->getDevice()->createShaderModuleUnique(createInfo);
}
void Pipeline::createRenderPass() {
    vk::AttachmentDescription colorAttachment{.format = swapchain->getSwapchainImageFormat(),
                                              .samples = vk::SampleCountFlagBits::e1,
                                              .loadOp = vk::AttachmentLoadOp::eClear,
                                              .storeOp = vk::AttachmentStoreOp::eStore,
                                              .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                                              .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                              .initialLayout = vk::ImageLayout::eUndefined,
                                              .finalLayout = vk::ImageLayout::ePresentSrcKHR};

    vk::AttachmentReference colorAttachmentReference{.attachment = 0, .layout = vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription subpassDescription{.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = &colorAttachmentReference};

    vk::SubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                     .dstSubpass = 0,
                                     .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                     .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                     .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite};

    vk::RenderPassCreateInfo renderPassCreateInfo{.attachmentCount = 1,
                                                  .pAttachments = &colorAttachment,
                                                  .subpassCount = 1,
                                                  .pSubpasses = &subpassDescription,
                                                  .dependencyCount = 1,
                                                  .pDependencies = &dependency};


    renderPass = device->getDevice()->createRenderPassUnique(renderPassCreateInfo);
}

void Pipeline::createDescriptorSetLayout() {
    vk::DescriptorSetLayoutBinding uboLayoutBinding{.binding = 0,
                                                    .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                    .descriptorCount = 1,
                                                    .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                                    .pImmutableSamplers = nullptr};

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};

    descriptorSetLayout = device->getDevice()->createDescriptorSetLayoutUnique(layoutCreateInfo);
}

const vk::RenderPass &Pipeline::getRenderPass() const { return renderPass.get(); }
const vk::UniquePipeline &Pipeline::getPipeline() const { return pipeline; }
const vk::UniqueDescriptorSetLayout &Pipeline::getDescriptorSetLayout() const { return descriptorSetLayout; }
const vk::UniquePipelineLayout &Pipeline::getPipelineLayout() const { return pipelineLayout; }
