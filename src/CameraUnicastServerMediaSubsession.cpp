#include <Logger.hpp>
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

        estBitrate = 400;

        auto source = replicator->createStreamReplica();

        // only discrete frames are being sent (w/o start code bytes)
        return H265VideoStreamDiscreteFramer::createNew(envir(), source);
    }

    RTPSink *
    CameraUnicastServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                         FramedSource *inputSource) {

        return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }

}


