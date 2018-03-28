#include "Transcoder.hpp"

namespace LIRS {

    Transcoder *Transcoder::newInstance(const std::string &sourceUrl, const std::string &devAlias,
                                        size_t frameWidth, size_t frameHeight, const std::string &rawPixelFormatStr,
                                        const std::string &encoderPixelFormatStr, size_t frameRate, size_t frameStep,
                                        size_t outputFrameRate) {

        // create new instance
        return new Transcoder(sourceUrl, devAlias, frameWidth, frameHeight, rawPixelFormatStr, encoderPixelFormatStr,
                              frameRate, frameStep, outputFrameRate);
    }

    Transcoder::~Transcoder() {

        isPlayingFlag.store(false); // signal to run() to stop decoding/encoding frames

        LOG(INFO) << "Transcoder has been destructed";
    }

    void Transcoder::run() {

        // set the playing flag
        isPlayingFlag.store(true);

        // read raw data from the device into the packet
        while (isPlayingFlag.load() && av_read_frame(decoderContext.formatContext, decodingPacket) == 0) {

            // check whether it is a video stream's data
            if (decodingPacket->stream_index == decoderContext.videoStream->index) {

                // fill raw frame with data from decoded packet
                if (decode(decoderContext.codecContext, rawFrame, decodingPacket)) {

                    // push frames to the buffer
                    auto statusCode = av_buffersrc_add_frame_flags(bufferSrcCtx, rawFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

                    if (statusCode < 0) continue; // workaround for buggy cameras

                    // pull frames from filter graph
                    while (true) {

                        statusCode = av_buffersink_get_frame(bufferSinkCtx, filterFrame);

                        if (statusCode == AVERROR(EAGAIN) || statusCode == AVERROR_EOF) {
                            break;
                        }

                        assert(statusCode >= 0);

                        av_frame_make_writable(convertedFrame);

                        // convert raw frame into another pixel format
                        sws_scale(converterContext, reinterpret_cast<const uint8_t *const *>(filterFrame->data),
                                  filterFrame->linesize, 0, static_cast<int>(frameHeight),
                                  convertedFrame->data, convertedFrame->linesize);

                        // copy pts/dts, etc.
                        av_frame_copy_props(convertedFrame, filterFrame);

                        if (encode(encoderContext.codecContext, convertedFrame, encodingPacket) >= 0) {

                            /*
                            av_packet_rescale_ts(encodingPacket, decoderContext.videoStream->time_base,
                                                 encoderContext.videoStream->time_base);
                            */

                            // invoke the callback indicating that a new encoded data is available
                            if (onEncodedDataCallback) {
                                onEncodedDataCallback(
                                        std::vector<uint8_t>(encodingPacket->data + NALU_START_CODE_BYTES_NUMBER,
                                                             encodingPacket->data + encodingPacket->size));
                            }
                        }

                        av_packet_unref(encodingPacket);
                    }

                    av_frame_unref(filterFrame);
                }
            }

            av_packet_unref(decodingPacket);
        }

        // capturing from the device is unavailable
        isPlayingFlag.store(false);

        cleanup(); // free memory, close handles, etc.
    }


    Transcoder::Transcoder(const std::string &url, const std::string &alias, size_t w, size_t h,
                           const std::string &rawPixFmtStr, const std::string &encPixFmtStr,
                           size_t frameRate, size_t frameStep, size_t outFrameRate)
            : videoSourceUrl(url), deviceAlias(alias), frameWidth(w), frameHeight(h),
              frameRate(AVRational{(int) frameRate, 1}), frameStep(frameStep),
              outputFrameRate(AVRational{(int) outFrameRate, 1}), sourceBitRate(0),
              decoderContext({}), encoderContext({}), rawFrame(nullptr), convertedFrame(nullptr), filterFrame(nullptr),
              decodingPacket(nullptr), encodingPacket(nullptr), converterContext(nullptr), filterGraph(nullptr),
              bufferSrcCtx(nullptr), bufferSinkCtx(nullptr), isPlayingFlag(false) {

        LOG(INFO) << "Constructing transcoder for \"" << videoSourceUrl << "\"";

        // get the pixel formats enumerations
        this->rawPixFormat = av_get_pix_fmt(rawPixFmtStr.c_str());
        this->encoderPixFormat = av_get_pix_fmt(encPixFmtStr.c_str());

        assert(rawPixFormat != AV_PIX_FMT_NONE && encoderPixFormat != AV_PIX_FMT_NONE);

        LOG(INFO) << "Decoder/encoder pixel formats: " << rawPixFmtStr << " and " << encPixFmtStr;

        registerAll();

        initializeDecoder();

        initializeEncoder();

        initializeConverter();

        initFilters();
    }

    void Transcoder::setOnEncodedDataCallback(std::function<void(std::vector<uint8_t> &&)> callback) {
        onEncodedDataCallback = std::move(callback);
    }

    void Transcoder::registerAll() {

        av_register_all();

        avdevice_register_all();

        avcodec_register_all();

        avfilter_register_all();
    }

    void Transcoder::initializeDecoder() {

        // holds the general (header) information about the format (container)
        decoderContext.formatContext = avformat_alloc_context();

        AVInputFormat *inputFormat = av_find_input_format("v4l2"); // using Video4Linux for capturing

        auto frameResolutionStr = utils::concatParams({frameWidth, frameHeight}, "x");
        auto framerateStr = utils::concatParams({(size_t) frameRate.num, (size_t) frameRate.den}, "/");

        AVDictionary *options = nullptr;
        av_dict_set(&options, "video_size", frameResolutionStr.data(), 0);
        av_dict_set(&options, "pixel_format", av_get_pix_fmt_name(rawPixFormat), 0);
        av_dict_set(&options, "framerate", framerateStr.data(), 0);

        int statCode = avformat_open_input(&decoderContext.formatContext, videoSourceUrl.data(),
                                           inputFormat, &options);
        av_dict_free(&options);
        assert(statCode == 0);

        statCode = avformat_find_stream_info(decoderContext.formatContext, nullptr);
        assert(statCode >= 0);

        av_dump_format(decoderContext.formatContext, 0, videoSourceUrl.data(), 0);

        // find video stream (if multiple video streams are available choose manually)
        int videoStreamIndex = av_find_best_stream(decoderContext.formatContext, AVMEDIA_TYPE_VIDEO, -1, -1,
                                                   &decoderContext.codec, 0);
        assert(videoStreamIndex >= 0);
        assert(decoderContext.codec);
        decoderContext.videoStream = decoderContext.formatContext->streams[videoStreamIndex];

        // create codec context (for each codec its own codec context)
        decoderContext.codecContext = avcodec_alloc_context3(decoderContext.codec);
        assert(decoderContext.codecContext);

        // copy video stream parameters to the codec context
        statCode = avcodec_parameters_to_context(decoderContext.codecContext, decoderContext.videoStream->codecpar);
        assert(statCode >= 0);

        // initialize the codec context to use the created codec context
        statCode = avcodec_open2(decoderContext.codecContext, decoderContext.codec, &options);
        assert(statCode == 0);

        // save info
        frameRate = decoderContext.videoStream->r_frame_rate;
        frameWidth = static_cast<size_t>(decoderContext.codecContext->width);
        frameHeight = static_cast<size_t>(decoderContext.codecContext->height);
        rawPixFormat = decoderContext.codecContext->pix_fmt;
        sourceBitRate = static_cast<size_t>(decoderContext.codecContext->bit_rate);

        decodingPacket = av_packet_alloc();
        av_init_packet(decodingPacket);

        rawFrame = av_frame_alloc();

        LOG(INFO) << "Decoder for \"" << videoSourceUrl << "\" has been created (framerate: "
                  << frameRate.num << "/" << frameRate.den << ", w x h: " << frameWidth << "x" << frameHeight << ")";
    }

    void Transcoder::initializeEncoder() {

        // allocate format context for an output format (null - no output file)
        int statCode = avformat_alloc_output_context2(&encoderContext.formatContext, nullptr, "null", nullptr);
        assert(statCode >= 0);

        encoderContext.codec = avcodec_find_encoder_by_name("libx265");
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

        encoderContext.codecContext->profile = FF_PROFILE_HEVC_MAIN;

        encoderContext.codecContext->time_base = (AVRational) {outputFrameRate.den, outputFrameRate.num};
        encoderContext.codecContext->framerate = outputFrameRate;

        // set encoder's pixel format (most of the players support yuv420p)
        encoderContext.codecContext->pix_fmt = encoderPixFormat;


        if (encoderContext.formatContext->flags & AVFMT_GLOBALHEADER) {
            encoderContext.codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        // copy encoder parameters to the video stream parameters
        avcodec_parameters_from_context(encoderContext.videoStream->codecpar, encoderContext.codecContext);

        AVDictionary *options = nullptr;

        // the faster you get, the less compression is achieved
        av_dict_set(&options, "preset", "ultrafast", 0);

        // optimization for fast encoding and low latency streaming
        av_dict_set(&options, "tune", "zerolatency", 0);

        // constant rate factor
        av_dict_set_int(&options, "crf", 32, 0);

        // set additional codec options
        av_opt_set(encoderContext.codecContext->priv_data, "x265-params", "slices=2:intra-refresh=1", 0);

        // open the output format to use given codec
        statCode = avcodec_open2(encoderContext.codecContext, encoderContext.codec, &options);
        av_dict_free(&options);
        assert(statCode == 0);

        // initializes time base
        statCode = avformat_write_header(encoderContext.formatContext, nullptr);
        assert(statCode >= 0);

        // report info to the console
        av_dump_format(encoderContext.formatContext, encoderContext.videoStream->index, "null", 1);

        encodingPacket = av_packet_alloc();
        av_init_packet(encodingPacket);
    }

    void Transcoder::initializeConverter() {

        // allocate frame to be used in converter manually allocating buffer
        convertedFrame = av_frame_alloc();
        convertedFrame->width = static_cast<int>(frameWidth);
        convertedFrame->height = static_cast<int>(frameHeight);
        convertedFrame->format = encoderPixFormat;
        int statCode = av_frame_get_buffer(convertedFrame, 0);
        assert(statCode == 0);

        // create converter from raw pixel format to encoder supported pixel format
        converterContext = sws_getCachedContext(nullptr, static_cast<int>(frameWidth), static_cast<int>(frameHeight),
                                                rawPixFormat, static_cast<int>(frameWidth),
                                                static_cast<int>(frameHeight), encoderPixFormat, SWS_FAST_BILINEAR,
                                                nullptr, nullptr, nullptr);

        LOG(INFO) << "Pixel format converter for \"" << videoSourceUrl << "\" has been created ("
                  << av_get_pix_fmt_name(rawPixFormat) << " -> " << av_get_pix_fmt_name(encoderPixFormat) << ")";

    }

    void Transcoder::initFilters() {

        filterFrame = av_frame_alloc();

        AVFilter *bufferSrc = avfilter_get_by_name("buffer");
        AVFilter *bufferSink = avfilter_get_by_name("buffersink");

        auto outputs = avfilter_inout_alloc();
        auto inputs = avfilter_inout_alloc();

        filterGraph = avfilter_graph_alloc();

        char args[64];
        snprintf(args, sizeof(args), "%d:%d:%d:%d:%d:%d:%d:frame_rate=%d/%d", (int) frameWidth, (int) frameHeight,
                 rawPixFormat, decoderContext.videoStream->time_base.num, decoderContext.videoStream->time_base.den,
                 1, 1, frameRate.num, frameRate.den);

        auto status = avfilter_graph_create_filter(&bufferSrcCtx, bufferSrc, "in", args, nullptr, filterGraph);
        assert(status >= 0);

        status = avfilter_graph_create_filter(&bufferSinkCtx, bufferSink, "out", nullptr, nullptr, filterGraph);
        assert(status >= 0);

        outputs->name = av_strdup("in");
        outputs->filter_ctx = bufferSrcCtx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = bufferSinkCtx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        char frameStepF[32];

        snprintf(frameStepF, sizeof(frameStepF), "framestep=step=%d", static_cast<unsigned int>(frameStep));

        status = avfilter_graph_parse(filterGraph, frameStepF, inputs, outputs, nullptr);
        assert(status >= 0);

        status = avfilter_graph_config(filterGraph, nullptr);
        assert(status >= 0);
    }

    int Transcoder::decode(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

        auto statCode = avcodec_send_packet(codecContext, packet);

        if (statCode < 0) {
            LOG(ERROR) << "Error sending a packet for decoding: " << videoSourceUrl;
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

    std::string Transcoder::getDeviceName() const {
        return videoSourceUrl;
    }

    std::string Transcoder::getAlias() const {
        return deviceAlias;
    }

    void Transcoder::cleanup() {

        avfilter_graph_free(&filterGraph);

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
        av_frame_free(&filterFrame);

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
    }
}