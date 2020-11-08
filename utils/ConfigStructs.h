//
// Created by Igor Frank on 06.11.20.
//

#ifndef VULKANAPP_CONFIGSTRUCTS_H
#define VULKANAPP_CONFIGSTRUCTS_H

#include <string>

struct ShadersConfig {
    std::string vertex;
    std::string fragemnt;
    std::string compute;
};

struct WindowConfig {
    std::string name;
    int width;
    int height;
};

struct VulkanConfig {
    WindowConfig window;
    ShadersConfig shaders;
};

struct SimulationConfig {
    int particleCount;
    std::string particleModel;
};

struct AppConfig {
    bool DEBUG;
    SimulationConfig simulation;
};

#endif//VULKANAPP_CONFIGSTRUCTS_H
