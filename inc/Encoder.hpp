#ifndef LIVE_VIDEO_STREAM_ENCODER_HPP
#define LIVE_VIDEO_STREAM_ENCODER_HPP

#include <vector>
#include <queue>
#include <mutex>
#include <functional>

#include "Logger.hpp"

#ifdef __cplusplus
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

/**
 * Use AVParser* to split data into frames.
 * Should I set bitrate manually?
 * Create own AVIOContext in order to not to use file? AVIOContext
 *
 */
namespace LIRS {

    class Encoder {
    public:

        Encoder() {

            av_register_all();
            avcodec_register_all();

            int statusCode;

            // allocate format context for an output format (using no output file)
            statusCode = avformat_alloc_output_context2(&formatContext, nullptr, "null", nullptr);
            assert(statusCode >= 0);

            codec = avcodec_find_encoder_by_name("libx264");
            assert(codec);

            // create new video output stream
            stream = avformat_new_stream(formatContext, codec);
            assert(stream);
            stream->id = formatContext->nb_streams - 1; // fixme: is this necessary?
            videoStreamIndex = stream->index;

            // create codec context
            codecContext = avcodec_alloc_context3(codec);
            assert(codecContext);

            // set up codec context
            codecContext->width = frameWidth;
            codecContext->height = frameHeight;
            codecContext->bit_rate = bitRate;
            codecContext->pix_fmt = AV_PIX_FMT_YUV422P;
            codecContext->profile = FF_PROFILE_H264_HIGH_422_INTRA;
            codecContext->time_base = (AVRational) {1, frameRate};
            codecContext->framerate = (AVRational) {frameRate, 1};

            if (formatContext->flags & AVFMT_GLOBALHEADER) {
                codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            // copy codec parameters from codec context
            avcodec_parameters_from_context(stream->codecpar, codecContext);

            AVDictionary *options = nullptr;
            av_dict_set(&options, "preset", "fast", 0);
            av_dict_set(&options, "tune", "zerolatency", 0);

            // initialize output format to use the given codec
            statusCode = avcodec_open2(codecContext, codec, &options);
            av_dict_free(&options);
            assert(statusCode == 0);

            // write header in order to obtain timebase information in the created output stream
            statusCode = avformat_write_header(formatContext, nullptr);
            assert(statusCode >= 0);

            // print debug info
            av_dump_format(formatContext, videoStreamIndex, nullptr, 1);

            convertedFrame = av_frame_alloc();
            assert(convertedFrame);
            convertedFrame->width = codecContext->width;
            convertedFrame->height = codecContext->height;
            convertedFrame->format = codecContext->pix_fmt;
            statusCode = av_frame_get_buffer(convertedFrame, 0);
            assert(statusCode == 0);

            // fixme: hardcoded pix format and source image resolution
            converterContext = sws_getCachedContext(nullptr, frameWidth, frameHeight, AV_PIX_FMT_YUYV422,
                                                    codecContext->width, codecContext->height,
                                                    codecContext->pix_fmt, SWS_BICUBIC,
                                                    nullptr, nullptr, nullptr);
            assert(converterContext);

        };

        ~Encoder() {
            LOG(INFO) << "Destructor encoder";
        }

        void setOnFrameCallback(const std::function<void()> &callback) {
            onFrameReadyCallback = callback;
        }

        void sendNewFrame(uint8_t *rawFrame, std::size_t rawFrameSize) {
            incomingQueueMutex.lock();

            // fixme: eliminate hardcoded queue size
            if (incomingQueue.size() < 16) {
                incomingQueue.push(std::vector<uint8_t>(rawFrame, rawFrame + rawFrameSize));
            }

            incomingQueueMutex.unlock();
        }

        void writeFrame(std::vector<uint8_t> frame) {

            memcpy(rawFrame->data[0], frame.data(), frame.size());

            sws_scale(converterContext, reinterpret_cast<const uint8_t *const *>(rawFrame->data), rawFrame->linesize,
                      0, codecContext->height, convertedFrame->data, convertedFrame->linesize);

            AVPacket packet{};
            av_init_packet(&packet);

            int statusCode;

            statusCode = enc(codecContext, convertedFrame, &packet);
            assert(statusCode >= 0);

            onFrameReadyCallback();
        }

        void releaseFrame() {
            outgoingQueueMutex.lock();

            if (!outgoingQueue.empty()) {
                // fixme: check if memory leaks
                outgoingQueue.front().clear();
                outgoingQueue.pop();
            }

            outgoingQueueMutex.unlock();
        }

        void run() {

            while (true) {

                if (!incomingQueue.empty()) {

                    incomingQueueMutex.lock();
                    auto frame = incomingQueue.front();
                    incomingQueue.pop();
                    incomingQueueMutex.unlock();

                    writeFrame(std::move(frame));
                }
            }
        }

        void getFrame(std::vector<uint8_t >& data) {

            if (!outgoingQueue.empty()) {
                data = outgoingQueue.front();
            } else {
                data.clear();
            }
        }

        int enc(AVCodecContext *codecContext, AVFrame* frame, AVPacket* packet) {

            // send the frame to the encoder
            int statusCode = avcodec_send_frame(codecContext, frame);

            if (statusCode < 0) {
                LOG(ERROR) << "Error during sending frame for encoding";
                return statusCode;
            }

            while(statusCode >= 0) {

                statusCode = avcodec_receive_packet(codecContext, packet);

                if (statusCode == AVERROR(EAGAIN)) {
                    LOG(WARN) << "Output is unavailable in the current state";
                    return statusCode;
                }

                if (statusCode == AVERROR_EOF) {
                    LOG(WARN) << "End of stream has been reached";
                    return statusCode;
                }

                if (statusCode < 0) {
                    LOG(ERROR) << "Error during encoding";
                    return statusCode;
                }

                return statusCode;
            }

            return statusCode;
        }

    private:
        std::queue<std::vector<uint8_t>> incomingQueue;
        std::queue<std::vector<uint8_t>> outgoingQueue;

        std::mutex incomingQueueMutex;
        std::mutex outgoingQueueMutex;

        int frameWidth = 640;
        int frameHeight = 480;
        int frameRate = 15;
        int bitRate = 400000;
        std::size_t frameCounter = 0;

        AVFormatContext *formatContext = nullptr;
        AVCodecContext *codecContext = nullptr;
        AVStream *stream = nullptr;
        AVCodec *codec = nullptr;
        int videoStreamIndex;

        AVFrame *rawFrame = nullptr;
        AVFrame *convertedFrame = nullptr;
        size_t rawFrameSize = 0;

        SwsContext *converterContext = nullptr;

        std::function<void()> onFrameReadyCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_ENCODER_HPP
