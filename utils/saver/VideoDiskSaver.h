//
// Created by root on 14.12.20.
//

#ifndef VULKANAPP_VIDEODISKSAVER_H
#define VULKANAPP_VIDEODISKSAVER_H


#include "StreamSaver.h"
#include "../Encoder.h"

class VideoDiskSaver : public StreamSaver {
public:
    ~VideoDiskSaver() override;

    void initStream(std::filesystem::path filename, unsigned int framerate, unsigned int width, unsigned int height) override;

    void endStream() override;

    void insertFrameBlocking(std::vector<std::byte> data, PixelFormat inputPixelFormat) override;

    std::future<void> insertFrameAsync(std::vector<std::byte> data, PixelFormat inputPixelFormat) override;

private:
    Encoder encoder;
    void insertFrameImpl(const std::vector<std::byte> &data, PixelFormat inputPixelFormat);
    AVPixelFormat getAVPixelFormat(PixelFormat pixelFormat);
};


#endif //VULKANAPP_VIDEODISKSAVER_H
