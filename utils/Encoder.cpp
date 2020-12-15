//
// Created by Igor Frank on 07.12.20.
//

#include "Encoder.h"
#include "spdlog/spdlog.h"

//TODO error
Encoder::Encoder()
        : formatContext(std::unique_ptr<AVFormatContext, AVFormatContextDeleter>(
        nullptr,
        [](auto *) {})),
          packet(std::unique_ptr<AVPacket, AVPacketDeleter>{
                  av_packet_alloc(), [](auto *) { /*av_packet_free(&packetPtr);*/ }}),
          outputStream(std::unique_ptr<AVStream, AVStreamDeleter>{nullptr,
                                                                  [](auto *) {
                                                                      /*av_freep(&streamPtr);*/
                                                                  }}) {}

void Encoder::write(const std::vector<std::byte> &data, AVPixelFormat inputPixelFormat) {
    const int in_linesize[1] = {4 * frame->width};
    static struct SwsContext *swsContext = nullptr;
    swsContext =
            sws_getCachedContext(swsContext, frame->width, frame->height, inputPixelFormat, frame->width,
                                 frame->height, AV_PIX_FMT_YUV420P, 0, nullptr, nullptr, nullptr);
    auto a = data.data();
    sws_scale(swsContext, (const uint8_t *const *) &a, in_linesize, 0, frame->height, frame->data,
              frame->linesize);
    encode();
    ++frame->pts;
}

Encoder::~Encoder() = default;

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
        av_packet_rescale_ts(packet.get(), codecContext->time_base, outputStream->time_base);
        packet->stream_index = outputStream->index;
        returnValue = av_interleaved_write_frame(formatContext.get(), packet.get());
        if (returnValue < 0) {
            spdlog::error(fmt::format("Error writing frame: {}", av_err2str(returnValue)));
        }
        av_packet_unref(packet.get());
    }
}

void Encoder::initEncoder(int width, int height, unsigned int framerate, const std::string &filePath) {
    {
        auto tmpPtr = formatContext.get();
        avformat_alloc_output_context2(&tmpPtr, nullptr, nullptr, filePath.c_str());
        formatContext = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>{tmpPtr, [](auto *) {/*TODO Free*/}};
    }

    //TODO error
    auto codec = std::experimental::observer_ptr<AVCodec>(avcodec_find_encoder(AV_CODEC_ID_H264));

    outputStream = std::unique_ptr<AVStream, AVStreamDeleter>{
            avformat_new_stream(formatContext.get(), nullptr), [](auto *) { /*TODO Free*/ }};
    outputStream->id = static_cast<int>(formatContext->nb_streams - 1);

    codecContext = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>{
            avcodec_alloc_context3(codec.get()), [](auto *) { /*avcodec_free_context(&contextPtr); */ }};
    avcodec_get_context_defaults3(codecContext.get(), codec.get());

    codecContext->codec_id = codec->id;
    codecContext->width = width;
    codecContext->height = height;
    codecContext->bit_rate = 16000*framerate;
    codecContext->time_base = outputStream->time_base = {.num = 1, .den = static_cast<int>(framerate)};
    codecContext->gop_size = 12;
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

    avcodec_parameters_from_context(outputStream->codecpar, codecContext.get());

    av_dump_format(formatContext.get(), 0, filePath.c_str(), 1);


    auto tmpPtr2 = formatContext.get();
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_open(&tmpPtr2->pb, filePath.c_str(), AVIO_FLAG_WRITE);
    }

    [[maybe_unused]] auto a = avformat_write_header(formatContext.get(), nullptr);
    av_frame_make_writable(frame.get());
}

void Encoder::endFile() {
    av_write_trailer(formatContext.get());
    avcodec_close(codecContext.get());
    if (!(formatContext->flags & AVFMT_NOFILE))
        avio_closep(&formatContext->pb);
}
