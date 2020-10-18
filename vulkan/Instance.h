//
// Created by Igor Frank on 18.10.20.
//

#ifndef VULKANAPP_INSTANCE_H
#define VULKANAPP_INSTANCE_H

#include <vulkan/vulkan.hpp>

using UniqueDebugUtilsMessengerEXTDynamic = vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic>;

class Instance {
public:
    explicit Instance(const std::string &appName = "", bool debug = false);
    virtual ~Instance();
    [[nodiscard]] const vk::Instance &getInstance() const;
    const std::vector<const char *> &getValidationLayers() const;

private:
    std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
    };

    vk::UniqueInstance instance;
    UniqueDebugUtilsMessengerEXTDynamic debugMessenger;
    std::string appName;
    bool debug;

    void createInstance();
    bool checkValidationLayerSupport();
    [[nodiscard]] std::vector<const char *> getRequiredExtensions() const;
    void setupDebugMessenger();
    static vk::DebugUtilsMessengerCreateInfoEXT getDebugMessengerCreateInfo();

};


#endif//VULKANAPP_INSTANCE_H
