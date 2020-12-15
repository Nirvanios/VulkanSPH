//
// Created by root on 14.12.20.
//

#include "ScreenshotDiskSaver.h"

#include <stb/stb_image_write.h>
#include <fmt/format.h>

void ScreenshotDiskSaver::saveImageBlocking(std::filesystem::path fileName, FilenameFormat filenameFormat,
                                            PixelFormat pixelFormat, ImageFormat imageFormat, unsigned int width,
                                            unsigned int height,
                                            std::vector<std::byte> data) {
    saveImageImpl(fileName, filenameFormat, pixelFormat, imageFormat, width, height, data);
}

std::future<void>
ScreenshotDiskSaver::saveImageAsync(const std::filesystem::path &fileName, FilenameFormat filenameFormat,
                                    PixelFormat pixelFormat, ImageFormat imageFormat, unsigned int width,
                                    unsigned int height,
                                    const std::vector<std::byte> &data) {
    return std::async(std::launch::async, &ScreenshotDiskSaver::saveImageImpl, this, fileName, filenameFormat,
                      pixelFormat,
                      imageFormat, width, height, data);
}

void
ScreenshotDiskSaver::saveImageImpl(const std::filesystem::path &filename, FilenameFormat filenameFormat,
                                   PixelFormat pixelFormat,
                                   ImageFormat imageFormat, unsigned int width, unsigned int height,
                                   const std::vector<std::byte> &data) {
    if (width > std::numeric_limits<int>::max()) throw std::overflow_error("Width parameter too big");
    if (height > std::numeric_limits<int>::max()) throw std::overflow_error("Height parameter too big");

    auto editedFilename = filename;
    auto component = getComponentCount(pixelFormat);

    switch (filenameFormat) {
        case FilenameFormat::None:
            break;
        case FilenameFormat::WithDateTime:
            editedFilename = editedFilename.replace_filename(
                    fmt::format("{}_{}.{}", editedFilename.filename().c_str(), /*std::chrono::system_clock::now()*/"aaa",
                                editedFilename.extension().c_str()));
            break;
    }

    switch (imageFormat) {
        case ImageFormat::PNG:
            stbi_write_png(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height), component,
                           data.data(), static_cast<int>(width * component));
            break;
        case ImageFormat::JPEG:
            stbi_write_jpg(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height), component,
                           data.data(), 100);
            break;
        case ImageFormat::BMP:
            stbi_write_bmp(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height), component,
                           data.data());
            break;
    }
}

ScreenshotDiskSaver::~ScreenshotDiskSaver() = default;
