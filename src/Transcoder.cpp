#include "Transcoder.hpp"

#include <utility>

namespace LIRS {


    Transcoder *
    Transcoder::newInstance(std::string sourceUrl, std::string shortDevName, size_t frameWidth, size_t frameHeight,
                            std::string rawPixelFormatStr, std::string encoderPixelFormatStr,
                            size_t frameRate, size_t outputFrameRate, size_t outputBitRate) {

        // invoke constructor in order to create new instance
        return new Transcoder(sourceUrl, shortDevName, frameWidth, frameHeight, rawPixelFormatStr,
                              encoderPixelFormatStr,
                              frameRate, outputFrameRate, outputBitRate);
    }

    Transcoder::~Transcoder() {

        // close dummy file
        avio_close(encoderContext.formatContext->pb);

        // cleanup converter
        sws_freeContext(converterContext);

        // cleanup packets used for decoding and encoding
        av_packet_free(&decodingPacket);
        av_packet_free(&encodingPacket);

        // cleanup frames for decoding and encoding
        av_frame_free(&rawFrame);
        av_frame_free(&convertedFrame);

        // cleanup decoder and encoder codec contexts
        avcodec_free_context(&decoderContext.codecContext);
        avcodec_free_context(&encoderContext.codecContext);

        // close input format for the video device
        avformat_close_input(&decoderContext.formatContext);

        // cleanup decoder and encoder format contexts
        avformat_free_context(decoderContext.formatContext);
        avformat_free_context(encoderContext.formatContext);

        // reset all class members
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

        LOG(INFO) << "Transcoder has been destructed";
    }

    void Transcoder::playVideo() {

        // set the playing flag
        isPlayingFlag.store(true);

        // read raw data from the device into the packet
        while (av_read_frame(decoderContext.formatContext, decodingPacket) == 0) {

            // check whether it is a video stream's data
            if (decodingPacket->stream_index == decoderContext.videoStream->index) {

                // lock access to the raw frame
                fetchLastFrameMutex.lock();

                // fill raw frame with data from decoded packet
                decode(decoderContext.codecContext, rawFrame, decodingPacket);

                fetchLastFrameMutex.unlock();
            }

            // reset the packet (see ffmpeg docs)
            av_packet_unref(decodingPacket);
        }

        // capturing from the device is unavailable
        isPlayingFlag.store(false);
    }

