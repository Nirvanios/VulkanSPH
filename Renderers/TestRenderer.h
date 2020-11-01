//
// Created by Igor Frank on 01.11.20.
//

#ifndef VULKANAPP_TESTRENDERER_H
#define VULKANAPP_TESTRENDERER_H


#include "../Third Party/Camera.h"
#include "../vulkan/VulkanCore.h"
class TestRenderer {
public:
    explicit TestRenderer(bool debug);
    virtual ~TestRenderer();
    void run();
private:
    bool leftMouseButtonPressed = false;
    double xMousePosition = 0.0, yMousePosition = 0.0;
    const bool DEBUG;

    Camera camera;
    const std::string windowName = "TestRenderer";
    GlfwWindow window{windowName};
    VulkanCore vulkanCore{window, DEBUG};

    Unsubscriber keyMovementSubscriber;
    Unsubscriber mouseMovementSubscriber;
    Unsubscriber mouseButtonSubscriber;

    void cameraKeyMovement(KeyMessage messgae);
    void cameraMouseMovement(MouseMovementMessage message);
    void cameraMouseButton(MouseButtonMessage message);

};


#endif//VULKANAPP_TESTRENDERER_H
