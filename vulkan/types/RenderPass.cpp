//
// Created by Igor Frank on 23.01.21.
//

#include "RenderPass.h"
RenderPass::RenderPass(vk::UniqueRenderPass renderPass) : renderPass(std::move(renderPass)) {}
