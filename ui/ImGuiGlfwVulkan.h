//
// Created by Igor Frank on 21.01.21.
//

#ifndef VULKANAPP_IMGUIGLFWVULKAN_H
#define VULKANAPP_IMGUIGLFWVULKAN_H

#include "../vulkan/types/Device.h"
#include "../vulkan/types/Swapchain.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <pf_imgui/ImGuiInterface.h>
#include <vulkan/vulkan.hpp>
namespace pf::ui::ig {

class ImGuiGlfwVulkan : public ImGuiInterface {
 public:
  ImGuiGlfwVulkan(std::shared_ptr<Device> inDevice, const std::shared_ptr<Instance>& instance,
                  vk::RenderPass pass, const vk::UniqueSurfaceKHR &surf,
                  std::shared_ptr<Swapchain> inSwapChain, GLFWwindow *handle,
                  ImGuiConfigFlags flags, toml::table config);

  void addToCommandBuffer(const vk::UniqueCommandBuffer &commandBuffer);

  ~ImGuiGlfwVulkan() override;

 protected:
  void renderImpl() override;

 private:
  void setupDescriptorPool();
  void uploadFonts(const vk::UniqueSurfaceKHR &surf);
  vk::UniqueDescriptorPool descriptorPool;
  std::shared_ptr<Device> device;
  std::shared_ptr<Swapchain> swapChain;
};

}// namespace pf::ui

#endif//VULKANAPP_IMGUIGLFWVULKAN_H
