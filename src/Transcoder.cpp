#include "Transcoder.hpp"

namespace LIRS {


    Transcoder *Transcoder::newInstance(std::string sourceUrl, size_t frameWidth, size_t frameHeight,
                                        std::string rawPixelFormatStr, std::string encoderPixelFormatStr,
                                        size_t frameRate, size_t outputFrameRate, size_t outputBitRate) {

        return new Transcoder(sourceUrl, frameWidth, frameHeight, rawPixelFormatStr, encoderPixelFormatStr,
                              frameRate, outputFrameRate, outputBitRate);
    }

    Transcoder::~Transcoder() {

        avio_close(encoderContext.formatContext->pb);

        sws_freeContext(converterContext);
        av_packet_free(&packet);
        av_frame_free(&rawFrame);
        av_frame_free(&convertedFrame);

        avcodec_free_context(&decoderContext.codecContext);
        avcodec_free_context(&encoderContext.codecContext);

        avformat_close_input(&decoderContext.formatContext);

        avformat_free_context(decoderContext.formatContext);
        avformat_free_context(encoderContext.formatContext);

        decoderContext = {};
        encoderContext = {};

        videoSourceUrl.clear();
        frameWidth = 0;
        frameHeight = 0;
        rawPixFormat = AV_PIX_FMT_NONE;
        encoderPixFormat = AV_PIX_FMT_NONE;
        frameRate = 0;
        outputFrameRate = 0;
        sourceBitRate = 0;
        outputBitRate = 0;

        LOG(INFO) << "Transcoder has been destructed.";
    }

