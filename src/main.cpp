//#include "FFmpegDecoder.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "Encoder.hpp"

const char* SOURCE = "/dev/video0";
constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;
constexpr int FRAMERATE = 15;

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

void opencvCallback(uint8_t* data) {
    cv::Mat raw(HEIGHT, WIDTH, CV_8UC2, data);
    cv::Mat bgr24;
    cv::cvtColor(raw, bgr24, CV_YUV2BGR_YUYV);
    cv::imshow("Stream", bgr24);
    cv::waitKey(1);
}

void simpleCallback(uint8_t* data) {
    LOG(INFO) << "Received frame";
}

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_TRACE);

    LOG(DEBUG) << "Start ...";

    using namespace LIRS;

    /*
    DecoderParams params{};
    params.pixelFormat = "yuyv422";
    params.frameHeight = HEIGHT;
    params.frameWidth = WIDTH;
    params.frameRate = FRAMERATE;

    Decoder* decoder = new FFmpegDecoder(SOURCE, params);
    decoder->setOnFrameCallback(simpleCallback);
    decoder->decodeLoop();

    delete(decoder);
    */

    Encoder* encoder = new Encoder();

    delete(encoder);

    LOG(WARN) << "Done.";

    return 0;
}
