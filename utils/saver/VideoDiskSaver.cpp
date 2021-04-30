//
// Created by root on 14.12.20.
//

#include "VideoDiskSaver.h"
#include "SaverTypes.h"
#include <fmt/format.h>

VideoDiskSaver::~VideoDiskSaver() = default;

void VideoDiskSaver::initStream(std::filesystem::path filename, unsigned int framerate,
                                unsigned int width, unsigned int height) {
  if (width > std::numeric_limits<int>::max()) throw std::overflow_error("Width parameter too big");
  if (height > std::numeric_limits<int>::max())
    throw std::overflow_error("Height parameter too big");
  auto validFilename = filename;
  int filecount = 1;
  while (exists(validFilename)) {
    validFilename = fmt::format("{}/{}({}){}", filename.parent_path().string(),
                                filename.stem().string(), filecount, filename.extension().string());
    ++filecount;
  }
  encoder.initEncoder(static_cast<int>(width), static_cast<int>(height), framerate, validFilename);
}

void VideoDiskSaver::endStream() { encoder.endFile(); }

void VideoDiskSaver::insertFrameBlocking(std::vector<std::byte> data,
                                         PixelFormat inputPixelFormat) {
  insertFrameImpl(data, inputPixelFormat);
}

std::future<void> VideoDiskSaver::insertFrameAsync(std::vector<std::byte> data,
                                                   PixelFormat inputPixelFormat) {
  return std::async(std::launch::async, &VideoDiskSaver::insertFrameImpl, this, data,
                    inputPixelFormat);
}

void VideoDiskSaver::insertFrameImpl(const std::vector<std::byte> &data,
                                     PixelFormat inputPixelFormat) {
  encoder.write(data, getAVPixelFormat(inputPixelFormat));
}

AVPixelFormat VideoDiskSaver::getAVPixelFormat(PixelFormat pixelFormat) {
  switch (pixelFormat) {
    case PixelFormat::RGB: return AV_PIX_FMT_RGB24;
    case PixelFormat::RGBA: return AV_PIX_FMT_RGBA;
    case PixelFormat::BGRA: return AV_PIX_FMT_BGRA;
    case PixelFormat::BGR: return AV_PIX_FMT_BGR24;
  }
  return AV_PIX_FMT_MONOBLACK;
}
