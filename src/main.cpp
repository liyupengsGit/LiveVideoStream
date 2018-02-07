#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
}
#endif

#define VIDEO_DEVICE "/dev/video0"

// forward declaration
void configureVideoCapture(cv::VideoCapture &);
void convertToBgrAndShow(const cv::Mat &);

int decode(AVCodecContext *avctx, AVFrame *frame, bool &gotFrame, AVPacket *pkt)
{
    int retCode;
    gotFrame = false;

    if (pkt)
    {
        retCode = avcodec_send_packet(avctx, pkt);
        if (retCode < 0) return retCode == AVERROR_EOF ? 0 : retCode;
    }

    retCode = avcodec_receive_frame(avctx, frame);
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
    av_log_set_level(AV_LOG_TRACE);

    avdevice_register_all();
    avcodec_register_all();
    av_register_all();

    int retCode;

    // find input format
    AVInputFormat *inputFormat = av_find_input_format("v4l2");
    if (!inputFormat)
    {
        LOG(ERROR) << "Couldn't find v4l2 input format.";
        return -1;
    }

    // set options
    AVDictionary *opts = nullptr;
    av_dict_set(&opts,"framerate", "30", 0);
    av_dict_set(&opts, "video_size", "640x480", 0);
    av_dict_set(&opts, "pixel_format", "yuyv422", 0);

    // allocate format context with the specified input format and options
    AVFormatContext *formatContext = nullptr;
    if ((retCode = avformat_open_input(&formatContext, VIDEO_DEVICE, inputFormat, &opts)) != 0)
    {
        LOG(ERROR) << "Couldn't open v4l2 device.";
        return retCode;
    }

    // find H.264 codec
    AVCodec *h264Codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!h264Codec) {
        LOG(ERROR) << "Couldn't find H.264 codec.";
        return -1;
    }

    // create new stream
    AVStream *stream = avformat_new_stream(formatContext, h264Codec);
    stream->codecpar->format = AV_PIX_FMT_YUYV422;
    stream->time_base = AVRational{1, 30}; // ~ 1/FPS

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
    videoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 920);
    videoCapture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);

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
