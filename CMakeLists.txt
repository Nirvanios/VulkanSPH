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

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION} AND CPM_VERSION STREQUAL CPM_DOWNLOAD_VERSION))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})


project(VulkanApp)

set(CMAKE_CXX_STANDARD 20)
list(APPEND flags "-fconcepts" "-fconcepts-diagnostics-depth=10")
list(APPEND flags "-Werror=return-type")
list(APPEND flags "-Wall" "-Wextra" "-Werror" "-Wpedantic" "-Wno-unknown-pragmas")
list(APPEND flags "-Wno-unused-function")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fconcepts -fconcepts-diagnostics-depth=10")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic -Wno-unknown-pragmas")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()

set(SHADERC_SKIP_TESTS ON)
set(SPIRV_CROSS_ENABLE_TESTS OFF)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
hunter_add_package(glm)
find_package(glm CONFIG REQUIRED)
hunter_add_package(imgui)
find_package(imgui CONFIG REQUIRED)
hunter_add_package(fmt)
find_package(fmt CONFIG REQUIRED)
hunter_add_package(spdlog)
find_package(spdlog CONFIG REQUIRED)

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



add_compile_definitions(SPDLOG_FMT_EXTERNAL
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
        VULKAN_HPP_NO_NODISCARD_WARNINGS
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        )

add_executable(VulkanApp main.cpp
        Utilities.h
        vulkan/VulkanCore.cpp vulkan/VulkanCore.h
        window/GlfwWindow.cpp window/GlfwWindow.h vulkan/Instance.cpp vulkan/Instance.h vulkan/Device.cpp vulkan/Device.h vulkan/Swapchain.cpp vulkan/Swapchain.h vulkan/Pipeline.cpp vulkan/Pipeline.h vulkan/Framebuffers.cpp vulkan/Framebuffers.h window/Callbacks.h vulkan/Types.h vulkan/builders/BufferBuilder.cpp vulkan/builders/BufferBuilder.h vulkan/Buffer.cpp vulkan/Buffer.h vulkan/VulkanUtils.h)
target_link_libraries(VulkanApp
        ${Vulkan_LIBRARIES} glfw spdlog::spdlog imgui::imgui fmt::fmt shaderc glslang)

target_compile_options(VulkanApp PRIVATE ${flags})