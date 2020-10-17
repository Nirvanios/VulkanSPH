//
// Created by Igor Frank on 17.10.20.
//

#include "spdlog/spdlog.h"

#include "GlfwWindow.h"

GlfwWindow::GlfwWindow(const std::string& windowName, const int width, const int height){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = std::unique_ptr<GLFWwindow, DestroyglfwWin>(glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr));
    this->windowName = windowName;
    spdlog::debug(fmt::format("Creating window \"{}\". Size: {}x{}.", windowName, width, height));
}

const Window &GlfwWindow::getWindow() const {
    return window;
}
void GlfwWindow::cleanUp() {
    spdlog::debug("Destroying window.");
    glfwTerminate();
}
GlfwWindow::~GlfwWindow() {
    cleanUp();
}
const std::string &GlfwWindow::getWindowName() const {
    return windowName;
}