    void Transcoder::fetchFrames() {

        // calculate the desired framerate's duration between frames in milliseconds
        const size_t outFrameRateMs = 1000 / outputFrameRate;

        // while the device is accessible
        while (isPlayingFlag.load()) {

            // get the starting point of time
            auto timeNow = std::chrono::high_resolution_clock::now();

            // make the frame writable (see ffmpeg docs)
            av_frame_make_writable(convertedFrame);

            // lock access to the raw frame
            fetchLastFrameMutex.lock();

            // convert raw frame into another pixel format and store it in convertedFrame
            sws_scale(converterContext, reinterpret_cast<const uint8_t *const *>(rawFrame->data),
                      rawFrame->linesize, 0, static_cast<int>(frameHeight), convertedFrame->data,
                      convertedFrame->linesize);

            // copy pts/dts, etc. (see ffmpeg docs)
            av_frame_copy_props(convertedFrame, rawFrame);

            fetchLastFrameMutex.unlock();

            // pass converted frame to the encoder and get encoded data in the packet
            if (encode(encoderContext.codecContext, convertedFrame, encodingPacket) >= 0) {

                /*
                av_packet_rescale_ts(encodingPacket, decoderContext.videoStream->time_base,
                                     encoderContext.videoStream->time_base);
                */

                // lock access to the encoded data queue
                outQueueMutex.lock();

                // add encoded data truncating first 4 bytes (start codes, NALU)
                outQueue.push(std::vector<uint8_t>(encodingPacket->data + H264_START_CODE_BYTES_NUMBER,
                                                   encodingPacket->data + encodingPacket->size));

                outQueueMutex.unlock();

                // invoke the callback indicating that a new encoded data is available
                if (onFrameCallback) {
                    onFrameCallback();
                }
            }

            // reset packet (see ffmpeg docs)
            av_packet_unref(encodingPacket);

            // get the ending point of time
            auto timeLast = std::chrono::high_resolution_clock::now();

            // calculate the elapsed time
            std::chrono::duration<double, std::milli> workTime = timeLast - timeNow;

            // sleep some delta ms in order to achieve the desired framerate
            if (workTime.count() < outFrameRateMs) {

                std::chrono::duration<double, std::milli> delta_ms(outFrameRateMs - workTime.count());
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(delta.count()));
            }
        }
    }

    bool Transcoder::retrieveEncodedData(std::vector<uint8_t> &data) {

        if (!outQueue.empty()) {

            // lock access to the queue
            outQueueMutex.lock();

            // retrieve encoded data from the queue
            data = outQueue.front();
            outQueue.pop();

            outQueueMutex.unlock();

            return true;
        }

        // no data is available
        return false;
    }

    Transcoder::Transcoder(std::string url, std::string shortDevName, size_t w, size_t h, std::string rawPixFmtStr,
                           std::string encPixFmtStr,
                           size_t frameRate, size_t outFrameRate, size_t outBitRate)
            : videoSourceUrl(std::move(url)), shortDeviceName(std::move(shortDevName)),
              frameWidth(w), frameHeight(h), frameRate(frameRate), outputFrameRate(outFrameRate),
              sourceBitRate(0), outputBitRate(outBitRate), decoderContext({}), encoderContext({}),
              rawFrame(nullptr), convertedFrame(nullptr), decodingPacket(nullptr), encodingPacket(nullptr),
              converterContext(nullptr), isPlayingFlag(false) {

        LOG(INFO) << "Constructing transcoder for \"" << videoSourceUrl << "\"";

        // get the pixel formats enumerations
        this->rawPixFormat = av_get_pix_fmt(rawPixFmtStr.c_str());
        this->encoderPixFormat = av_get_pix_fmt(encPixFmtStr.c_str());

        // todo throw exception or smth else
        assert(rawPixFormat != AV_PIX_FMT_NONE && encoderPixFormat != AV_PIX_FMT_NONE);

        LOG(INFO) << "Decoder/encoder pixel formats: " << rawPixFmtStr << " and " << encPixFmtStr;

        // register ffmpeg codecs, etc.
        registerAll();

        // initialize decoder and encoder stuff

        initializeDecoder();

        initializeEncoder();

        // initialize converter form raw pixel format to the supported by the encoder one.
        initializeConverter();
    }

    void Transcoder::setOnFrameCallback(std::function<void()> callback) {
        onFrameCallback = std::move(callback);
    }

    void Transcoder::registerAll() {
        LOG(DEBUG) << "Registering AV components";
        av_register_all();
        avdevice_register_all(); // register devices, e.g. v4l2
        avcodec_register_all(); // register codecs, e.g. H.264, HEVC(H.265)
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

        // choose the appropriate profile
        if (encoderPixFormat == AV_PIX_FMT_YUYV422) {
            encoderContext.codecContext->profile = FF_PROFILE_H264_HIGH_422_INTRA;
        } else {
            encoderContext.codecContext->profile = FF_PROFILE_H264_HIGH_10_INTRA;
        }

        encoderContext.codecContext->time_base = (AVRational) {1, static_cast<int>(outputFrameRate)};
        encoderContext.codecContext->framerate = (AVRational) {static_cast<int>(outputFrameRate), 1};
        encoderContext.codecContext->pix_fmt = encoderPixFormat;

        // copy encoder parameters to the video stream parameters
        avcodec_parameters_from_context(encoderContext.videoStream->codecpar, encoderContext.codecContext);
        encoderContext.videoStream->r_frame_rate = (AVRational) {static_cast<int>(outputFrameRate), 1};
        encoderContext.videoStream->avg_frame_rate = (AVRational) {static_cast<int>(outputFrameRate), 1};

        // set the encoder options
        AVDictionary *options = nullptr;
        av_dict_set(&options, "preset", "veryfast",
                    0); // slower, slow, medium, fast, faster, veryfast, superfast, ultrfast
        av_dict_set(&options, "tune", "zerolatency", 0); // for live streaming
        av_dict_set_int(&options, "crf", 21, 0); // 22 and 23 are acceptable

        // open the output format to use given codec
        statCode = avcodec_open2(encoderContext.codecContext, encoderContext.codec, &options);
        av_dict_free(&options);
        assert(statCode == 0);

        // initializes time base (see ffmpeg docs)
        statCode = avformat_write_header(encoderContext.formatContext, nullptr);
        assert(statCode >= 0);

        // report info to the console
        av_dump_format(encoderContext.formatContext, encoderContext.videoStream->index, "null", 1);
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

        // allocate packets for the encoder and decoder

        decodingPacket = av_packet_alloc();
        av_init_packet(decodingPacket);

        encodingPacket = av_packet_alloc();
        av_init_packet(encodingPacket);

        // allocate raw frame
        rawFrame = av_frame_alloc();

        // allocate frame to be used in converter manually allocating buffer
        convertedFrame = av_frame_alloc();
        convertedFrame->width = static_cast<int>(frameWidth);
        convertedFrame->height = static_cast<int>(frameHeight);
        convertedFrame->format = encoderPixFormat;
        auto statCode = av_frame_get_buffer(convertedFrame, 0);
        assert(statCode == 0);

        // create converter from raw pixel format to encoder supported pixel format
        converterContext = sws_getCachedContext(nullptr, static_cast<int>(frameWidth),
                                                static_cast<int>(frameHeight), rawPixFormat,
                                                static_cast<int>(frameWidth), static_cast<int>(frameHeight),
                                                encoderPixFormat, SWS_BILINEAR, nullptr, nullptr, nullptr);

        LOG(INFO) << "Pixel format converter for \"" << videoSourceUrl << "\" has been created ("
                  << av_get_pix_fmt_name(rawPixFormat) << " -> " << av_get_pix_fmt_name(encoderPixFormat) << ")";

    }

    std::string Transcoder::getDeviceName() const {
        return videoSourceUrl;
    }

    std::string Transcoder::getAlias() const {
        return shortDeviceName;
    }

}