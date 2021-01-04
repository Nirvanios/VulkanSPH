//
// Created by Igor Frank on 02.11.20.
//

#ifndef VULKANAPP_CONFIG_H
#define VULKANAPP_CONFIG_H

#include "ConfigStructs.h"
#include <string>
class Config {
 public:
  explicit Config(const std::string &configFile);
  [[nodiscard]] const AppConfig &getApp() const;
  [[nodiscard]] const VulkanConfig &getVulkan() const;

 private:
  AppConfig app;
  VulkanConfig Vulkan;
};

#endif//VULKANAPP_CONFIG_H
