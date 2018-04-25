#include <CameraUnicastServerMediaSubsession.hpp>

namespace LIRS {

    CameraUnicastServerMediaSubsession *CameraUnicastServerMediaSubsession::createNew(UsageEnvironment &env,
                                                                                      StreamReplicator *replicator) {
        return new CameraUnicastServerMediaSubsession(env, replicator);
    }

    CameraUnicastServerMediaSubsession::CameraUnicastServerMediaSubsession(UsageEnvironment &env,
                                                                           StreamReplicator *replicator)
            : OnDemandServerMediaSubsession(env, False), replicator(replicator) {}

    FramedSource *
    CameraUnicastServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {

        LOG(INFO) << "Create new stream source for client: " << clientSessionId;

        estBitrate = Configuration::DEFAULT_BITRATE_KBPS;

        auto source = replicator->createStreamReplica();

        // only discrete frames are being sent (w/o start code bytes)
        return H265VideoStreamDiscreteFramer::createNew(envir(), source);
    }

    RTPSink *
    CameraUnicastServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                         FramedSource *inputSource) {

        auto sink = H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
        sink->setPacketSizes(Configuration::DEFAULT_UDP_PACKET_SIZE, Configuration::DEFAULT_UDP_PACKET_SIZE);
        return sink;
    }

}


