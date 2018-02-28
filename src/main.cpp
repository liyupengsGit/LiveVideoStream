#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <csignal>
#include "Logger.hpp"
#include "Transcoder.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_TRACE);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video0", 640, 480, 15, AV_PIX_FMT_YUYV422, 400000);

    delete(transcoder);

    return 0;
}
