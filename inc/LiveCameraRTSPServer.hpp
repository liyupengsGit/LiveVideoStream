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

            allocatedTranscoders.clear();
            allocatedVideoSources.clear();
            watcher = 0;

            LOG(INFO) << "RTSP server has been destructed";
        }

        void stopServer() {
            watcher = 's';
        }

        void addTranscoder(Transcoder* transcoder) {
            if (transcoder) allocatedTranscoders.push_back(transcoder);
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

            for (auto &transcoder : allocatedTranscoders) {
                addMediaSession(transcoder, "camera" + transcoder->getDeviceName(), "description");
            }

            env->taskScheduler().doEventLoop(&watcher); // do not return

            Medium::close(server); // deletes all server media sessions

            // delete all framed sources
            for (auto &src : allocatedVideoSources) {
                if (src) Medium::close(src);
            }

            env->reclaim();
            delete scheduler;
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

        std::vector<Transcoder*> allocatedTranscoders;
        std::vector<FramedSource*> allocatedVideoSources;

        void announceStream(ServerMediaSession* sms, const std::string &deviceName) {
            auto url = server->rtspURL(sms);
            *env << "Play the stream for camera \"" << deviceName.data() << "\" using the URL: " << url << "\n";
            delete[] url;
        }

        void addMediaSession(Transcoder *transcoder, const std::string &streamName, const std::string &streamDesc) {

            auto framedSource = H264FramedSource::createNew(*env, transcoder);

            allocatedVideoSources.push_back(framedSource);

            auto replicator = StreamReplicator::createNew(*env, framedSource, False);

            auto sms = ServerMediaSession::createNew(*env, streamName.c_str(), streamDesc.c_str());

            sms->addSubsession(CameraUnicastServerMediaSubsession::createNew(*env, replicator));

            server->addServerMediaSession(sms);

            announceStream(sms, transcoder->getDeviceName());
        }

    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
