//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_SWAPCHAIN_H
#define VULKANAPP_SWAPCHAIN_H

#include "Device.h"
#include <vector>
#include <vulkan/vulkan.hpp>
struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

class Swapchain {

public:
    const vk::UniqueSwapchainKHR &getSwapchain() const;

    Swapchain(std::shared_ptr<Device> device, const vk::UniqueSurfaceKHR &surface, int width, int height);
    static SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice physicalDevice, const vk::UniqueSurfaceKHR &surface);
    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    static vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, int width, int height);


private:
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;
    vk::UniqueSwapchainKHR swapchain;
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::UniqueImageView> swapChainImageViews;


    std::shared_ptr<Device> device;
    const vk::UniqueSurfaceKHR &surface;
    int width, height;

    void createSwapchain();
    void createImageViews();
};


#endif//VULKANAPP_SWAPCHAIN_H
