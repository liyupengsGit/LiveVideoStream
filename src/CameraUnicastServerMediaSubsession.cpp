#include "CameraUnicastServerMediaSubsession.hpp"

namespace LIRS {

    CameraUnicastServerMediaSubsession *CameraUnicastServerMediaSubsession::createNew(UsageEnvironment &env,
                                                                        StreamReplicator *replicator) {
        return new CameraUnicastServerMediaSubsession(env, replicator);
    }

    CameraUnicastServerMediaSubsession::CameraUnicastServerMediaSubsession(UsageEnvironment &env, StreamReplicator *replicator)
            : OnDemandServerMediaSubsession(env, False), replicator(replicator) {
    }

    FramedSource *CameraUnicastServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned &estBitrate) {
        auto source = replicator->createStreamReplica();
        return H264VideoStreamDiscreteFramer::createNew(envir(), source);
    }

    RTPSink *CameraUnicastServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                  FramedSource *inputSource) {
        return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }

}


