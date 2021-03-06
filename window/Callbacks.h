//
// Created by Igor Frank on 23.10.20.
//

#ifndef VULKANAPP_CALLBACKS_H
#define VULKANAPP_CALLBACKS_H

#include "../vulkan/VulkanCore.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
void framebufferResizeCallback(GLFWwindow *window, int, int) {
  auto app = reinterpret_cast<VulkanCore *>(glfwGetWindowUserPointer(window));
  app->setFramebufferResized(true);
  spdlog::debug("Callback resized");
}

#endif//VULKANAPP_CALLBACKS_H
