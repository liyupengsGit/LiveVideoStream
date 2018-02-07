#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

}
#endif

#define VIDEO_DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480

// forward declaration
void configureVideoCapture(cv::VideoCapture &);
void convertToBgrAndShow(const cv::Mat &);

int decode(AVCodecContext *codecContext, AVFrame *frame, bool &gotFrame, AVPacket *pkt)
{
    int retCode;
    gotFrame = false;

    if (pkt)
    {
        retCode = avcodec_send_packet(codecContext, pkt);
        if (retCode < 0) return retCode == AVERROR_EOF ? 0 : retCode;
    }

    retCode = avcodec_receive_frame(codecContext, frame);
    if (retCode < 0 && retCode != AVERROR(EAGAIN) && retCode != AVERROR_EOF)
    {
        return retCode;
    }

    if (retCode >= 0) gotFrame = true;

    return 0;
}

int main(int argc, char** argv)
{
    initLogger(LOG_LEVEL_DEBUG);
    av_log_set_level(AV_LOG_DEBUG);

    av_register_all();
    avcodec_register_all();
    avdevice_register_all();

    AVCodecContext *pCodecCtx = nullptr;
    auto *pFormatCtx   = avformat_alloc_context();
    AVCodec *pCodec    = nullptr;
    auto *inputFormat  = av_find_input_format("v4l2");
    AVFrame *pFrame    = nullptr;
    AVFrame *pFrameRGB = nullptr;
    AVDictionary *opts = nullptr;
    int statusCode;

    av_dict_set(&opts, "framerate", "15", 0);
    av_dict_set(&opts, "video_size", "640x480", 0);
    av_dict_set(&opts, "pixel_format", "yuyv422", 0);

    statusCode = avformat_open_input(&pFormatCtx, VIDEO_DEVICE, inputFormat, &opts);
    assert(statusCode >= 0);

    statusCode = avformat_find_stream_info(pFormatCtx, nullptr);
    assert(statusCode >= 0);

    av_dump_format(pFormatCtx, 0, VIDEO_DEVICE, 0);

    int videoStreamIdx = -1;
    for (int i = 0; i < pFormatCtx->nb_streams; ++i)
    {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIdx = i;
            break;
        }
    }
    assert(videoStreamIdx >= 0);

    auto *codecParams = pFormatCtx->streams[videoStreamIdx]->codecpar;

    pCodec    = avcodec_find_decoder(codecParams->codec_id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    assert(pCodec && pCodecCtx);

    statusCode = avcodec_parameters_to_context(pCodecCtx, codecParams);
    assert(statusCode >= 0);

//    av_dict_set(&opts, "b", "2.5M", 0);
    statusCode = avcodec_open2(pCodecCtx, pCodec, &opts);
    assert(statusCode == 0);

    pFrame    = av_frame_alloc();
    pFrameRGB = av_frame_alloc();

    uint8_t *buffer = nullptr;
    int numBytes = -1;

    AVPixelFormat pixelFormat = AV_PIX_FMT_BGR24;
    numBytes = av_image_get_buffer_size(pixelFormat, WIDTH, HEIGHT, 1);
    assert(numBytes > 0);

    LOG(INFO) << "Buffer size: " << numBytes;
    buffer = static_cast<uint8_t *>(av_mallocz(numBytes * sizeof(uint8_t)));
    assert(buffer);

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, pixelFormat, WIDTH, HEIGHT, 1);

    int res;
    bool frameFinished;
    AVPacket packet{};

    int counter = 300;
    cv::namedWindow("Display", CV_WINDOW_AUTOSIZE);

    while ((res = av_read_frame(pFormatCtx, &packet)) >= 0 && counter-- > 0)
    {

      if (packet.stream_index == videoStreamIdx)
      {
          decode(pCodecCtx, pFrame, frameFinished, &packet);

          if (frameFinished)
          {
              struct SwsContext *convertCtx = sws_getCachedContext(nullptr, pCodecCtx->width, pCodecCtx->height,
                                                                   pCodecCtx->pix_fmt,
                                                                   pCodecCtx->width, pCodecCtx->height,
                                                                   pixelFormat, SWS_BICUBIC,
                                                                   nullptr, nullptr, nullptr);

              sws_scale(convertCtx, (const uint8_t* const*) pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                        pFrameRGB->data,pFrameRGB->linesize);

              cv::Mat img(pFrame->height, pFrame->width, CV_8UC3, pFrameRGB->data[0]);
              cv::imshow("display", img);
              cvWaitKey(1);

              av_packet_unref(&packet);
              sws_freeContext(convertCtx);
          }

      }
    }

    av_packet_unref(&packet);
    av_free(pFrame);
    av_free(pFrameRGB);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    avformat_free_context(pFormatCtx);

    /*
    // OpenCV video capturing API
    cv::VideoCapture videoCapture(VIDEO_DEVICE, cv::CAP_V4L2);

    if (!videoCapture.isOpened()) {
        LOG(FATAL) << "Video device: " << VIDEO_DEVICE << " cannot be opened.";
        return 1;
    }

    // configure stream parameters
    configureVideoCapture(videoCapture);

    cv::Mat frame;
    cv::namedWindow("Stream", CV_WINDOW_AUTOSIZE);

    for (int framesNeeded = 256; framesNeeded > 0; framesNeeded--) {

        // consider using grab/retrieve workflow
        if(!videoCapture.read(frame)) {
            LOG(ERROR) << "Frame has not been read.";
        } else {

            // FIXME: this way is not the best way of decreasing frames per second
            if (framesNeeded % 2 == 0) {
                // drop frames
                continue;
            }

            convertToBgrAndShow(frame);
        }
    }
    */

    return 0;
}

/*
 * Configure video capture properties.
 */
void configureVideoCapture(cv::VideoCapture &videoCapture)
{

    // Do not convert captured frames into RGB (CPU consuming, use ffmpeg instead)
    videoCapture.set(cv::CAP_PROP_CONVERT_RGB, false);

    // Frame height and width in pixels
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, HEIGHT);
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, WIDTH);

    // Frames per second
    videoCapture.set(cv::CAP_PROP_FPS, 10);
}

/*
 * Convert raw frame into BGR and show it in GUI.
 */
void convertToBgrAndShow(const cv::Mat &rawFrame)
{
    cv::Mat bgrFrame(rawFrame.rows, rawFrame.cols, CV_8UC3);
    cv::cvtColor(rawFrame, bgrFrame, CV_YUV2BGR_YUYV); // convert to BGR
    cv::imshow("Stream", bgrFrame);
    cv::waitKey(30);
}
