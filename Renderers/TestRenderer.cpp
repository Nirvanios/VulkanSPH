//
// Created by Igor Frank on 01.11.20.
//

#include "TestRenderer.h"

#include <ctime>
#include <utility>

TestRenderer::TestRenderer(const Config &config)
    : config(config), window(config.getVulkan().window.name, config.getVulkan().window.width, config.getVulkan().window.height),
      vulkanCore(config, window, camera.Position),
      keyMovementSubscriber(window.subscribeToKeyEvents([this](KeyMessage message) { cameraKeyMovement(message); })),
      mouseMovementSubscriber(window.subscribeToMouseMovementEvents([this](MouseMovementMessage message) { cameraMouseMovement(message); })),
      mouseButtonSubscriber(window.subscribeToMouseButtonEvents([this](MouseButtonMessage message) { cameraMouseButton(message); })) {
    vulkanCore.setViewMatrixGetter([this]() { return camera.GetViewMatrix(); });

    auto particles = createParticles();
    vulkanCore.initVulkan(Utilities::loadModelFromObj(config.getApp().simulation.particleModel), std::span<ParticleRecord>{particles.data(), particles.size()});
}

void TestRenderer::run() { vulkanCore.run(); }

void TestRenderer::cameraKeyMovement(KeyMessage messgae) {
    switch (messgae.key) {
        case GLFW_KEY_W:
        case GLFW_KEY_UP:
            camera.ProcessKeyboard(Camera_Movement::FORWARD, 0.1);
            break;
        case GLFW_KEY_S:
        case GLFW_KEY_DOWN:
            camera.ProcessKeyboard(Camera_Movement::BACKWARD, 0.1);
            break;
        case GLFW_KEY_A:
        case GLFW_KEY_LEFT:
            camera.ProcessKeyboard(Camera_Movement::LEFT, 0.1);
            break;
        case GLFW_KEY_D:
        case GLFW_KEY_RIGHT:
            camera.ProcessKeyboard(Camera_Movement::RIGHT, 0.1);
            break;
    }
}
void TestRenderer::cameraMouseMovement(MouseMovementMessage message) {
    if (leftMouseButtonPressed) {
        camera.ProcessMouseMovement(static_cast<float>(xMousePosition - message.xPosition), static_cast<float>(message.yPosition - yMousePosition));
        xMousePosition = message.xPosition;
        yMousePosition = message.yPosition;
    }
}
void TestRenderer::cameraMouseButton(MouseButtonMessage message) {
    leftMouseButtonPressed = (message.button == GLFW_MOUSE_BUTTON_LEFT && message.action == MouseButtonAction::Press);
    if (leftMouseButtonPressed) {
        glfwGetCursorPos(window.getWindow().get(), &xMousePosition, &yMousePosition);
        glfwSetInputMode(window.getWindow().get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(window.getWindow().get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

std::vector<ParticleRecord> TestRenderer::createParticles() {
    std::vector<ParticleRecord> data{static_cast<size_t>(config.getApp().simulation.particleCount)};
    std::srand(std::time(nullptr));
    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 2; ++x) {
                data[(z * 4 * 2) + (y * 2) + x].position = glm::vec4{x, y, z, 0.0f};
                data[(z * 4 * 2) + (y * 2) + x].velocity = glm::vec4{std::rand(), std::rand(), std::rand(), 0.0f};
                data[(z * 4 * 2) + (y * 2) + x].velocity /= RAND_MAX;
                data[(z * 4 * 2) + (y * 2) + x].velocity *= 1;
            }
        }
    }
    return data;
}

TestRenderer::~TestRenderer() {
    mouseButtonSubscriber.unsubscribe();
    mouseMovementSubscriber.unsubscribe();
    keyMovementSubscriber.unsubscribe();
}
