#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "Logger.hpp"
#include "Transcoder.hpp"

void callback(uint8_t* data) {
    cv::Mat raw (480, 640, CV_8UC2, data);
    cv::Mat gray;
    cv::cvtColor(raw, gray, CV_YUV2BGR_YUYV);
    cv::imshow("Stream", gray);
    cv::waitKey(1);
}

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_TRACE);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video0", 640, 480, AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUV422P, 15, 5, 400000);

    std::thread([transcoder]() {
        transcoder->callback = callback;
        transcoder->playVideo();
    }).join();

    delete(transcoder);

    return 0;
}
