//
// Created by Igor Frank on 02.11.20.
//

#ifndef VULKANAPP_CONFIG_H
#define VULKANAPP_CONFIG_H


#include "ConfigStructs.h"
#include <string>
class Config {
public:
    Config(const std::string &configFile);
    const AppConfig &getApp() const;
    const VulkanConfig &getVulkan() const;

private:
    AppConfig app;
    VulkanConfig Vulkan;
};


#endif//VULKANAPP_CONFIG_H
