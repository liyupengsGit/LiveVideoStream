#ifndef LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
#define LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP

#include <UsageEnvironment.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>
#include <liveMedia.hh>
#include "LiveCamFramedSource.hpp"
#include "CameraUnicastServerMediaSubsession.hpp"
#include "TestServerMediaSubsession.hpp"

namespace LIRS {

    class LiveCameraRTSPServer {

    public:

        explicit LiveCameraRTSPServer(unsigned int port = DEFAULT_RTSP_PORT_NUMBER, int httpPort = -1) :
                rtspPort(port), httpTunnelingPort(httpPort), watcher(0),
                scheduler(nullptr), env(nullptr), server(nullptr) {

            // create scheduler and environment
            scheduler = BasicTaskScheduler::createNew();
            env = BasicUsageEnvironment::createNew(*scheduler);

            destAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
        }

        ~LiveCameraRTSPServer() {

            Medium::close(server); // deletes all server media sessions

            // delete all framed sources
            for (auto &src : allocatedVideoSources) {
                if (src) Medium::close(src);
            }

            env->reclaim();

            delete scheduler;

            transcoders.clear();
            allocatedVideoSources.clear();
            watcher = 0;

            LOG(INFO) << "RTSP server has been destructed";
        }

        /**
         * Changes the watch variable in order to stop the event loop.
         */
        void stopServer() {
            watcher = 's';
        }

        /**
         * Adds transcoder as a source in order to create server media session with it.
         *
         * @param transcoder - a reference to the transcoder.
         */
        void addTranscoder(Transcoder *transcoder) {
            transcoders.push_back(transcoder);
        }

        /*
         * Creates a new RTSP server adding subsessions to each video source.
         */
        void run() {

            if (server) return; // already running

            // create server listening on the specified RTSP port
            server = RTSPServer::createNew(*env, rtspPort);

            if (!server) {
                *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
                exit(1);
            }

            if (httpTunnelingPort != -1) { // set up HTTP tunneling (see Live555 docs)
                auto res = server->setUpTunnelingOverHTTP(httpTunnelingPort);
                if (res) {
                    *env << "Enabled HTTP tunneling over: " << httpTunnelingPort << "\n";
                }
            }

            // create media session for each video source (transcoder)
            for (auto &transcoder : transcoders) {
                addMediaSession(transcoder, transcoder->getAlias(), "description");
            }

            env->taskScheduler().doEventLoop(&watcher); // do not return
        }

        /** Constants **/

        static const unsigned int DEFAULT_RTSP_PORT_NUMBER = 8554;

        static const unsigned int OUT_PACKET_BUFFER_MAX_SIZE = 2 * 1000 * 1000;

    private:

        /**
         * RTSP port number.
         */
        unsigned int rtspPort;

        /**
         * RTSP server's HTTP tunneling port number.
         */
        int httpTunnelingPort;

        /*
         * Watch variable to control the event loop.
         */
        char watcher;

        TaskScheduler *scheduler;
        UsageEnvironment *env;

        RTSPServer *server;

        struct in_addr destAddress{};

        /**
         * Pointers to video sources (transcoders).
         */
        std::vector<Transcoder *> transcoders;

        /**
         * Pointers to framed sources (Live555).
         * Hold in order to cleanup.
         */
        std::vector<FramedSource *> allocatedVideoSources;

        /**
         * Announce new create media session.
         *
         * @param sms - created server media session.
         * @param deviceName - the name of the video source device.
         */
        void announceStream(ServerMediaSession *sms, const std::string &deviceName) {
            auto url = server->rtspURL(sms);
            *env << "Play the stream for camera \"" << deviceName.data() << "\" using the URL: " << url << "\n";
            delete[] url;
        }

        /**
         * Adds new server media session using transcoder as a source to the server.
         *
         * @param transcoder - video source.
         * @param streamName - the name of the stream (part of the URL), e.g. rtsp://.../<camera/1>.
         * @param streamDesc -description of the stream.
         */
        void addMediaSession(Transcoder *transcoder, const std::string &streamName, const std::string &streamDesc) {

            static unsigned startPortNum = 18888;

            // create framed source based on transcoder
            auto framedSource = LiveCamFramedSource::createNew(*env, transcoder);

            allocatedVideoSources.push_back(framedSource);

            // create media session with the specified path and description
            ServerMediaSession *sms = ServerMediaSession::createNew(*env, streamName.c_str(), streamDesc.c_str());

            OutPacketBuffer::maxSize = OUT_PACKET_BUFFER_MAX_SIZE;

            // add unicast subsession using replicator
            sms->addSubsession(MulticastServerMediaSubsession::createNew(*env, destAddress, Port(startPortNum++),
                                                                         Port(startPortNum++), 1, 96, framedSource));

            server->addServerMediaSession(sms);

            // announce stream
            announceStream(sms, transcoder->getDeviceName());
        }

    };
}

#endif //LIVE_VIDEO_STREAM_LIVE_RTSP_SERVER_HPP
