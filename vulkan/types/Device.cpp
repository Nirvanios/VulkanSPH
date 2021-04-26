//
// Created by Igor Frank on 18.10.20.
//

#include <algorithm>
#include <utility>

#include "../../utils/Utilities.h"
#include "Device.h"
#include "Swapchain.h"
#include <set>
#include <spdlog/spdlog.h>

Device::Device(std::shared_ptr<Instance> instance, const vk::UniqueSurfaceKHR &surface, bool debug)
    : debug(debug), surface(surface), instance(std::move(instance)) {
  pickPhysicalDevice();
  createLogicalDevice();
  spdlog::debug("Created logical device.");
}

void Device::pickPhysicalDevice() {
  auto devices = instance->getInstance().enumeratePhysicalDevices();
  if (devices.empty()) { throw std::runtime_error("Failed to find GPUs with Vulkan support!"); }
  auto it =
      std::find_if(devices.begin(), devices.end(), [this](const vk::PhysicalDevice &phyDevice) {
        auto properties = phyDevice.getProperties();
        auto features = phyDevice.getFeatures();
        auto extensionsSupported = checkDeviceExtensionSupport(phyDevice);
        auto swapchainAdequate = false;
        if (extensionsSupported) {
          auto swapchainSupportDetails = Swapchain::querySwapChainSupport(phyDevice, surface);
          swapchainAdequate = !swapchainSupportDetails.formats.empty()
              && !swapchainSupportDetails.presentModes.empty();
        }
        return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu
            && features.geometryShader && findQueueFamilies(phyDevice, surface).isComplete()
            && extensionsSupported && swapchainAdequate && features.samplerAnisotropy;
      });

  if (it == devices.end()) { throw std::runtime_error("Failed to find suitable GPU!"); }
  physicalDevice = *it;
  for (const auto &availableDevice : devices) {
    if (availableDevice == *it)
      spdlog::info(fmt::format("Available GPU: {}.<-- SELECTED",
                               availableDevice.getProperties().deviceName));
    else
      spdlog::info(fmt::format("Available GPU: {}.", availableDevice.getProperties().deviceName));
  }
}

Device::QueueFamilyIndices Device::findQueueFamilies(const vk::PhysicalDevice &device,
                                                     const vk::UniqueSurfaceKHR &surface) {
  QueueFamilyIndices indices;

  uint32_t i = 0;
  auto queueFamilies = device.getQueueFamilyProperties();
  std::for_each(queueFamilies.begin(), queueFamilies.end(),
                [&i, &indices, &device, &surface](const vk::QueueFamilyProperties &property) {
                  auto presentSupport = device.getSurfaceSupportKHR(i, surface.get());
                  if (property.queueFlags & vk::QueueFlagBits::eGraphics)
                    indices.graphicsFamily = i;
                  if (presentSupport) { indices.presentFamily = i; }
                  if (property.queueFlags & vk::QueueFlagBits::eCompute) indices.computeFamily = i;
                  ++i;
                });
  return indices;
}

void Device::createLogicalDevice() {
  indices = findQueueFamilies(physicalDevice, surface);
  auto priority = 1.0f;
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    vk::DeviceQueueCreateInfo queueCreateInfo{.queueFamilyIndex = queueFamily,
                                              .queueCount = 1,
                                              .pQueuePriorities = &priority};
    queueCreateInfos.emplace_back(queueCreateInfo);
  }
  vk::PhysicalDeviceFeatures deviceFeatures{.geometryShader = VK_TRUE, .samplerAnisotropy = VK_TRUE};
  vk::DeviceCreateInfo createInfo{
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
      .pEnabledFeatures = &deviceFeatures};

  if (debug) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(instance->getValidationLayers().size());
    createInfo.ppEnabledLayerNames = instance->getValidationLayers().data();
  } else {
    createInfo.enabledLayerCount = 0;
  }
  device = physicalDevice.createDeviceUnique(createInfo);
}

vk::Queue Device::getGraphicsQueue() const {
  return device->getQueue(indices.graphicsFamily.value(), 0);
}

vk::Queue Device::getPresentQueue() const {
  return device->getQueue(indices.presentFamily.value(), 0);
}

vk::Queue Device::getComputeQueue() const {
  return device->getQueue(indices.computeFamily.value(), 0);
}

const vk::PhysicalDevice &Device::getPhysicalDevice() const { return physicalDevice; }

const vk::UniqueDevice &Device::getDevice() const { return device; }

bool Device::checkDeviceExtensionSupport(const vk::PhysicalDevice &phyDevice) {
  auto availableExtensions = phyDevice.enumerateDeviceExtensionProperties();
  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}
std::vector<vk::UniqueCommandBuffer>
Device::allocateCommandBuffer(const vk::UniqueCommandPool &commandPool, uint32_t count) const {
  std::vector<vk::UniqueCommandBuffer> commandBuffer{count};
  vk::CommandBufferAllocateInfo bufferAllocateInfo{.commandPool = commandPool.get(),
                                                   .level = vk::CommandBufferLevel::ePrimary,
                                                   .commandBufferCount =
                                                       static_cast<uint32_t>(commandBuffer.size())};
  return device->allocateCommandBuffersUnique(bufferAllocateInfo);
}
