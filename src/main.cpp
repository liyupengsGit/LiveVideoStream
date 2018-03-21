#include "Logger.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_DEBUG);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video3", "camera", 744, 480, "bayer_grbg8", "yuv420p", 15, 5, 15);

    auto server = new LIRS::LiveCameraRTSPServer();
    server->addTranscoder(*transcoder);

    std::thread([&](){
        std::this_thread::sleep_for(std::chrono::seconds(10));
        server->stopServer();
    }).detach();

    server->run();

    delete server;

    return 0;
}
