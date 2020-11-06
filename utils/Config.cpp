//
// Created by Igor Frank on 02.11.20.
//
#include "Config.h"

#include <iostream>
#include <toml.hpp>

Config::Config(const std::string &configFile) {
    const auto data = toml::parse(configFile);
    const auto &tomlApp = toml::find(data, "App");
    const auto &tomlVulkan = toml::find(data, "Vulkan");
    const auto &tomlShaders = toml::find(tomlVulkan, "shaders");
    const auto &tomlWindow = toml::find(tomlVulkan, "window");

    app.DEBUG = toml::find<bool>(tomlApp, "DEBUG");
    Vulkan.window.height = toml::find<int>(tomlWindow, "height");
    Vulkan.window.width = toml::find<int>(tomlWindow, "width");
    Vulkan.window.name = toml::find<std::string>(tomlWindow, "name");

    Vulkan.shaders.fragemnt = toml::find<std::string>(tomlShaders, "fragment");
    Vulkan.shaders.vertex = toml::find<std::string>(tomlShaders, "vertex");
}
const AppConfig &Config::getApp() const { return app; }
const VulkanConfig &Config::getVulkan() const { return Vulkan; }