//
// Created by Igor Frank on 18.10.20.
//

#include "Swapchain.h"
#include <spdlog/spdlog.h>


SwapChainSupportDetails Swapchain::querySwapChainSupport(vk::PhysicalDevice physicalDevice, const vk::UniqueSurfaceKHR &surface) {
    SwapChainSupportDetails details;

    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
    details.formats = physicalDevice.getSurfaceFormatsKHR(surface.get());
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface.get());
    return details;
}

//TODO combine
vk::SurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    auto it = std::find_if(availableFormats.begin(), availableFormats.end(), [](const auto &availableFormat) {
        return availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    if (it == availableFormats.end())
        return availableFormats[0];
    else
        return *it;
}

vk::PresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    auto it = std::find_if(availablePresentModes.begin(), availablePresentModes.end(), [](const auto &availableFormat) {
        return availableFormat == vk::PresentModeKHR::eMailbox;
    });
    if (it == availablePresentModes.end())
        return vk::PresentModeKHR::eFifo;
    else
        return *it;
}
vk::Extent2D Swapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        vk::Extent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}
void Swapchain::createSwapchain() {
    auto swapChainSupport = querySwapChainSupport(device->getPhysicalDevice(), surface);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    swapchainImageFormat = surfaceFormat.format;
    auto presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    int width, height;
    glfwGetFramebufferSize(window.getWindow().get(), &width, &height);
    swapchainExtent = chooseSwapExtent(swapChainSupport.capabilities, width, height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{.surface = surface.get(),
                                          .minImageCount = imageCount,
                                          .imageFormat = surfaceFormat.format,
                                          .imageColorSpace = surfaceFormat.colorSpace,
                                          .imageExtent = swapchainExtent,
                                          .imageArrayLayers = 1,
                                          .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
                                          .preTransform = swapChainSupport.capabilities.currentTransform,
                                          .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                          .presentMode = presentMode,
                                          .clipped = VK_TRUE,
                                          .oldSwapchain = nullptr};

    auto indices = Device::findQueueFamilies(device->getPhysicalDevice(), surface);
    std::array<uint32_t, 2> queueFamilyIndices = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;    // Optional
        createInfo.pQueueFamilyIndices = nullptr;// Optional
    }
    swapchain = device->getDevice()->createSwapchainKHRUnique(createInfo);

    swapchainImages.clear();
    swapchainImages = device->getDevice()->getSwapchainImagesKHR(swapchain.get());
}

Swapchain::Swapchain(std::shared_ptr<Device> device, const vk::UniqueSurfaceKHR &surface,const GlfwWindow &window) : device(device), window(window), surface(surface) {
    createSwapchain();
    spdlog::debug("Created swapchain.");
    createImageViews();
    spdlog::debug("Created imageviews.");
}

const vk::UniqueSwapchainKHR &Swapchain::getSwapchain() const {
    return swapchain;
}

void Swapchain::createImageViews() {
    swapChainImageViews.clear();
    for (const auto &swapchainImage : swapchainImages) {
        vk::ImageViewCreateInfo createInfo{.image = swapchainImage,
                                           .viewType = vk::ImageViewType::e2D,
                                           .format = swapchainImageFormat,
                                           .components = {.r = vk::ComponentSwizzle::eIdentity,
                                                          .g = vk::ComponentSwizzle::eIdentity,
                                                          .b = vk::ComponentSwizzle::eIdentity,
                                                          .a = vk::ComponentSwizzle::eIdentity},
                                           .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                                .baseMipLevel = 0,
                                                                .levelCount = 1,
                                                                .baseArrayLayer = 0,
                                                                .layerCount = 1}};
        swapChainImageViews.emplace_back(device->getDevice()->createImageViewUnique(createInfo));
    }

}
const vk::Extent2D &Swapchain::getSwapchainExtent() const {
    return swapchainExtent;
}
vk::Format Swapchain::getSwapchainImageFormat() const {
    return swapchainImageFormat;
}
const std::vector<vk::UniqueImageView> &Swapchain::getSwapChainImageViews() const {
    return swapChainImageViews;
}
