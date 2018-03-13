#ifndef LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
#define LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include "H264FramedSource.hpp"
#include "CustomServerMediaSubsession.hpp"

namespace LIRS {

    class LiveRTSPServer {

    public:

        explicit LiveRTSPServer(uint port = 8554, int httpPort = -1) :
                rtspPort(port), httpTunnelingPort(httpPort), quit(0), taskScheduler(nullptr), env(nullptr) {

            taskScheduler = BasicTaskScheduler::createNew();
            env = BasicUsageEnvironment::createNew(*taskScheduler);
        }

        ~LiveRTSPServer() {
            LOG(INFO) << "RTSP server destructor";
        }

        void run() {

            OutPacketBuffer::maxSize = 500000;

            auto rtspServer = RTSPServer::createNew(*env, rtspPort);

            if (!rtspServer) {
                *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
                exit(1);
            }

            if (httpTunnelingPort != -1) {
                rtspServer->setUpTunnelingOverHTTP(httpTunnelingPort);
            }

            // H.264 video elementary stream #0
            {

                auto transcoder = Transcoder::newInstance("/dev/video0", 640, 480, "yuyv422", "yuv422p", 15, 15, 500000);
                std::thread([transcoder](){
                    transcoder->playVideo();
                }).detach();

                auto source = H264FramedSource::createNew(*env, transcoder);
                auto replicator = StreamReplicator::createNew(*env, source, False);

                std::string streamName = "camera";
                std::string description = "UVC #0 laptop camera";

                auto sms = ServerMediaSession::createNew(*env, streamName.c_str(), description.c_str());
                sms->addSubsession(CustomServerMediaSubsession::createNew(*env, replicator));
                rtspServer->addServerMediaSession(sms);

                auto url = rtspServer->rtspURL(sms);
                *env << "Play the stream #0 using URL: " << url << "\n";
                delete [] url;
            }

            // H.264 video elementary stream #1
            {

                auto transcoder = Transcoder::newInstance("/dev/video1", 640, 480, "yuyv422", "yuv422p", 15, 15, 500000);

                std::thread([transcoder](){
                    transcoder->playVideo();
                }).detach();

                auto source = H264FramedSource::createNew(*env, transcoder);
                auto replicator = StreamReplicator::createNew(*env, source, False);

                std::string streamName = "other";
                std::string description = "UVC #1 laptop camera";

                auto sms = ServerMediaSession::createNew(*env, streamName.c_str(), description.c_str());
                sms->addSubsession(CustomServerMediaSubsession::createNew(*env, replicator));
                rtspServer->addServerMediaSession(sms);

                auto url = rtspServer->rtspURL(sms);
                *env << "Play the stream #1 using URL: " << url << "\n";
                delete [] url;
            }

            *env << "Playing stream ...\n";

            env->taskScheduler().doEventLoop(&quit);

            Medium::close(rtspServer);
            delete taskScheduler;
        }


    private:

        uint rtspPort;
        int httpTunnelingPort;
        char quit;

        TaskScheduler* taskScheduler;
        UsageEnvironment* env;

        static const uint RTP_PORT_NUMBER = 18888;
    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
