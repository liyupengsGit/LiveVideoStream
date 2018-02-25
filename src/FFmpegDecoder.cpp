#include "FFmpegDecoder.hpp"

#include <utility>

/**
 * @todo
 *  - remove asserts, don't use hardcoded opts, throw exceptions on errors
 *  - check timestamp is valid and present
 *  - Rename logger.h to Logger.hpp
 *  - Add comments to the FFmpegDecoder.hpp
 *  -
 */

namespace LIRS {

    FFmpegDecoder::FFmpegDecoder(std::string videoSourcePath, const DecoderParams& params)
            : Decoder(std::move(videoSourcePath), params),
              formatContext(nullptr), codecContext(nullptr), codec(nullptr),
              videoStream(nullptr), videoStreamIndex(-1), options(nullptr) {

        registerAll();

        // holds the general (header) information about the format (container)
        formatContext = avformat_alloc_context();
        assert(formatContext);

        // input format (video4linux device)
        auto inputFormat = av_find_input_format("v4l2");
        assert(inputFormat);

        // set demuxer options
        auto frameResolution = utils::concatParams({_frameWidth, _frameHeight}, "x");
        av_dict_set_int(&options, "framerate", _frameRate, 0);
        av_dict_set(&options, "video_size", frameResolution.data(), 0);
        av_dict_set(&options, "pixel_format", _pixelFormat.data(), 0);

        // open an input stream
        int statusCode = avformat_open_input(&formatContext, _videoSourcePath.data(), inputFormat, &options);
        av_dict_free(&options);
        assert(statusCode == 0);

        // read packets to get information about all of the streams
        statusCode = avformat_find_stream_info(formatContext, nullptr);
        assert(statusCode >= 0);

        av_dump_format(formatContext, 0, videoSourcePath.data(), 0);

        // find video stream among other streams (find video stream index)
        videoStreamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        assert(videoStreamIndex >= 0);
        assert(codec);
        videoStream = formatContext->streams[statusCode];

        // create codec context for the codec
        codecContext = avcodec_alloc_context3(codec);
        assert(codecContext);

        // copy video stream parameters to the created codec context
        statusCode = avcodec_parameters_to_context(codecContext, videoStream->codecpar);
        assert(statusCode >= 0);

        // initialize the codec context to use the given codec
        statusCode = avcodec_open2(codecContext, codec, &options);
        assert(statusCode == 0);

        // save bit rate info
        _bitRate = static_cast<size_t>(videoStream->codecpar->bit_rate);
    }


    FFmpegDecoder::~FFmpegDecoder() {
        av_dict_free(&options);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        avformat_free_context(formatContext);
        videoStreamIndex = -1;
    }


    void FFmpegDecoder::registerAll() {
        av_register_all();
        avdevice_register_all();
        avcodec_register_all();
    }


    int FFmpegDecoder::decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

        int statusCode = avcodec_send_packet(codecContext, packet);

        // couldn't send packet to the decoder
        if (statusCode < 0) {
            LOG(ERROR) << "Error sending packet for decoding";
            return statusCode;
        }

        statusCode = avcodec_receive_frame(codecContext, frame);

        if (statusCode == AVERROR(EAGAIN)) {
            LOG(WARN) << "Unable to read output. Try to send new input";
            return statusCode;
        }

        if (statusCode == AVERROR_EOF) {
            LOG(WARN) << "End of stream has been reached";
            return statusCode;
        }

        if (statusCode < 0) {
            LOG(ERROR) << "Error during decoding";
            return statusCode;
        }

        return statusCode;
    }


    void FFmpegDecoder::decodeLoop() {

        int statusCode;
        int counter = 64;

        // create and init packet
        auto packet = av_packet_alloc();
        av_init_packet(packet);

        auto rawFrame = av_frame_alloc();

        // fill the packet with a raw data from the input stream
        while (av_read_frame(formatContext, packet) == 0 && counter --> 0) {

            // if it is a video stream's data (may be from the audio stream)
            if (packet->stream_index == videoStreamIndex) {

                // decode packet (retrieve frame)
                statusCode = decode(codecContext, rawFrame, packet);

                // if the packet is successfully decoded
                if (statusCode >= 0) {

                    if (_onFrameCallback) {

                        // todo copy the frame??? or allocate new frame each time
                        _onFrameCallback(rawFrame->data[0]); // callback
                    }
                }
            }

            // cleanup the packet
            av_packet_unref(packet);
        }

        // cleanup the packet and frame
        av_packet_free(&packet);
        av_frame_free(&rawFrame);
    }
}
