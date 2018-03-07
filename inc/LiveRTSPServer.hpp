#ifndef LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
#define LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include <utility>
#include "H264FramedSource.hpp"
#include "Transcoder.hpp"

namespace LIRS {

    class LiveRTSPServer {

    public:

        explicit LiveRTSPServer(std::shared_ptr<Transcoder> src, int port = 8554, int httpPort = -1) :
                rtspPort(port), httpTunnelingPort(httpPort), videoSink(nullptr),
                videoSource(nullptr), source(std::move(src)), quit(0) {}

        ~LiveRTSPServer() {
            LOG(INFO) << "RTSP server destructor";
        }

        void run() {

            auto taskScheduler = BasicTaskScheduler::createNew();
            auto env = BasicUsageEnvironment::createNew(*taskScheduler);

            // create 'groupsocks' for RTP and RTCP
            struct in_addr destinationAddress{};
            destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);

            auto rtpPortNumber = 18888U;
            auto rtcpPortNumber = rtpPortNumber + 1;
            auto ttl = 255U;

            const Port rtpPort(rtpPortNumber);
            const Port rtcpPort(rtcpPortNumber);

            Groupsock rtpGroupSock(*env, destinationAddress, rtpPort, ttl);
            rtpGroupSock.multicastSendOnly(); // ssm source
            Groupsock rtcpGroupSock(*env, destinationAddress, rtcpPort, ttl);
            rtcpGroupSock.multicastSendOnly();

            // Create a 'H264 Video RTP' sink from the RTP 'groupscok'
            OutPacketBuffer::maxSize = 2000000;
            videoSink = H264VideoRTPSink::createNew(*env, &rtpGroupSock, 96);

            auto estimatedSessionBandwidth = 1024U; // in kbps
            auto maxCNAMElen = 100U;
            unsigned char CNAME[maxCNAMElen + 1];
            gethostname(reinterpret_cast<char *>(CNAME), maxCNAMElen);
            CNAME[maxCNAMElen] = '\0';

            auto rtcp = RTCPInstance::createNew(*env, &rtcpGroupSock, estimatedSessionBandwidth, CNAME, videoSink,
                                                nullptr, True); // starts automatically

            auto rtspServer = RTSPServer::createNew(*env, rtspPort);

            if (!rtspServer) {
                *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
                exit(1); // todo exit properly
            } else {
                std::string desc = "LIRS Streaming Session (camera)";

                auto sms = ServerMediaSession::createNew(*env, "camera", "UVC camera #0", desc.data(), True);

                sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink, rtcp));
                rtspServer->addServerMediaSession(sms);

                auto url = rtspServer->rtspURL(sms);
                *env << "Play this stream using the URL \"" << url << "\"\n";
                delete (url);
                *env << "Start streaming ...\n";

                auto cameraSource = H264FramedSource::createNew(*env, source);

                videoSource = H264VideoStreamFramer::createNew(*env, cameraSource);

                // start playing
                videoSink->startPlaying(*videoSource, nullptr, videoSink);

                env->taskScheduler().doEventLoop(&quit);
            }
        }


    private:

        int rtspPort;
        int httpTunnelingPort;

        RTPSink *videoSink;
        FramedSource *videoSource;
        std::shared_ptr<Transcoder> source;

        char quit;
    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
