#include "Logger.hpp"
#include "LiveRTSPServer.hpp"

int main(int argc, char **argv) {

    initLogger(log4cpp::Priority::DEBUG);
    av_log_set_level(AV_LOG_WARNING);

    auto server = new LIRS::LiveRTSPServer();
    server->run();

    return 0;
}
