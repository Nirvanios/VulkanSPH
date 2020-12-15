//
// Created by root on 14.12.20.
//

#ifndef VULKANAPP_SCREENSHOTDISKSAVER_H
#define VULKANAPP_SCREENSHOTDISKSAVER_H


#include <vector>
#include <future>
#include "ImageSaver.h"

class ScreenshotDiskSaver : public ImageSaver {
public:
    ~ScreenshotDiskSaver() override;

    void saveImageBlocking(std::filesystem::path fileName, FilenameFormat filenameFormat, PixelFormat pixelFormat,
                           ImageFormat imageFormat, unsigned int width, unsigned int height,
                           std::vector<std::byte> data) override;

    std::future<void>
    saveImageAsync(const std::filesystem::path &fileName, FilenameFormat filenameFormat, PixelFormat pixelFormat,
                   ImageFormat imageFormat, unsigned int width, unsigned int height,
                   const std::vector<std::byte> &data) override;

private:
    void saveImageImpl(const std::filesystem::path &filename, FilenameFormat filenameFormat, PixelFormat pixelFormat,
                       ImageFormat imageFormat, unsigned int width, unsigned int height,
                       const std::vector<std::byte> &data);
};


#endif //VULKANAPP_SCREENSHOTDISKSAVER_H
