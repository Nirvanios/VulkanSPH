//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_GLFWWINDOW_H
#define VULKANAPP_GLFWWINDOW_H

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

enum class CallbackTypes{
    eFramebufferSize,
    eMouse
};


struct DestroyglfwWin {

    void operator()(GLFWwindow *ptr) {
        glfwDestroyWindow(ptr);
    }
};

using Window = std::unique_ptr<GLFWwindow, DestroyglfwWin>;

class GlfwWindow {
public:
    [[nodiscard]] const std::string &getWindowName() const;
    explicit GlfwWindow(const std::string &windowName = "VulkanApp", int width = 800, int height = 600);
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
    void cleanUp();
};


#endif//VULKANAPP_GLFWWINDOW_H
