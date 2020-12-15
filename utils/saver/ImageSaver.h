//
// Created by Igor Frank on 14.12.20.
//

#ifndef VULKANAPP_IMAGESAVER_H
#define VULKANAPP_IMAGESAVER_H

#include <filesystem>

#include "SaverTypes.h"

class ImageSaver {
public:
    virtual ~ImageSaver() = default;

    virtual void
    saveImageBlocking(std::filesystem::path fileName, FilenameFormat filenameFormat, PixelFormat pixelFormat,
                      ImageFormat imageFormat, unsigned int width, unsigned int height,
                      std::vector<std::byte> data) = 0;

    virtual std::future<void>
    saveImageAsync(const std::filesystem::path &fileName, FilenameFormat filenameFormat, PixelFormat pixelFormat,
                   ImageFormat imageFormat, unsigned int width, unsigned int height,
                   const std::vector<std::byte> &data) = 0;
};

#endif//VULKANAPP_IMAGESAVER_H
