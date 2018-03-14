#ifndef LIVE_VIDEO_STREAM_TRANSCODER_HPP
#define LIVE_VIDEO_STREAM_TRANSCODER_HPP

#include <string>
#include <thread>
#include <mutex>
#include <queue>

#include "Utils.hpp"
#include "Logger.hpp"

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}
#endif

namespace LIRS {

    typedef struct TranscoderContext {
        AVFormatContext *formatContext;
        AVCodecContext *codecContext;
        AVCodec *codec;
        AVStream *videoStream;

        TranscoderContext() : formatContext(nullptr), codecContext(nullptr), codec(nullptr), videoStream(nullptr) {}

    } TranscoderContext;

    class Transcoder {

    public:

        static Transcoder *newInstance(std::string sourceUrl, size_t frameWidth, size_t frameHeight,
                                       std::string rawPixelFormatStr, std::string encoderPixelFormatStr,
                                       size_t frameRate, size_t outputFrameRate, size_t outputBitRate);

        Transcoder(const Transcoder &) = delete;

        Transcoder &operator=(const Transcoder &) = delete;

        ~Transcoder();

        void playVideo();

        bool retrieveFrame(std::vector<uint8_t> &frame);

        void setOnFrameCallback(std::function<void()> callback);

        std::string getDeviceName() const;

    private:

        Transcoder(std::string url, size_t w, size_t h, std::string rawPixFmtStr, std::string encPixFmtStr,
                   size_t frameRate, size_t outFrameRate, size_t outBitRate);

        /* parameters */

        std::string videoSourceUrl;

        size_t frameWidth;
        size_t frameHeight;

        AVPixelFormat rawPixFormat;
        AVPixelFormat encoderPixFormat;

        size_t frameRate;
        size_t outputFrameRate;

        size_t sourceBitRate;
        size_t outputBitRate;

        TranscoderContext decoderContext;
        TranscoderContext encoderContext;

        AVFrame *rawFrame;
        AVFrame *convertedFrame;
        AVPacket *packet;

        SwsContext *converterContext;

        std::mutex outQueueMutex;
        std::queue<std::vector<uint8_t>> outQueue;

        /* Methods */

        void registerAll();

        void initializeDecoder();

        void initializeEncoder();

        int decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet);

        int encode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet);

        void initializeConverter();

        /* Parameters */

        std::function<void()> onFrameCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_TRANSCODER_HPP
