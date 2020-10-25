//
// Created by Igor Frank on 18.10.20.
//

#include "Instance.h"
#include "../Utilities.h"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        [[maybe_unused]] void *pUserData) {

    auto msgType = "";
    switch (messageType) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            msgType = "General";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            msgType = "Performance";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            msgType = "Validation";
            break;
        default:
            break;
    }
    auto msg = fmt::format("[Validation layer] [{}]: {}", msgType, pCallbackData->pMessage);

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            //spdlog::info(msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn(msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error(msg);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
            break;
    }

    return VK_FALSE;
}

Instance::Instance(const std::string &appName, bool debug) : appName(appName), debug(debug) {
    createInstance();
    spdlog::debug("Created vulkan instance.");
    setupDebugMessenger();
    spdlog::debug("Created debug messenger.");
}

void Instance::createInstance() {
    if (debug and !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    auto extensions = getRequiredExtensions();
    const vk::ApplicationInfo appInfo{.pApplicationName = appName.c_str(),
                                      .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
                                      .pEngineName = "No Engine",
                                      .engineVersion = VK_MAKE_VERSION(0, 0, 0),
                                      .apiVersion = VK_API_VERSION_1_1};
    vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
                                      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                      .ppEnabledExtensionNames = extensions.data()};
    auto debugCreateInfo = getDebugMessengerCreateInfo();
    if (debug) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }
    instance = vk::createInstanceUnique(createInfo);
}

bool Instance::checkValidationLayerSupport() {
    auto availableLayers = vk::enumerateInstanceLayerProperties();

    std::vector<std::string> layerNames(validationLayers.size());
    std::transform(validationLayers.begin(), validationLayers.end(), layerNames.begin(),
                   [](const char *const c) { return std::string{c}; });

    return std::any_of(availableLayers.begin(), availableLayers.end(), [layerNames](const auto &layerProperties) {
        return Utilities::isIn(std::string(layerProperties.layerName), layerNames);
    });
}

std::vector<const char *> Instance::getRequiredExtensions() const {
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (debug) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}
void Instance::setupDebugMessenger() {
    if (debug) {
        auto createInfo = getDebugMessengerCreateInfo();
        debugMessenger = instance->createDebugUtilsMessengerEXTUnique(createInfo, nullptr, vk::DispatchLoaderDynamic{instance.get(), vkGetInstanceProcAddr});
    }
}

vk::DebugUtilsMessengerCreateInfoEXT Instance::getDebugMessengerCreateInfo() {
    return vk::DebugUtilsMessengerCreateInfoEXT{
            .messageSeverity = /*vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |*/ vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr};
}
const vk::Instance &Instance::getInstance() const {
    return instance.get();
}
Instance::~Instance() {
    debugMessenger.release();
    instance.release();
}
const std::vector<const char *> &Instance::getValidationLayers() const {
    return validationLayers;
}
