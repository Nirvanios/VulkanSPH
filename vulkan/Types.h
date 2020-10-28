//
// Created by Igor Frank on 25.10.20.
//

#ifndef VULKANAPP_TYPES_H
#define VULKANAPP_TYPES_H

#include "glm/glm.hpp"
#include <vulkan/vulkan.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {.binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attribute{vk::VertexInputAttributeDescription{.location = 0,
                                                                                                         .binding = 0,
                                                                                                         .format = vk::Format::eR32G32B32Sfloat,
                                                                                                         .offset = offsetof(Vertex, pos)},
                                                                     vk::VertexInputAttributeDescription{.location = 1,
                                                                                                         .binding = 0,
                                                                                                         .format = vk::Format::eR32G32B32Sfloat,
                                                                                                         .offset = offsetof(Vertex, color)}};
        return attribute;
    }
};

#endif//VULKANAPP_TYPES_H
