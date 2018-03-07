#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "Logger.hpp"
#include "LiveRTSPServer.hpp"


int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_INFO);

    auto transcoder = LIRS::Transcoder::newInstance("/dev/video0", 640, 480, AV_PIX_FMT_YUYV422, AV_PIX_FMT_YUV422P, 15, 15, 700000);
    std::shared_ptr<LIRS::Transcoder> transcoderPtr(transcoder);

    // server
    auto server = new LIRS::LiveRTSPServer(transcoderPtr, 8554);
    std::thread([server](){server->run();}).detach();

    // transcoder  (raw data -> H.264 NALUs)
    transcoder->playVideo();

    return 0;
}
