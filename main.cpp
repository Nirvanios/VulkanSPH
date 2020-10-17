#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "spdlog/spdlog.h"

#include "vulkan/VulkanCore.h"
#include "window/GlfwWindow.h"

void setupLogger(bool debug = false) {
    if (debug)
        spdlog::set_level(spdlog::level::debug);
}

int main() {
    const auto DEBUG = true;
    setupLogger(DEBUG);

    GlfwWindow window;
    VulkanCore vulkanCore{window, DEBUG};

    try {
        vulkanCore.run();
    } catch (const std::exception &e) {
        spdlog::error(fmt::format(e.what()));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}