#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include "spdlog/spdlog.h"

#include "vulkan/VulkanCore.h"

void setupLogger(){
    spdlog::set_level(spdlog::level::debug);
}

int main() {
    setupLogger();

    VulkanCore vulkanCore;

    try {
        vulkanCore.run();
    } catch (const std::exception& e) {
        spdlog::error(fmt::format(e.what()));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}