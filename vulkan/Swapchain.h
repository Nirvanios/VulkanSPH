//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_SWAPCHAIN_H
#define VULKANAPP_SWAPCHAIN_H

#include "../window/GlfwWindow.h"
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
    [[nodiscard]] const vk::UniqueSwapchainKHR &getSwapchain() const;
    [[nodiscard]] const vk::Extent2D &getSwapchainExtent() const;
    [[nodiscard]] vk::Format getSwapchainImageFormat() const;
    [[nodiscard]] const std::vector<vk::UniqueImageView> &getSwapChainImageViews() const;

    void createSwapchain();
    void createImageViews();

    Swapchain(std::shared_ptr<Device> device, const vk::UniqueSurfaceKHR &surface, const GlfwWindow &window);
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
    const GlfwWindow &window;
    const vk::UniqueSurfaceKHR &surface;

};


#endif//VULKANAPP_SWAPCHAIN_H
