//
// Created by Igor Frank on 14.12.20.
//

#ifndef VULKANAPP_STREAMSAVER_H
#define VULKANAPP_STREAMSAVER_H

#include <future>
#include <filesystem>
#include <vector>

#include "SaverTypes.h"

class StreamSaver {
public:
    virtual ~StreamSaver()= default;

    virtual void initStream(std::filesystem::path filename, unsigned int framerate, unsigned int width, unsigned int height) = 0;

    virtual void endStream() = 0;

    virtual void insertFrameBlocking(std::vector<std::byte> data, PixelFormat inputPixelFormat) = 0;

    virtual std::future<void> insertFrameAsync(std::vector<std::byte> data, PixelFormat inputPixelFormat) = 0;

};

#endif//VULKANAPP_STREAMSAVER_H
