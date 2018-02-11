#include <opencv2/highgui.hpp>

#include "logger.h"
#include "FFmpegDecoder.hpp"

#define VIDEO_SOURCE "/dev/video0"

constexpr int WIDTH = 640;
constexpr int HEIGHT = 480;

void callback(uint8_t *data) {
    cv::Mat img(HEIGHT, WIDTH, CV_8UC3, data);
    cv::imshow("Display", img);
    cvWaitKey(1);
}

int main(int argc, char **argv) {

    initLogger(LOG_LEVEL_DEBUG);

    LIRS::DecoderParams params{};
    params.frameRate = 15;
    params.frameWidth = WIDTH;
    params.frameHeight = HEIGHT;

    LIRS::Decoder *decoder = new LIRS::FFmpegDecoder(VIDEO_SOURCE, params);

    decoder->setOnFrameCallback(callback);
    decoder->decodeLoop();

    delete decoder;

    return 0;
}
