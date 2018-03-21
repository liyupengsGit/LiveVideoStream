#include "Logger.hpp"
#include "LiveCameraRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_DEBUG);

    auto transcoder3 = LIRS::Transcoder::newInstance("/dev/video3", "camera3", 744, 480, "bayer_grbg8", "yuv420p", 15, 2, 5);
    auto transcoder2 = LIRS::Transcoder::newInstance("/dev/video2", "camera2", 744, 480, "bayer_grbg8", "yuv420p", 15, 2, 5);
    auto transcoder1 = LIRS::Transcoder::newInstance("/dev/video1", "camera1", 744, 480, "bayer_grbg8", "yuv420p", 15, 2, 5);
    auto transcoder0 = LIRS::Transcoder::newInstance("/dev/video0", "camera0", 1280, 720, "yuyv422", "yuv420p", 50, 8, 5);

    auto server = new LIRS::LiveCameraRTSPServer();
    server->addTranscoder(transcoder0);
    server->addTranscoder(transcoder1);
    server->addTranscoder(transcoder2);
    server->addTranscoder(transcoder3);

    std::thread([&](){
        server->run();
    }).detach();

    delete server;

    return 0;
}
