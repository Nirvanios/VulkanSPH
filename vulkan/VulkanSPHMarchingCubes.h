//
// Created by Igor Frank on 19.04.21.
//

#ifndef VULKANAPP_VULKANSPHMARCHINGCUBES_H
#define VULKANAPP_VULKANSPHMARCHINGCUBES_H

#include "../ui/ImGuiGlfwVulkan.h"
#include "../utils/Config.h"
#include "types/Buffer.h"
#include "types/DescriptorSet.h"
#include "types/Device.h"
#include "types/Framebuffers.h"
#include "types/Swapchain.h"
#include "types/Types.h"

class VulkanSPHMarchingCubes {

 public:
  VulkanSPHMarchingCubes(const Config &inConfig, const SimulationInfoSPH &simulationInfo,
                         const GridInfo &inGridInfo, std::shared_ptr<Device> inDevice,
                         const vk::UniqueSurfaceKHR &surface,
                         std::shared_ptr<Swapchain> inSwapchain,
                         std::shared_ptr<Buffer> inBufferParticles,
                         std::shared_ptr<Buffer> inBufferIndexes,
                         std::shared_ptr<Buffer> inBufferSortedPairs,
                         std::vector<std::shared_ptr<Buffer>> inBuffersUniformMVP,
                         std::vector<std::shared_ptr<Buffer>> inBuffersUniformCameraPos);
  vk::UniqueSemaphore run(const vk::UniqueSemaphore &inSemaphore);
  vk::UniqueSemaphore draw(const vk::UniqueSemaphore &inSemaphore, unsigned int imageIndex,
                           const vk::UniqueFence &fenceInFlight);
  void setFramebuffersSwapchain(const std::shared_ptr<Framebuffers> &framebuffer);
  void setImgui(std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> inImgui);

 private:
  enum class Stages { ComputeColors, Render };
  enum class RenderStages { Vertex, Geometry, Fragment };

  void submit(Stages pipelineStage, const vk::Fence &submitFence = nullptr,
              const std::optional<vk::Semaphore> &inSemaphore = std::nullopt,
              const std::optional<vk::Semaphore> &outSemaphore = std::nullopt,
              SubmitSemaphoreType submitSemaphoreType = SubmitSemaphoreType::None);
  void recordCommandBuffer(Stages pipelineStage, unsigned int imageIndex);
  void recordRenderpass(unsigned int imageIndex, const vk::UniqueCommandBuffer &commandBuffer);
  void swapBuffers(std::shared_ptr<Buffer> &buffer1, std::shared_ptr<Buffer> &buffer2);
  void updateDescriptorSets();
  void fillDescriptorBufferInfo();
  void createBuffers();
  void waitFence();

  std::string shaderPathTemplate;
  std::map<RenderStages, std::string> renderShaderFiles = {
      {RenderStages::Vertex, "shader.vert"},
      {RenderStages::Geometry, "GenerateTriangles.geom"},
      {RenderStages::Fragment, "shader.frag"}};
  std::map<Stages, std::string> computeShaderFiles{
      {Stages::ComputeColors, "ColorCompute.comp"},
  };

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
  int currentFrame = 0;

  const Config &config;
  MarchingCubesInfo marchingCubesInfo;

  std::shared_ptr<pf::ui::ig::ImGuiGlfwVulkan> imgui;

  std::shared_ptr<Device> device;
  std::shared_ptr<Swapchain> swapchain;

  std::shared_ptr<Framebuffers> framebuffersSwapchain;

  vk::Queue queueCompute;
  vk::Queue queueRender;

  vk::UniqueDescriptorPool descriptorPool;
  std::map<Stages, std::shared_ptr<DescriptorSet>> descriptorSets;

  vk::UniqueCommandPool commandPoolCompute;
  vk::UniqueCommandPool commandPoolRender;
  vk::UniqueCommandBuffer commandBufferCompute;
  std::vector<vk::UniqueCommandBuffer> commandBufferRender;

  std::vector<vk::UniqueSemaphore> semaphores;
  int currentSemaphore;

  vk::UniqueFence fence;

  std::shared_ptr<Buffer> bufferGridColors;
  std::shared_ptr<Buffer> bufferParticles;
  std::shared_ptr<Buffer> bufferGrid;
  std::shared_ptr<Buffer> bufferIndexes;
  std::shared_ptr<Buffer> bufferEdgeToVertexLUT;
  std::shared_ptr<Buffer> bufferPolygonCountLUT;
  std::shared_ptr<Buffer> bufferEdgesLUT;

  std::shared_ptr<Buffer> bufferVertex;
  std::vector<std::shared_ptr<Buffer>> bufferUnioformMVP;
  std::vector<std::shared_ptr<Buffer>> bufferUnioformCameraPos;

  std::map<Stages, std::shared_ptr<Pipeline>> pipelines;

  std::map<Stages, std::vector<DescriptorBufferInfo>> descriptorBufferInfosCompute;

  std::map<Stages, std::vector<PipelineLayoutBindingInfo>> bindingInfos{
      {Stages::ComputeColors,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 2,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute},
        PipelineLayoutBindingInfo{.binding = 3,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eCompute}}},
      {Stages::Render,
       {PipelineLayoutBindingInfo{.binding = 0,
                                  .descriptorType = vk::DescriptorType::eUniformBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eVertex},
        PipelineLayoutBindingInfo{.binding = 1,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 2,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 3,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 4,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 5,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 6,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 7,
                                  .descriptorType = vk::DescriptorType::eStorageBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eGeometry},
        PipelineLayoutBindingInfo{.binding = 8,
                                  .descriptorType = vk::DescriptorType::eUniformBuffer,
                                  .descriptorCount = 1,
                                  .stageFlags = vk::ShaderStageFlagBits::eFragment},
        {PipelineLayoutBindingInfo{.binding = 9,
                                   .descriptorType = vk::DescriptorType::eUniformBuffer,
                                   .descriptorCount = 1,
                                   .stageFlags = vk::ShaderStageFlagBits::eGeometry}}}}};
};

#endif//VULKANAPP_VULKANSPHMARCHINGCUBES_H
