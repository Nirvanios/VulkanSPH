//
// Created by Igor Frank on 07.12.20.
//

#ifndef VULKANAPP_ENCODER_H
#define VULKANAPP_ENCODER_H

#include <experimental/memory>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>//for av_image_alloc only
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}
#undef av_err2str
#define av_err2str(errnum) \
av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)



//TODO Memery leaks
class Encoder {
  using AVFormatContextDeleter = void (*)(AVFormatContext *);
  using AVCodecContextDeleter = void (*)(AVCodecContext *);
  using AVFrameDeleter = void (*)(AVFrame *);
  using AVPacketDeleter = void (*)(AVPacket *);
  using AVStreamDeleter = void (*)(AVStream *);

 public:
  Encoder();
  virtual ~Encoder();

  void initEncoder(int width, int height, unsigned int framerate, const std::string &filePath);
  void write(const std::vector<std::byte> &data, AVPixelFormat inputPixelFormat);
  void endFile();

 private:
  std::array<uint8_t, 4>endcode{ 0, 0, 1, 0xb7 };
  std::unique_ptr<AVFormatContext, AVFormatContextDeleter> formatContext;
  std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codecContext{nullptr, [](auto) {}};
  std::unique_ptr<AVFrame, AVFrameDeleter> frame{nullptr, [](auto) {}};
  std::unique_ptr<AVPacket, AVPacketDeleter> packet;
  std::unique_ptr<AVStream, AVStreamDeleter> outputStream;



  void encode(bool useNullFrame = false);
};

#endif//VULKANAPP_ENCODER_H
