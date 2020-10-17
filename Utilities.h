//
// Created by Igor Frank on 07.10.19.
//

#ifndef VULKANTEST_UTILITIES_H
#define VULKANTEST_UTILITIES_H

#include <fstream>
#include <string>
#include <vector>

class Utilities {
public:
    static std::string readFile(const std::string &filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::string buffer(fileSize, ' ');

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    };

    template<typename T, typename Container = std::vector<T>>
    static bool isIn(T value, Container &&container) {
        return std::any_of(container.begin(), container.end(), [value](const auto &a) { return value == a; });
    }
};


#endif//VULKANTEST_UTILITIES_H
