#ifndef LIVE_VIDEO_STREAM_TRANSCODER_HPP
#define LIVE_VIDEO_STREAM_TRANSCODER_HPP

#include <string>
#include <chrono>
#include <thread>

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

    typedef struct EncoderContext {
        AVFormatContext *formatContext = nullptr;
        AVCodecContext *codecContext = nullptr;
        AVCodec *codec = nullptr;
        AVStream *videoStream = nullptr;
    } EncoderContext;

    typedef struct DecoderContext {
        AVFormatContext *formatContext = nullptr;
        AVCodecContext *codecContext = nullptr;
        AVCodec *codec = nullptr;
        AVStream *videoStream = nullptr;
    } DecoderContext;


    class Transcoder {

    public:

        // fixme: decouple from AV frame rate and pixel format (pixdesc utils)
        static Transcoder *newInstance(std::string sourceUrl, size_t frameWidth, size_t frameHeight,
                                       size_t frameRate, AVPixelFormat rawPixelFormat, size_t outputBitRate) {

            return new Transcoder(std::move(sourceUrl), frameWidth, frameHeight, rawPixelFormat, frameRate,
                                  outputBitRate);
        }

        /* prohibit copying */
        Transcoder(const Transcoder &) = delete;

        Transcoder &operator=(const Transcoder &) = delete;

        // destructor
        ~Transcoder() {
            cleanupEncoder();
            cleanupDecoder();
            LOG(INFO) << "Transcoder has been destructed.";
        }

        // decoder/encoder loop

        void playVideo() {

            int counter = 10;

            while (av_read_frame(decoderContext.formatContext, packet) == 0 && counter-- > 0) {

                if (packet->data && packet->stream_index == decoderContext.videoStream->index) {

                    if (decode(decoderContext.codecContext, rawFrame, packet)) {

                        LOG(INFO) << "Got frame: "
                                  << av_image_get_buffer_size(rawPixFormat, (int) frameWidth, (int) frameHeight, 32);
                    }
                }
                av_packet_unref(packet);
            }
        }


    private:

// todo save source bit rate, frame W/H, frame rate, pixel format
        // todo add encoder's pixel format
        Transcoder(std::string sourceUrl, size_t frameWidth, size_t frameHeight, AVPixelFormat rawPixFormat,
                   size_t frameRate, size_t outputBitRate)
                : videoSourceUrl(std::move(sourceUrl)), frameWidth(frameWidth), frameHeight(frameHeight),
                  rawPixFormat(rawPixFormat), frameRate(frameRate),
                  sourceBitRate(0), outputBitRate(outputBitRate) {

            registerAll();

            initializeDecoder();

            initializeEncoder();

            initializeConverter();

            std::thread([&]() {
                playVideo();
            }).join();
        }

        /* parameters */

        std::string videoSourceUrl;

        size_t frameWidth;
        size_t frameHeight;

        AVPixelFormat rawPixFormat;
        AVPixelFormat encoderPixFormat = AV_PIX_FMT_YUV422P;
        size_t frameRate;

        size_t sourceBitRate;
        size_t outputBitRate;

        // decoder
        DecoderContext decoderContext;
        AVFrame *rawFrame = nullptr;
        AVPacket *packet = nullptr;

        // encoder
        EncoderContext encoderContext;
        AVFrame *convertedFrame = nullptr;
        SwsContext *converterContext = nullptr;

        /* Methods */
        void registerAll() {
            av_register_all();
            avdevice_register_all();
            avcodec_register_all();
        }

        // Decoder methods

        void initializeDecoder() {

            // holds the general (header) information about the format (container)
            decoderContext.formatContext = avformat_alloc_context();
            auto inputFormat = av_find_input_format("v4l2");

            auto frameResolution = utils::concatParams({frameWidth, frameHeight}, "x");

            AVDictionary *options = nullptr;
            av_dict_set(&options, "video_size", frameResolution.data(), 0);
            av_dict_set(&options, "pixel_format", av_get_pix_fmt_name(rawPixFormat), 0);
            av_dict_set_int(&options, "framerate", frameRate, 0);

            int statCode = avformat_open_input(&decoderContext.formatContext, videoSourceUrl.data(), inputFormat,
                                               &options);
            av_dict_free(&options);
            assert(statCode == 0);

            statCode = avformat_find_stream_info(decoderContext.formatContext, nullptr);
            assert(statCode >= 0);

            av_dump_format(decoderContext.formatContext, 0, videoSourceUrl.data(), 0);

            // find video stream
            auto videoStreamIndex = av_find_best_stream(decoderContext.formatContext, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                        &decoderContext.codec, 0);
            assert(videoStreamIndex >= 0);
            assert(decoderContext.codec);
            decoderContext.videoStream = decoderContext.formatContext->streams[videoStreamIndex];

            // create codec context (for each codec its own codec context)
            decoderContext.codecContext = avcodec_alloc_context3(decoderContext.codec);
            assert(decoderContext.codecContext);
            statCode = avcodec_parameters_to_context(decoderContext.codecContext, decoderContext.videoStream->codecpar);
            assert(statCode >= 0);

            // initialize the codec context to use the created codec context
            statCode = avcodec_open2(decoderContext.codecContext, decoderContext.codec, &options);
            assert(statCode == 0);

            // save info (in case of change)
            frameRate = static_cast<size_t>(decoderContext.videoStream->r_frame_rate.num /
                                            decoderContext.videoStream->r_frame_rate.den);
            rawPixFormat = decoderContext.codecContext->pix_fmt;
            frameWidth = static_cast<size_t>(decoderContext.codecContext->width);
            frameHeight = static_cast<size_t>(decoderContext.codecContext->height);
            sourceBitRate = static_cast<size_t>(decoderContext.codecContext->bit_rate);

            // init packet and frame (decoder)
            packet = av_packet_alloc();
            av_init_packet(packet);

            rawFrame = av_frame_alloc();
        }

        void cleanupDecoder() {
            av_packet_free(&packet);
            av_frame_free(&rawFrame);
            avcodec_free_context(&decoderContext.codecContext);
            avformat_close_input(&decoderContext.formatContext);
            avformat_free_context(decoderContext.formatContext);
            decoderContext = {};
            packet = nullptr;
            rawFrame = nullptr;
        }

        bool decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

            auto statCode = avcodec_send_packet(codecContext, packet);

            if (statCode < 0) {
                LOG(ERROR) << "Error sending a packet for decoding";
                return false;
            }

            statCode = avcodec_receive_frame(codecContext, frame);

            if (statCode == AVERROR(EAGAIN) || statCode == AVERROR_EOF) {
                LOG(WARN) << "No frames available or end of file has been reached";
                return false;
            }

            if (statCode < 0) {
                LOG(ERROR) << "Error during decoding";
                return false;
            }

            return true;
        }

        // End decoder methods

        void initializeEncoder() {

            // allocate format context for an output format (no output file)
            auto statCode = avformat_alloc_output_context2(&encoderContext.formatContext, nullptr, "null", nullptr);
            assert(statCode >= 0);

            encoderContext.codec = avcodec_find_encoder_by_name("libx264"); // fixme: hardcoded
            assert(encoderContext.codec);

            // create new video output stream
            encoderContext.videoStream = avformat_new_stream(encoderContext.formatContext, encoderContext.codec);
            assert(encoderContext.videoStream);
            encoderContext.videoStream->id = encoderContext.formatContext->nb_streams - 1;

            // create codec context (for each codec new codec context)
            encoderContext.codecContext = avcodec_alloc_context3(encoderContext.codec);
            assert(encoderContext.codecContext);

            // set up parameters
            // todo: add keyint for frame rate regulation
            encoderContext.codecContext->width = static_cast<int>(frameWidth);
            encoderContext.codecContext->height = static_cast<int>(frameHeight);
            encoderContext.codecContext->bit_rate = outputBitRate;
            encoderContext.codecContext->keyint_min = 5;
            encoderContext.codecContext->framerate = (AVRational) {static_cast<int>(frameRate), 1};
            encoderContext.codecContext->time_base = (AVRational) {1, static_cast<int>(frameRate)};
            encoderContext.codecContext->profile = FF_PROFILE_H264_HIGH_422_INTRA;
            encoderContext.codecContext->pix_fmt = encoderPixFormat;

            if (encoderContext.formatContext->flags & AVFMT_GLOBALHEADER) {
                encoderContext.codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            avcodec_parameters_from_context(encoderContext.videoStream->codecpar, encoderContext.codecContext);

            AVDictionary *options = nullptr;
            av_dict_set(&options, "preset", "fast", 0);
            av_dict_set(&options, "tune", "zerolatency", 0);

            // open the output format to use given codec
            statCode = avcodec_open2(encoderContext.codecContext, encoderContext.codec, &options);
            av_dict_free(&options);
            assert(statCode == 0);

            statCode = avformat_write_header(encoderContext.formatContext, nullptr);
            assert(statCode >= 0);

            av_dump_format(encoderContext.formatContext, encoderContext.videoStream->index, videoSourceUrl.data(), 1);
        }

        void cleanupEncoder() {

        }

        bool encode() {

            auto statCode = 1UL;

            return true;
        }

        void initializeConverter() {

            convertedFrame = av_frame_alloc();
            convertedFrame->width = static_cast<int>(frameWidth);
            convertedFrame->height = static_cast<int>(frameRate);
            convertedFrame->format = encoderPixFormat;
            auto statCode = av_frame_get_buffer(convertedFrame, 0);
            assert(statCode == 0);

            converterContext = sws_getCachedContext(nullptr,
                                                    static_cast<int>(frameWidth), static_cast<int>(frameHeight),
                                                    rawPixFormat,
                                                    static_cast<int>(frameWidth), static_cast<int>(frameHeight),
                                                    encoderPixFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);

        };

    }

#endif //LIVE_VIDEO_STREAM_TRANSCODER_HPP
