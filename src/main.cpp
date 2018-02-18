#include <opencv2/highgui.hpp>

#include "logger.h"
#include "FFmpegDecoder.hpp"
#include "Encoder.hpp"

#define VIDEO_SOURCE "/dev/video0"

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;
constexpr int FRAMERATE = 15;

void callback(uint8_t *data) {
    cv::Mat img(HEIGHT, WIDTH, CV_8UC3, data);
    cv::imshow("Display", img);
    cvWaitKey(1);
}

int dec(AVCodecContext *codecContext, AVFrame *frame, AVPacket *packet) {

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

    /*
    LOG(INFO) << "Frame " << codecContext->frame_number << "(type=" << av_get_picture_type_char(frame->pict_type)
              << ", size=" << frame->pkt_size << " bytes) pts " << frame->pts
              << " [DTS " << frame->pkt_dts << "]";
    */

    return statusCode;
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

int main(int argc, char **argv) {

    initLogger(LOG_LEVEL_DEBUG);
    av_log_set_level(AV_LOG_TRACE);

    // register ffmpeg
    av_register_all();
    avcodec_register_all();
    avdevice_register_all();
    avformat_network_init();

    int statusCode;

    // decoder
    AVDictionary* opts           = nullptr;
    AVCodecContext* inCodecCtx   = nullptr;
    AVFormatContext* inFormatCtx = nullptr;
    AVInputFormat* inFormat      = nullptr;
    AVStream* inStream           = nullptr;
    AVCodec* decoderCodec        = nullptr;
    int inVideoStreamIdx         = -1;      // index of the video stream

    // holds the general (header) information about the format (container)
    inFormatCtx = avformat_alloc_context();
    assert(inFormatCtx);

    // input format (video4linux device)
    inFormat = av_find_input_format("v4l2");
    assert(inFormat);

    // set demuxer options
    av_dict_set_int(&opts, "framerate", FRAMERATE, 0);
    av_dict_set(&opts, "video_size", "640x480", 0);
    av_dict_set(&opts, "pixel_format", "yuyv422", 0);

    // open an input stream and read header
    statusCode = avformat_open_input(&inFormatCtx, VIDEO_SOURCE, inFormat, &opts);
    av_dict_free(&opts);
    assert(statusCode == 0);

    // read packets to get information about all of the streams
    statusCode = avformat_find_stream_info(inFormatCtx, nullptr);
    assert(statusCode >= 0);

    // print debug information
    av_dump_format(inFormatCtx, 0, VIDEO_SOURCE, 0);

    // find video stream among others (if present)
    inVideoStreamIdx = av_find_best_stream(inFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoderCodec, 0);
    assert(inVideoStreamIdx >= 0);

    // save reference to the found video stream
    inStream = inFormatCtx->streams[inVideoStreamIdx];
    assert(decoderCodec);
    assert(inStream);

    // create codec context for the specified codec
    inCodecCtx = avcodec_alloc_context3(decoderCodec);
    assert(inCodecCtx);

    // fill the codec context with the supplied video stream parameters
    statusCode = avcodec_parameters_to_context(inCodecCtx, inStream->codecpar);
    assert(statusCode >= 0);

    // initialize the codec context to use the given codec
    statusCode = avcodec_open2(inCodecCtx, decoderCodec, nullptr);
    assert(statusCode == 0);

    std::cout << "\n==================== INPUT STREAM END =====================\n" << std::endl;

    // encoder
    const std::string fileExtension = "mp4";
    std::string url = "/home/itis/video.mp4";
    AVFormatContext* outFormatCtx = nullptr;
    AVStream* outStream           = nullptr;
    AVCodec* encoderCodec         = nullptr;
    AVCodecContext* outCodecCtx   = nullptr;
    int outVideoStreamIdx         = -1;

    // allocate format context for an output format
    statusCode = avformat_alloc_output_context2(&outFormatCtx, nullptr, fileExtension.data(), url.data());
    assert(statusCode >= 0);

    // find codec for encoding (H.264)
    encoderCodec = avcodec_find_encoder_by_name("libx264");
    assert(encoderCodec);

    // create new video output stream
    outStream = avformat_new_stream(outFormatCtx, encoderCodec);
    assert(outStream);
    outVideoStreamIdx = outStream->index;

    // create codec context
    outCodecCtx = avcodec_alloc_context3(encoderCodec);
    assert(outCodecCtx);

    // set up codec context
    outCodecCtx->width        = WIDTH;
    outCodecCtx->height       = HEIGHT;
    outCodecCtx->pix_fmt      = AV_PIX_FMT_YUV422P;
    outCodecCtx->profile      = FF_PROFILE_H264_HIGH_422_INTRA;
    outCodecCtx->time_base    = (AVRational) {1, FRAMERATE};
    outCodecCtx->framerate    = (AVRational) {FRAMERATE, 1};
    if (outFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        outCodecCtx->flags |= AVFMT_GLOBALHEADER;
    }

    // copy codec parameters from codec context
    avcodec_parameters_from_context(outStream->codecpar, outCodecCtx);

    // see https://trac.ffmpeg.org/wiki/Encode/H.264
//    av_dict_set_int(&opts, "crf", 10, 0);
    av_dict_set(&opts, "preset", "fast", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);

    // initialize output format context to use the given codec
    statusCode = avcodec_open2(outCodecCtx, encoderCodec, &opts);
    av_dict_free(&opts);
    assert(statusCode == 0);

    // print debug info
    av_dump_format(outFormatCtx, outVideoStreamIdx, url.data(), 1);

    // open file to write
    if (!(outFormatCtx->flags & AVFMT_NOFILE)) {
        avio_open(&outFormatCtx->pb, url.data(), AVIO_FLAG_WRITE);
    }

    statusCode = avformat_write_header(outFormatCtx, nullptr);
    assert(statusCode >= 0);

    // create frame to be encoded
    AVFrame* convertedFrame = av_frame_alloc();
    assert(convertedFrame);

    convertedFrame->width   = outCodecCtx->width;
    convertedFrame->height  = outCodecCtx->height;
    convertedFrame->format  = outCodecCtx->pix_fmt;
    statusCode = av_frame_get_buffer(convertedFrame, 32);
    assert(statusCode == 0);

    // converter
    SwsContext* imageConverter = sws_getCachedContext(nullptr, inCodecCtx->width, inCodecCtx->height,
                                                      inCodecCtx->pix_fmt,
                                                      outCodecCtx->width, outCodecCtx->height,
                                                      outCodecCtx->pix_fmt,
                                                      SWS_BICUBIC, nullptr, nullptr, nullptr);

    // create packet
    AVPacket* packet = av_packet_alloc();
    assert(packet);
    av_init_packet(packet);

    AVFrame* rawFrame = av_frame_alloc();
    assert(rawFrame);

    int counter = FRAMERATE * 3; // 10 seconds

    // fill the packet with data from the input stream (raw data)
    while (av_read_frame(inFormatCtx, packet) == 0 && counter --> 0) {

        // if it's the video stream
        if (packet->stream_index == inVideoStreamIdx) {

            // decode packet (retrieve frame)
            statusCode = dec(inCodecCtx, rawFrame, packet);

            av_frame_make_writable(convertedFrame);

            if (statusCode >= 0) {

                sws_scale(imageConverter, reinterpret_cast<const uint8_t *const *>(rawFrame->data),
                          rawFrame->linesize, 0, inCodecCtx->height,
                          convertedFrame->data, convertedFrame->linesize);

                // copy PTS, DTS, etc. to the converted frame
                av_frame_copy_props(convertedFrame, rawFrame);

                statusCode = enc(outCodecCtx, convertedFrame, packet);

                if (statusCode >= 0) {

                    // rescale timestamp
                    av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

                    statusCode = av_write_frame(outFormatCtx, packet);
                    assert(statusCode == 0);
                }

            } else {
                break; // reached end of stream or error occurred
            }
        }

        av_packet_unref(packet);
    }

    // flush the encoder
    statusCode = enc(outCodecCtx, nullptr, packet);

    // write the trailer
    statusCode = av_write_trailer(outFormatCtx);
    assert(statusCode == 0);

    // free all allocated memory and close opened files
    avio_close(outFormatCtx->pb);

    sws_freeContext(imageConverter);

    av_packet_free(&packet);
    av_frame_free(&rawFrame);
    av_frame_free(&convertedFrame);

    avcodec_free_context(&inCodecCtx);
    avcodec_free_context(&outCodecCtx);

    avformat_close_input(&inFormatCtx);

    avformat_free_context(inFormatCtx);
    avformat_free_context(outFormatCtx);

    return 0;
}
