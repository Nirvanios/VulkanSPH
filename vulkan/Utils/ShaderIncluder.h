//
// Created by Igor Frank on 16.11.20.
//

#ifndef VULKANAPP_SHADERINCLUDER_H
#define VULKANAPP_SHADERINCLUDER_H

#include "../../utils/Utilities.h"
#include "shaderc/shaderc.hpp"

class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
 public:
  void addLibrary(const std::string &path) { libraries.emplace_back(path); };
  shaderc_include_result *GetInclude(const char *requested_source, shaderc_include_type,
                                     const char *, size_t) override {
    auto file = Utilities::readFile("/home/kuro/CLionProjects/VulkanApp/shaders/SPH/Kernels.comp");
    includeResult = shaderc_include_result{.source_name = "Kernels.comp",
                                           .source_name_length = strlen("Kernels.comp"),
                                           .content = file.c_str(),
                                           .content_length = file.size(),
                                           .user_data = nullptr};
    return new shaderc_include_result{.source_name = requested_source,
                                      .source_name_length = strlen(requested_source),
                                      .content = file.data(),
                                      .content_length = file.size(),
                                      .user_data = nullptr};
  };
  void ReleaseInclude(shaderc_include_result *) override{};

 private:
  std::vector<std::string> libraries;
  shaderc_include_result includeResult;
};

#endif//VULKANAPP_SHADERINCLUDER_H
