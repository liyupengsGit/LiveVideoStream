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

    typedef struct FrameHolder {
        std::vector<uint8_t> data;
        size_t dataSize;
        size_t frameId;
    } FrameHolder;

    class Encoder {
    public:

        Encoder() {
            av_register_all();
            avcodec_register_all();
            avdevice_register_all();
            avformat_network_init();

            int statusCode;
            const std::string url = "rtsp://test:passwd@0.0.0.0:16005/live/0";

            statusCode = avformat_alloc_output_context2(&formatContext, nullptr, "rtsp", url.data());
            assert(statusCode >= 0);


        };

        ~Encoder() {
            LOG(INFO) << "Destructor encoder";
        }

        void setOnFrameCallback(const std::function<void()> &onFrameCallback) {
            Encoder::onFrameCallback = onFrameCallback;
        }

        void sendNewFrame(uint8_t *rgbFrame) {
            incomingQueueMutex.lock();

            if (incomingQueue.size() < 32) {
                incomingQueue.push(rgbFrame);
            }

            incomingQueueMutex.unlock();
        }

        void writeFrame(uint8_t *rgbFrame) {
            memcpy(srcFrame->data[0], rgbFrame, (size_t) bufferSize);
            sws_scale(converterContext, reinterpret_cast<const uint8_t *const *>(srcFrame->data), srcFrame->linesize,
                      0, codecContext->height, dstFrame->data, dstFrame->linesize);

            AVPacket packet{};
            // todo
        }

        char releaseFrame() {
            outgoingQueueMutex.lock();

            if (!outgoingQueue.empty()) {
                auto frame = outgoingQueue.front();
                outgoingQueue.pop();
                delete frame;
            }

            outgoingQueueMutex.unlock();
            return 1;
        }

        void run() {

            while (true) {
                if (!incomingQueue.empty()) {

                    incomingQueueMutex.lock();
                    auto frame = incomingQueue.front();
                    incomingQueue.pop();
                    incomingQueueMutex.unlock();
                    if (!frame) {
                        writeFrame(frame);
                    }
                }
            }
        }

        char getFrame(uint8_t **frameBuffer, uint &frameSize) {

            if (!outgoingQueue.empty()) {

                auto frame = outgoingQueue.front();
                *frameBuffer = frame->data.data();
                frameSize = (uint) frame->dataSize;
                return 1;
            }

            *frameBuffer = nullptr;
            frameSize = 0;
            return 0;
        }

    private:
        std::queue<uint8_t *> incomingQueue;
        std::queue<FrameHolder *> outgoingQueue;

        std::mutex incomingQueueMutex;
        std::mutex outgoingQueueMutex;

        int frameWidth = 640;
        int frameHeight = 480;

        AVFormatContext *formatContext = nullptr;
        AVOutputFormat *outputFormat = nullptr;
        AVCodecContext *codecContext = nullptr;
        AVStream *stream = nullptr;
        AVCodec *codec = nullptr;

        AVFrame *srcFrame = nullptr;
        AVFrame *dstFrame = nullptr;
        SwsContext *converterContext = nullptr;
        int bufferSize = 32;
        std::function<void()> onFrameCallback;
    };
}

#endif //LIVE_VIDEO_STREAM_ENCODER_HPP
