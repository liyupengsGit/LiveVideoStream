#include "Logger.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_WARNING);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video0", "camera", 640, 480, "yuyv422", "yuv422p", 15, 4, 500000);

    auto server = new LIRS::LiveCameraRTSPServer();
    server->addTranscoder(transcoder);
    server->run();

    delete server;
    delete transcoder;

    return 0;
}
