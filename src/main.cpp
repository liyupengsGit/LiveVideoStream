#include "Logger.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_WARNING);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video3", "camera", 744, 480, "bayer_grbg8", "yuv420p", 15, 5, 500000);

    auto server = new LIRS::LiveCameraRTSPServer();
    server->addTranscoder(transcoder);
    server->run();

    delete server;
    delete transcoder;

    return 0;
}
