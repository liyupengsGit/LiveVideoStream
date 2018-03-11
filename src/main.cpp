#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "Logger.hpp"
#include "LiveRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_INFO);

    auto cam0Transcoder = LIRS::Transcoder::newInstance("/dev/video0", 640, 480, AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUV422P, 15, 15, 500000);
    std::shared_ptr<LIRS::Transcoder> transcoderPtr(cam0Transcoder);

    // run in a separate thread
    std::thread([cam0Transcoder]() {
        cam0Transcoder->playVideo();
    }).detach();

    // server (in main thread)
    auto server = new LIRS::LiveRTSPServer(cam0Transcoder, 8554);
    server->run();

    return 0;
}
