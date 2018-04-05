#include "Logger.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);

    av_log_set_level(AV_LOG_VERBOSE);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video0", "camera", 640, 480, "yuyv422", "yuv420p", 15, 15);

    auto server = new LIRS::LiveCameraRTSPServer();

    server->addTranscoder(transcoder);

    server->run();

    delete server;

    return 0;
}
