#ifndef LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
#define LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include "H264FramedSource.hpp"
#include "CameraUnicastServerMediaSubsession.hpp"

namespace LIRS {

    class LiveCameraRTSPServer {

    public:

        explicit LiveCameraRTSPServer(unsigned int port = DEFAULT_RTSP_PORT_NUMBER, int httpPort = -1) :
                rtspPort(port), httpTunnelingPort(httpPort), watcher(0),
                scheduler(nullptr), env(nullptr), server(nullptr) {

            OutPacketBuffer::maxSize = OUT_PACKET_BUFFER_MAX_SIZE;

            scheduler = BasicTaskScheduler::createNew();
            env = BasicUsageEnvironment::createNew(*scheduler);
        }

        ~LiveCameraRTSPServer() {
            // todo cleanup
            LOG(INFO) << "RTSP server destructor";
        }

        void run() {

            if (server) return; // already running

            server = RTSPServer::createNew(*env, rtspPort);

            if (!server) {
                *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
                exit(1);
            }

            if (httpTunnelingPort != -1) {
                auto res = server->setUpTunnelingOverHTTP(httpTunnelingPort);
                if (res) {
                    *env << "Enabled HTTP tunneling over: " << httpTunnelingPort << "\n";
                }
            }

            auto transcoder = Transcoder::newInstance("/dev/video0", 640, 480, "yuyv422", "yuv422p", 15, 4, 500000);

            addMediaSession(transcoder, "camera", "description");

            auto transcoder2 = Transcoder::newInstance("/dev/video1", 640, 480, "yuyv422", "yuv422p", 15, 4, 500000);

            addMediaSession(transcoder2, "camera2", "description");

            env->taskScheduler().doEventLoop(&watcher); // do not return

            delete transcoder;
        }

        /** Constants **/

        static const unsigned int DEFAULT_RTSP_PORT_NUMBER = 8554;

        static const unsigned int OUT_PACKET_BUFFER_MAX_SIZE = 500 * 1000;

    private:

        unsigned int rtspPort;
        int httpTunnelingPort;

        char watcher;

        TaskScheduler* scheduler;
        UsageEnvironment* env;
        RTSPServer* server;

        void announceStream(ServerMediaSession* sms, const std::string &deviceName) {
            auto url = server->rtspURL(sms);
            *env << "Play the stream for camera \"" << deviceName.data() << "\" using the URL: " << url << "\n";
            delete[] url;
        }

        void addMediaSession(Transcoder *transcoder, const std::string &streamName, const std::string &streamDesc) {

            auto framedSource = H264FramedSource::createNew(*env, transcoder);

            auto replicator = StreamReplicator::createNew(*env, framedSource, False);

            auto sms = ServerMediaSession::createNew(*env, streamName.c_str(), streamDesc.c_str());

            sms->addSubsession(CameraUnicastServerMediaSubsession::createNew(*env, replicator));

            server->addServerMediaSession(sms);

            announceStream(sms, transcoder->getDeviceName());
        }

    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