    void Transcoder::playVideo() {

        LOG(INFO) << "Starting playing video from camera...";

        while (av_read_frame(decoderContext.formatContext, packet) == 0) {

            if (packet->stream_index == decoderContext.videoStream->index) {

                if (decode(decoderContext.codecContext, rawFrame, packet) >= 0) {

                    av_frame_make_writable(convertedFrame);

                    sws_scale(converterContext, reinterpret_cast<const uint8_t *const *>(rawFrame->data),
                              rawFrame->linesize, 0, static_cast<int>(frameHeight), convertedFrame->data,
                              convertedFrame->linesize);

                    av_frame_copy_props(convertedFrame, rawFrame);

                    if (encode(encoderContext.codecContext, convertedFrame, packet) >= 0) {

                        // fixme: don't needed?
                        av_packet_rescale_ts(packet, decoderContext.videoStream->time_base,
                                             encoderContext.videoStream->time_base);

                        outQueueMutex.lock();
                        // fixme: copy
                        outQueue.push(std::vector<uint8_t>(packet->data + 4, packet->data + packet->size));
                        outQueueMutex.unlock();

                        if (onFrameCallback) {
                            onFrameCallback();
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }
    }

    bool Transcoder::retrieveFrame(std::vector<uint8_t> &frame) {
        if (!outQueue.empty()) {
            outQueueMutex.lock();
            frame = outQueue.front();
            outQueue.pop();
            outQueueMutex.unlock();
            return true;
        }
        return false;
    }

    Transcoder::Transcoder(std::string url, size_t w, size_t h, std::string rawPixFmtStr, std::string encPixFmtStr,
                           size_t frameRate, size_t outFrameRate, size_t outBitRate)
            : videoSourceUrl(std::move(url)), frameWidth(w), frameHeight(h),
              frameRate(frameRate), outputFrameRate(outFrameRate),
              sourceBitRate(0), outputBitRate(outBitRate),
              decoderContext({}), encoderContext({}),
              rawFrame(nullptr), convertedFrame(nullptr), packet(nullptr),
              converterContext(nullptr) {

        LOG(INFO) << "Constructing transcoder for \"" << videoSourceUrl << "\"";

        this->rawPixFormat = av_get_pix_fmt(rawPixFmtStr.c_str());
        this->encoderPixFormat = av_get_pix_fmt(encPixFmtStr.c_str());

        assert(rawPixFormat != AV_PIX_FMT_NONE && encoderPixFormat != AV_PIX_FMT_NONE);

        LOG(INFO) << "Decoder/encoder pixel formats: " << rawPixFmtStr << " and " << encPixFmtStr;

        registerAll();

        initializeDecoder();

        initializeEncoder();

        initializeConverter();
    }

    void Transcoder::setOnFrameCallback(std::function<void()> callback) {
        onFrameCallback = std::move(callback);
    }

    void Transcoder::registerAll() {
        LOG(DEBUG) << "Registering AV components";
        av_register_all();
        avdevice_register_all();
        avcodec_register_all();
    }

    void Transcoder::initializeDecoder() {

        // holds the general (header) information about the format (container)
        decoderContext.formatContext = avformat_alloc_context();
        auto inputFormat = av_find_input_format("v4l2");

        auto frameResolution = utils::concatParams({frameWidth, frameHeight}, "x");

        AVDictionary *options = nullptr;
        av_dict_set(&options, "video_size", frameResolution.data(), 0);
        av_dict_set(&options, "pixel_format", av_get_pix_fmt_name(rawPixFormat), 0);
        av_dict_set_int(&options, "framerate", frameRate, 0);

        int statCode = avformat_open_input(&decoderContext.formatContext, videoSourceUrl.data(),
                                           inputFormat, &options);
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

        // save info
        frameRate = static_cast<size_t>(decoderContext.videoStream->r_frame_rate.num /
                                        decoderContext.videoStream->r_frame_rate.den);
        frameWidth = static_cast<size_t>(decoderContext.codecContext->width);
        frameHeight = static_cast<size_t>(decoderContext.codecContext->height);
        rawPixFormat = decoderContext.codecContext->pix_fmt;
        sourceBitRate = static_cast<size_t>(decoderContext.codecContext->bit_rate);

        LOG(INFO) << "Decoder for \"" << videoSourceUrl << "\" has been created (fps: "
                  << frameRate << ", width x height: " << frameWidth << "x" << frameHeight << ")";
    }

    void Transcoder::initializeEncoder() {

        // allocate format context for an output format (null - no output file)
        auto statCode = avformat_alloc_output_context2(&encoderContext.formatContext, nullptr, "null", nullptr);
        assert(statCode >= 0);

        encoderContext.codec = avcodec_find_encoder_by_name("libx264");
        assert(encoderContext.codec);

        // create new video output stream
        encoderContext.videoStream = avformat_new_stream(encoderContext.formatContext, encoderContext.codec);
        assert(encoderContext.videoStream);
        encoderContext.videoStream->id = encoderContext.formatContext->nb_streams - 1;

        // create codec context (for each codec new codec context)
        encoderContext.codecContext = avcodec_alloc_context3(encoderContext.codec);
        assert(encoderContext.codecContext);

        // set up parameters
        encoderContext.codecContext->width = static_cast<int>(frameWidth);
        encoderContext.codecContext->height = static_cast<int>(frameHeight);
        encoderContext.codecContext->profile = FF_PROFILE_H264_HIGH_422_INTRA;
        encoderContext.codecContext->time_base = (AVRational) {1, static_cast<int>(frameRate)};
        encoderContext.codecContext->framerate = (AVRational) {static_cast<int>(frameRate), 1};
        encoderContext.codecContext->pix_fmt = encoderPixFormat;

        avcodec_parameters_from_context(encoderContext.videoStream->codecpar, encoderContext.codecContext);
        encoderContext.videoStream->r_frame_rate = (AVRational) {static_cast<int>(outputFrameRate), 1};
        encoderContext.videoStream->avg_frame_rate = (AVRational) {static_cast<int>(outputFrameRate), 1};

        AVDictionary *options = nullptr;
        av_dict_set(&options, "preset", "veryfast", 0);
        av_dict_set(&options, "tune", "zerolatency", 0);
        av_dict_set_int(&options, "b", outputBitRate, 0);

        // open the output format to use given codec
        statCode = avcodec_open2(encoderContext.codecContext, encoderContext.codec, &options);
        av_dict_free(&options);
        assert(statCode == 0);

        statCode = avformat_write_header(encoderContext.formatContext, nullptr);
        assert(statCode >= 0);

        av_dump_format(encoderContext.formatContext, encoderContext.videoStream->index, "null", 1);

        LOG(INFO) << "Encoder for \"" << videoSourceUrl << "\" " << "has been created (preset: veryfast, "
                  << "tune: zerolatency, bitrate: " << outputBitRate << ")";

    }

    int Transcoder::decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

        auto statCode = avcodec_send_packet(codecContext, packet);

        if (statCode < 0) {
            LOG(ERROR) << "Error sending a packet for decoding";
            return statCode;
        }

        statCode = avcodec_receive_frame(codecContext, frame);

        if (statCode == AVERROR(EAGAIN) || statCode == AVERROR_EOF) {
            LOG(WARN) << "No frames available or end of file has been reached";
            return statCode;
        }

        if (statCode < 0) {
            LOG(ERROR) << "Error during decoding";
            return statCode;
        }

        return true;
    }

    int Transcoder::encode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

        auto statCode = avcodec_send_frame(codecContext, frame);

        if (statCode < 0) {
            LOG(ERROR) << "Error during sending frame for encoding";
            return statCode;
        }

        statCode = avcodec_receive_packet(codecContext, packet);

        if (statCode == AVERROR(EAGAIN) || statCode == AVERROR_EOF) {
            LOG(WARN) << "EAGAIN or EOF while encoding";
            return statCode;

        }

        if (statCode < 0) {
            LOG(ERROR) << "Error during receiving packet for encoding";
            return statCode;
        }

        return statCode;
    }

    void Transcoder::initializeConverter() {

        packet = av_packet_alloc();
        av_init_packet(packet);

        rawFrame = av_frame_alloc();

        convertedFrame = av_frame_alloc();
        convertedFrame->width = static_cast<int>(frameWidth);
        convertedFrame->height = static_cast<int>(frameHeight);
        convertedFrame->format = encoderPixFormat;
        auto statCode = av_frame_get_buffer(convertedFrame, 0);
        assert(statCode == 0);

        converterContext = sws_getCachedContext(nullptr, static_cast<int>(frameWidth),
                                                static_cast<int>(frameHeight), rawPixFormat,
                                                static_cast<int>(frameWidth), static_cast<int>(frameHeight),
                                                encoderPixFormat, SWS_BICUBIC, nullptr, nullptr, nullptr);

        LOG(INFO) << "Pixel format converter for \"" << videoSourceUrl << "\" has been created ("
                  << av_get_pix_fmt_name(rawPixFormat) << " -> " << av_get_pix_fmt_name(encoderPixFormat) << ")";

    }


}