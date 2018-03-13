#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "Logger.hpp"
#include "LiveRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_WARNING);

    auto cam0Transcoder = LIRS::Transcoder::newInstance("/dev/video0", 640, 480, "yuyv422", "yuv422p", 15, 15, 500000);

    // run in a separate thread
    std::thread([cam0Transcoder]() {
        cam0Transcoder->playVideo();
    }).detach();

    // run server in the main thread on port 8554
    auto server = new LIRS::LiveRTSPServer(cam0Transcoder, 8554);
    server->run();

    // cleanup
    delete(cam0Transcoder);
    delete(server);

    return 0;
}
