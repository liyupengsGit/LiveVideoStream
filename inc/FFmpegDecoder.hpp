#ifndef LIVE_VIDEO_STREAM_FFMPEG_DECODER_HPP
#define LIVE_VIDEO_STREAM_FFMPEG_DECODER_HPP

#include "Decoder.hpp" // abstract base class for decoders
#include "Utils.hpp"
#include "Logger.hpp"

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}
#endif

namespace LIRS {

    /**
     * FFmpeg based decoder.
     */
    class FFmpegDecoder : public Decoder {
    public:

        FFmpegDecoder(std::string videoSourcePath, const DecoderParams& params);

        void decodeLoop() override;

        ~FFmpegDecoder() override;

    private:

        AVFormatContext *formatContext;
        AVCodecContext *codecContext;
        AVCodec *codec;

        AVStream *videoStream;
        int videoStreamIndex;

        AVDictionary *options;

        /**
         * Initializes ffmpeg environment registering all available codecs,
         * formats, devices, etc.
         */
        void registerAll();

        /**
         * Retrieves frame with a raw image data.
         *
         * @param codecContext codec context.
         * @param frame raw image frame which is to be populated with data.
         * @param packet packet to be sent to the decoder.
         * @return >= 0 - if success, otherwise ffmpeg error code (AVERROR).
         */
        int decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet);

    };
}

#endif //LIVE_VIDEO_STREAM_FFMPEG_DECODER_HPP
