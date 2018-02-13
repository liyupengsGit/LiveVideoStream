#ifndef LIVE_VIDEO_STREAM_ENCODER_HPP
#define LIVE_VIDEO_STREAM_ENCODER_HPP

#include <vector>
#include <functional>
#include <queue>
#include <mutex>
#include <libavutil/error.h>

#include "logger.h"

#ifdef __cplusplus
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <x264.h>
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

        Encoder() : videoTime(0) {

            avformat_network_init(); // unnecessary

            int statusCode;

            avcodec_register_all();
            av_register_all();

            // find encoder (H.264)
            pVideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
            assert(pVideoCodec);

            statusCode = avformat_alloc_output_context2(&pFormatCtx, nullptr, "h264", fileName.data());
            assert(statusCode >= 0);

            pOutputFmt = pFormatCtx->oformat;
            pOutputFmt->video_codec = AV_CODEC_ID_H264;
            pOutputFmt->audio_codec = AV_CODEC_ID_NONE;
            pOutputFmt->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            assert(pOutputFmt);

            // create new stream
            auto newStream = avformat_new_stream(pFormatCtx, pVideoCodec);
            assert(newStream);

            // copy codec parameters to the codec context
            newStream->codecpar->codec_id   = AV_CODEC_ID_H264;
            newStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
            newStream->codecpar->format     = AV_PIX_FMT_YUV422P;
            newStream->codecpar->width      = frameWidth;
            newStream->codecpar->height     = frameHeight;
//            newStream->codecpar->bit_rate   = bitsPerSecond; // todo set this up manually or not?
            pVideoStream = newStream;

            pCodecCtx = avcodec_alloc_context3(pVideoCodec);
            avcodec_parameters_to_context(pCodecCtx, newStream->codecpar);

            // settings for intra-only
            pCodecCtx->gop_size = 0;
            pCodecCtx->has_b_frames = 0;
            pCodecCtx->time_base = (AVRational) {1, framesPerSecond};
            av_opt_set(pCodecCtx->priv_data, "preset", "slow", 0);
            av_opt_set(pCodecCtx->priv_data, "tune", "zerolatency", 0);
//            av_opt_set(pCodecCtx->priv_data, "crf", "0", 0); // lossless


            // create own AVIOContext in order to not to use file.
//            avio_alloc_context()

            av_dump_format(pFormatCtx, 0, fileName.data(), 1);

            statusCode = avcodec_open2(pCodecCtx, pVideoCodec, nullptr);
            assert(statusCode >= 0);

//            statusCode = avio_open(&pFormatCtx->pb, fileName.data(), AVIO_FLAG_WRITE);
//            assert(statusCode >= 0);
//
//            statusCode = avformat_write_header(pFormatCtx, nullptr);
//            assert(statusCode >= 0);


        };

        ~Encoder() {
            LOG(INFO) << "Destructor encoder";
        }

        void setOnFrameCallback(const std::function<void()> &onFrameCallback) {
            Encoder::onFrameCallback = onFrameCallback;
        }

        void sendNewFrame(uint8_t* rgbFrame) {
            incomingQueueMutex.lock();

            if (incomingQueue.size() < 32) {
                incomingQueue.push(rgbFrame);
            }

            incomingQueueMutex.unlock();
        }

        void writeFrame(uint8_t* rgbFrame) {
            memcpy(srcFrame->data[0], rgbFrame, (size_t) bufferSize);
            sws_scale(swsCtx, reinterpret_cast<const uint8_t *const *>(srcFrame->data), srcFrame->linesize,
                      0, pCodecCtx->height, dstFrame->data, dstFrame->linesize);

            AVPacket packet {};
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

        char getFrame(uint8_t** frameBuffer, uint &frameSize) {

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

        int framesPerSecond = 15;
        int gobSize = 12;
        int bitsPerSecond = 400000; // BPS
        int frameCount = 0;
        int frameWidth = 640;
        int frameHeight = 480;
        double videoTime;
        std::string fileName = "/tmp/video.h264";

        AVCodecContext* pCodecCtx = nullptr;
        AVStream* pVideoStream = nullptr;
        AVOutputFormat* pOutputFmt = nullptr;
        AVFormatContext* pFormatCtx = nullptr;
        AVCodec* pVideoCodec = nullptr;
        AVFrame* srcFrame = nullptr;
        AVFrame* dstFrame = nullptr;
        SwsContext *swsCtx = nullptr;
        int bufferSize = 32;
        std::function<void()> onFrameCallback;

    };
}

#endif //LIVE_VIDEO_STREAM_ENCODER_HPP
