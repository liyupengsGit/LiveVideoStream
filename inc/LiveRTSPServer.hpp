#ifndef LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
#define LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include "H264FramedSource.hpp"

namespace LIRS {

    class LiveRTSPServer {

    public:

        explicit LiveRTSPServer(Transcoder *transcoder0, int port = 8554, int httpPort = -1) :
                rtspPort(port), httpTunnelingPort(httpPort), videoSink0(nullptr),
                videoSourceES0(nullptr), transcoder0(transcoder0), quit(0) {}

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
            OutPacketBuffer::maxSize = 500000;
            videoSink0 = H264VideoRTPSink::createNew(*env, &rtpGroupSock, 96);

            auto estimatedSessionBandwidth = 1024U; // in kbps
            auto urlMaxLength = 100U;
            unsigned char URL[urlMaxLength + 1];
            gethostname(reinterpret_cast<char *>(URL), urlMaxLength);
            URL[urlMaxLength] = '\0';

            auto rtcp0 = RTCPInstance::createNew(*env, &rtcpGroupSock, estimatedSessionBandwidth, URL, videoSink0,
                                                nullptr, True); // starts automatically

            auto rtspServer = RTSPServer::createNew(*env, rtspPort);

            if (!rtspServer) {
                *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
                return; // todo exit properly
            } else {

                auto sms = ServerMediaSession::createNew(*env, "cam0", "UVC device",
                                                         "LIRS Video Streaming Session (camera#0)", True);
                sms->addSubsession(PassiveServerMediaSubsession::createNew(*videoSink0, rtcp0));
                rtspServer->addServerMediaSession(sms);

                auto url = rtspServer->rtspURL(sms);
                *env << "Play camera#0 stream using the URL \"" << url << "\"\n";
                delete (url);

                auto camera0Source = H264FramedSource::createNew(*env, transcoder0); // first camera

                // discrete framer is more optimized than framer
                videoSourceES0 = H264VideoStreamDiscreteFramer::createNew(*env, camera0Source);

                // start playing
                videoSink0->startPlaying(*videoSourceES0, nullptr, videoSink0);

                *env << "Start streaming ...\n";

                env->taskScheduler().doEventLoop(&quit);
            }
        }


    private:

        int rtspPort;
        int httpTunnelingPort;

        RTPSink *videoSink0;
        FramedSource *videoSourceES0;
        Transcoder* transcoder0;

        char quit;
    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
