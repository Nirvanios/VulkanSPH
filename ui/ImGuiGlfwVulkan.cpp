//
// Created by Igor Frank on 21.01.21.
//

#include "ImGuiGlfwVulkan.h"
#include "../vulkan/VulkanCore.h"
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <utility>

namespace details {
void checkVkResult(VkResult err) {
  if (err == 0) { return; }
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0) { throw std::runtime_error(fmt::format("Error: VkResult = {}", err)); }
}
}// namespace details

pf::ui::ig::ImGuiGlfwVulkan::ImGuiGlfwVulkan(std::shared_ptr<Device> inDevice,
                                             const std::shared_ptr<Instance> &instance,
                                             vk::RenderPass pass, const vk::UniqueSurfaceKHR &surf,
                                             std::shared_ptr<Swapchain> inSwapChain,
                                             GLFWwindow *handle, ImGuiConfigFlags flags,
                                             toml::table config)
    : ImGuiInterface(flags, std::move(config)), device(std::move(inDevice)),
      swapChain(std::move(inSwapChain)) {
  const auto physicalDevice = device->getPhysicalDevice();
  const auto imageCount = swapChain->getSwapchainImageCount();

  setupDescriptorPool();
  ImGui_ImplGlfw_InitForVulkan(handle, true);
  auto init_info = ImGui_ImplVulkan_InitInfo();
  init_info.Instance = instance->getInstance();
  init_info.PhysicalDevice = physicalDevice;
  init_info.Device = device->getDevice().get();
  init_info.QueueFamily =
      device->findQueueFamilies(device->getPhysicalDevice(), surf).graphicsFamily.value();
  init_info.Queue = device->getPresentQueue();
  init_info.PipelineCache = VK_NULL_HANDLE;
  init_info.DescriptorPool = descriptorPool.get();
  init_info.Allocator = nullptr;
  init_info.MinImageCount = imageCount;
  init_info.ImageCount = imageCount;
  init_info.CheckVkResultFn = details::checkVkResult;
  ImGui_ImplVulkan_Init(&init_info, pass);

  uploadFonts(surf);

  /*
    swapChain->addRebuildListener(
      [&] { ImGui_ImplVulkan_SetMinImageCount(swapChain->getImageViews().size()); });
*/
}
void pf::ui::ig::ImGuiGlfwVulkan::addToCommandBuffer(const vk::UniqueCommandBuffer &commandBuffer) {
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.get());
}
pf::ui::ig::ImGuiGlfwVulkan::~ImGuiGlfwVulkan() {
  device->getDevice()->waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
void pf::ui::ig::ImGuiGlfwVulkan::renderImpl() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (hasMenuBar()) { menuBar->render(); }
  std::ranges::for_each(getWindows(), [](auto &window) { window.render(); });
  renderDialogs();
  ImGui::Render();
}
void pf::ui::ig::ImGuiGlfwVulkan::setupDescriptorPool() {
  constexpr auto DESCRIPTOR_COUNT = 1000;
  std::array<vk::DescriptorPoolSize, 11> poolSize{
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageImage,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformTexelBuffer,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageTexelBuffer,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBufferDynamic,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBufferDynamic,
                             .descriptorCount = DESCRIPTOR_COUNT},
      vk::DescriptorPoolSize{.type = vk::DescriptorType::eInputAttachment,
                             .descriptorCount = DESCRIPTOR_COUNT}};

  vk::DescriptorPoolCreateInfo poolCreateInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = DESCRIPTOR_COUNT * poolSize.size(),
      .poolSizeCount = poolSize.size(),
      .pPoolSizes = poolSize.data()};
  descriptorPool = device->getDevice()->createDescriptorPoolUnique(poolCreateInfo);
}
void pf::ui::ig::ImGuiGlfwVulkan::uploadFonts(const vk::UniqueSurfaceKHR &surf) {
  auto queueFamilyIndices = Device::findQueueFamilies(device->getPhysicalDevice(), surf);

  vk::CommandPoolCreateInfo commandPoolCreateInfoGraphics{
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  auto commandPool = device->getDevice()->createCommandPoolUnique(commandPoolCreateInfoGraphics);

  auto commandBuffer = VulkanUtils::beginOnetimeCommand(commandPool, device);
  ImGui_ImplVulkan_CreateFontsTexture(*commandBuffer);
  VulkanUtils::endOnetimeCommand(std::move(commandBuffer), device->getGraphicsQueue());

  device->getDevice()->waitIdle();
}
