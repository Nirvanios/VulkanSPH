//
// Created by Igor Frank on 07.12.20.
//

#include "Encoder.h"
#include "spdlog/spdlog.h"

//TODO error
Encoder::Encoder(int width, int height, const std::string &filePath)
    : outputFile(filePath + ".h264", std::ios::binary),
      formatContext(std::unique_ptr<AVFormatContext, AVFormatContextDeleter>(
          nullptr,
          [](auto*) {})),
      packet(std::unique_ptr<AVPacket, AVPacketDeleter>{
          av_packet_alloc(), [](auto *) { /*av_packet_free(&packetPtr);*/ }}) {

  {
    auto tmpPtr = formatContext.get();
    avformat_alloc_output_context2(&tmpPtr, nullptr, nullptr, filePath.c_str());
    formatContext = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>{tmpPtr, [](auto *){/*TODO Free*/}};
  }

  //auto format = formatContext->oformat;

  //TODO error
  auto codec = std::experimental::observer_ptr<AVCodec>(avcodec_find_encoder(AV_CODEC_ID_H264));

  outputStream.stream = std::unique_ptr<AVStream, AVStreamDeleter>{
      avformat_new_stream(formatContext.get(), nullptr), [](auto *) { /*TODO Free*/ }};
  outputStream.stream->id = formatContext->nb_streams - 1;

  codecContext = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>{
      avcodec_alloc_context3(codec.get()), [](auto *) { /*avcodec_free_context(&contextPtr); */ }};
  avcodec_get_context_defaults3(codecContext.get(), codec.get());

  codecContext->codec_id = codec->id;
  codecContext->width = width;
  codecContext->height = height;
  codecContext->bit_rate = 400000;
  codecContext->time_base = outputStream.stream->time_base = {.num = 1, .den = 25};
  codecContext->gop_size = 12;
  //codecContext->level = 31;
  codecContext->max_b_frames = 1;
  codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

  av_opt_set(codecContext->priv_data, "preset", "slow", 0);

  avcodec_open2(codecContext.get(), codec.get(), nullptr);

  frame = std::unique_ptr<AVFrame, AVFrameDeleter>{av_frame_alloc(), [](auto) {
                                                     //av_freep(&framePtr->data[0]);
                                                     //av_frame_free(&framePtr);
                                                   }};
  frame->format = codecContext->pix_fmt;
  frame->width = codecContext->width;
  frame->height = codecContext->height;
  av_image_alloc(frame->data, frame->linesize, frame->width, frame->height,
                 static_cast<AVPixelFormat>(frame->format), 32);
  frame->pts = 0;

  avcodec_parameters_from_context(outputStream.stream->codecpar, codecContext.get());

  av_dump_format(formatContext.get(), 0, filePath.c_str(), 1);


    auto tmpPtr2 = formatContext.get();
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
      avio_open(&tmpPtr2->pb, filePath.c_str(), AVIO_FLAG_WRITE);
    }

  [[maybe_unused]] auto a = avformat_write_header(formatContext.get(), nullptr);
  av_frame_make_writable(frame.get());


  /*

  formatContext->oformat = format;
  formatContext->url = new char[filePath.length() + 1];
  strcpy(formatContext->url, filePath.c_str());

  std::unique_ptr<AVDictionary, void (*)(AVDictionary *)> formatOptions{
      nullptr, [](auto *dictionaryPtr) { av_dict_free(&dictionaryPtr); }};
  {
    auto tmpPtr = formatOptions.get();
    av_dict_set(&tmpPtr, "movflags", "faststart", 0);
  }

  codecContext->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;

  codecContext->framerate = {.num = 25, .den = 1};


  av_opt_set(codecContext->priv_data, "cfr", "20", 0);
  //av_opt_set(codecContext->priv_data, "profile", "main", 0);
  //av_opt_set(codecContext->priv_data, "b-pyramid", "0", 0);



  av_dump_format(formatContext.get(), 0, filePath.c_str(), 1);

  avio_open(&formatContext->pb, filePath.c_str(), AVIO_FLAG_WRITE);
  {
    auto tmpPtr = formatOptions.get();
    [[maybe_unused]] auto a = avformat_write_header(formatContext.get(), &tmpPtr);
  }


  av_frame_get_buffer(frame.get(), 32);
  av_frame_make_writable(frame.get());*/
}
void Encoder::write(const std::vector<char> &data) {
  const int in_linesize[1] = {4 * frame->width};
  static struct SwsContext *swsContext = NULL;
  swsContext =
      sws_getCachedContext(swsContext, frame->width, frame->height, AV_PIX_FMT_RGBA, frame->width,
                           frame->height, AV_PIX_FMT_YUV420P, 0, NULL, NULL, NULL);
  auto a = data.data();
  sws_scale(swsContext, (const uint8_t *const *) &a, in_linesize, 0, frame->height, frame->data,
            frame->linesize);
  encode();
  ++frame->pts;
}

Encoder::~Encoder() {
  //encode(true);
  //outputFile.write(reinterpret_cast<const char *>(endcode.data()), endcode.size());
  av_write_trailer(formatContext.get());
  avcodec_close(codecContext.get());
  if (!(formatContext->flags & AVFMT_NOFILE))
    avio_closep(&formatContext->pb);
}
void Encoder::encode(bool useNullFrame) {
  int returnValue = 0;
  av_init_packet(packet.get());
  packet->data = nullptr;
  packet->size = 0;

  returnValue = avcodec_send_frame(codecContext.get(), useNullFrame ? nullptr : frame.get());
  if (returnValue < 0) {
    spdlog::error(fmt::format("Error sending frame: {}", av_err2str(returnValue)));
  }

  while (returnValue >= 0) {
    returnValue = avcodec_receive_packet(codecContext.get(), packet.get());
    if (returnValue == AVERROR(EAGAIN) || returnValue == AVERROR_EOF) break;
    else if (returnValue < 0) {
      spdlog::error(fmt::format("Error encoding a frame: {}", av_err2str(returnValue)));
    }
    //outputFile.write(reinterpret_cast<const char *>(packet->data), packet->size);
    av_packet_rescale_ts(packet.get(), codecContext->time_base, outputStream.stream->time_base);
    packet->stream_index = outputStream.stream->index;
    returnValue = av_interleaved_write_frame(formatContext.get(), packet.get());
    if (returnValue < 0) {
      spdlog::error(fmt::format("Error writing frame: {}", av_err2str(returnValue)));
    }
    av_packet_unref(packet.get());
  }
}
