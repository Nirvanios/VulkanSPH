cmake_minimum_required(VERSION 3.17)

include("cmake/HunterGate.cmake")

HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.269.tar.gz"
        SHA1 "64024b7b95b4c86d50ae05b926814448c93a70a0"
)

set(CPM_SOURCE_CACHE ${CMAKE_SOURCE_DIR}/.cache)
include(FetchContent)

set(CPM_DOWNLOAD_LOCATION "${CMAKE_SOURCE_DIR}/cmake/CPM.cmake")
set(CPM_DOWNLOAD_VERSION 0.27.4)
set(VK_LAYER_ENABLES VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)

if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION} AND CPM_VERSION STREQUAL CPM_DOWNLOAD_VERSION))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif ()

include(${CPM_DOWNLOAD_LOCATION})


project(VulkanApp)

set(CMAKE_CXX_STANDARD 20)
list(APPEND flags "-fconcepts" "-fconcepts-diagnostics-depth=10")
list(APPEND flags "-Werror=return-type")
list(APPEND flags "-Wall" "-Wextra" "-Werror" "-Wpedantic" "-Wno-unknown-pragmas")
list(APPEND flags "-Wno-unused-function")


#find_program(CCACHE_PROGRAM ccache)
#[[
if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()
]]

set(RANGE_V3_TESTS OFF)
set(SHADERC_SKIP_TESTS ON)
set(SPIRV_CROSS_ENABLE_TESTS OFF)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

find_library(AVCODEC_LIBRARY avcodec)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
hunter_add_package(glm)
find_package(glm CONFIG REQUIRED)
hunter_add_package(fmt)
find_package(fmt CONFIG REQUIRED)
hunter_add_package(spdlog)
find_package(spdlog CONFIG REQUIRED)
hunter_add_package(stb)
find_package(stb CONFIG REQUIRED)

CPMAddPackage(
        NAME glslang
        GITHUB_REPOSITORY KhronosGroup/glslang
        GIT_TAG master-tot
)
CPMAddPackage(
        NAME spirvheaders
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Headers
        GIT_TAG 1.5.3.reservations1
)
CPMAddPackage(
        NAME spirvtools
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Tools
        GIT_TAG v2020.4
)
CPMAddPackage(
        NAME spirvcross
        GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
        GIT_TAG 2020-09-17
)
CPMAddPackage(
        NAME shaderc
        GITHUB_REPOSITORY google/shaderc
        GIT_TAG v2020.3
)
CPMAddPackage(
        NAME toml11
        GITHUB_REPOSITORY ToruNiina/toml11
        GIT_TAG v3.6.0
)
CPMAddPackage(
        NAME range-v3
        GITHUB_REPOSITORY ericniebler/range-v3
        GIT_TAG 0.11.0
)
CPMAddPackage(
        NAME tinyobjloader
        GITHUB_REPOSITORY tinyobjloader/tinyobjloader
        GIT_TAG v2.0.0rc7
)
CPMAddPackage(
        NAME pf_imgui
        GITHUB_REPOSITORY PetrFlajsingr/pf_imgui
        GIT_TAG 0.1
)

include_directories("Third Party/")

add_compile_definitions(
        STB_IMAGE_IMPLEMENTATION
        STB_IMAGE_WRITE_IMPLEMENTATION
        SPDLOG_FMT_EXTERNAL
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_NO_NODISCARD_WARNINGS
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_SWIZZLE
        ANKERL_NANOBENCH_IMPLEMENT
)

# doporucuji si udelat seznam cpp a h souboru, pak je to tu prehlednejsi, mrkni na muj projekt jak to tam mam
add_executable(VulkanApp main.cpp
        utils/Utilities.h
        vulkan/VulkanCore.cpp vulkan/VulkanCore.h
        window/GlfwWindow.cpp window/GlfwWindow.h vulkan/types/Instance.cpp vulkan/types/Instance.h
        vulkan/types/Device.cpp vulkan/types/Device.h vulkan/types/Swapchain.cpp vulkan/types/Swapchain.h
        vulkan/types/Pipeline.cpp vulkan/types/Pipeline.h vulkan/types/Framebuffers.cpp vulkan/types/Framebuffers.h
        window/Callbacks.h vulkan/types/Types.h vulkan/builders/BufferBuilder.cpp vulkan/builders/BufferBuilder.h
        vulkan/types/Buffer.cpp vulkan/types/Buffer.h vulkan/Utils/VulkanUtils.h window/EventDispatchingWindow.cpp
        window/EventDispatchingWindow.h window/Messages.h Renderers/TestRenderer.cpp Renderers/TestRenderer.h
        utils/Config.cpp utils/Config.h utils/ConfigStructs.h vulkan/builders/PipelineBuilder.cpp
        vulkan/builders/PipelineBuilder.h vulkan/types/DescriptorSet.cpp vulkan/types/DescriptorSet.h
        vulkan/types/Image.cpp vulkan/types/Image.h vulkan/builders/ImageBuilder.cpp vulkan/builders/ImageBuilder.h
        vulkan/Utils/ShaderIncluder.h utils/Encoder.cpp utils/Encoder.h utils/Capture.cpp utils/Capture.h
        utils/saver/ImageSaver.h utils/saver/StreamSaver.h utils/saver/ImageStreamSaver.h utils/saver/VideoDiskSaver.cpp
        utils/saver/VideoDiskSaver.h utils/saver/ScreenshotDiskSaver.cpp utils/saver/ScreenshotDiskSaver.h
        utils/saver/SaverTypes.h vulkan/VulkanSort.cpp vulkan/VulkanSort.h vulkan/VulkanSPH.cpp vulkan/VulkanSPH.h
        vulkan/VulkanGridSPH.cpp vulkan/VulkanGridSPH.h ui/ImGuiGlfwVulkan.cpp ui/ImGuiGlfwVulkan.h
        Third\ Party/imgui/imgui_impl_glfw.cpp Third\ Party/imgui/imgui_impl_vulkan.cpp utils/FPSCounter.cpp utils/FPSCounter.h
        vulkan/types/RenderPass.cpp vulkan/types/RenderPass.h vulkan/builders/RenderPassBuilder.cpp vulkan/builders/RenderPassBuilder.h)


target_link_libraries(VulkanApp PUBLIC
        -lbfd -ldl
        ${Vulkan_LIBRARIES} glfw spdlog::spdlog fmt::fmt stb::stb
        shaderc_combined glslang toml11 range-v3 tinyobjloader ${AVCODEC_LIBRARY} avutil avformat swscale
        pf_imgui::pf_imgui pf_common::pf_common)
add_backward(VulkanApp)
target_compile_options(VulkanApp PRIVATE ${flags})