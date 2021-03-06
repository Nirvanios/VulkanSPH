//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_DEVICE_H
#define VULKANAPP_DEVICE_H

#include "Instance.h"

class Device {
 private:
  struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;

    [[nodiscard]] bool isComplete() const {
      return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
    }
  };

  const std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
      //VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
      VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME
  };

  QueueFamilyIndices indices;

  bool debug;

  vk::PhysicalDevice physicalDevice;
  vk::UniqueDevice device;
  const vk::UniqueSurfaceKHR &surface;

  std::shared_ptr<Instance> instance;

  void pickPhysicalDevice();

  void createLogicalDevice();
  bool checkDeviceExtensionSupport(const vk::PhysicalDevice &phyDevice);

 public:
  explicit Device(std::shared_ptr<Instance> instance, const vk::UniqueSurfaceKHR &surface,
                  bool debug = false);

  [[nodiscard]] vk::Queue getGraphicsQueue() const;
  [[nodiscard]] vk::Queue getPresentQueue() const;
  [[nodiscard]] vk::Queue getComputeQueue() const;
  [[nodiscard]] const vk::PhysicalDevice &getPhysicalDevice() const;
  [[nodiscard]] const vk::UniqueDevice &getDevice() const;
  [[nodiscard]] std::vector<vk::UniqueCommandBuffer>
  allocateCommandBuffer(const vk::UniqueCommandPool &commandPool, uint32_t count) const;

  [[nodiscard]] static QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice &device,
                                                            const vk::UniqueSurfaceKHR &surface);
};

#endif//VULKANAPP_DEVICE_H
