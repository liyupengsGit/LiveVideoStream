#include <iostream>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include "logger.h"

#define VIDEO_DEVICE "/dev/video0"

int main(int argc, char** argv)
{
    using namespace std;

    initLogger(LOG_LEVEL_INFO); // initialize logger

    cv::VideoCapture videoCapture(VIDEO_DEVICE, cv::CAP_V4L2);

    if (!videoCapture.isOpened()) {
        LOG(FATAL) << "Video device: " << VIDEO_DEVICE << " cannot be opened.";
        return 1;
    }

    LOG(INFO) << "Video device '" << VIDEO_DEVICE << "' is ready to stream.";

    /* configure stream parameters */

    // Do not convert captured frames into RGB (CPU consuming, use ffmpeg instead)
    videoCapture.set(cv::CAP_PROP_CONVERT_RGB, false);



    cv::Mat frame;

    for (int framesNeeded = 64; framesNeeded > 0; framesNeeded--){

        if(!videoCapture.read(frame)) {
            LOG(WARN) << "Frame has not been read.";
        } else {
            LOG(INFO) << "Frame has been grabbed.";
        }
    }
}