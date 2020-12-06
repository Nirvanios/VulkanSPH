//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_GLFWWINDOW_H
#define VULKANAPP_GLFWWINDOW_H

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include "EventDispatchingWindow.h"
#include <GLFW/glfw3.h>

enum class CallbackTypes { eFramebufferSize, eMouse };

struct DestroyglfwWin {

  void operator()(GLFWwindow *ptr) { glfwDestroyWindow(ptr); }
};

using Window = std::unique_ptr<GLFWwindow, DestroyglfwWin>;

class GlfwWindow : public EventDispatchingWindow {
 public:
  [[nodiscard]] const std::string &getWindowName() const;
  explicit GlfwWindow(const std::string &windowName = "VulkanApp", int width = 800,
                      int height = 600);
  virtual ~GlfwWindow();
  [[nodiscard]] const Window &getWindow() const;
  [[nodiscard]] int getWidth() const;
  [[nodiscard]] int getHeight() const;
  void setFramebufferCallback();
  void checkMinimized();

 private:
  Window window;
  std::string windowName;
  int width, height;

  static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
  static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
  static void mousePositionCallback(GLFWwindow *window, double xpos, double ypos);
  void cleanUp();
};

#endif//VULKANAPP_GLFWWINDOW_H
