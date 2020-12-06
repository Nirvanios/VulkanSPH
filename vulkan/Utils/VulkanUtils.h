//
// Created by Igor Frank on 30.10.20.
//

#ifndef VULKANAPP_VULKANUTILS_H
#define VULKANAPP_VULKANUTILS_H

#include <shaderc/shaderc.hpp>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.hpp>

#include "../Device.h"
#include "ShaderIncluder.h"

namespace VulkanUtils {

inline std::vector<uint32_t> compileShader(const std::string &source_name, shaderc_shader_kind kind,
                                           const std::string &source) {
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);

  options.SetTargetSpirv(shaderc_spirv_version_1_3);
  options.SetIncluder(std::make_unique<ShaderIncluder>());

  auto result = compiler.PreprocessGlsl(source, kind, source_name.c_str(), options);
  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    throw vk::InvalidShaderNVError(fmt::format("Shader preprocess: {}", result.GetErrorMessage()));
  }

  auto module = compiler.CompileGlslToSpv({result.cbegin(), result.cend()}, kind,
                                          source_name.c_str(), options);
  if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
    throw vk::InvalidShaderNVError(fmt::format("Shader compilation: {}", module.GetErrorMessage()));
  }

  return {module.cbegin(), module.cend()};
}

inline uint32_t findMemoryType(const std::shared_ptr<Device> &device, uint32_t typeFilter,
                               const vk::MemoryPropertyFlags &properties) {
  auto memProperties = device->getPhysicalDevice().getMemoryProperties();
  uint32_t i = 0;
  for (const auto &type : memProperties.memoryTypes) {
    if ((typeFilter & (1 << i)) && (type.propertyFlags & properties)) { return i; }
    ++i;
  }
  throw std::runtime_error("failed to find suitable memory type!");
}

inline bool hasStencilComponent(vk::Format format) {
  return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

inline vk::UniqueCommandBuffer beginOnetimeCommand(const vk::UniqueCommandPool &commandPool,
                                                   std::shared_ptr<Device> device) {
  vk::CommandBufferAllocateInfo allocateInfo{.commandPool = commandPool.get(),
                                             .level = vk::CommandBufferLevel::ePrimary,
                                             .commandBufferCount = 1};
  auto commandBuffer = device->getDevice()->allocateCommandBuffersUnique(allocateInfo);

  vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
  commandBuffer[0]->begin(beginInfo);
  return std::move(commandBuffer[0]);
}
inline void endOnetimeCommand(vk::UniqueCommandBuffer commandBuffer, const vk::Queue &queue,
                              const std::vector<vk::Semaphore> &semaphores = {}) {
  commandBuffer->end();
  vk::SubmitInfo submitInfo{.waitSemaphoreCount = static_cast<uint32_t>(semaphores.size()),
                            .pWaitSemaphores = semaphores.data(),
                            .commandBufferCount = 1,
                            .pCommandBuffers = &commandBuffer.get()};

  queue.submit(1, &submitInfo, nullptr);
  queue.waitIdle();
}

}// namespace VulkanUtils

#endif//VULKANAPP_VULKANUTILS_H
