//
// Created by Igor Frank on 17.10.20.
//

#ifndef VULKANAPP_GLFWWINDOW_H
#define VULKANAPP_GLFWWINDOW_H

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


struct DestroyglfwWin{

    void operator()(GLFWwindow* ptr){
        glfwDestroyWindow(ptr);
    }

};

using Window = std::unique_ptr<GLFWwindow, DestroyglfwWin>;

class GlfwWindow {
private:
    Window window;

public:
    explicit GlfwWindow(const std::string& windowName = "VulkanApp", int width = 800, int height = 600);
    virtual ~GlfwWindow();
    [[nodiscard]] const Window &getWindow() const;

private:
    void cleanUp();
};


#endif//VULKANAPP_GLFWWINDOW_H
