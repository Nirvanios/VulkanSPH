//
// Created by root on 14.12.20.
//

#include "ScreenshotDiskSaver.h"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <glm/detail/type_vec1.hpp>
#include <glm/glm.hpp>
#include <range/v3/view.hpp>
#include <stb/stb_image_write.h>

void ScreenshotDiskSaver::saveImageBlocking(std::filesystem::path fileName,
                                            FilenameFormat filenameFormat, PixelFormat pixelFormat,
                                            ImageFormat imageFormat, unsigned int width,
                                            unsigned int height, std::vector<std::byte> data) {
  saveImageImpl(fileName, filenameFormat, pixelFormat, imageFormat, width, height, data);
}

std::future<void> ScreenshotDiskSaver::saveImageAsync(const std::filesystem::path &fileName,
                                                      FilenameFormat filenameFormat,
                                                      PixelFormat pixelFormat,
                                                      ImageFormat imageFormat, unsigned int width,
                                                      unsigned int height,
                                                      const std::vector<std::byte> &data) {
  return std::async(std::launch::async, &ScreenshotDiskSaver::saveImageImpl, this, fileName,
                    filenameFormat, pixelFormat, imageFormat, width, height, data);
}

void ScreenshotDiskSaver::saveImageImpl(const std::filesystem::path &filename,
                                        FilenameFormat filenameFormat, PixelFormat pixelFormat,
                                        ImageFormat imageFormat, unsigned int width,
                                        unsigned int height, const std::vector<std::byte> &data) {
  if (width > std::numeric_limits<int>::max()) throw std::overflow_error("Width parameter too big");
  if (height > std::numeric_limits<int>::max())
    throw std::overflow_error("Height parameter too big");

  auto pictureData = &data;
  auto rgbData = std::vector<std::byte>{};

  if (pixelFormat == PixelFormat::BGRA) {
    rgbData = data | ranges::views::chunk(4) | ranges::views::transform([](auto a4Bytes) {
                return std::vector<std::byte>{a4Bytes[2], a4Bytes[1], a4Bytes[0], a4Bytes[3]};
              })
        | ranges::views::cache1 | ranges::views::join | ranges::to_vector;
    pictureData = &rgbData;
  }

  auto editedFilename = filename;
  auto component = getComponentCount(pixelFormat);

  switch (filenameFormat) {
    case FilenameFormat::None: break;
    case FilenameFormat::WithDateTime:
      editedFilename = editedFilename.replace_filename(
          fmt::format("{}_{:%y-%m-%d[%H:%M:%S]}{}",//editedFilename.parent_path().string(),
                      editedFilename.stem().string(), fmt::localtime(std::time(nullptr)),
                      editedFilename.extension().string()));
      break;
  }

  switch (imageFormat) {
    case ImageFormat::PNG:
      stbi_write_png(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height),
                     component, pictureData->data(), static_cast<int>(width * component));
      break;
    case ImageFormat::JPEG:
      stbi_write_jpg(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height),
                     component, pictureData->data(), 100);
      break;
    case ImageFormat::BMP:
      stbi_write_bmp(editedFilename.c_str(), static_cast<int>(width), static_cast<int>(height),
                     component, pictureData->data());
      break;
  }
}

ScreenshotDiskSaver::~ScreenshotDiskSaver() = default;
