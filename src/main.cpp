#include "Config.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);

    av_log_set_level(AV_LOG_VERBOSE);

    LIRS::Configuration configuration;
    configuration.loadConfig(argc, argv);

//    auto rare = LIRS::Transcoder::newInstance(RIGHT_CAM, "right", 744, 480, "bayer_grbg8", "yuv420p", 15, LIRS::Configuration::DEFAULT_STREAMING_FRAMERATE);
//
//    auto server = new LIRS::LiveCameraRTSPServer();
//
//    server->addTranscoder(rare);
//
//    server->run();
//
//    delete server;

    return 0;
}
