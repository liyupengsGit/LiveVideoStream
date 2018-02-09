#include <opencv2/highgui.hpp>

#include "logger.h"
#include "FFmpegDecoder.h"

#define VIDEO_SOURCE "/dev/video0"

void callback(uint8_t *data) {
    cv::Mat img(480, 640, CV_8UC3, data);
    cv::imshow("Display", img);
    cvWaitKey(1);
}

int main(int argc, char **argv) {

    initLogger(LOG_LEVEL_DEBUG);

    LIRS::DecoderParams params{};
    params.frameRate = 15;
    params.frameWidth = 640;
    params.frameHeight = 480;

    LIRS::Decoder *decoder = new LIRS::FFmpegDecoder(VIDEO_SOURCE, params);

    decoder->setOnFrameCallback(callback);
    decoder->decodeLoop();

    delete decoder;

    return 0;
}
