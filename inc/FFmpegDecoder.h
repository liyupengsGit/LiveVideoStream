#ifndef LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
#define LIVE_VIDEO_STREAM_FFMPEG_DECODER_H

#include "Decoder.h" // abstract base class for decoders

#ifdef __cplusplus
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

/**
 * @todo consider sending flush packets (with NULL data and size), see avcodec_send_packet(..)
 * @todo remove asserts, don't use hardcoded opts, throw exceptions on errors
 *
 */
namespace LIRS {

    /**
     * Decoder class using ffmpeg.
     */
    class FFmpegDecoder : public Decoder {
    public:

        explicit FFmpegDecoder(std::string videoSourcePath, DecoderParams params = {})
                : Decoder(std::move(videoSourcePath), params),
                  pCodecCtx(nullptr), pFormatCtx(nullptr), pImageConvertCtx(nullptr),
                  pFrame(nullptr), videoStreamIdx(-1)
        {
            registerAll(); // register codecs, devices, etc.

            pFormatCtx = avformat_alloc_context();
            assert(pFormatCtx);

            auto pInputFormat = av_find_input_format("v4l2"); // video4linux2
            assert(pInputFormat);

            AVDictionary *opts = nullptr;
            av_dict_set(&opts, "framerate", std::to_string(_frameRate).c_str(), 0);
            av_dict_set(&opts, "video_size", "640x480", 0);
            av_dict_set(&opts, "pixel_format", "yuyv422", 0);

            int statusCode = avformat_open_input(&pFormatCtx, _videoSourcePath.data(), pInputFormat, &opts);
            assert(statusCode >= 0);

            statusCode = avformat_find_stream_info(pFormatCtx, nullptr);
            assert(statusCode >= 0);

            av_dump_format(pFormatCtx, 0, _videoSourcePath.data(), 0);

            for (uint i = 0; i < pFormatCtx->nb_streams; ++i) {
                if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    videoStreamIdx = i;
                    break;
                }
            }
            assert(videoStreamIdx >= 0);

            auto pVideoStream = pFormatCtx->streams[videoStreamIdx];
            auto pCodec = avcodec_find_decoder(pVideoStream->codecpar->codec_id);
            pCodecCtx = avcodec_alloc_context3(pCodec);
            assert(pCodec && pCodecCtx);

            statusCode = avcodec_parameters_to_context(pCodecCtx, pVideoStream->codecpar);
            assert(statusCode >= 0);

            av_dict_set(&opts, "b", "2.5M", 0); // todo what is this?
            statusCode = avcodec_open2(pCodecCtx, pCodec, &opts);
            assert(statusCode == 0);

            pFrame = av_frame_alloc();
            assert(pFrame);

            AVPixelFormat pixelFormat = AV_PIX_FMT_BGR24; // todo experiment on this
            int numBytes = av_image_get_buffer_size(pixelFormat, pCodecCtx->width, pCodecCtx->height, 1); // todo find out what is the align
            assert(numBytes > 0);

            buffer.reserve((uint) numBytes + AV_INPUT_BUFFER_PADDING_SIZE);

            av_image_fill_arrays(pFrame->data, pFrame->linesize, buffer.data(), pixelFormat,
                                 pCodecCtx->width, pCodecCtx->height, 1); // todo align? why eq to 1?

            pImageConvertCtx = sws_getCachedContext(nullptr, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                                    pCodecCtx->width, pCodecCtx->height, pixelFormat, SWS_BICUBIC,
                                                    nullptr, nullptr, nullptr);
            assert(pImageConvertCtx);

            _frameHeight = static_cast<size_t>(pCodecCtx->height);
            _frameWidth = static_cast<size_t>(pCodecCtx->width);
            _bitRate = static_cast<size_t>(pFormatCtx->bit_rate);
            _gop = static_cast<size_t>(pCodecCtx->gop_size);
            _frameRate = static_cast<size_t>(pVideoStream->avg_frame_rate.num / pVideoStream->avg_frame_rate.den);
        }


        void decodeLoop() override {

            AVFrame* pRawFrame = av_frame_alloc();
            AVPacket packet{};

            bool isFrameReady = false; // default - it's not ready
            int statusCode = 0;
            int counter = 120;

            while (av_read_frame(pFormatCtx, &packet) == 0 && counter --> 0) {

                if (packet.buf && packet.stream_index == videoStreamIdx) {

                    statusCode = decode(pCodecCtx, pRawFrame, isFrameReady, &packet);

                    if (statusCode != 0) {
                        LOG(WARN) << "Frame was not captured, error code: " << statusCode;
                    }

                    if (isFrameReady) {

                        // convert pix format
                        sws_scale(pImageConvertCtx, static_cast<const uint8_t* const*>(pRawFrame->data),
                                  pRawFrame->linesize, 0, (int)_frameHeight,
                                  pFrame->data, pFrame->linesize);

                        // calling the callback function
                        _onFrameCallback(pFrame->data[0]);
                    }

                    av_packet_unref(&packet);
                }
            }

            av_frame_free(&pRawFrame);
            av_packet_unref(&packet);
        }


        /**
         * Destructs all allocated objects and closes contexts.
         */
        ~FFmpegDecoder() override {
            LOG(INFO) << "Destructing decoder";

            sws_freeContext(pImageConvertCtx);
            av_frame_unref(pFrame);
            av_free(pFrame);
            avcodec_free_context(&pCodecCtx);
            avformat_close_input(&pFormatCtx);
            avformat_free_context(pFormatCtx);
        }

    private:

        // contexts
        AVCodecContext *pCodecCtx;
        AVFormatContext *pFormatCtx;
        SwsContext *pImageConvertCtx;

        // converted frame
        AVFrame *pFrame;

        // index of the video stream
        int videoStreamIdx;

        // frame's buffer
        std::vector<uint8_t> buffer;

    private:

        /**
         * Initializes FFmpeg environment registering all available codecs,
         * formats, devices, etc.
         */
        void registerAll() {
            LOG(DEBUG) << "Init ffmpeg environment";

            av_register_all();
            avdevice_register_all();
            avcodec_register_all();
        }

        /**
         * Sends packet and retrieves frame with raw image data.
         *
         * @param codecContext codec context.
         * @param frame raw image frame which is to be populated with data.
         * @param gotFrame flag is set to true when frame was captured otherwise it is set to false.
         * @param packet packet to be sent to the decoder.
         * @return 0 - if success, otherwise error code.
         */
        int decode(AVCodecContext *codecContext, AVFrame *frame, bool &gotFrame, AVPacket *packet) {

            int statusCode = 0;
            gotFrame = false;

            statusCode = avcodec_send_packet(codecContext, packet);

            // the end of stream has been reached, success / or error
            if (statusCode < 0) return statusCode == AVERROR_EOF ? 0 : statusCode;

            statusCode = avcodec_receive_frame(codecContext, frame);

            if (statusCode < 0 && statusCode != AVERROR(EAGAIN) && statusCode != AVERROR_EOF) {
                return statusCode;
            }

            if (statusCode >= 0) gotFrame = true;

            return 0;
        }
    };
}

#endif //LIVE_VIDEO_STREAM_FFMPEG_DECODER_H
